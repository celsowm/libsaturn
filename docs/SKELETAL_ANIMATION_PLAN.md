# LibSaturn — Real-Time Skeletal Animation, CPU Skinning, Advanced Character Rendering and Fighting-Game Examples

(Original implementation brief saved on 2026-10-11. Status on 2026-10-13: library, tooling, three examples and emulator gates implemented. Physical hardware acceptance and higher display-rate optimization remain open. See [the engineering/readiness report](FIGHTING_GAME_READINESS.md).)

## Role and Mission

You are a senior Sega Saturn engine developer specializing in C/C++, SH-2 optimization, VDP1/VDP2 rendering, fixed-point mathematics, skeletal animation, graphics pipelines, asset processing and dual-processor systems.

Your task is to **implement, integrate, optimize, test, document and commit a complete real-time skeletal character animation subsystem into `celsowm/libsaturn`**, including new examples that demonstrate its capabilities on actual Sega Saturn hardware.

**This is an implementation task, not merely an architectural analysis or planning exercise.**

The ultimate goal is to make LibSaturn suitable for sophisticated polygonal fighting games comparable in technical requirements to Tekken 3 and Virtua Fighter 2, while keeping every new API generic enough for platformers, RPGs, action games and other genres.

Reference repositories:

- Primary repository: `https://github.com/celsowm/libsaturn`
- Technical reference: `https://github.com/n0madic/Tekken3Remake`
- Additional architectural context: `https://github.com/celsowm/vf2-decomp` (if accessible)

Do not directly port Tekken 3 or Virtua Fighter 2 into LibSaturn.

Do not copy proprietary game assets or code whose license is incompatible with the intended use.

The Tekken3Remake repository is useful because its architecture demonstrates demanding real-world requirements: hierarchical skeletons, multiple joint influences, per-vertex local positions, animation blending, root motion, character attachments, hit reactions and deterministic gameplay.

---

# 1. Non-Negotiable Engineering Requirements

## 1.1 Inspect the existing codebase first

Before implementing anything:

1. Inspect the latest `main` branch.
2. Read `AGENTS.md` and the applicable repository instructions.
3. Inspect current public headers, implementations, host tests, tools, examples and build scripts.
4. Read the relevant architectural and performance documentation.
5. Identify which requested capabilities already exist.
6. Extend or refactor existing functionality instead of duplicating it.
7. Run the existing tests and establish a baseline.

Pay particular attention to:

```text
include/saturn/anim3d.h
include/saturn/mesh3d.h
include/saturn/mesh3d_draw.h
include/saturn/model3d.h
include/saturn/render3d.h
include/saturn/scene.h
include/saturn/scene3d_faces.h
include/saturn/transform3d.h
include/saturn/parallel.h
include/saturn/asset.h
include/saturn/texture.h
include/saturn/resource_plan.h
include/saturn/vdp1.h
include/saturn/dma.h

tools/import_model.py
tools/model_pipeline/

docs/ANIMATED_GLTF_MODEL_PIPELINE_PLAN.md
docs/PARALLEL_RUNTIME_ARCHITECTURE.md
docs/RESOURCE_PERFORMANCE_LEDGER.md
docs/HARDWARE_ACCEPTANCE_CHECKLIST.md
```

The actual current repository is authoritative. Paths and APIs may have changed.

The existing library already provides baked vertex animations, fixed-point mathematics, transform hierarchies, a global 3D scene painter, model assets, asynchronous work execution, CD asset infrastructure and VDP1 command accounting.

**Do not implement competing versions of these systems.**

## 1.2 Architectural principles

Maintain the following properties:

- Bare-metal Sega Saturn compatibility.
- Stock Saturn as the mandatory baseline.
- No required expansion RAM cartridge.
- No required external runtime or Godot dependency.
- Deterministic fixed-point runtime calculations.
- No per-frame heap allocation.
- Caller-owned, explicitly bounded memory.
- No STL containers requiring a hosted runtime in the Saturn hot path.
- No hidden global mutable animation state.
- No unnecessary floating-point operations on SH-2.
- No runtime glTF, JSON or PNG parsing.
- No per-frame texture reupload for ordinary skeletal animation.
- No duplicate renderer, model, scene or texture ownership systems.
- No backward-compatibility shims merely to preserve obsolete internal APIs.
- Clear ownership and lifetime contracts.
- Useful, measurable performance counters.

Preserve existing functionality and migrate internal consumers when a breaking refactor is justified.

Keep the public C ABI compatible with the existing C-facing, C++-implemented LibSaturn architecture.

Use C++ internally where appropriate, but expose clean C interfaces.

Keep the implementation reusable and DRY.

---

# 2. Phase One — Generic Skeletal Animation Runtime

Implement a complete skeletal animation subsystem.

Proposed public API:

```text
include/saturn/skeleton3d.h
```

