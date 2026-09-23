# LibSaturn — Abstraction Spectrum and Breaking Refactor Master Plan

**Status (2026-09-23):** Started. First slice fixes asset registry path integrity/manifest preflight and the standard sprite screen-coordinate conversion, with host regression cases. All other items below are planned, not asserted complete. This document is a forward-looking contract; historical progress claims in older plans require reconciliation against current main.

**Policy:** Breaking changes are allowed across public headers, layouts, examples, tools, builds, and documentation. Remove displaced implementations and obsolete compatibility shims; preserve real hardware requirements, deliberate bare-metal access, bounded memory, and observable behavior. Each milestone must produce verifiable tests and usable examples. Do not treat a documentation-only change as completed implementation.

## 1. Non-negotiable philosophy: an optional spectrum of abstraction

LibSaturn must support several development styles equally, without forcing the engine onto bare-metal users:

- **L0: Direct hardware.** Explicit register/VRAM/CRAM/Sound RAM/DMA/Dual-SH2 control. No scene, asset registry, physics world, hidden frame loop, or mandatory heap.
- **L1: Independent primitives.** VDP1 command emission, VDP2 configuration, transfers, audio voices, CD, geometric and mathematical operations, and explicit task submission. The caller owns order and resources.
- **L2: Specialized reusable components.** Meshes, materials, animation, collision, physics, texture regions, streaming, view caches, render queues. Modules remain independently constructible and independently testable.
- **L3: Integrated optional runtime.** Scene, entities, camera, animation, physics, assets, global face sorting, overlay orchestration, and task scheduling, composed from the lower layers.

A lower layer MUST NOT depend on initialization, policy, data structures, or callback symbols from a higher layer. Static linking should pull in only required objects. High-level operations may adopt externally owned low-level resources through explicit borrowed/owned handles and lifetime rules. Raw VDP1 commands are never silently captured by the scene; mixing raw and scene submissions requires explicit frame passes and documented composition ordering.

**One implementation does not mean one API.** A direct draw path and a scene draw path may coexist, but geometry conversion, clipping, coordinate transforms, VRAM address math, and command encoding should have one shared implementation where semantics genuinely coincide. Global face sorting applies only to items submitted to the scene; raw draws retain caller-controlled order. Painter sorting cannot provide a general z-buffer or correctly resolve arbitrary intersecting polygons.

## 2. Ownership and dependency rules

- **SRP:** assets own identity/metadata; loaders own decoding; memory pools own allocation; textures own their GPU lifecycle; scenes own submission and visibility; renderer owns clipping, sort and emission; HAL owns register encodings and transfers. The scene coordinates but does not reimplement specialized services.
- **OCP:** introduce new colliders, geometry sources, material capabilities and backend-specific presenters through bounded typed data, static dispatch tables or narrow adapters. Avoid growing game-specific switches and avoid virtual dispatch where simpler C/static composition suffices.
- **LSP:** public capability and failure contracts must be equivalent across supported Master, Slave and software/fallback implementations. Do not falsely promise features a backend cannot provide.
- **ISP:** narrow public headers and independent component initialization. The umbrella header is convenience only. Keep internal painter keys, register shadow state and temporary face storage out of ordinary game-facing contracts.
- **DIP:** hardware drivers cannot invoke high-level audio/scene policy. A bounded cooperative-service hook owned by the runtime can service streaming during blocking CD waits without the CD driver knowing audio. Shared math belongs outside graphics so physics does not depend on rendering.
- **Memory:** explicit requirements/init APIs or caller-provided typed arenas; no mandatory heap, static initialization side effects, surprise per-frame allocations, unbounded data structures, hidden cross-SH2 cache sync or arbitrary copies.
- **Hardware:** keep numeric register values, bit fields, hardware widths and exact timing where required. Centralize and document their meanings, not every harmless zero/one. Separate hardware constants, validated algorithm limits, and user-configurable policy.

## 3. Cross-cutting contracts to freeze first

1. **Results:** distinguish submitted, prepared, culled, clipped, rejected for capacity, emitted, and deliberately skipped-as-unsupported. The frame exposes the first actionable failure and actual emitted-command counts; no unexpected error becomes a successful flush.
2. **Lifetimes:** define owned vs borrowed geometry/textures/palettes/materials, handles with generations, scene/frame scratch lifetime, and behavior when partially committed hardware transfers fail. Do not pretend that partial VRAM writes can always be rolled back.
3. **Coordinates/time:** one active resolution and one screen-to-native transformation used by all screen-space wrappers; explicit point-fixed conversion rules; shared clock for voice expiration and streams, with 50/60Hz modes supported only when video validation and hardware configuration genuinely support them.
4. **Capability:** document which features require VDP1, VDP2, Slave SH-2, RAM cartridge, or synchronization. Optional hardware must not become a requirement for primitive users.
5. **Constants:** name physical tolerances and geometry limits separately; check public painter-pass bounds before storing; represent CD command words and mask fields semantically. Never silently clamp user-facing render priorities.

## 4. Render and scene redesign

Build one optional L3 render plan for static cached faces, dynamic instances, meshes and specialized scene drawables: collect -> transform/animate only where needed -> cull/clip -> produce comparable painter keys -> globally order -> enforce budget -> emit to VDP1 -> explicit overlay/VDP2 composition. Cached projected geometry skips unnecessary work but enters the same global scene sort when used in L3. L1 direct draw stays immediate and independent.

