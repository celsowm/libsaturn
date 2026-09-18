# LibSaturn Animated glTF/GLB Model Pipeline — Execution Plan

## Purpose

Implement a complete, generic, offline animated 3D asset pipeline for LibSaturn and finish `examples/basic_3d_animation` as an interactive Sega Saturn ISO.

The acceptance example is expected to use these files when they are present in the repository:

```text
examples/basic_3d_animation/assets/male_basic_walk_30_frames_loop.glb
examples/basic_3d_animation/assets/LICENSE.txt
```

The supplied GLB is the acceptance asset, **not** the architecture. The feature being built is a reusable LibSaturn pipeline for modern glTF/GLB skinned and animated models.

The final ISO must show the animated textured character walking in a stable loop and allow the user to orbit, pitch and zoom the camera around the model. The implementation must remain appropriate for a bare-metal Sega Saturn: no runtime GLB parsing, no runtime PNG decoding, no heap dependency, no skeletal skinning cost unless explicitly justified by measurement, and no per-frame texture upload.

This plan also creates a reusable host-side polygon simplification tool under `tools/` based on modern attribute-aware QEM/edge-collapse simplification with animation-aware and silhouette-aware validation.

---

# Harness execution contract

This document is intended to be executed by a coding harness from top to bottom.

Rules:

1. Work directly in `celsowm/libsaturn` on `main`.
2. Inspect the current repository before implementing anything. The repository may have changed after this plan was written.
3. Read `docs/TEXTURED_3D_MODEL_PIPELINE_PLAN.md` first. Reuse any static textured-model infrastructure already implemented from that plan. Do not build a second incompatible model system.
4. If the textured-model plan has not yet been implemented, implement only the prerequisites required here using the same contracts, or complete that plan first when that is cleaner. Do not duplicate UV baking, palette handling, texture deduplication or model asset formats.
5. Run relevant tests after every coherent phase.
6. Do not stop at parsing a GLB, generating a simplified mesh, rendering a static character, or producing an ISO that does not animate.
7. Do not add asset-specific hacks, bone names, material names, hard-coded coordinates, hard-coded UVs, or special cases for the supplied walking model.
8. Do not parse GLB/glTF, decode image formats, simplify geometry, evaluate a skeleton, or quantize textures on the Saturn unless this plan explicitly says the operation belongs at runtime.
9. Do not add heap allocation to the Saturn runtime.
10. Do not add static constructors.
11. Preserve source compatibility for existing public APIs and examples unless a change is demonstrably necessary.
12. Commit coherent, green phases directly to `main`.
13. Do not silently replace the supplied example asset.
14. Before committing or redistributing the supplied GLB or generated derivatives, inspect `examples/basic_3d_animation/assets/LICENSE.txt` and comply with it. If redistribution or derivative redistribution is not allowed, keep the external asset as a local acceptance fixture and add a repository-owned permissively licensed animated fixture for CI instead.
15. If the GLB is not present when the acceptance phase begins, report the exact missing path and stop before claiming the example complete. Generic tool/runtime work may still proceed using repository-owned fixtures.
16. Do not hard-code observed counts from the acceptance GLB into any algorithm. Counts are diagnostics only.

---

# Definition of done

This project is complete only when all of the following are true:

```text
modern animated GLB
  mesh + UV + material + skin + skeleton + animation
                    |
                    | host-side importer
                    v
           source model representation
                    |
          +---------+----------+
          |                    |
          v                    v
 animation-aware          canonical UV
 simplification           texture baking
          |                    |
          v                    v
 Saturn face budget       dedup indexed8
          |                    |
          +---------+----------+
                    |
                    v
       baked animation vertex frames
                    |
                    v
       generated Saturn C/H asset
                    |
                    | startup upload
                    v
           VDP1 texture handles
                    |
                    | per-frame pose decode
                    v
          caller-owned mesh vertices
                    |
                    | cull + painter sort
                    v
          VDP1 distorted sprites
                    |
                    v
     interactive basic_3d_animation ISO
```

The ISO must:

- render the supplied character recognizably;
- preserve texture orientation and major silhouette features;
- play the walking animation as a stable loop;
- permit interactive orbit, pitch and zoom;
- remain stable while camera and animation are active together;
- fit within VDP1 command and texture VRAM limits;
- perform no texture upload in the frame loop;
- perform no GLB parsing, PNG decoding or polygon simplification on-console;
- perform no heap allocation;
- introduce no static constructors.

---

# Known acceptance-asset observations

These are observations from the supplied walking GLB and must be re-verified by the importer. They are **not** constants or required assumptions:

```text
approximately 1 mesh
approximately 3,176 vertices
approximately 1,552 source triangles
1 material / texture set
approximately 31 joints
1 looping animation
approximately 1 second duration
approximately 30 Hz animation sampling
embedded texture around 256x256
```

The source triangle count is intentionally far above a sensible single-object VDP1 command budget. The pipeline must therefore prove that simplification is not an optional demo feature but a real hardware-adaptation stage.

---

# Phase 0 — Baseline, dependency audit and hardware budget

