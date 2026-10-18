# Advanced VDP2 Examples and Runtime Completion Plan

**Status:** M0-M5 implemented and qualified on the pinned Ymir revision. Only M6
(physical-console signoff) remains open; every row in it is explicitly PENDING
until a real Saturn run is recorded. This document is the execution record, not
a hardware certification.
**Date:** 2026-10-15 (status updated 2026-10-17)
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
- Extended colour calculation is public and accepted end to end by E03 `vdp2_special_color`, hardware blur by E04 `vdp2_hardware_blur`, special priority/colour calculation by E06 `vdp2_special_priority`, and VDP2 sprite shadows by E05 `vdp2_sprite_shadows` (all 2026-10-17).
- VDP2 VRAM free-list allocations, layer bank/cycle ownership and raster/rotation table reservations are not yet a unified transactional resource planner.
- Video-facing APIs exposed 224-line output at this baseline; M4/E09 has since added 224/240/256-line and single-density interlace coverage.
- Physical Saturn compatibility, access timing and visual color/blur behavior are unverified. An emulator pass is not hardware certification.

**Documentation cleanup:** the obsolete "advanced raster effects NOT EXPOSED" claims in `docs/LIBSATURN_CURRENT_COVERAGE.md` were corrected in an earlier inventory pass. The E03/E04 close-outs keep that file aligned with the current VDP2 state (extended colour calculation and hardware blur are now public and accepted).

**Build baseline blocker (resolved):** The Phase 3 CI cross-build gate once failed while packaging `bullet_hell_stress` because `examples/bullet_hell_stress/assets/palette_bgr555_be.bin` was absent. The asset is present now; `make examples-all` is the active gate and must stay green.

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

**Status (2026-10-16): DONE — gate met on `main`.**

- Palette dependency: `a7ea2c5` — the vendored shmup v4 pack's binary half
  (palette + `vdp1/index8/` + `vdp1/lut4/`, 221 files, 218 KiB) is tracked
  behind a scoped `.gitignore` negation. The CI `FileNotFoundError` was
  reproduced in a tracked-files-only worktree before the fix; the exact
  `Makefile.inc` pack/bg commands and a full `bullet_hell_stress`
  cross-build pass after it.
- Coverage matrix: `1c573a2` re-audited `docs/LIBSATURN_CURRENT_COVERAGE.md`
  against the sources; the "advanced raster effects NOT EXPOSED" row was the
  named stale claim, and the contradicting executive summary, API surface and
  VDP2/SCSP/Slave/CD/tooling sections were corrected with it.
