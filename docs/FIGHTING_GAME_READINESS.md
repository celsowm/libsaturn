# Fighting-game readiness and implementation report

The skeletal character subsystem is implemented and exercised in three stock-RAM
Saturn examples. It provides a reusable foundation for a fighting game, but the
current two-fighter presentation does **not** meet a 30 or 60 rendered-frame/s
target. Physical Saturn acceptance remains open. Emulator measurements below
are evidence about these workloads, not hardware certification.

The initial measurements below correspond to the completed original scope.
The subsequent [character preparation study](CHARACTER_PREPARATION_PERFORMANCE.md)
reduces two-actor CPU cost by about 6 ms while retaining approximately 15
submissions/s; it documents the current implementation and added binding storage.

The implementation is commit
[`4c5bcee`](https://github.com/celsowm/libsaturn/commit/4c5bcee),
`feat(characters): integrate skeletal assets, dual SH-2 jobs and playable examples`.
The accompanying documentation commit is identifiable with
`git log -1 --format='%h %s' -- docs/FIGHTING_GAME_READINESS.md`.

## Delivered APIs and architecture

`skeleton3d.h` supplies parent-first fixed-point pose evaluation, quaternion
sampling, clips, playback, crossfades, events, root motion and attachments.
Caller storage supports large skeletons; a 64-joint chain is host-tested and
the optional robot uses 102 joints. Unchanged local matrices are reused while
parent changes still propagate through the existing transform evaluator.

`skin3d.h` supports compact rigid vertices and two/three-influence seam vertices
with distinct joint-local anchors, normals, bounds, mesh updates, face views,
LOD selection and bounded parallel jobs. Jobs reuse the existing executor,
handles and SH-2 cache publication rules. AUTO overlaps one Slave deformation
with the next actor's Master deformation. Forced SLAVE remains a comparison
mode; moving every job to Slave does not produce the same overlap.

`character3d.h` and `character_asset_format.h` add validated SKA1 archive views,
individual clip loading, managed material ownership and optional ground shadows.
Immutable topology stays resident; deformation changes per-instance vertices.
Fighters, floor and shadows use the canonical global scene painter, texture
registry and VDP1 backend. No new renderer, allocator or thread system is added.
`scene3d_faces.h` exposes optional FRT profiling; `time.h` exposes calibrated
timer frequency. Buffers and task counts are bounded, with no per-frame heap use.

The existing importer has a selectable skeletal C/SKA1 export and a generic
multi-anchor intermediate representation. Original example content is MIT;
the optional robot retains its CC BY 4.0 attribution. Native random-access clips
remain selected after the documented compression study. See
[CPU skinning and archive format](CPU_SKINNING_3D.md) and
[rendering contracts](CHARACTER_RENDERING_3D.md).

## Examples and content

The viewer provides nine named clips, speed, pause, camera, skeleton and lighting
controls. Vector Arena provides two independent fighters, deterministic opponent
logic, body separation, sidestepping, guard, attacks, hit reactions, knockdown,
health/timer and resource release/reload. The stress example compares 1/2/4/8
actors with MASTER/SLAVE/AUTO and selectable LOD and motion complexity.
Their READMEs document every control and project build/run command.

All characters have 24 named joints. High/medium/low detail use respectively
338/260/143 deformation vertices and 401/301/151 native quad faces. The high
asset includes 157 two-anchor vertices; all three influence counts are covered
by runtime correctness tests. Costumes use 24 indexed texture patterns each,
8,448 pixel bytes per owner, including one face panel and three baked lighting
variants. Directional light is baked from bind-pose surface orientation; it does
not follow animated normals. RGB Gouraud lighting is an explicit alternative.

Body separation uses fixed-point XZ circles every simulation tick, with bounded
wall correction. Walking into an opponent cannot swap the fighters' horizontal
order; sidestepping around the body remains allowed. The debug overlay shows
the body volume independently of hit/hurt proximity checks. This is example
gameplay logic, not accurate mesh collision or a complete combat engine.

## Validation on 2026-10-13

The full `make test` gate passed with the project's Python interpreter, including
existing host regressions and tool tests. New suites cover hierarchy/pose reuse,
fixed-point skinning, scene interleaving, material rules, archive corruption and
capacities, per-clip reads, independent owners, serial/parallel equivalence and
deterministic combat/body collisions. CMake public C/C++ header and source
manifest checks passed (3/3). Skeletal export, binary and surface-bake tool
suites passed (24 tests). The original GLB importer was also exercised with a
real skeletal SKA1 export. CI runs the corresponding host/tool/header checks.
An earlier gate exposed an undersized host texture-descriptor fixture after the
art update; the test now derives capacity from the archive header. The repeated
full gate passes. No failing ISO builds remain; the optional robot also rebuilt
successfully before restoring the default original-character viewer.

`build-example.ps1` built all three ISOs with stock memory and the 16 KiB Master
stack guard. `harness/run-character-plan.ps1` exercised the viewer for 1,860
fields, arena for 1,150, stress for 3,100 and sustained body collision for 750.
The viewer's paused scene matched pixel for pixel. Arena diagnostics recorded
hits, blocks, two accepted actors and four resource loads across round reset.
Every workload retained all requested actors, command capacity and serial/worker
equivalence, with zero panic/error counters. The collision scenario holds RIGHT
for 500 fields and checks post-resolution positions and accumulated overlaps.
It recorded 720 checked simulation ticks and zero overlaps, including wall
contact, with the player still on the original side of the opponent.

Mednafen is the default interactive emulator. Actual disc-backed character
loading was separately exercised there with no expansion cartridge. Ymir boots
the BIOS then injects the application, so its standard tests explicitly use
embedded archives through the same asset cache; they do not prove BIOS CD boot.
Generated maps, JSON, RAM dumps, logs and screenshots remain in `harness/build/`.
Reproduce automatic checks with:

```powershell
.\harness\run-character-plan.ps1 -Bios bios/saturn_bios_us.bin
```

## Measured CPU and rendering costs

The following NTSC Ymir results use contiguous VBlank intervals, omit three
transition samples and retain all elapsed 60 Hz simulation steps. Submission/s
counts completed list submissions, not distinct displayed images. CPU/skin
columns are median milliseconds. One/two actors use HIGH, four MEDIUM and eight
LOW. The varied-clip interval is kept separate from this comparison.

| Actors | MASTER submissions/s; CPU; skin | SLAVE submissions/s; CPU; skin | AUTO submissions/s; CPU; skin |
| --- | --- | --- | --- |
| 1 | 20.00; 34; 4 | 20.00; 33; 4 | 20.14; 33; 4 |
| 2 | 15.00; 60; 9 | 15.00; 60; 9 | 15.00; 56; 5 |
| 4 | 10.00; 95; 14 | 10.00; 94.5; 13 | 10.00; 89; 7 |
| 8 | 7.52; 119; 15 | 7.50; 120; 16 | 8.57; 112; 9 |

Scene preparation costs approximately 14/29/44/48 ms in these AUTO workloads.
The two-fighter arena snapshot used 676 vertices, 402 visible faces and 569 of
2,048 command slots, with 4 ms skinning, 31 ms preparation, 3.291 ms sorting and
7.851 ms emission. The largest eight-actor snapshot used 1,144 vertices and
903 commands. Geometry preparation and frame handoff remain substantial costs;
skin acceleration alone is insufficient for the desired display rate.

## Stock-memory accounting

The final builds and emulator gates recorded the following bounded application storage. This
includes actor/archive/scene buffers; it excludes unrelated library storage,
code and stack. Link-map occupied endpoints account for those separately.
Collision instrumentation adds only 24 bytes to WRAM-H.

| Example | Owned buffers | Character pixels | WRAM-H occupied end | WRAM-L occupied end |
| --- | ---: | ---: | --- | --- |
| Viewer | 336,820 B | 8,448 B | `0x060c7018` | `0x00266350` |
| Arena | 461,640 B | 16,896 B | `0x060e59b8` | `0x00266350` |
| Stress | 526,380 B | 8,448 B | `0x060f5628` | `0x00266350` |

WRAM-H ends at `0x06100000`; WRAM-L at `0x00300000`, with 512 bytes reserved
for the executor. The tightest listed WRAM-H build leaves 43,480 bytes above
its BSS endpoint, including the required 16,384-byte Master stack reserve.
No RAM expansion is required. The high archive is 96,074 bytes per costume;
resident clip poses use 71,520 bytes. Constant-channel analysis reduces those
poses to 16,464 bytes with exact integer values, but its runtime decoder has
not been SH-2 benchmarked and is not advertised as implemented acceleration.

The fixed scene arrays occupy 212,000 bytes, an actor's pose/skin/geometry and
scratch arrays 27,080 bytes, and each resident archive/descriptor/material slot
97,740 bytes. A high-detail deformation output alone is 4,056 bytes; three
338-vertex vector arrays (local output, world output and normals) reserve
12,168 bytes per actor. All are counted in the owned-buffer column, including
scratch capacity that remains allocated at lower LOD. No per-frame allocator
peak is hidden in those figures; physical stack high-water marks remain unmeasured.

Final SH-2 link-map section sizes are below. The linker places read-only tables
inside `.text`, so these are whole-image sizes rather than code-only subsystem
costs. `.wram_l` includes the bounded benchmark log and existing library buffers.

| Example | `.text` including read-only tables | `.data` | `.bss` | `.wram_l` |
| --- | ---: | ---: | ---: | ---: |
| Viewer | 380,772 B | 492 B | 417,480 B | 418,640 B |
| Arena | 381,280 B | 492 B | 542,312 B | 418,640 B |
| Stress | 381,172 B | 492 B | 607,048 B | 418,640 B |

## Remaining acceptance and port work

Run the [physical hardware checklist](HARDWARE_ACCEPTANCE_CHECKLIST.md) before
claiming Saturn compatibility or final timings. Cache visibility, CD timing,
VDP1 color behavior, stack high-water marks and PAL behavior need physical or
additional targeted evidence. Indexed texture lighting and contact shadows
are approximations; arbitrary intersections cannot be resolved exactly by a
painter with no Z-buffer. Grabs/throws and intersecting textured limbs still
need content-specific visual acceptance, rather than an assertion that sorting
alone solves them.

Library work for higher-end fighting games includes profiling/optimizing scene
preparation and command emission, conservative pre-skin culling, measuring an
optional compact-channel decoder on SH-2, and selecting lower-cost visual assets
against a real target display budget. A faithful port additionally requires its
own legal asset conversion, move/frame data, hit/hurt volumes, collision rules,
throws, camera/stage rules, effects, audio and game-specific animation semantics.
None of those proprietary formats or combat assumptions belong in the core API.

The earlier robot optimization and rejected render experiments are documented
in [the performance study](SKELETAL_ANIMATION_PERFORMANCE_STUDY.md). This report
records emulator measurements; it does not convert them into hardware estimates.
