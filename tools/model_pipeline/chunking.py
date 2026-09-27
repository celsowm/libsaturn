"""World-space triangles, ground extraction and grid chunking for city_walk.

This is the opposite of ``model.from_gltf``: that merges every primitive into
one soup and insists on a skin, while a city needs each node's transform
applied, triangles bucketed into a spatial grid, and the flat ground pulled out
of the geometry entirely (it becomes a VDP2 bitmap, not VDP1 faces).

Everything here is numpy-vectorised except the clipper, which only touches the
few triangles that actually cross a chunk border.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field

import numpy as np

from . import gltf
from .gltf import GltfError

# Grid constants mirror examples/city_walk/city_grid.h; city_chunker asserts
# they agree with the emitted header instead of trusting either copy.
GRID_X = 16
GRID_Z = 16
CHUNK_UNITS = 32.0
ORIGIN_X = -128.0
ORIGIN_Z = -320.0
OVERHANG_UNITS = 16.0

ALPHA_OPAQUE, ALPHA_MASK, ALPHA_BLEND = "OPAQUE", "MASK", "BLEND"


@dataclass
class MaterialInfo:
    name: str
    base_linear: tuple[float, float, float]
    alpha_mode: str = ALPHA_OPAQUE
    alpha_cutoff: float = 0.5
    image: np.ndarray | None = None  # (h, w, 4) uint8, sRGB texels
    unlit: bool = False
    emissive: tuple[float, float, float] | None = None


@dataclass
class WorldTriangles:
    """Triangles in world space (glTF axes: +Y up, right-handed)."""

    pos: np.ndarray  # (N, 3, 3) float64
    uv: np.ndarray  # (N, 3, 2) float64, zeros when the primitive has none
    material: np.ndarray  # (N,) int32, -1 = no material
    node: np.ndarray  # (N,) int32, index of the glTF node


def _identity() -> np.ndarray:
    return np.eye(4)


def _node_matrix(node: dict) -> np.ndarray:
    """glTF column-major 16 floats -> a numpy matrix acting on column vectors."""
    return np.array(gltf.node_local_matrix(node), dtype=np.float64).reshape(4, 4, order="F")


def _accessor_array(glb: gltf.GlbData, index: int, width: int) -> np.ndarray:
    rows = gltf.read_accessor(glb, index).rows
    return np.asarray(rows, dtype=np.float64).reshape(-1, width)


def load_world_triangles(glb: gltf.GlbData) -> WorldTriangles:
    doc = glb.json
    nodes = doc.get("nodes", [])
    scenes = doc.get("scenes", [])
    roots = scenes[doc.get("scene", 0)].get("nodes", []) if scenes else list(range(len(nodes)))

    world: dict[int, np.ndarray] = {}
    stack = [(int(r), _identity()) for r in roots]
    while stack:
        index, parent = stack.pop()
        m = parent @ _node_matrix(nodes[index])
        world[index] = m
        for child in nodes[index].get("children", []):
            stack.append((int(child), m))

    mesh_cache: dict[int, list[tuple[np.ndarray, np.ndarray, np.ndarray, int]]] = {}

    def mesh_primitives(mesh_index: int):
        if mesh_index in mesh_cache:
            return mesh_cache[mesh_index]
        prims = []
        for primitive in doc["meshes"][mesh_index].get("primitives", []):
            gltf.check_primitive_mode(primitive, mesh_index)
            attrs = primitive.get("attributes", {})
            if "POSITION" not in attrs:
                continue
            p = _accessor_array(glb, attrs["POSITION"], 3)
            uv = (_accessor_array(glb, attrs["TEXCOORD_0"], 2)
                  if "TEXCOORD_0" in attrs else np.zeros((len(p), 2)))
            if "indices" in primitive:
                idx = np.asarray(gltf.read_indices(glb, primitive["indices"]), dtype=np.int64)
            else:
                idx = np.arange(len(p), dtype=np.int64)
            idx = idx[: len(idx) // 3 * 3].reshape(-1, 3)
            prims.append((p, uv, idx, int(primitive.get("material", -1))))
        mesh_cache[mesh_index] = prims
        return prims

    pos_parts, uv_parts, mat_parts, node_parts = [], [], [], []
    for node_index in sorted(world):
        node = nodes[node_index]
        if "mesh" not in node:
            continue
        m = world[node_index]
        flip = np.linalg.det(m[:3, :3]) < 0.0
        for p, uv, idx, material in mesh_primitives(int(node["mesh"])):
            wp = p @ m[:3, :3].T + m[:3, 3]
            tri = idx[:, ::-1] if flip else idx  # a mirror reverses the winding
            pos_parts.append(wp[tri])
            uv_parts.append(uv[tri])
            mat_parts.append(np.full(len(tri), material, dtype=np.int32))
            node_parts.append(np.full(len(tri), node_index, dtype=np.int32))
    if not pos_parts:
        raise GltfError("no triangles found in any mesh node")
    return WorldTriangles(
        pos=np.concatenate(pos_parts),
        uv=np.concatenate(uv_parts),
        material=np.concatenate(mat_parts),
        node=np.concatenate(node_parts),
    )


def _texture_image(glb: gltf.GlbData, texture_index: int) -> np.ndarray:
    tex = glb.json["textures"][texture_index]
    source = tex.get("source")
    if source is None:
        source = tex.get("extensions", {}).get("EXT_texture_webp", {}).get("source")
    if source is None:
        raise GltfError(f"texture {texture_index} has no image source")
    payload, _mime = gltf.extract_image_bytes(glb, int(source))
    width, height, pixels = gltf.decode_image_rgba(payload, f"texture {texture_index}")
    return np.asarray(pixels, dtype=np.uint8).reshape(height, width, 4)


def read_materials(glb: gltf.GlbData) -> list[MaterialInfo]:
    out: list[MaterialInfo] = []
    for m in glb.json.get("materials", []):
        pbr = m.get("pbrMetallicRoughness", {})
        factor = pbr.get("baseColorFactor", [1.0, 1.0, 1.0, 1.0])
        image = None
        if "baseColorTexture" in pbr:
            image = _texture_image(glb, int(pbr["baseColorTexture"]["index"]))
        ext = m.get("extensions", {})
        emissive = m.get("emissiveFactor")
        out.append(MaterialInfo(
            name=str(m.get("name", "")),
            # baseColorFactor is LINEAR in glTF; callers must encode it to sRGB
            # exactly once (see the project's gltf_basecolor_linear_trap note).
            base_linear=(float(factor[0]), float(factor[1]), float(factor[2])),
            alpha_mode=str(m.get("alphaMode", ALPHA_OPAQUE)),
            alpha_cutoff=float(m.get("alphaCutoff", 0.5)),
            image=image,
            unlit="KHR_materials_unlit" in ext,
            emissive=tuple(float(v) for v in emissive) if emissive else None,
        ))
    return out


def _srgb_encode(linear: np.ndarray) -> np.ndarray:
    linear = np.clip(linear, 0.0, 1.0)
    return np.where(linear <= 0.0031308, linear * 12.92, 1.055 * np.power(linear, 1.0 / 2.4) - 0.055)


def _srgb_decode(srgb: np.ndarray) -> np.ndarray:
    return np.where(srgb <= 0.04045, srgb / 12.92, np.power((srgb + 0.055) / 1.055, 2.4))


def triangle_colors(tris: WorldTriangles, materials: list[MaterialInfo]) -> tuple[np.ndarray, np.ndarray]:
    """Per-triangle albedo as sRGB bytes (N, 3), and a keep mask (N,).

    Textured triangles sample the texel under the UV centroid (nearest, glTF
    convention: v grows downward, wrap = repeat). A MASK material whose texel
    is transparent is dropped, which is what removes the tree-sprite
    cut-outs instead of drawing them as opaque squares. The material's linear
    factor multiplies the LINEAR texel and is encoded once.
    """
    n = len(tris.material)
    rgb = np.zeros((n, 3), dtype=np.float64)  # linear
    keep = np.ones(n, dtype=bool)
    centroid_uv = tris.uv.mean(axis=1)
    for mi in np.unique(tris.material):
        sel = tris.material == mi
        if mi < 0 or mi >= len(materials):
            rgb[sel] = 0.8
            continue
        mat = materials[mi]
        factor = np.array(mat.base_linear)
        if mat.image is None:
            rgb[sel] = factor
            continue
        h, w = mat.image.shape[:2]
        u = np.mod(centroid_uv[sel, 0], 1.0)
        v = np.mod(centroid_uv[sel, 1], 1.0)
        ix = np.clip((u * w).astype(np.int64), 0, w - 1)
        iy = np.clip((v * h).astype(np.int64), 0, h - 1)
        texel = mat.image[iy, ix].astype(np.float64) / 255.0
        rgb[sel] = _srgb_decode(texel[:, :3]) * factor
        if mat.alpha_mode == ALPHA_MASK:
            drop = np.zeros(n, dtype=bool)
            drop[np.nonzero(sel)[0]] = texel[:, 3] < mat.alpha_cutoff
            keep &= ~drop
    srgb = np.rint(_srgb_encode(rgb) * 255.0).astype(np.uint8)
    return srgb, keep


def triangle_normals(pos: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Unit normals (N, 3) and areas (N,) for CCW-front triangles."""
    e1 = pos[:, 1] - pos[:, 0]
    e2 = pos[:, 2] - pos[:, 0]
    cross = np.cross(e1, e2)
    length = np.linalg.norm(cross, axis=1)
    safe = np.where(length > 0.0, length, 1.0)
    return cross / safe[:, None], length * 0.5


