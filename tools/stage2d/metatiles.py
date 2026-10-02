"""Metatile compilation: cut one or more logical cell grids into (1 << shift)-square metatiles,
share identical ones, and write each grid as a map of metatile indices.

The same compiler serves terrain2 (cells are tile words, shift 0..5, up to four layers sharing the
table) and stage_map2 (cells are VDP2 pattern words, shift 0..4). Output is deterministic: metatiles
are numbered in first-seen order, layer by layer, row-major.
"""

from __future__ import annotations

from dataclasses import dataclass, field

from .errors import Stage2dError


@dataclass
class LayerMap:
    cols: int  # in metatiles
    rows: int
    cells: list[int]  # cols * rows metatile indices, row-major


@dataclass
class MetatileSet:
    shift: int
    cells: list[int] = field(default_factory=list)  # count * (1 << 2 * shift) words
    layers: list[LayerMap] = field(default_factory=list)

    @property
    def count(self) -> int:
        return len(self.cells) >> (2 * self.shift)


def compile_metatiles(grids, shift: int, fill: int = 0, max_metatiles: int = 65535,
                      max_shift: int = 5, empty_first: bool = True) -> MetatileSet:
    """`grids` is a list of rectangular cell grids (list of rows). Grids whose size is not a
    multiple of the metatile edge are padded on the right and bottom with `fill`. With
    `empty_first` metatile 0 is the all-`fill` metatile, so a zeroed map cell means "nothing"."""
    if not 0 <= shift <= max_shift:
        raise Stage2dError(f"metatile shift {shift} is outside 0..{max_shift}")
    if not grids:
        raise Stage2dError("at least one layer is required")
    edge = 1 << shift
    index: dict[tuple, int] = {}
    out = MetatileSet(shift)

    def intern(block: tuple) -> int:
        found = index.get(block)
        if found is not None:
            return found
        number = len(index)
        if number >= max_metatiles:
            raise Stage2dError(f"more than {max_metatiles} distinct metatiles")
        index[block] = number
        out.cells.extend(block)
        return number

    if empty_first:
        intern((fill,) * (edge * edge))

    for g, grid in enumerate(grids):
        height, width = len(grid), len(grid[0])
        cols, rows = -(-width // edge), -(-height // edge)
        if cols > 0xFFFF or rows > 0xFFFF:
            raise Stage2dError(f"layer {g}: {cols} x {rows} metatiles does not fit 16 bits")
        cells = []
        for my in range(rows):
            for mx in range(cols):
                block = []
                for y in range(edge):
                    gy = my * edge + y
                    for x in range(edge):
                        gx = mx * edge + x
                        block.append(grid[gy][gx] if gy < height and gx < width else fill)
                cells.append(intern(tuple(block)))
        out.layers.append(LayerMap(cols, rows, cells))
    return out