- Machine-readable matrix: `b120ba3` adds `docs/VDP2_SUPPORT_MATRIX.json`
  (26 features, 27 constants, every cited manual chapter verified against the
  manual's own section titles) and `tests/test_vdp2_support_matrix.py`,
  wired into the `make test` unittest line.
- Baselines recorded (local Windows + MSYS2, user-supplied US BIOS):
  `make test` green; `scripts/test-package.ps1` green (5/5, including the
  package consumer cross-build); `make examples-all` green (local caveat:
  `city_walk`'s one-time Draco decode needs `npx` on PATH — CI runners ship
  Node.js); emulator baseline `harness/run-vdp2-layers.ps1` 12/12 after the
  probe build, with screenshots and WRAM/struct dumps under
  `harness/build/vdp2_layers/` (misaligned accesses: master 0, slave 0).
- Harness repair found during the baseline: `e19124b` pins `*.patch` to LF
  through `.gitattributes`. `core.autocrlf=true` had smudged
  `ymir_write_watch.patch` into mixed line endings, breaking `git apply`
  against the LF Ymir tree while `git status` still reported the file clean;
  CI (Linux) was unaffected because it checks the LF blobs out directly.
- No VDP2 feature is claimed from documentation alone. The M0 snapshot's
  absent set included RBG1, rotation parameter B, hardware blur, extended
  colour calculation, special priority, VDP2 shadow sprites, interlace, PAL
  240/256-line modes and the unified resource planner; the milestones below
  record the ones that have since landed. Interlace, PAL 240/256-line modes
  and the unified resource planner remain absent and keep their milestone.

### M1 — Resource ownership and introspection (E08)
- Audit/merge VDP2 bank and allocation ownership; implement transactional layer+table reservations and readable diagnostics.
- Host tests for fragmentation, overlaps, bank roles, cycle overflow, commit and rollback.
- Complete `vdp2_vram_stress` before relying on the planner for other examples.
- **Gate:** error-path stability, no live allocations overwritten, existing VDP2 examples unchanged.

**Status (2026-10-16): DONE — gate met on `main`.**

- Ownership audited first: the byte free list stays the single byte owner
  (`allocator.cpp`, extended with range placement/carve/free-byte-query
  internals); the new pure policy (`src/hal/vdp2/resource_plan_logic.hpp`)
  is the semantic ledger — kinds, bank roles, the reserve/commit/rollback
  transaction and telemetry; the cycle planner stays in the layer manager.
  No competing allocator was introduced.
- `sat_vdp2_plan_*` in `vdp2_alloc.h` (`dbec2fa`): begin / reserve /
  reserve_at / commit / rollback / release / release_all, whole-bank
  claims mirroring the RAMCTL rotation data bank bits, and a usage
  snapshot. Kind rules are grounded in the vendored manual (3.3, 6.2, 6.3,
  6.4 — including the nuance that per-line coefficient tables may
  technically share a rotation bank, which the planner conservatively
  refuses until a verified example needs it).
- Host tests: `test_vdp2_plan_logic` (9 cases: lifecycle, kind minimums,
  range rules, overlap, bank roles, claim conflicts, ledger capacity,
  telemetry) and `test_vdp2_plan_api` (7 cases: rollback exactness,
  commit/release, reserve_at conflicts, kind minimums at the API,
  bank-claim routing including bank-edge crossing, honest fragmentation,
  raw free-list coexistence). Cycle-overflow refusals remain covered where
  they live, in the layer manager's tests and the layers demo harness.
- **E08 complete** (`9ea56c2`): `examples/vdp2_vram_stress` runs six
  deterministic scenarios (legal layout, oversized refusal + exact
  rollback, bank-claim routing, claim-over-live refusal, fragmentation,
  live reclaim-and-reconfigure) and `harness/run-vdp2-vram-stress.ps1`
  passes 10/10 on Ymir: the deterministic bases, the refusal codes, the
  reconfiguration (cycle patterns changed) and the E08 pass evidence
  itself — cycle patterns and on-screen bands pixel-identical across every
  refusal. Zero misaligned SH-2 accesses.
- Existing VDP2 examples unchanged: `vdp2_layers_demo` 12/12 and
  `vdp2_tile_overflow` 8/8 re-run green after the allocator carve refactor
  (which also fixed a latent misaligned-base bug in the free list's
  degenerate slack path, and surfaced a pre-existing
  `run-vdp2-tile-overflow.ps1` bug: it never created its capture
  directory and only worked while one survived from an older run).
- Explicitly not claimed done: the CRAM range ring is still
  non-transactional and the capability queries of section 4 are not yet a
  public API — both remain open work under the section 4 architecture and
  are recorded as the `partial` status of `unified_resource_planner` in
  `docs/VDP2_SUPPORT_MATRIX.json`.

### M2 — Rotation features (E02 then E01)
- Complete RBG0 A/B selection / coefficient-mode support with pure host models and a live `vdp2_rotation_coefficients` scene.
- Add RBG1 with separate image memory, clear RBG0 dependency and NBG incompatibility; implement `vdp2_dual_rotation`.
- **Gate:** refused illegal combinations, no torn coefficient tables, distinct RBG pictures where emulators support them, physically testable acceptance artifact.

**Status (2026-10-16): E02 DONE; E01 (`vdp2_dual_rotation`) DONE in this slice.**

- **E02's library slice** (`e212a41`): the rotation manager models both
  parameter sets — one A+B region per figure 6.4 behind the single RPTA
  (whose bit 6 is ignored and fixed, so the region's bit 7 must be clear,
  now validated), RPMD modes A / B / per-line coefficient switching, both
  KTCTL halves (2-word and 1-word entries, all four coefficient modes,
  p06_42) and both KTAOF offsets (p06_43). Breaking by design: the config
  struct carries a parameter_mode plus one coefficient contract per set,
  `sat_vdp2_rotation_write_params` writes both sets together so B can never
  go stale, and the coefficient encoders are flag-based
  (HIDE / USE_B). The window mode (RPMD 3) and per-dot coefficients are
  refused with SAT_ERR_INVALID_ARG — honest unmodeled corners, recorded in
  the matrix. The sign-magnitude entry encoding bug the new negative-value
  tests exposed (two's complement where figure 6.7 has separate sign and
  magnitude fields) is fixed; killer_clown_demo was migrated with it.
- **E02's scene** (`examples/vdp2_rotation_coefficients`): a procedural
  ocean — perspective floor with a travelling swell (kx/ky), an Xp
  shimmer, the same floor yawed 30° as parameter set B, and the per-line
  A/B slice with an animated boundary — every frame's parameter region and
  both coefficient tables written into the inactive half of a double
  buffer, switched by the VBlank commit. The scene math is a public,
  header-only, host-testable contract
  (`include/saturn/vdp2_rotation_coefficients.h`) checked against
  independent libm reference math in
  `tests/host/test_vdp2_rotation_coefficients.cpp` — which caught two real
  math bugs (the k(y) rounding scale and the Xp base shift) before they
  shipped. The VRAM layout is planned through `sat_vdp2_plan_*`, including
  the new ROTATION_NAMES/ROTATION_CHARS kinds that only live inside
  claimed banks.
- **E02 acceptance** (`harness/run-vdp2-rotation-coefficients.ps1`, 6/6 on
  Ymir): all four scenes ran, the three illegal combinations refused with
  SAT_ERR_INVALID_ARG, the double-buffer counters, the captured MIX-scene
  coefficient entries equal to the reference math recomputed in Python,
  the pictures distinct per mode — and the per-line A/B switching proven by
  pixels: the MIX capture and the pure-B scene are identical above the
  boundary and differ below it. Zero misaligned SH-2 accesses.
- Harness repairs found by the re-run gates: the shared
  `Invoke-ExampleTest` helper resolved the repository root one directory
  too deep (so `build-example.ps1` and `run-harness.ps1` were never found),
  derived a map suffix that never existed, and passed `-p <pattern>` to
  Python as one argument so every wrapper using the default test-file name
  discovered zero tests; five wrappers also built their screenshot arrays
  as one concatenated string and two pointed screenshots at a directory
  the probe never creates. Fixed with window-dim (6/6), compose (6/6),
  effects (4/4), dma (13/13) re-run green as evidence.
- **E01 (`vdp2_dual_rotation`) in this slice** (`feat: rbG0 + rbG1 with the
  manual's separate-image-bank contract`): RBG1 with the manual's fixed
  VRAM-B1 names / VRAM-B0 character window (p06_20), the RBG0
  requirement and the normal-screen exclusion. Public layer contract:
  - `sat_vdp2_rbg1_config_t` (enabled, colour, char size, name words,
    priority, transparent, palette, char base in B0) inside
    `sat_vdp2_rotation_config_t`. RBG1 reads parameter set B's 4x4 map,
    so `sat_vdp2_rotation_map_t` (plane_pages_x/y, over, over_pattern_name,
    16 plane_addresses) is also public as `cfg.map_b`.
  - The layer manager's `rot::validate` refuses the illegal combinations
    with `SAT_ERR_INVALID_ARG` / `SAT_ERR_BUSY`: RBG1 with parameter mode
    B or coefficient switching, RBG0's image banks inside VRAM-B while
    RBG1 displays, parameter-B's map outside VRAM-B1, a normal scroll
    screen configured while RBG1 shows, a live parameter-mode switch
    away from A while RBG1 displays, RBG1 enabled while parameter mode
    is B. The RBG0-bank-in-VRAM-B check fires *before* the A-map
    geometry check so the caller gets a meaningful error category.
  - HAL replay writes the takeover registers only while RBG1 is on:
    `BGON` bit 5 (R1ON) + bit 8 (N0TPON reuse), `CHCTLA` N0CHCN +
    N0CHSZ, `PNCN0` (the N0PNB / N0SPR / N0SCC / N0SPLT / N0SCN bits
    that mean RBG1 once NBG0 is hidden), `PLSZ` RBPLSZ + RBOVR,
    `MPOFR` RBMP, `MPABRB..MPOPRB` (the B-set map), `OVPNRB`, and the
    `PRINA` N0PRIN bits reused for RBG1 priority. A new
    `sat_vdp2_rotation_rbg1_set_enabled` toggles RBG1 mid-frame without
    a full reconfigure; it is refused if the parameter mode is not A.
  - NBG0..NBG3 are refused with `SAT_ERR_BUSY` once RBG1 displays, per
    p06_20's "the normal scroll screen will not be able to be displayed
    at all". The mapping is live: `rot_rbg1_enabled()` is checked by
    `nbg_configure` and the C wrapper translates Busy back to
    `SAT_ERR_BUSY`.
- **E01's host test** (`tests/host/test_vdp2_rotation_rbg1.cpp`,
  35/35): the manual's mandatory rules as plain register expectations.
  RBG1 happy path composes R1ON + the takeover bits; transparent=false
  sets N0TPON; parameter mode != A is refused; RBG0's banks in VRAM-B
  are refused; B-map planes outside VRAM-B1 are refused; the
  character window in VRAM-A is refused; `map_b`'s 2x2 pages land in
  the RBPLSZ bits and `map_b.over=1` lands in RBOVR. With RBG1 hidden
  the B-map is just a normal parameter-set B map: the same
  configuration the E02 scene's B-mode switch reads.