# ---------------------------------------------------------------------------
# Ground
# ---------------------------------------------------------------------------

GROUND_TOLERANCE = 0.5  # units around ground_y that still count as the road
GROUND_UP_COS = 0.95  # normal.y above this is "faces up"


def estimate_ground_y(tris: WorldTriangles, normals: np.ndarray, areas: np.ndarray) -> float:
    """Area-weighted mode of the up-facing triangles' height (0.25-unit bins).

    The mode, not the mean: rooftops are up-facing too, and would drag a mean
    off the street. The street is where most horizontal area is."""
    up = normals[:, 1] > GROUND_UP_COS
    if not up.any():
        raise GltfError("no up-facing triangles: cannot estimate the ground height")
    y = tris.pos[up][:, :, 1].mean(axis=1)
    bins = np.floor(y / 0.25).astype(np.int64)
    weights: dict[int, float] = {}
    for b, a in zip(bins.tolist(), areas[up].tolist()):
        weights[b] = weights.get(b, 0.0) + a
    best = max(weights.items(), key=lambda kv: (kv[1], -kv[0]))[0]
    return (best + 0.5) * 0.25


def classify_ground(tris: WorldTriangles, normals: np.ndarray, ground_y: float) -> np.ndarray:
    """True for triangles that lie flat on the street: up-facing, all three
    corners within GROUND_TOLERANCE of ground_y."""
    y = tris.pos[:, :, 1]
    flat = normals[:, 1] > GROUND_UP_COS
    near = np.abs(y - ground_y).max(axis=1) <= GROUND_TOLERANCE
    return flat & near


