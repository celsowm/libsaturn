# LibSaturn Textured 3D Model Pipeline — Execution Plan

## Purpose

Implement a complete, generic, offline textured-3D asset pipeline for LibSaturn and finish `examples/basic_3d_texture` as an interactive Sega Saturn ISO.

The example is expected to use these source assets when they are present in the repository:

```text
examples/basic_3d_texture/assets/player00.png
examples/basic_3d_texture/assets/sonic.mtl
examples/basic_3d_texture/assets/sonic.obj
```

The end result must not be a Sonic-specific renderer. Sonic is the acceptance asset. The feature being built is a reusable LibSaturn pipeline for conventional UV-mapped models.

The final example must build to an ISO/CUE and be interactively controllable: the user must be able to orbit around the 3D model and zoom in/out with a Saturn pad.

---

## Harness execution contract

This document is intended to be executed by a coding harness from top to bottom.

Rules:

1. Work directly in `celsowm/libsaturn` on `main`.
2. Inspect the current code before implementing anything; this plan describes contracts, not blind patches.
3. Run the relevant tests after every coherent phase.
4. Do not stop at a textured cube or proof-of-concept.
5. Do not add Sonic-specific branches, names, hard-coded UVs, coordinates, materials, or texture logic to the library.
6. Do not parse OBJ, MTL, PNG, TGA, BMP, or JPEG on the Saturn.
7. Do not add heap allocation to the runtime.
8. Do not add static constructors.
9. Preserve source compatibility for existing examples and existing public APIs unless a change is demonstrably necessary.
10. Commit coherent, tested phases directly to `main`.
11. Do not automatically replace the supplied source assets. Treat them as input fixtures owned by the example.
12. If the three `basic_3d_texture/assets` files are not present when execution begins, stop before the acceptance-example phase and report the exact missing paths. Do not substitute another Sonic asset. Generic importer/runtime work may still proceed if it can be tested with repository-owned fixtures.

---

## Definition of done

This project is complete only when all of the following are true:

```text
OBJ + MTL + image texture
          |
          | host-side importer
          v
canonical VDP1-compatible baked face textures
          |
          +-- geometry in Saturn fixed point
          +-- face -> texture index mapping
          +-- shared indexed palette(s)
          +-- deduplicated texture payloads
          v
generated C/H model asset
          |
          | upload once at startup
          v
sat_texture_t runtime handles
          |
          v
sat_draw_mesh textured path
          |
          v
VDP1 distorted sprites
          |
          v
interactive basic_3d_texture ISO
```

The ISO must show the supplied 3D model textured correctly enough to recognize the source asset, allow user-controlled orbit and zoom, and remain stable while moving the camera.

---

# Phase 0 — Baseline and repository audit

Before modifying code, inspect at minimum:

```text
include/saturn/mesh3d.h
include/saturn/render3d.h
include/saturn/vdp1.h
include/saturn/input.h
src/core/mesh3d_api.cpp
src/core/mesh3d_logic.hpp
src/core/render3d_api.cpp
src/core/render3d_logic.hpp
src/core/vdp1_api.cpp
src/core/logic.hpp
src/hal/vdp1.cpp
src/hal/vdp1.hpp
tools/convert_indexed8.py
Makefile
build-example.ps1
run-example.ps1
tests/host/
harness/
```

Also inspect the VDP1 documentation already vendored under `docs/sega_saturn_hardware` before encoding hardware limits.

Record:

- current test result;
- current `examples-all` result where the environment supports it;
- current VDP1 command-list capacity;
- current texture-VRAM allocation strategy;
- actual legal VDP1 texture width/height limits from the hardware documentation and current HAL;
- current palette/CRAM ownership rules.

Do not guess hardware limits from field sizes in C code alone.

Baseline gate:

```text
make test
python -m unittest tests/test_asset_converter.py
```

Run any existing harness test suite that is normally used by this repository.

---

# Phase 1 — Keep source UVs offline

Do **not** add conventional per-vertex UV arrays to `sat_mesh_t` merely to carry OBJ data into the runtime.

VDP1 does not consume arbitrary modern UV coordinates per vertex. Source UVs are importer input, not required runtime state.

The intended separation is:

```text
source model
  vertices + UVs + materials
            |
            | host tool
            v
compiled Saturn model
  vertices + quad indices + face texture ids + baked textures
            |
            | runtime
            v
VDP1 commands
```

