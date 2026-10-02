"""Entity region partition for saturn/entity_stream2.h: points in, a CSR index out.

Descriptors are written region by region in row-major region order, keeping the author's order
inside a region (the order the runtime activates them in). The index is what
sat_entity_index2_validate accepts.
"""

from __future__ import annotations

from dataclasses import dataclass

from .errors import Stage2dError


@dataclass
class Desc:
    x: int  # pixels from the origin
    y: int
    kind: int
    data: int


@dataclass
class EntityIndex:
    descs: list[Desc]
    region_start: list[int]
    origin_x: int
    origin_y: int
    region_cols: int
    region_rows: int
    region_shift: int


def _u16(value, what: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or not 0 <= value <= 0xFFFF:
        raise Stage2dError(f"{what}: {value!r} is not 0..65535")
    return value


def build_index(items, region_shift: int, origin=None, size=None) -> EntityIndex:
    """`items`: dicts with x, y (world pixels, integers), optional kind and data. `origin` (world px
    of the grid's top-left corner; default: the items' minimum, rounded down to a region edge) and
    `size` (cols, rows of regions; default: just enough) let a stage pin the grid to its map."""
    if not isinstance(region_shift, int) or not 3 <= region_shift <= 15:
        raise Stage2dError(f"region_shift {region_shift!r} is outside 3..15")
    if len(items) > 0xFFFF:
        raise Stage2dError(f"{len(items)} entities; one index holds at most 65535 (split the stage)")
    edge = 1 << region_shift
    points = []
    for i, item in enumerate(items):
        for key in ("x", "y"):
            if isinstance(item.get(key), bool) or not isinstance(item.get(key), int):
                raise Stage2dError(f"entity {i}: {key} must be an integer pixel position")
        points.append((item["x"], item["y"], _u16(item.get("kind", 0), f"entity {i} kind"),
                       _u16(item.get("data", 0), f"entity {i} data")))
    if origin is None:
        ox = (min((p[0] for p in points), default=0) // edge) * edge
        oy = (min((p[1] for p in points), default=0) // edge) * edge
    else:
        ox, oy = origin
    if size is None:
        cols = max(((max((p[0] for p in points), default=ox) - ox) >> region_shift) + 1, 1)
        rows = max(((max((p[1] for p in points), default=oy) - oy) >> region_shift) + 1, 1)
    else:
        cols, rows = size
    if not (1 <= cols <= 0xFFFF and 1 <= rows <= 0xFFFF):
        raise Stage2dError(f"region grid {cols} x {rows} does not fit 16 bits")
    if cols * rows > 0xFFFF:
        raise Stage2dError(f"{cols * rows} regions; the region table is 16-bit indexed, use a larger region_shift")
    if not (-(1 << 31) <= ox < (1 << 31) and -(1 << 31) <= oy < (1 << 31)):
        raise Stage2dError("origin does not fit 32 bits")

    keyed = []
    for i, (x, y, kind, data) in enumerate(points):
        dx, dy = x - ox, y - oy
        if not (0 <= dx < cols << region_shift and 0 <= dy < rows << region_shift):
            raise Stage2dError(f"entity {i} at ({x}, {y}) lies outside the region grid")
        if dx > 0xFFFF or dy > 0xFFFF:
            raise Stage2dError(f"entity {i}: offset ({dx}, {dy}) from the origin exceeds 65535 px")
        region = (dy >> region_shift) * cols + (dx >> region_shift)
        keyed.append((region, i, Desc(dx, dy, kind, data)))
    keyed.sort(key=lambda k: (k[0], k[1]))  # region-major, stable inside a region

    start = [0] * (cols * rows + 1)
    for region, _, _ in keyed:
        start[region + 1] += 1
    for i in range(1, len(start)):
        start[i] += start[i - 1]
    return EntityIndex([k[2] for k in keyed], start, ox, oy, cols, rows, region_shift)


def validate_index(ix: EntityIndex) -> None:
    """The checks of sat_entity_index2_validate."""
    regions = ix.region_cols * ix.region_rows
    if ix.region_cols == 0 or ix.region_rows == 0 or not 3 <= ix.region_shift <= 15:
        raise Stage2dError("entity index: bad region grid")
    if len(ix.region_start) != regions + 1 or ix.region_start[0] != 0 or ix.region_start[-1] != len(ix.descs):
        raise Stage2dError("entity index: region table shape")
    for r in range(regions):
        first, last = ix.region_start[r], ix.region_start[r + 1]
        if last < first:
            raise Stage2dError("entity index: region table is not monotonic")
        col, row = r % ix.region_cols, r // ix.region_cols
        for d in ix.descs[first:last]:
            if d.x >> ix.region_shift != col or d.y >> ix.region_shift != row:
                raise Stage2dError(f"entity index: descriptor ({d.x}, {d.y}) is listed in region {r}")


def index_bytes(desc_count: int, region_count: int) -> int:
    """sat_entity_index2_bytes."""
    return desc_count * 8 + (region_count + 1) * 2