def underground_mask(tris: WorldTriangles, ground_y: float) -> np.ndarray:
    """Triangles wholly beneath the street surface, which no one can see."""
    return tris.pos[:, :, 1].max(axis=1) <= ground_y + 1e-3


def rasterize_ground(pos: np.ndarray, colors: np.ndarray, heights: np.ndarray,
                     width: int, height: int, units_per_dot_x: int, units_per_dot_z: int,
                     fill_index: int = 0, fill_bounds=None, scale=None):
    """Top-down colour-index bitmap (height, width) of flat triangles.

    ``colors`` are palette indices >= 1; 0 stays "no ground" (transparent on
    the RBG0 plane). Lower triangles are painted first, so a raised kerb lands
    on top of the road beside it. A dot belongs to a triangle when its centre
    is inside; a triangle smaller than a dot claims the dot under its
    centroid, so slivers of lane marking are never silently lost."""
    if scale is not None:
        units_per_dot_x, units_per_dot_z = scale
    bitmap = np.zeros((height, width), dtype=np.uint8)
    layer = np.full((height, width), -1, dtype=np.int32) if scale is not None else None
    if fill_index and fill_bounds is not None:
        # The source may have no street at all under part of the city (this
        # one has none under half of it). Fill the footprint first so the
        # plane is never see-through under a building; real ground paints over.
        (min_x, min_z, max_x, max_z) = fill_bounds
        x0 = max(int(math.floor((min_x - ORIGIN_X) / units_per_dot_x)), 0)
        x1 = min(int(math.ceil((max_x - ORIGIN_X) / units_per_dot_x)), width)
        z0 = max(int(math.floor((min_z - ORIGIN_Z) / units_per_dot_z)), 0)
        z1 = min(int(math.ceil((max_z - ORIGIN_Z) / units_per_dot_z)), height)
        bitmap[z0:z1, x0:x1] = fill_index
    # Low to high; at the same height (to a millimetre) big before small, so a
    # coplanar lane dash or crosswalk stripe lands on the road it is painted on.
    e1 = pos[:, 1] - pos[:, 0]
    e2 = pos[:, 2] - pos[:, 0]
    area = 0.5 * np.abs(e1[:, 0] * e2[:, 2] - e1[:, 2] * e2[:, 0])
    order = np.lexsort((-area, np.round(heights, 3)))
    for rank, t in enumerate(order.tolist()):
        p = pos[t]
        xs = (p[:, 0] - ORIGIN_X) / units_per_dot_x
        zs = (p[:, 2] - ORIGIN_Z) / units_per_dot_z
        x0 = max(int(math.floor(xs.min())), 0)
        x1 = min(int(math.ceil(xs.max())), width)
        z0 = max(int(math.floor(zs.min())), 0)
        z1 = min(int(math.ceil(zs.max())), height)
        if x0 >= x1 or z0 >= z1:
            continue
        gx, gz = np.meshgrid(np.arange(x0, x1) + 0.5, np.arange(z0, z1) + 0.5)
        d = (zs[1] - zs[2]) * (xs[0] - xs[2]) + (xs[2] - xs[1]) * (zs[0] - zs[2])
        if abs(d) < 1e-12:
            continue
        w0 = ((zs[1] - zs[2]) * (gx - xs[2]) + (xs[2] - xs[1]) * (gz - zs[2])) / d
        w1 = ((zs[2] - zs[0]) * (gx - xs[2]) + (xs[0] - xs[2]) * (gz - zs[2])) / d
        inside = (w0 >= -1e-9) & (w1 >= -1e-9) & (1.0 - w0 - w1 >= -1e-9)
        if inside.any():
            bitmap[z0:z1, x0:x1][inside] = colors[t]
            if layer is not None:
                layer[z0:z1, x0:x1][inside] = rank
        else:
            cx = int(math.floor(xs.mean()))
            cz = int(math.floor(zs.mean()))
            if 0 <= cx < width and 0 <= cz < height:
                bitmap[cz, cx] = colors[t]
                if layer is not None:
                    layer[cz, cx] = rank
    if layer is not None:
        return bitmap, layer
    return bitmap