The generated asset must therefore contain no source-format parser state and should normally contain no source UV table.

---

# Phase 2 — Generic textured-mesh runtime

Extend the existing mesh draw path so each face may optionally be textured.

The current untextured behavior must remain valid.

Use a compact face-to-texture index mapping rather than storing one texture pointer per face.

A suitable public shape is conceptually similar to:

```c
#define SAT_MESH_TEXTURE_NONE 0xFFFFu

typedef struct sat_mesh_draw {
    const sat_mat4_t* view_proj;
    sat_vec3_t eye;

    uint16_t color;
    const uint16_t* face_colors;

    const sat_texture_t* textures;
    uint16_t texture_count;
    const uint16_t* face_texture_indices;

    sat_fx16_t ambient;
    uint16_t flags;

    uint8_t* order;
    uint32_t* depth;
} sat_mesh_draw_t;
```

Exact field naming may change after code inspection, but preserve these semantics.

Rendering contract:

- `face_texture_indices == NULL`: current polygon path, unchanged.
- `face_texture_indices[face] == SAT_MESH_TEXTURE_NONE`: polygon path for that face.
- valid texture index: use the existing projected textured-quad/distorted-sprite path.
- invalid texture index: return `SAT_ERR_INVALID_ARG` rather than reading past arrays.
- culling and sorting must be shared with the existing implementation, not forked into a second renderer.
- triangles represented as degenerate quads must remain supported.

Textured faces do not need dynamic per-face RGB lighting in the first implementation. If current VDP1 capabilities do not provide the same modulation semantics, document that `SAT_MESH_SHADE` applies only to polygon faces rather than implementing fake behavior.

Add host-testable logic so texture selection and face draw mode can be tested without real VDP1 hardware.

Required tests:

- legacy untextured mesh behavior unchanged;
- mixed textured and untextured faces;
- valid texture lookup;
- invalid texture lookup;
- backface culling parity;
- sorting parity;
- degenerate triangle face support;
- command-capacity propagation.

---

# Phase 3 — Separate palette upload from texture upload

The current high-level indexed8 upload path may upload a palette together with every texture. That is wasteful for a model whose many baked face textures share one palette.

Refactor the VDP1 API cleanly so the runtime can:

1. upload a palette once;
2. upload many indexed8 pixel surfaces that reference that palette;
3. still preserve the existing `sat_tex_upload_indexed8()` convenience API unchanged for current users.

Do not expose raw VRAM pointer arithmetic to game code.

The public API may gain helpers such as:

```c
sat_result_t sat_palette_upload_indexed8(...);
sat_result_t sat_tex_upload_indexed8_pixels(...);
```

Use names consistent with the existing library after inspection.

Required guarantees:

- width validation remains centralized;
- texture VRAM allocation remains owned by the VDP1 layer;
- VRAM exhaustion returns `SAT_ERR_CAPACITY`;
- palette bank validation remains centralized;
- no duplicate palette upload for every model face.

---

# Phase 4 — Compiled model asset format

Add a compact immutable generated-model format suitable for C/H output.

Prefer a new public header dedicated to compiled model assets rather than bloating unrelated structures, for example:

```text
include/saturn/model3d.h
```

Conceptual structures:

```c
typedef struct sat_model_texture_asset {
    const uint8_t* pixels;
    uint16_t width;
    uint16_t height;
    uint16_t palette_index;
    uint16_t flags;
    uint32_t pixel_count;
} sat_model_texture_asset_t;

typedef struct sat_model_asset {
    const sat_vec3_t* vertices;
    uint16_t vertex_count;

    const uint16_t* indices;
    uint16_t face_count;

    const uint16_t* face_texture_indices;

    const sat_model_texture_asset_t* textures;
    uint16_t texture_count;

    const uint16_t* palettes_rgb555;
    uint16_t palette_count;
} sat_model_asset_t;
```

The exact representation can improve on this, but requirements are:

- immutable generated data;
- one top-level descriptor per model;
- no heap;
- no runtime source parsing;
- geometry already converted to LibSaturn fixed point;
- face data already in LibSaturn A/B/C/D ordering;
- unique baked textures only;
- face-to-texture indices reference the deduplicated set;
- palette data is shared rather than copied into every texture structure.

