#!/usr/bin/env python3
"""How close is city_walk to the source GLB? Render both, compare pixels.

A host-only z-buffered rasteriser draws triangle soups from the street-level
viewpoints city_walk uses, with the same camera (55-degree vertical field of
view over 224 rows, eye 2 units above the street) and the same flat Lambert
light the chunker bakes. It renders the source model as the reference and a
candidate (any list of world-space triangles and colours) the same way, so the
difference is geometry and colour only, never painter-order artefacts.

Metric per view: the share of pixels whose colour differs from the reference by
more than a visible threshold (``--threshold``, Euclidean sRGB), plus the mean
error. Lower is closer to the original.

    python tools/city_fidelity.py --glb build/generated/city_walk/city_plain.glb \
        --out build/generated/city_walk/fidelity
"""

from __future__ import annotations

import argparse
import math
import sys
from dataclasses import dataclass
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))

from model_pipeline import chunking as ch  # noqa: E402

WIDTH, HEIGHT = 320, 224
FOCAL = 215.0  # pixels: 55 degrees over 224 rows, the runtime projection
EYE_HEIGHT = 2.0
NEAR = 0.25
FAR = 128.0  # the fade's end: nothing beyond it is visible in the game

# (chunk x, chunk z, local x, local z, yaw degrees): examples/city_walk/player.c
VIEWPOINTS = (
    (3, 7, 31, 28, 0), (5, 5, 11, 8, 90), (3, 12, 31, 24, 180),
    (8, 5, 31, 12, 270), (5, 8, 27, 28, 0), (10, 2, 27, 16, 90), (8, 5, 15, 12, 0),
)


@dataclass
class Camera:
    eye: np.ndarray
    forward: np.ndarray
    right: np.ndarray

    @staticmethod
    def at(viewpoint, ground_y: float) -> "Camera":
        cx, cz, lx, lz, yaw = viewpoint
        ex = ch.ORIGIN_X + cx * ch.CHUNK_UNITS + lx
        ez = ch.ORIGIN_Z + cz * ch.CHUNK_UNITS + lz
        a = math.radians(yaw)
        # Right-handed like glTF and sat_camera3d: right = forward x up.
        return Camera(np.array([ex, ground_y + EYE_HEIGHT, ez]),
                      np.array([math.sin(a), 0.0, math.cos(a)]),
                      np.array([-math.cos(a), 0.0, math.sin(a)]))


def shade(rgb: np.ndarray, normals: np.ndarray, light, ambient: float, diffuse: float) -> np.ndarray:
    """Flat Lambert in linear light, like chunking.shade_rgb555, per triangle."""
    l = np.asarray(light, dtype=np.float64)
    l = l / np.linalg.norm(l)
    k = ambient + diffuse * np.clip(normals @ l, 0.0, 1.0)
    lin = ch._srgb_decode(rgb.astype(np.float64) / 255.0) * k[:, None]
    return np.clip(np.rint(ch._srgb_encode(np.clip(lin, 0, 1)) * 255.0), 0, 255).astype(np.uint8)


def sky() -> np.ndarray:
    img = np.zeros((HEIGHT, WIDTH, 3), dtype=np.uint8)
    t = np.arange(HEIGHT)[:, None] / HEIGHT
    img[:] = np.stack([120 + 60 * t, 170 + 50 * t, 230 - 20 * t], axis=-1).astype(np.uint8)
    return img


def _clip_near(p: np.ndarray):
    """Clip one camera-space triangle (3, 3) against z >= NEAR; 0..2 triangles."""
    inside = p[:, 2] >= NEAR
    if inside.all():
        return [p]
    if not inside.any():
        return []
    poly = []
    for i in range(3):
        a, b = p[i], p[(i + 1) % 3]
        if a[2] >= NEAR:
            poly.append(a)
        if (a[2] >= NEAR) != (b[2] >= NEAR):
            t = (NEAR - a[2]) / (b[2] - a[2])
            poly.append(a + t * (b - a))
    return [np.array([poly[0], poly[i], poly[i + 1]]) for i in range(1, len(poly) - 1)]