def rasterize_ground_marked(pos: np.ndarray, colors: np.ndarray, heights: np.ndarray,
                            width: int, height: int, units_per_dot_x: int, units_per_dot_z: int,
                            fill_index: int = 0, fill_bounds=None, sub: int = 4,
                            mark_share: float = 0.25) -> np.ndarray:
    """Like :func:`rasterize_ground`, but thin road markings survive.

    Every dot is split into ``sub`` x ``sub`` samples, painted low to high.
    A dot then takes the colour of the HIGHEST layer that covers at least
    ``mark_share`` of it: a 0.3-unit lane dash or crosswalk stripe on 1-unit
    dots owns the dots it crosses instead of vanishing between dot centres
    (point sampling kept one dot of a dash in three, at random)."""
    fine = rasterize_ground(pos, colors, heights, width * sub, height * sub,
                            1, 1, fill_index, None if fill_bounds is None else fill_bounds,
                            scale=(units_per_dot_x / sub, units_per_dot_z / sub))
    colour, layer = fine
    n = sub * sub
    c = colour.reshape(height, sub, width, sub).transpose(0, 2, 1, 3).reshape(height, width, n)
    h = layer.reshape(height, sub, width, sub).transpose(0, 2, 1, 3).reshape(height, width, n)
    counts = (c[..., :, None] == c[..., None, :]).sum(axis=-1)
    eligible = (counts >= max(1, int(round(mark_share * n)))) & (c > 0)
    # Among eligible samples, the highest layer wins; a dot with no eligible
    # sample (a sliver of everything) takes its most common colour.
    score = np.where(eligible, h, -np.inf)
    best = np.argmax(score, axis=-1)
    none = ~eligible.any(axis=-1)
    mode = np.argmax(counts, axis=-1)
    pick = np.where(none, mode, best)
    return np.take_along_axis(c, pick[..., None], axis=-1)[..., 0].astype(np.uint8)


