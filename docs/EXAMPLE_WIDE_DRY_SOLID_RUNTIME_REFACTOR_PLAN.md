# LibSaturn — Example-Wide DRY/SOLID Runtime Refactoring Plan

**Status:** IMPLEMENTATION COMPLETE; EMULATOR QUALIFICATION COMPLETE; STOCK-SATURN QUALIFICATION OPEN — canonical scene, animation, environment, sprite-state, save-schema, resource-planning and repository-cutover slices are implemented, all 33 examples/build gates pass, and the available modified-Ymir plus Mednafen grid is recorded in the ownership/performance ledgers. The only remaining gate is stock-Saturn hardware memory/visual/timing evidence; this document does not claim that unavailable gate passed.
**Audit date:** 2026-09-20.
**Audit snapshot:** `main` tree `2dbbd94`; this working tree contains the uncommitted implementation and evidence changes listed in the ownership/performance ledgers.
**Coverage:** All 33 `examples/*/main.c` entry points, their directly related game/shared headers, current public runtime headers and existing refactor plans.
**Delivery policy:** Breaking, repository-wide migration; no obligation to retain old high-level signatures, layouts, aliases, shims, or duplicate render paths. Preserve documented *hardware* behavior and deliberately low-level diagnostic access.
**Relationship to existing plans:** This is a current-state, cross-example execution companion to [EXAMPLE_DRIVEN_BREAKING_API_REFACTOR_PLAN.md](EXAMPLE_DRIVEN_BREAKING_API_REFACTOR_PLAN.md), [HIGH_LEVEL_RUNTIME_API_PLAN.md](HIGH_LEVEL_RUNTIME_API_PLAN.md), [PUBLIC_API_OWNERSHIP.md](PUBLIC_API_OWNERSHIP.md), and the subsystem plans. It extends their coverage; it does **not** reset already implemented work or claim to supersede hardware reference manuals. At contract freeze, reconcile conflicting old recipes into one canonical API document and remove contradictory status statements.

## 0. Mission, evidence, and scope boundaries

Make normal Saturn games describe **what exists and how it behaves**, while LibSaturn owns reusable geometry, instances, animation, visibility, clipping, sort, material/CRAM/VRAM policy, frame planning, scene compositing, fixed-step physics integration, and resource lifecycle. Gameplay owns stage content, scoring, scripted behaviors, authored art, victory rules, and input binding choices. The HAL owns VDP1/VDP2/SCSP/SMPC/SCU/CD register and transfer details.

Success is **not** a larger convenience-function catalog. Success is deleting repeated generic machinery from games, preserving measurable behavior, providing one clear owner for each responsibility, and keeping the runtime small, deterministic, resource-bounded, and operable on stock Sega Saturn hardware.

### 0.1 Baseline facts already implemented — do not re-plan as missing

- The shared six-vertex/eight-face octahedron and `sat_mesh_build_octahedron` already exist. Skybridge already builds one gem mesh and submits gem instances, instead of generating a diamond and sorting its facets at every draw.
- `sat_scene3d_faces_t` already collects faces across multiple objects and orders platform/pig/gem faces in a shared painter. `sat_scene3d_faces_submit_quad`, `submit_box`, `submit_tiled_quad`, and `submit_instance` already exist. The renderer is **not** thereby a depth-buffered scene engine or an all-materials canonical renderer.
- `sat_scene3d_solid_pool_t` deduplicates indexed solid colors, and Skybridge uses it. Avoid claiming all of its 36 colors still allocate one separately managed user texture each: inspect the **current** pool implementation and actual VRAM/CRAM usage before optimization.
- `sat_orbit_camera3d_fit_bounds/apply_pad` already replace duplicate Basic 3D model-viewer orbit logic. `sat_anim_prepare_model_instance` already replaces Skybridge/Explorer's earlier game-owned per-vertex transform loops.
- `sat_vdp2_rbg0_ground_* ` shared math and the bounded environment/transfer/layer coordination are now promoted to the library; raw teaching probes remain deliberately explicit.
- `sat_app_frame_begin/end`, logical textures/regions, asset registry, music/sound APIs, `sat_body2_*`, spatial hashes, and step-clock helpers already exist. Extend, consolidate, and migrate them rather than introducing parallel versions.
- Current `docs/EXAMPLE_DRIVEN_BREAKING_API_REFACTOR_PLAN.md` and `docs/REFRACTOR_BASELINE.md` still describe some now-completed slices in historical tense or as outstanding; their status paragraphs/line anchors are not ground truth for current HEAD.

### 0.2 Current seam audit after the migration