def render(pos: np.ndarray, colours: np.ndarray, cam: Camera, background=None,
           backface_cull: bool = False, textures=None) -> np.ndarray:
    """Z-buffered flat rendering of world triangles ``pos`` (N, 3, 3).

    ``textures`` (optional): ``(tex_id (N,), uv (N, 3, 2), images, whole (N,))``.
    A triangle with tex_id >= 0 samples ``images[tex_id]`` (H, W, 3) with its
    uv interpolated LINEARLY in screen space, which is what a VDP1 distorted
    sprite does (no perspective correction). A textured triangle of a quad with
    a corner behind the near plane (``whole`` False) is drawn in its flat
    colour instead: the sprite cannot be clipped, the runtime falls back."""
    img = sky() if background is None else background.copy()
    zbuf = np.full((HEIGHT, WIDTH), np.inf)
    rel = pos - cam.eye
    cam_pos = np.stack([rel @ cam.right, rel[..., 1], rel @ cam.forward], axis=-1)
    z = cam_pos[..., 2]
    near_enough = (z.max(axis=1) >= NEAR) & (z.min(axis=1) <= FAR)
    # Cheap wedge test: a triangle entirely left or right of the view is out.
    half = (WIDTH / 2.0) / FOCAL
    xs = cam_pos[..., 0]
    zc = np.maximum(z, NEAR)
    out_left = (xs < -half * zc - 1.0).all(axis=1)
    out_right = (xs > half * zc + 1.0).all(axis=1)
    keep = np.nonzero(near_enough & ~out_left & ~out_right)[0]
    for t in keep.tolist():
        tri = cam_pos[t]
        if backface_cull:
            n = np.cross(tri[1] - tri[0], tri[2] - tri[0])
            if float(n @ tri[0]) >= 0.0:
                continue
        tex = None
        if textures is not None and textures[0][t] >= 0 and textures[3][t]:
            tex = (textures[2][textures[0][t]], textures[1][t])
        for piece in _clip_near(tri):
            sx = WIDTH / 2.0 + FOCAL * piece[:, 0] / piece[:, 2]
            sy = HEIGHT / 2.0 - FOCAL * piece[:, 1] / piece[:, 2]
            x0 = max(int(math.floor(sx.min())), 0)
            x1 = min(int(math.ceil(sx.max())), WIDTH)
            y0 = max(int(math.floor(sy.min())), 0)
            y1 = min(int(math.ceil(sy.max())), HEIGHT)
            if x0 >= x1 or y0 >= y1:
                continue
            d = (sy[1] - sy[2]) * (sx[0] - sx[2]) + (sx[2] - sx[1]) * (sy[0] - sy[2])
            if abs(d) < 1e-12:
                continue
            gx, gy = np.meshgrid(np.arange(x0, x1) + 0.5, np.arange(y0, y1) + 0.5)
            w0 = ((sy[1] - sy[2]) * (gx - sx[2]) + (sx[2] - sx[1]) * (gy - sy[2])) / d
            w1 = ((sy[2] - sy[0]) * (gx - sx[2]) + (sx[0] - sx[2]) * (gy - sy[2])) / d
            w2 = 1.0 - w0 - w1
            inside = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
            if not inside.any():
                continue
            # Perspective-correct depth: interpolate 1/z in screen space.
            inv = w0 / piece[0, 2] + w1 / piece[1, 2] + w2 / piece[2, 2]
            depth = 1.0 / np.maximum(inv, 1e-9)
            zb = zbuf[y0:y1, x0:x1]
            win = inside & (depth < zb)
            zb[win] = depth[win]
            if tex is None:
                img[y0:y1, x0:x1][win] = colours[t]
            else:
                image, uv = tex
                u = w0 * uv[0, 0] + w1 * uv[1, 0] + w2 * uv[2, 0]
                v = w0 * uv[0, 1] + w1 * uv[1, 1] + w2 * uv[2, 1]
                ui = np.clip(u.astype(np.int64), 0, image.shape[1] - 1)
                vi = np.clip(v.astype(np.int64), 0, image.shape[0] - 1)
                img[y0:y1, x0:x1][win] = image[vi[win], ui[win]]
    return img