# ---------------------------------------------------------------------------
# Colour quantisation and shading
# ---------------------------------------------------------------------------

def median_cut(colors: np.ndarray, weights: np.ndarray, k: int) -> np.ndarray:
    """Deterministic weighted median cut of sRGB bytes (N, 3) into <= k boxes.

    Returns the palette (m, 3) uint8 of weighted means. Splits the box with the
    largest weighted variance along its widest channel at the weighted median,
    so a rare-but-vivid colour (a red door) still gets its own box instead of
    being averaged into a wall."""
    uniq, inverse = np.unique(colors, axis=0, return_inverse=True)
    w = np.bincount(inverse.reshape(-1), weights=weights, minlength=len(uniq))
    if len(uniq) <= k:
        return uniq.astype(np.uint8)
    boxes = [np.arange(len(uniq))]
    lin = uniq.astype(np.float64)

    def spread(idx):
        ww = w[idx]
        total = ww.sum()
        if total <= 0.0 or len(idx) < 2:
            return -1.0
        mean = (lin[idx] * ww[:, None]).sum(axis=0) / total
        return float((((lin[idx] - mean) ** 2).sum(axis=1) * ww).sum())

    while len(boxes) < k:
        scores = [spread(b) for b in boxes]
        target = int(np.argmax(scores))
        if scores[target] <= 0.0:
            break
        idx = boxes.pop(target)
        span = lin[idx].max(axis=0) - lin[idx].min(axis=0)
        axis = int(np.argmax(span))
        order = idx[np.argsort(lin[idx][:, axis], kind="stable")]
        cum = np.cumsum(w[order])
        cut = int(np.searchsorted(cum, cum[-1] / 2.0))
        cut = min(max(cut, 0), len(order) - 2) + 1
        boxes.append(order[:cut])
        boxes.append(order[cut:])
    palette = []
    for b in boxes:
        ww = w[b]
        palette.append(np.rint((lin[b] * ww[:, None]).sum(axis=0) / max(ww.sum(), 1e-12)))
    return np.asarray(palette, dtype=np.uint8)


def nearest_palette(colors: np.ndarray, palette: np.ndarray) -> np.ndarray:
    """Index of the closest palette entry (Euclidean in sRGB bytes)."""
    out = np.empty(len(colors), dtype=np.int32)
    pal = palette.astype(np.float64)
    step = 65536
    for i in range(0, len(colors), step):
        chunk = colors[i:i + step].astype(np.float64)
        d = ((chunk[:, None, :] - pal[None, :, :]) ** 2).sum(axis=2)
        out[i:i + step] = np.argmin(d, axis=1)
    return out


def shade_rgb555(base_srgb: np.ndarray, level: int, levels: int,
                 ambient: float, diffuse: float) -> int:
    """RGB555 of a base colour lit at ``level`` of ``levels`` (linear light,
    encoded to sRGB once), as face_colors.shade_palette does."""
    lin = _srgb_decode(np.asarray(base_srgb, dtype=np.float64) / 255.0)
    light = ambient + diffuse * (level / (levels - 1) if levels > 1 else 1.0)
    enc = _srgb_encode(lin * light)
    r, g, b = (min(31, int(round(float(c) * 31.0))) for c in enc)
    return (b << 10) | (g << 5) | r