| Area | Current evidence | Owner / boundary |
| --- | --- | --- |
| Three 3D APIs | Historical immediate/queue routes are removed from the public code contract; `scene.h` delegates to `scene3d_faces.h` | PASS — raw VDP1 mesh lowering remains explicitly named for focused renderer tests/probes |
| Skybridge instance lifetime | `g_gem_instances[]` and `g_pig_instance` are initialized once; draw paths update bounded pose/fade state | PASS — persistent instance, transform, animator and material bindings |
| Skybridge geometry/physics | `stage_box`, `draw_seesaw`, `sb_deck_slices`, `sb_platform_surface_y` consume shared surface/deck math | PASS — authored level data stays local; generic geometry/collider ownership is shared |
| Skybridge visibility/frame | Fade, visibility, command-overflow/HUD reservation and `sat_follow_camera3d` are integrated | PASS — bounded scene/frame policy owns shared work; game keeps course-specific rules |
| Pac-Man 3D | `sat_view_cache_t` owns 16 fixed views; immutable actor variants are prepared once and dynamic eyes remain explicit child quads | PASS — generation invalidation and bounded replay are covered; richer billboard child-instance policy remains optional |
| Explorer | Shared index sort, canonical scene submission for rocks/shadows/drones/Egg, and bounded VDP2 environment controller are active | PASS — authored Egg local pose and landmark overlay presentation remain game-owned |
| 2D sprites | `pacman_2d` uses `sat_sprite_anim_t`; `runtime_2d` uses `sat_sprite_region_anim_t` with bounded prewarm | PASS — authored procedural atlas generation remains local |
| Sound/storage | Jukebox uses `sat_cdfs_source_manifest` plus `sat_asset_register_manifest`; music lifecycle and stream counters are bounded | PASS — synchronous CD semantics stay truthful; focused source adapters remain explicit |
| HUD/resources | `sat_hud_t`, `sat_resource_plan_t`, scene command telemetry and caller-owned scratch are available and used by migrated examples | PASS — optional examples may still use lower-level font/probe calls where that is their teaching purpose |
| Dynamic images | Voxel display probe manually packs/writes every RBG0 row; runtime2d/voxel terrain use logical dynamic textures | OPTIONAL — a backend-specific surface presenter is not required for the core cutover; **raw probe remains raw** |

### 0.3 Non-goals and no-fake-abstraction rules

- Do not promise a Z-buffer, exact sorting of intersecting coplanar/crossing polygons, arbitrary programmable shaders, desktop RGBA semantics, unconstrained projective UV clipping, physically simulated wave geometry from an RBG0 bitmap, automatic independent CD I/O, or guaranteed 60 fps without measurements.
- No hidden malloc/new, standard-library heap, background thread, unbounded recursion/container, unconstrained C++ virtual graph, per-frame full-model copies, per-instance duplicate texture upload, or silent buffer growth. Use caller-provided arenas or static bounded pools and precise count/byte estimation.
- Stock Saturn is the mandatory target. RAM cartridges, Slave SH-2, DMA, analog peripherals, and advanced VDP2 effects are **optional** and cannot become core prerequisites.
- Low-level educational probes *must* be able to demonstrate raw registers, direct VRAM writes, native SCSP behavior, palette edge cases and measurable tearing. Their intentional hardware work is not game-level DRY debt.
- Keep generated assets and their attribution with the owning example. Do not move domain-specific maze rules, pig checkpoints, authored biome colors, procedural art, or deliberately specialized voxel raycasting into the runtime merely because another example also has a loop.

## 1. Canonical architecture and SOLID ownership rules

```text
game logic + authored stages + manifests + input mapping
                         |
      bounded world state / instances / animator / physics
           camera controllers + fixed-step coordinator
                         |
           canonical scene descriptions and policies
         geometry / material / visibility / view cache
                         |
             one deterministic render planner
      VDP1 face lowering + VDP2 environment coordination
                         |
       explicit HAL: VDP1 / VDP2 / SCSP / SMPC / SCU / CD
```

1. **SRP:** assets are immutable source definitions; instances own mutable object state; animators own clip clocks/pose; geometry owns topology; colliders own contact semantics; cameras own view math; scene owns visibility/submission; renderer owns clipping/order/commands; HAL owns registers. Avoid one mega `sat_game` or `sat_scene` handling input, physics, music, saving and rendering.
2. **OCP:** add geometry providers, material capabilities, camera controllers, environment modes, physics shape providers and presenters through small typed descriptors/adapters; no growing game-specific switch over `pig`, `gem`, `ghost`, `ocean`, etc. Do not introduce a large plugin/vtable framework for a few fixed Saturn modes.
3. **LSP:** capability/fallback behavior must be explicit. A textured drawable that cannot be safely clipped must report or use its documented opaque fallback, not masquerade as a fully generic translucent polygon. Each presenter backend declares real pixel formats, transfer windows and synchronization.
4. **ISP:** game-facing headers stay small and composable; renderer-internal face/clipping structures and VDP2 register tables do not leak into model-instance and gameplay APIs. Raw hardware functions live in clearly named low-level interfaces.
5. **DIP:** SDL/raylib adapters, game code and scene orchestration depend on the canonical runtime contracts; Saturn register workarounds stay below them. Never add SDL/raylib behavior to the core as a design constraint.
6. **DRY:** exactly one implementation owns projection, camera-space depth, face sort, solid/textured near clipping, texture-region address math, palette deduplication, RBG0 coefficient encoding, static-view invalidation, fixed-step consumption and render-budget accounting. Consolidate old implementations and delete displaced paths as part of each breaking slice.

### 1.1 Memory, lifetimes and errors