def _coarse(img: np.ndarray, k: int = 4) -> np.ndarray:
    h, w = img.shape[0] // k * k, img.shape[1] // k * k
    return img[:h, :w].astype(np.float64).reshape(h // k, k, w // k, k, 3).mean(axis=(1, 3))


def compare(a: np.ndarray, b: np.ndarray, threshold: float) -> dict:
    """Share of 4x4 blocks whose mean colour differs visibly. Block means
    forgive the one-pixel edge shifts every approximation has and still see a
    missing window row, a wrong wall colour or an absent building."""
    diff = np.linalg.norm(_coarse(a) - _coarse(b), axis=-1)
    return {"differs": float((diff > threshold).mean()), "mean_error": float(diff.mean())}


def load_source(glb_path: Path):
    """Visible source triangles, their shaded colours and the street height,
    with the same drops the chunker makes (alpha-mask cards, underground)."""
    from model_pipeline import gltf

    glb = gltf.parse_glb(glb_path)
    tris = ch.load_world_triangles(glb)
    materials = ch.read_materials(glb)
    rgb, keep = ch.triangle_colors(tris, materials)
    normals, areas = ch.triangle_normals(tris.pos)
    masked = np.isin(tris.material, [i for i, m in enumerate(materials) if m.alpha_mode == ch.ALPHA_MASK])
    ground_y = ch.estimate_ground_y(tris, normals, areas)
    visible = keep & ~masked & (areas > 1e-9) & ~ch.underground_mask(tris, ground_y)
    return tris.pos[visible], rgb[visible], normals[visible], ground_y


def _rgb555(v: int):
    return ((v & 31) * 255 // 31, ((v >> 5) & 31) * 255 // 31, ((v >> 10) & 31) * 255 // 31)


def archive_ground(archive: dict, cam: Camera) -> np.ndarray:
    """Sky plus the RBG0 ground bitmap as the runtime projects it."""
    img = sky()
    ground = archive["ground"]
    hdr = archive["header"]
    if ground is None:
        return img
    gy = hdr["ground_y"] / 65536.0
    pal = np.array([_rgb555(c) for c in ground["palette"]] + [(0, 0, 0)] * 256, dtype=np.uint8)
    horizon = HEIGHT // 2
    ys = np.arange(horizon + 1, HEIGHT)
    xs = np.arange(WIDTH)
    dy = (ys - horizon + 0.5) / FOCAL
    depth = (cam.eye[1] - gy) / dy
    side = (xs[None, :] - WIDTH / 2.0 + 0.5) / FOCAL * depth[:, None]
    wx = cam.eye[0] + cam.forward[0] * depth[:, None] + cam.right[0] * side
    wz = cam.eye[2] + cam.forward[2] * depth[:, None] + cam.right[2] * side
    upd_x, upd_z = (max(v, 1) for v in hdr["ground_upd"])
    bx = np.floor((wx - ch.ORIGIN_X) / upd_x).astype(np.int64)
    bz = np.floor((wz - ch.ORIGIN_Z) / upd_z).astype(np.int64)
    inside = (bx >= 0) & (bx < hdr["ground_width"]) & (bz >= 0) & (bz < hdr["ground_height"])
    inside &= (depth <= FAR)[:, None]
    idx = np.zeros(bx.shape, dtype=np.int64)
    idx[inside] = ground["bitmap"][bz[inside], bx[inside]]
    region = img[horizon + 1:, :]
    region[idx > 0] = pal[idx[idx > 0]]
    return img


def archive_triangles(archive: dict, viewpoint, cam: "Camera | None" = None):
    """World triangles and colours the runtime would draw from ``viewpoint``:
    LOD by Chebyshev chunk distance (rings 1/2/3), falling back coarser.
    Also returns the texture binding for :func:`render`."""
    cx0, cz0 = viewpoint[0], viewpoint[1]
    base_y = archive["header"]["world_min_y"] / 65536.0
    mats = archive["materials"]
    tpal = np.array([_rgb555(c) for c in archive.get("texture_palette", [])] or [(0, 0, 0)],
                    dtype=np.uint8)
    pos, col, tex_id, uvs, whole, images = [], [], [], [], [], []
    for cz in range(ch.GRID_Z):
        for cx in range(ch.GRID_X):
            d = max(abs(cx - cx0), abs(cz - cz0))
            if d > 3:
                continue
            want = 0 if d <= 1 else (1 if d <= 2 else 2)
            blob = next((archive["blobs"].get((cz * ch.GRID_X + cx, l)) for l in range(want, 3)
                         if (cz * ch.GRID_X + cx, l) in archive["blobs"]), None)
            if blob is None:
                continue
            v = blob["vertices"].astype(np.float64) / 64.0
            w = np.stack([ch.ORIGIN_X + cx * ch.CHUNK_UNITS + v[:, 0], base_y + v[:, 1],
                          ch.ORIGIN_Z + cz * ch.CHUNK_UNITS + v[:, 2]], axis=1)
            base_img = len(images)
            for t in blob.get("textures", []):
                images.append(tpal[np.clip(t.astype(np.int64), 0, len(tpal) - 1)])
            for a, b, c, dd, m, texture in blob["faces"]:
                # Runtime winding (a, b, c, d) is clockwise; triangles are (a,b,c),(a,c,d).
                tid = base_img + texture - 1 if texture else -1
                ok = True
                if cam is not None and texture:
                    z = (w[[a, b, c, dd]] - cam.eye) @ cam.forward
                    ok = bool((z >= NEAR).all())
                if texture:
                    th, tw = images[tid].shape[:2]
                    corner_uv = {a: (0.0, 0.0), b: (tw, 0.0), c: (tw, th), dd: (0.0, th)}
                else:
                    corner_uv = {}
                for tri in ((a, b, c), (a, c, dd)) if dd != c else ((a, b, c),):
                    pos.append(w[list(tri)])
                    col.append(_rgb555(mats[m]))
                    tex_id.append(tid)
                    whole.append(ok)
                    uvs.append([corner_uv.get(i, (0.0, 0.0)) for i in tri])
    if not pos:
        empty = (np.zeros(0, dtype=np.int64), np.zeros((0, 3, 2)), [], np.zeros(0, dtype=bool))
        return np.zeros((0, 3, 3)), np.zeros((0, 3), dtype=np.uint8), empty
    binding = (np.array(tex_id), np.array(uvs, dtype=np.float64), images, np.array(whole))
    return np.array(pos), np.array(col, dtype=np.uint8), binding


def score_archive(archive_path: Path, ref_dir: Path, out_dir: Path, threshold: float) -> list:
    import city_preview
    from PIL import Image

    archive = city_preview.read_archive(archive_path.read_bytes())
    gy = archive["header"]["ground_y"] / 65536.0
    rows = []
    for i, vp in enumerate(VIEWPOINTS):
        cam = Camera.at(vp, gy)
        pos, col, binding = archive_triangles(archive, vp, cam)
        img = render(pos, col, cam, background=archive_ground(archive, cam), backface_cull=True,
                     textures=binding)
        Image.fromarray(img).save(out_dir / f"candidate_vp{i}.png")
        ref = np.asarray(Image.open(ref_dir / f"reference_vp{i}.png").convert("RGB"))
        rows.append(compare(ref, img, threshold))
    return rows


def main(argv=None) -> int:
    from PIL import Image

    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--glb", type=Path, required=True, help="Draco-decoded source GLB")
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--light-dir", default="-0.5,0.6,0.8")
    ap.add_argument("--ambient", type=float, default=0.42)
    ap.add_argument("--diffuse", type=float, default=0.60)
    ap.add_argument("--archive", type=Path, help="score this CITY.BIN against the references")
    ap.add_argument("--threshold", type=float, default=40.0)
    opts = ap.parse_args(argv)
    if opts.archive:
        rows = score_archive(opts.archive, opts.out, opts.out, opts.threshold)
        for i, r in enumerate(rows):
            print(f"vp{i}: {r['differs']:.1%} of pixels differ, mean error {r['mean_error']:.1f}")
        print(f"mean: {np.mean([r['differs'] for r in rows]):.1%} differ, "
              f"{np.mean([r['mean_error'] for r in rows]):.1f} mean error")
        return 0
    light = tuple(float(v) for v in opts.light_dir.split(","))
    pos, rgb, normals, ground_y = load_source(opts.glb)
    # Lawns are authored upside down and drawn double-sided: shade by |n|.
    lit_n = np.where(normals[:, 1:2] < -0.5, -normals, normals)
    colours = shade(rgb, lit_n, light, opts.ambient, opts.diffuse)
    opts.out.mkdir(parents=True, exist_ok=True)
    for i, vp in enumerate(VIEWPOINTS):
        img = render(pos, colours, Camera.at(vp, ground_y))
        Image.fromarray(img).save(opts.out / f"reference_vp{i}.png")
        print(f"reference_vp{i}.png  viewpoint {vp}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
