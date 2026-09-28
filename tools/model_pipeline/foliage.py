"""Build-time foliage impostors for city_walk.

The source city has two vegetation representations:
- alpha-masked TreeSpriteAtlas cards: preserve one source card per object;
- dense opaque low-poly vegetation (hundreds of triangles per object): render
  one orthographic low-poly impostor instead of feeding it to the block sampler.

Both become one camera-facing INDEX8 cutout quad at runtime.  This keeps the
source silhouette/detail while bounding every plant to one VDP1 face.
"""
from __future__ import annotations

from dataclasses import dataclass
import math

import numpy as np
from PIL import Image, ImageDraw

from . import chunking as ch
from . import emit_bin


# Screen-facing foliage needs a minimum pixel footprint. The old 2/1/0.5
# texels-per-unit rule turned a two-unit-wide near tree into an 8 px sprite.
# Keep close cards crisp first; overloaded chunks drop the smallest foliage
# item before going below these per-LOD floors.
LOD_TEXELS_PER_UNIT = (8.0, 5.0, 3.0)
LOD_MIN_WIDTH = (32, 24, 16)
LOD_MAX_WIDTH = (64, 48, 32)
LOD_MAX_HEIGHT = (128, 96, 64)
LOD_MAX_ITEMS = (255, 8, 2)
MAX_SOURCE_PIXELS = 128


@dataclass
class Billboard:
    node: int
    chunk: int
    center_x: float
    center_z: float
    bottom_y: float
    top_y: float
    width: float
    rgba: np.ndarray
    source_kind: str

    @property
    def area(self) -> float:
        return self.width * max(0.0, self.top_y - self.bottom_y)


def foliage_material_ids(materials) -> set[int]:
    """Materials that must never enter the architectural block sampler."""
    return {
        i for i, m in enumerate(materials)
        if m.alpha_mode == ch.ALPHA_MASK or m.name.strip().lower() == "vegetation"
    }


def _unit_horizontal(v):
    out = np.asarray([v[0], 0.0, v[2]], dtype=np.float64)
    n = float(np.linalg.norm(out))
    if n < 1e-9:
        return np.asarray([0.0, 0.0, 1.0])
    return out / n


def _resize_source(rgba: np.ndarray, max_pixels: int = MAX_SOURCE_PIXELS) -> np.ndarray:
    h, w = rgba.shape[:2]
    if max(h, w) <= max_pixels:
        return rgba.copy()
    scale = max_pixels / float(max(h, w))
    nw, nh = max(1, int(round(w * scale))), max(1, int(round(h * scale)))
    return np.asarray(Image.fromarray(rgba, "RGBA").resize((nw, nh), Image.Resampling.LANCZOS))