Define ownership and generation/lifetime rules for geometry assets, model resources, textures/regions, palettes/materials, instances, pose/output scratch, view caches, persistent static scenes, VDP2 environments, audio assets, save schemas, and frame resources. Provide `*_requirements` / `*_init(storage, capacity)` or one shared typed arena descriptor; validate alignment, capacity, format, bounds and child/parent lifetime *before* mutation when feasible. Keep matrices in local-space assets immutable; derive world pose once per dirty instance and per update step.

All operations report distinct invalid descriptor, unsupported feature/material combination, stale handle, capacity, command exhaustion, busy hardware, I/O and verification failures. Optional debug counters explain skipped faces/instances, unsafe UV, clipped vertices, CRAM conflict, VBlank transfer overrun and HUD reservation. Do not convert unexpected failures into silent success.

### 1.2 Coordinate/time contracts

Use a single signed fixed-point convention for local/world/camera coordinates, units, handedness, winding, projection near plane, screen axes, wrap/overflow and transformation order. Clarify time in milliseconds vs 16.16 seconds vs display ticks; the engine exposes deterministic fixed-step consumption with maximum catch-up and edge-trigger input delivered once per display frame. A game chooses control mapping and whether paused simulations advance.

## 2. Workstream A — one public 3D scene and renderer core [P0]

**Observed:** the immediate scene and callback/object queue facades have been
removed from the public code contract; the current migration surface is the
canonical scene description plus the renderer's explicit low-level lowering
path. Remaining acceptance work is cross-object visibility and measured
capacity/performance validation.

**Refactor/create:**

1. Sketch and compile three representative consumers **before freezing names**: Skybridge mixed platforms/pig/gems; Pac-Man static cached maze + dynamic actors; Explorer chunk-space scenery + animated craft. Confirm every consumer uses one public scene, one material/instance model and explicit environment attachment without fallback to private manual sort/clip.
2. Define one canonical `sat_scene`-like lifecycle: initialize with bounded scene/frame storage, set camera and optional environment, submit primitives/instances/static-cache views, flush world, reserve/render overlay, finish. The scene exposes explicit native override only for hardware probes/specialized draws.
3. Keep `sat_scene_t` as the sole game-facing scene submission route. The former `sat_scene3d_queue_t` and `sat_draw_mesh` names are removed; explicit `sat_vdp1_draw_mesh` remains only for low-level renderer tests/probes. Face generation, clipping, projection, ordering, material lowering and command emission must remain owned by one implementation.
4. Separate per-instance world bounds/camera frustum visibility from per-face sorting. The existing cross-object face painter is the baseline; **preserve**, optimize and integrate it, not regress to per-object-center painting. Sorting is deterministic stable O(n log n) for potentially large queues; budget the actual scratch cost.
5. Use explicit layer/pass semantics only for artistic overlays and known exceptions; do not draw all actors above all walls to hide sort bugs. For geometrically intersecting faces, expose subdivision/partition policy or an explicit unsupported/diagnostic state. Model depth ranges and supporting-platform/near-eye penetration need conservative policies.
6. Solid camera-near and viewport clipping, degenerate rejection, winding/backface, VDP1 coordinate boundaries and textured fallback share one renderer pipeline. True UV-preserving clipping is a separately gated feature; if not supported, use a documented safe tiled/opaque fallback without giant distorted sprites.
7. Make world/actor/critical/HUD command reservations a named frame-budget policy, not local boolean flags scattered through examples. Flush must have deterministic failure atomicity and no double submission.

**Deletion/migration gate:** redundant high-level queue/immediate/faces public
routes and their separate algorithms are now deleted; explicitly low-level
HAL drawing remains available. No client game computes another actor's painter
order or has its own clipping implementation.

**Acceptance:** camera-only pig/gem visibility reverses correctly; near-eye platform does not eliminate HUD or collapse frame time; Pac-Man walls/eyes/actors have correct reciprocal visibility across all camera views; order stable across equal-depth entries; invalid capacity never writes beyond buffers; command/CPU/VRAM deltas measured.

## 3. Workstream B — immutable geometry, persistent instances, materials [P0]

1. Retain existing octahedron/box/plane/sphere/cylinder/wedge generators; consolidate geometry requirements/face winding/face-group conventions. Add reusable *bounded* deck-with-hole, slope/seesaw surface, rect strip/rail and billboard geometry only where a general model/primitive cannot already express them.
2. Create persistent `asset -> model -> instance` relations. A shared immutable mesh/material binding can drive N gems, ghosts, rail modules, terrain objects or model viewers. Each instance has stable handle, local transform, optional parent, visibility, bounds, render policy and optional animator. Instance children form an acyclic bounded relation with dirty-matrix propagation, not a recursive heap scene graph.
3. Preserve the current animated pose-preparation helper as an internal ingredient. Decode per-instance local pose only when advanced; combine transform once; never transform an already-world-space pose a second time. One model may drive independent animators; never mutate base mesh indices/vertices per draw.
4. Move skybridge gem local matrix construction, transient descriptor setup, pig shade binding and Explorer's residual matrix/bind/draw boilerplate behind persistent instances. Pac-Man caches authored mouth variants once and uses shared ghost geometry and camera-facing eye attachments instead of building models per actor per frame.
5. Define a canonical material resource: solid indexed/RGB, textured indexed, Gouraud where supported, palette and per-face material groups, priority, native color calculation and fade capability. Keep unsupported combinations observable. Deduplicate color/material references and source texture uploads without assuming arbitrary alpha.
6. Add explicit material/texture resource estimates, CRAM bank constraints, pool exhaustion policy, animated-model clip-name IDs/union bounds, source asset validation and clean partial-initialization unwind.

