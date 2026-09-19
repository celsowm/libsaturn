# LibSaturn — Full-Stack API & Examples Refactoring Plan (Breaking Rewrite)

**Status:** PROPOSED — implementation has not started.  
**Date:** 2026-09-19.  
**Repository:** `celsowm/libsaturn`; execute coherent, validated slices directly on `main` under `AGENTS.md`.  
**Policy:** **ZERO backward-compatibility commitment.** There is no compatibility shim, deprecated alias, dual implementation, preserved old layout, old symbol ABI, or old example retained for historical API users. Prefer a single better contract and migrate/remove every repository-owned consumer atomically. Existing external users must adapt to the new version. Hardware correctness, bounded memory, observable behavior and tests are **not** optional.

## 0. Authority, objective, and non-negotiable decisions

This plan is the **breaking-rewrite authority** for the example-driven runtime areas below. `docs/HIGH_LEVEL_RUNTIME_API_PLAN.md`, `docs/SCENE3D_PAINTER_QUEUE_PLAN.md`, `docs/SKYBRIDGE_3D_PLATFORMER_EXAMPLE_PLAN.md`, `docs/PUBLIC_API_OWNERSHIP.md` and subsystem plans are evidence and source material, **not compatibility constraints**. In case of conflict, this plan wins for the refactored API: specifically, any requirements to preserve source compatibility, old function signatures, old struct layouts, incremental deprecation or old usage examples are revoked. Keep hardware reference documents and verified hardware invariants authoritative for actual register behavior. Reconcile or supersede contradictory documents in the final migration; never leave two competing API recipes.

**Desired outcome:** a normal Saturn game expresses geometry, model instances, animations, transforms, material/fade, camera, level collision and scene submission without independently deriving face indices, clip polygons, pixel-to-VRAM addresses, depth sort or VDP1 command structures. Technical/hardware probes may deliberately use raw interfaces, but application examples never reimplement a generic subsystem to draw a box, octahedron, sprite, model, moving deck, billboard or RBG0 perspective background.

**No false abstraction:** VDP1 has no depth buffer, indexed VDP2 color-calculation is not general per-pixel RGBA, UV-preserving textured near clipping is not the same as solid quad clipping, RBG0 is an image plane rather than physical wave geometry, and a generic mesh does not automatically solve exact occlusion of intersecting solids. When a requested effect is physically unsupported, expose an explicit supported policy/fallback and a test, not misleading alpha/depth semantics.

**Keep these design properties, not old API compatibility:** stock Saturn is the mandatory baseline; caller-provided bounded storage, fixed-point deterministic host-testable math, explicit memory/command budgets, no surprise allocation, controllable VBlank/VDP1/VDP2 ordering and low-level diagnostic access. Every old public type/function may be deleted or renamed; these properties must survive the new API. Do not make optional RAM cart, Slave SH-2, SCU DMA, analog controller, CD streaming or new VDP2 effects prerequisites for the core path.

**No example-to-example includes.** A reusable implementation belongs under `include/saturn/` + `src/core/` (or a shared runtime component), a developer tool under `tools/`, an authored asset under the owning example. Technical demonstrations may contain deliberately low-level code but cannot be imported as a library by games.

## 1. Verified audit — concrete debt and ownership