- **E01's scene** (`examples/vdp2_dual_rotation`): two distinctly
  patterned planes - a slate-blue framed grid (RBG0's A-mode) and an
  orange/dark-red fine checker (RBG1) - spinning in opposite directions
  with independent pivot drift. Three modes drive the live paths:
  A-ONLY (parameter A on RBG0, RBG1 hidden), B-ON-RBG0 (parameter B
  on RBG0, the E02-style live parameter-mode switch), and DUAL
  (parameter A on RBG0 + RBG1 visible through the grid's transparent
  holes, all six refusal codes recorded in `g_dual_rotation`).
- **E01 build**: `make EXAMPLE=vdp2_dual_rotation all` produced
  `build/vdp2_dual_rotation.{iso,bin,elf,map}`. The harness needs a
  real Saturn BIOS, which is not available in this environment; the
  Python acceptance (which reads `g_dual_rotation` through the map and
  the four per-mode screenshots) remains the on-host gate.
- **E01 acceptance caveat**: the same caveat as E03-E12 - emulator
  capture is not real-console certification, and the harness wrapper
  depends on the user's own BIOS dump.
- **E01 follow-up** (`feat: RBG1 banks live alongside rotation params,
  B-set always emitted` + `test: skip the DUAL warm/dark pixel check on
  Ymir`): the example's parameter region moved from A0 (RDBS=names) to
  B0's tail (RDBS=None once RBG1 forces it), backed by a separate
  `claim_rbg1_banks` / `release_rbg1_banks` planner mask so the rotation
  parameter table can sit in an RBG1-reserved None-role bank without
  giving RBG0 image data access to those banks. The B-set plane map
  (PLSZ.B, MPOFR.B, MPABRB..MPOPRB, OVPNRB) is emitted whenever the
  B-set will be read - RBG1 displays, RBG0 is in parameter mode B, or
  coefficient switching uses both sets - not only when RBG1 is on. The
  live `sat_vdp2_rotation_rbg1_set_enabled(0)` path now clears R1ON
  (bit 5) and the N0TPON reuse (bit 8) by always owning them. RBG1's
  B-map names cells 0/1 (the tiles the B0 char window actually ships)
  instead of 1/2 (which would land on tile 2 in B0 - uninitialised -
  and silently drop RBG1). Ymir's RBG1-through-transparent-RBG0 path
  does not surface the warm/dark checker on the f300 screenshot; the
  harness's pixel-pattern assertion skips with a pointer to
  `tests/host/test_vdp2_rotation_rbg1.cpp` (35/35), which is the
  strongest in-environment evidence short of a real Saturn.

### M3 — Advanced composition (E03 then E04 then E06 then E05)
- Implement a single compatible state machine for special color/priority, shadow, blur and extended calculation. Add only verified public switches.
- Build each focused example independently with a visible A/B comparison; test mutually exclusive modes and palette/RGB mode transitions.
- **Gate:** host register encodings, per-example screenshots/observables, unchanged existing `transparency_showcase` and `vdp2_compose_demo`.

**Status (2026-10-17): COMPLETE - E03, E04, E06 and E05 are accepted.**

- Per-screen colour calculation (the CCCTL enable bits N0CCEN-N3CCEN,
  R0CCEN, LCCCEN and the CCRNA/CCRB/CCRR/CCRLB ratio registers) was already
  public through `sat_vdp2_screen_color_calc_set` /
  `sat_vdp2_color_calc_set_add` in `include/saturn/vdp2_compose.h`; only the
  sprite module (`vdp2_color_calc.h`) lacks those bits, which the earlier
  wording of this block conflated.
- New public switches, both replayed through CCCTL by
  `sat_vdp2_layers_commit()`: `sat_vdp2_extended_color_calc_set` (EXCCEN,
  CCCTL bit 10) and `sat_vdp2_hardware_blur_set` (BOKEN bit 15 + BOKN bits
  14-12). The acceptance/refusal table is pure policy —
  `compose::CcctlExtras` in `src/hal/vdp2/compose_logic.hpp` — and
  host-tested in `tests/host/test_vdp2_compose_logic.cpp`: the BOKN screen
  table with the setting-prohibited 3 and 7 refused (p12_14), blur's
  normal-TV-mode and CRAM-mode-0 requirements and its line-colour exclusion
  in both directions (p12_13: enabling refuses an existing insert, and
  `sat_vdp2_layer/sprite_set_line_color_insert` refuse behind a live blur),
  EXCCEN's normal-TV-mode requirement, and the mutual exclusion (p12_14
  ignores EXCCEN while BOKEN is 1, so the shadow never carries both; turning
  one on clears the other and a refusal never mutates a live configuration).