**Acceptance:** eight gems and N ghosts reuse one geometry source each; no per-frame static primitive construction; animated independent instances never bleed pose or material state; model/animation source remains unchanged after 10,000 transforms; capacity/ownership tests pass; stock-RAM resource ledger updated.

## 4. Workstream C — shared level surface/collider and character mechanics [P0]

1. Define one immutable *surface description* and current dynamic pose for flat decks, holes (up to declared aperture/slice budget), slopes, seesaws, elevators, moving/collapsing/one-way platforms. A topology builder produces visual section references and collision representation **from the same source**; no separate invisible collision floor over an opening.
2. Define `surface_height_at`, solid-footprint/ground-support, side/head contact, swept/tunneling guards, broad-phase bounds, carry velocity/displacement and moving-platform contact events. Physics queries do not infer surface from camera or renderer; renderer never invents colliders.
3. Extract generic `sb_deck_slices`, platform surface/elevator/seesaw geometry and crossbar/strip generation where physically reusable. Keep Skybridge course tables, completion, gem count, checkpoint, damage, platform timings and unique artwork in game code.
4. Compose reusable character motion primitives (acceleration/brake/friction/gravity/jump/coyote/buffer) with explicit player controller parameters and pad mapping adapter. Do not hardcode pig dimensions, camera-relative input or directional pad buttons in the physics kernel.
5. Consolidate physics 2D/3D fixed-step orchestration, colliders/spatial broad-phase and contact telemetry without pretending that 2D and 3D have identical manifolds. Provide stable, measured scaling and optional static spatial preindex.

**Acceptance:** Skybridge all four courses maintain holes, moving/tilting decks and zero-gem completion; no phantom floor, missed ceiling/side hits or incorrect rider carry; repeated deterministic fixed-step runs match; physics examples preserve collision behavior; no visual/collision mismatch at slope endpoints.

## 5. Workstream D — camera controllers, bounds and time [P1]

1. Keep existing orbit fit/apply-pad functionality; unify it with canonical 3D camera state rather than add another independent camera matrix implementation.
2. Add small policy/state controllers for orbit, fixed, chase/follow (smoothing, lag, obstruction strategy, snap/reset, offset/look-ahead, camera-relative movement basis). Input binding remains a separate adapter; controllers can be updated without any pad attached.
3. Replace Skybridge's hand-written chase smoothing/offset/reset with `sat_follow_camera3d`; migrate Runtime 3D's hand-written trigonometric orbit to the existing orbit controller; preserve Basic 3D's successful fitted camera usage. Projection remains owned by `sat_camera3d`.
4. Use imported animated union bounds and configured screen margins/near guards for model framing. Validate camera Y-up and screen-right against authored model axes and Explorer horizon.
5. Provide explicit `step_clock`/frame orchestration that distinguishes display frame from simulation tick and handles pause/long-frame catch-up and once-only pressed edges.

**Acceptance:** stable pig screen position across orbit; correct snap after fall; model viewer zoom/clip bounds valid for small and animated models; camera frame reproducibility, no extra projection rebuild per instance and no change in gameplay orientation on camera-only turns.

## 6. Workstream E — VDP2 environment, palette and safe transfer [P0/P1]

1. Promote reusable NBG0 sky + RBG0 perspective terrain/ocean composition into one *environment* descriptor/controller. Cover bitmap/map, horizon, coefficient table, rotation, tile/repeat, scrolling, layer priority, sprite priority, palette allocation, optional color-calculation configuration, exact VRAM partitions and register-latch scheduling.
2. Reuse existing `sat_vdp2_rbg0_ground_*` math and existing indexed bitmap upload functions. Move Skybridge/Explorer *coordination* (their current direct coefficient and 48-word table writes, layer setup, hardcoded placement) into the runtime; authored sky/sea color functions remain game-owned.
3. Static images upload once; dynamic water movement may update rotation/scroll, selective palette slots, optionally verified line effects — **not** force full bitmap re-upload every frame. A panorama/horizon seam contract handles wrap and camera movement consistently across NBG0/RBG0.
4. Validate VRAM bank occupancy, CRAM overlap, NBG0/RBG0/VDP1 priority, transparent codes and indexed-sprite color-calculation combinations before activation. If an effect is unsupported, return a capability/fallback result rather than fabricate RGBA blending.
5. Keep `vdp2_nbg0_ground`, `vdp2_nbg0_ground_api`, `vdp2_nbg0_image`, `vdp2_rbg0_ground`, `vdp2_nbg0_rbg0_combo` as clearly categorized raw-versus-high-level examples. At least one high-level example must need **zero** manual VDP2 VRAM addresses; at least one deliberately raw probe remains to teach register layout.
6. Separate dynamic surface presentation from environment setup. Offer an *optional* backend-specific surface presenter for logical-texture VDP1 vs RBG0 bitmap display with explicit format/pitch/transfer-window/swap or tearing capability. The voxel display probe deliberately keeps its raw writes as a comparison baseline.

