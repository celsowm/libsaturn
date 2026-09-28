"""Texture sampling, face baking, palette construction and indexing."""
from __future__ import annotations
from collections import Counter
from dataclasses import dataclass
import math
from pathlib import Path
from .errors import ImportError
from .constants import FX16_ONE, VDP1_MAX_TEXTURE_HEIGHT, VDP1_MAX_TEXTURE_WIDTH
from saturn_asset_common import rgb888_to_rgb555

def load_rgba_image(path: Path) -> tuple[int, int, list[tuple[int, int, int, int]]]:
    try:
        from PIL import Image
    except ModuleNotFoundError as exc:
        raise ImportError("Image support needs Pillow: pip install pillow") from exc
    try:
        img = Image.open(path)
    except FileNotFoundError:
        raise ImportError(f"Texture image not found: {path}")
    except OSError as exc:
        raise ImportError(f"Cannot open texture image {path}: {exc}")
    img = img.convert("RGBA")
    w, h = img.size
    if w <= 0 or h <= 0:
        raise ImportError(f"Texture image {path} has invalid size {w}x{h}")
    get_flat = getattr(img, "get_flattened_data", None)
    data = list(get_flat()) if callable(get_flat) else list(img.getdata())
    pixels = [(r, g, b, a) for (r, g, b, a) in data]
    return w, h, pixels


# ----------------------------------------------------------------------
# Canonicalization and winding
# ----------------------------------------------------------------------

@dataclass
class BakedFaceInput:
    # Quad geometry indices + UVs in LibSaturn A/B/C/D order.
    vert_ids: tuple[int, int, int, int]
    uvs: tuple[tuple[float, float], tuple[float, float], tuple[float, float], tuple[float, float]]
    mtl: str | None
    lineno: int
    is_triangle: bool


def canonicalize_faces(
    model: ObjModel,
    reverse_winding: bool,
    flip_x: bool,
    flip_y: bool,
    flip_z: bool,
) -> tuple[list[tuple[float, float, float]], list[BakedFaceInput]]:
    verts = list(model.vertices)
    if flip_x or flip_y or flip_z:
        flipped = []
        for (x, y, z) in verts:
            if flip_x:
                x = -x
            if flip_y:
                y = -y
            if flip_z:
                z = -z
            flipped.append((x, y, z))
        verts = flipped
        # Mirroring across an odd number of axes inverts orientation, so the
        # winding reversal toggles to keep outward normals outward.
        if (int(bool(flip_x)) + int(bool(flip_y)) + int(bool(flip_z))) % 2 == 1:
            reverse_winding = not reverse_winding

    out: list[BakedFaceInput] = []
    for face in model.faces:
        items = face["verts"]
        mtl = face["mtl"]
        lineno = face["lineno"]
        for _, vti in items:
            if vti is None:
                raise ImportError(
                    f"Face at line {lineno} is missing UV indices "
                    "(all faces need v/vt for textured import)"
                )
        if len(items) == 3:
            (a, au), (b, bu), (c, cu) = items
            auv = model.uvs[au]  # type: ignore[index]
            buv = model.uvs[bu]  # type: ignore[index]
            cuv = model.uvs[cu]  # type: ignore[index]
            if reverse_winding:
                # Direct OBJ order (CCW) would wind inward under
                # cross(D-A, B-A); keep OBJ order for --reverse-winding.
                out.append(BakedFaceInput((a, b, c, c), (auv, buv, cuv, cuv), mtl, lineno, True))
            else:
                # Default: reverse to clockwise (outward): A=v0, B=v2, C=v1.
                out.append(BakedFaceInput((a, c, b, b), (auv, cuv, buv, buv), mtl, lineno, True))
        elif len(items) == 4:
            (a, au), (b, bu), (c, cu), (d, du) = items
            auv = model.uvs[au]  # type: ignore[index]
            buv = model.uvs[bu]  # type: ignore[index]
            cuv = model.uvs[cu]  # type: ignore[index]
            duv = model.uvs[du]  # type: ignore[index]
            if reverse_winding:
                out.append(
                    BakedFaceInput((a, b, c, d), (auv, buv, cuv, duv), mtl, lineno, False)
                )
            else:
                # Reverse to clockwise: A=v0, B=v3, C=v2, D=v1.
                out.append(
                    BakedFaceInput((a, d, c, b), (auv, duv, cuv, buv), mtl, lineno, False)
                )
        else:  # pragma: no cover - fan triangulation above guarantees 3/4
            raise ImportError(f"Face at line {lineno} has {len(items)} vertices after triangulation")
    return verts, out