This name is a starting suggestion. Adapt naming to the actual established API conventions.

## 2.1 Skeleton representation

Support:

- Hierarchical bones/joints.
- Parent-child relationships.
- Local and world-space transforms.
- Rest pose.
- Bind pose and inverse bind transforms where applicable.
- Parent-relative joint offsets.
- Fixed-point translation and rotation.
- Compact pose data.
- Dynamic bone matrices.
- Multiple model instances sharing immutable skeleton assets.
- Selective bone and attachment updates.
- Optional animation-driven root motion.

The API must support at least 32 joints per skeleton without a hard-coded Tekken-specific joint layout.

Larger configurations may be supported using caller-provided buffers.

Reuse the existing `transform3d` infrastructure rather than creating a competing hierarchy evaluator.

## 2.2 Pose evaluation

Implement:

```c
/* Illustrative API only. */
sat_result_t sat_skeleton_evaluate(
    sat_skeleton_instance_t* instance,
    const sat_skeleton_pose_t* pose
);
```

The evaluator must:

1. Validate the hierarchy and input buffers.
2. Evaluate parent transforms before children.
3. Compute world-space joint matrices.
4. Avoid recursive evaluation.
5. Use fixed-point arithmetic.
6. Allow multiple independent character instances.
7. Avoid allocations and redundant matrix work.
8. Offer deterministic behavior in host and Saturn builds.

Support explicitly defined rotation representation and conversion conventions.

Avoid Euler-angle interpolation artifacts where practical.

Do not introduce floating-point quaternion evaluation into the SH-2 hot path without benchmarks demonstrating its acceptability.

## 2.3 Animation sampling

Support:

- Animation clips.
- Frame-based sampling.
- Variable playback speed.
- Looping and non-looping playback.
- Pausing and resuming.
- Transition blending.
- Crossfading.
- Root-motion extraction.
- Animation events.
- Independent playback for multiple characters.
- Pose evaluation at explicit fixed simulation frames.

A fighting game often changes animations before the previous one finishes.

The implementation must therefore support transitions between arbitrary poses without assuming every animation is a simple, isolated loop.

Preserve the existing baked vertex-animation API for cases where it is more efficient.

The new system should complement existing animation capabilities.

---

# 3. Phase Two — CPU Skeletal Skinning

Implement:

```text
include/saturn/skin3d.h
```

The purpose is to deform polygon meshes using dynamically evaluated skeleton poses.

## 3.1 Skinning requirements

Support:

- One joint influence per vertex.
- Two joint influences.
- Three joint influences.
- Configurable weights.
- Fixed-point joint transformations.
- Compact vertex and influence formats.
- Immutable mesh topology.
- Mutable per-instance vertex buffers.
- Optional normal transformation.
- Reusable bone matrices.
- Shared immutable model assets.

**Important: support multi-anchor skinning, not only conventional linear blend skinning.**

Some older fighting-game models blend distinct part-local positions transformed through different joints.

The generic representation must be capable of calculating:

```text
P = w0 * (M0 * P0)
  + w1 * (M1 * P1)
  + w2 * (M2 * P2)
```

Here, `P0`, `P1` and `P2` may be different local positions.

Do not assume every influence shares one bind-space vertex position.

This is necessary for faithfully representing certain PlayStation and arcade character meshes.

## 3.2 Memory-efficient representation

Avoid storing three complete positions per vertex if the vertex uses only one influence.

Investigate:

- Compact single-influence records.
- Separate multi-influence seam records.
- Small integer joint indices.
- Fixed-point or quantized weights.
- Shared immutable source geometry.
- Precomputed inverse bind transforms.
- Fixed-size transformation buffers.
- Reusable scratch memory.

Choose a compact representation based on measured memory usage and decoding cost.

Document its binary format.

## 3.3 Efficient deformation

Provide an interface conceptually equivalent to:

```c
sat_result_t sat_skin_deform(
    const sat_skin_asset_t* skin,
    const sat_skeleton_instance_t* skeleton,
    sat_vec3_t* output_vertices,
    uint32_t output_capacity
);
```

Required optimizations:

- Avoid repeated bone matrix calculations.
- Transform each unique source vertex only when needed.
- Minimize 64-bit arithmetic where possible without sacrificing correctness.
- Provide a fast path for single-influence vertices.
- Optimize exact half-weight blending.
- Avoid repeatedly transforming duplicated triangle corners.
- Keep memory accesses predictable.
- Support efficient SH-2 execution.
- Check all capacities before modifying output.

Implement and benchmark separate generic and optimized execution paths where justified.

Do not introduce platform-specific assembly until a working, tested C/C++ implementation exists and profiling shows a measurable benefit.

---

# 4. Phase Three — Integrate Skinned Models with the Existing 3D Scene

This is a critical integration step.

The animation system must not become an isolated subsystem with its own renderer.

