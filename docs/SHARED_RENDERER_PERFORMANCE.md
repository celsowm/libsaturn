# Shared renderer performance

## First shared-kernel refactor

The two-cube ladder and Dino both call the shared geometric visibility kernel
in `src/core/geometry/mesh_logic.hpp`. That kernel now separates ordinary small
faces from the general wide-normal calculation. The fast path proves that all
six edge components are in `[-16384,16383]` in Q16 storage units. Their cross
products then fit signed 32 bits, and the sum of three normal/eye products fits
signed 64 bits. This removes wide cross-product and split-sign arithmetic for
ordinary small faces. Larger faces and collapsed corners use the separate
general kernel, kept out of line so its register/stack requirements do not
burden the small-face path.

The geometric result remains exact: no normal quantization, screen-winding
substitution, changed rounding or per-game visibility policy. Both the mesh
renderer and scene renderer benefit automatically. The refactor requires no
application opt-in and introduces no compatibility adapter.

## Measurements

Local Ymir revision `5c571f680e3606de08813af4de43dc00e90cc6e3`, enabled SH-2
cache emulation, normal 1:1 clock and stock RAM, measured on 2026-10-18.
These are emulator CPU submission rates per nominal 60 NTSC fields, not host
real-time speed, distinct presented buffers or physical Saturn certification.

| Workload | Before | After |
| --- | ---: | ---: |
| RGB cubes, 864 quads per actor, median work | 69.250 ms | 64.644 ms |
| Same, geometry preparation | 54.203 ms | 49.606 ms |
| Same, submissions/s | 12 | 15 |
| Textured cubes, 864 quads per actor, median work | 74.314 ms | 69.720 ms |
| Paired textured skin cubes, median work | 61.990 ms | 59.715 ms |
| Dino animated, submissions/s | 18.4 | 20.0 |
| Dino paused at time zero, light disabled, submissions/s | 19.3 | 20.0 |
| Dino control sequence, submissions/s | 16.4 | 17.4 |

The dense RGB CPU work reduction is about 6.7%; crossing a field boundary
produces the larger 12-to-15 cadence change. The minimal six-quad cube still
runs at 60 submissions/s but pays a small classification overhead: 1.909 ms
before versus 1.933 ms after. The result is not a claim that every face size
or workload gets the same improvement, nor that a complete character reaches
60 Hz.

Same-density RGB cube images have zero differing model-region pixels before
and after the refactor. Dino's paused, unlit model-region images are also
pixel-identical. The animated scenario uses real evolving poses; its images
at the same field can differ as submission cadence changes.

The existing Dino runtime gate passes animated, paused and control runs with
zero panic, overflow, music error or failed parallel job. Pose and geometry
jobs run on the Slave; a stopped snapshot may contain one in-flight job.
Host coverage includes 20,000 randomized exact-sign cases plus targeted
edge-boundary, near-edge-on, warped-quad and collapsed-corner tests against
128-bit dot products.

Private retained evidence is under `harness/build/`: baseline
`cube-ladder-publish`, `dino-pipeline-before-animated`,
`dino-pipeline-baseline-paused-v2`, `dino-pipeline-controls`; after refactor
`cube-split-kernels`, `dino-split-kernels-animated`,
`dino-split-kernels-paused` and `dino-split-kernels-controls`.

## Topology/pose binding refactor

The validated instance binding now describes immutable index/material topology,
not a particular mesh descriptor or vertex-buffer address. It retains topology
array identities and validated vertex/face counts. Submissions may switch pose
buffers, descriptor addresses and capacities; current non-null buffers and
capacities are still checked before projection. Changed topology identities or
counts reject atomically. Indices, face-material maps, material tables and their
referenced descriptors must stay immutable for the binding lifetime. Rebuild
bindings after edits; do not reuse a binding as a trust flag for mutable tables.
Texture residency is still checked before any flush emissions.

This deliberately changes the public binding structure instead of preserving
obsolete identity fields. Code using the initializer continues to initialize
normally; all consumers must rebuild against the new header/library together.
The Dino now creates one binding for each immutable face partition after its
material table is initialized. Both Master and Slave reuse those bindings while
collecting alternating decoded poses. The existing parallel publication path
synchronizes each binding and the current pose buffer before dispatch.

Using the same Ymir configuration and warmed interval as above:

| Dino workload | Split visibility kernel | Topology/pose binding |
| --- | ---: | ---: |
| Animated | 20.0 submissions/s | 20.0 submissions/s |
| Paused, unlit | 20.0 submissions/s | 20.0 submissions/s |
| Control sequence | 17.4 submissions/s | 18.5 submissions/s |

Paused model pixels are identical. All three runtime scenarios pass at both
600 and 1,200 fields. A complete 12-field animated trace gives these exclusive
cycle counts for submission itself plus `valid_material` (not inclusive frame
cost): Master 1,368,043 before versus 1,025,438 after; Slave 958,760 versus
694,851. Projection costs remain essentially unchanged. Flush still validates
materials, explaining the remaining Master `valid_material` work. The default
animated cadence did not cross another field boundary; these trace reductions
must not be advertised as a corresponding frame-rate increase.