**Acceptance:** Skybridge sea/sky, Explorer horizon/biomes, transparency showcase and RBG0 combo show no new seam, flicker, palette corruption or unexpected blending; bounded per-frame upload bytes measured; stock-Saturn VDP2 memory planner rejects overlaps.

## 7. Workstream F — static view cache and scalable visibility [P1]

1. Add opt-in immutable scene bake for a declared finite camera set, with precomputed view-specific projection, visible faces, shading, stable painter ordering and compact metadata for mutable elements (e.g., pellet occupancy).
2. Pac-Man 3D retains its 16 views and incremental progress/loading behavior. Cache invalidation keys include geometry/material/camera/projection/visibility-policy versions; no stale cache replay after mutation.
3. Shared 16/32-bit index/sort facilities must not be capped at 255 objects by a legacy `uint8_t` index or require sorting full face structs each frame.
4. Use bounding-volume culling and spatial prefilter for large scenes. Support Explorer's chunk-relative coordinate strategy and specialized projection as a *typed adapter*, not force globally accumulated world fixed-point positions that overflow or lose precision.
5. Expose debug counters for culled, clipped, baked, cache hits, rejected submissions and ordering conflicts.

**Acceptance:** all 16 Pac-Man headings preserve walls/pellets/eyes, bounded prewarm remains responsive, cached frame cost stays no worse without a measured reason; Explorer chunk crossings remain seamless; no O(n²) per-frame sort for variable-size dynamic render queues.

## 8. Workstream G — 2D sprites, atlas, UI and input [P1/P2]

1. Layer a persistent logical sprite/animation resource on existing `sat_texture_t`, region preparation and render2d. Frames map to named animation/state/direction or compact indexed ranges, with per-instance frame clock, loop/hold, facing retention and optional palette variant.
2. Pac-Man 2D keeps authored procedural artwork but emits/prepares a shared atlas or declared region set at init; render uses sprite state + instance position rather than `direction × frame` texture arrays and hand-coded frightened/flash frame selection.
3. Runtime 2D uses declarative region prewarming and can demonstrate a camera, sprite animation, dynamic surface and UI without manually addressing atlas regions per tick.
4. Introduce a tiny screen-space HUD API for text/value/bar/marker/layout, numeric formatting and opt-in diagnostic overlay; reuse existing font/text/render2d rather than create a retained desktop GUI. Reserve HUD commands and keep UI above the world in a known pass.
5. Consolidate input snapshots/events so a canonical frame poll happens once. Avoid `sat_app_frame_begin` followed by redundant explicit `sat_input_poll/sat_pad_poll_port` unless documented multiport semantics require it. Bind actions in the game, not inside camera or physics.

**Acceptance:** Pac-Man ghost/pac/flash directions match baseline with no per-frame upload, text/score remain readable under world overflow, input pressed events occur once per display tick, input-debug stays a dedicated device diagnostic.

## 9. Workstream H — assets, audio and CD composition [P1]

1. Generate explicit per-example manifests for static textures, regions, fonts, models, clips, data, sounds and streamed music. Validate formats/sizes/capacities at build time; runtime registration/loading receives a bounded manifest plus provided storage.
2. Unify `validate -> mesh init -> model copy -> texture/material upload -> anim state -> draw binding` into one model-resource preparation contract with precise memory/VRAM requirements, explicit handles and rollback; never re-upload per instance.
3. Introduce an optional CDFS/VFS source-mount manifest or helper that resolves disc paths, validates extents and exposes logical assets without reproducing per-track mount/lookup/backend/descriptor glue in the Jukebox. Keep synchronous CD-block semantics truthful; cooperative prefetch remains cooperative.
4. Provide a small event-to-SFX binding/mixer policy for one-shot/loop/priority/volume/pitch with explicit voice exhaustion; game chooses event names and mappings. Do not force specialized audio probes into a gameplay sound-map facade.
5. Music lifecycle should allow a safe bounded open/prefetch/play/replace/pause/resume/stop/close operation with observable underrun/refill and no sample-format/volume regression. Before changing SCSP or streaming, follow `docs/SCSP_AUDIO_STREAMING_GUIDE.md` and hardware manuals; retain diagnostics for source sample position, SCSP registers and Sound RAM.
6. Resource scopes centralize partial-failure cleanup and ownership without hiding lifetime or general-purpose allocation.

**Acceptance:** Jukebox can switch tracks repeatedly without leaked handles or false nonblocking claims; unchanged pitch and no clipping/distortion; runtime2d init/teardown and multi-instance model resources are bounded and clean; BIOS/emulator checks recorded where available.

## 10. Workstream I — saves, persistence schema and optional cartridge [P2]

1. Keep existing `sat_save_*` device and format/read/write/verify primitives; introduce an **optional** typed/versioned save-record facade for magic, schema, payload size, checksum/verification policy and explicit migration callbacks.
2. A caller-provided serializer/deserializer controls endianness, persistent wire format and game version; no blind byte-copy migration of changed C struct layouts. Explicitly surface not-formatted/no-space/not-found/verify/version errors.
3. Support built-in save and compatible cartridge devices through documented capabilities, without making an optional cartridge a default dependency. Destructive formatting never occurs as an automatic convenience fallback.
4. Retain raw `save_cartridge_probe` and low-level verification demonstrations.