It must use the canonical LibSaturn scene architecture.

## 4.1 Dynamic mesh integration

The existing renderer already supports:

- VDP1 polygon and textured sprite submission.
- Face sorting.
- Screen projection.
- Near clipping.
- Visibility testing.
- Gouraud shading.
- Scene-wide painter ordering.
- VDP1 command budgets.

Extend these capabilities to support continuously deformed meshes.

Required behavior:

1. Mesh indices and material assignments remain immutable.
2. Bone evaluation updates a character instance.
3. Skinning writes into caller-owned dynamic vertex buffers.
4. The scene consumes the resulting vertices.
5. The existing painter orders all visible faces.
6. VDP1 commands are generated through the canonical backend.

A game should not need to rebuild mesh topology or reallocate materials every frame.

Investigate whether existing instance submission can already consume mutable vertex buffers correctly.

If it can, reuse it.

Otherwise, add the minimum necessary generic API.

## 4.2 Inter-character depth ordering

Test situations involving:

- Two characters standing close together.
- One character passing in front of the other.
- Intersecting arms and legs.
- Grabs and throws.
- Characters crossing during a movement.
- Animated geometry crossing static arena geometry.

The renderer has no Z-buffer.

Maintain a single global painter order for interacting geometry.

Do not simply render Character A followed by Character B.

Document painter limitations explicitly.

For problematic intersecting surfaces, implement a bounded and reusable solution or an explicitly selected approximation.

Avoid pretending that sorting alone can provide exact hardware Z-buffer behavior.

## 4.3 Visibility and command budgeting

Implement efficient support for:

- Character-level visibility.
- Per-submesh visibility where useful.
- Backface culling.
- Camera-frustum rejection.
- Conservative animated bounds.
- Optional model LOD.
- Deterministic VDP1 command allocation.
- HUD command reservation.
- Clear capacity errors and diagnostics.

Do not silently discard important character polygons simply because the scene command budget was exceeded.

Prefer an explicit quality or LOD policy.

Keep animation independent of frame-rendering success.

---

# 5. Phase Four — Character Materials and Lighting

Improve reusable rendering facilities for textured, animated characters.

The objective is to approximate the visual presentation of sophisticated fifth-generation console fighting games within Saturn hardware constraints.

## 5.1 Lighting

Investigate and implement appropriate combinations of:

- Flat face shading.
- Gouraud shading.
- Vertex/normal-based lighting.
- Baked lighting.
- Palette-based shading variants.
- Directional lighting.
- Ambient lighting.
- Controlled highlights.
- Character-specific lighting parameters.

Do not assume that VDP1 indexed textures provide arbitrary modern per-pixel lighting.

Respect actual VDP1 color modes and hardware restrictions.

Prefer performance-conscious lighting schemes suitable for two animated fighters.

## 5.2 Character materials

Support:

- Multiple materials per character.
- Shared texture palettes.
- Material overrides.
- Per-submesh visibility.
- UV-preserving face conversion.
- Alpha cutout where supported.
- Explicit fallback policies for unsupported transparency.

Provide robust handling of textured triangle geometry converted to VDP1-compatible face commands.

Do not invent hardware texture interpolation capabilities.

## 5.3 Character shadows

Provide a generic lightweight character-ground shadow system.

It may use projected planar geometry, appropriately composited sprites or another hardware-supported technique.

Requirements:

- Follow the animated character.
- Remain aligned with the ground.
- Avoid significant command overhead.
- Support two simultaneous characters.
- Work with scene ordering and VDP2 backgrounds.
- Be optional and measurable.

---

# 6. Phase Five — Asset Pipeline Improvements

Extend the existing host-side importer and model pipeline.

Do not build an incompatible second importer.

## 6.1 Generic skeletal model import

Support skeletal glTF/GLB models containing:

- Mesh geometry.
- Bone hierarchies.
- Skin weights.
- Bind poses.
- Multiple animation clips.
- Textures and materials.
- Named joints.
- Multiple meshes or submeshes.

The host tools should:

1. Parse source model data.
2. Normalize coordinate systems and scale.
3. Validate skeleton hierarchies.
4. Quantize animation data.
5. Simplify geometry when requested.
6. Preserve skin weights.
7. Bake VDP1-compatible textures.
8. Generate compact binary assets.
9. Produce detailed memory and polygon reports.
10. Verify the encoded output against the source.

Keep the existing baked vertex animation workflow functioning.

Add a selectable skeletal runtime export path.

## 6.2 Support multi-anchor geometry

Provide a generic intermediate format for multiple joint-local positions per logical vertex.

A converter must be able to represent historical model formats without forcing everything into conventional single-bind-position glTF skinning.

The specialized parser for Tekken KMD and its game data does not belong in LibSaturn.

Only the generic model representation and runtime should be part of the library.

## 6.3 Animation compression

Investigate:

- Quantized translations.
- Compact rotations.
- Delta encoding.
- Constant-channel elimination.
- Keyframe reduction.
- Clip metadata.
- Per-animation loading.
- Fast random access for arbitrary animation frames.

Fighting games need short animation transitions and frequent nonsequential clip selection.

Therefore, do not choose a compression format that requires decoding an entire long sequence every time a move changes.

Measure compressed size, decoding cost and pose error.

---

# 7. Phase Six — Dual SH-2 Skeletal Workloads

Reuse the current LibSaturn parallel executor.

Do not create a second threading or task system.

## 7.1 Parallel tasks

Add appropriate task adapters for:

- Skeleton pose evaluation, when beneficial.
- Vertex skinning.
- Optional normal transformation.
- Character geometry preparation.

Use the existing queue, handles, ownership and cache-coherency mechanisms.

All shared buffers must follow the library's existing Master/Slave memory rules.

## 7.2 Execution policies

Support at least:

```text
MASTER_ONLY
SLAVE_ASSISTED
AUTO
```

These are conceptual modes; use existing conventions.

The Master SH-2 must remain fully functional without Slave acceleration.

A possible workload:

```text
Master SH-2:
    - Simulation and animation state selection
    - Camera and scene management
    - VDP1 submission
    - HUD and audio coordination

Slave SH-2:
    - Selected skeleton/skin jobs
    - Character vertex deformation
    - Geometry preparation where profitable
```

Actual partitioning must be benchmark-driven.

Avoid assigning tiny jobs to the Slave when synchronization costs exceed computation time.

## 7.3 Determinism and concurrency

Require:

- Same output vertices in serial and parallel paths.
- No buffer ownership races.
- Explicit synchronization.
- Proper SH-2 cache publication and invalidation.
- No use-after-release.
- No shared writable scratch between independent character jobs.
- Safe cancellation and fallback.
- No simultaneous Master/Slave writes to the same output span.

Test both single-character and dual-character workloads.

---

# 8. Phase Seven — Character Asset Streaming and Resource Budgets

Reuse the current asset manager, CD Block, cache, texture and resource planning subsystems.

Implement the remaining integration necessary for real character assets.

Support:

- Loading only the currently selected characters.
- Sharing immutable animation and model data.
- Loading character-specific textures and palettes.
- Loading animation clips as needed.
- Explicit prefetching.
- Resource release between scenes.
- Bounded CD-backed asset loading.
- Optional expansion RAM caching.

The game must remain capable of running on an unexpanded Sega Saturn.

Provide helpers to estimate:

```text
Skeleton bytes
Skinning source bytes
Dynamic vertex buffer bytes
Animation clip bytes
Material and texture VRAM bytes
Frame scratch bytes
Scene face buffer bytes
VDP1 command requirements
Optional parallel scratch bytes
```

Create validation tests for loading and unloading two large independent character assets.

Do not assume that expansion RAM can replace every form of native work RAM or VRAM.

---

# 9. Phase Eight — New Examples

Implement real, complete examples under `examples/`.

Each example must have:

- A dedicated source directory.
- Its own `README.md`.
- Explicit asset ownership and licensing.
- Integration with the existing build system.
- A bootable Saturn ISO.
- Controller support when appropriate.
- Runtime performance instrumentation.
- Host-side tests where applicable.
- Automated emulator validation when supported.

Use original procedural or permissively licensed assets that can legally be distributed.

Do not require users to provide Tekken 3 or Virtua Fighter files to run these examples.

## Example A — `skeletal_animation_3d`

### Purpose

Demonstrate a single textured 3D character using real-time skeletal animation.

### Requirements

Create a recognizably humanoid low-poly character with articulated:

- Head
- Neck
- Torso
- Shoulders
- Upper arms
- Forearms
- Hands
- Hips
- Thighs
- Lower legs
- Feet

Provide animation clips:

1. Idle breathing.
2. Walking.
3. Running.
4. Jumping.
5. Punching.
6. Kicking.

Implement smooth transitions and blended poses.

### Controls

Provide:

- Animation selection.
- Pause/resume.
- Animation speed adjustment.
- Camera orbit.
- Camera zoom.
- Toggle shading.
- Toggle skeleton visualization.
- Toggle skinning mode if multiple generic implementations are available.

The example must demonstrate a **genuinely deforming skinned mesh**, not merely rigid geometry assembled from independently animated blocks.

Include seam vertices influenced by two different joints.

### On-screen diagnostics

Display:

```text
FPS
SIMULATION FPS
ANIMATION CLIP
ANIMATION FRAME
BONE COUNT
VERTEX COUNT
VISIBLE FACES
VDP1 COMMANDS
SKINNING TIME
RENDER PREPARATION TIME
MEMORY USAGE
```

Use actual measurements when available.

Do not label estimated metrics as measured data.

### Acceptance

The character must animate correctly while the camera moves continuously.

