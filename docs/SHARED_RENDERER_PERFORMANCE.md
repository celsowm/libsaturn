# Shared renderer performance

See the [pipeline study](RENDERER_PIPELINE_STUDY.md) for the next targets,
actual per-CPU CCR values, observed drawing spans and ownership constraints.

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

## Exact projection kernel refactor

Projection now computes `trunc(trunc(coordinate * scale / depth) / 2)` instead
of dividing by twice the depth. These expressions are exactly equal under
signed truncation toward zero. The positive depth always fits signed 32 bits;
the doubled value previously required a software-division fallback above
`INT32_MAX`. Before starting the hardware divider, the new kernel clamps at
`+/-4096 * depth`, proving its intermediate quotient lies in `[-4095,4095]`.
The final divide by two operates on a 32-bit quotient. Signed viewport scales
also avoid negating a 64-bit intermediate before the Y multiplication.

The general rotated-camera loop is a separate non-inlined kernel. This lets
the compiler allocate registers independently from the sparse-camera loop.
Camera classification still runs per batch; matrices and poses may change
normally. No reciprocal approximation, reduced coordinate precision or altered
near-plane policy is involved. All callers of the shared projection benefit.

With the same Ymir setup and 12-field animated trace window, including both
the projection kernel and its public entry routine:

| CPU | Topology/pose binding baseline | New projection | Reduction |
| --- | ---: | ---: | ---: |
| Master exclusive execution cycles | 743,648 | 659,190 | 11.3% |
| Slave exclusive execution cycles | 573,970 | 503,140 | 12.3% |

These are function costs in a particular complete trace, not total-frame
speedups. Default animated and paused Dino remain at 20.0 submissions/s;
the control sequence measures 18.7 versus 18.5. All three runtime scenarios
pass at 600 and 1,200 fields. Paused, unlit model pixels remain identical.
The independent Mednafen CD boot also completes successfully.

The cube ladder measures the sparse-camera path with the same retained
split-visibility-kernel baseline used above:

| Cube workload | Before work | New work | Before submit/s | New submit/s |
| --- | ---: | ---: | ---: | ---: |
| Six quads per actor | 1.933 ms | 1.900 ms | 60 | 60 |
| RGB, 96 quads per actor | 9.281 ms | 8.898 ms | 60 | 60 |
| RGB, 384 quads per actor | 30.939 ms | 29.486 ms | 30 | 30 |
| RGB, 864 quads per actor | 64.644 ms | 61.382 ms | 15 | 15 |
| Textured, 864 quads per actor | 69.720 ms | 66.443 ms | 12 | 15 |
| Paired textured skin, 864 quads per actor | 59.715 ms | 58.203 ms | 15 | 15 |

Dense RGB geometry falls from 49.606 to 46.339 ms. The textured cadence change
crosses a field threshold; it is not a 25% CPU-work reduction. All six cube
model-region image comparisons have zero differing pixels. Skinning and
emission costs remain essentially unchanged.

An unsigned-magnitude prototype increased Dino projection costs by about 2–3%
and was discarded. Moving the factor two into the 64-bit numerator worked but
saved less than dividing the bounded 32-bit quotient by two. Those variants
are retained only as local experimental artifacts, not production kernels.
Host coverage compares the production kernel against independent signed
64-bit division, including both input signs, `INT32_MIN/MAX`, odd numerators,
clamp transitions, depths around the old doubled-divisor boundary, 50,000
random full-range scales and 50,000 random viewport-range scales. Existing
batch tests still exercise sparse/general camera changes and untouched X/Y
for nonpositive depth.

Retained final evidence: `harness/build/cube-project-split`,
`dino-project-split-animated`, `dino-project-split-paused`,
`dino-project-split-controls` and `dino-project-split-mednafen`.

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

1. Measure opportunities to overlap useful work with divider latency in the
   projection kernels. The exact quotient and separate camera loops above now
   cover both Dino orbit cameras and the sparse cube camera. Retain exact
   clipping and coordinate behavior; reject changes that only help one path
   while regressing the other.
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

## Immutable material command prefixes

For an instance with stable uploaded textures, compile the static part of its
VDP1 commands once. Dino uses this path for its shared opaque material table.
It keeps dynamic projected corners and the global painter ordering; it does
not cache a pose or submit a separate list per actor.

```c
static sat_scene3d_material_program_t commands[MATERIAL_COUNT];
static sat_scene3d_material_programs_t programs;

/* After upload and sat_scene3d_instance_binding_init(). Check each result. */
sat_result_t result = sat_scene3d_material_programs_init(
    &programs, materials, MATERIAL_COUNT, commands, MATERIAL_COUNT);
if (result != SAT_OK) return result;
binding.programs = &programs;
/* Build any sat_scene3d_source_plan only after attaching this table. */
```

Storage costs 16 bytes per material on SH-2 plus a 12-byte table. No allocation
occurs. A NULL `binding.programs` keeps ordinary dispatch. Unsupported RGB,
Gouraud and color calculation materials receive general entries and may coexist
with compiled entries. An instance slot override, unsafe projected bounds or
near clipping also retains general dispatch. Only opaque, unshaded, safe indexed
faces borrow compiled entries. Mesh effects and texture format remain encoded
in the native command prefix.

Materials, texture descriptors, the table and entry storage must remain
immutable and alive until queued faces have flushed and asynchronous tasks
have completed and released. Rebuild the table and source plan after changing
material fields, palette, dimensions, VRAM address or descriptor identity.
Invalid texture residency and owner generations still reject the complete
queue before emission. Recompilation is not a texture lifetime lock.

`sat_scene3d_face_t.program_valid` selects its active union member. Face
inspection tools must read `program` for compiled entries and `material` for
ordinary entries. Only initialized active fields may be compared or hashed;
projected-safe records do not carry initialized world corners.

The source plan publishes both compiled ranges on creation. Submitting CPUs
skip these static ranges afterward; every receiving backend refreshes them,
including Master fallback and a restarted Slave. Without a source plan the
conservative publisher refreshes the table and entries with ordinary sources.
Changing `binding.programs` requires source-plan republication.

On the controlled Dino run, median work fell from 36.709 to 34.236 ms and
emission from 6.728 to 4.938 ms. Submissions remained 20/s. Fixed-pose pixels
are identical. The procedural paired RGB cube, which does not opt into
compilation, pays 53.300 to 54.468 ms; its cadence remains 15/s. The additional
queue tag and dispatch have a cost. Preparation selects a specialized ordinary
or compiled loop once per instance; the initial shared-loop prototype was
worse at 55.157 ms. See the [study](RENDERER_PIPELINE_STUDY.md) and retained
`harness/studies/material-programs/results.json` for scope and evidence.