**Acceptance:** demo can reopen/update boot count across compatible versions; unknown versions are rejected or passed to an explicit migration; write failure leaves diagnosable state; memory and file-slot budgets are documented.

## 11. Workstream J — resource planning, frame budgets, diagnostics [P0/P1]

1. Provide central requirements for RAM/WRAM, VRAM/CRAM, model/anim scratch, scene face buffer, key/index sort, render cache, texture regions, VDP2 coefficient/rotation tables, audio staging and HUD quota; calculate from immutable manifests where feasible.
2. One planner/arena validates alignment, overlap, capacities, critical reservations and chosen optional quality tier before or at scene activation. Avoid silently repacking currently referenced VRAM or allocating a second palette copy.
3. Per-frame planner differentiates mandatory player/support geometry, optional decorations, static baked geometry and protected HUD. Prioritize *declared* passes with deterministic overflow policy, not incidental draw-call order.
4. Expose counters plus debug HUD: input/event overflow, face/command count, per-material drop reasons, cull/LOD/fade, VRAM upload bytes, palette writes, audio underruns, queue/sort/cache timings. Keep instrumentation optional/cheap in release.
5. Define observable error recovery; avoid `SAT_ERR_CAPACITY` being treated as a generic benign outcome for mandatory geometry while preserving intentionally nonfatal optional effects.

**Acceptance:** Skybridge HUD never vanishes under near-camera or full-scene command pressure; capacities prevent OOB and partial resource corruption; diagnostics identify the precise dropped class; per-frame CPU and memory costs are measured, not guessed.

## 12. Workstream K — public API pruning, docs, tooling, SOLID checks [P0]

1. Produce a **single canonical header ownership map** covering game-facing scene, geometry, model, instance, animator, material, camera, physics, environment, surface/presenter, sprite, HUD, asset/audio and save schema. Keep raw `sat_vdp1_*`, `sat_vdp2_*`, SCSP/CD probes in explicit hardware interfaces.
2. Delete displaced `*_ex2`, legacy overloads/shims, competing public facades, copied generic helpers and obsolete implementations atomically with consumers/tests. Do not preserve stale code purely to build old examples.
3. Classify every example as **canonical high-level game**, **focused feature demo**, or **intentional hardware probe**. Raw hardware operations are allowed in a labeled probe; copied generic render/resource algorithms are not allowed in a game.
4. Update `saturn/saturn.h`, README, API docs, sample snippets, current coverage, ownership, SDL2/raylib readiness, `REFRACTOR_BASELINE`, prior plan status and code comments at the same migration boundary. Correct historical source-line claims in old documents; track done vs blocked vs not-started with commit IDs.
5. Add host/API tests for sole public owner per capability, compile-only API sketches for the three representative games, static scan for forbidden cross-example includes and former generic duplicate helpers, and CI all-example compilation/link gates.
6. Follow `AGENTS.md`: execute available project build/run/emulator scripts yourself; record missing SH-2 toolchain, BIOS, emulator GUI or hardware as blockers rather than marking visuals passed.
7. Maintain a measured *before/after* performance and resource ledger per migrated example (VDP1 commands, face counts, sort comparisons/cycles, CPU update/render time, VRAM/CRAM, main RAM and audio/VDP2 transfer work).

**Acceptance:** a new game can render an animated character, 8 gem instances, solid and holed platforms, a moving camera, sea/sky, HUD and SFX without implementing a generic clipper, painter, palette allocator, VRAM map, orbit/follow math or engine-owned physics geometry.

## 13. Complete example migration/acceptance matrix (33 entry points)

Do not silently omit small examples. Each row must be marked **canonical**, **feature**, or **probe** at migration time; preserve intentional raw operations only in probes. All examples must build against the new single-owner public contracts.