def apply_scale(
    verts: list[tuple[float, float, float]], scale: float
) -> list[tuple[float, float, float]]:
    if scale == 1.0:
        return verts
    return [(x * scale, y * scale, z * scale) for (x, y, z) in verts]


def float_to_fx16(value: float) -> int:
    return int(math.floor(value * FX16_ONE + 0.5))


# ----------------------------------------------------------------------
# Baking
# ----------------------------------------------------------------------

def uv_to_pixel(uv: tuple[float, float], img_w: int, img_h: int) -> tuple[float, float]:
    u, v = uv
    # OBJ v=0 is the bottom of the image; image row 0 is the top.
    x = u * (img_w - 1)
    y = (1.0 - v) * (img_h - 1)
    return x, y


def estimate_face_size(
    uvs: tuple[tuple[float, float], tuple[float, float], tuple[float, float], tuple[float, float]],
    img_w: int,
    img_h: int,
    texture_scale: float,
) -> tuple[int, int]:
    (au, av), (bu, bv), (cu, cv), (du, dv) = uvs
    ax, ay = au * img_w, (1.0 - av) * img_h
    bx, by = bu * img_w, (1.0 - bv) * img_h
    cx, cy = cu * img_w, (1.0 - cv) * img_h
    dx, dy = du * img_w, (1.0 - dv) * img_h
    top = math.hypot(bx - ax, by - ay)
    bottom = math.hypot(cx - dx, cy - dy)
    left = math.hypot(dx - ax, dy - ay)
    right = math.hypot(cx - bx, cy - by)
    w = max(top, bottom) * texture_scale
    h = max(left, right) * texture_scale
    w = max(1, int(round(w)))
    h = max(1, int(round(h)))
    return w, h


