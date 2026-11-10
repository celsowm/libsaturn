# World Visibility, HLOD and Distance-Fade Integration Plan

**Repository:** LibSaturn (reusable engine, not a game-specific feature)  
**Target:** stock Sega Saturn, no mandatory expansion RAM  
**Status:** implementation in progress — M0 through M3 complete; `city_walk` migration/validation is complete; M4's original-geometry outdoor acceptance scene remains to be added
**Execution owner:** Codex  
**Branch:** `main`, incremental validated commits (no parallel compatibility engine)

## 0. Mission and non-negotiable boundaries

Build an allocation-free, deterministic, general-purpose **world visibility and LOD planner** that schedules immutable or streamed 3D geometry under measured CPU, scene-face, VDP1-command, and texture-residency constraints. Integrate the existing distance-fade policy and preserve the canonical `sat_scene_t` painter. Provide offline HLOD-generation hooks and a reproducible Saturn example that proves improved frame stability without abrupt horizon pop-in.

The initial consumer is a large, freely explorable outdoor scene (e.g., castle exterior plus garden), **but the feature must be reusable** by `city_walk`, `skybridge_3d`, `infinite_explorer`, character demos, and future games. Do not put Nintendo/other third-party assets in LibSaturn.

**Required invariants:**

1. **One scene owner.** `sat_scene_t` owns projection/painter/emission, and the new planner selects inputs only. Do not build a competing sorter, z-buffer, rasterizer, or immediate-mode scene path.
2. **Fade is not LOD crossfade.** `sat_fade3d_slot()` affects supported indexed VDP1 sprites blended over **lower-priority VDP2 layers**, not two overlapping VDP1 surfaces. LOD mesh switches need separate temporal/visual policies. Never claim generic VDP1-on-VDP1 alpha.
3. **Explicit rendering limitations.** No implicit "fade" for opaque RGB polygons or incompatible Gouraud materials. Capability-checked fallback: opaque, artist-approved palette fog/dither, or cull; report which was selected. Do not write an unsupported slot to the scene.
4. **No per-frame heap, IO, hidden uploads or source re-parsing.** Caller owns all planner inputs, outputs, previous states, scratch, and resident resources. Planning is pure CPU logic and must be usable in native host tests.
5. **Correctness before cost.** Frustum tests are conservative; no near-field disappearing collision landmarks, no stale streaming generations, no partial output on validation/capacity failure, no fixed-point overflow.
6. **Budget is contractual.** Account for scene-face capacity, VDP1 world-command estimate, HUD/end reserve, and CPU policy independently. Do not equate faces with actual VDP1 commands: near-plane fallback and clipping may emit more.
7. **Do not repurpose painter passes as arbitrary occlusion groups.** Geometry that physically inter-occludes must generally share a pass; architected exceptions (background versus world, pre-split giant decks) must be explicit and image-tested.
8. **Stock hardware first.** 4 MiB cartridge, disc streaming, and second SH-2 are optional acceleration paths; a bounded stock-RAM baseline is mandatory.
9. **Mainline quality.** Clean up affected example code rather than leaving two competing planners or unused compatibility layers. Keep unrelated games and public behavior working; intentional breaking SDK changes require updating all in-tree callers and documentation together.
10. **Documentation is in English**, per `AGENTS.md`. Run commands yourself; do not ask the user to execute builds or emulators unless a genuine external blocker remains.

## 1. Verified starting points

Read these exact sources first, and revalidate against current `main` before editing:

| Existing subsystem | Location | Reuse / explicit gap |
| --- | --- | --- |
| Quantized distance fade + hysteresis | `include/saturn/fade3d.h`, `src/graphics/3d/rendering/fade.cpp`, `src/graphics/3d/rendering/fade_logic.hpp` | Already supports eight mapped slots and one caller-owned history byte per object. Extend by composition, not duplication. |
| LOD face-range selector | `include/saturn/skin3d.h`, `src/graphics/3d/animation/character.cpp` | `sat_lod_select()` selects against a single face budget; it does **not** manage world visibility or temporal LOD selection. |
| City planner | `examples/city_walk/world_lod.c`, `city_grid.h`, `render.c`, `residency.c` | Adapter supplies authored distance rings and residency to the generic planner; grid/chunk IO/collision remain game-specific. Preserve the pinned draw trace. |
| World scene submission | `include/saturn/scene.h`, `include/saturn/scene3d_faces.h` | AABB early reject; immutable topology binding; `sat_scene_submit_instance`; `sat_scene_stats` includes `world_commands`, capacity, culls, rejects and fallbacks. |
| Fade in a playable game | `examples/skybridge_3d/world.c`, `README.md` | Indexed materials/VDP2 priorities, supported eight-step blend, 2-unit state hysteresis, persistent temporal correctness fixes. |
| Large world / VDP2 | `examples/city_walk/`, `examples/vdp2_rbg0_ground/` | Camera-relative coordinates, streamed geometry, world budget; VDP2 sky/ground remove huge surfaces from VDP1. |
| Host simplification | `tools/model_pipeline/simplification.py`, `lod.py`, `metrics.py` | QEM/edge-collapse and quality gates; add static-scene partition/HLOD adapters without hard-wiring model import or runtime packaging. |
| Performance constraints | `docs/RENDERING_BIG_O_AUDIT.md`, `docs/TRANSPARENCY_ALPHA.md`, `docs/SKELETAL_ANIMATION_3D.md` | Preserve bounded scene painter complexity, actual VDP2 compositing semantics and memory discipline. |
| Build manifest | `cmake/LibSaturnSources.cmake` | Single source list for Make/CMake; update it when adding .cpp runtime files and run manifest tests. |

Baseline available from `city_walk`: 176/64/24 faces per 3 chunk LODs and a 500-face target frame budget, with eight fade steps over 64..128 world units. **These are example-specific settings, not universal defaults**. Its Ymir 30 fps result is **not** a stock-hardware guarantee; Ymir does not model the VDP1 pixel fill bottleneck completely.

## 2. Architecture and responsibility split

```text
offline assets: source geometry/materials/optional collision
        |
        +--> spatial partitions + validated LODs + optional HLOD/impostors
        |    + metadata (AABBs, geometric error, costs, asset IDs)
        v
game/streaming integration: residency state, generation, request priorities
        |
        v
sat_world_lod_*: visibility -> desired LOD -> resident fallback
                -> stable hysteresis -> deterministic budget governor
                -> fade request / material capability
                -> immutable frame draw plan and diagnostics
        |
        +--> optional prefetch suggestions (NO implicit IO)
        v
game binds chosen mesh/texture/instance
        |
        v
sat_scene_submit_instance(...) -> one shared painter -> sat_scene_flush(...)
        |
        v
scene stats + measured time -> bounded next-frame budget update
```

The runtime planner is not a renderer, a CD/cart allocator, or a scene graph. Keep a generic *visibility/budget planner* separate from the *hardware fade mapping* and the *offline asset compiler*. Game code maps logical asset IDs to `sat_mesh_t`, `sat_scene3d_material_t`, and `sat_scene3d_instance_t`. No pointers to transient descriptors may survive beyond the scene flush or outstanding workers.

### Candidate modules (adjust exact file names if needed)

- `include/saturn/world_lod3d.h`: public opaque-ish POD/caller-owned interface.
- `src/graphics/3d/scene/world_lod.cpp`: API adapter, validation, bounded planning.
- `src/graphics/3d/scene/world_lod_logic.hpp`: pure deterministic selection/math, host-testable.
- `include/saturn/world_lod_fade3d.h` **only if needed**: small explicit adapter from selected draw item to existing `sat_fade3d_slots_t` and material capability; avoid a second fade implementation.
- `tools/model_pipeline/world_hlod.py` (or equivalent): offline partition/LOD packaging, using existing simplification and metrics instead of rewriting QEM.
- `tests/host/test_world_lod3d.cpp`, `tests/tools/test_world_hlod.py`, and an original-assets example/probe, preferably extending `city_walk` with an opt-in A/B baseline.

Do not introduce every module upfront. Favor a small coherent set whose responsibilities remain independently testable.

## 3. Public runtime API contract (design before code)

Propose an API in `world_lod3d.h` with **fixed-width, versioned or explicitly sized caller-owned structs**. The following are conceptual fields, not mandatory names; define a clean, minimal ABI before implementation:

- `sat_world_lod_level_t`: immutable LOD ID, face/vertex count, upper-bound command estimate or risk class, texture bytes (if known), geometric-error proxy, residency token/slot + generation.
- `sat_world_lod_object_t`: stable object ID, local or camera-relative AABB/sphere, LOD table/span, object priority/importance class, hard-pin/near-safety flags, material fade capability, optional user tag. **No global absolute 16.16 world coordinates** that can overflow on large maps.
- `sat_world_lod_state_t`: caller-owned per-object last LOD, last drawable state, fade slot, state generation and optional cooldown; distinguish uninitialized from valid level 0.
- `sat_world_lod_view_t`: camera and viewport or conservative view/frustum representation; camera-relative fixed-point coordinates; movement-independent projected-size calculation.
- `sat_world_lod_budget_t`: face ceiling, conservative command admission ceiling, reserved scene/HUD margin, optional CPU-cost target, per-class minimum/priority; all independently enforced.
- `sat_world_lod_draw_t`: object ID, selected LOD, asset generation, fade request/slot, projected-size/depth, estimated face/command cost, fallback reason and painter group **hint** (not automatic override).
- `sat_world_lod_stats_t`: considered, frustum rejected, absent/not-ready, selected per LOD, coarsened, degraded by budget, pinned/protected, dropped, fades, over-budget failure, predicted cost.
- `sat_world_lod_plan(...)`: input immutable objects + mutable state + caller output arrays and scratch; no allocations. On `SAT_ERR_CAPACITY` or invalid arguments, do not publish a half-valid plan. Specify whether state commits atomically with output after full success.
- Optional `sat_world_lod_budget_feedback(...)`: stable bounded controller consuming actual `sat_scene_stats_t` and *measured* CPU/field time; no invented frame timing.

**Capacity and determinism:** validate object count, max LOD count, ID uniqueness (or use documented stable index), all AABB extents, face and texture counts, residency generation and integer overflow before writing persistent state. Output ordering deterministic under permutations or explicitly documented stable input order; tie breaks by stable object ID. Budget failure must be observable, not silently dropped inside `sat_scene_submit_instance`.

**Scene ownership:** planner returns a draw list. It never uploads textures, calls `sat_begin_frame`, submits VDP1 commands, sets VDP2 registers, or inserts an arbitrary extra painter sort. Application code calls existing `sat_scene_submit_instance` with established immutable-topology bindings where possible.

## 4. Desired LOD policy and stability

1. Measure projected footprint (AABB conservative radius/extent) and optionally offline geometric error in screen pixels, rather than only camera Euclidean distance. Use view-space depth and valid near-plane guards. Prefer fixed-point/64-bit intermediates with proven ranges.
2. Support asymmetric thresholds / **LOD hysteresis**: promote to more detail only after the closer threshold is crossed; demote only after the farther threshold. Add optional minimum dwell for rapid camera oscillations, without freezing LOD after teleport.
3. Always prefer the finest **resident and valid** LOD compatible with the desired quality and budget. When finer data is pending, allow a valid coarser fallback; never reference stale slot generations. Missing data should produce a prefetch suggestion rather than hidden disk work.
4. Preserve essential/pinned objects (player, near collision silhouettes, interactable landmarks); normal world scenery yields budget first. Pinning must not bypass arithmetic or command capacities: if an impossible protected baseline exceeds the budget, return an explicit error/policy outcome.
5. Degrade **farthest/least important** objects first, re-evaluating their actual next resident LOD costs; only then drop optional far objects. A "coarser" level is not assumed cheaper: reject malformed non-monotonic data or explicitly handle it. Limit degradations to configured maximum quality loss.
6. Never silently select an EMPTY LOD as an equivalent substitute for nearby architecture. Empty-detail LOD may be valid only for optional distant details with authored permission.
7. Prefer static/temporal actor bounds over per-frame per-vertex scans. Camera-relative origin rebasing must not cause state or visibility discontinuity.
8. Maintain manageable complexity `O(N * L + fixed bounded sort)` for N objects and small L; no repeated N^2 over-budget rescans, no per-face planning.