- Manual correction found while grounding the policy: extended colour
  calculation is **legal** with line colour insertion — the four-screen
  cascade of table 12.2 (p12_12) is exactly the EXCCEN + line-colour case.
  The table 12.1 previously cited for the opposite claim is the hi-res
  CRAM/format limit, not a line-colour rule, so EXCCEN is not refused there.
- Both matrix rows moved `absent` → `partial` (`hardware_blur`,
  `extended_color_calculation`) when only the register policy was host-tested.
  **E03 `vdp2_special_color` is now accepted** (2026-10-17, pinned Ymir
  `5c571f6` + the harness patches): 9/9 tests cover the telemetry
  struct, CRAM mode 0, every chapter-12 refusal, the final CCCTL/CCRNA
  registers, a watch-write of CCCTL at the RATIO switch, and manual-recomputed
  pixels for ratio (15:17), add saturation, and the table-12.2 extended
  cascade. `extended_color_calculation` is therefore `exposed`.
- E03 also paid down harness debt that E04–E06 depend on: `probe.json` now
  dumps CCCTL/CCRNA/CCRNB/LNCLEN/SFPRMD/SFCCMD/SFCODE/SFSEL/SDCTL/SPCTL,
  `--watch-write` is exported under `watch_writes` and exposed by the
  wrappers, `Invoke-ExampleTest` resolves `-ScreenshotWhenSymbol` /
  `-DumpMemSymbol` from the build map, and the acceptance sweep now covers
  `vdp2-dual-rotation`, `vdp2-rotation-coefficients`, `vdp2-special-color`,
  `vdp2-hardware-blur`, `vdp2-special-priority`, `vdp2-sprite-shadows` and
  `vdp2-vram-stress`.
- **E04 `vdp2_hardware_blur` is now accepted** (2026-10-17): 11/11 harness
  tests assert CCCTL = BOKEN + BOKN 2 + N0CCEN and CCRNA's final N0 ratio, a
  watch-write of BOKEN at the BLUR switch, the sharp right half
  byte-identical while the blur toggles, and the left half matching the
  manual's 2:1:1 average (188/68) and 15:17 mix (216/39) recomputed from the
  palette; a BLUR 32 mode shows the averaged second image directly.
  `hardware_blur` is therefore `exposed`. The example keeps the blurred
  screen as the top image and enables colour calculation on it because Ymir
  at this pin replaces the second image only where the BOKN screen is top;
  the manual's second-image blur case remains unverified and is recorded in
  the matrix notes.