def shade_levels(normals: np.ndarray, light_dir: tuple[float, float, float], levels: int) -> np.ndarray:
    """Lambert level per triangle, 0..levels-1, under one fixed light."""
    l = np.asarray(light_dir, dtype=np.float64)
    l = l / np.linalg.norm(l)
    d = np.clip(normals @ l, 0.0, 1.0)
    return np.rint(d * (levels - 1)).astype(np.int32)


# ---------------------------------------------------------------------------
# Grid clipping and chunk assignment
# ---------------------------------------------------------------------------

def _split_polygon(poly, axis: int, value: float):
    """Sutherland-Hodgman split of a convex polygon by the plane p[axis]=value."""
    below: list[np.ndarray] = []
    above: list[np.ndarray] = []
    n = len(poly)
    for i in range(n):
        a, b = poly[i], poly[(i + 1) % n]
        da, db = a[axis] - value, b[axis] - value
        if da <= 0.0:
            below.append(a)
        if da >= 0.0:
            above.append(a)
        if (da < 0.0 < db) or (db < 0.0 < da):
            t = da / (da - db)
            p = a + (b - a) * t
            p[axis] = value
            below.append(p)
            above.append(p.copy())
    return below, above


def clip_to_grid(pos: np.ndarray, attrs: dict):
    """Cut every triangle at the chunk borders so each piece lies in one cell.

    A ground plane or a long wall would otherwise keep its centroid in one
    chunk while its corners sit hundreds of units away, overflowing both the
    int16 quantisation range and the overhang the runtime guarantees. Only the
    triangles that actually cross a border are touched."""
    lo = pos.min(axis=1)
    hi = pos.max(axis=1)
    cx0 = np.floor((lo[:, 0] - ORIGIN_X) / CHUNK_UNITS)
    cx1 = np.floor((hi[:, 0] - ORIGIN_X) / CHUNK_UNITS)
    cz0 = np.floor((lo[:, 2] - ORIGIN_Z) / CHUNK_UNITS)
    cz1 = np.floor((hi[:, 2] - ORIGIN_Z) / CHUNK_UNITS)
    crossing = (cx0 != cx1) | (cz0 != cz1)
    out_pos = [pos[~crossing]]
    out_attrs = {k: [v[~crossing]] for k, v in attrs.items()}
    new_pos: list[np.ndarray] = []
    new_src: list[int] = []
    pieces_max = 0
    for t in np.nonzero(crossing)[0].tolist():
        polys = [[pos[t][0].copy(), pos[t][1].copy(), pos[t][2].copy()]]
        for axis, origin in ((0, ORIGIN_X), (2, ORIGIN_Z)):
            a0 = int(math.floor((lo[t][axis] - origin) / CHUNK_UNITS))
            a1 = int(math.floor((hi[t][axis] - origin) / CHUNK_UNITS))
            for k in range(a0 + 1, a1 + 1):
                value = origin + k * CHUNK_UNITS
                nxt = []
                for poly in polys:
                    lo_side, hi_side = _split_polygon(poly, axis, value)
                    nxt.extend(p for p in (lo_side, hi_side) if len(p) >= 3)
                polys = nxt
        count = 0
        for poly in polys:
            for i in range(1, len(poly) - 1):
                tri = np.array([poly[0], poly[i], poly[i + 1]])
                if np.linalg.norm(np.cross(tri[1] - tri[0], tri[2] - tri[0])) < 1e-9:
                    continue
                new_pos.append(tri)
                new_src.append(t)
                count += 1
        pieces_max = max(pieces_max, count)
    if new_pos:
        out_pos.append(np.asarray(new_pos))
        src = np.asarray(new_src, dtype=np.int64)
        for k, v in attrs.items():
            out_attrs[k].append(v[src])
    return np.concatenate(out_pos), {k: np.concatenate(v) for k, v in out_attrs.items()}, {
        "clipped_triangles": int(crossing.sum()),
        "pieces_created": len(new_pos),
        "max_pieces_from_one": pieces_max,
    }