**No-pop acceptance:** switching a whole opaque LOD does not magically dissolve. Combine generous LOD distance, matching silhouette/geometric error thresholds, and separately budgeted detail subgroups that can appear/disappear with fade only when their material and background support it. Prefer an authored billboard/impostor at the farthest distances, validated from multiple angles. Do not render two full overlapping VDP1 LODs as a fake alpha crossfade.

## 5. Fade / VDP1–VDP2 composition contract

- Reuse `sat_fade3d_slot()` and the one-byte state/hysteresis path from `skybridge_3d`, do not implement parallel slot quantization.
- At the boundary of planning and material submission, classify candidate as:
  - indexed8 + suitable VDP2 background and priorities: request one of slots 0..7 (or opaque/culled);
  - RGB polygon, incompatible shading/texture format, or VDP1-only background: explicitly choose opaque, supported conservative distance tint/dither, or skip *only when authorized*. Do not pretend VDP2 blends with existing VDP1 surfaces.
- `SAT_FADE3D_SLOT_CULLED` must never be passed as a hardware color-calc slot.
- Ensure the VDP2 blend slots/priorities are configured once per appropriate video state; reproduce the `skybridge_3d` PRISA/commit-order temporal regression. Keep VDP1 framebuffer erase transparent where VDP2 must show through.
- No dynamic global per-object VDP2 ratio reconfiguration in the middle of a frame. Eight ratios are shared hardware slots, not eight private textures.
- Keep **fading optional** and expose a no-fade reference mode; the CPU budget planner should produce the same selected geometry in both for A/B correctness.
- Separate near/opaque inter-occluding world geometry from distant background layering. The painter's `pass` is not a substitute for proper world-space occlusion. Test Mario/pig-like avatar beside and behind walls from multiple camera angles.

## 6. Adaptive budget governor

Implement as an **opt-in, bounded and observable** controller, never an automatic hidden game policy.

- Inputs: `sat_scene_stats_t` (`world_commands`, `commands_capacity`, `budget_blocked_faces`, `clipped_faces`, `fallback_faces`, `rejected_faces`, `result`) plus measured FRT elapsed time, field count / VBlank interval. Do not use `0` profiling samples as evidence if profile metrics were compiled out.
- Distinguish VDP1 world command count, HUD reservation, CPU preparation time and raster/fill-rate risk. **An emulator with incomplete fill-rate modeling cannot certify hardware fps.**
- Start from a user-selected target (e.g. 30 fps / 2 NTSC fields) and a conservative maximum face/command budget.
- If persistent measured overload, degrade by small steps no more than once per cooldown window; under sustained headroom, recover **more slowly** than degradation. Use EWMA or bounded integer smoothing; handle the first frame, pause, reset, scene change, dropped frame, and telemetry wrap.
- Never solve a capacity failure by deleting essential near geometry, overrunning ring buffers, losing HUD reserve or reducing collision fidelity.
- Expose current target, selected budget, overload reason and transition counters to the demo HUD/harness; permit disabling the controller entirely.
- Keep lighting/transparency and LOD requests separate. Test fade enabled/disabled and governor enabled/disabled in all combinations.

## 7. Offline static-scene HLOD pipeline (host-only)

Build on `tools/model_pipeline/simplification.py`, `lod.py` and `metrics.py`; do **not** copy the QEM implementation or mix Python importer policy into the Saturn runtime.

1. Partition a static scene by authored components or spatial grid, clipping/splitting faces across cell boundaries where needed; reject unmatched geometry. Preserve consistent winding, UV corner order, material identity, silhouette and seam policy.
2. Generate LOD0/1/2 directly from the same original reference, not recursively simplified outputs. Preserve architectural silhouettes, doors, windows, edge trims and portals via importance/material annotations. Avoid concave quad merging or UV warps that VDP1 cannot map correctly.
3. Optionally bake architectural detail onto simple planar facade quads (as `city_walk` does), with measured texel density, draw-angle bounds, wrap semantics and near-plane fallback. HLOD/impostor generation is opt-in and isolated in a converter, not mandatory for all models.
4. Emit **metrics per cell and per LOD**: faces, vertices, source triangle coverage, texture bytes, largest VDP1 sprite extent, view-angle silhouette error, geometric error, AABB, original asset checksum, palette/UV validation and budget failures. Reject quality-gate failures instead of silently shipping them.
5. Provide deterministic, compact, endian-explicit runtime metadata, independent of game franchise. Respect fixed-point camera-relative bounds; demonstrate >181-unit logical worlds without overflow.
6. Collision models remain original or separately authored; changing render LOD must never change collision, trigger, or physics state.
7. Do not write any copyrighted demo/test assets into LibSaturn. Use a procedural miniature castle/towers/trees or other original synthetic fixtures.

