# Two-cube renderer pipeline benchmark

`examples/cube_pipeline_bench` reduces renderer performance investigations to
two procedural cubes. No imported models, game logic, expansion RAM, or retail
disc assets are used. A cube starts with eight shared vertices and six quads.
Subdivisions increase geometry without increasing the cubes' screen footprint.

## Build and run

```sh
make all EXAMPLE=cube_pipeline_bench CUBE_BENCH_STAGE=1 CUBE_BENCH_DIV=1
```

The build produces the usual native CD files under `build/`. The HUD displays
the stage, subdivision count and `FRAME NNNN` (the submission serial).
`CUBE_BENCH_HUD=0` removes the HUD; keep this setting consistent across comparisons.

| Stage | Work added each frame |
| --- | --- |
| 0 | Empty frame and HUD |
| 1 | Direct VDP1 command construction, CPU VRAM upload, PTMR trigger and EDSR completion wait |
| 2 | SDK polygon emission, cached projected coordinates and painter order |
| 3 | World translation and SDK projection, then the same cached native polygon order |
| 4 | Full scene submission: world transform, projection, visibility, face preparation, sorting and RGB emission |
| 5 | Stage 4 with an 8×8 LUT4 checker texture |
| 6 | Stage 4 plus rigid skinning |
| 7 | Stage 5 plus rigid skinning |
| 8 | Stage 7 with the second actor's scene preparation dispatched to the Slave SH-2 |
| 9 | Stage 8 with a rotating joint on each actor |

Stages 1–3 cache visibility and painter order during initialization. They are
deliberate lower bounds for this fixed scene, not substitutes for a dynamic
scene renderer. Stage 3 recomputes projection but keeps that fixed order.
Stages 4–9 recompute scene preparation. The skin has one joint; it isolates
vertex skinning and does not claim to represent a complete character rig.

`CUBE_BENCH_DIV` accepts 1 through 12. Each actor has `6*n*n+2` shared vertices
and `6*n*n` quads. Densities 1, 4, 8 and 12 give 6, 96, 384 and 864 quads per
actor. Tiny projected faces can collapse after integer rounding; the telemetry
records emitted face/command counts as well as source topology.

## Reproducible Ymir ladder

Build the harness as described in [its README](../harness/README.md). Supply
your local BIOS path. The script retains each ELF, map, CD image, exact command,
build/probe log, screenshot, panic dump, telemetry and analysis in a fresh
output directory. Paths below are relative to the LibSaturn root.

```sh
python3 harness/scripts/cube_pipeline.py \
  --bios /path/to/saturn_101a_us.bin \
  --out harness/build/cube-ladder

python3 harness/scripts/cube_pipeline.py \
  --report harness/build/cube-ladder/matrix.json \
  --compare raw native projection scene

python3 harness/scripts/cube_pipeline.py \
  --report harness/build/cube-ladder/matrix.json \
  --compare raw-864 native-864 projection-864 scene-864
```

`--compare` requires Pillow. The runner itself uses the Python standard library.
Use `--python /path/to/python` for Make's asset tools, `--compiler-dir` for the
SH-2 toolchain, and `--probe` for a different harness build. `--cases` selects
a subset. The default matrix includes 21 cases, covering stages 0–5 and 7 at
the smallest density, density sweeps, and paired/animated dense cases.
Stage 6 is available through Make for the RGB skinning control.

The runner locks the shared example build aliases until it has copied all
artifacts. Different retained cases can then execute without reusing an ELF or
disc overwritten by another build. Do not run an unrelated manual Make of this
same example concurrently on a platform without Make's `flock` support.

The harness initializes a real BIOS and injects the application into work RAM;
this mode isolates rendering and **does not prove CD boot**. Use the independent
Mednafen recording below for that.

## CPU profiling and actual CD boot

```sh
python3 harness/scripts/cube_pipeline.py \
  --bios /path/to/saturn_101a_us.bin \
  --out harness/build/cube-profile --cases scene-864 --profile

python3 harness/scripts/analyze_cpu_profile.py \
  --prefix harness/build/cube-profile/scene-864/debug \
  --elf harness/build/cube-profile/scene-864/cube_pipeline_bench.elf \
  --nm /path/to/sh2eb-elf-nm \
  --out harness/build/cube-profile/scene-864/profile-summary.json

python3 harness/scripts/cube_pipeline.py \
  --record-cd harness/build/cube-ladder/raw \
  --bios /path/to/saturn_101a_us.bin \
  --out harness/build/cube-mednafen-raw --seconds 25
```

The recorder requires Mednafen, ffmpeg and ffprobe. It boots the retained CUE,
records a movie, extracts its last second as a screenshot and records hashes.
Inspect the recording and advancing frame counter; a zero process exit code
alone is not visual confirmation. `--mednafen` overrides the executable.

The profiler retains PC cost histograms, frame/interrupt/call events and register
snapshots. Truncated/dropped captures are rejected. The analyzer reconciles the
histograms with each CPU's totals and attributes exclusive interpreter step
costs to matching ELF functions. These percentages are not inclusive call times.
Use its optional `--objdump /path/to/sh2eb-elf-objdump` to disassemble captured
instruction words. Keep the matching ELF with every profile.

## Timing and ownership