No broken joints, exploding vertices, visible seam separation or cumulative transform drift.

---

## Example B — `fighter_arena_3d`

### Purpose

Demonstrate a small playable 3D fighting-game environment using the new generic character rendering infrastructure.

This is not intended to implement a complete fighting-game engine inside LibSaturn.

It is an integration and stress example.

### Requirements

Build a visually convincing Sega Saturn-style 3D arena featuring:

- Two independently animated low-poly fighters.
- A simple arena floor.
- A VDP2 background.
- Two character shadows.
- Dynamic 3D camera.
- Health bars.
- Round timer.
- Basic hit effects.
- Collision visualization toggle.
- Reusable animation transitions.
- Correct overlap ordering.

The fighters must have distinct appearances and animation states.

Use procedural or legally redistributable characters.

### Gameplay

Implement a minimal playable demonstration:

```text
MOVE FORWARD
MOVE BACKWARD
SIDESTEP
PUNCH
KICK
GUARD
JUMP
```

Include:

- A simple opponent AI.
- Basic proximity logic.
- Sample hitboxes and hurtboxes.
- Hit reaction animations.
- Health reduction.
- Knockdown.
- Round reset.

All gameplay mechanics here are example-local.

Do not introduce game-specific combat rules into the reusable LibSaturn API.

### Rendering requirements

Demonstrate:

- Two full animated characters.
- Shared and distinct materials.
- Proper scene-wide face ordering.
- Simultaneous animation changes.
- Character-ground shadows.
- Texture residency.
- Stable HUD rendering.
- No animation stalls when fighters overlap.

### Camera

Create a camera that:

- Frames both fighters.
- Adjusts distance based on fighter separation.
- Smoothly follows the combat.
- Avoids excessive movement.
- Does not produce clipping artifacts during close combat.

Reuse existing camera and scene infrastructure.

### Acceptance

The example must be genuinely playable.

It must be possible to move, attack, defend, take damage and restart a round.

The result must look like a purposeful 3D fighting-game demonstration, not an untextured skeleton debug scene.

---

## Example C — `skinning_stress_3d`

### Purpose

Benchmark the skeletal animation and rendering pipeline under reproducible loads.

### Requirements

Provide selectable workloads:

```text
1 animated character
2 animated characters
4 animated characters
8 animated characters
```

Also provide geometry detail presets and different animation complexity levels.

Do not assume the largest workload can render at full detail within the Saturn's command budget.

The example must show exactly what was accepted, culled, downgraded or rejected.

### Runtime modes

Compare:

```text
CPU skinning on Master SH-2
CPU skinning using Slave SH-2
Automatic dispatch
```

### Performance counters

Measure or calculate, as appropriate:

- Skeleton evaluation time.
- Vertex skinning time.
- Geometry preparation time.
- Painter sorting time.
- VDP1 submission time.
- Total frame time.
- Maximum frame time.
- Frame rate.
- Face count.
- VDP1 command count.
- VDP1 budget exhaustion.
- Allocated work RAM.
- Texture VRAM usage.
- Parallel queue and synchronization overhead.

Make the benchmark deterministic using fixed poses, camera paths and workload configurations.

Document all measurement caveats.

Provide a host benchmark for comparing serial and parallel-equivalent outputs.

---

# 10. Testing Strategy

Testing is mandatory and should accompany each implementation phase.

## 10.1 Host tests

Implement tests for:

### Skeletons

- Empty skeleton rejection.
- One-bone hierarchy.
- Multi-level hierarchy.
- Invalid parent references.
- Cyclic parent rejection.
- Local-to-world transformation.
- Bind-pose correctness.
- Deterministic matrix evaluation.

### Skinning

- One influence.
- Two influences.
- Three influences.
- Different local positions per influence.
- Exact half-weight blending.
- Weight normalization.
- Invalid joint index.
- Invalid buffer capacity.
- Extreme coordinate values.
- Overflow behavior.
- Signed arithmetic behavior.
- Shared asset with independent character instances.

### Animation

- Clip sampling.
- Looping.
- Non-looping playback.
- Frame transitions.
- Crossfades.
- Root motion.
- Animation event dispatch.
- Correct state after rapid animation switching.

### Rendering

- Dynamic mesh updates.
- Unchanged index and material arrays.
- Correct face orientation.
- Visible geometry.
- Backface culling.
- Character overlap.
- Clipping.
- Command overflow handling.
- Texture residency.

### Parallel execution

- Serial/parallel output equivalence.
- Independent jobs.
- Cancellation.
- Master-only fallback.
- Cache ownership.
- Capacity failures.
- Repeated frames without state corruption.

## 10.2 Golden pose fixtures

Create small reference datasets containing:

```text
Skeleton hierarchy
Rest pose
Animation pose
Expected joint matrices
Expected deformed vertices
Expected rendered bounds
```