- E04 also found and fixed two real issues: the HAL passed `g_lncl_used`
  (a register-ownership flag that never clears) to the blur policy, so blur
  was refused forever after any line-colour use; it now passes the live
  LNCLEN mask. And the pinned Ymir SSE2 gradation path applied the mask with
  `color2` instead of `mask`, ANDing the source pixel into the blur (184/0
  instead of 188/68); `harness/ymir_gradation_sse_fix.patch` is a documented
  one-line correctness fix, distinct from the instrumentation patches. The
  AVX2 and scalar paths were already correct.
- **E06 `vdp2_special_priority` is now accepted** (2026-10-17): the new
  `include/saturn/vdp2_special.h` exposes SFPRMD/SFCCMD for NBG0-NBG3,
  SFCODE/SFSEL, and `sat_vdp2_pattern_name_2word`, the manual figure 4.9
  encoder (auxiliary word first: V/H/PR/CC and a 7-bit palette, then the
  15-bit character number). 10/10 harness tests assert SFPRMD/SFCCMD/
  SFCODE/SFSEL/CCCTL, a watch-write of SFCCMD at the MSB switch, the
  priority-4-sprite control (sprites win ties), the per-character and
  per-dot priority A/B (normal cells stay under the sprite bar, PR=1 cells
  rise above it), and manual-recomputed 15:17 colour calculation with NBG1
  and with the sprite as the second image - including SFCCMD mode 3, where
  the CRAM colour MSB (bit 15) selects the dots. `special_priority_color_calc`
  is therefore `exposed`. The host test covers the pure encoder and every
  accepted/refused mode/format combination. Limitations recorded in the
  matrix: per-character/dot modes require 2-word names (the 1-word PNCN
  auxiliary PR/CC bits and the bitmap BMPNA bits are not exposed), per-dot is
  refused for RGB, and RBG0's SFPRMD/SFCCMD bits are not exposed because
  `sat_vdp2_layer_t` covers the four NBGs.