Add helper(s) that bind or copy generated geometry into caller-owned `sat_mesh_t` storage when required and upload unique model textures into caller-supplied `sat_texture_t[]` storage.

Prefer zero-copy geometry when safe. If the existing mutable `sat_mesh_t` contract prevents const generated arrays from being referenced safely, do not cast away const silently. Either add a clean immutable mesh view for generated models or explicitly copy into caller-provided mutable storage.

---

# Phase 5 — Host-side OBJ/MTL importer

Create a generic tool, preferably:

```text
tools/import_model.py
```

Initial source support:

- Wavefront OBJ;
- MTL;
- PNG, TGA, BMP, JPEG via Pillow.

Required OBJ features:

- `v`;
- `vt`;
- `f`;
- `mtllib`;
- `usemtl`;
- positive indices;
- negative indices;
- triangles;
- quads.

Required MTL feature:

- `map_Kd`.

Normals from `vn` may be parsed/ignored initially because LibSaturn derives face normals from geometry.

Faces with more than four vertices should be triangulated deterministically with a fan unless a correctness issue is found; otherwise reject them with a precise diagnostic.

Importer CLI must support at least:

```text
--input
--out-prefix
--symbol
--scale
--flip-x
--flip-y
--flip-z
--reverse-winding
--palette-index
--max-texture-width
--max-texture-height
```

Optional quality switches may be added, such as a sampling mode or a global texture-quality scale.

Do not hard-code Sonic dimensions, orientation, filenames, material names, or scale.

---

# Phase 6 — Face canonicalization and winding

LibSaturn's face convention is authoritative:

```text
A = top-left
B = top-right
C = bottom-right
D = bottom-left
normal = cross(D - A, B - A)
```

The importer must deliberately convert OBJ winding into that convention.

Do not guess triangle duplication ordering.

Create unit tests using a known CCW triangle and known quad, then prove that the generated degenerate quad produces the expected outward normal under LibSaturn's current normal implementation.

A triangle must become a degenerate quad whose geometry and UV duplication are consistent with one another.

The importer must also handle OBJ's UV vertical convention correctly when sampling image pixels.

Required tests:

- CCW triangle orientation;
- reversed triangle orientation;
- CCW quad orientation;
- `--reverse-winding`;
- axis flips;
- negative face indices;
- UV V-axis conversion.

---

# Phase 7 — Canonical UV baking

This is the core of the feature.

Do not implement only a UV bounding-box crop.

For every source face, produce a new canonical rectangular texture whose four logical corners correspond to the VDP1 face corners:

```text
canonical texture          VDP1 face

(0,0) ----- (1,0)          A ----- B
  |           |             \       \
  |           |              \       \
(0,1) ----- (1,1)          D ----- C
```

For a quad with source UVs `uvA`, `uvB`, `uvC`, `uvD`, bake by sampling the source texture through a deterministic parameterization. A bilinear parameterization is an acceptable first implementation:

```text
top    = lerp(uvA, uvB, s)
bottom = lerp(uvD, uvC, s)
uv     = lerp(top, bottom, t)
```

where `s,t` span the output canonical texture.

For triangles represented as degenerate quads, the duplicated geometry corner must have the corresponding duplicated UV. The canonical rectangle is then collapsed by VDP1 together with the degenerate geometric quad.

Do not naively crop a rectangular area of an atlas for a triangle and allow unrelated atlas texels to fill the unused half.

Sampling must be deterministic. Choose nearest-neighbor or bilinear filtering explicitly and test it. If bilinear is used, clamp sampling to the intended source image bounds and avoid atlas bleeding at material boundaries.

---

# Phase 8 — Baked resolution and VDP1 legality

Estimate the desired baked texture dimensions from the source face's UV footprint in source texels.

A reasonable estimate is based on the maximum opposite-edge lengths:

```text
width  ~= max(|A-B|, |D-C|)
height ~= max(|A-D|, |B-C|)
```

measured in source-image pixel space.

Then conform to real VDP1 requirements discovered in Phase 0.

Requirements:

- dimensions never become zero;
- width is legal for VDP1, including required multiple-of-8 alignment;
- do not merely append unused transparent columns to reach an aligned width, because VDP1 maps the entire stored width over the polygon;
- instead, resample the same UV domain across the legal aligned width;
- enforce hardware maximum width and height;
- respect CLI quality limits;
- fail with a clear face/material diagnostic if a source face cannot be represented safely.