def chunk_of(pos: np.ndarray) -> np.ndarray:
    """Chunk index (cz * GRID_X + cx) of each triangle by its centroid, clamped
    onto the grid so a stray triangle at the very edge lands in a real chunk."""
    c = pos.mean(axis=1)
    cx = np.clip(np.floor((c[:, 0] - ORIGIN_X) / CHUNK_UNITS).astype(np.int64), 0, GRID_X - 1)
    cz = np.clip(np.floor((c[:, 2] - ORIGIN_Z) / CHUNK_UNITS).astype(np.int64), 0, GRID_Z - 1)
    return cz * GRID_X + cx


# ---------------------------------------------------------------------------
# Triangle pairs -> quads (runtime winding)
# ---------------------------------------------------------------------------

def _unit(v: np.ndarray) -> np.ndarray:
    n = float(np.linalg.norm(v))
    return v / n if n > 0.0 else v


def merge_to_quads(vertices: np.ndarray, triangles, materials, max_fold_deg: float = 35.0):
    """Greedy pairing of edge-sharing, same-material triangles into convex
    quads. Returns ``(corners, material)`` with corners in CCW-outward order
    (3 for a lone triangle, 4 for a quad); ``to_runtime_face`` reverses them.

    The two triangles must fold no more than ``max_fold_deg`` and the four
    corners must form a convex polygon in their common plane, or the VDP1 would
    draw the quad's bilinear surface instead of the two triangles."""
    fold_cos = math.cos(math.radians(max_fold_deg))
    edge_tris: dict = {}
    for ti, (a, b, c) in enumerate(triangles):
        for u, v in ((a, b), (b, c), (c, a)):
            edge_tris.setdefault((min(u, v), max(u, v)), []).append(ti)

    candidates = []
    for (u, v), owners in edge_tris.items():
        if len(owners) != 2:
            continue
        t1, t2 = owners
        if materials[t1] != materials[t2]:
            continue
        tri1, tri2 = triangles[t1], triangles[t2]
        k = next((i for i in range(3) if {tri1[i], tri1[(i + 1) % 3]} == {u, v}), None)
        a, b, c = tri1[k], tri1[(k + 1) % 3], tri1[(k + 2) % 3]
        m = next((i for i in range(3) if tri2[i] == b and tri2[(i + 1) % 3] == a), None)
        if m is None:
            continue  # inconsistent winding across the shared edge
        d = tri2[(m + 2) % 3]
        if d in (a, b, c):
            continue
        n1 = _unit(np.cross(vertices[b] - vertices[a], vertices[c] - vertices[a]))
        n2 = _unit(np.cross(vertices[a] - vertices[b], vertices[d] - vertices[b]))
        fold = float(n1 @ n2)
        if fold < fold_cos:
            continue
        poly = (a, d, b, c)  # CCW outward (t1 = a b c, t2 = b a d)
        nrm = _unit(n1 + n2)
        ok = True
        for i in range(4):
            p0, p1, p2 = vertices[poly[i - 1]], vertices[poly[i]], vertices[poly[(i + 1) % 4]]
            if float(np.cross(p1 - p0, p2 - p1) @ nrm) <= 1e-12:
                ok = False
                break
        if not ok:
            continue
        hinge = float(np.linalg.norm(vertices[b] - vertices[a]))
        longest = max(float(np.linalg.norm(vertices[x] - vertices[y]))
                      for x, y in ((a, b), (a, c), (b, c), (a, d), (b, d)))
        score = (1.0 - fold) + (1.0 - hinge / longest if longest > 0.0 else 1.0)
        candidates.append((score, t1, t2, poly))
    candidates.sort(key=lambda item: (item[0], item[1], item[2]))
    used = [False] * len(triangles)
    merged: dict = {}
    for _score, t1, t2, poly in candidates:
        if used[t1] or used[t2]:
            continue
        used[t1] = used[t2] = True
        merged[min(t1, t2)] = (max(t1, t2), poly)
    faces = []
    skip: set = set()
    for ti, tri in enumerate(triangles):
        if ti in skip:
            continue
        if ti in merged:
            other, poly = merged[ti]
            skip.add(other)
            faces.append((poly, materials[ti]))
        else:
            faces.append((tuple(tri), materials[ti]))
    return faces