| Consumer / source | Observed example-side work | New owner / required change |
| --- | --- | --- |
| `skybridge_3d/main.c:207–339` | `put_quad` and `box3` manually near-clip, project, screen-clip, create distorted sprites; patterned floor has a separate UV-unsafe path | One renderer-owned robust solid/textured face pipeline, explicit material fallback, command accounting |
| `skybridge_3d/main.c:345–399` | `draw_gem` creates and sorts eight octahedron faces each draw | Shared primitive octahedron mesh + immutable geometry + per-instance transform/material |
| `skybridge_3d/main.c:404–545` | Seesaw geometry, split decks, hole rims, decorative boxes and surface-specific faces built manually | Scene primitives / indexed mesh composition; shared level shape and collider derivation; keep authored decorations data-driven |
| `skybridge_3d/main.c:570–603` | Pig decodes animation, transforms mesh in place, maps every face shade, binds scratch and draws | Unified animated-model instance + cached immutable mesh + frame scratch, animation/material binding |
| `skybridge_3d/main.c:609–740` | Per-platform fade history, visibility/penetration special cases, camera depth and scene callback submission | Scene visibility + fade component + explicit per-object bypass policy, preserve player/support invariants |
| `skybridge_3d/main.c:810–838, 839–920` | Creates many repeated solid-pixel textures/palettes and authors sea/sky VRAM configuration | Material/palette pool and VDP2 perspective environment module |
| `skybridge_3d/main.c:1129–1186` | Orbit/follow camera, smoothing, view/projection math copied into game | Reusable camera controller with caller-owned state |
| `skybridge_3d/game.h:193–275` | Tilt/hole surface equations and deck slices manually coordinated between render and collision | One level shape contract with consistent render/collision sampling |
| `pacman_3d/main.c:393–583` | Builds projected static maze faces, computes screen-area culling, stores 16 camera views, insertion-sorts baked faces | Reusable static-scene view cache/bake and deterministic bounded sort; **preserve** amortized prerender optimization |
| `pacman_3d/main.c:670–833` | Mesh binding and scratch repeated; rebuilding Pac-Man wedge and ghost box every draw; ghost-eye billboard vertices manually constructed | Instance renderer, cached mouth frames/shapes and camera-facing billboards |
| `pacman_3d/main.c:904–933` | Local world/actor painter workarounds | Explicit renderer ordering/occlusion policy; do not pretend object-center sorting equals depth testing |
| `infinite_explorer/main.c:76–85, 157–239` | Model scratch and hand-rotated/translated Egg Mobile vertices | Composable animation and model transforms (yaw, bank, translation) without handwritten vertex loop |
| `infinite_explorer/main.c:360–398, 400–495` | Bespoke projection/render list, insertion sort, boulder billboard/shadow geometry | Reusable bounded visibility/sort and renderer adapter for chunk-relative/specialized perspective; preserve chunk coordinate precision |
| `infinite_explorer/main.c:145–155, 241–273` | Direct RBG0 bitmap/coefficients/layer setup | Reusable RBG0 perspective-world component |
| `basic_3d_texture/main.c:90–173`; `basic_3d_animation/main.c:167–276` | Nearly identical orbit/zoom/input/matrix code | One orbit camera; game-specific controller binding outside mathematical camera |
| `basic_3d_texture/main.c:219–285`; `basic_3d_animation/main.c:335–437` | Repeated mesh ownership, model upload, bounds/framing and draw setup | Unified model-instance and camera framing facade |
| `distance_fade_3d/main.c:221–295` | Fade slot mapping, projection, billboard, native color-calc command | Material-bound quantized distance fade + scene/renderer integration; keep an explicit raw-effect probe |
| `pacman_2d/main.c:165–292, 303–395` | Procedural sprite images, repeated sprite uploads, direction/frame selection | Preserve artwork generation; runtime sprite animation/atlas/texture-region workflow owns reuse |
| `vdp2_rbg0_ground`, `vdp2_nbg0_rbg0_combo`, `infinite_explorer`, `skybridge_3d` | Repeated bitmap/coefficient/rotation setup; Skybridge imports `examples/vdp2_rbg0_ground/rbg0_math.h` | Shared VDP2 math and perspective-plane API; low-level demo retains raw reference implementation |
| `physics_3d`, `voxel_terrain`, `runtime_2d`, `runtime_3d` | Already exercise many underlying APIs; physics demo draws faces individually; voxel has specialized dynamic texture | Migrate public signatures; keep physics/voxel algorithms specialized, use runtime examples as clean golden paths |

**Audit qualification:** function/line ranges refer to the 2026-09-19 inspected `main` snapshot and must be rechecked before code edits. Not all duplicate logic is bad: authored art, level rules, special render caches, domain-specific physics and hardware education have different owners than a general-purpose runtime.

## 2. New architecture and dependency direction

```text
game logic / authored level / asset declarations
                  |
       fixed-step world & instances
       / camera / animation / physics
                  |
         scene description + policies
                  |
   geometry | material | visibility | UV
                  |
          bounded render planner
       / VDP1 commands / VDP2 scene
                  |
              Saturn HAL
```

