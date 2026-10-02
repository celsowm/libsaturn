"""Rectangular grids of cell words, written either as lists of lists or as rows of characters."""

from __future__ import annotations

from typing import Callable

from .errors import Stage2dError


def parse_grid(rows, legend, cell_fn: Callable[[object], int], what: str) -> list[list[int]]:
    """`rows` is a list of lists (cells) or a list of strings (one legend character per cell).
    `cell_fn` turns a cell into a 16-bit word. Every row must have the same length."""
    if not isinstance(rows, list) or not rows:
        raise Stage2dError(f"{what}: rows must be a non-empty list")
    legend = legend or {}
    grid: list[list[int]] = []
    for y, row in enumerate(rows):
        if isinstance(row, str):
            cells = []
            for ch in row:
                if ch not in legend:
                    raise Stage2dError(f"{what}: row {y} uses '{ch}', which is not in the legend")
                cells.append(legend[ch])
        elif isinstance(row, list):
            cells = row
        else:
            raise Stage2dError(f"{what}: row {y} must be a list or a string")
        grid.append([cell_fn(c) for c in cells])
    width = len(grid[0])
    if width == 0:
        raise Stage2dError(f"{what}: rows are empty")
    for y, row in enumerate(grid):
        if len(row) != width:
            raise Stage2dError(f"{what}: row {y} has {len(row)} cells, row 0 has {width}")
    return grid


def word(value, what: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or not 0 <= value <= 0xFFFF:
        raise Stage2dError(f"{what}: {value!r} is not a 16-bit word")
    return value
