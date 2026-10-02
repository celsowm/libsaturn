# Stage2D offline tools

`tools/stage2d_tool.py` compiles one JSON spec into a C header and source with the read-only data of
the 2D runtime modules: terrain profiles and maps (`saturn/terrain2.h`), VDP2 metatile maps
(`saturn/stage_map2.h`), entity region indexes (`saturn/entity_stream2.h`), animation clips
(`saturn/sprite_clip.h`) and Bezier arc-length tables (`saturn/path2.h`). Nothing in it knows any
particular game. Importers for a specific game belong outside this package (the plan's tools/import
territory, none exists yet) and emit this spec; no data derived from a reference game is committed here (see the provenance rule in
`docs/SONIC_CLASS_2D_RUNTIME_REFACTOR_PLAN.md`, section 20).

An installed LibSaturn package ships the tool as `<prefix>/share/libsaturn/tools/stage2d_tool.py`
(CMake: `${LIBSATURN_STAGE2D_TOOL}` after `find_package(LibSaturn)`), so a game that lives outside
this repository runs it from there; the `stage2d/` package it imports sits next to it.

```
python tools/stage2d_tool.py build SPEC.json --out-dir DIR [--max-bytes N]
python tools/stage2d_tool.py check SPEC.json [--max-bytes N]
```

`build` writes `<name>.h` and `<name>.c` (deterministic, LF endings, positional initialisers of the
runtime structs, valid as C and C++) and prints the bytes of read-only data per section. `check` runs
every validation without writing. `--max-bytes` fails the run when the total is over a budget. Errors
are one line naming the offending item and exit with status 1.

The package lives in `tools/stage2d/`: `terrain.py`, `metatiles.py`, `entities.py`, `clips.py`,
`paths.py`, `grid.py`, `build.py` (spec to data) and `emit_c.py` (data to C, plus `size_report`).

## Spec

Every section is optional. `name` (lowercase letters, digits, underscores) prefixes every symbol.

### `profiles`
A list of terrain profiles. Profile 0 is always the built-in `empty` one. Each entry has a `name` and
one of:

- `columns`: eight extents in -8..8 (positive anchors the solid to the bottom, negative to the top,
  +-8 is full). The row table is derived exactly as `sat_terrain_profile2_from_columns` does; a shape
  whose pixel row has a hole is refused (split it across tiles).
- `mask`: eight strings of `#` and `.` (columns must be one run touching the top or bottom).
- `from` plus `transform` (a name or a list): a variant of an earlier profile.

Optional: `angle` (0..255, or `"auto"` to derive it from a floor or ceiling ramp: flat floor 0, a
ramp rising to the right 224, flat ceiling 128), `flags` (a number or a list of `"one_way"` and
numbers), `category` (default 1), `material`, and `variants`, a list of transforms (`flip_x`,
`flip_y`, `rot90`, `rot180`, `rot270`; rotations are clockwise on screen) that add `name.<op>`
profiles with the angle carried along. At most 1024 profiles.

### `terrain`
`metatile_shift` (0..5, default 2), `outside` (`empty`, `solid`, `clamp`), `legend` (character to
cell), and 1 to 4 `layers`, each `{"rows": [...]}` of strings (one legend character per tile) or lists.
A cell is a profile name with optional `|fx`, `|fy`, `|u0`..`|u15` (flips and the four game bits), a
raw 16-bit number, or `"."`/`""` for the empty profile. Layers are cut into metatiles sharing one
table (metatile 0 is the all-empty one) and must come out the same size in metatiles.

Output: `<name>_profiles`, `<name>_terrain_metatiles`, `<name>_terrain_layer_N` and `*_COUNT`,
`*_SHIFT`, `*_COLS`, `*_ROWS`, `*_OUTSIDE` macros, bound with `sat_terrain_map2_init`.

### `stage_map`
`shift` (0..4), `fill` (the cell word used for padding and for metatile 0), optional `legend`, and
1 to 4 `layers` of 16-bit VDP2 cell words. At most 16384 metatiles.

Output: `<name>_map_tileset`, `<name>_map_layer_N` with `*_W` / `*_H` macros.

### `entities`
`region_shift` (3..15), `items` (`x`, `y` integers in world pixels, `kind`, `data` as 16-bit values),
optional `origin` and `size` (regions) to pin the grid to a map. Descriptors are written region by
region in row-major order, keeping the author's order inside a region, which is the order the runtime
activates them in. One index holds at most 65535 entities, 65535 regions and 65535 px between its
origin and any entity.

Output: `<name>_entity_descs`, `<name>_entity_region_start` and a ready `<name>_entity_index`.

### `clips`
`sheet` (`cell_w`, `cell_h`, `columns`, optional `origin_x` / `origin_y`) and `clips`. A clip has a
`name`, `mode` (`once`, `loop`, `ping_pong`), `loop_start` and `frames`. A frame has `source`
`[x, y, w, h]` or `cell` (with an optional `span`), `pivot` (`[x, y]` or `top_left`, `top_center`,
`center`, `bottom_left`, `bottom_center`), `duration` in ticks, `event`, `flags` and `shapes`
(`x`, `y`, `w`, `h`, `kind`, `index`, `flags`; a shape without `w` / `h` is a point). Identical shape
runs are stored once, also as part of a longer run.

Output: a ready `<name>_clip_set`, `<NAME>_CLIP_<CLIP>` indexes and `<NAME>_CLIP_REGION_COUNT` for the
texture region budget.

### `paths`
`name`, `kind` (`quadratic`, `cubic`), `points` (pixels, floats allowed) and `table_entries` (2..1025,
default 129). The tool integrates the curve with 64 chords per table interval, finer than the runtime
builds for itself; attach the table with `sat_path2_attach_table`. Lines, polylines, arcs and circles
are exact without one.

Output: `<name>_path_<n>_points` (16.16) and `<name>_path_<n>_table`.

## Verification

- `tests/tools/test_stage2d.py` covers every module, the error messages, determinism, the CLI and the
  provenance boundary, and compares a fresh build of the synthetic spec
  (`tests/fixtures/stage2d/synthetic_stage.json`) against the committed output in
  `tests/host/fixtures/`.
- `tests/host/test_stage2d_generated.cpp` compiles that committed output and runs it through the real
  runtime: every profile through `sat_terrain_profile2_validate` and `_from_columns`, the map through
  `sat_terrain_map2_validate` and probes, the entity index through `sat_entity_index2_validate` and a
  stream, the clips through `sat_clip_set_validate` and a player, the path tables through
  `sat_path2_attach_table` and `sat_path2_sample`, and the byte accounting against the runtime
  `*_requirements` / `*_bytes` helpers.

To regenerate the fixture after a deliberate change:
`python tools/stage2d_tool.py build tests/fixtures/stage2d/synthetic_stage.json --out-dir tests/host/fixtures`.
