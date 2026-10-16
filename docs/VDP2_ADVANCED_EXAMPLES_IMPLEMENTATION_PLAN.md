# Advanced VDP2 Examples and Runtime Completion Plan

**Status:** Proposed, not implemented. This document is an execution plan, not evidence of a working feature.
**Date:** 2026-10-15
**Repository/branch:** `celsowm/libsaturn` / `main`
**Policy:** Implement coherent, tested slices directly on `main`, following `AGENTS.md`. No separate demonstration-only register hacks when a reusable public API is required. Documentation must stay in English.

## 1. Goal

Turn the remaining Sega Saturn VDP2 capabilities into tested, reusable LibSaturn facilities, demonstrated through **12 visually meaningful, interactive examples**. Preserve bare-metal control, deterministic fixed memory, and explicit hardware constraints. Build attractive original scenes, but treat correctness, reproducibility, frame timing and real-hardware compatibility as acceptance criteria rather than assumptions.

Avoid reimplementing the already-working `vdp2_layers_demo`, `vdp2_effects_demo`, `vdp2_compose_demo`, `vdp2_hires_light`, `vdp2_window_dim`, `vdp2_rbg0_ground`, `tate_fb_rotation`, and related demos. Reuse and extend their production APIs and regression tests.

**Not a shader engine:** The VDP2 has no general-purpose GPU shaders. Every proposed effect must map to documented VDP2 operations; any artistic approximation must be labelled as such.

## 2. Current baseline and inventory (2026-10-15)

Existing, usable code:
- `include/saturn/vdp2_layers.h`: NBG0-3, cell/bitmap, priorities, scrolling, scaling, cycle-pattern planning, RBG0 cell rotation parameter A, line/vertical-cell scroll, back/line-color screens.
- `include/saturn/vdp2_compose.h`: W0/W1 rectangular/line windows, sprite-window selection via palette sprite mode, mosaic, per-screen ratios, hi-res palette-sprite calculation, CRAM mode 0/1.
- `include/saturn/vdp2.h`: the older NBG0 and RBG0 bitmap interfaces, plus sprite priority/color-window helpers.
- `include/saturn/vdp2_alloc.h`: a stand-alone VDP2 VRAM free-list allocator and CRAM range allocator.
- `include/saturn/vdp2_color_calc.h` and `vdp2_color_offset.h`: current color calculation, additive modes, fades, offsets.
- `src/hal/vdp2/{nbg_logic,rotation_logic,compose_logic}.hpp`: host-testable register and layout policies.
- `tests/host/test_vdp2_*.cpp` and `harness/run-vdp2-*.ps1`: host and emulator checks.

Existing limitations or unvalidated opportunities:
- The modern RBG0 layer manager implements **rotation parameter A only**; more rotation modes/parameter-B switching need an explicit owner and test surface.
- RBG1 has no first-class public layer contract. RBG1 imposes restrictions on NBG coexistence, separate banks and RBG0 ownership.
- No public blur, extended color-calculation, full special-priority/color-calculation or VDP2 shadow feature family has been demonstrated end to end.
- VDP2 VRAM free-list allocations, layer bank/cycle ownership and raster/rotation table reservations are not yet a unified transactional resource planner.
- Video-facing APIs presently expose 224-line output, not PAL 240/256-line and interlace mode coverage.
- Physical Saturn compatibility, access timing and visual color/blur behavior are unverified. An emulator pass is not hardware certification.

**Documentation cleanup:** `docs/LIBSATURN_CURRENT_COVERAGE.md` contains obsolete claims such as “advanced raster effects NOT EXPOSED,” despite the later implementation in `vdp2_layers.h`. Update that matrix based on source and tests when completing the inventory.

**Build baseline blocker:** The latest Phase 3 CI cross-build gate failed while packaging `bullet_hell_stress` because `examples/bullet_hell_stress/assets/palette_bgr555_be.bin` was absent. Repair that independently before treating `make examples-all` as a green gate; never suppress the failure to ship VDP2 examples.

## 3. Hardware contracts and non-negotiable requirements

