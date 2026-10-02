"""Terrain profile compilation for saturn/terrain2.h.

A profile is an 8 x 8 solid mask stored as eight column extents and eight row extents (see the
header). This module builds profiles from column heights or an ASCII mask, derives the row table
exactly as sat_terrain_profile2_from_columns does, validates both tables the way
sat_terrain_profile2_validate does, prepares flipped / rotated variants, and derives a surface
angle from a ramp's shape.
"""

from __future__ import annotations

import math
from dataclasses import dataclass

from .errors import Stage2dError

TILE = 8
ONE_WAY = 0x01
FLAG_NAMES = {"one_way": ONE_WAY}
OPS = ("flip_x", "flip_y", "rot90", "rot180", "rot270")


@dataclass(frozen=True)
class Profile:
    name: str
    column: tuple[int, ...]
    row: tuple[int, ...]
    material: int = 0
    angle: int = 0
    flags: int = 0
    category: int = 0


def column_solid(h: int, y: int) -> bool:
    return y >= TILE - h if h > 0 else (h < 0 and y < -h)


def row_solid(w: int, x: int) -> bool:
    return x >= TILE - w if w > 0 else (w < 0 and x < -w)


def mask_of(columns) -> list[list[bool]]:
    """mask[y][x] from the column table."""
    return [[column_solid(columns[x], y) for x in range(TILE)] for y in range(TILE)]


def rows_from_columns(columns, name: str = "profile") -> tuple[int, ...]:
    """The row table, with the same rules as the runtime: a row is one run anchored to a side."""
    rows = []
    for y in range(TILE):
        solid = [column_solid(columns[x], y) for x in range(TILE)]
        count = sum(solid)
        left = 0
        while left < TILE and solid[left]:
            left += 1
        right = 0
        while right < TILE - left and solid[TILE - 1 - right]:
            right += 1
        if count == 0:
            rows.append(0)
        elif right == count:
            rows.append(count)
        elif left == count:
            rows.append(-count)
        else:
            raise Stage2dError(
                f"{name}: pixel row {y} has a hole or a floating run; split the shape across tiles")
    return tuple(rows)


def columns_from_mask(mask, name: str = "profile") -> tuple[int, ...]:
    """Column extents from mask[y][x]: each column must be one run touching the top or bottom."""
    columns = []
    for x in range(TILE):
        ys = [y for y in range(TILE) if mask[y][x]]
        if not ys:
            columns.append(0)
            continue
        lo, hi = ys[0], ys[-1]
        if hi - lo + 1 != len(ys):
            raise Stage2dError(f"{name}: column {x} has a gap")
        if lo == 0 and hi == TILE - 1:
            columns.append(TILE)
        elif hi == TILE - 1:
            columns.append(len(ys))
        elif lo == 0:
            columns.append(-len(ys))
        else:
            raise Stage2dError(f"{name}: column {x} floats; a column must touch the top or bottom edge")
    return tuple(columns)


def mask_from_text(lines, name: str = "profile") -> list[list[bool]]:
    if not isinstance(lines, list) or len(lines) != TILE:
        raise Stage2dError(f"{name}: a mask needs {TILE} rows")
    mask = []
    for y, line in enumerate(lines):
        if not isinstance(line, str) or len(line) != TILE or any(c not in "#.X " for c in line):
            raise Stage2dError(f"{name}: mask row {y} must be {TILE} characters of '#' and '.'")
        mask.append([c in "#X" for c in line])
    return mask


def validate_profile(p: Profile) -> None:
    """Range and consistency, the same checks as sat_terrain_profile2_validate."""
    if len(p.column) != TILE or len(p.row) != TILE:
        raise Stage2dError(f"{p.name}: profiles have {TILE} columns and {TILE} rows")
    for v in p.column + p.row:
        if not -TILE <= v <= TILE:
            raise Stage2dError(f"{p.name}: extent {v} is outside -8..8")
    for y in range(TILE):
        for x in range(TILE):
            if column_solid(p.column[x], y) != row_solid(p.row[y], x):
                raise Stage2dError(f"{p.name}: column and row tables disagree at pixel ({x}, {y})")
    for label, value, limit in (("angle", p.angle, 255), ("flags", p.flags, 255),
                                ("category", p.category, 255), ("material", p.material, 65535)):
        if isinstance(value, bool) or not isinstance(value, int) or not 0 <= value <= limit:
            raise Stage2dError(f"{p.name}: {label} {value!r} is outside 0..{limit}")


def make_profile(name, columns, angle=0, flags=0, category=0, material=0) -> Profile:
    columns = tuple(columns)
    if len(columns) != TILE or any(isinstance(v, bool) or not isinstance(v, int) for v in columns):
        raise Stage2dError(f"{name}: columns must be {TILE} integers")
    for v in columns:
        if not -TILE <= v <= TILE:
            raise Stage2dError(f"{name}: column extent {v} is outside -8..8")
    p = Profile(name, columns, rows_from_columns(columns, name), material, angle, flags, category)
    validate_profile(p)
    return p


