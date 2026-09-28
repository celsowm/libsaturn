"""City as extruded blocks: a top-down surface model and the faces built from it.

Generic mesh simplification cannot take a 1,700-triangle city chunk down to
~170 faces without tearing it: buildings are hollow shells, and removing 90%
of their triangles leaves floating roof slabs, missing walls and shards. What a
walker at street level sees of a low-poly city is its skyline of boxes, so we
rebuild it as boxes on purpose:

1. ``surface_model`` samples every triangle into a 0.5-unit top-down grid and
   keeps, per cell, the highest surface, the colour on top of it and the most
   common wall colour around it (a digital surface model).
2. ``block_levels`` turns that into flat-topped regions (same top colour, same
   height band), merging slivers away. Coarser levels merge harder.
3. ``chunk_faces`` extrudes the regions of one chunk: walls only where a
   neighbour is lower, tops only where the eye (fixed at street level) can see
   them. Output faces share vertices and are always closed: a block can never
   lose a wall, because walls are derived from height steps, not kept or
   dropped triangle by triangle.

Everything low (road, sidewalk, lawn) goes into the VDP2 ground bitmap instead,
so the VDP1 draws only what stands up.
"""

from __future__ import annotations

import math
from dataclasses import dataclass

import numpy as np
from scipy import ndimage

from . import chunking as ch

CELL = 0.5  # units per surface-model cell
GRID_CELLS = int(round(ch.CHUNK_UNITS * ch.GRID_X / CELL))  # 1024
CHUNK_CELLS = int(round(ch.CHUNK_UNITS / CELL))  # 64
LOW_UNITS = 0.6  # at most this far above the street is "ground" (VDP2)
UP_NY = 0.5  # |normal.y| above this: a surface you stand on
WALL_NY = 0.5  # |normal.y| below this: a wall
EMPTY = -1


@dataclass
class SurfaceModel:
    """Per-cell top-down description, grid (z, x), origin at the grid origin."""

    height: np.ndarray  # (N, N) float32, units above the street, -inf = nothing
    top: np.ndarray  # (N, N) int32 palette index on top, EMPTY = none
    wall: np.ndarray  # (N, N) int32 dominant wall palette index, EMPTY = none
    wall_sum: np.ndarray | None = None  # (N, N, 3) summed sRGB of wall samples
    wall_count: np.ndarray | None = None  # (N, N) wall samples per cell
    palette: np.ndarray | None = None  # (K, 3) sRGB the indices refer to


def _barycentric_grid(n: int) -> np.ndarray:
    pts = [(i / n, j / n) for i in range(n + 1) for j in range(n + 1 - i)]
    b = np.array(pts, dtype=np.float64)
    return np.column_stack([1.0 - b[:, 0] - b[:, 1], b[:, 0], b[:, 1]])