1. Read the vendored Sega VDP2 manual before defining each register bit: `docs/sega_saturn_hardware/hard/vdp2/hon/` (chapters 2, 3, 6, 8-13, 15-16). Cite section/page filenames in hardware-sensitive code comments.
2. **RBG1 is not NBG4.** The hardware requires RBG0 when RBG1 is displayed, excludes normal scrolling screens in that arrangement, and reserves separate VRAM regions for rotation image data. Do not draw both RBGs plus NBGs as though that were legal.
3. Validate VRAM-A0/A1/B0/B1 ownership, RAMCTL bank roles and cycle access patterns *before writing registers*. Transactional failure preserves the existing display; return explicit `SAT_ERR_CAPACITY`, `SAT_ERR_UNSUPPORTED` or `SAT_ERR_BUSY`.
4. Respect incompatible global modes: sprite type vs RGB sprites, CCCTL add vs ratio, blur vs extended color calculation and line-color insertion, mosaic vs vertical-cell scroll, high-resolution cycle pattern limitations, and CRAM requirements.
5. **Hardware blur** is valid only in normal TV modes with CRAM mode 0 and is incompatible with extended color calculation and line-color insertion (VDP2 manual `p12_13.md`, `p12_14.md`). Never emulate it with preblurred art while claiming a hardware test.
6. Separate **VDP2 shadow sprites** (palette encoding, shadow controls, target layer priority) from VDP1 framebuffer shadow/half-luminance. A VDP1 shadow comparison is a useful control, not a substitute.
7. Maintain the C-facing public ABI and reusable HAL/C++ policy; no runtime heap, unbounded allocations, virtual hardware simulation in production or duplicated VDP2 register ownership.
8. A register shadow must be owned by exactly one path, and VBlank commits must not tear. Line/rotation/coefficient tables must be double-buffered where necessary, aligned, and never overwritten while VDP2 is reading them.
9. Preserve builds and behavior of existing examples, especially `dino_demo`, `skybridge_3d`, `fighter_arena_3d`, `killer_clown_demo`, `vdp2_layers_demo`, `vdp2_compose_demo`, `vdp2_hires_light` and `bullet_hell_stress`.
10. All demonstration art must be original or clearly licensed for redistribution. Commercial Saturn games are visual references only; no extracted screenshots, textures or copyrighted assets shipped.
11. When an emulator does not implement a hardware feature, record **UNVERIFIED/UNSUPPORTED** instead of using a fake expected screenshot or asserting correctness.
12. Update examples, headers, build manifests, docs and host/harness tests in the same coherent commit as each new capability. No half-working example or unsupported public API.

## 4. Proposed architecture: one transactional VDP2 resource owner

Before complex showcases, plan and implement a small `VDP2 composition/resource plan` that composes existing low-level primitives rather than replacing them with a desktop scene graph.

- **Reservations:** explicit bank/byte ranges for pattern names, character/bitmap memory, rotation parameters, coefficient/line-scroll/window tables, palette ranges and scratch buffers. Detect overlaps, insufficient alignment, stale reservations and conflicting ownership.
- **Fetch schedule:** reuse the existing normal-layer cycle planner; add clear RBG0/RBG1 bank-use constraints and high-resolution rules rather than weakening the working allocator.
- **Register state:** one coherent shadow/commit path for RAMCTL, BGON, rotation, windows, priority, special effects and CRAM. Keep direct hardware access optional but externally conflicting configurations must fail predictably.
- **Capabilities:** query compatibility of requested displays/effects for the selected TV/CRAM mode before altering registers. Report a useful error for mutually exclusive effects.
- **Lifetime:** fixed-capacity handles or caller-owned descriptors, deterministic teardown and reclamation across scene changes, generation checks where appropriate.
- **Telemetry:** bytes reserved/free per VRAM bank, active screens, access-cycle budget, allocations refused, upload bytes/elapsed FRT ticks and last conflict reason.
- **Testability:** pure layout/register encoders and a host-model of acceptance/refusal; no fabricated runtime FPS.

Names and exact API structures must be chosen after auditing ownership in `src/graphics/vdp2/` and `src/hal/vdp2/`; do not prematurely introduce several competing public allocators. Document migration from existing `vdp2_alloc.h`, `vdp2_layers.h` and legacy `vdp2.h`. Breaking migrations are allowed if all in-repo consumers move together.

## 5. Twelve concrete example specifications

Each example shall have an independent `examples/<name>/main.c` (helpers as needed), standard `Makefile.inc` only when assets/build flags require it, a README with controls/limits, stable HUD counters, original assets, host tests for its new reusable logic, and a reproducible BIOS-bootable ISO/CUE.