def to_runtime_face(corners):
    """CCW-outward polygon -> the runtime A,B,C,D (mesh3d.h: the outward
    normal is cross(D - A, B - A), i.e. the reverse of CCW). A triangle
    repeats its last corner (d == c)."""
    if len(corners) == 3:
        a, b, c = corners
        return (a, c, b, b)
    p0, p1, p2, p3 = corners
    return (p0, p3, p2, p1)


# ---------------------------------------------------------------------------
# Collision boxes
# ---------------------------------------------------------------------------

def _greedy_rectangles(occupied: np.ndarray):
    """Cover the True cells with row runs, merged downward while the next row
    has the identical run. Deterministic, not minimal."""
    n_z, n_x = occupied.shape
    rects: list = []
    active: dict = {}
    for z in range(n_z):
        row_runs = []
        x = 0
        while x < n_x:
            if occupied[z, x]:
                start = x
                while x < n_x and occupied[z, x]:
                    x += 1
                row_runs.append((start, x - 1))
            else:
                x += 1
        next_active: dict = {}
        for run in row_runs:
            if run in active:
                rect = active[run]
                rect[3] = z
                next_active[run] = rect
            else:
                rect = [run[0], z, run[1], z]
                rects.append(rect)
                next_active[run] = rect
        active = next_active
    return [tuple(r) for r in rects]


def collision_boxes(pos: np.ndarray, cell_x0: float, cell_z0: float, ground_y: float,
                    max_boxes: int, min_height: float = 1.2):
    """Axis-aligned boxes (cx, cy, cz, hx, hy, hz) covering what blocks a
    walker in one chunk.

    Anything reaching more than ``min_height`` above the street blocks. Roofs
    fill the inside of a building (its walls are hollow shells), walls mark
    their footprint by bounding box. The occupancy grid starts at 1 unit and is
    coarsened until the greedy cover fits ``max_boxes``; a coarser grid only
    ever grows the boxes, so a walker is never let through."""
    if len(pos) == 0:
        return []
    pos = pos[pos[:, :, 1].max(axis=1) > ground_y + min_height]
    if len(pos) == 0:
        return []
    for cell in (1.0, 2.0, 4.0, 8.0, 16.0, 32.0):
        n = int(round(CHUNK_UNITS / cell))
        occupied = np.zeros((n, n), dtype=bool)
        height = np.full((n, n), ground_y, dtype=np.float64)
        for p in pos:
            x = (p[:, 0] - cell_x0) / cell
            z = (p[:, 2] - cell_z0) / cell
            x0 = max(int(math.floor(x.min())), 0)
            x1 = min(int(math.floor(x.max())), n - 1)
            z0 = max(int(math.floor(z.min())), 0)
            z1 = min(int(math.floor(z.max())), n - 1)
            if x0 > x1 or z0 > z1:
                continue
            occupied[z0:z1 + 1, x0:x1 + 1] = True
            block = height[z0:z1 + 1, x0:x1 + 1]
            np.maximum(block, float(p[:, 1].max()), out=block)
        rects = _greedy_rectangles(occupied)
        if len(rects) <= max_boxes:
            boxes = []
            for (rx0, rz0, rx1, rz1) in rects:
                top_y = float(height[rz0:rz1 + 1, rx0:rx1 + 1].max())
                hx = (rx1 - rx0 + 1) * cell / 2.0
                hz = (rz1 - rz0 + 1) * cell / 2.0
                hy = (top_y - ground_y) / 2.0
                boxes.append((cell_x0 + rx0 * cell + hx, ground_y + hy,
                              cell_z0 + rz0 * cell + hz, hx, hy, hz))
            return boxes
    raise GltfError(f"collision cover needs more than {max_boxes} boxes even at 32-unit cells")