def conform_size(
    est_w: int,
    est_h: int,
    max_w: int,
    max_h: int,
    mtl: str | None,
    lineno: int,
) -> tuple[int, int]:
    if max_w < 8 or max_w > VDP1_MAX_TEXTURE_WIDTH or (max_w & 7) != 0:
        raise ImportError(
            f"--max-texture-width must be a multiple of 8 in 8..{VDP1_MAX_TEXTURE_WIDTH} "
            f"(got {max_w})"
        )
    if max_h < 1 or max_h > VDP1_MAX_TEXTURE_HEIGHT:
        raise ImportError(
            f"--max-texture-height must be in 1..{VDP1_MAX_TEXTURE_HEIGHT} (got {max_h})"
        )
    # Resample the same UV domain across the legal aligned width instead of
    # padding with unused columns (the VDP1 maps the whole stored width).
    aligned_w = ((max(est_w, 1) + 7) // 8) * 8
    if aligned_w < 8:
        aligned_w = 8
    out_h = max(est_h, 1)
    if aligned_w > max_w or out_h > max_h:
        raise ImportError(
            f"Face (material '{mtl}', OBJ line {lineno}) needs {aligned_w}x{out_h} "
            f"but the limit is {max_w}x{max_h}; raise --max-texture-width/height "
            f"(hardware max {VDP1_MAX_TEXTURE_WIDTH}x{VDP1_MAX_TEXTURE_HEIGHT}) "
            f"or lower --texture-scale"
        )
    return aligned_w, out_h


def bake_face_rgba(
    uvs: tuple[tuple[float, float], tuple[float, float], tuple[float, float], tuple[float, float]],
    img_w: int,
    img_h: int,
    img_pixels: list[tuple[int, int, int, int]],
    out_w: int,
    out_h: int,
    sampling: str,
) -> list[tuple[int, int, int, int]]:
    if sampling == "area":
        return _bake_face_area(uvs, img_w, img_h, img_pixels, out_w, out_h)
    if sampling != "nearest":
        raise ImportError(f"Unknown --sampling '{sampling}' (supported: nearest, area)")
    (au, av), (bu, bv), (cu, cv), (du, dv) = uvs
    out: list[tuple[int, int, int, int]] = []
    for y in range(out_h):
        t = (y + 0.5) / out_h
        for x in range(out_w):
            s = (x + 0.5) / out_w
            # Bilinear parameterization over the canonical rectangle:
            # top = lerp(A, B, s); bottom = lerp(D, C, s); uv = lerp(top, bottom, t).
            top_u = au + (bu - au) * s
            top_v = av + (bv - av) * s
            bot_u = du + (cu - du) * s
            bot_v = dv + (cv - dv) * s
            u = top_u + (bot_u - top_u) * t
            v = top_v + (bot_v - top_v) * t
            # Clamp to the intended source bounds (no atlas bleeding).
            if u < 0.0:
                u = 0.0
            elif u > 1.0:
                u = 1.0
            if v < 0.0:
                v = 0.0
            elif v > 1.0:
                v = 1.0
            fx = u * (img_w - 1)
            fy = (1.0 - v) * (img_h - 1)
            ix = int(math.floor(fx + 0.5))
            iy = int(math.floor(fy + 0.5))
            if ix < 0:
                ix = 0
            elif ix >= img_w:
                ix = img_w - 1
            if iy < 0:
                iy = 0
            elif iy >= img_h:
                iy = img_h - 1
            out.append(img_pixels[iy * img_w + ix])
    return out


_LINEAR_CACHE: dict[int, tuple] = {}


def _linear_image(img_w: int, img_h: int, img_pixels) -> "np.ndarray":
    import numpy as np

    key = id(img_pixels)
    hit = _LINEAR_CACHE.get(key)
    if hit is None or hit[0] is not img_pixels:
        srgb = np.asarray(img_pixels, dtype=np.float64).reshape(img_h, img_w, 4)
        lin = srgb.copy()
        lin[..., :3] = (srgb[..., :3] / 255.0) ** 2.2
        lin[..., 3] /= 255.0
        hit = (img_pixels, lin)
        _LINEAR_CACHE[key] = hit
    return hit[1]


def _bake_face_area(uvs, img_w, img_h, img_pixels, out_w, out_h):
    """Box-filtered bake: each output texel averages the source area it covers.

    The VDP1 samples textures nearest-neighbour with no mipmapping, so a face
    baked at more texels than it covers on screen shimmers into speckle
    noise. Baking at the on-screen size (see --texel-extent) with this filter
    is the offline equivalent of a mip level. Samples are bilinear source
    fetches on a k x k grid per texel, averaged in linear light.
    """
    import numpy as np

    lin = _linear_image(img_w, img_h, img_pixels)
    (au, av), (bu, bv), (cu, cv), (du, dv) = uvs
    src_w, src_h = estimate_face_size(uvs, img_w, img_h, 1.0)
    ratio = max(src_w / out_w, src_h / out_h, 1.0)
    k = int(min(8, max(2, math.ceil(ratio * 1.5))))
    sub = (np.arange(k) + 0.5) / k
    s = ((np.arange(out_w)[:, None] + sub[None, :]) / out_w).reshape(-1)
    t = ((np.arange(out_h)[:, None] + sub[None, :]) / out_h).reshape(-1)
    S, T = np.meshgrid(s, t)
    top_u = au + (bu - au) * S
    top_v = av + (bv - av) * S
    bot_u = du + (cu - du) * S
    bot_v = dv + (cv - dv) * S
    u = np.clip(top_u + (bot_u - top_u) * T, 0.0, 1.0)
    v = np.clip(top_v + (bot_v - top_v) * T, 0.0, 1.0)
    fx = u * (img_w - 1)
    fy = (1.0 - v) * (img_h - 1)
    x0 = np.clip(np.floor(fx).astype(int), 0, img_w - 1)
    y0 = np.clip(np.floor(fy).astype(int), 0, img_h - 1)
    x1 = np.minimum(x0 + 1, img_w - 1)
    y1 = np.minimum(y0 + 1, img_h - 1)
    wx = (fx - x0)[..., None]
    wy = (fy - y0)[..., None]
    val = ((lin[y0, x0] * (1 - wx) + lin[y0, x1] * wx) * (1 - wy)
           + (lin[y1, x0] * (1 - wx) + lin[y1, x1] * wx) * wy)
    val = val.reshape(out_h, k, out_w, k, 4).mean(axis=(1, 3))
    rgb = np.clip(np.round((val[..., :3] ** (1 / 2.2)) * 255.0), 0, 255).astype(int)
    alpha = np.clip(np.round(val[..., 3] * 255.0), 0, 255).astype(int)
    out = np.concatenate([rgb, alpha[..., None]], axis=-1).reshape(-1, 4)
    return [tuple(int(c) for c in px) for px in out]


# ----------------------------------------------------------------------
# Palette
# ----------------------------------------------------------------------

def build_shared_palette(
    baked_rgba: list[list[tuple[int, int, int, int]]],
) -> tuple[list[tuple[int, int, int]], bool, list[int]]:
    """Returns (palette_rgb888, has_transparency, per-face opaque flags).

    palette index 0 is reserved for transparent pixels when any baked face
    has transparency; opaque colors fill the remaining entries.
    """
    has_transparency = any(a < 128 for face in baked_rgba for (_, _, _, a) in face)
    limit = 255 if has_transparency else 256

    opaque: list[tuple[int, int, int]] = []
    for face in baked_rgba:
        for (r, g, b, a) in face:
            if a >= 128:
                opaque.append((r, g, b))
    if not opaque:
        # Fully transparent model: single transparent entry.
        return [(0, 0, 0)], True, [0] * len(baked_rgba)

    counts = Counter(opaque)
    distinct = len(counts)
    if distinct <= limit:
        # Deterministic: most frequent first, ties broken by RGB.
        ordered = sorted(counts.items(), key=lambda kv: (-kv[1], kv[0]))
        entries = [rgb for (rgb, _) in ordered]
    else:
        entries = _quantize_colors(opaque, limit)

    if has_transparency:
        palette = [(0, 0, 0)] + entries[:255]
    else:
        palette = entries[:256]
    return palette, has_transparency, [0] * len(baked_rgba)


def _quantize_colors(
    opaque: list[tuple[int, int, int]], limit: int
) -> list[tuple[int, int, int]]:
    from PIL import Image

    n = len(opaque)
    w = min(n, 1024)
    h = (n + w - 1) // w
    img = Image.new("RGB", (w, h), (0, 0, 0))
    img.putdata(opaque + [(0, 0, 0)] * (w * h - n))
    q = img.quantize(colors=limit, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = q.getpalette() or []
    entries: list[tuple[int, int, int]] = []
    for i in range(limit):
        r = pal[i * 3] if i * 3 < len(pal) else 0
        g = pal[i * 3 + 1] if i * 3 + 1 < len(pal) else 0
        b = pal[i * 3 + 2] if i * 3 + 2 < len(pal) else 0
        entries.append((r, g, b))
    # Drop trailing duplicates while keeping order deterministic.
    seen: set[tuple[int, int, int]] = set()
    unique: list[tuple[int, int, int]] = []
    for e in entries:
        if e not in seen:
            seen.add(e)
            unique.append(e)
    # Keep frequency ordering deterministic: re-sort by frequency in the
    # source with RGB tiebreak, restricted to the quantized set.
    allowed = set(unique)
    counts = Counter(c for c in opaque if c in allowed)
    # Colors in the quantized palette that never occur map from nearest
    # source colors; keep them at the end in palette order.
    ranked = sorted(
        [c for c in unique if c in counts],
        key=lambda c: (-counts[c], c),
    )
    tail = [c for c in unique if c not in counts]
    return ranked + tail


def quantize_face_lut4(
    rgba: list[tuple[int, int, int, int]], width: int, height: int,
) -> tuple[list[int], bytes]:
    """One baked face as a LUT4 texture: (16 RGB555 entries, packed texels).

    Each face gets its own palette, fitted to the few colors a small patch of
    the source texture actually holds, instead of sharing one 256-color bank
    across the whole model. Texel code 0 is left unused: with the sprite's
    transparency code active it would punch a hole, so a face keeps 15
    colors. Colors are reduced to RGB555 first -- the hardware cannot show
    the difference, and it lets a face with few distinct colors keep them
    exactly. Entries are sorted so faces with the same colors share a table.
    Texels pack two per byte, leftmost in the high nibble.
    """
    from PIL import Image

    rgb555 = [rgb888_to_rgb555(r, g, b) for (r, g, b, _a) in rgba]
    distinct = sorted(set(rgb555))
    if len(distinct) <= 15:
        colors = distinct
        index_of = {c: i for i, c in enumerate(colors)}
        codes = [index_of[c] for c in rgb555]
    else:
        img = Image.new("RGB", (width, height))
        img.putdata([(r, g, b) for (r, g, b, _a) in rgba])
        q = img.quantize(colors=15, method=Image.Quantize.MEDIANCUT, kmeans=2,
                         dither=Image.Dither.NONE)
        pal = q.getpalette() or []
        indices = list(q.tobytes())  # mode "P": one palette index per byte
        raw = {i: rgb888_to_rgb555(pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2])
               for i in set(indices)}
        colors = sorted(set(raw.values()))
        index_of = {c: i for i, c in enumerate(colors)}
        codes = [index_of[raw[i]] for i in indices]
    lut = [colors[0]] + colors + [colors[-1]] * (15 - len(colors))
    packed = bytearray()
    for y in range(height):
        row = codes[y * width:(y + 1) * width]
        for x in range(0, width, 2):
            packed.append(((row[x] + 1) << 4) | (row[x + 1] + 1))
    return lut, bytes(packed)


def _lut_code_palette(baked_rgba, lo: int, hi: int):
    """One shared palette in codes lo..hi, and a LUT4 entry -> code mapper.

    An 8-bit/pixel VDP1 framebuffer (the 640/704-wide hi-res modes) keeps
    only the low byte of what a lookup table yields, which VDP2 then reads as
    a colour-RAM index: RGB table entries are not allowed there. The whole
    model therefore shares one palette of hi - lo + 1 colours, and each face's
    15-colour table holds the codes of its nearest palette colours. Codes
    outside lo..hi stay free for the transparent code 0, the sprite shadow
    code and whatever the program draws next to the model.
    Returns (256-entry RGB555 palette, mapper from an RGB555 LUT entry).
    """
    opaque = [(r, g, b) for face in baked_rgba for (r, g, b, _a) in face]
    count = hi - lo + 1
    distinct = sorted(set(opaque))
    entries = distinct if len(distinct) <= count else _quantize_colors(opaque, count)
    entries = entries[:count]
    palette = [0x0000] * 256
    for i, (r, g, b) in enumerate(entries):
        palette[lo + i] = rgb888_to_rgb555(r, g, b)
    cache: dict[int, int] = {}

    def snap(c555: int) -> int:
        hit = cache.get(c555)
        if hit is None:
            r = (c555 & 0x1F) << 3
            g = ((c555 >> 5) & 0x1F) << 3
            b = ((c555 >> 10) & 0x1F) << 3
            best = min(range(len(entries)), key=lambda i: (
                (entries[i][0] - r) ** 2 + (entries[i][1] - g) ** 2
                + (entries[i][2] - b) ** 2))
            hit = cache[c555] = lo + best
        return hit

    return palette, snap


def map_faces_to_indices(
    baked_rgba: list[list[tuple[int, int, int, int]]],
    face_sizes: list[tuple[int, int]],
    palette_rgb888: list[tuple[int, int, int]],
) -> list[bytes]:
    from PIL import Image

    pal_img = Image.new("P", (16, 16))
    flat: list[int] = []
    for (r, g, b) in palette_rgb888:
        flat.extend([r, g, b])
    while len(flat) < 256 * 3:
        flat.extend([0, 0, 0])
    pal_img.putpalette(flat[: 256 * 3])

    out: list[bytes] = []
    for face, (w, h) in zip(baked_rgba, face_sizes):
        # Transparent texels must map to index 0: Pillow's quantize has no
        # alpha concept, so remap in two steps. Build an RGB image where
        # transparent texels are painted with the reserved palette entry 0
        # color only if that color is unique to transparency; otherwise paint
        # them magenta and fix up afterwards. Simpler and exact: map opaque
        # texels through Pillow, force transparent texels to 0.
        rgb_pixels = [(r, g, b) for (r, g, b, a) in face]
        img = Image.new("RGB", (w, h))
        img.putdata(rgb_pixels)
        q = img.quantize(palette=pal_img, dither=Image.Dither.NONE)
        get_flat = getattr(q, "get_flattened_data", None)
        indices = list(get_flat()) if callable(get_flat) else list(q.getdata())
        fixed = bytearray(len(indices))
        for i, ((r, g, b, a), idx) in enumerate(zip(face, indices)):
            if a < 128:
                fixed[i] = 0
            else:
                fixed[i] = idx
                # Never let an opaque texel claim the transparent index when
                # transparency is in use: if Pillow mapped an opaque color to
                # 0 (because palette[0] is black and the texel is black),
                # redirect it to the nearest non-zero entry.
                if palette_rgb888 and len(palette_rgb888) > 1 and idx == 0 and (0, 0, 0) in palette_rgb888:
                    # palette[0] is the reserved transparent slot; find best
                    # non-zero match for this opaque color.
                    best = 1
                    best_d = None
                    for pi in range(1, len(palette_rgb888)):
                        pr, pg, pb = palette_rgb888[pi]
                        d = (pr - r) ** 2 + (pg - g) ** 2 + (pb - b) ** 2
                        if best_d is None or d < best_d:
                            best_d = d
                            best = pi
                    fixed[i] = best
        out.append(bytes(fixed))
    return out


# ----------------------------------------------------------------------
# Import driver
# ----------------------------------------------------------------------