| ID | Directory | Main demonstration | Library work / critical hardware constraint | Pass evidence |
|---|---|---|---|---|
| E01 | `vdp2_dual_rotation` | Two independently moving/rotating patterned planes, with A-only/RBG0 and dual-screen modes toggled by pad. Distinct patterns expose wrong bank mapping. | RBG1 ownership and safe transition; RBG0 + RBG1 only with documented exclusions for normal BGs; separate VRAM image banks. | Host rejects illegal NBG+RBG1 and overlapping banks; Ymir/Mednafen screenshots where supported; correct teardown. |
| E02 | `vdp2_rotation_coefficients` | An endless ocean/perspective floor; live changes to A/B parameter selection, coefficient mode, wave and horizon. | Modern rotation A/B register/table contract, parameter-switching modes and per-line/per-dot coefficients only where documented; double-buffered tables. | Host samples against independent reference math; smooth phase changes; no flashing/tearing or OOB VRAM. |
| E03 | `vdp2_special_color` | Layered light/glow test with normal ratio, add, extended blend and constrained special-color selections, plus side-by-side comparison scenes. | CCCTL/SPCTL special/extended color policies, legal per-screen CRAM and ratio selection; mutual exclusion with blur and high-res where needed. | Golden register encodings, invalid combinations refused; reproducible representative pixel/color checks. |
| E04 | `vdp2_hardware_blur` | A scene with distant background/foreground; press button to toggle genuine VDP2 horizontal blur. | BOKEN/BOKN ownership; CRAM mode 0; normal TV mode; reject line color + extended color conflict. | Compare registers and displayed region; negative tests for CRAM 1, hi-res and incompatible composition. |
| E05 | `vdp2_sprite_shadows` | Moving shadow silhouette over a scrolling ground; side-by-side VDP1 vs VDP2 shadow mechanism. | Proper VDP2 palette sprite shadow encoding, shadow selection/control, target-layer restrictions and sprite/RGB mode exclusivity. | Ground changes while unrelated pixels remain unchanged; host encoding cases; screenshot/physical checklist. |
| E06 | `vdp2_special_priority` | Scene with foreground vegetation/gates and moving sprites weaving across depth bands; toggle normal/special priority. | Special-priority handling for cell/palette color data where available; preserve existing 8-priority sprite API and RGB sprite mode. | Character/pixel pattern priority tests, legal composition, visual before/after capture. |
| E07 | `vdp2_dynamic_tile_stream` | Continuous race-road/side-scroller with live chunk swaps and no visible seam; choose update workload. | Bounded tile/map cache, non-overlapping double-buffered regions, DMA/CPU transfer selection, VBlank publication and CD/VFS integration if disc-backed. | 10-minute emulator stress, CRC of maps/chunks, no uninitialized tile, invalid DMA/cycle work refused. |
| E08 | `vdp2_vram_stress` | Four-bank VRAM/cycle HUD while configurations scale from legal to purposely oversized. | Resource planner diagnostics; fragmentation, CRAM/palette range, bank conflict/cycle selection and failure recovery. | Exhaustive host boundary cases; deterministic refusal without screen corruption, reclaim and reconfigure. |
| E09 | `vdp2_video_modes` | Interactive geometry/color-resolution test patterns; normal 320/352/640/704 and additional PAL/interlace modes as individually verified. | Explicit mode negotiation, TVMD + VDP1 framebuffer/CRAM compatibility, field timing, valid window coordinates. Implement PAL 240/256 and interlace only after manual/BIOS/host gates. | Mode matrix with NTSC/PAL/hi-res/interlace supported/unsupported; emulator captures; real-console acceptance pending. |
| E10 | `vdp2_parallax_world` | Original rich parallax forest/coast with moving clouds, waves, foreground masks, sky gradients. | Exercise existing NBG0-3, line-scroll/line-color, windows, RBG0 where legal, no duplicated HAL. | 60-second replay with deterministic scene and HUD; CPU/VDP1 savings measured against a comparable reference. |
| E11 | `vdp2_fighting_stage` | Stock-memory fighting backdrop: distant crowd, animated lighting, ring floor, sprites and dynamic coverage. | Integrate existing 3D fighter rendering with VDP2 composition; no assumptions of unsupported simultaneous screens or shadow types. | Measure VDP1 commands, 3D preparation time, visible frame cadence and painter correctness before/after, with identical fighter simulation. |
| E12 | `vdp2_neon_city` | Original cyberpunk night city with signs, windows, moving spotlights and reflective-looking ground. | Final integration of windows, CRAM color offsets, light/color calculation, tile streaming and rotation; document each actual hardware vs artistic approximation. | Long-running interactive scene; zero invalid registrations/uploads, validated composition, profiling report and hardware checklist. |