| Example | Classification / work | Must preserve |
| --- | --- | --- |
| `audio_showcase` | Feature/probe: manifest, sound lifecycle/voice diagnostics; keep parameter experiments explicit | Sample rate/pitch, pan, looping, voice stats, music controls |
| `basic_3d_animation` | Canonical viewer: persistent animated instance, existing orbit/animated bounds, material capability | Clip controls, framing, pausing, textured/Gouraud appearance |
| `basic_3d_texture` | Canonical viewer: model resource preparation, existing orbit, scene submission | Texture appearance, rotation/zoom, no duplicate uploads |
| `cd_block_probe` | Raw hardware probe: keep synchronous sector read and error/status visibility | Physical read semantics and timeout/error output |
| `cd_streaming_jukebox` | Feature: CDFS/asset manifest, music lifecycle, bounded prefetch | Correct tracks, continuous playback, no new distortion |
| `distance_fade_3d` | Feature + optional raw effect probe: material fade/slot binding through scene | Distinct opaque, quantized fade stages and cutoff |
| `hello_world` | Canonical minimal app: smallest text/surface setup, frame path | Text actually appears; no needless infrastructure |
| `infinite_explorer` | Canonical game: chunk-aware scene adapter, animated instance, VDP2 environment, visibility | World continuity, landmark order, sky/horizon, craft animation and sound |
| `input_debug` | Input probe: keep button-event/counter visibility | Held/pressed/released and port/device behavior |
| `input_move` | Feature: optional reusable movement/input and HUD helpers | Held vs stepped movement and bounds |
| `mvp_2d_scene` | Canonical minimal 2D: asset/atlas/font resource and frame lifecycle | Textured sprite and primitive appearance |
| `pacman_2d` | Canonical game: sprite atlas/animator, shared UI, resource lifecycle | Maze, direction, frightened/flash, scores and motion |
| `pacman_3d` | Canonical game: 16-view cache, immutable variants, scene painter, eye billboards | All angles, baked startup behavior, pellet masks and actor occlusion |
| `physics_2d` | Feature: reusable fixed-step/physics/box presentation | Collisions, bounce, pair counts, input |
| `physics_3d` | Feature: canonical mesh instance submission; keep demonstrative collision probes | Ray/box tests, body motion, visible contacts |
| `ram_cart_demo` | Optional device feature/probe; resource ownership remains explicit | Cart detection/read/write/capacity and stock fallback |
| `red_square` | Canonical small drawing/palette demo | Pad-driven palette changes and square draw |
| `runtime_2d` | Canonical API acceptance: logical manifest, sprite animation, surface update, camera/clip/UI/audio | Dynamic texture, events, controls, music, teardown |
| `runtime_3d` | Canonical API acceptance: model resource + persistent instance + shared orbit and scene | Animated/rotating model view, orbit, HUD |
| `save_backup_demo` | Feature: optional versioned save schema on existing raw device API | Counter, verification, enumeration and unformatted handling |
| `save_cartridge_probe` | Device probe: raw info/error path | Device recognition, safe no-format behavior |
| `sega_bg` | Canonical asset/textured background minimum | Same image/palette and predictable startup |
| `skybridge_3d` | Primary all-layer acceptance: surface/collider, pig/gems, materials, 3D painter, VDP2 environment, HUD/audio | Four courses, true holes/tilt, optional gems, collisions, no HUD loss |
| `text_sprite` | Canonical font/sprite minimum | Correct texture/palette/text presentation |
| `transparency_showcase` | Focused capabilities/probe: native blend vs documented fallback | Distinguishable VDP1/VDP2 modes, stable priority and palette |
| `tvstat_probe` | Raw timing probe: no forced high-level frame control | TVSTAT timing measurement remains direct |
| `vdp2_nbg0_ground` | Raw teaching probe: retain native NBG0 register path | Native tile/map/scroll demonstration |
| `vdp2_nbg0_ground_api` | High-level counterpart: use canonical NBG0 image/map helper | Same output as raw teaching counterpart |
| `vdp2_nbg0_image` | Raw/image probe: document intended native upload steps | Correct tile packing, pattern map and image |
| `vdp2_nbg0_rbg0_combo` | Environment acceptance + retained explicit raw comparison | Compositing, coefficient/rotation and priority |
| `vdp2_rbg0_ground` | Raw ground probe + shared math; optional canonical twin | Perspective, scroll, horizon and visuals |
| `voxel_display_probe` | Intentional raw RBG0 presenter benchmark + optional canonical comparison | Tearing/transfer evidence; raw writes remain observable |
| `voxel_terrain` | Specialized renderer using canonical texture/frame resources | Terrain visuals, controls and bounded pixel throughput |

### 13.1 Supporting file migration

Audit and update `examples/skybridge_3d/game.h`, `scenery.h`, `examples/infinite_explorer/explorer_logic.h`, `examples/common/pacman_game.{c,h}`, `pacman_maze.h`, each `Makefile.inc`, generated model headers/manifests and build/import scripts. The fact that a helper appears in a game header does not automatically make it reusable: distinguish generic algorithm from authored gameplay.

## 14. Implementation order — coherent, measurable, directly on main

**Precondition to every slice:** inspect current HEAD and `AGENTS.md`; maintain a one-row work ledger with base SHA, changed public contracts, example migrations, removed old symbols, tests run, build/run/emulator results and resource deltas. Do not overwrite another contributor's newer changes.