The importer should support reducing texture resolution globally or by maximum dimension so real models can fit VDP1 VRAM.

---

# Phase 9 — Palette strategy and transparency

Default model import should produce a shared indexed8 palette for all baked textures when feasible.

Preferred flow:

```text
bake all canonical faces in RGBA
             |
             v
collect visible RGB colors
             |
             v
build shared <= 256 entry palette
             |
             v
map every baked face to indexed8
```

Transparency contract:

- if alpha/transparency exists, reserve palette index 0 for transparent pixels;
- quantize opaque colors into the remaining entries;
- if the entire model is opaque, generated texture metadata may use `SAT_SPRITE_FLAG_OPAQUE`;
- never accidentally reinterpret an ordinary black/dark color as transparent.

Keep the format extensible to multiple palettes later, but keep the first implementation simple unless the acceptance asset demonstrably requires more than one palette.

Palette generation must be deterministic.

---

# Phase 10 — Deduplication

Deduplicate **after** canonical face baking and palette mapping.

Do not assume that equal source UV bounding boxes imply equal final textures: orientation may differ.

A deduplication key must include everything that changes interpretation, at minimum:

```text
width
height
indexed pixel bytes
palette identity
relevant sprite flags
```

Generate:

```text
face_texture_indices[face_count]
```

and one payload for each unique final texture.

The importer must print statistics:

```text
source vertices
source UVs
source faces
source triangles
source quads
materials
baked faces before dedup
unique textures after dedup
palette count
indexed texture bytes
palette bytes
estimated VDP1 VRAM usage
largest baked texture
```

Do not encode any expected Sonic texture count as an algorithmic constant.

---

# Phase 11 — Deterministic generated C/H output

The importer must emit generated files compilable directly by the SH-2 build.

Example output:

```text
build/generated/basic_3d_texture/sonic_model.h
build/generated/basic_3d_texture/sonic_model.c
```

The public generated header should expose one top-level descriptor, e.g.:

```c
extern const sat_model_asset_t sonic_model_asset;
```

Application code must not know one generated symbol per face.

Requirements:

- byte-identical output on repeated runs with identical input/options;
- stable face order;
- stable texture dedup order;
- no generated runtime floating-point parsing;
- geometry emitted as LibSaturn fixed-point constants;
- generated files contain clear size/count metadata;
- generated symbols sanitized deterministically.

Add a test that imports the same fixture twice and byte-compares both C/H outputs.

---

# Phase 12 — Build system integration

Integrate model generation into the existing per-example build mechanism.

Do not hard-code Sonic into the root Makefile.

Prefer `examples/basic_3d_texture/Makefile.inc` declaring its own model generation dependencies/options.

The generic root build may gain reusable variables/rules if they benefit other examples.

Keep the existing `convert_indexed8.py` 2D path working.

If palette/image utilities are shared between `convert_indexed8.py` and `import_model.py`, refactor them into a small Python module rather than turning the old 2D converter into a large unrelated script.

The expected user-facing build must remain simple:

```bash
make EXAMPLE=basic_3d_texture
```

Windows launcher acceptance should work with the repository's normal flow, for example:

```powershell
.\run-example.ps1 basic_3d_texture -Emulator mednafen -BiosProfile auto
```

Expected artifacts:

```text
build/basic_3d_texture.elf
build/basic_3d_texture.bin
build/basic_3d_texture.iso
build/basic_3d_texture.cue
```

---

# Phase 13 — Repository-owned importer fixtures

Importer unit tests must not depend exclusively on Sonic assets.

Add tiny repository-owned fixtures under a test-data location, containing at minimum:

- one triangle;
- one quad;
- a tiny atlas with asymmetric markings;
- one UV mapping rotated 90 degrees;
- one flipped mapping;
- one non-axis-aligned mapping;
- two faces that become identical after canonical baking, proving deduplication;
- a transparent texel case;
- a negative-index OBJ case.

These fixtures exist to make importer behavior testable and legally distributable independent of the `basic_3d_texture` acceptance asset.

---

# Phase 14 — Finish `examples/basic_3d_texture`

The example already has/will have:

```text
examples/basic_3d_texture/assets/player00.png
examples/basic_3d_texture/assets/sonic.mtl
examples/basic_3d_texture/assets/sonic.obj
```