- **Public canonical API only:** invent final cohesive naming after checking every current header; do not append `_v2`, `_ex2`, `legacy`, `compat`, transitional wrappers or duplicate scene/render APIs. Old declarations and implementation deleted within the corresponding breaking slice. Update umbrella header `saturn/saturn.h`, tests, examples and docs in the same commit.
- **Separated responsibilities:** geometry generates immutable canonical local-space vertices/indices/face groups; instances own transformation/animation state; materials own color/texture/fade description; visibility is per-object/submesh bound-aware; renderer owns projection/clipping/face ordering/native command encoding; scene owns submission order and frame resources; camera owns view math; gameplay owns scoring, objectives and input policy.
- **Ownership:** caller supplies arenas or statically sized buffers up front; library returns bounded handles/typed views with lifetime and generation rules. No heap, implicit full mesh copy per frame, dynamic `std::vector`, unbounded recursion or invisible VRAM repacking. Expose `required_bytes` / `required_counts` estimators for geometry, instances, clip output, queue, materials and VDP2 working tables; capacities validated before partial frame mutation when feasible.
- **Coordinate and time contract:** fixed 16.16 local-space Y-up geometry, consistent winding, explicit model units; camera space and screen coordinate conventions in one header; update animations/physics from bounded display-frame delta or fixed-step clock; transforms must not destructively accumulate on already-world-transformed vertices.
- **Error contract:** invalid descriptor, stale handle, incompatible format, unsupported blend, capacity/command exhaustion and busy hardware are distinguishable. Debug builds supply optional counters/diagnostic event codes without aborting the entire game. Do not silently drop HUD because a near-camera face consumed the command list.
- **Rendering abstraction:** command planner should handle VDP1 and VDP2 resource coordination, but do not force VDP2 ground/sky through the VDP1 mesh interface. Raw HAL remains available with freshly defined low-level signatures strictly for tools, probes, and specialized consumers.

### 2.1 Illustrative public usage (design target, NOT implemented API)

```c
sat_scene_t scene;
sat_primitive_desc_t gem_shape = SAT_OCTAHEDRON(/*radius, half-height*/);
sat_geometry_t gem_geometry;
sat_material_t gold;
sat_model_t gem_model;
sat_instance_t gems[8];

sat_geometry_create(&gem_geometry, &gem_shape, geometry_memory);
sat_material_create_solid(&gold, gold_palette, material_memory);
sat_model_create(&gem_model, &gem_geometry, &gold);
for (int i = 0; i < 8; ++i)
    sat_instance_init(&gems[i], &gem_model, instance_memory[i]);

sat_scene_begin(&scene, &camera);
for (int i = 0; i < 8; ++i) {
    if (pickup_active[i]) {
        sat_instance_set_transform(&gems[i], &pickup_transforms[i]);
        sat_scene_submit(&scene, &gems[i]);
    }
}
sat_scene_flush(&scene);
```

Finalize names/signatures only after an executable host API sketch for **all** representative scenes; prefer a shorter, single-owner API over adding one convenience wrapper for every old call. An octahedron needs four equator vertices and two tips, eight outward-facing triangle facets, explicit face/material groups and optional independent horizontal radius and vertical half-height. Generate it **once**; vary bob, spin and placement on instances only. Renderer sorts the facets, not the game.

## 3. Workstream A — geometry, immutable assets, and instancing

1. Redesign `mesh3d.h` geometry around immutable shared local-space meshes, instance transforms and optional mutable pose output. Provide clear counts/builders for box, cube, plane, sphere, cylinder, wedge sphere, **octahedron**, axis-aligned deck/slope, quad, camera-facing quad, and indexed mesh sections. A primitive descriptor with type + size parameters must not hide unbounded tessellation. Keep explicit face winding and material-group assignments.
2. Provide static/dynamic split: build each platform/decor model at level-load, store many instances of same shape; only animation pose or changing topology writes vertex buffers per tick. Cache Pac-Man mouth frames (closed, 2-band, 4-band) once, share ghost geometry, use instance transforms. Avoid per-frame rebuilding boxes, diamonds or faces.
3. Build composition from existing shapes via user-authored scene graph **without** expensive generic tree traversal: flat bounded instance array, optional parent transform with validated acyclic references, cached world matrix, dirty flags, explicit update pass. Direct instance list is canonical for rendering.
4. Support palette-indexed solid faces and textured face groups on the same immutable geometry, with sorted per-face draw scratch; no assumption that RGB Gouraud works with indexed VDP2 composition.
5. Add precomputed bounds for each asset/animation clip and correct world-bounds update per instance; use for camera framing, visibility and collision broad phase. Model importer exports bounds/visual center/axis/clip identifiers as generated metadata.