| Phase | Deliverable and predecessor | Required exit gate |
| --- | --- | --- |
| 0. Refresh audit | Compare this dated snapshot to HEAD; classify all examples, inventory public names; record actual host/build/emulator baselines and historical plan drift | Evidence/coverage ledger, no completed feature falsely called missing |
| 1. Freeze contract | Compile host API sketches for Skybridge, Pac-Man 3D, Explorer and minimal 2D; settle scene/model/instance/material/camera/environment/resource ownership | Single API naming/dependency map, no unresolved competing game-facing render paths |
| 2. Renderer core | Consolidate scene/painter/immediate algorithms, material and clipping policy, critical/HUD budget; migrate Skybridge and runtime3d | Bounded painter/near-eye/HUD tests, remove displaced public path |
| 3. Persistent resources | Shared models, instance state, parent/pose/animator/material handles, requirements/arena; migrate gem/pig/Pac-Man/Explorer/Basic 3D | No per-frame immutable geometry rebuild or duplicate model uploads |
| 4. Level/physics | Canonical visual+collision surface and character controller; migrate Skybridge 4 courses and physics demos | Hole/slope/move/carry/ceiling/contact deterministic and visual gates |
| 5. Camera/cache | Orbit/chase unified; fixed-view cache and chunk adapter; migrate Pac-Man/Explorer/model viewers | 16-view equivalence, no horizon/precision drift, measured resource bounds |
| 6. VDP2/material/presenter | Environment resource planner, sky/ground/sea and palette/color calc; optional surface presenter | Mixed-scene captures and raw-vs-high-level probe equivalence |
| 7. 2D/UI/input | Sprite state/atlas, HUD/debug policy and one input snapshot; migrate Pac-Man 2D/runtime2d/small apps | Animation, input and UI visual gates |
| 8. Manifest/audio/save | Resource loading/cleanup, CD/music bindings, optional schema facade; migrate Jukebox/audio/save demos | No sound distortion/leaks and save/version safety |
| 9. Repository cutover | Remove obsolete symbols/files, update all 33 examples and docs, finish SDL/raylib native readiness | Full host suite, all example builds, no stale recipes/imports |
| 10. Hardware qualification | Stock hardware/emulator captures and performance ledger; document unsupported capabilities | Only mark COMPLETE if all required gates have evidence; otherwise enumerate exact blockers |

**Commit policy:** one coherent validated slice at a time on `main`, with repository-owned consumers/tests/docs updated in the same slice. Compatibility with third-party source code is not a reason to preserve duplicate public APIs. Raw diagnostic access is explicitly retained. If toolchain/emulator/hardware is missing, state exactly what could not be tested and never conflate compilation with visual hardware correctness.

## 15. Acceptance tests, instrumentation and forbidden regressions

### 15.1 Host/API tests

- Static mesh immutability, facet winding/material groups, shared N-instance lifetime, parent-cycle and stale-handle rejection, no transform drift, independent animators and bounds.
- One deterministic scene-wide sort for multiple moving meshes; tie stability; pass semantics; near plane, screen edges, degenerates, exact supported/unsupported UV/color-calc responses; no capacity overrun or duplicate flush.
- Geometry/collider shared deck hole slices and seesaw height; swept foot/head/side contact, moving support carry, no collision floor over void; fixed-step determinism and pressed-edge once.
- View-cache key invalidation and 16-angle replay; chunk-relative projections at large positive/negative coordinates; correct camera basis.
- CRAM/VRAM resource overlap rejection, bounded coefficient/rotation writes, palette ownership and rollback.
- Atlas state/direction/frame selection, resource teardown, CD source manifest validation, music lifecycle, save version/failure handling.

### 15.2 Required emulator/hardware capture grid

Emulator evidence already collected: the modified Ymir debug harness ran the
canonical Explorer, Physics3D, DistanceFade3D and Pac-Man 3D paths with frame
captures/input replays, including the Pac-Man 3D 16-angle replay and scene
telemetry; Mednafen was launched through the project runner for controlled
Pac-Man 2D, Physics3D and DistanceFade3D smoke runs, with process cleanup
verified after each run. These results qualify the emulator-facing migration
and are recorded in `docs/RUNTIME_OWNERSHIP_AND_EXAMPLE_LEDGER.md` and
`docs/RESOURCE_PERFORMANCE_LEDGER.md`; they do not substitute for the stock
Saturn portion of this grid.

- Skybridge: pig and gem reverse near/far order on a camera-only turn; Course 1 previous-pier camera penetration at approximately X=7 Z=30/31/32 with jump and HUD always visible; representative elevator, real Course 3 hole, Course 4 seesaw, fall/reset and finish with 0/8 gems.
- Pac-Man 3D: 16 angles with ghost/pac/eyes alternating behind/in front of walls; pellet occupancy dynamic while cache remains valid.
- Basic 3D/static+animated viewers: small and animated model bounds, zoom extremes, material/texture appearance.
- Explorer: horizon seam at several headings, chunk transition in each direction, biome/palette change, Egg Mobile idle/turn/bank.
- Distance fade/transparency/RBG0: quantized slots, opaque near geometry against sea, NBG0+RBG0+VDP1 priorities, indexed vs RGB capability response.
- Pac-Man 2D/runtime2d: direction/flash timing, clipping/UI, dynamic texture and music; voxel raw-vs-presenter tearing and row-transfer behavior.

### 15.3 Performance and completeness ledger

Record before/after: frame time/update/render, VDP1 commands and reserved HUD count, sorted/projected/clipped faces, sort time, geometry/pose CPU time, VRAM/CRAM/main RAM and WRAM peaks, per-frame VDP2/CRAM transfer words, model/atlas upload counts, CD refill/SCSP underrun metrics and static cache startup cost. Do not invent performance thresholds before baseline measurement. Every unexplained regression requires remediation or explicit accepted trade-off with supporting hardware evidence.

**Definition of done:** all 33 examples are built and classified; games no longer reimplement generic clipping, scene face sorting, static primitive construction per frame, VRAM-layout math or camera follow/orbit algorithms; only one documented canonical high-level API owns each responsibility; low-level probes retain direct access; stock-Saturn memory and visual behavior are verified; all intentionally unsupported hardware semantics are explicit; legacy APIs and stale docs are removed or reconciled.