**Controls:** Standardize `D-pad` to move/select, `A` toggle the focal feature, `B` toggle debug overlays, `C` change submode, `START` reset. Document deviations when a scene needs its own camera/gameplay controls. The HUD must state active TV mode, CRAM mode, backgrounds, VRAM/cycle use, VDP1 command count (when relevant) and counts of rejected configurations.

**Performance reporting:** Record VBlanks, submitted frames, late swaps, SH-2 prep time and DMA bytes/timing where available. Do not label the VBlank frequency as measured scene FPS or extrapolate emulator timings to real hardware. Specific FPS targets are performance goals to be established from baseline measurements, not guaranteed hardware capabilities.

## 6. Milestones, dependencies and commit gates

### M0 — Baseline, docs and clean builds
- Confirm `main` baseline, inventory all public VDP2 APIs, supported hardware modes and existing examples.
- Fix the independently discovered missing SHMUP palette dependency without hiding it or deleting the sample; make `make examples-all` green.
- Correct the stale VDP2 coverage matrix. Add a machine-readable support/compatibility matrix (actual code vs manual features) and tests for its exposed constants.
- Record host test, package test, Saturn cross-build and emulator baseline, including existing failures.
- **Gate:** current tests pass, all existing examples build; no VDP2 feature claimed merely because it appears in documentation.

### M1 — Resource ownership and introspection (E08)
- Audit/merge VDP2 bank and allocation ownership; implement transactional layer+table reservations and readable diagnostics.
- Host tests for fragmentation, overlaps, bank roles, cycle overflow, commit and rollback.
- Complete `vdp2_vram_stress` before relying on the planner for other examples.
- **Gate:** error-path stability, no live allocations overwritten, existing VDP2 examples unchanged.

### M2 — Rotation features (E02 then E01)
- Complete RBG0 A/B selection / coefficient-mode support with pure host models and a live `vdp2_rotation_coefficients` scene.
- Add RBG1 with separate image memory, clear RBG0 dependency and NBG incompatibility; implement `vdp2_dual_rotation`.
- **Gate:** refused illegal combinations, no torn coefficient tables, distinct RBG pictures where emulators support them, physically testable acceptance artifact.

### M3 — Advanced composition (E03 then E06 then E05 then E04)
- Implement a single compatible state machine for special color/priority, shadow, blur and extended calculation. Add only verified public switches.
- Build each focused example independently with a visible A/B comparison; test mutually exclusive modes and palette/RGB mode transitions.
- **Gate:** host register encodings, per-example screenshots/observables, unchanged existing `transparency_showcase` and `vdp2_compose_demo`.

### M4 — Streaming and expanded video coverage (E07 then E09)
- Create bounded tile/map update scheduling with safe VBlank publication and stock-RAM accounting.
- Add supported TV modes only after hardware/manual validation, keeping normal existing modes unchanged.
- **Gate:** long-running chunk cycling; video mode matrix distinguishes tested, experimental and unsupported entries; no PAL timing regressions.

### M5 — Game-quality showcases (E10 then E11 then E12)
- Build `vdp2_parallax_world`, `vdp2_fighting_stage`, `vdp2_neon_city` using the shared APIs.
- Use original, redistributable scene assets and compare rendering/performance with realistic control scenarios.
- **Gate:** no example-local copy of general-purpose register or cache management; stable 10-minute runs; no stock-RAM overrun.

### M6 — Hardware signoff and developer experience
- Run/record physical Saturn tests across region/video mode, console revision and disc/ODE where available. Keep **PENDING** where no machine exists.
- Extend `docs/HARDWARE_ACCEPTANCE_CHECKLIST.md` with every VDP2 example; attach console/BIOS, mode, cable, screenshot/video, anomaly and capture hashes.
- Add indexed example list to README only for completed demos, and explain compatibility and limitations for outside users.
- **Gate:** no claim of “hardware proven” without a recorded physical run; all host/build/harness gates green.

## 7. Implementation workflow for every example