Generate these from an independent mathematical reference implementation on the host.

Compare fixed-point output with clearly documented error tolerances.

For exact integer cases, require exact equality.

## 10.3 Emulator validation

Build and execute the new examples using the repository's established Ymir automation.

Where supported, validate with Mednafen as well.

Check:

- Successful Saturn boot.
- Correct rendering.
- Controller response.
- Frame progression.
- No crashes.
- No memory corruption.
- No invalid VDP1 commands.
- No visible texture corruption.
- No deadlocks during parallel execution.
- Correct HUD rendering.
- Reproducible test scenarios.

Use deterministic controller scripts and capture screenshots at defined frames.

Do not treat compilation alone as evidence that the example works.

## 10.4 Real hardware

Provide a hardware acceptance checklist and measurable success criteria.

Do not claim real-hardware validation unless actual hardware was used.

Emulator measurements are useful but cannot conclusively establish physical Saturn timing.

---

# 11. Performance and Memory Acceptance

The stock Saturn baseline must not depend on optional cartridges or unavailable hardware extensions.

Performance objectives:

1. Preserve deterministic simulation updates independently from rendering.
2. Aim for smooth two-fighter rendering.
3. Target 60 Hz simulation when the workload permits.
4. Target 30 FPS rendering as an initial optimization goal for the two-fighter example.
5. Investigate higher rendering frame rates if achievable.
6. Do not falsify frame counters or drop simulation work to meet a display target.
7. Provide deterministic fallback quality settings when scene complexity is excessive.

These are engineering targets, not guaranteed capabilities.

For every new subsystem, measure:

```text
.text size
.data size
.bss size
Peak scratch usage
Persistent RAM usage
Animation asset sizes
Per-character geometry memory
VDP1 VRAM usage
VDP1 command count
Master SH-2 execution cost
Slave SH-2 execution cost
Synchronization overhead
```

Prefer actual measured improvements over speculative optimizations.

Compare the new workloads against an unchanged baseline when possible.

Avoid adding significant overhead to existing LibSaturn examples that do not use skeletal animation.

---

# 12. Implementation and Repository Structure

Prefer the following organization, adapting to current repository conventions:

```text
include/saturn/
    skeleton3d.h
    skin3d.h

src/
    core/
        [skeletal animation implementation]
        [CPU skinning implementation]

    [existing parallel subsystem]
        [skinning task adapter]

tools/
    model_pipeline/
        [skeletal import support]
        [animation compression]
        [skin weight conversion]

examples/
    skeletal_animation_3d/
    fighter_arena_3d/
    skinning_stress_3d/

tests/
    host/
        [skeleton tests]
        [skinning tests]
        [animation tests]
        [parallel tests]

docs/
    SKELETAL_ANIMATION_3D.md
    CPU_SKINNING_3D.md
    CHARACTER_RENDERING_3D.md
    FIGHTING_GAME_READINESS.md
```

Avoid introducing directories or source files that duplicate the project's existing architecture.

All new public APIs must be included in the appropriate umbrella headers.

Update:

- CMake.
- The existing Makefile/source manifest.
- Package exports, if needed.
- Host tests.
- Example registration.
- Documentation.
- CI and runtime harness integration.

New examples must build through the same supported workflows as existing examples.

They must consume LibSaturn itself, not copy its source files.

---

# 13. Documentation Requirements

Create comprehensive documentation covering:

## Skeletal runtime

- Public API usage.
- Memory ownership.
- Hierarchy representation.
- Pose evaluation.
- Fixed-point conventions.
- Animation transitions.
- Performance characteristics.

## CPU skinning

- Influence format.
- Multi-anchor representation.
- Quantization.
- Skinning mathematics.
- Runtime memory layout.
- Serial and parallel execution.

## Renderer integration

- Dynamic vertex ownership.
- Scene submission.
- Face sorting.
- Character overlap.
- Material limitations.
- VDP1 command usage.

## Asset pipeline

- Source requirements.
- Export commands.
- Compression options.
- Generated file formats.
- Error reporting.
- Licensing requirements.

## Fighting-game readiness

Create:

```text
docs/FIGHTING_GAME_READINESS.md
```

Include a matrix for:

- Real-time skeletal animation.
- CPU skinning.
- Multiple characters.
- Animation blending.
- Root motion.
- Attachments.
- Textured geometry.
- Character lighting.
- Shadows.
- Dynamic camera.
- Animation events.
- Multi-character rendering.
- VDP1 command management.
- Asset streaming.
- Dual SH-2 acceleration.

For each item record:

```text
IMPLEMENTED
PARTIAL
NOT IMPLEMENTED
```

Also include supporting code paths, tests, measured limitations and remaining work.

Be precise: a tested API is not automatically proof of acceptable two-fighter performance.

---

# 14. Execution Workflow

Execute the task in coherent phases.