Before modifying code, inspect at minimum:

```text
docs/TEXTURED_3D_MODEL_PIPELINE_PLAN.md
include/saturn/mesh3d.h
include/saturn/render3d.h
include/saturn/vdp1.h
include/saturn/input.h
include/saturn/math3d.h
src/core/mesh3d_api.cpp
src/core/mesh3d_logic.hpp
src/core/render3d_api.cpp
src/core/render3d_logic.hpp
src/core/math3d_api.cpp
src/core/math3d_logic.hpp
src/core/vdp1_api.cpp
src/hal/vdp1.cpp
src/hal/vdp1.hpp
Makefile
build-example.ps1
run-example.ps1
tools/
tests/
tests/host/
harness/
```

Also inspect the vendored VDP1 hardware documentation before encoding limits.

Record:

- current host-test results;
- current Python-tool test results;
- current `examples-all` result where supported;
- actual command-list capacity and reserved setup/end commands;
- command consumption of one textured face;
- current texture VRAM allocation strategy;
- current palette/CRAM strategy;
- current sorting face-count limit;
- whether the static textured model pipeline already exists and its public contracts.

The current runtime historically used an 8-bit face sort order in places. Do not assume that old limit remains correct. Inspect it. If animated models require more faces than the sort representation allows but still fit VDP1 command capacity, widen the sort-index representation cleanly instead of splitting a character arbitrarily just to preserve an internal `uint8_t` limitation.

Baseline gate:

```text
make test
python -m unittest discover tests
```

Run the existing harness suite normally used by the repository.

---

# Phase 1 — Acceptance asset and license audit

When these files exist:

```text
examples/basic_3d_animation/assets/male_basic_walk_30_frames_loop.glb
examples/basic_3d_animation/assets/LICENSE.txt
```

inspect `LICENSE.txt` before generating committed derivatives.

Record in the implementation report:

- redistribution allowed or not;
- derivative redistribution allowed or not;
- attribution requirements;
- whether the original GLB may remain in the public example directory;
- whether generated C/H derived data may be committed.

If the license prevents public redistribution, do **not** bypass it. Use the file locally as an acceptance fixture and create a tiny permissively licensed animated glTF fixture for committed tests.

The generic pipeline must not depend on this asset's filename or structure.

---

# Phase 2 — Tool architecture

Create a reusable host-side model tool stack. Prefer this structure unless repository conventions suggest a cleaner equivalent:

```text
tools/
├── import_model.py
├── simplify_model.py
├── model_pipeline/
│   ├── __init__.py
│   ├── gltf.py
│   ├── obj.py                  # reuse/static pipeline if already present
│   ├── mesh.py
│   ├── skinning.py
│   ├── animation.py
│   ├── simplification.py
│   ├── metrics.py
│   ├── silhouette.py
│   ├── lod.py
│   ├── uv_bake.py              # reuse/static pipeline
│   ├── palette.py              # reuse/static pipeline
│   ├── saturn_profile.py
│   ├── emit_c.py
│   └── report.py
└── model_native/
    ├── CMakeLists.txt
    ├── meshopt_simplifier.cpp
    └── third_party/
        └── meshoptimizer/
```

Exact names may change, but preserve separation of concerns.

`import_model.py` is the end-to-end compiler.

`simplify_model.py` is a standalone inspection/optimization tool that can simplify an animated GLB and emit a simplified GLB plus JSON report without requiring Saturn asset generation.

The native simplifier is a host build artifact only. It must never be linked into `libsaturn.a` or the SH-2 example binary.

---

# Phase 3 — glTF/GLB reader

Implement the glTF 2.0 subset required for robust animated character import.

Support `.glb` first. Supporting textual `.gltf` with external files is desirable if it does not compromise the implementation, but `.glb` is mandatory.

Required GLB/glTF concepts:

- GLB header/chunks;
- JSON chunk;
- BIN chunk;
- buffers;
- bufferViews;
- accessors;
- component types;
- normalized integer accessors;
- byte offsets;
- byte stride/interleaved vertex data;
- nodes;
- meshes;
- primitives;
- materials;
- textures/images;
- embedded image bufferViews;
- `POSITION`;
- `NORMAL` where present;
- `TEXCOORD_0`;
- `JOINTS_0`;
- `WEIGHTS_0`;
- indices with 8/16/32-bit legal glTF index formats;
- skins;
- inverse bind matrices;
- animations;
- samplers;
- channels;
- translation/rotation/scale animation targets;
- quaternion rotation;
- STEP and LINEAR interpolation;
- CUBICSPLINE either implemented correctly or rejected with a precise diagnostic in the first version.

Support triangle-list primitives. Reject unsupported primitive modes with a clear message unless deterministic conversion is implemented.

Do not make the first importer depend on a giant runtime framework. A small Python parser using standard library binary/JSON facilities is acceptable. Pillow may be used for embedded image decoding because the repository already uses it in host tools.

Add deterministic parser tests using tiny generated GLB fixtures.

Required tests include:

- valid GLB header/chunks;
- malformed/truncated GLB;
- accessor offset and stride;
- normalized weights;
- 16-bit and 32-bit indices;
- embedded PNG extraction;
- node transforms;
- skin accessors;
- animation channel parsing;
- unsupported primitive diagnostic.

---

# Phase 4 — Canonical source model representation

Create a host-only source representation shared by OBJ and glTF importers where practical.

It should be able to represent at least:

```text
vertices
normals
UVs
triangle indices
materials
textures
skin joint indices
skin weights
skeleton hierarchy
inverse bind matrices
animation clips
```

Do not force Saturn-specific A/B/C/D quad conventions into the parser. Parsing and Saturn compilation are separate stages.

Normalize skin weights deterministically. Handle vertices with fewer than four meaningful influences. Reject invalid joint indices.

Preserve material boundaries and UV seams explicitly so simplification can protect them.

---

# Phase 5 — Host skeletal animation evaluator

Implement a deterministic host-side evaluator for glTF skinning.

The evaluator must:

1. compute node local transforms;
2. compute global node transforms through the hierarchy;
3. sample animation channels at arbitrary times;
4. normalize interpolated quaternions;
5. compute skin matrices using the glTF skin contract and inverse bind matrices;
6. evaluate weighted skinned vertex positions;
7. evaluate skinned normals where needed for validation;
8. support looping clips cleanly.

Do not assume the source clip is exactly 30 samples or exactly one second. Inspect its sampler input times.

For the acceptance asset, verify that the end pose loops cleanly to the first pose. If a duplicate terminal sample represents `t=duration`, do not store an unnecessary duplicate runtime frame.

Add tests using analytically simple skeletons:

- one translated joint;
- one rotated joint;
- two weighted joints;
- hierarchy parent/child transform;
- quaternion interpolation;
- loop end behavior.

---

# Phase 6 — Native attribute-aware simplification engine

Integrate a modern QEM/edge-collapse simplifier suitable for production offline use.

Preferred implementation: pin and vendor `meshoptimizer` under the host tools, including its license and pinned source revision. If the repository's policy prefers a fetched dependency, make the fetch deterministic and pinned. Do not track an unpinned moving `master`.

Use the strongest attribute-aware simplification API available in the pinned version after inspecting that version's actual API. Do not blindly code against a function name from memory.

The simplifier must account for more than geometric position.

Protected/weighted attributes should include where applicable:

- normals;
- UV coordinates;
- UV seams;
- material boundaries;
- skinning influence discontinuities;
- animation importance;
- boundary/silhouette importance.

The native tool should expose a stable machine-readable protocol to Python. Prefer file/stdio data with a documented binary or JSON envelope over brittle command-line arrays containing thousands of numbers.

The native layer should be small: mesh optimization belongs there; GLB parsing, animation sampling, quality validation and reporting stay in Python.

Add host-native tests for:

- trivial triangle no-op;
- planar grid reduction;
- seam protection;
- material-boundary locking;
- deterministic output;
- requested target behavior;
- no invalid indices;
- no degenerate explosion.

---

# Phase 7 — Animation-aware simplification importance

Do not simplify an animated character solely in bind/rest pose.

Before simplification, sample the source clip at deterministic times. Default to every authored frame/sample when the clip is small; otherwise sample at a configurable rate plus key extrema.

For each source vertex compute an animation-importance signal from its motion/deformation across sampled poses.

At minimum include:

- displacement range across the clip;
- deviation from rigid/rest behavior;
- proximity to highly deforming joint regions inferred from weights;
- normal variation if available.

High-motion articulation regions such as elbows, knees, shoulders, hips, ankles and neck must naturally become more expensive to damage through the generic metrics, not through bone-name heuristics.

Feed this information into simplification using the strongest mechanism supported by the selected simplifier:

- attribute weights;
- lock masks;
- per-vertex importance converted into protected constraints;
- staged reduction where high-importance regions are simplified later.

Do not fork meshoptimizer into a large custom QEM implementation unless measurements prove the API cannot meet the required quality.

---

# Phase 8 — Silhouette-aware importance

For low-resolution console rendering, silhouette preservation is often more visually important than internal tessellation.

Implement a host-side silhouette importance pass.

Use a deterministic set of camera directions around the model, for example 16 or 32 approximately uniform directions. Evaluate multiple representative animation poses.

A practical implementation may either:

- detect mesh silhouette edges geometrically from face orientation per view; or
- use a tiny deterministic software rasterizer and compare silhouette masks.

The result should increase preservation cost for vertices/edges that repeatedly contribute to the visible outline.

No asset-specific camera angles.

Expose a CLI control such as:

```text
--silhouette-views 32
--silhouette-weight 5.0
```

and sensible quality presets.

Add tests using simple shapes where silhouette damage is obvious.

---

# Phase 9 — Standalone `tools/simplify_model.py`

Create a user-facing standalone simplification tool.

Required basic usage:

```bash
python tools/simplify_model.py \
  --input model.glb \
  --output build/model_simplified.glb \
  --target-triangles 300 \
  --animation-aware \
  --preserve-uv \
  --preserve-silhouette \
  --report build/model_simplified.report.json
```