def _masked_card(pos: np.ndarray, uv: np.ndarray, mat, node: int) -> tuple[np.ndarray, float]:
    """Pick one of the source object's crossed cards and crop its real atlas region."""
    normals, areas = ch.triangle_normals(pos)
    best = None
    for seed in range(len(pos)):
        n = normals[seed]
        group = np.abs(normals @ n) >= 0.98
        score = float(areas[group].sum())
        key = (score, -seed)
        if best is None or key > best[0]:
            best = (key, group, n)
    group, normal = best[1], best[2]
    p, t = pos[group], uv[group]
    view = _unit_horizontal(normal)
    right = np.asarray([view[2], 0.0, -view[0]])
    projected = p.reshape(-1, 3) @ right
    width = max(float(projected.max() - projected.min()), 0.25)

    if mat.image is None:
        img = np.zeros((16, 16, 4), dtype=np.uint8)
        img[2:-2, 6:10] = (60, 90, 35, 255)
        return img, width

    flat_uv = t.reshape(-1, 2)
    u0, v0 = flat_uv.min(axis=0)
    u1, v1 = flat_uv.max(axis=0)
    # TreeSpriteAtlas cells are ordinary non-wrapping subrectangles.
    u0, v0 = max(0.0, float(u0)), max(0.0, float(v0))
    u1, v1 = min(1.0, float(u1)), min(1.0, float(v1))
    h, w = mat.image.shape[:2]
    x0 = max(0, min(w - 1, int(math.floor(u0 * w))))
    x1 = max(x0 + 1, min(w, int(math.ceil(u1 * w))))
    y0 = max(0, min(h - 1, int(math.floor(v0 * h))))
    y1 = max(y0 + 1, min(h, int(math.ceil(v1 * h))))
    image = mat.image[y0:y1, x0:x1].copy()

    # Match world +Y to image top. glTF UV v grows downward in this pipeline.
    yy = p.reshape(-1, 3)[:, 1]
    vv = flat_uv[:, 1]
    if len(yy) >= 2:
        mid = (float(yy.min()) + float(yy.max())) * 0.5
        top_v = float(vv[yy >= mid].mean()) if np.any(yy >= mid) else float(vv.mean())
        bot_v = float(vv[yy < mid].mean()) if np.any(yy < mid) else float(vv.mean())
        if top_v > bot_v:
            image = image[::-1].copy()

    alpha = image[..., 3] >= int(round(mat.alpha_cutoff * 255.0))
    image[..., 3] = np.where(alpha, 255, 0).astype(np.uint8)
    factor = np.asarray(mat.base_linear, dtype=np.float64)
    if not np.allclose(factor, 1.0):
        lin = ch._srgb_decode(image[..., :3].astype(np.float64) / 255.0) * factor
        image[..., :3] = np.clip(np.rint(ch._srgb_encode(lin) * 255.0), 0, 255).astype(np.uint8)
    return _resize_source(image), width


def _mesh_impostor(pos: np.ndarray, colours: np.ndarray) -> tuple[np.ndarray, float]:
    """Orthographically rasterize dense source vegetation as a transparent low-poly sprite."""
    view = np.asarray([1.0, 0.0, 1.0], dtype=np.float64)
    view /= np.linalg.norm(view)
    right = np.asarray([view[2], 0.0, -view[0]])
    px = pos @ right
    py = pos[..., 1]
    depth = pos @ view
    x0, x1 = float(px.min()), float(px.max())
    y0, y1 = float(py.min()), float(py.max())
    width, height = max(x1 - x0, 0.25), max(y1 - y0, 0.25)
    if width >= height:
        w = MAX_SOURCE_PIXELS
        h = max(8, int(round(MAX_SOURCE_PIXELS * height / width)))
    else:
        h = MAX_SOURCE_PIXELS
        w = max(8, int(round(MAX_SOURCE_PIXELS * width / height)))
    image = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    order = np.argsort(depth.mean(axis=1))
    sx = (w - 1) / width
    sy = (h - 1) / height
    for ti in order.tolist():
        pts = [((float(px[ti, j]) - x0) * sx,
                (y1 - float(py[ti, j])) * sy) for j in range(3)]
        c = tuple(int(v) for v in colours[ti])
        draw.polygon(pts, fill=(c[0], c[1], c[2], 255))
    return np.asarray(image), width