Retained evidence: `harness/build/dino-topology-animated`,
`dino-topology-paused`, `dino-topology-controls`, and the independent Mednafen
CD boot in `dino-topology-mednafen`. Host coverage exercises a new pose buffer
with changed coordinates, an independent descriptor, changed capacity, rejected
topology/count changes, missing pose buffers and insufficient capacities.

## Why the library is the target

The [cube ladder](CUBE_PIPELINE_BENCH.md) holds screen area constant and
produces the same pixels across direct and general paths. Its cached direct
VDP1 control takes 9.480 ms at 864 quads per actor; full scene preparation
takes much more CPU work. The control deliberately omits dynamic geometry
work, so it establishes headroom rather than a replacement fighting renderer.

Dino has 1,174 runtime vertices, 1,499 faces and 1,256 texture descriptors.
Its poses are baked/decoded rather than joint-skinned at runtime, and it
already splits face preparation 60/40 between CPUs. A skinning-only change
would not address its common renderer costs.

A non-truncated 12-field animated baseline profile attributed these exclusive
Master execution-cycle percentages to shared routines:

| Routine | Master cycles |
| --- | ---: |
| `render3d::project_vertices` | 13.79% |
| `sat_scene3d_faces_submit_instance` itself | 13.10% |
| `mesh3d::quad_visible` | 11.65% |
| face `append` | 7.61% |
| `valid_material` | 5.86% |
| VDP1 `push_distorted_sprite` | 5.72% |
| cache `invalidate_range` | 5.04% |

Percentages are exclusive, include waits in the denominator and describe that
trace rather than a whole-run average. Slave mailbox polling must be separated
from useful task execution when identifying a frame's critical path.

Two less useful prototypes were removed: adding only a small-normal dot-product
shortcut barely changed the timings; copying the projection matrix onto the
CPU stack slightly worsened the cube measurements and did not improve Dino
cadence. Moving working buffers between WRAM banks previously saved only 2–3%
in the cube emulator control. None of those experiments is a claimed fix.

## Next shared work

1. Refactor the general rotated-camera projection kernel: inspect invariant
   loads, SH-2 register spills and repeated clamp/divider setup. Measure both
   Dino orbit cameras and the cube's sparse matrix; retain clipping and exact
   projected-coordinate behavior.
2. Extend adoption of the topology/pose binding contract to other animated
   assets after checking their mutation lifetimes. Dino now uses it; remaining
   work includes measuring publication and flush validation independently.
3. Reduce prepared-face copying/cache invalidation and amortize VDP1 emission
   setup after measuring useful work versus waits. Preserve two independently
   animated actors, ownership, command capacity and draw completion.

Change one component at a time and retain only demonstrated improvements.
Backward API compatibility is not a requirement for this work; callers may
be migrated when a new contract is needed. Correctness and measured gains are.

## Reproduce the Dino comparison

Build the harness following [its instructions](../harness/README.md). Use the
normal model-import dependencies, FFmpeg and a local BIOS:

```sh
make all EXAMPLE=dino_demo PYTHON=/path/to/python -j1
python3 harness/scripts/dino_pipeline.py --bios /path/to/bios.bin \
  --out harness/build/dino-before --profile
python3 harness/scripts/dino_pipeline.py --bios /path/to/bios.bin \
  --out harness/build/dino-paused --scenario paused
python3 harness/scripts/dino_pipeline.py --bios /path/to/bios.bin \
  --out harness/build/dino-controls --scenario controls
```

Repeat after rebuilding into fresh output directories. Compare fixed poses
with Pillow installed:

```sh
python3 harness/scripts/dino_pipeline.py \
  --compare harness/build/dino-paused-before harness/build/dino-paused-after
```

The runner freezes each ELF/map/disc, supplies the BIOS Slave handoff during
injection, checks runtime diagnostics at 600 and 1,200 fields, measures the
warmed interval and rejects incomplete CPU profiles. `--build` can select a
previously retained directory. `analysis_checkout` identifies the analysis
checkout; hashes identify the actual retained executable, including when it
comes from an earlier build.

For independent Mednafen CD boot and a retained movie/screenshot:

```sh
python3 harness/scripts/cube_pipeline.py \
  --record-cd harness/build/dino-before --record-example dino_demo \
  --bios /path/to/bios.bin --out harness/build/dino-mednafen --seconds 30
```

The audit also exposed two shared build issues. Audio-bearing discs used GNU
`install -D`, unsupported by macOS's `install`; commit `3dc441f` replaces it
with directory creation/copying. Host tests neither loaded their `.d` files
nor retained dependencies from every translation unit in multi-source tests;
the dependency rule now merges and loads those inputs so header refactors
rebuild the actual tests.
