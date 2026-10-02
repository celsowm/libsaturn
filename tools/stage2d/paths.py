"""Arc-length tables for the Bezier paths of saturn/path2.h.

The runtime can build a table itself from a handful of chords per interval; an offline tool can
afford a much finer one. The table has `entries` evenly spaced curve parameters, entry 0 is 0 and
the last entry is the curve's length, all in 16.16 pixels.
"""

from __future__ import annotations

import math

from .errors import Stage2dError

TABLE_MIN = 2
TABLE_MAX = 1025


def _point(points, t: float):
    u = 1.0 - t
    if len(points) == 3:
        w = (u * u, 2 * u * t, t * t)
    else:
        w = (u * u * u, 3 * u * u * t, 3 * u * t * t, t * t * t)
    return (sum(wi * p[0] for wi, p in zip(w, points)), sum(wi * p[1] for wi, p in zip(w, points)))


def bezier_table(points, entries: int, chords_per_interval: int = 64) -> list[int]:
    """`points`: three (quadratic) or four (cubic) control points in pixels. Returns the 16.16
    cumulative arc length at each of `entries` evenly spaced parameters."""
    if len(points) not in (3, 4):
        raise Stage2dError("a Bezier path has 3 (quadratic) or 4 (cubic) control points")
    if not TABLE_MIN <= entries <= TABLE_MAX:
        raise Stage2dError(f"table entries {entries} is outside {TABLE_MIN}..{TABLE_MAX}")
    for p in points:
        if len(p) != 2 or any(abs(v) >= 32768 for v in p):
            raise Stage2dError("control points must be [x, y] within +-32767 px")
    total = 0.0
    table = [0]
    prev = _point(points, 0.0)
    steps = (entries - 1) * chords_per_interval
    for i in range(1, steps + 1):
        at = _point(points, i / steps)
        total += math.hypot(at[0] - prev[0], at[1] - prev[1])
        prev = at
        if i % chords_per_interval == 0:
            table.append(round(total * 65536))
    if table[-1] <= 0:
        raise Stage2dError("a Bezier path must have a non-zero length")
    if table[-1] > 0x7FFFFFFF:
        raise Stage2dError("a Bezier path longer than 32767 px does not fit 16.16")
    return table