Saturn-oriented usage:

```bash
python tools/simplify_model.py \
  --input model.glb \
  --output build/model_saturn.glb \
  --profile saturn-vdp1 \
  --max-triangles 280 \
  --animation-aware \
  --generate-lods \
  --report build/model_saturn.report.json
```

CLI categories should include:

```text
General
  --input
  --output
  --report
  --target-triangles
  --target-ratio
  --target-error
  --quality conservative|balanced|aggressive

Preservation
  --preserve-uv / --no-preserve-uv
  --preserve-normals
  --preserve-boundaries
  --preserve-silhouette
  --animation-aware

Animation
  --animation NAME_OR_INDEX
  --sample-fps N
  --sample-all-authored-frames
  --animation-weight FLOAT

Silhouette
  --silhouette-views N
  --silhouette-weight FLOAT

Saturn
  --profile saturn-vdp1
  --max-triangles N
  --max-vdp1-commands N
  --generate-lods

Diagnostics
  --verbose
  --dump-metrics
```

The tool must not blindly force the requested triangle count if quality gates fail.

If `280` triangles damages the animated surface beyond the active quality threshold, the tool should search upward for the smallest acceptable triangle count and clearly report that it delivered, for example, `347` instead of `280`.

That behavior is a core feature, not a warning-only afterthought.

---

# Phase 10 — Quality validation and automatic target search

The simplifier must validate candidate output against the source, across animation poses.

Implement deterministic quality metrics normalized by model scale.

At minimum:

## Animated surface error

For representative/all sampled poses, compare the original animated surface against the simplified animated surface.

A robust offline metric should sample original vertices and/or deterministic barycentric surface points and compute nearest-point distance to simplified triangles.

Track at least:

```text
mean error
RMS error
95th percentile
maximum error
```

Normalize by source model bounding-box diagonal so thresholds are scale-independent.

## Normal error

Where normals are available, report angular deviation statistics on matched/sample points.

## UV integrity

Require zero illegal cross-seam collapses under conservative/balanced profiles. Report seam violations explicitly.

## Material integrity

Require zero material-boundary merges unless an explicit aggressive option permits them.

## Silhouette error

Compare deterministic rendered/geometric silhouettes from multiple views and representative animation poses.

A software-mask implementation should report metrics such as IoU or differing-pixel percentage.

## Search policy

Implement a deterministic target search:

```text
requested target
      |
      v
candidate simplify
      |
      v
quality metrics
      |
  +---+---+
  |       |
PASS     FAIL
  |       |
try      increase
lower    triangle count
count      |
  +----<---+
```

Use binary search or another bounded deterministic strategy rather than decrementing one triangle at a time.

Quality presets should be explicit data structures, not magic values scattered throughout code.

A reasonable starting point to calibrate, not immutable truth:

```text
conservative:
  very high seam/boundary protection
  max animated surface error around <= 0.5% bbox diagonal
  strong silhouette preservation

balanced:
  max animated surface error around <= 0.75-1.0% bbox diagonal
  strong seam protection
  moderate/strong silhouette preservation

aggressive:
  higher geometric tolerance
  seams/material boundaries still protected by default
```

Tune thresholds against tests and document final values.

---

# Phase 11 — Simplified GLB output

The standalone simplifier should emit a valid simplified `.glb` for inspection in ordinary desktop viewers.

Preserve as applicable:

- node hierarchy;
- skin;
- inverse bind matrices;
- animation channels;
- material;
- embedded image;
- UVs;
- joint indices;
- joint weights;
- normals;
- simplified indices/vertex set.

If simplification keeps a subset/remap of source vertices, compact all vertex attributes consistently.

Do not emit stale accessors or dangling joint/vertex references.

Output must be deterministic: identical input/options should produce byte-identical GLB if practical. If JSON ordering or container padding makes exact GLB bytes impractical, all semantic buffers and the JSON report must be deterministic and a normalized semantic hash must be stable.

Add a round-trip test: parse generated simplified GLB again and validate counts, indices, skin references and animation presence.

---

# Phase 12 — LOD generation

Support optional generation of multiple validated LODs from the source model.

Do not recursively simplify LOD1 into LOD2 if doing so compounds errors unnecessarily. Prefer deriving each LOD directly from the original source under its own target/quality gate.

Example targets may be selected from the measured hardware budget, e.g.:

```text
LOD0  near       ~350-450 triangles
LOD1  normal     ~220-300 triangles
LOD2  far        ~120-180 triangles
LOD3  very far    ~60-100 triangles
```

These are not hard-coded universal counts.

The report must state which LODs pass which quality profiles.

Runtime LOD switching is optional for the first `basic_3d_animation` ISO. Generating and validating LODs is required if `--generate-lods` is requested.

---

# Phase 13 — Saturn command-budget profile

Create a host-side Saturn profile that converts hardware/runtime constraints into a model face budget.

Do not encode only `max_triangles=280` and call it a Saturn profile.

The profile should consider:

- VDP1 command-list capacity;
- setup commands inserted each frame;
- end command;
- expected HUD/debug command reserve;
- one distorted-sprite command per visible textured triangle/quad;
- any extra commands required by the example;
- sorting representation limits;
- texture VRAM budget after command area;
- palette use.

The simplifier report should estimate:

```text
worst-case model face commands
reserved non-model commands
total command estimate
command headroom
unique baked texture bytes
palette bytes
estimated VDP1 VRAM headroom
```

The final acceptance model must have safety headroom. Do not target 100% of command capacity.

---

# Phase 14 — Offline animation strategy: bake poses, not skeletal skinning on SH-2

For the first generic animated-model runtime, do **not** perform four-weight skeletal skinning for thousands of vertices every frame on the Saturn.

Use the host skinning evaluator to bake simplified-model vertex positions for runtime animation frames.

The source remains skeletal. The generated Saturn representation becomes vertex animation.

```text
GLB skin + bones + animation
             |
             | host skinning
             v
      frame 0 positions
      frame 1 positions
      frame 2 positions
            ...
             |
             v
   compact Saturn pose stream
```

Texture mapping, face indices and material assignment remain shared across all frames.

Do not bake a different texture set per frame.

Do not store source joints/weights/inverse-bind matrices in the runtime asset unless a later runtime-skinning feature explicitly needs them.

---

# Phase 15 — Runtime animation frame rate and loop sampling

Animation sampling must preserve clip timing rather than assuming `frame = vblank / 2` globally.

The generated animation descriptor should include at least:

```text
frame_count
sample_rate or frame duration
loop flag
duration
```

The runtime should advance animation with a fixed-point time accumulator based on VBlank timing so both NTSC and PAL behave sensibly.

For a 30 Hz clip:

```text
NTSC ~60 Hz -> roughly one animation frame every two VBlanks
PAL  ~50 Hz -> fractional accumulator, not a broken integer divisor
```

Provide pause/resume and reset capability for the example.

The terminal source key at exactly `duration` should not become a redundant extra runtime frame if it is equivalent to frame zero for a loop.

---

# Phase 16 — Compact baked-pose representation

Do not automatically store all animation vertices as 32-bit 16.16 triples if a compact representation gives a major memory win with negligible visual error.

Implement or evaluate a compact deterministic encoding, preferably signed 16-bit local positions with per-model or per-axis scale/bias.

Conceptual format:

```c
typedef struct sat_anim_position_encoding {
    sat_fx16_t bias_x;
    sat_fx16_t bias_y;
    sat_fx16_t bias_z;
    sat_fx16_t scale_x;
    sat_fx16_t scale_y;
    sat_fx16_t scale_z;
} sat_anim_position_encoding_t;

/* frame_count * vertex_count * xyz */
const int16_t position_stream[];
```

At runtime decode one pose into caller-owned `sat_vec3_t vertices[]` used by the mesh renderer.

Requirements:

- deterministic quantization;
- bounded error reported by the importer;
- no heap;
- decode one frame without touching texture state;
- interpolation may be added later but is not required initially;
- benchmark decode cost on the target build/emulator.

If 16-bit quantization damages the acceptance model beyond the configured quality threshold, use a higher-precision encoding and report the measured tradeoff. Do not choose compression over correctness blindly.

---

# Phase 17 — Animated compiled asset format

Extend/reuse the static generated model format rather than creating a parallel renderer-specific universe.

A conceptual addition is:

```c
typedef struct sat_model_animation_asset {
    const int16_t* positions;
    uint16_t frame_count;
    uint16_t vertex_count;
    uint16_t sample_rate_num;
    uint16_t sample_rate_den;
    uint16_t flags;
    sat_anim_position_encoding_t encoding;
} sat_model_animation_asset_t;

typedef struct sat_animated_model_asset {
    const sat_model_asset_t* model;
    const sat_model_animation_asset_t* animations;
    uint16_t animation_count;
} sat_animated_model_asset_t;
```

Exact API may improve after inspecting the static model implementation.

Requirements:

- one immutable top-level generated descriptor;
- no runtime GLB structures;
- no joints/weights in the first runtime format;
- shared static indices and face-texture mapping;
- shared deduplicated texture set;
- one or more named/indexed animation clips possible in the format even if the acceptance asset has only one;
- caller-owned runtime state;
- no global hidden animation registry.

Add helpers conceptually equivalent to:

```text
initialize animation state
set clip
reset clip
advance time
sample/decode current pose into caller vertex buffer
```

Keep rendering separate from animation state update.

---

# Phase 18 — Reuse the textured-model UV baking pipeline

Animated geometry does not require animated textures for this acceptance target.

Reuse the static pipeline's:

- UV canonicalization;
- triangle-to-degenerate-quad convention if VDP1 rendering still uses it;
- canonical face texture baking;
- indexed8 palette generation;
- transparency rules;
- texture deduplication;
- texture upload helpers;
- face-to-texture mapping.

Perform canonical face-texture baking from the **simplified topology**, not from the original 1,552 faces that will never be rendered.

The order should normally be:

```text
parse source
   |
   v
simplify topology
   |
   v
compact/remap attributes
   |
   +------> bake animation poses
   |
   +------> bake final face textures
                 |
                 v
             deduplicate
```

This avoids wasting bake time and VRAM budget on faces removed by simplification.

UV seams and orientation must remain correct after simplification.

---

# Phase 19 — End-to-end `tools/import_model.py`

Extend the generic importer so one command can compile the animated GLB for Saturn.

Target usage:

```bash
python tools/import_model.py \
  --input examples/basic_3d_animation/assets/male_basic_walk_30_frames_loop.glb \
  --target saturn \
  --simplify auto \
  --quality balanced \
  --animation all \
  --out-prefix build/generated/basic_3d_animation/male_walk
```

Useful options should include:

```text
--input
--out-prefix
--symbol
--target saturn
--scale
--flip-x/y/z
--reverse-winding
--simplify off|auto|TARGET
--quality conservative|balanced|aggressive
--max-triangles
--max-vdp1-commands
--animation all|NAME|INDEX
--animation-fps source|N
--palette-index
--texture-quality FLOAT
--generate-lods
--report PATH
```

When `--simplify auto` is selected with `--target saturn`, determine the smallest model satisfying both quality gates and Saturn resource gates. Do not merely use a static triangle count.

The end-to-end importer must print a concise human report and write a machine-readable JSON report.

---

# Phase 20 — Generated report

For an animated model, report at least:

```text
SOURCE
  vertices
  triangles
  materials
  textures
  joints
  animation clips
  selected clip duration
  source key/sample count

SIMPLIFICATION
  requested target
  delivered triangle count
  delivered vertex count
  reduction percentage
  protected UV seams
  material boundaries
  quality preset

ANIMATION QUALITY
  sampled poses
  mean animated surface error
  RMS error
  p95 error
  max error
  silhouette metric
  normal error metric

SATURN ANIMATION
  baked frame count
  runtime sample rate
  duplicate loop frame removed yes/no
  position encoding
  pose-stream bytes
  quantization error

TEXTURES
  baked face count
  unique textures after dedup
  indexed pixel bytes
  palette bytes
  estimated texture VRAM use

VDP1
  worst-case model commands
  reserved commands
  command headroom

RESULT
  PASS/FAIL saturn-vdp1 profile
```

JSON keys must be stable enough for automated assertions.

---

# Phase 21 — Build-system integration

Integrate animated generated assets with the existing per-example `Makefile.inc` pattern.

Do not hard-code the acceptance GLB into the root Makefile.

The example should declare its own model generation inputs/outputs.

Model generation must rebuild when:

- the GLB changes;
- the importer changes;
- the simplifier changes;
- relevant model-pipeline Python modules change;
- simplification/profile options change.

Do not rebuild generated model assets every invocation when inputs are unchanged.

Add a host-tool build target if needed:

```text
make model-tools
```

or integrate the native simplifier build automatically as a dependency of model import.

Keep Windows/MSYS2 and normal shell workflows working.

---

# Phase 22 — `examples/basic_3d_animation`

Finish/create:

```text
examples/basic_3d_animation/
├── assets/
│   ├── male_basic_walk_30_frames_loop.glb
│   └── LICENSE.txt
├── Makefile.inc
└── main.c
```

If licensing requires additional attribution files, include them.

The example should be an interactive animated-model viewer, not a static screenshot.

Runtime startup:

```text
initialize app/video/input
        |
        v
upload model palette(s) once
        |
        v
upload unique baked textures once
        |
        v
initialize animation state
        |
        v
allocate/bind caller-owned pose vertex buffer
        |
        v
main loop
```

Main loop:

```text
poll pad
   |
advance animation time
   |
decode current pose vertices
   |
update camera
   |
build view-projection
   |
begin VDP1 frame
   |
cull + sort + draw textured model
   |
draw small HUD if enabled
   |
submit / wait VBlank
```

No texture upload inside the loop.

---

# Phase 23 — Interactive controls

Use the existing digital pad API.

Default controls:

```text
LEFT / RIGHT   orbit camera yaw
UP / DOWN      camera pitch
L / R          zoom out / zoom in
A              pause/resume animation
B              reset camera and animation
C              cycle available LOD if runtime LODs are implemented;
               otherwise toggle a useful debug mode
START          toggle HUD/help
```

Clamp pitch and zoom to useful ranges.

Camera motion must be frame-rate independent enough for NTSC/PAL behavior.

Prefer moving the camera around the model rather than destructively transforming generated vertices every frame. Animation pose decode should produce model-local positions; camera/view matrix handles inspection.

---

# Phase 24 — HUD and debug observability

A small optional HUD is useful for acceptance.

Display compactly, without consuming excessive VDP1 commands:

```text
ANIM RUN/PAUSE
FRAME xx/yy
TRIS nnn
TEX nn
LOD n
CAM DIST ...
```

Do not make the HUD mandatory if it prevents the model from fitting the command budget. Reserve its command budget explicitly in the Saturn profile.

