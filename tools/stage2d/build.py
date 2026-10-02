"""Turns a stage2d spec (JSON) into validated data, ready for emit_c."""

from __future__ import annotations

import json
import re
from dataclasses import dataclass, field
from pathlib import Path

from . import clips as clips_mod
from . import entities as entities_mod
from . import metatiles as metatiles_mod
from . import paths as paths_mod
from . import terrain as terrain_mod
from .errors import Stage2dError
from .grid import parse_grid, word

OUTSIDE = {"empty": 0, "solid": 1, "clamp": 2}
TILE_FLIP_X, TILE_FLIP_Y = 0x0400, 0x0800
NAME_RE = re.compile(r"^[a-z][a-z0-9_]*$")


@dataclass
class TerrainData:
    shift: int
    outside: int
    tiles: metatiles_mod.MetatileSet
    cols: int
    rows: int


@dataclass
class StageMapData:
    shift: int
    tiles: metatiles_mod.MetatileSet


@dataclass
class PathData:
    name: str
    points: list
    table: list


@dataclass
class StageData:
    name: str
    profiles: list = field(default_factory=list)
    terrain: TerrainData | None = None
    stage_map: StageMapData | None = None
    entities: entities_mod.EntityIndex | None = None
    clips: clips_mod.ClipSet | None = None
    paths: list = field(default_factory=list)


def load_spec(path) -> dict:
    try:
        return json.loads(Path(path).read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise Stage2dError(f"{path}: not valid JSON ({exc})") from exc


def _terrain_cell_fn(profile_index: dict[str, int]):
    def cell(c) -> int:
        if c is None or c in ("", "."):
            return 0
        if isinstance(c, int) and not isinstance(c, bool):
            return word(c, "terrain tile word")
        if not isinstance(c, str):
            raise Stage2dError(f"terrain cell {c!r} must be a profile name or a number")
        parts = c.split("|")
        if parts[0] not in profile_index:
            raise Stage2dError(f"terrain cell '{c}': unknown profile '{parts[0]}'")
        value = profile_index[parts[0]]
        for mod in parts[1:]:
            if mod == "fx":
                value |= TILE_FLIP_X
            elif mod == "fy":
                value |= TILE_FLIP_Y
            elif re.fullmatch(r"u([0-9]|1[0-5])", mod):
                value |= int(mod[1:]) << 12
            else:
                raise Stage2dError(f"terrain cell '{c}': unknown modifier '{mod}' (use fx, fy, u0..u15)")
        return value
    return cell


def _build_terrain(spec: dict, profiles) -> TerrainData:
    if len(profiles) < 2:
        raise Stage2dError("terrain needs profiles")
    index = {p.name: i for i, p in enumerate(profiles)}
    layers = spec.get("layers")
    if not isinstance(layers, list) or not 1 <= len(layers) <= 4:
        raise Stage2dError("terrain: 1 to 4 layers are required")
    legend = spec.get("legend", {})
    grids = [parse_grid(layer.get("rows"), legend, _terrain_cell_fn(index), f"terrain layer {i}")
             for i, layer in enumerate(layers)]
    outside = OUTSIDE.get(spec.get("outside", "empty"))
    if outside is None:
        raise Stage2dError(f"terrain: outside must be one of {', '.join(OUTSIDE)}")
    shift = spec.get("metatile_shift", 2)
    tiles = metatiles_mod.compile_metatiles(grids, shift, fill=0, max_metatiles=0xFFFF, max_shift=5)
    sizes = {(m.cols, m.rows) for m in tiles.layers}
    if len(sizes) != 1:
        raise Stage2dError(f"terrain: layers must be the same size in metatiles, got {sorted(sizes)}")
    for w in tiles.cells:
        if (w & 0x3FF) >= len(profiles):
            raise Stage2dError(f"terrain: tile word {w:#06x} indexes profile {w & 0x3FF} of {len(profiles)}")
    cols, rows = sizes.pop()
    return TerrainData(shift, outside, tiles, cols, rows)


def _build_stage_map(spec: dict) -> StageMapData:
    layers = spec.get("layers")
    if not isinstance(layers, list) or not 1 <= len(layers) <= 4:
        raise Stage2dError("stage_map: 1 to 4 layers are required")
    legend = spec.get("legend", {})
    grids = [parse_grid(layer.get("rows"), legend, lambda c: word(c, "stage_map cell"), f"stage_map layer {i}")
             for i, layer in enumerate(layers)]
    shift = spec.get("shift", 2)
    tiles = metatiles_mod.compile_metatiles(grids, shift, fill=word(spec.get("fill", 0), "stage_map fill"),
                                            max_metatiles=16384, max_shift=4)
    return StageMapData(shift, tiles)


def _build_entities(spec: dict) -> entities_mod.EntityIndex:
    origin = spec.get("origin")
    size = spec.get("size")
    index = entities_mod.build_index(spec.get("items", []), spec.get("region_shift", 6),
                                     tuple(origin) if origin else None, tuple(size) if size else None)
    entities_mod.validate_index(index)
    return index


def _build_paths(specs) -> list[PathData]:
    out, names = [], set()
    for i, spec in enumerate(specs):
        name = spec.get("name")
        if not isinstance(name, str) or not NAME_RE.match(name) or name in names:
            raise Stage2dError(f"path {i}: needs a unique name of lowercase letters, digits and underscores")
        names.add(name)
        kind = spec.get("kind")
        count = {"quadratic": 3, "cubic": 4}.get(kind)
        if count is None or len(spec.get("points", [])) != count:
            raise Stage2dError(f"{name}: kind is quadratic (3 points) or cubic (4 points)")
        table = paths_mod.bezier_table(spec["points"], spec.get("table_entries", 129))
        out.append(PathData(name, spec["points"], table))
    return out


def build_stage(spec: dict) -> StageData:
    name = spec.get("name")
    if not isinstance(name, str) or not NAME_RE.match(name):
        raise Stage2dError("spec: 'name' must be lowercase letters, digits and underscores (it prefixes every symbol)")
    stage = StageData(name)
    if "profiles" in spec or "terrain" in spec:
        stage.profiles = terrain_mod.compile_profiles(spec.get("profiles", []))
    if "terrain" in spec:
        stage.terrain = _build_terrain(spec["terrain"], stage.profiles)
    if "stage_map" in spec:
        stage.stage_map = _build_stage_map(spec["stage_map"])
    if "entities" in spec:
        stage.entities = _build_entities(spec["entities"])
    if "clips" in spec:
        stage.clips = clips_mod.pack_clips(spec["clips"])
    if "paths" in spec:
        stage.paths = _build_paths(spec["paths"])
    return stage