FRT telemetry stores bounded, per-submission timings for frame begin, animation,
skin, geometry, flush, HUD and present. `work` includes the instrumentation;
`wait` is the preceding VBlank wait and is excluded from `work`. Even empty
stages have timer overhead. The decoder checks topology, serial publication,
command capacity, panics, draw timeouts and minimum measurement duration.

The raw stage owns the VDP1 command table at `0x25c00000`, writes 32-byte
commands, starts plotting through PTMR at `0x25d00004`, and polls EDSR CEF at
`0x25d00010` until its own END command completes. Every raw plot is joined
before the next upload. Its `present` includes CPU upload and synchronous
plot wait; `plot` isolates the trigger/wait interval. The SDK stage's `present`
can return before VDP1 finishes, so compare complete submission cadence and
reported stage costs, not just those two `present` values.

Register semantics follow the local [VDP1 process flow](sega_saturn_hardware/hard/vdp1/hon/p03_10.md)
and [plot trigger manual](sega_saturn_hardware/hard/vdp1/hon/p04_13.md).
This raw path is a diagnostic control rather than a new production driver.

The reported rate is CPU submissions per **nominal 60 NTSC fields**, measured
from the guest field counter. It does not prove distinct displayed buffers,
host real-time speed, physical Saturn timings, or performance parity with VF2.
Ymir uses enabled SH-2 cache emulation and the normal 1:1 clock factor.

## Recorded result: 2026-10-18

Local Ymir revision `5c571f680e3606de08813af4de43dc00e90cc6e3`, SDK base
`a46598b` plus this example. Each run used 90 BIOS initialization fields and
1,200 application fields, discarded the first 120 fields and required at
least 600 measured fields. HUD was enabled. Times are median instrumented ms.
The artifact directory for this run is `harness/build/cube-ladder-publish/`
(private and ignored by Git); every case records executable and source hashes.

| Case | Quads per actor | Submit/s | Work ms | Geometry ms | Emit ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| Empty | 6 | 60 | 0.722 | 0.005 | 0.005 |
| Raw VDP1 | 6 | 60 | 0.788 | 0.005 | 0.057 |
| SDK native polygons | 6 | 60 | 0.822 | 0.005 | 0.090 |
| Projection + native | 6 | 60 | 0.988 | 0.176 | 0.090 |
| RGB scene | 6 | 60 | 1.909 | 0.518 | 0.584 |
| Textured scene | 6 | 60 | 1.952 | 0.522 | 0.627 |
| Textured skin | 6 | 60 | 2.061 | 0.522 | 0.622 |
| Raw VDP1 | 96 | 60 | 1.833 | 0.005 | 0.836 |
| RGB scene | 96 | 60 | 9.357 | 6.388 | 2.009 |
| Textured scene | 96 | 60 | 9.979 | 6.388 | 2.631 |
| Raw VDP1 | 384 | 60 | 4.886 | 0.005 | 3.305 |
| RGB scene | 384 | 30 | 33.177 | 25.190 | 6.538 |
| Textured scene | 384 | 20 | 35.660 | 25.190 | 9.022 |
| Raw VDP1 | 864 | 60 | 9.480 | 0.005 | 6.764 |
| SDK native polygons | 864 | 60 | 13.100 | 0.005 | 11.029 |
| Projection + native | 864 | 30 | 30.113 | 16.961 | 11.086 |
| RGB scene | 864 | 12 | 69.250 | 54.203 | 12.900 |
| Textured scene | 864 | 12 | 74.314 | 54.203 | 17.968 |
| Textured skin | 864 | 10 | 84.062 | 54.313 | 17.968 |
| Paired textured skin | 864 | 15 | 61.990 | 32.260 | 17.963 |
| Animated paired skin | 864 | 15 | 64.915 | 33.675 | 19.596 |

At densities 1 and 12, the raw/native/projection/RGB-scene screenshots were
pixel-identical in the cube region `(0,48)-(320,192)`: zero differing pixels.
These are same-density comparisons; subdivision changes raster edge rounding.

A separate six-field profile of `scene-864` attributed 21.09% of Master
execution cycles to `sat_scene3d_faces_submit_instance` itself, 19.78% to
`render3d::project_vertices`, 17.89% to `mesh3d::quad_visible`, and 11.86% to
the face `append` routine. These are exclusive percentages of that trace,
including its waits/interrupts in the denominator, not a whole-run average.

The experiment reproduces the expensive path without a character model:
projection, general world transforms and per-face preparation dominate the
dense case. Texture adds about 5 ms; rigid skinning adds about 9.6 ms. Paired
preparation reduces geometry from about 54 to 32 ms but leaves the other costs.
The fixed area and identical RGB pixels make VDP1 fill alone insufficient to
explain the gap. The cached native control establishes available headroom; it
does not show that dynamic scene preparation can simply be omitted for a fight.

The next optimization target is the general geometry path, with this ladder
as the regression control: preserve visibility/rounding and demonstrate reduced
per-frame transform, projection and face-preparation cost before returning to
the character. No production renderer optimization is claimed by this example.

Host checks cover mesh sharing, winding, manifold edges and rejected capacities,
plus telemetry ring coherence, FRT units and failure rejection. CI builds all
ten stages and the densest paired scene under the stock RAM/disc contracts.