**A gate:** octahedron normals/winding, degenerates/capacity/invalid dimensions, cube equivalence, face-group shading, one geometry shared by 8 gems and N ghosts, unmodified base vertices after 10,000 transforms, explicit total RAM/VRAM/commands on stock configuration.

## 4. Workstream B — one safe 3D renderer, material system and frame budget

1. Replace competing `put_quad`, per-face manual projection and conflicting immediate/scene paths with **one renderer core**. A low-level face emitter is private to renderer/HAL; immediate and queued APIs use the same clipping, material and command logic. Deliberately remove old public internal-layout-dependent entry points once consumers are migrated.
2. Solid near clipping in **camera space**, correct triangle/quad triangulation, screen clipping, backface/winding consistency, valid VDP1 coordinate range, reject numerical degeneracy. Textured near/screen clipping must interpolate UV/projective attributes correctly **or** expose an explicit conservative textured fallback that never stretches/shows broken geometry. Never draw a malformed giant quad; do not claim UV clipping until tests prove it.
3. Materials: typed opaque indexed texture, indexed solid, RGB polygon, Gouraud, indexed color calculation, dither fallback and explicit unsupported combinations. Separate palette allocation/deduplication, material descriptors, texture upload and per-frame draw state. Replace Skybridge's 36 repeated 16×16 solid textures and pig-specific replicated shade upload with shared indexed solid-color representation or a bounded canonical texture atlas where hardware requires a sprite. Track actual VRAM/CRAM cost.
4. Opacity/fade: separate **distance tint/cull**, indexed VDP2 ratio-based color calc, VDP1 native effects and alpha-like approximations; do not unify into fictitious unconstrained RGBA. Configure global ratio slots once, resolve material/priority/plane compatibility centrally, define exact fallback for RGB vs indexed and mixed VDP2 scenes.
5. Painter: sort camera-space object anchors and per-mesh faces, use bounding depth ranges/section splitting for large platforms, support explicit passes only as documented overrides. For intersecting/overlapping solids supply level partitioning or diagnostic "cannot order exactly" instead of promising Z-buffer-like correctness. Maintain fixed-budget stable deterministic sorting O(n log n) for arbitrary queue sizes and explicit complexity for each per-mesh pass.
6. Culling/LOD: frustum/bounds, per-object near-camera intersection policy, hysteretic fade state kept with an **instance**, budget-based optional simplification; supporting platform visibility must not depend on naive center-only cull. Draw overlays/HUD in reserved final pass. Frame planner reserves command budget for pig, support and HUD and emits counters for clipped/skipped/dropped material classes.
7. Render cache for Pac-Man's 16 fixed camera angles: cache immutable projected geometry, precomputed shading and visible-face decisions; accept bounded prewarm across loading frames. Allow dynamic state masks for pellets. Cache invalidates deterministically on static geometry/material/camera-key changes. Cache is optional and must not be forced on free-roaming scenes.

**B gate:** camera moves through/over a large platform without frame stall or HUD disappearance; texture clipping has checkerboard-UV reference images; turn-in-place reverses pig/gem visibility correctly; no face-order flicker of independent actors; 16-view static bake stays within measured memory/startup budget and preserves original Pac-Man visibility; failures expose diagnostic counts.

## 5. Workstream C — camera, transforms, animated models and timing