Use the generic importer to generate the compiled model. Do not hand-edit generated geometry or UV output for this asset.

The example should be a small interactive model viewer rather than a static screenshot.

## Required controls

Use the existing digital pad API.

Default mapping:

```text
LEFT / RIGHT   orbit yaw around the model
UP / DOWN      orbit pitch, clamped to a safe range
L              zoom out
R              zoom in
A              toggle automatic slow orbit
B              reset yaw, pitch and zoom
START          toggle a small debug/help HUD if practical
```

If L/R proves awkward in emulator defaults, A/C may additionally act as zoom controls, but keep L/R support.

The controls must use held state for smooth continuous movement, not only one-frame presses.

## Camera behavior

Prefer orbiting the camera around an immutable model rather than repeatedly transforming generated vertices in place.

Reasons:

- avoids cumulative fixed-point drift;
- keeps generated geometry immutable;
- exercises the real culling/sorting path as the view changes;
- avoids requiring a model-matrix feature solely for this example.

Maintain:

```text
yaw
pitch
camera distance
target/model center
```

Compute camera position from these values using existing fixed-point math helpers.

Clamp:

- pitch to avoid flipping over the poles unless explicitly supported;
- zoom to keep the camera outside the model and inside stable projection ranges.

The model should be centered automatically from imported bounds rather than from a Sonic-specific magic coordinate. The importer should emit bounds/center metadata, or the example should derive them from the generic model descriptor during initialization.

## Visual presentation

Keep the example deliberately simple:

- clean background/backdrop;
- model centered on screen;
- optional small text showing controls, unique texture count and camera distance;
- no unrelated gameplay systems.

The purpose is to demonstrate textured 3D and make inspection easy.

## Per-frame behavior

Do not upload textures every frame.

Startup:

```text
initialize Saturn
load/upload model palette(s)
upload unique baked textures once
prepare mesh/model bindings
compute model center/bounds
```

Frame:

```text
poll pad
update yaw/pitch/zoom
build camera/view-projection
begin frame
draw textured model with culling + painter sorting
draw optional HUD
submit/wait vblank
```

Use backface culling and painter sorting.

Do not rely on a depth buffer; Saturn VDP1 does not provide the modern depth-buffer path that such a viewer would normally use.

---

# Phase 15 — Sonic acceptance measurements

When the supplied asset exists, inspect it and record actual counts rather than trusting previous manual estimates.

The expected rough characteristics from prior inspection are approximately:

```text
texture: player00.png, 256x256
OBJ vertices: 285
OBJ UV coordinates: 106
faces: 185
quads: 101
triangles: 84
materials: 1
```

These are acceptance expectations, not parser constants.

The harness must print/report the actual imported counts and flag a large mismatch for investigation.

Also report:

- canonical baked textures before dedup;
- unique textures after dedup;
- total indexed texture bytes;
- palette bytes;
- estimated VDP1 VRAM usage including alignment;
- maximum per-face baked dimensions.

The model has substantial UV reuse; the implementation must demonstrate that it does not blindly allocate one independent full atlas per face.

If VRAM pressure is too high, solve it generically through texture baking resolution controls/deduplication, not by deleting arbitrary Sonic faces or adding hard-coded exceptions.

---

# Phase 16 — VRAM and command-budget gates

The importer/runtime must make resource use visible.

Before uploading an imported model, ensure its unique texture payload can fit the current VDP1 texture allocation region together with the command area and any other VDP1 resources used by the example.

Add clean capacity checks rather than allowing cursor wraparound or memory corruption.

The example has about 185 source faces, but with backface culling only a subset should normally be submitted. Nevertheless, never assume a fixed visible percentage.

Ensure the command buffer can safely hold:

```text
setup commands
+ worst-case visible model face commands
+ HUD/other VDP1 commands used by the example
+ END command
```

If the existing default command capacity is insufficient, size the example's command storage appropriately within the library's supported contract. Do not silently truncate model faces.

---

# Phase 17 — Automated tests

## Python importer tests

Required:

- OBJ parser;
- MTL resolution;
- material switching;
- image loading;
- negative OBJ indices;
- triangle canonicalization;
- quad canonicalization;
- winding;
- UV V-axis conversion;
- rotated UV mapping;
- flipped UV mapping;
- non-axis-aligned UV mapping;
- canonical bake sampling;
- VDP1 width alignment by resampling, not padding;
- hardware max-dimension rejection;
- shared palette creation;
- transparency index reservation;
- fully opaque flagging;
- deduplication;
- fixed-point geometry emission;
- deterministic C/H generation;
- malformed OBJ diagnostic;
- missing MTL diagnostic;
- missing texture diagnostic.

## C/C++ host tests

Required:

- compiled-model descriptor validation;
- model texture upload validation using stubs where necessary;
- mixed textured/polygon mesh faces;
- face texture lookup;
- culling parity;
- sorting parity;
- invalid face texture index;
- texture capacity errors;
- triangle degenerate quad path;
- old mesh behavior remains unchanged.

## Harness tests

If the repository harness can inspect generated VDP1 command/state output, add a fixture for `basic_3d_texture` or a small textured-model fixture proving:

- distorted sprite commands are emitted for textured faces;
- command texture addresses are valid;
- no texture upload occurs during normal per-frame drawing;
- changing camera orientation changes projected corners without changing model texture data.

---

# Phase 18 — Emulator acceptance

Build and run `basic_3d_texture` through the normal emulator path.

Acceptance checklist:

- ISO boots;
- model is visible;
- texture orientation is recognizably correct;
- no obvious whole-atlas-on-every-face failure;
- no major atlas bleeding;
- triangle faces appear as triangles rather than rectangular garbage;
- quad textures remain attached while orbiting;
- LEFT/RIGHT orbit continuously;
- UP/DOWN pitch continuously;
- L/R zoom works continuously;
- B reset works;
- optional auto-orbit works if implemented;
- no crash while rotating continuously for at least several minutes;
- no texture re-upload each frame;
- painter sorting remains stable enough for the model;
- no command buffer overflow;
- no VDP1 VRAM corruption;
- no static constructors;
- no heap allocation.

If framebuffer capture tooling already available in the repository can be automated, save a deterministic acceptance capture at a known camera angle and add a regression check that is robust to emulator output details.

Do not use a manually viewed screenshot as the only test.

---

# Phase 19 — Documentation

Update the main documentation after implementation with a concise 3D model import section.

Show the pipeline:

```text
OBJ + MTL + PNG
      |
      | tools/import_model.py
      v
generated C/H model
      |
      | upload once
      v
LibSaturn textured mesh
      |
      v
VDP1 distorted sprites
```

Document:

- why arbitrary source UVs are baked offline;
- VDP1 texture constraints;
- texture deduplication;
- palette sharing;
- triangles as degenerate quads;
- no runtime OBJ/PNG parsing;
- how to build `basic_3d_texture`;
- viewer controls.

Keep this execution plan in `docs/` as architectural history unless implementation discoveries make part of it materially incorrect; if so, update this file with the final chosen design rather than leaving contradictions.

---

# Suggested commit sequence

Use small coherent commits, each green for its own scope:

```text
1. feat(mesh3d): add generic textured face rendering
2. feat(vdp1): support shared palette texture uploads
3. feat(model3d): add compiled model asset runtime
4. feat(tools): add OBJ/MTL textured model importer
5. test(model3d): add importer and runtime coverage
6. build(model3d): integrate generated model assets
7. feat(example): make basic_3d_texture interactive
8. docs: document textured 3D asset pipeline
```

Commit names may vary, but do not combine the whole project into one opaque commit.

---

# Final quality gate

Before declaring the plan complete, run all applicable commands and report results.

At minimum:

```text
make test
python -m unittest tests/test_asset_converter.py
<new importer tests>
make EXAMPLE=basic_3d_texture
make examples-all
<existing harness tests>
```

Then verify:

1. generated importer output is byte-identical across two runs;
2. the three supplied model assets are imported through the generic path;
3. the model loads without runtime source parsing;
4. unique textures are uploaded once;
5. the final ISO/CUE is produced;
6. the example is interactively orbitable and zoomable;
7. existing examples still compile;
8. existing host tests still pass;
9. no heap dependency was introduced;
10. no static constructors were introduced;
11. no Sonic-specific logic exists in reusable LibSaturn source files;
12. importer statistics and final VRAM/command usage are included in the completion report.

Do not declare success because the project merely compiles. The final gate is a working, controllable, textured 3D model viewer on the Saturn execution path.