For each phase:

1. Inspect affected code.
2. Implement the functionality.
3. Update tests.
4. Run relevant tests.
5. Fix failures.
6. Build affected examples.
7. Validate applicable behavior.
8. Update documentation.
9. Commit the changes.

Work directly on `main` and push the verified commits, provided the repository remains consistent with the baseline and no unrelated changes would be overwritten.

Do not rewrite or discard other contributors' work.

Use descriptive commits, for example:

```text
feat(animation): add skeletal pose evaluation
feat(animation): implement fixed-point multi-anchor skinning
feat(render3d): integrate skinned instances into scene renderer
feat(assets): add compact skeletal asset export
feat(parallel): support bounded skinning jobs
feat(examples): add skeletal animation demo
feat(examples): add playable fighter arena
feat(examples): add skinning benchmark
test(animation): add skeletal and skinning golden fixtures
docs: document fighting-game readiness
```

These are illustrative commit boundaries; adapt them to actual implementation dependencies.

**Do not stop after writing a plan.**

**Do not stop after adding headers or stubs.**

**Do not stop after implementing skinning without renderer integration.**

**Do not stop after implementing the library without usable examples.**

Finish each coherent phase with working, validated code.

Do not claim successful compilation, emulator execution, benchmark results or hardware compatibility without having obtained the corresponding evidence.

If a tool or external dependency is unavailable, record the exact blocker, complete all independently verifiable portions and report what remains unverified.

---

# 15. Final Deliverables and Definition of Done

The task is complete only when the following deliverables exist.

### Core library

- [x] Generic skeletal runtime.
- [x] Fixed-point pose evaluation.
- [x] One-to-three-influence CPU skinning.
- [x] Multi-anchor joint-local vertex support.
- [x] Runtime animation sampling and blending.
- [x] Root-motion support.
- [x] Dynamic mesh integration into the canonical scene.
- [x] Bounded memory and command accounting.
- [x] Character materials and lighting support.
- [x] Optional character-ground shadows.
- [x] Compact skeletal asset tooling.
- [x] Dual SH-2 skinning workload integration or a documented benchmark-backed reason to leave it disabled.
- [x] Resource loading and unloading integration.

### New examples

- [x] `skeletal_animation_3d` — working animated humanoid.
- [x] `fighter_arena_3d` — playable two-character fighting demonstration.
- [x] `skinning_stress_3d` — reproducible performance comparison.
- [x] All three examples build as Saturn ISOs.
- [x] All relevant examples are exercised in an emulator where tooling permits.
- [x] Runtime diagnostics are visible and accurate.

### Validation

- [x] Host unit tests.
- [x] Golden pose tests.
- [x] Skinning correctness tests.
- [x] Serial/parallel equivalence tests.
- [x] Renderer integration tests.
- [x] Existing LibSaturn regression tests.
- [x] No new unexplained regressions in established examples.
- [x] Memory and CPU performance reports.
- [x] Hardware acceptance instructions.

### Documentation

- [x] Public API documentation.
- [x] Asset pipeline documentation.
- [x] Performance documentation.
- [x] README updates.
- [x] Fighting-game readiness report.

---

# 16. Final Engineering Report

After implementation, provide a technical report containing:

1. Exact commits pushed to `main`.
2. Public APIs added or changed.
3. Internal architecture modifications.
4. New example descriptions and controls.
5. Successful and unsuccessful builds.
6. Host test results.
7. Emulator validation evidence.
8. Memory budgets.
9. Character and face counts.
10. Measured skinning and rendering costs.
11. Master-only versus Slave-assisted results.
12. Known hardware limitations.
13. Remaining work to support a faithful Tekken 3-style port.
14. Remaining work specifically related to LibSaturn rather than game-specific logic.

Clearly distinguish measured facts from engineering estimates.

## Final Objective

Deliver a robust reusable skeletal character subsystem enabling LibSaturn applications to display and animate multiple articulated, textured 3D characters efficiently on a real Sega Saturn.

The results must form a solid foundation for future fighting-game ports without introducing Tekken-specific assumptions into the core library.

Prioritize correctness and actual Saturn hardware behavior, then memory efficiency, then performance optimization.

**Implement the system, create the examples, test the results, commit the work and report verified outcomes.**

---

# Progress log

- 2026-10-11, phase 1 (uncommitted): `include/saturn/skeleton3d.h`,
  `src/graphics/3d/animation/{skeleton,clip}.cpp`, host tests
  `test_skeleton3d` / `test_skeleton_clip` pass, SH-2 cross-compile clean. See
  `docs/SKELETAL_ANIMATION_3D.md`. Next: phase 2 (`skin3d.h`, multi-anchor
  skinning). Emulator work waits until the Sonic Advance gate runs finish.