## 8. Milestones — implement and commit in this order

### M0 — Baseline and reproducibility
- Record current `main` SHA and **run** available host gates for `test_city_walk`, `test_skin_present`, `test_scene3d_faces_api`, `test_fade3d` (confirm actual test names), plus a build and Ymir capture of `city_walk` and `skybridge_3d`.
- Store commands, toolchain, current baseline face/command counts, display cadence, known emulator limitations and screenshot hashes in a dated results document.
- If a run is blocked, document the actual blocker and run all available alternatives; never report a test not run as passed.

### M1 — Pure planner + capacity-safe API
- Introduce `world_lod3d.h` + pure logic, explicit state and caller-owned scratch.
- Implement conservative visibility/desired-size LOD, stable ordering, residency/generation fallback, far-first degradation and hard face budget.
- Host tests: zero/one object, 1/2/3/4+ LODs, invalid tables, non-monotonic face costs, capacity atomicity, out-of-frustum, negative coordinates, camera-origin rebase, tie determinism, empty LOD, teleport, pinned baseline failure.
- Prove no per-frame heap and no implicit VDP2/VDP1 dependency in the planner.

### M2 — Fade bridge and temporal stability
- Compose selected LOD with existing `sat_fade3d_slot`; ensure supported indexed geometry and lower-priority VDP2 backdrop.
- Test fade state reset on scene/object-generation changes, 1-LSB threshold jitter, camera reversal, teleport, LOD swap, priority register commit ordering and no-fade fallback.
- Visual A/B: no alternating opacity at a motionless camera; avatar/walls have correct occlusion; indexed geometry fades only where compositing is supported.

### M3 — Budget feedback and instrumentation
- Add optional governor with conservative command admission and measured field/CPU feedback.
- Host tests: stable load, transient spikes, persistent overload, recovery, frame pauses, profile-disabled, overflow and cap saturation.
- HUD/harness reports wanted/selected LOD distribution, frustum rejects, stale-residency misses, fade levels, dropped optional sectors, face budget, true VDP1 world commands and field cadence.
- No optimistic performance claim based only on command counts.

### M4 — Real consumer migration
- Integrate planner into `city_walk` and remove the now-duplicated budget/LOD policy from its `city_grid.h`; **retain** the game-specific grid/rings/chunk IO/collision implementation.
- Compare deterministic camera/walk traces **before and after** migration, with the *adaptive governor disabled* and legacy thresholds pinned: same visible cell IDs and LODs at the same inputs, unless an explicitly documented correctness fix changes the plan.
- Keep the per-chunk fade already used by the game; do not implement a second fade state machine. Run no-cartridge and cartridge-aware modes as available.
- Add an original-assets outdoor test (castle-like silhouette + ground + avatar) that exercises a free camera, occlusion, LOD transitions and VDP2 fade over many yaw/pitch angles.

### M5 — Offline HLOD + packaging
- Add static-scene HLOD pipeline and self-contained synthetic tests: UV seams, material cuts, window/door silhouette, cell-boundary crossings, texture limits, source checksum and independent read-back.
- Add a generated demo scene build rule with exact output byte accounting and 16.16 coordinate guards. No private ROM assets or generated media in the public repository.
- Test LOD source-to-variant image comparisons at fixed cameras (quantified view error + manual visual sign-off).