Unify instance/material ownership and remove redundant per-batch sorting when the merge resorts globally. Provide bounded failure metrics and optional performance instrumentation. Preserve explicit low-level texture/command paths and document painter limitations for intersecting surfaces and VDP1/VDP2 layering.

## 5. Assets, texture transactions and storage

Normalize logical paths before comparing any registry entry and before publishing any manifest element. Preflight the entire bounded batch, including live-path collisions, capacity and invalid descriptors, so a rejected manifest does not partially register. Separate resource identity from residency (main RAM / VDP1 VRAM / SCSP). Split generic registry metadata from typed loaders and format-specific descriptors; design explicit borrowed-resource imports. Validate texture/palette updates before mutating logical state and expose a recovery state for irreversible partial hardware writes. Refresh only affected texture regions when hardware alignment permits.

## 6. Physics, audio, parallel, HAL

Split world stepping into integration, kinematic motion, broadphase, narrowphase/CCD, solver and pose update; make pair generation bounded and avoid repeating all-actor scans for each solver iteration. Separate voice manager, Sound RAM allocator, sound registry and audio clock. Remove the weak direct CD -> audio symbol dependency. Remove Skybridge-specific policy from generic parallel scheduling; make shared-buffer publication/invalidation and input/output ownership explicit, and compare Master/Slave semantic results. Keep HAL register writes isolated from rendering/game policies.

## 7. Delivery sequence and test gates

**M1 — Contracts and concrete integrity defects (in progress):** normalized asset duplicates, manifest preflight, active screen coordinates, painter-pass validation, accepted/skipped face counts, and direct-frame error reporting are implemented. The third slice shares a private scene-error contract across transformation lookup and asynchronous submission/merge. A valid pending task remains transient (SAT_ERR_BUSY); a failed task, invalid output, or rejected merge is recorded. Texture failure states now invalidate parent and all prepared regions on failed hardware writes and permit explicit full-update recovery. Palette rebinding now preflights without mutating the registry and commits only after a successful palette transfer; a failed transfer leaves previous logical ownership intact while marking the texture dirty. Rect updates now transfer affected VDP1 words/rows and only intersecting prepared regions, without reallocating VRAM. The scene now measures actual VDP1 world commands using a counter delta around the synchronous face flush, including partial emissions on a failed flush; this excludes earlier raw and cached-view commands. Next: hardware-side partial-update validation and canonical cache submission.

**M2 — Dependency isolation:** shared math out of graphics, cooperative blocking-I/O services, narrow headers, typed assets, bounded memory ownership. Test independent linking of L0/L1 clients without L3 runtime initialization.

**M3 — Rendering integration:** canonical L3 queue for cached/static/dynamic faces, consistent material/instance semantics, bounded command accounting and optional immediate L1 emission. Golden visual/occlusion tests, capacity/failure-injection tests, command-budget tests.

**M4 — Physics/audio/parallel:** per-batch conservative/runtime/Master geometry dispatch is implemented: the generic runtime no longer knows Skybridge's force-split compile flag, while the measured conservative AUTO default remains. Remaining work includes broadphase, audio/SCSP ownership, stronger SH-2 executor contracts and correctness/performance comparisons.

**M5 — Breaking migration and cleanup:** update every example/tools/docs to the new contracts, delete old entry points and duplicated engines instead of adding indefinite shims; isolate build artifacts by effective flags; ensure every host/tool test is discovered. Reconcile obsolete plan status sections.

**Every gate:** run host tests, Saturn cross-builds, representative example builds, available emulator checks, and collect command counts, memory budgets, and timing where relevant. Stock-hardware validation is a separate gate and must not be claimed without evidence. Do not publish a numerical speedup without measurements. A completed high-level feature must not break a standalone lower-level consumer.

## 8. Immediate first-slice implementation details

- Asset insertion scans every live slot for a normalized-path collision before choosing the earliest free slot; a hole cannot hide a later duplicate.
- Manifest preflight normalizes each path and checks all previous paths and live entries before inserting anything, keeping two bounded path buffers rather than allocating a large array on the Saturn stack.
- Screen sprite positioning consumes the active configured width/height and reuses the same screen-to-native helper as scaled sprites.
- Host regressions cover an earlier freed slot, different textual representations of one path, manifest non-mutation on rejected entries, successful valid manifests, and screen centering with two configured resolutions.

**Known limitation:** texture full updates expose NEEDS_RECOVERY and block drawing/partial updates after hardware failure. Palette preflight and commit protect logical ownership, but failed CRAM transfers may still have partially overwritten a sole-owner bank; this is not a hardware rollback. Rect updates now transfer intersecting ranges only, and may change caller-owned pixels before returning an error. Batch release has no scene argument and therefore cannot record its own failure in the frame; independently initiated low-level operations remain outside the optional L3 contract. Successful face dispatches are separately counted from physical VDP1 command deltas; cached-view replay remains immediate and is not part of that delta. Render-queue unification, PAL, partial-VRAM recovery, physics complexity, CD/audio coupling and any empirically adaptive geometry dispatch algorithm remain open.