1. One `sat_camera_t` (final name TBD) with explicit look-at/projection parameters, near/far and screen projection, matrix cache and verified Y-up/screen-right convention. Orbit controller supports yaw/pitch/zoom bounds, follow target, smoothing, snap/reset on respawn, camera-relative movement basis and configurable obstruction strategy. Input mapping remains a separate game adapter; do not bake pad A/B/C into camera math.
2. Model asset vs instance vs animator vs pose scratch clearly separated. One asset can drive many instances and share static indices, texture resources and palette; animation decode targets per-instance pose only if needed. Compose animator output with full model matrix (yaw, bank, scale, translation) in one place; fix Egg Mobile's per-vertex hand transforms and Skybridge pig's per-face manual material/shade plumbing.
3. Expose bounded clip-name/ID manifest, deterministic clip transitions/loop/hold/reset, step by measured time, and material frame metadata without hardcoded animation order assumptions. Preserve exact stop/pause semantics and documented asset coordinate convention.
4. Camera auto-frame helper accepts **animated clip union bounds** (not bind-pose alone), configurable screen-margin/zoom policy and near-plane guard for sub-unit models. One implementation replaces almost identical `basic_3d_texture` and `basic_3d_animation` orbit code, while allowing each test to choose its own framing policy.
5. Fixed-step physics scheduler and graphics interpolation are distinct; cap catch-up without silently changing jump physics with slow render; rate and drop counters exposed.

**C gate:** animated Sonic/Eggman/pig draw through canonical facade, positions/rotations do not drift across repeated frames, idle/walk/jump and left/right turn clips reproducible, camera zoom extremes never consume invalid projection, both Basic 3D examples share zero locally copied camera math, player direction independent of camera-only orbit.

## 6. Workstream D — collision, surfaces, moving geometry and level representation

1. Unify a canonical **level surface description** (static mesh/box, slope/hinge, holes/solid regions, moving-lift transform, collapsing visibility/collider state). Gameplay's authored rules remain in its own module; generic representation/sampling/swept collision reside in library.
2. Geometry and collision must derive from the same immutable deck slices/hole mask and current platform pose, or from explicitly versioned mirrored descriptors checked for consistency. Never draw a solid floor over a collision hole, never allow invisible support over a void.
3. Provide shared surface-height query for sloped/seesaw platforms with stable fixed-point precision and exact end points; moving platform translation/tilt update once per tick, surface collider and render transform updated as one transaction. Player carry uses the signed platform delta only while supported.
4. AABB/capsule/mesh shape supported explicitly rather than disguising a pig box as a sphere body; swept contact/substep guard for side, top, head, ledge and moving deck; optional one-way policy. Surface friction, acceleration, braking, ice/grip, coyote time and jump buffering can be composed from library physics and game controller; **course rules** and goals remain game-owned.
5. Spatial locality: broad-phase query over neighboring bounded sectors, static scene preindex, dynamic collider update cost measured. Avoid quadratic full-scene per-tick checking and unbounded per-frame rebuilds.
6. Distinguish geometric shadow projection onto **actual supporting surface** from a fake fixed-y plane; render and collision read one platform pose.

**D gate:** Skybridge 4 courses retain traversability, optional 0/8 gems win, Course 2 elevators, Course 3 six holes, Course 4 six seesaws, collapser transitions, surface coefficients and contact shadow; no phantom floors, under-platform ceiling contact, fall-through on moving support, or camera-dependent physics.

## 7. Workstream E — VDP2 perspective environment and compositing

1. Move `rbg0_math.h` from example ownership into canonical library math; migrate all includes, then delete old example-shared file and duplicates. Provide a `perspective_plane`/RBG0 environment descriptor: bitmap source, tile/repetition, horizon, camera pan/rotation/scale, coefficient table, rotation params, designated VRAM partitions and palette slots.
2. HAL owns register encoding, register shadow/latch ordering, VRAM writes and VBlank commit. Runtime owns safe stock-Saturn memory layout validation and NBG0 sky + RBG0 ground + VDP1 scene priority/color-calc integration; sample game supplies sky art, ocean phase and camera values.
3. Upload static bitmap/sky once; animate sea with bounded rotation/scroll and selective palette mutation. Optional verified scanline/line-scroll or coefficient modulation must have a distinct capability flag and visual/hardware test; do not claim wave-height collisions or arbitrary real alpha. Define visible fallback without optional features.
4. Provide helper for panorama wrap/seam and sky/horizon relation to camera, keep sky NBG0 and cloud VDP1 sprite layers independent. Explicitly instrument per-frame VRAM/CRAM words, DMA applicability, VBlank timing and RBG0 coefficient changes.
5. Preserve **raw educational** RBG0 examples demonstrating native tables/registers; game examples use the high-level environment exclusively. Compare Skybridge and Infinite Explorer visuals before and after.