### M6 — Saturn acceptance, optimization and cleanup
- Run `cmake --preset host-tests`, relevant `make test` gates, Make/CMake manifest parity, SH-2 build and installed-package consumer. Use actual paths/commands from current docs; don't force unsupported presets.
- Build example with `build-example.ps1` and run `run-example.ps1` / Ymir harness with user-provided BIOS where available.
- Sustained moving-camera routes, stationary boundary jitter, rapid yaw reversals, approaching/receding walls, 4+ overlapping scenery cells, missing LOD during streaming, max face pressure, invalid textures and fade/no-fade toggles.
- Inspect screenshots at corresponding camera poses and consecutive frames; record true render submissions, VBlank/field pacing, VDP1 commands, fill-rate caveat, RAM/VRAM residency and priority register state.
- Remove obsolete planner duplication, update `docs/RENDERING_BIG_O_AUDIT.md`, `docs/TRANSPARENCY_ALPHA.md` only where behavior actually changed, examples README and public header docs.
- Every meaningful milestone is independently buildable and committed to `main`; don't defer tests to a final giant patch.

## 9. Hard acceptance gates

| Area | Pass condition |
| --- | --- |
| Generic API | Standalone C and C++ public-header compilation; no game-specific types in public headers; no heap or hidden IO in planner |
| Determinism | Identical visible draw-plan IDs/LODs for identical input and prior state; stable ties and no uninitialized memory |
| Correctness | Conservative AABB/frustum reject; no stale generations, fixed-point wrapping, invalid index/texture reads, partial result on failures |
| Cost | Selected estimated faces/commands stay within configured independent budgets; protected baseline overflow is explicit |
| Temporal | LOD threshold jitter settles; camera reversal/teleport reset cleanly; no entire far chunks abruptly appear when fade and background support transition |
| Fade hardware | Eight supported slots configured correctly; incompatible material case is explicit; no claim of blending two VDP1 polygons |
| Occlusion | Correct wall/avatar ordering from multiple views; no painter-pass hack globally overriding depth |
| Memory | Per-object state, arrays, scratch, texture and double-buffer ownership accounted for; no unbounded stack or additional per-frame alloc |
| Performance | Before/after command count, CPU prepare time, field cadence and screenshots reported; no emulator-only fps described as physical-hardware fact |
| Compatibility | Existing examples and focused tests green; `city_walk` streaming and fade behavior preserved except documented fixes |
| Repository hygiene | No private assets, no cloned duplicate SDK prefixes, source manifest updated, English docs and recorded verification commands |

## 10. Execution instructions for Codex

1. Read `AGENTS.md` first, inspect the current sources referenced above, and **verify the branch/head** before any modification. This document is an implementation request; **implement code, tests and example**, not merely another plan.
2. Start with **M0**, then implement **M1 through M6** in coherent, testable commits on **`main`**. Do not create a long-lived feature branch unless explicitly requested. Resolve ambiguities using the smallest clean reusable API.
3. **Do not copy `city_walk` wholesale** into `src`; extract mechanisms while keeping world-grid/ring policy in the example. Remove duplicate logic after migrating consumers. Avoid unused wrappers, unnecessary backwards-compatibility scaffolding and silent fallbacks.
4. Run appropriate host and SH-2 commands and emulator harness yourself after each milestone. Respect the project disk budget; prefer the existing build/install prefix. Explicitly report blocked hardware tests and do not claim they passed.
5. For any performance regression or visual artifact, record before/after metrics and fix the cause before continuing. A render budget that just hides nearby objects is not an acceptable optimization.
6. Publish a final change ledger with: commits, files, API synopsis, actual commands executed + results, screenshots/hashes, budget and frame metrics, remaining limitations, and the next measured bottleneck. No invented measurements.

## 11. Related source documentation

- [3D distance fade design](DISTANCE_FADE_3D_PLAN.md)
- [VDP1 / VDP2 transparency semantics](TRANSPARENCY_ALPHA.md)
- [Rendering Big-O audit](RENDERING_BIG_O_AUDIT.md)
- [City streaming plan](CITY_WALK_STREAMING_PLAN.md)
- [Skeletal animation and submesh LOD](SKELETAL_ANIMATION_3D.md)
- [SDK packaging and build workflow](PACKAGING.md)

This plan deliberately avoids coding castle-specific geometry or porting Nintendo assets into LibSaturn. The project-specific outdoor-world importer should consume these shared APIs after they are validated with original test data.