def derive_angle(columns, name: str = "profile") -> int:
    """The surface angle of a floor or ceiling ramp, from a least-squares line through its column
    heights. A flat floor is 0, a ramp rising to the right is negative (224 for 45 degrees); a
    ceiling mirrors the floor (128 - a). Walls and mixed shapes need an explicit angle."""
    # a full column (+-8) is both a floor and a ceiling column, so it does not decide the side
    if all(h > 0 for h in columns):
        sign = 0
    elif all(h < 0 or h == TILE for h in columns):
        sign = 1
    else:
        raise Stage2dError(f"{name}: angle 'auto' needs a floor or ceiling shape (every column filled from one side)")
    heights = [abs(h) for h in columns]
    n = TILE
    mean_x = (n - 1) / 2
    mean_h = sum(heights) / n
    num = sum((x - mean_x) * (h - mean_h) for x, h in enumerate(heights))
    den = sum((x - mean_x) ** 2 for x in range(n))
    slope = num / den  # height gained per pixel to the right
    floor = round(math.atan2(-slope, 1.0) * 256 / (2 * math.pi)) & 255
    return (128 - floor) & 255 if sign else floor


def _rot90(mask):
    return [[mask[TILE - 1 - x][y] for x in range(TILE)] for y in range(TILE)]


def transform_mask(mask, op: str):
    if op == "flip_x":
        return [row[::-1] for row in mask]
    if op == "flip_y":
        return mask[::-1]
    turns = {"rot90": 1, "rot180": 2, "rot270": 3}.get(op)
    if turns is None:
        raise Stage2dError(f"unknown transform '{op}' (use one of {', '.join(OPS)})")
    for _ in range(turns):
        mask = _rot90(mask)
    return mask


def transform_angle(angle: int, op: str) -> int:
    """Where the surface angle goes when the tile is flipped or rotated clockwise on screen."""
    if op == "flip_x":
        return (-angle) & 255
    if op == "flip_y":
        return (128 - angle) & 255
    return (angle + {"rot90": 64, "rot180": 128, "rot270": 192}[op]) & 255


def transform_profile(p: Profile, op: str, name: str | None = None) -> Profile:
    mask = transform_mask(mask_of(p.column), op)
    name = name or f"{p.name}.{op}"
    columns = columns_from_mask(mask, name)
    q = Profile(name, columns, rows_from_columns(columns, name), p.material,
                transform_angle(p.angle, op), p.flags, p.category)
    validate_profile(q)
    return q


def _flags(value, name: str) -> int:
    if isinstance(value, int) and not isinstance(value, bool):
        return value
    if isinstance(value, list):
        total = 0
        for item in value:
            if isinstance(item, int) and not isinstance(item, bool):
                total |= item
            elif item in FLAG_NAMES:
                total |= FLAG_NAMES[item]
            else:
                raise Stage2dError(f"{name}: unknown flag {item!r} (known: {', '.join(FLAG_NAMES)}; or use numbers 16..128 for game bits)")
        return total
    raise Stage2dError(f"{name}: flags must be a number or a list")


def compile_profiles(specs) -> list[Profile]:
    """Profile 0 is always the empty profile. Each spec has a `name` and one of `columns` / `mask`
    / `from` (+ `transform`), plus optional angle (number or "auto"), flags, category, material and
    `variants` (a list of transforms that add `name.<op>` profiles)."""
    profiles = [Profile("empty", (0,) * TILE, (0,) * TILE)]
    seen = {"empty"}

    def add(p: Profile):
        if p.name in seen:
            raise Stage2dError(f"profile name '{p.name}' is used twice")
        seen.add(p.name)
        profiles.append(p)

    by_name = {"empty": profiles[0]}
    for spec in specs or []:
        name = spec.get("name")
        if not isinstance(name, str) or not name:
            raise Stage2dError("every profile needs a name")
        if "from" in spec:
            base = by_name.get(spec["from"])
            if base is None:
                raise Stage2dError(f"{name}: unknown base profile '{spec['from']}'")
            ops = spec.get("transform", [])
            ops = [ops] if isinstance(ops, str) else ops
            p = base
            for op in ops:
                p = transform_profile(p, op, name)
            p = Profile(name, p.column, p.row, spec.get("material", p.material), p.angle,
                        _flags(spec["flags"], name) if "flags" in spec else p.flags,
                        spec.get("category", p.category))
            if "angle" in spec and spec["angle"] != "auto":
                p = Profile(p.name, p.column, p.row, p.material, spec["angle"], p.flags, p.category)
            validate_profile(p)
        else:
            if "columns" in spec:
                columns = tuple(spec["columns"])
            elif "mask" in spec:
                columns = columns_from_mask(mask_from_text(spec["mask"], name), name)
            else:
                raise Stage2dError(f"{name}: needs columns, mask or from")
            angle = spec.get("angle", 0)
            if angle == "auto":
                angle = derive_angle(columns, name)
            p = make_profile(name, columns, angle, _flags(spec.get("flags", 0), name),
                             spec.get("category", 1), spec.get("material", 0))
        add(p)
        by_name[name] = p
        for op in spec.get("variants", []):
            v = transform_profile(p, op)
            add(v)
            by_name[v.name] = v
    if len(profiles) > 1024:
        raise Stage2dError(f"{len(profiles)} profiles; a tile word indexes at most 1024")
    return profiles