**E gate:** no `examples/.../rbg0_math.h` include in other examples; no direct `0x25E00000` memory write in a game example; sky/ocean horizon stable through camera movement, no visible seam or palette corruption, indexed sprites composited correctly, stock-Saturn VRAM layout passes validator.

## 8. Workstream F — 2D sprite/animation & shared user-interface layer

1. Consolidate `runtime_2d` logical textures, source regions, sprite-sheet prewarming, frame mapping and bounded atlas policies into one canonical API; add a small state/direction sprite animator with no per-frame texture uploads.
2. Pac-Man 2D may keep pixel-art generation (`build_pac`, ghost silhouettes) as authored art; place images in static generated spritesheet/atlas or startup bake, then animate by logical frame/region references, not repeated palette/resource management per actor.
3. Offer lightweight HUD/text primitives with explicit screen-space pass, reserved VDP1 command capacity, stable font ownership and optional sprite-number/text draw; avoid a heavyweight general GUI framework. Use common debug stats overlay for frame/command/cull/VRAM.
4. Ensure SDL2 and raylib adapters, if/when implemented, only translate their semantics into this native runtime and contain no hidden Saturn register, collision or clipping workaround.

**F gate:** Pac-Man 2D appearance, direction, frightened/flash animation and maze layout retained; texture uploads are startup/explicit mutation only; game HUD survives world-command overflow; `runtime_2d` remains canonical usage.

## 9. Workstream G — assets, sound and lifecycle de-duplication

1. Generated model import pipeline exports texture/material groups, authored axis conversion, union animation bounds, clip names, optional palette shade metadata and explicit resource manifests. Validation happens at build time and runtime descriptor open; no silent face-index/palette overflow.
2. Canonical asset/model/animator initialization and teardown path replaces repeated `validate → mesh_init → copy → upload → anim_init → bind_draw` sequences. Expose memory estimator and caller-provided resource scope; failure cleans all partially prepared handles without hidden allocation.
3. Audio examples can remain domain-specific synthesis/probe demonstrations. Game examples should load/register one-shot, loop and music resources through a single high-level event-to-sound mapper with explicit volume/pitch/voice limits; do not silently reintroduce bad SCSP sample format/clip gain. Follow `docs/SCSP_AUDIO_STREAMING_GUIDE.md` before changing sound code.
4. One app/frame lifecycle enforces VBlank, input snapshot, deterministic update steps, layer commit, VDP1 transparent erase, scene flush, HUD and audio update sequencing; expose a low-level explicit frame mode for hardware demos without retaining the old API shape.

**G gate:** model input failures predictable, no wrong clip by importer ordering, zero out-of-lifetime asset references, music/FX without new distortion, no scene flicker during VBlank, clear resource ownership docs.

## 10. Execution plan — ordered slices with mandatory deletion gates