If a runtime frame misses capacity, expose that clearly during development rather than silently rendering corrupt lists.

---

# Phase 25 — Runtime performance gates

Measure or estimate the frame-loop cost of:

- pose decode;
- matrix transforms/projection;
- culling;
- painter sorting;
- VDP1 command emission.

Do not perform nearest-surface quality checks, simplification metrics, silhouette analysis or any other host validation at runtime.

The acceptance example must maintain a stable interactive update rate in the existing target emulator flow. Exact FPS target should be documented after measurement; do not fabricate one before profiling.

If pose decoding dominates:

1. profile first;
2. reduce unnecessary copies;
3. consider a decode format optimized for SH-2;
4. consider storing 16.16 frames only if memory is acceptable;
5. do not remove animation-aware quality protections just to save decode time.

---

# Phase 26 — Automated tests: glTF and animation

Add Python tests for:

- GLB parser;
- accessor stride/offset;
- normalized joint weights;
- skin hierarchy;
- inverse bind matrices;
- animation sampling;
- quaternion interpolation;
- loop endpoint handling;
- embedded image extraction;
- deterministic output.

Add runtime/host C++ tests for:

- animation time accumulator;
- loop wrap;
- pause/reset;
- pose decode;
- quantization extremes;
- malformed animation descriptor;
- caller-capacity failures;
- compatibility with static `sat_model_asset_t` rendering.

No test should require copyrighted acceptance assets unless licensing and repository policy explicitly permit it. Use tiny programmatically generated fixtures for unit tests.

---

# Phase 27 — Automated tests: simplification

Add tests that assert behavior, not just successful execution.

Required cases:

- simple grid reaches requested reduction;
- UV seam is not crossed in conservative/balanced mode;
- material boundary is preserved;
- an articulated two-bone test preserves the bending region better with animation-aware mode than bind-pose-only mode;
- silhouette-weighted mode preserves a protruding feature better than geometry-only mode;
- repeated runs are deterministic;
- target-search returns a larger mesh when the requested target fails quality gates;
- simplified GLB reparses correctly;
- no out-of-range indices;
- no invalid joint indices/weights after compaction;
- report fields are stable.

Add a regression fixture specifically designed so naïve simplification destroys a knee/elbow-like bend. The animation-aware path must demonstrably score better under the tool's own objective metrics.

---

# Phase 28 — Automated tests: Saturn resource gates

Add deterministic tests for the Saturn profile:

- face/command budget calculation;
- reserved command headroom;
- texture VRAM estimate;
- palette bytes;
- generated pose-stream bytes;
- hard failure when no candidate can satisfy quality and hardware limits simultaneously.

The tool must be able to say:

```text
FAIL: no model candidate satisfies the selected Saturn profile
best quality-valid candidate: 412 triangles
available model face budget: 340
```

rather than silently forcing 340 and damaging the model.

---

# Phase 29 — Acceptance-asset import

Once the supplied GLB is present and licensing permits its use, run the complete pipeline on:

```text
examples/basic_3d_animation/assets/male_basic_walk_30_frames_loop.glb
```

Re-verify and report source statistics.

Generate a sequence of candidate simplifications, conceptually around:

```text
1200
900
700
500
400
350
300
250
200
150
```

The implementation may use a smarter search rather than literally producing each number.

Select the smallest candidate that satisfies:

1. active quality preset;
2. UV/material integrity;
3. animation quality;
4. silhouette quality;
5. VDP1 command budget with headroom;
6. texture VRAM budget;
7. runtime pose memory budget.

Do not declare a fixed target such as 280 correct before metrics are known.

Save the report under the build/generated output for inspection.

---

# Phase 30 — Visual acceptance in emulator

Build:

```text
make EXAMPLE=basic_3d_animation
```

Expected outputs include the normal ELF/BIN/ISO/CUE artifacts for the repository build flow.

Run through the existing modified Ymir harness workflow.

Acceptance requires:

- model appears correctly oriented;
- texture is recognizable and not randomly mirrored;
- no obvious atlas bleeding;
- walk animation loops without a visible catastrophic pop;
- knees/elbows/shoulders do not collapse into obviously broken topology;
- orbit works continuously;
- pitch works;
- zoom works;
- pause/resume works;
- reset works;
- texture remains stable while model animates;
- no per-frame texture upload;
- no VDP1 list overflow;
- no memory corruption;
- no static constructor issue;
- no heap dependency.

If emulator framebuffer capture/probe infrastructure exists, add an automated probe for multiple animation frames and camera positions. Do not claim screenshot-only manual inspection as the sole regression test when a deterministic assertion is practical.

---

# Phase 31 — Visual comparison artifacts

The host simplification tool should make quality inspection easy.

When requested, emit:

```text
original statistics
simplified statistics
JSON quality report
simplified GLB preview
optional silhouette comparison images
optional per-pose worst-error diagnostics
```

A useful diagnostic mode should identify which animation time and source region produced maximum geometric error.

Do not make preview artifacts part of the shipped Saturn binary.

---

# Phase 32 — Documentation

Update the project documentation with an animated-model pipeline section.

