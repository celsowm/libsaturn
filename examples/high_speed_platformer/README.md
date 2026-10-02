# high_speed_platformer

A stage-based 2D platformer built from the generic 2D runtime modules of libsaturn, and nothing
else: no module in the library knows this game. It is the acceptance example of
`docs/SONIC_CLASS_2D_RUNTIME_REFACTOR_PLAN.md` and is written to be moved into its own repository
(see "Boundary" below).

Everything in it is original or synthetic: the stage is placed by code, the art is drawn by code,
and no data comes from any other game.

## Controls

| Button | Action |
| --- | --- |
| D-pad left / right | run |
| A / B / C | jump (hold for a higher jump; also leaves a platform or the rail) |
| D-pad down | roll (needs some speed; keeps the speed on slopes) |

The stage runs from the left wall to the finish line at the right. Falling into a pit respawns at the
last checkpoint flag.

## What it exercises

| Stress item | Where |
| --- | --- |
| high horizontal speed | the dash pads set the ground speed to 14 px/step; a hill run reaches 17 |
| linear and profiled slopes | 45 degree and 22.5 degree ramps made of `columns` profiles |
| floor / wall / ceiling traversal | the loop: an octagon, 16 x 16 tiles |
| moving support | two platforms follow Path2 curves (a Bezier and a circle) |
| one-way support | the planks and the top-solid platform |
| layer switching | three Physics2 sensors flip the hero's terrain layer around the loop |
| generic path | the platforms; the rail is a cubic Bezier |
| rail-like path follower | the hero hangs from the rail and rides the curve |
| generic collectibles | rings, with a sparkle clip on pickup |
| regional entity activation | all rings, pads, springs and flags stream through `entity_stream2` |
| large logical map | 3072 x 384 px, three VDP2 layers kept resident by `stage_map2` |
| camera look-ahead, bounds, shake | `follow_camera2d`: dead zone, speed look-ahead, a locked finish room, shake on hard landings and pads |
| sprite animation | `sprite_clip` clips with pivots, events (footsteps), rates and shapes (ring pickup boxes) |
| multiple VDP2 layers / parallax | NBG0 terrain, NBG1 hills at 1/2 speed, NBG2 clouds at 1/4 |

## Layout

| File | What it is |
| --- | --- |
| `game.c`, `game.h` | the game: acceleration, slopes, jumps, rail, pickups, camera and entity glue. No hardware. |
| `view.c`, `view.h` | VDP2 layers, texture, VDP1 draws and the HUD |
| `art.c`, `art.h` | procedural cells, palettes and the sprite sheet |
| `main.c` | the frame loop (60 Hz fixed steps, up to three catch-up steps per frame) |
| `tools/gen_stage.py` | writes the stage2d spec and the layout (spawn, triggers, platforms) |
| `Makefile.inc`, `stage.mk`, `host_test.mk` | build rules |
| `harness/high_speed_platformer.pad`, `tools/probe_check.py` | the scripted probe run and its check |

`gen_stage.py` and `tools/stage2d_tool.py` produce `stage.h/.c` and `layout.h/.c` into
`build/generated/high_speed_platformer/`; nothing generated is checked in.

## Build and run

```
.\build-example.ps1 -Example high_speed_platformer
.\run-example.ps1  -Example high_speed_platformer
```

The simulation test plays the same `game.c` without a video chip and asserts the stress items:

```
make test        # runs tests/host/test_high_speed_platformer.cpp and ..._boundary.cpp
```

The Ymir probe plays the whole stage with `harness/high_speed_platformer.pad` (run right, one short
jump over the first pit). The game publishes counters in `g_hsp_telemetry`, and
`tools/probe_check.py` reads them out of work RAM and fails unless the stage was cleared without a
death, the loop switched layers twice and the speed passed the dash-pad speed:

```
python examples/high_speed_platformer/tools/probe_check.py --shots build/hsp_shots
```

## Boundary

The example includes public `saturn/*` headers only, never `examples/common`, and defines its own
check macro. `tests/host/test_high_speed_platformer_boundary.cpp` fails the build on a private
include, a reference to another example, or a missing file the extraction needs.