| Slice | Dependency and deliverable | Exit gate before moving on |
| --- | --- | --- |
| **0. Capture baseline** | Inspect actual HEAD, public headers, all example includes, build scripts, `AGENTS.md`, hardware docs and model importer; capture resource/visual/frame baselines and known failures. Publish `docs/REFRACTOR_BASELINE.md` + API inventory and per-example problem ledger. | Baseline host tests/build/emulator attempt recorded; failures documented, not silently assumed passing. |
| **1. Freeze new contracts** | Write canonical ownership, geometry/material/instance/renderer/scene/camera/environment/lifecycle header sketch and resource ledger, decide names once; document intentionally removed public surface. | At least 3 sample programs (Skybridge gem+platform+pig; Pac-Man static scene+actors; Explorer mixed 3D/VDP2) can be expressed without generic handwritten face/depth/VRAM processing. |
| **2. Geometry and instances** | Implement octahedron + static mesh/cache/instance transforms and bounds; update primitive/model tests. Delete displaced builders/signatures and migrate consumers. | A gate; no static primitive rebuilt each frame in migrated examples. |
| **3. Material and renderer core** | Unified solid indexed/RGB/textured paths, clipping/UV policy, material/palette reuse, command budget; remove local generic `put_quad` variants. | B clipping/material tests and stock-Saturn capture; HUD protected. |
| **4. Scene, occlusion, static cache** | Canonical queue, culling/fade, static-view bake, painter policy; remove independent general-purpose sorts. | Camera-only pig/gem and Pac-Man wall/actor tests; no false claim of Z-buffer. |
| **5. Camera, models and animation** | Orbit/follow, animation-aware bounds, instance draw/pose lifecycle and frame timing; migrate 3D Basic, runtime, pig, Egg Mobile. | C gate and regression screenshots. |
| **6. Level physics** | Common deck/slope/holes/collider shape, spatial/swept queries, instance sync; migrate Skybridge while retaining game rules. | D gate across all four courses. |
| **7. VDP2 environment** | Promote RBG0 math, unified RBG0/NBG0 scene, safe VRAM layout, horizons/water; migrate Skybridge/Explorer, keep low-level probes. | E gate and RBG0 visual/hardware evidence. |
| **8. 2D/HUD/audio/assets** | Sprite animator and atlas, frame orchestration and model asset scope; Pac-Man 2D/other games migrate, sound only where justified by audit. | F/G gates, audio sample invariants, bounded uploads. |
| **9. Repository-wide breaking cutover** | Remove old APIs, old implementations, duplicate helpers, deprecated config and stale samples; refresh docs, scripts, README and SDL/raylib-readiness requirements for **new** API only. | Entire repository builds with new public headers; no deprecated symbol/old example imports, no compatibility wrappers or dual paths. |
| **10. Final qualification** | Run full host suite, all example builds, deterministic scripts, emulator captures, performance/memory and hardware checklist; record before/after and accepted platform limits. | Final acceptance matrix complete; remaining true hardware limits explicitly documented; only then mark plan COMPLETE. |

**Commit discipline:** each slice must be coherent and testable; it may break third-party users but must not leave repository-owned examples/tests/docs half-migrated. Commit directly to `main` only after that slice's checks. A slice may require multiple internal commits, but no misleading "complete" claim before its gate. If target emulator/BIOS/hardware is unavailable, record objective blocker and pass only checks actually run; do not substitute screenshots from another version. No automatic passing of an unverified visual gate.

## 11. Cross-example migration matrix (must be checked, not inferred)

| Example | Required canonical target | Preserve on-screen or behavioral contract |
| --- | --- | --- |
| `skybridge_3d` | all A–G | 4 courses, actual holes/tilt, pig animations, 8 optional gems, sea/sky, fades, checkpoints, finish, sound, no HUD vanishing |
| `pacman_3d` | A/B/C, static cache | pre-baked 16 camera angles, readable mouth/ghost eyes, existing maze/actors/pellet behavior, bounded startup |
| `infinite_explorer` | B/C/E/G and special projection adapter | chunk-relative world, seamless horizon, Egg Mobile bank/turn, terrain/weather/objective presentation |
| `basic_3d_texture` | A/B/C/G | Sonic asset framing, zoom/orbit and face appearance |
| `basic_3d_animation` | A/B/C/G | animated bounds framing, clip controls, color/Gouraud capability distinction |
| `distance_fade_3d` | B/C/E | demonstrably distinct opaque, 4-level and 8-level fade/cutoff |
| `pacman_2d` | F/G | image animation, ghost states, score, maze positions |
| `runtime_2d`, `runtime_3d` | all affected public contracts | short canonical demonstrations, not old-API compatibility demos |
| `physics_2d`, `physics_3d` | affected collision/renderer contracts | same contact behavior; can retain educational manual draw where explicitly scoped |
| `voxel_terrain`, `voxel_display_probe` | affected texture/frame contracts only | preserve specialized voxel renderer and measurement probes |
| `vdp2_*`, `transparency_showcase`, `audio_showcase` and hardware probes | HAL changes where necessary; deliberate raw demonstration | preserve explicit register-effect and audio observations; do not force high-level API when raw behavior is the teaching goal |
| Remaining `examples/*`, tools, harness and host tests | compile/link/repository-wide symbol migration | no stale calls and no dead public headers |

## 12. Test and performance specification