def _samples(pos: np.ndarray, spacing: float, batch_points: int = 3_000_000):
    """Yield ``(triangle_index, points)`` covering each triangle at ``spacing``."""
    edges = np.stack([pos[:, 1] - pos[:, 0], pos[:, 2] - pos[:, 1], pos[:, 0] - pos[:, 2]], axis=1)
    longest = np.linalg.norm(edges, axis=2).max(axis=1)
    subdiv = np.clip(np.ceil(longest / spacing), 1, 512).astype(np.int64)
    for n in np.unique(subdiv).tolist():
        tri = np.nonzero(subdiv == n)[0]
        bary = _barycentric_grid(int(n))
        step = max(1, batch_points // len(bary))
        for s in range(0, len(tri), step):
            sel = tri[s:s + step]
            # Pulled a hair toward the centroid: a sample exactly on an edge
            # would otherwise land in the cell beyond the face.
            bary_in = bary * (1.0 - 1e-3) + (1e-3 / 3.0)
            pts = np.einsum("pk,tkd->tpd", bary_in, pos[sel])
            yield np.repeat(sel, len(bary)), pts.reshape(-1, 3)


def surface_model(pos: np.ndarray, colour: np.ndarray, normals: np.ndarray,
                  ground_y: float, palette: np.ndarray | None = None) -> SurfaceModel:
    """Sample triangles ``pos`` (N, 3, 3) with palette ``colour`` (N,) into cells."""
    n = GRID_CELLS
    height = np.full(n * n, -np.inf, dtype=np.float64)
    top_y = np.full(n * n, -np.inf, dtype=np.float64)
    top = np.full(n * n, EMPTY, dtype=np.int64)
    wall_keys = []
    wall_sum = np.zeros((n * n, 3), dtype=np.float64)
    wall_count = np.zeros(n * n, dtype=np.float64)
    ny = normals[:, 1]
    # Horizontal faces count on either side: this source authors some lawns
    # upside down (the viewer draws them double-sided), and for a closed
    # solid the highest surface faces up anyway.
    # Walls are nudged a hair inside so a sample exactly on a cell border
    # belongs to the building, not to the street cell outside it.
    nudge = np.where(np.abs(ny)[:, None] < WALL_NY, -normals * 0.05, 0.0)
    for tri, pts in _samples(pos, CELL * 0.5):
        pts = pts + nudge[tri]
        ix = np.floor((pts[:, 0] - ch.ORIGIN_X) / CELL).astype(np.int64)
        iz = np.floor((pts[:, 2] - ch.ORIGIN_Z) / CELL).astype(np.int64)
        ok = (ix >= 0) & (ix < n) & (iz >= 0) & (iz < n)
        cell = (iz * n + ix)[ok]
        y = pts[ok, 1] - ground_y
        t = tri[ok]
        np.maximum.at(height, cell, y)
        up = np.abs(ny[t]) > UP_NY
        if up.any():
            # Highest up-facing sample per cell wins the top colour.
            cu, yu, col = cell[up], y[up], colour[t[up]]
            order = np.lexsort((yu, cu))
            cu, yu, col = cu[order], yu[order], col[order]
            last = np.r_[cu[1:] != cu[:-1], True]
            cu, yu, col = cu[last], yu[last], col[last]
            better = yu > top_y[cu]
            top_y[cu[better]] = yu[better]
            top[cu[better]] = col[better]
        wl = np.abs(ny[t]) < WALL_NY
        if wl.any():
            wall_keys.append(cell[wl] * 256 + colour[t[wl]])
            if palette is not None:
                rgb = palette[colour[t[wl]]].astype(np.float64)
                for k in range(3):
                    wall_sum[:, k] += np.bincount(cell[wl], weights=rgb[:, k], minlength=n * n)
                wall_count += np.bincount(cell[wl], minlength=n * n)
    wall = np.full(n * n, EMPTY, dtype=np.int64)
    if wall_keys:
        keys, counts = np.unique(np.concatenate(wall_keys), return_counts=True)
        cells = keys // 256
        order = np.lexsort((-counts, cells))
        cells_s, keys_s = cells[order], keys[order]
        first = np.r_[True, cells_s[1:] != cells_s[:-1]]
        wall[cells_s[first]] = keys_s[first] % 256
    # A cell with a wall but no up-facing sample (a parapet rim, a pole) shows
    # its wall colour on top; one with only a top shows the top on its walls.
    top = np.where(top == EMPTY, wall, top)
    wall = np.where(wall == EMPTY, top, wall)
    return SurfaceModel(height.reshape(n, n).astype(np.float32),
                        top.reshape(n, n).astype(np.int32),
                        wall.reshape(n, n).astype(np.int32),
                        wall_sum.reshape(n, n, 3), wall_count.reshape(n, n),
                        None if palette is None else np.asarray(palette))


# ---------------------------------------------------------------------------
# Ground bitmap
# ---------------------------------------------------------------------------

def ground_bitmap(sm: SurfaceModel, units_per_dot_x: int, units_per_dot_z: int,
                  fill_index: int, footprint: np.ndarray | None = None) -> np.ndarray:
    """(H, W) uint8 palette indices (palette index + 1; 0 = transparent).

    A dot takes the most common colour of its cells: the top of the low ones
    (road, lawn, kerb), the wall colour under a building, and ``fill_index``
    where the source has nothing but ``footprint`` says the city is."""
    low = sm.height <= LOW_UNITS
    has = np.isfinite(sm.height)
    per_cell = np.where(has & low, sm.top + 1, np.where(has, sm.wall + 1, 0)).astype(np.int32)
    if fill_index and footprint is not None:
        per_cell = np.where((per_cell == 0) & footprint, fill_index, per_cell)
    fx = int(round(units_per_dot_x / CELL))
    fz = int(round(units_per_dot_z / CELL))
    n = GRID_CELLS
    blocks = per_cell.reshape(n // fz, fz, n // fx, fx).transpose(0, 2, 1, 3).reshape(n // fz, n // fx, fz * fx)
    # Prefer any real colour over "nothing" so thin lane marks and kerbs survive.
    votes = (blocks[..., :, None] == blocks[..., None, :]).sum(axis=-1)
    votes = np.where(blocks == 0, 0, votes)
    pick = np.argmax(votes, axis=-1)
    return np.take_along_axis(blocks, pick[..., None], axis=-1)[..., 0].astype(np.uint8)


def city_footprint(min_x: float, min_z: float, max_x: float, max_z: float) -> np.ndarray:
    """Cells inside a world rectangle: the source's base slab, which spans the
    empty corners of the city too, so the ground plane is never see-through
    there (a hole shows the sky below the horizon)."""
    n = GRID_CELLS
    x0 = max(int(math.floor((min_x - ch.ORIGIN_X) / CELL)), 0)
    x1 = min(int(math.ceil((max_x - ch.ORIGIN_X) / CELL)), n)
    z0 = max(int(math.floor((min_z - ch.ORIGIN_Z) / CELL)), 0)
    z1 = min(int(math.ceil((max_z - ch.ORIGIN_Z) / CELL)), n)
    out = np.zeros((n, n), dtype=bool)
    out[z0:z1, x0:x1] = True
    return out


# ---------------------------------------------------------------------------
# Block levels
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class LevelSpec:
    close_cells: int  # join blocks closer than this many cells into one
    open_cells: int  # then remove anything thinner than this many cells
    height_step: float  # units per height band
    min_area: float  # square units; smaller regions merge into a neighbour


# Finest first. A chunk takes the finest level whose faces fit its LOD caps.
# Coarser levels join neighbours before they remove slivers, so a dense block
# of small buildings becomes one big block instead of disappearing.
LEVELS = (
    # Street-near shell: preserve half-unit recesses and narrow architectural
    # volumes. Dense chunks automatically fall back to the old levels below.
    LevelSpec(0, 1, 0.5, 0.25),
    LevelSpec(0, 2, 1.0, 1.0),
    LevelSpec(0, 2, 1.5, 2.0),
    LevelSpec(2, 3, 2.0, 4.0),
    LevelSpec(3, 4, 3.0, 8.0),
    LevelSpec(4, 4, 4.0, 16.0),
    LevelSpec(6, 6, 6.0, 32.0),
    LevelSpec(8, 8, 8.0, 64.0),
    LevelSpec(12, 12, 12.0, 128.0),
    LevelSpec(16, 16, 16.0, 256.0),
)


@dataclass
class BlockLevel:
    height: np.ndarray  # (N, N) float32, 0 = ground, else block top above the street
    region: np.ndarray  # (N, N) int32, 0 = ground
    top: np.ndarray  # per region palette index on top
    wall: np.ndarray  # per region palette index on the walls


def _mode(values: np.ndarray, labels: np.ndarray, count: int) -> np.ndarray:
    """Most common non-negative value per label 1..count (EMPTY when none)."""
    out = np.full(count + 1, EMPTY, dtype=np.int64)
    ok = (labels > 0) & (values >= 0)
    if not ok.any():
        return out
    keys = labels[ok].astype(np.int64) * 256 + values[ok]
    k, c = np.unique(keys, return_counts=True)
    lab = k // 256
    order = np.lexsort((-c, lab))
    lab_s, k_s = lab[order], k[order]
    first = np.r_[True, lab_s[1:] != lab_s[:-1]]
    out[lab_s[first]] = k_s[first] % 256
    return out


def block_level(sm: SurfaceModel, spec: LevelSpec) -> BlockLevel:
    source_tall = np.isfinite(sm.height) & (sm.height > LOW_UNITS)
    tall = source_tall
    src_h, src_top = np.where(source_tall, sm.height, 0.0), sm.top
    if spec.close_cells > 0:
        st = np.ones((spec.close_cells + 1, spec.close_cells + 1), dtype=bool)
        tall = ndimage.binary_closing(tall, structure=st) | source_tall
        # A filled gap takes the height and colour of the nearest real block.
        _, (iz, ix) = ndimage.distance_transform_edt(~source_tall, return_indices=True)
        src_h, src_top = src_h[iz, ix], sm.top[iz, ix]
    if spec.open_cells > 1:
        st = np.ones((spec.open_cells, spec.open_cells), dtype=bool)
        tall = ndimage.binary_opening(tall, structure=st)
    h = np.where(tall, src_h, 0.0)
    band = np.where(tall, np.floor(h / spec.height_step).astype(np.int64) + 1, 0)
    # Regions: 4-connected cells sharing a top colour and a height band.
    key = np.where(tall, band * 256 + np.maximum(src_top, 0), 0)
    region = np.zeros(key.shape, dtype=np.int32)
    next_label = 0
    for value in np.unique(key[key > 0]).tolist():
        lab, count = ndimage.label(key == value)
        region[lab > 0] = lab[lab > 0] + next_label
        next_label += count
    min_cells = spec.min_area / (CELL * CELL)
    # Merge slivers into the neighbour they share the longest border with, or
    # into the street when they stand alone. Smallest first, a few rounds.
    for _ in range(8):
        sizes = np.bincount(region.ravel(), minlength=next_label + 1)
        small = np.nonzero((sizes > 0) & (sizes < min_cells))[0]
        small = small[small > 0]
        if len(small) == 0:
            break
        target = _merge_targets(region, next_label)
        is_small = np.zeros(next_label + 1, dtype=bool)
        is_small[small] = True
        remap = np.arange(next_label + 1)
        for lab in small[np.argsort(sizes[small], kind="stable")].tolist():
            t = int(target[lab])
            if t > 0 and not is_small[t]:
                remap[lab] = t
            elif t <= 0:
                remap[lab] = 0
        # Follow chains so a label never maps onto one that moved too.
        for _ in range(4):
            remap = remap[remap]
        changed = remap[region] != region
        if not changed.any():
            break
        region = remap[region].astype(np.int32)
    region = _relabel(region)
    count = int(region.max())
    height = np.zeros(region.shape, dtype=np.float32)
    top = np.full(count + 1, EMPTY, dtype=np.int64)
    wall = np.full(count + 1, EMPTY, dtype=np.int64)
    if count:
        # A region stands at the tallest height it covers. Regions are global,
        # so chunks on either side of a border read the same number.
        tallest = ndimage.maximum(h, labels=region, index=np.arange(1, count + 1))
        tallest = np.maximum(np.asarray(tallest, dtype=np.float64), LOW_UNITS + 0.25)
        per_region = np.r_[0.0, tallest].astype(np.float32)
        height = per_region[region]
        top = _mode(src_top, region, count)
        wall_cells = _mode(sm.wall, region, count)
        if sm.palette is not None:
            # The mean wall colour, not the most common one: a glass tower is
            # dark panes in light frames, and the mode painted it solid black.
            idx = np.arange(1, count + 1)
            n_s = np.asarray(ndimage.sum(sm.wall_count, labels=region, index=idx))
            sums = np.stack([np.asarray(ndimage.sum(sm.wall_sum[..., k], labels=region, index=idx))
                             for k in range(3)], axis=1)
            has = n_s > 0
            mean = np.where(has[:, None], sums / np.maximum(n_s, 1)[:, None], 0.0)
            near = ch.nearest_palette(np.rint(mean).astype(np.uint8), sm.palette)
            wall_cells[1:] = np.where(has, near, wall_cells[1:])
        wall = np.where(wall_cells == EMPTY, top, wall_cells)
    return BlockLevel(height, region, top, wall)


def _merge_targets(region: np.ndarray, count: int) -> np.ndarray:
    """For every label, the neighbouring label it shares the most border with
    (0 = the street wins, -1 = no neighbour at all)."""
    pairs = []
    for a, b in ((region[:, :-1], region[:, 1:]), (region[:-1, :], region[1:, :])):
        diff = a != b
        pairs.append(np.stack([a[diff], b[diff]], axis=1))
        pairs.append(np.stack([b[diff], a[diff]], axis=1))
    p = np.concatenate(pairs).astype(np.int64)
    target = np.full(count + 1, -1, dtype=np.int64)
    if len(p) == 0:
        return target
    keys, counts = np.unique(p[:, 0] * (count + 1) + p[:, 1], return_counts=True)
    src, dst = keys // (count + 1), keys % (count + 1)
    # Prefer a real block over the street when the borders are equal.
    order = np.lexsort(((dst <= 0).astype(np.int64), -counts, src))
    src_s, dst_s = src[order], dst[order]
    first = np.r_[True, src_s[1:] != src_s[:-1]]
    target[src_s[first]] = dst_s[first]
    return target


def _relabel(region: np.ndarray) -> np.ndarray:
    labels = np.unique(region)
    labels = labels[labels > 0]
    lut = np.zeros(int(region.max()) + 1, dtype=np.int32)
    lut[labels] = np.arange(1, len(labels) + 1, dtype=np.int32)
    return lut[region]


# ---------------------------------------------------------------------------
# Faces for one chunk
# ---------------------------------------------------------------------------

UP, EAST, WEST, SOUTH, NORTH = range(5)  # +Y, +X, -X, +Z, -Z
DIRECTION_NORMALS = np.array([(0, 1, 0), (1, 0, 0), (-1, 0, 0), (0, 0, 1), (0, 0, -1)], dtype=np.float64)


def _rectangles(mask: np.ndarray):
    """Greedy maximal-ish rectangles over True cells: grow right, then down."""
    mask = mask.copy()
    n_z, n_x = mask.shape
    out = []
    for z in range(n_z):
        x = 0
        while x < n_x:
            if not mask[z, x]:
                x += 1
                continue
            x1 = x
            while x1 + 1 < n_x and mask[z, x1 + 1]:
                x1 += 1
            z1 = z
            while z1 + 1 < n_z and mask[z1 + 1, x:x1 + 1].all():
                z1 += 1
            mask[z:z1 + 1, x:x1 + 1] = False
            out.append((x, z, x1, z1))
            x = x1 + 1
    return out


def chunk_faces(level: BlockLevel, cx: int, cz: int, eye_units: float):
    """Faces for chunk (cx, cz) from ``level``.

    Returns ``(vertices, faces)``: world-space (x, y_above_street, z) floats
    and ``(corners_ccw_outward, palette_index, direction)``. A block top is
    only emitted when it is below ``eye_units`` (a walker at street level never
    sees a roof above their eyes, and it would cost a face per region)."""
    c = CHUNK_CELLS
    z0, x0 = cz * c, cx * c
    n = GRID_CELLS
    # One-cell rim of the neighbouring chunks so walls on the border know what
    # stands next door (the same numbers the neighbour chunk sees).
    zs, ze = max(z0 - 1, 0), min(z0 + c + 1, n)
    xs, xe = max(x0 - 1, 0), min(x0 + c + 1, n)
    hpad = np.zeros((c + 2, c + 2), dtype=np.float32)
    hpad[zs - (z0 - 1):ze - (z0 - 1), xs - (x0 - 1):xe - (x0 - 1)] = level.height[zs:ze, xs:xe]
    h = hpad[1:-1, 1:-1]
    reg = level.region[z0:z0 + c, x0:x0 + c]
    verts: dict = {}
    vlist: list = []
    faces: list = []

    def vid(x_cell: float, y: float, z_cell: float) -> int:
        key = (round(x_cell * 2), round(y * 64), round(z_cell * 2))
        idx = verts.get(key)
        if idx is None:
            idx = len(vlist)
            verts[key] = idx
            vlist.append((ch.ORIGIN_X + x_cell * CELL, y, ch.ORIGIN_Z + z_cell * CELL))
        return idx

    def quad(points, colour, direction):
        corners = [vid(*p) for p in points]
        faces.append((tuple(corners), int(colour), direction))

    for lab in np.unique(reg[reg > 0]).tolist():
        mask = reg == lab
        top_c, wall_c = int(level.top[lab]), int(level.wall[lab])
        rects = _rectangles(mask)
        for (rx0, rz0, rx1, rz1) in rects:
            top_h = float(h[rz0, rx0])
            gx0, gz0 = x0 + rx0, z0 + rz0
            gx1, gz1 = x0 + rx1 + 1, z0 + rz1 + 1
            if top_h <= eye_units:
                quad([(gx0, top_h, gz0), (gx0, top_h, gz1), (gx1, top_h, gz1), (gx1, top_h, gz0)],
                     top_c, UP)
            # Walls: for each side, runs of equal neighbour height below us.
            sides = (
                (WEST, [(rz, rx0 - 1) for rz in range(rz0, rz1 + 1)]),
                (EAST, [(rz, rx1 + 1) for rz in range(rz0, rz1 + 1)]),
                (NORTH, [(rz0 - 1, rx) for rx in range(rx0, rx1 + 1)]),
                (SOUTH, [(rz1 + 1, rx) for rx in range(rx0, rx1 + 1)]),
            )
            for direction, cells in sides:
                runs = []
                for i, (nz_, nx_) in enumerate(cells):
                    below = float(hpad[nz_ + 1, nx_ + 1])
                    if below >= top_h:
                        below = None
                    if runs and runs[-1][2] == below and runs[-1][1] == i - 1:
                        runs[-1][1] = i
                    else:
                        runs.append([i, i, below])
                for (i0, i1, below) in runs:
                    if below is None:
                        continue
                    if direction in (WEST, EAST):
                        x = gx0 if direction == WEST else gx1
                        za, zb = gz0 + i0, gz0 + i1 + 1
                        pts = [(x, below, za), (x, below, zb), (x, top_h, zb), (x, top_h, za)]
                    else:
                        z = gz0 if direction == NORTH else gz1
                        xa, xb = gx0 + i0, gx0 + i1 + 1
                        pts = [(xa, below, z), (xb, below, z), (xb, top_h, z), (xa, top_h, z)]
                    quad(pts, wall_c, direction)
    vertices = np.array(vlist, dtype=np.float64).reshape(-1, 3)
    return vertices, [_orient(vertices, f) for f in faces]


def _orient(vertices: np.ndarray, face):
    """Make a face's corners CCW seen from outside (its direction's normal)."""
    corners, colour, direction = face
    p = vertices[list(corners)]
    normal = np.cross(p[1] - p[0], p[2] - p[0])
    if float(normal @ DIRECTION_NORMALS[direction]) < 0.0:
        corners = tuple(reversed(corners))
    return corners, colour, direction


def chunk_boxes(level: BlockLevel, cx: int, cz: int, ground_y: float, max_boxes: int,
                min_height: float = 1.2):
    """Collision boxes (cx, cy, cz, hx, hy, hz) in world units from the same
    blocks the walker sees, coarsened until they fit ``max_boxes``. A coarser
    cover only grows, so it never lets a walker through a wall."""
    c = CHUNK_CELLS
    h = level.height[cz * c:(cz + 1) * c, cx * c:(cx + 1) * c]
    solid = h > min_height
    if not solid.any():
        return []
    for factor in (1, 2, 4, 8, 16, 32, 64):
        m = c // factor
        occ = solid.reshape(m, factor, m, factor).any(axis=(1, 3))
        hmax = h.reshape(m, factor, m, factor).max(axis=(1, 3))
        rects = _rectangles(occ)
        if len(rects) <= max_boxes:
            out = []
            size = CELL * factor
            for (rx0, rz0, rx1, rz1) in rects:
                top_y = float(hmax[rz0:rz1 + 1, rx0:rx1 + 1].max())
                hx = (rx1 - rx0 + 1) * size / 2.0
                hz = (rz1 - rz0 + 1) * size / 2.0
                wx = ch.ORIGIN_X + (cx * c) * CELL + rx0 * size + hx
                wz = ch.ORIGIN_Z + (cz * c) * CELL + rz0 * size + hz
                out.append((wx, ground_y + top_y / 2.0, wz, hx, top_y / 2.0, hz))
            return out
    raise ValueError(f"collision cover needs more than {max_boxes} boxes")