1. Inspect the latest `main`, `AGENTS.md`, related library files, the exact VDP2 manual section and the closest existing example.
2. Record an implementation matrix row: hardware register rules, incompatible modes, VRAM bytes/banks/cycles, chosen public API, expected failure code, artifacts and test coverage.
3. Implement **pure testable HAL policy first**, then the C-facing API and resource planner wiring; keep backend state single-owner.
4. Write host tests for success *and failure*: invalid color mode, VRAM overlap, out-of-range table, incompatible layers, illegal refresh mode, over-capacity and safe replay after error.
5. Create the original interactive scene and independent build; use `make EXAMPLE=<name>` or the project's scripts and run the appropriate emulator/harness yourself per `AGENTS.md`.
6. Capture deterministic telemetry and representative screenshots wherever the emulator supports the feature. State unsupported/unknown when it does not.
7. Re-run `make test`, affected baseline examples, `make examples-all`, package/build gates and the VDP2 acceptance harness. Do not silently downgrade failing tests.
8. Update example README, public docs and real-hardware checklist; commit the coherent unit to `main` with evidence and follow-up limitations.

**Recommended automation:** introduce a `harness/run-vdp2-advanced.ps1` command that accepts example name, NTSC/PAL, emulator and frame budget, writes machine-readable reports plus screenshots, and allows deterministic comparison with current VDP2 probes. Reuse existing harness scripts; do not invent an alternate emulator runner.

## 8. Formal definition of done

For each E01-E12, all boxes must be checked before changing its status to DONE:

- [ ] Scene compiles to a Saturn disc using the ordinary build.
- [ ] Every new hardware feature is implemented through a reusable, documented public LibSaturn API.
- [ ] VRAM/cycle/CRAM/register ownership and incompatible states are handled explicitly.
- [ ] Host regression tests cover encoded register values, edge capacities, teardown and invalid combinations.
- [ ] Interactive controls make the specific VDP2 capability visibly identifiable; no placeholder checkerboard-only completion.
- [ ] Emulator acceptance is reproducible, with named emulator/BIOS, mode, captures and telemetry. Unimplemented emulator features are labelled accurately.
- [ ] Measurements distinguish completed submissions from actually displayed frames and avoid physical-hardware claims.
- [ ] Stock RAM and VDP1/VDP2 memory budgets are tracked and no runtime heap is added.
- [ ] No regression in existing demo consumers or legacy supported graphics.
- [ ] All tests/builds required by the chosen slice pass on `main`; failing unrelated baseline gates are documented and addressed.
- [ ] Physical hardware remains explicitly PENDING until demonstrated.
- [ ] README, build manifest, feature matrix and hardware acceptance checklist are updated.

## 9. Out of scope

- GPU shaders, true Z-buffering, arbitrary per-pixel alpha between VDP1 primitives or a software renderer masquerading as native VDP2.
- Shipping copyrighted commercial game assets or claiming original games' exact visual techniques without evidence.
- Reimplementing already-complete NBG line-scroll/window/mosaic support just to create files.
- Promising 60 FPS on a workload before measuring a specific console or accurately modelled emulator.
- Declaring a feature complete because its register bits can be written but no viewable/testable example exercises them.

## 10. Primary sources and entry points

- Sega VDP2 manual: `docs/sega_saturn_hardware/hard/vdp2/hon/p02_20.md` (interlace), `p03_30.md` (VRAM cycles), `p06_20.md` (RBG0/RBG1), `p06_30.md` / `p06_40.md` (rotation/coefficient), `p08_20.md` (windows), `p09_10.md` / `p09_20.md` (sprite formats/priority), `p12_12.md` / `p12_13.md` / `p12_14.md` (extended color/blur), and the shadow/special-priority chapters as individually relevant.
- Runtime: `include/saturn/vdp2_layers.h`, `vdp2_compose.h`, `vdp2_alloc.h`, `vdp2.h`, `video.h` and their implementations under `src/graphics/vdp2/` and `src/hal/vdp2/`.
- Existing fixtures: `examples/vdp2_*`, `tests/host/test_vdp2_*.cpp`, `harness/run-vdp2-*.ps1`.
- Baseline acceptance: `docs/HARDWARE_ACCEPTANCE_CHECKLIST.md`, `docs/LIBSATURN_CURRENT_COVERAGE.md`.

**First actionable implementation slice:** repair baseline build; reconcile VDP2 resource ownership; deliver **E08 `vdp2_vram_stress`** with deterministic rejection/rollback tests. Then proceed to rotation and composition capability slices. 