- 2026-10-11, phase 2 (uncommitted): `include/saturn/skin3d.h`,
  `src/graphics/3d/animation/skin.cpp`, `tests/host/test_skin3d.cpp` (8 tests
  pass, worst error 0.79 LSB vs a double reference). Next: phase 3 (scene
  integration of deformed vertex buffers) after reading `scene3d_faces` and
  `mesh3d_draw`.
- 2026-10-11, phase 3 start (uncommitted): `sat_skin_update_mesh`,
  `sat_skin_validate_mesh`, `sat_skin_bounds`; `test_skin_scene` shows two
  skinned characters interleaving in one painter queue (stubbed VDP1). Still to
  do in phases 3-4: per-submesh visibility, LOD/budget policy, lighting,
  materials, shadows.
- 2026-10-11, phases 3-4 partial (uncommitted): `character.cpp` adds
  `sat_mesh_face_view`, `sat_lod_select`, `sat_mesh_face_ramp`;
  `test_skin_present` passes. Remaining: Gouraud lighting, shadows, material
  overrides, alpha policy, then phase 5 asset pipeline.
- 2026-10-11, phase 5 pipeline (uncommitted): `tools/model_pipeline/skeletal_export.py`
  (parent-first skeleton, rigid-only joint check, frame-major baked clips with
  sign-continuous Q15 quaternions, multi-anchor skin with rigid/blend split and
  Q15 weights, float `verify` against `animation.evaluate_positions`, size
  report, deterministic C emission). `tests/tools/test_skeletal_export.py`: 9
  tests pass. Remaining: CLI wrapper + real GLB run, animation compression
  study, then phases 6-8 (need probe/emulator).

- 2026-10-13, completion of the software/example scope: SKA1 assets and per-clip
  loading, managed character materials/shadows, bounded dual-SH-2 skin jobs,
  original nine-clip humanoid viewer, playable two-character arena and
  1/2/4/8-actor stress matrix are implemented. Full host/tool regressions,
  public package headers, all three ISO builds and Ymir gates pass. Mednafen
  also exercised actual disc-backed assets without an expansion cartridge.
  Body separation now prevents forward movement through the opponent; original
  geometry, fighting pose and texture panels replace the earlier presentation.
  Configuration values are named constants.
- Acceptance still open: physical Saturn measurements, PAL character gates,
  content-specific grabs/throws and intersecting textured surfaces, and a
  30/60 rendered-frame/s target. Current two-actor Ymir submission throughput
  was about 15/s at original-scope acceptance while simulation remained 60 Hz.
  The subsequent paired-geometry follow-up reaches 20 submissions/s on Ymir.
  These limits are documented,
  rather than reported as completed hardware/performance goals. See
  [FIGHTING_GAME_READINESS.md](FIGHTING_GAME_READINESS.md) and
  [HARDWARE_ACCEPTANCE_CHECKLIST.md](HARDWARE_ACCEPTANCE_CHECKLIST.md).
- 2026-10-13, preparation follow-up: character examples use the existing
  immutable instance binding, rebuilt on LOD/reload and disabled for mutable
  flash/Gouraud materials. Direct reuse of the existing matrix helper removes
  unused homogeneous transform work without changing rounding. Matched
  two-HIGH-character Ymir intervals reduce CPU by about 6 ms and preparation
  from 29 to 22–23 ms. Submission throughput remains approximately 15/s;
  higher frame-rate goals remain open. See
  [CHARACTER_PREPARATION_PERFORMANCE.md](CHARACTER_PREPARATION_PERFORMANCE.md).
- 2026-10-13, handoff follow-up: a bounded companion log separates VBlank wait
  and VDP1 submission durations and records draw/swap counters. Matched stock
  two-HIGH-character runs reproduce the CPU result exactly. SCU-DMA raises
  median submission cost from about 1.46 to 2.56 ms on Ymir without improving
  throughput, so CPU command uploads remain the default. No draw timeouts or
  guest panics occurred in the validated runs. The next measured target is
  coarse geometry preparation through the existing batch/parallel adapter.
  See [CHARACTER_HANDOFF_PERFORMANCE.md](CHARACTER_HANDOFF_PERFORMANCE.md).
- 2026-10-13, paired geometry follow-up: the examples reuse the existing scene
  batch adapter to prepare consecutive actor pairs concurrently, then merge
  in source order before one Master painter/emission pass. Matched two-HIGH
  AUTO CPU cost falls from 50 to 41 ms and preparation from 22 to 13 ms;
  AUTO/SLAVE reach 20 submissions/s while simulation stays at 60 Hz. Continuous
  equivalence exposed and verified a fix for early Slave completion ownership.
  Full host/tool, viewer, arena, collision, material and twelve-workload stress
  gates pass on stock RAM. Physical/PAL and higher two-actor display targets
  remain open. See [CHARACTER_GEOMETRY_PERFORMANCE.md](CHARACTER_GEOMETRY_PERFORMANCE.md).