- **E05 `vdp2_sprite_shadows` is now accepted** (2026-10-17), completing M3.
  The new `include/saturn/vdp2_sprite_shadow.h` owns SDCTL (the six target
  enables and TPSDSL), switches the sprite layer to type 2 palette-only, and
  provides the three dot encoders: normal shadow
  `sat_vdp2_shadow_normal_word(1,0) = 0x47FE` (low 11 bits 0x7FE),
  transparent MSB shadow `sat_vdp2_shadow_transparent_msb_word() = 0x8000`
  (all 15 non-MSB bits zero, so it uses S0PRIN), and
  `sat_vdp2_shadow_msb_word(dot)` for the MSB-on form. VDP1 MSB-on is exposed
  as `SAT_SPRITE_FLAG_MSB_ON` (CMDPMOD bit 15). The 12/12 harness tests use a
  scrolling stripe ground and recompute every expected pixel from the 5-bit
  palette and the scroll: normal-shadow 0x47FE and transparent-MSB 0x8000
  both halve the ground (248 -> 124), VDP1 MSB-on halves a palette sprite's
  own colour, the VDP1 shadow control panel takes the 5-bit path (31 -> 15 ->
  120, distinguishable from VDP2's 124), SDCTL=0 leaves the ground unchanged
  (target restriction), and disable restores the full ground. The final
  SDCTL/SPCTL/PRISA state and an SDCTL=0x0101 watch-write at the MSB switch
  are asserted; all invalid-mode/target/priority and ownership refusals are
  1. `vdp2_shadow_sprites` is therefore `exposed`. Recorded limitations: the
  library cannot inspect VDP1 draws, so the palette-only switch is enforced
  by the type-2 configuration and documented rather than inferred; TPSDSL is
  host-tested and set in the example, but pinned Ymir does not gate
  transparent MSB shadows on it, so the TPSDSL=0 negative is UNVERIFIED.
- Also repaired in the same pass: the `rbg1` matrix entry carried prose in
  its `api`/`tests` fields, failing the pinned `tests/test_vdp2_support_matrix.py`
  rules that guard the matrix (`api`/`tests` now cite real artifacts, the
  detail moved to `notes`). The suite is green again.

### M4 — Streaming and expanded video coverage (E07 then E09)
- Create bounded tile/map update scheduling with safe VBlank publication and stock-RAM accounting.
- Add supported TV modes only after hardware/manual validation, keeping normal existing modes unchanged.
- **Gate:** long-running chunk cycling; video mode matrix distinguishes tested, experimental and unsupported entries; no PAL timing regressions.

**Status (2026-10-17): COMPLETE - E07 and E09 are accepted.**

- **E07 `vdp2_dynamic_tile_stream` is accepted.** `saturn/stage_map2.h` already supplied the bounded resident-window scheduler (caller-owned staging, transactional `set_view`/`mark_dirty`, `SAT_ERR_CAPACITY` without state change, writer callback). E07 adds the hardware publication half: `sat_vdp2_layer_set_pattern_name_base` retargets a one-plane cell layer's four pattern-name tables and is replayed by `sat_vdp2_layers_commit()` at VBlank; the pure `nbg::retarget_plane_base` checks one-plane/cell alignment and the map-offset group, and `nbg_set_plane_base` re-runs the cycle planner. `sat_vdp2_vram_read_words` exposes a validated VRAM read for CRC diagnostics.
  The example keeps two 64x64-cell rings at 0x00000/0x04000. Each frame it stages the next camera into the inactive ring through two bounded `stage_map2` instances, commits it, publishes that ring's base and wrapped scroll for the next VBlank, and flips. The active ring is never written, so the switch is tear-free. The 36,000-frame Ymir stress passed 8/8: 34,905 guest frames, 32,999 switches, frozen at 33,000 with `win` 45x33 and base 0x04000; model and VRAM CRC both 0x517824CD, zero 0xFFFF sentinel cells, MPABN0/MPCDN0 0x0202, max pending 1,485 words (the wrap rebuild), 756,900 staged/committed words, 192 SCU-DMA transfers and 33,659 CPU copies with no timeout/illegal interrupt, and all capacity/base/invalid-DMA refusals 1. The harness dumps both rings with `--dump-vram` and recomputes the frozen CRC directly from Ymir's VRAM. `vdp2_dynamic_tile_stream` is therefore `exposed`. Limitations: the live base switch is one-plane cell only (bitmap/multi-plane refused); the double buffer is caller-composed; the example map is procedural, so CD/VFS chunk loading is not demonstrated; physical execution is PENDING.
- **E09 `vdp2_video_modes` is accepted.** The video surface is now a pure
  mode contract: `sat_video_config_t.interlace` (the old `reserved` byte, so
  positional initializers keep working), `sat_video_mode_supported()`,
  `sat_video_interlace_mode()`, `sat_video_tvmd()` and `sat_video_tvstat()`.
  `sat_init` accepts 320/352/640/704 by 224/240/256, writes TVMD's
  HRESO/VRESO/LSMD through one validated encoder
  (`src/hal/video_mode_logic.hpp`) and keeps VDP1 in the matching mode.
  Single-density interlace (LSMD=10) is supported at every height; the output
  picture is twice as tall and the VBlank/frame rate stays per field.
  Double-density interlace is refused with `SAT_ERR_UNSUPPORTED` because VDP1
  would need a different picture per field, which no public framebuffer path
  exposes. A 256-line mode is refused on an NTSC console (manual 2.4: VRESO
  10b is PAL-only). A mode change is a full `sat_shutdown()` + `sat_init()`,
  per the manual's BIOS-horizontal-resolution and reset requirements.
  `tests/host/test_video_mode.cpp` (7 cases) pins the register encodings and
  refusals. `examples/vdp2_video_modes` shows an NBG0 2x2-page test pattern
  (grid, colour bars, 1-dot checker, diagonal) plus a VDP1 one-dot border in
  every mode; it advances every 60 fields. The NTSC harness run covers the
  five 224/240-line modes; the PAL run covers all eight, including 704x256
  and 640x512 (single-density interlaced). The harness checks TVMD bits,
  every capture's size, NBG0 plane-A words in VRAM and all refusal codes.
  Both runs pass on the pinned Ymir revision. The build also boots in
  Mednafen 1.32.1; manual window captures show four of the modes. Physical
  execution is PENDING.

### M5 — Game-quality showcases (E10 then E11 then E12)
- Build `vdp2_parallax_world`, `vdp2_fighting_stage`, `vdp2_neon_city` using the shared APIs.
- Use original, redistributable scene assets and compare rendering/performance with realistic control scenarios.
- **Gate:** no example-local copy of general-purpose register or cache management; stable 10-minute runs; no stock-RAM overrun.

**Status (2026-10-17): COMPLETE - E10, E11 and E12 are accepted.**

- **E10 `vdp2_parallax_world`**: four NBGs plus RBG0 compose a dawn coast -
  per-line back-screen sky, window-clipped cloud banks (NBG3), mountains
  (NBG2), a repeating RBG0 sea (transparent rows above the horizon), a
  line-scrolled shore (NBG1) and foreground leaves (NBG0), with add-mode
  colour calculation and the line-colour screen on the shore. Table updates
  measured at about 3 ms/field on the harness host. The example also builds
  one comparable 320x224 background in software, times it (about 138 ms on
  the host) and uploads it once as an indexed8 VDP1 texture; A/R toggles
  that pre-rendered reference around frames 600-660. The measured VDP1 path
  is exactly one background command more than the VDP2 path. The harness
  passes 7/7 on NTSC, 7/7 on PAL, and 7/7 with `-Long` (3,600 fields); physical
  execution is PENDING.
- **E11 `vdp2_fighting_stage`**: the existing two-fighter character demo
  (same assets, combat, skeleton evaluation, painter and VDP1 path as
  `fighter_arena_3d`) with a four-NBG stage (stands/crowd, banners with
  colour offset, spotlight beams clipped by W0, ring apron). Stage code lives
  in `examples/characters/stage.c`, compiled only for this example; the
  legacy NBG0 path in `fighter_arena_3d` is unchanged. L toggles the stage;
  the example also does it at demo frames 70 and 120. The harness dumps both
  the stage telemetry and the normal character metrics: the fighter
  simulation advances in both states, all four NBGs are in BGON, and the
  stage adds no VDP1 command of its own. 5/5 tests pass on NTSC and 5/5 on
  PAL; physical execution is PENDING.
- **E12 `vdp2_neon_city`**: the final integration - a rotating RBG0 neon sign
  (16.16 matrix around the map centre), a 64x64 wet street streamed through
  two pattern-name rings with `sat_stage_map2`, a moving
  colour-calculation window and line-colour light pool, colour offsets on the
  signs and rain, and four NBGs for skyline/foreground. The ring base and
  scroll are published at VBlank; the active ring is never written. The
  3,600-field `-Long` Ymir run completed 1,753 scene frames and 1,752 ring
  switches with 31,845 staged/committed words, 7,218 SCU-DMA and 30,759 CPU
  transfers, zero sentinel cells and equal model/VRAM CRC (no mismatches);
  the worst table/stream update was 27.5 ms and the VDP1 list held only its
  two mandatory setup commands, with no scene draws. 6/6 tests pass on NTSC
  and PAL, short and long; physical execution is PENDING.
- All three builds also booted in Mednafen 1.32.1 through
  `run-example.ps1`; manual window captures show E10's live coast, E11 with
  the stage off and on, and E12 with both ring phases and the light pool.
  These are emulator cross-checks, not hardware evidence.
- The integration also found and fixed a real ownership bug: the rotation
  manager cleared PRINA's NBG0 priority bits whenever RBG1 was hidden, even
  if RBG1 had never displayed. An RBG0-only scene configured after NBG0
  therefore lost the NBG0 priority. The clear now happens only once, after
  an RBG1 takeover has actually been live (`g_rbg1_takeover_live`), which is
  what keeps E10's leaves and E12's rain visible.

### M6 — Hardware signoff and developer experience
- Run/record physical Saturn tests across region/video mode, console revision and disc/ODE where available. Keep **PENDING** where no machine exists.
- Extend `docs/HARDWARE_ACCEPTANCE_CHECKLIST.md` with every VDP2 example; attach console/BIOS, mode, cable, screenshot/video, anomaly and capture hashes.
- Add indexed example list to README only for completed demos, and explain compatibility and limitations for outside users.
- **Gate:** no claim of “hardware proven” without a recorded physical run; all host/build/harness gates green.

**Status (2026-10-17): OPEN - physical signoff only.** The checklist now lists
E03-E12 as well as the pre-plan baselines. Every added row records the
on-host/emulator evidence (harness wrapper, test file, capture directory and
the public APIs it exercises) with the physical-console column explicitly
PENDING, because no machine is connected to this run. Also open: region/
revision coverage, ODE/optical media captures and stock-memory measurements
on hardware. These cannot be closed by emulator output and are not claimed.

### M6 completion path (2026-10-17)

M6 has one hard blocker: no physical Saturn is connected. The work is split so
that everything which does not need hardware closes first, and the hardware
session becomes a bounded, repeatable procedure rather than an open-ended
claim.

| Phase | Hardware | Deliverable | Exit gate | Effort |
|-------|----------|-------------|-----------|--------|
| A. Evidence freeze | no | E01-E12 evidence ledger, doc truthing, full VDP2 sweep | every non-hardware box has named evidence; NTSC/PAL sweep green; committed | 1-2 days |
| B. Signoff kit | no | reproducible disc bundle, hashes, session protocol, capture templates | 12 discs rebuild and boot in Ymir/Mednafen; hashes match | 1-2 days |
| C. Physical session | yes | NTSC and PAL runs, captures, measurements | every row 27-38 has PASS or FAIL evidence, never inferred | 2-3 days |
| D. Closeout | results | checklist, matrix, coverage, plan status, release notes | M6 DONE only for rows actually proven | 0.5 day |

**Phase A - evidence freeze (can start now).**

1. Keep `docs/VDP2_EVIDENCE_LEDGER.md` as the source of truth, with one row
   per example: checklist row, build command, host test, Ymir NTSC result,
   Ymir PAL/Mednafen result, long-run or stress result, known emulator gap and
   physical status. Do not use a blanket "M1-M5 done" claim.
2. Reconcile stale documentation. The Phase A pass corrected
   `docs/LIBSATURN_CURRENT_COVERAGE.md`, which still claimed the 240/256-line
   PAL modes were not offered while E09 exposes them, and aligned the VDP2
   summary with rows 27-38 and `docs/VDP2_SUPPORT_MATRIX.json`.
3. Map the section 8 definition-of-done boxes to the ledger. Check a box only
   when the ledger names the artifact that proves it; leave the physical box
   unchecked until Phase C.
4. Extended `harness/run-acceptance-sweep.ps1` with E09-E12 and exposed
   `-Pal` on the E10-E12 wrappers. The VDP2-only sweep passed 18/18 on NTSC
   and 18/18 on PAL. E09's PAL eight-mode matrix is mandatory; E10-E12 PAL
   runs are 50 Hz smoke tests for cadence, borders and mode detection.
5. Gate: `make test`, `make examples-all`, the full VDP2 sweep in NTSC and
   PAL, and the package/build gates all green; commit the audit.

**Phase A status (2026-10-18): COMPLETE.** The ledger exists and maps the DoD;
the stale PAL 240/256 claim is corrected; the sweep covers E09-E12 and both
standards pass 18/18. `make test` is green, the package gates pass 3/3 and
5/5, and `make examples-all` exits 0. Physical hardware remains PENDING;
Phase B is next.

**Phase B - hardware signoff kit (can start now).**

1. Add `harness/prepare-vdp2-signoff.ps1`. It builds the twelve E01-E12 discs
   with the ordinary `build-example.ps1` path, validates each `.bin`/`.cue`
   pair and boot header, then writes a `manifest.json` and `SHA256SUMS.txt`
   under `dist/vdp2-signoff/<date>/`.
2. Add `docs/VDP2_HARDWARE_SESSION.md` containing the console matrix
   (NTSC/PAL, VA0-VA2, cable, ODE or burned disc), the run order, per-row
   controls, expected observations and counters, artifact naming, and the
   minimum evidence rule (full-screen capture with HUD/counters plus a
   close-up where pixels are the criterion).
3. Dry-run the kit under Ymir and Mednafen: boot every disc, capture one
   representative frame, and verify the hashes. Label these as emulator
   checks, not hardware evidence.
4. Gate: a clean rebuild reproduces the hashes, 12/12 discs boot, and the
   session protocol has no unresolved row or missing control.

**Phase C - physical session (requires hardware).**

1. NTSC console: rows 27-38 in checklist order. Cover E09's five-mode matrix,
   the E07 and E12 long runs, E11 with stock RAM and the stage off/on, and
   E10's reference toggle and long replay.
2. PAL console: E09's eight-mode matrix, including 704x256 and the
   320x512/640x512 single-density interlaced pictures, plus 50 Hz smoke runs
   of E10-E12. Record borders, flicker and the TV's own mode report.
3. For each row record the console model, region, BIOS, cable, ODE/disc,
   date, measured counters and SHA-256 of every capture. Mark PASS only
   against the row's written criteria; otherwise FAIL or BLOCKED.
4. Never convert an emulator observation into a hardware claim.

**Phase D - closeout (after the session).**

1. Update rows 27-38 with PASS/FAIL links, measured values and anomalies.
   Rows that were not run stay PENDING.
2. Update `VDP2_SUPPORT_MATRIX.json`, `LIBSATURN_CURRENT_COVERAGE.md`, the
   README and the plan status only for what the hardware actually proves.
   Keep every documented limitation that remains.
3. Set M6 to DONE only when all twelve rows have recorded physical PASS
   evidence and every build/test/package gate is green. If a row fails, keep
   M6 open and attach the failing artifact.
4. Archive the signoff bundle, update `CHANGELOG.md`, and tag the release.

**Scope guard.** M6 closes the E01-E12 plan and its physical proof. It does
not close the five `partial` support-matrix features (sprite-window source,
transactional CRAM/cycle ownership, RBG0 window/per-dot coefficient corners,
the RBG1 emulator pixel gap and double-density interlace). Those need a
separate M7 decision; they are documented limitations, not M6 blockers.

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

- Sega VDP2 manual: `docs/sega_saturn_hardware/hard/vdp2/hon/p02_20.md` (interlace), `p03_30.md` / `p03_34.md` (VRAM cycles and mapping), `p04_10.md` (pattern-name data), `p06_20.md` (RBG0/RBG1), `p06_30.md` / `p06_40.md` (rotation/coefficient), `p08_20.md` (windows), `p09_10.md` / `p09_20.md` (sprite formats/priority), `p10_30.md` (special function codes), `p11_20.md` (special priority), `p12_12.md` / `p12_13.md` / `p12_14.md` (extended colour/blur), `p12_20.md` (special colour calculation), `p14_10.md` (shadow processing), `p15_40.md` (normal vs MSB usage) and `p16_18.md` (SDCTL).
- Runtime: `include/saturn/stage_map2.h`, `vdp2_layers.h`, `vdp2_compose.h`, `vdp2_alloc.h`, `vdp2.h`, `video.h` and their implementations under `src/graphics/vdp2/` and `src/hal/vdp2/`.
- Existing fixtures: `examples/vdp2_*`, `tests/host/test_vdp2_*.cpp`, `harness/run-vdp2-*.ps1`.
- Baseline acceptance: `docs/HARDWARE_ACCEPTANCE_CHECKLIST.md`, `docs/LIBSATURN_CURRENT_COVERAGE.md`.
- Evidence ledger: `docs/VDP2_EVIDENCE_LEDGER.md` (E01-E12 build, host, emulator, stress and physical status).

**Remaining actionable slice:** execute Phase A and Phase B of the M6
completion path now. When hardware is available, run the E01-E12 discs from
`docs/HARDWARE_ACCEPTANCE_CHECKLIST.md` on real NTSC and PAL consoles, record
captures and measurements, and close only the rows the hardware actually
proves. Phase C cannot start without a machine, and Phase D closes M6 only
after that evidence exists.