**Per changed public API:** compile one minimal host consumer and the relevant Saturn example, update header/API docs and assert no obsolete symbol remains in tree; run `make test` or documented test command, plus `build-example.ps1`/`run-example.ps1` or the repository's actual platform-appropriate wrapper per `AGENTS.md`. CI must fail if a game example contains known forbidden generic helper names (`draw_gem` geometry builder, locally copied orbit math, `put_quad` hardware clipper, direct RBG0 VRAM writes, example-to-example library include) after its owning slice has migrated. Do **not** ban the same code in labeled low-level hardware probes.

**Host invariants:** octahedron 6 vertices/8 faces, winding/normals/face groups; near-plane + screen-edge triangles and UV interpolation/fallback; index bounds and palette conflicts; painter stability/equal-depth/pass behavior; fade hysteresis and mix of RGB/indexed capabilities; scene capacity/failure atomicity; animation pose/transform compose and multi-instance independence; 16-camera static-cache invalidation; world-to-screen/right-axis mapping; course 3 hole-topology vs support, course 4 slope height vs visual vertices and moving carry; frame-step repeatability.

**Emulator capture matrix:** camera-only orbit pig vs gem with same XYZ; Course 1 fixed second platform at approximately X=7 Z=30/31/32, one step per capture then jump with HUD still present; every course's representative moving/hole/seesaw and finish with 0/8 gems; Pac-Man all 16 headings and moving actors behind/in front of relevant walls; Sonic and animated model camera extremes; Explorer horizon seam, biome transitions and bank/turn; 4/8 distance-fade stages; VDP2 priority and indexed material against water, including near-camera clipping and transparency stability. Compare reference screenshots and instrument command counts; never "fix" an occlusion issue by drawing all actors above all walls without documenting scene-specific reason.

**Performance ledger, before and after:** display frames/update, VDP1 draw command count and reserved HUD quota, average/max transformed vertices and emitted/clipped faces, queue sort count/time, per-frame CPU cost of model decode/physics/VRAM update, VRAM/CRAM and main RAM peak, RBG0 upload bytes, static-cache bytes and bake duration. Baseline measurements must precede numerical performance budgets: no invented FPS guarantee. Enforce no **unexplained** regression versus baseline; add quality tiers (LOD/cull/dither/object count) when the hardware cannot sustain a visual feature. Test on stock-Saturn configuration first; optional devices separately.

**CI split:** fast deterministic host unit/API tests; example builds with strict warnings and link-size report; harness scripted frame/image probes on supported emulator; manual original-hardware observations when accessible. Tests for numerical/geometric correctness do not prove visual/hardware compositing: keep both gates.

## 13. Final definition of done — hard checklist

- [ ] Only one documented canonical public API per responsibility, and legacy declarations/implementations/shims deleted. Breaking migration version and concise external porting notes published; do not promise source/ABI compatibility.
- [ ] All in-repo examples, tests, tools, docs and harnesses updated together; every game example free of its previously identified generic renderer/material/camera/geometry/VDP2 workarounds. Remaining raw paths explicitly classified as probes or specialized algorithms.
- [ ] Octahedron/pig/ghost/world models use shared immutable geometry + transform instances; animated pose memory is explicit; no repeated per-frame primitive generation.
- [ ] Solid and textured renderer safe at near/screen clipping with truthful UV and transparency capabilities; mixed VDP1/VDP2 compositing validated; HUD budget reserved.
- [ ] Scene culling/fade/painter order and optional static cache centralized; behavior when exact visibility cannot be represented documented and visually tested.
- [ ] Skybridge gameplay, Pac-Man 2D/3D, Infinite Explorer, Basic 3D, fade, terrain, voxel, physics and audio example acceptance rows satisfied or blocked with precise evidence.
- [ ] No game includes a header from another example, no game hardcodes raw RBG0 VRAM address, no redundant sprite/color palette upload per instance, no duplicate camera/orbit implementation.
- [ ] Full host tests and all example builds pass; emulator/hardware results accurately recorded, performance/resource ledger published; implementation and current-plan status updated with commit SHAs.

**Definition of architectural success:** writing a **new** 3D Saturn game using only canonical LibSaturn primitives/model instances, camera, materials, world/scene and renderer should not require copying Skybridge/Pac-Man/Explorer's generic helper functions. Raw Saturn specifics remain possible in intentionally low-level demonstrations, but are never the default path for common gameplay.