Show the architecture:

```text
animated GLB
    |
    | simplify_model.py / import_model.py
    v
animation-aware QEM simplification
    |
    +--> simplified GLB preview/report
    |
    v
host skeletal skinning
    |
    v
baked pose stream
    |
    +--> static indices
    +--> baked/dedup textures
    v
generated C/H
    |
    v
LibSaturn runtime
    |
    v
VDP1 animated textured model
```

Document explicitly:

- why source skeletal animation is baked to vertex animation for the first Saturn runtime;
- why simplification is animation-aware;
- why UV seams/material boundaries are protected;
- how quality gates override a too-aggressive requested triangle count;
- how to use `tools/simplify_model.py`;
- how to use `tools/import_model.py`;
- how to build `basic_3d_animation`;
- how to control the example;
- current limitations.

---

# Phase 33 — Current limitations to document honestly

The first implementation may intentionally not support:

- morph targets;
- multiple UV sets;
- tangent-space normal maps;
- PBR shading on Saturn;
- runtime skeletal skinning;
- CUBICSPLINE animation until implemented correctly;
- arbitrary glTF primitive modes;
- multiple skins attached to one primitive if not required initially;
- GPU-style perspective-correct UV interpolation beyond what VDP1 distorted sprites provide.

Reject unsupported inputs clearly instead of silently rendering them incorrectly.

---

# Phase 34 — Final quality gates

Before declaring the plan complete:

1. Run all existing host tests.
2. Run all new GLB/importer tests.
3. Run all new simplifier tests.
4. Run all static textured-model tests.
5. Build all existing examples where the environment supports them.
6. Build `basic_3d_texture` if present to prove the animated work did not regress static textured meshes.
7. Build `basic_3d_animation`.
8. Run the end-to-end importer twice and verify deterministic generated outputs/reports.
9. Run the standalone simplifier twice and verify deterministic semantic output.
10. Verify no heap dependency was introduced into the Saturn runtime.
11. Verify no static constructors were introduced.
12. Verify texture upload happens only at startup for the acceptance example.
13. Verify source UVs, joints and weights are absent from the final Saturn runtime asset unless explicitly required by a documented later feature.
14. Verify animation timing works under both NTSC-like and PAL-like update rates in host tests.
15. Verify the final simplification report satisfies the selected quality gates.
16. Verify VDP1 command and VRAM budgets have explicit headroom.
17. Verify license/attribution obligations are satisfied.
18. Run the ISO in the existing emulator flow and exercise all controls.
19. Record final source/simplified triangle counts, unique texture count, pose-stream bytes and VDP1 estimates.
20. Do not call the work complete if the model only renders statically.

---

# Suggested commit sequence

Keep commits coherent and green. A good sequence is:

```text
1. tools(gltf): add deterministic GLB parser and animation evaluator
2. tools(mesh): add pinned attribute-aware simplification backend
3. tools(mesh): add animation/silhouette quality metrics and auto target search
4. tools(mesh): add simplify_model CLI and simplified GLB output
5. model3d: add baked animation asset/runtime pose decoding
6. tools(model): compile animated GLB to Saturn generated assets
7. example: add interactive basic_3d_animation viewer
8. tests/docs: add acceptance gates, reports and documentation
```

Adjust if the current repository structure makes another split cleaner.

Every commit should pass the tests relevant to the code it introduces.

---

# Explicit non-goals / anti-shortcuts

Do **not** finish this plan by doing any of the following:

- rendering the original 1,500+ triangle model and hoping culling makes it fit;
- truncating the face list when VDP1 commands run out;
- simplifying only the bind pose;
- simplifying geometry while ignoring UV seams;
- reducing to an arbitrary fixed triangle count with no quality validation;
- baking 30 complete copies of the texture set;
- performing skeletal skinning for all source vertices every Saturn frame without measurement and justification;
- uploading textures every frame;
- parsing GLB on the Saturn;
- adding the walking model's joint names as special cases;
- implementing a one-off converter under the example directory;
- reporting success merely because the ISO boots;
- hiding resource overflows by dropping faces silently;
- silently violating the acceptance asset's license.

---

# Desired end-state

At the end, a developer should be able to take a conventional animated GLB and run something close to:

```bash
python tools/simplify_model.py \
  --input character.glb \
  --profile saturn-vdp1 \
  --animation-aware \
  --preserve-silhouette \
  --output build/character_preview.glb \
  --report build/character_preview.json

python tools/import_model.py \
  --input character.glb \
  --target saturn \
  --simplify auto \
  --quality balanced \
  --animation all \
  --out-prefix build/generated/character
```

and receive:

```text
source animated model
        |
        v
quality-constrained simplification
        |
        v
validated Saturn-compatible topology
        |
        +--> deduplicated face textures
        +--> compact baked pose animation
        v
generated C/H
        |
        v
interactive textured animated model on Sega Saturn
```

For `basic_3d_animation`, the final result is a bootable ISO/CUE where the walking character animates continuously and the player can inspect it by orbiting, pitching and zooming the camera with the Saturn controller.