def build_billboards(tris, materials, rgb, material_ids: set[int]):
    """Return chunk -> Billboard[] for all selected foliage source triangles."""
    selected = np.isin(tris.material, list(material_ids))
    by_chunk: dict[int, list[Billboard]] = {}
    if not selected.any():
        return by_chunk
    for node in np.unique(tris.node[selected]).tolist():
        sel = selected & (tris.node == node)
        ids, counts = np.unique(tris.material[sel], return_counts=True)
        mi = int(ids[int(np.argmax(counts))])
        same = sel & (tris.material == mi)
        pos, uv, cols = tris.pos[same], tris.uv[same], rgb[same]
        if len(pos) == 0:
            continue
        mat = materials[mi]
        flat = pos.reshape(-1, 3)
        center_x = float((flat[:, 0].min() + flat[:, 0].max()) * 0.5)
        center_z = float((flat[:, 2].min() + flat[:, 2].max()) * 0.5)
        bottom_y, top_y = float(flat[:, 1].min()), float(flat[:, 1].max())
        if mat.alpha_mode == ch.ALPHA_MASK:
            rgba, width = _masked_card(pos, uv, mat, node)
            kind = "source_mask"
        else:
            rgba, width = _mesh_impostor(pos, cols)
            kind = "mesh_impostor"
        cx = int(math.floor((center_x - ch.ORIGIN_X) / ch.CHUNK_UNITS))
        cz = int(math.floor((center_z - ch.ORIGIN_Z) / ch.CHUNK_UNITS))
        if not (0 <= cx < ch.GRID_X and 0 <= cz < ch.GRID_Z):
            continue
        chunk = cz * ch.GRID_X + cx
        by_chunk.setdefault(chunk, []).append(Billboard(
            node=int(node), chunk=chunk, center_x=center_x, center_z=center_z,
            bottom_y=bottom_y, top_y=top_y, width=max(width, 0.25), rgba=rgba,
            source_kind=kind))
    for items in by_chunk.values():
        items.sort(key=lambda b: (-b.area, b.node))
    return by_chunk


def select_for_lod(items: list[Billboard], lod: int, count: int | None = None):
    limit = LOD_MAX_ITEMS[lod]
    n = min(len(items), limit if count is None else count)
    return items[:n]


def texture_for_lod(item: Billboard, lod: int):
    """RGBA cutout with a real visual floor instead of tiny world-space texels."""
    height_units = max(item.top_y - item.bottom_y, 0.25)
    width_units = max(item.width, 0.25)
    rate = LOD_TEXELS_PER_UNIT[lod]
    requested = int(math.ceil(width_units * rate / 8.0)) * 8
    w = min(LOD_MAX_WIDTH[lod], max(LOD_MIN_WIDTH[lod], requested))
    # Preserve the world-space aspect when the width floor lifts a small tree;
    # otherwise a 32 px-wide card could remain only 8 px tall.
    aspect_h = int(math.ceil(w * height_units / width_units))
    density_h = int(math.ceil(height_units * rate))
    h = min(LOD_MAX_HEIGHT[lod], max(8, aspect_h, density_h))
    image = Image.fromarray(item.rgba, "RGBA").resize((w, h), Image.Resampling.LANCZOS)
    rgba = np.asarray(image).copy()
    rgba[..., 3] = np.where(rgba[..., 3] >= 96, 255, 0).astype(np.uint8)
    flags = emit_bin.TEXTURE_FLAG_CUTOUT | emit_bin.TEXTURE_FLAG_BILLBOARD
    return rgba, flags


def texture_set_for_lod(items: list[Billboard], lod: int):
    """Unique textures plus 1-based per-item references.

    Repeated source cards are common in the city. Keeping one copy per chunk
    buys resolution without spending the slot's VRAM again for identical trees.
    """
    unique = []
    refs = []
    lookup = {}
    for item in items:
        rgba, flags = texture_for_lod(item, lod)
        key = (rgba.shape[1], rgba.shape[0], flags, rgba.tobytes())
        index = lookup.get(key)
        if index is None:
            index = len(unique) + 1
            lookup[key] = index
            unique.append((rgba, flags))
        refs.append(index)
    return unique, refs


def world_quad(item: Billboard) -> np.ndarray:
    """X-aligned template; runtime BILLBOARD rotates it around the Y axis."""
    half = item.width * 0.5
    x0, x1 = item.center_x - half, item.center_x + half
    y0, y1, z = item.bottom_y, item.top_y, item.center_z
    # Runtime order A(top-left), B(top-right), C(bottom-right), D(bottom-left).
    return np.asarray([
        [x0, y1, z], [x1, y1, z], [x1, y0, z], [x0, y0, z]
    ], dtype=np.float64)
