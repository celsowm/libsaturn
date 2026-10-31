# VDP2 E01-E13 evidence ledger

**Date:** 2026-10-18 (Phase A/B), 2026-10-19 (M7 CRAM follow-up and E01 colour fix)
**Scope:** M6 Phase A evidence freeze and Phase B signoff-kit dry-run for the
twelve VDP2 examples in
`docs/VDP2_ADVANCED_EXAMPLES_IMPLEMENTATION_PLAN.md`, plus E13
(`vdp2_parallax_lab`), a post-Phase-A art-backed showcase built on the same
public layer API; the M7 follow-up adds transactional CRAM ranges.
**Physical hardware:** out of scope here and still **PENDING** for every
row. Nothing in this file is a hardware claim.

## How to read this ledger

- **PASS** means a named command or test produced a green result here.
- **PARTIAL** means the evidence covers only part of the stated contract.
- **NOT RUN** means no suitable environment or artifact exists yet.
- **N/A** means the check does not apply to that example.
- Build commands are `.\build-example.ps1 -Example <name>`; the sweep builds
  every example again before running it.
- Ymir runs use the user's own BIOS dump. The full wrapper set is exercised by
  `harness/run-acceptance-sweep.ps1` in NTSC (US BIOS) and PAL (EU BIOS).

## E01-E13 evidence

| Example (row) | Build | Host test | Ymir NTSC | Ymir PAL | Mednafen | Long / stress | Known emulator gap |
|---|---|---|---|---|---|---|---|
| **E01** `vdp2_dual_rotation` (29) | `build-example.ps1` | `tests/host/test_vdp2_rotation_rbg1.cpp` 35/35 | `run-vdp2-dual-rotation.ps1` 6/6 (DUAL pixel check passes, no skip) | `run-acceptance-sweep.ps1 -Pal` PASS | 1.32.1 BIOS disc boot in the Phase B dry-run | N/A | The old DUAL pixel skip was a colour-mode bug in E01 (256-colour configuration over 4bpp/16-colour tiles); the example now uses 16 colours and the checker is visible through RBG0's holes. Per-dot coefficients and the rotation window remain outside the NBG rotation path. |
| **E02** `vdp2_rotation_coefficients` (28) | `build-example.ps1` | `tests/host/test_vdp2_rotation_coefficients.cpp` PASS | `run-vdp2-rotation-coefficients.ps1` 6/6 | sweep PASS | NOT RUN | N/A | Rotation window (RPMD 3), per-dot coefficients and KTCTL line-colour composition are deliberately refused. |
| **E03** `vdp2_special_color` (30) | `build-example.ps1` | `tests/host/test_vdp2_compose_logic.cpp` PASS | `run-vdp2-special-color.ps1` 9/9 | sweep PASS | NOT RUN | N/A | Emulator pixel checks recompute the manual's table-12.2 cascade; physical output is PENDING. |
| **E04** `vdp2_hardware_blur` (31) | `build-example.ps1` | `tests/host/test_vdp2_compose_logic.cpp` PASS | `run-vdp2-hardware-blur.ps1` 11/11 | sweep PASS | NOT RUN | N/A | Second-image blur case is unverified on pinned Ymir; the harness pins the documented SSE2 fix. |
| **E05** `vdp2_sprite_shadows` (33) | `build-example.ps1` | `tests/host/test_vdp2_sprite_shadow.cpp` PASS | `run-vdp2-sprite-shadows.ps1` 12/12 | sweep PASS | NOT RUN | N/A | TPSDSL=0 negative is unverified because pinned Ymir does not gate the transparent MSB shadow on it. |
| **E06** `vdp2_special_priority` (32) | `build-example.ps1` | `tests/host/test_vdp2_special.cpp` PASS | `run-vdp2-special-priority.ps1` 10/10 | sweep PASS | NOT RUN | N/A | 1-word PNCN auxiliary PR/CC bits and bitmap BMPNA bits are not exposed; RBG0's special bits are not exposed. |
| **E07** `vdp2_dynamic_tile_stream` (34) | `build-example.ps1` | `tests/host/test_stage_map2.cpp`, `tests/host/test_vdp2_tile_stream.cpp` PASS | `run-vdp2-dynamic-tile-stream.ps1` 8 tests, 1 skipped (stress-only) | sweep PASS | NOT RUN | 36,000-frame stress 8/8; 34,905 guest frames, 32,999 switches, CRC `0x517824CD` | One-plane cell maps only; the double buffer is caller-composed; the map is procedural. |
| **E08** `vdp2_vram_stress` (27) | `build-example.ps1` | `tests/host/test_vdp2_plan_logic.cpp`, `tests/host/test_vdp2_plan_api.cpp` PASS (CRAM rollback/reuse included) | `run-vdp2-vram-stress.ps1` 11/11 | sweep PASS | NOT RUN | Seven scenarios in the default run | CRAM ranges are transactional; the cycle patterns remain planned inside the layer manager. |
| **E09** `vdp2_video_modes` (35) | `build-example.ps1` | `tests/host/test_video_mode.cpp` PASS | `run-vdp2-video-modes.ps1` 6/6 (five modes) | `run-vdp2-video-modes.ps1 -Pal` 6/6 (all eight modes) | 1.32.1 window captures of four modes | N/A | Double-density interlace and dedicated-monitor modes are refused; mode changes are full `sat_shutdown` + `sat_init`. |
| **E10** `vdp2_parallax_world` (36) | `build-example.ps1` | No scene-specific host test; shared VDP2 API host tests | `run-vdp2-parallax-world.ps1` 7/7 | `run-vdp2-parallax-world.ps1 -Pal` 7/7 | 1.32.1 live-path capture | `-Long` 7/7, 3,600 fields | Reference path is a pre-rendered VDP1 comparison, not a second hardware renderer. |
| **E11** `vdp2_fighting_stage` (37) | `build-example.ps1` | Shared character and VDP2 host tests | `run-vdp2-fighting-stage.ps1` 5/5 | `run-vdp2-fighting-stage.ps1 -Pal` 5/5 | 1.32.1 stage off/on captures | N/A | Stage is an E11-only compilation of the shared demo; `fighter_arena_3d` keeps its legacy NBG0 backdrop. |
| **E12** `vdp2_neon_city` (38) | `build-example.ps1` | Shared VDP2 API host tests | `run-vdp2-neon-city.ps1` 6/6 | `run-vdp2-neon-city.ps1 -Pal` 6/6 | 1.32.1 ring 0/1 and light-pool captures | `-Long` 6/6, 1,753 scene frames, 1,752 switches, equal model/VRAM CRC | Light pool is an artistic approximation; all timings are emulator-side. |
| **E13** `vdp2_parallax_lab` (post-A) | `build-example.ps1` | Shared VDP2 API host tests | `run-vdp2-parallax-lab.ps1` 4/4, 1 skipped (`-Long`) | `run-vdp2-parallax-lab.ps1 -Pal` 4/4, 1 skipped (`-Long`) | 1.32.1 BIOS disc boot in the Phase B dry-run; live pan captures | `-Long` 5/5, 2,400 fields | Art is a 512x256 reduction of an external four-layer lab, not seamless; alpha is thresholded, the sea is lightly softened against scroll shimmer, and the source was not authored for Saturn. |

The sweep logs are written to `harness/build/sweep_ntsc/` and
`harness/build/sweep_pal/`; each wrapper also leaves its own probe JSON,
WRAM dump, screenshots and test log under `harness/build/<example>/` or
`harness/build/<example>_pal/`.

## Reproduction commands

```powershell
.\harness\run-acceptance-sweep.ps1 -Bios .\bios\saturn_bios_us.bin
.\harness\run-acceptance-sweep.ps1 -Pal -Bios .\bios\saturn_bios_eu.bin
.\harness\run-vdp2-parallax-lab.ps1 -Bios .\bios\saturn_bios_us.bin
.\harness\run-vdp2-parallax-lab.ps1 -Pal -Bios .\bios\saturn_bios_eu.bin
.\harness\prepare-vdp2-signoff.ps1
.\harness\verify-vdp2-signoff.ps1 -Bundle .\dist\vdp2-signoff\2026-10-18
.\harness\dry-run-vdp2-signoff.ps1 -Bundle .\dist\vdp2-signoff\2026-10-18
.\scripts\test-package.ps1
```

Run `make test` and `make examples-all` from an MSYS2 UCRT64 shell with the
SH-2 toolchain on `PATH` (or use the project's MSYS2 launcher). Keep
`/ucrt64/bin` on the same `PATH`: some host test binaries are dynamically
linked and need `libstdc++-6.dll` from that directory.

## Phase A and Phase B gate status

| Gate | Result |
|---|---|
| `make test` | PASS (`[test] all host tests passed`, tool tests OK) |
| VDP2-only Ymir sweep, NTSC (Phase A baseline) | 18/18 PASS |
| VDP2-only Ymir sweep, PAL (Phase A baseline) | 18/18 PASS |
| VDP2-only Ymir sweep, NTSC (current 19-item set) | **19/19 PASS** (2026-10-18) |
| VDP2-only Ymir sweep, PAL (current 19-item set) | **19/19 PASS** (2026-10-18) |
| `scripts/test-package.ps1` | PASS, host 3/3 and package 5/5 |
| `make examples-all` | PASS (exit 0) |
| Clean `git diff --check` | PASS |
| Phase B signoff bundle | 13/13 `.bin`/`.cue` pairs validated, 13/13 strict boot reports, 107 bundle files |
| Phase B reproducibility | second clean build reproduced **13/13** disc hashes |
| Phase B Mednafen dry-run | **13/13** BIOS disc boots to the application scene (emulator only) |
| M7 CRAM transaction | palette allocator 6/6, new plan API cases, E08 11/11 NTSC and PAL |
| E13 post-A verification | NTSC 4/4 (+1 `-Long` skip) and PAL 4/4 (+1 skip); the 18/18 Phase A sweep count predates E13; current set is 19/19. |

## Phase B hardware signoff kit

Prepared 2026-10-19 from clean commit `d988fb2`; a clean rebuild reproduced
every disc hash. This refresh includes the E01 16-colour fix.

- Bundle: `dist/vdp2-signoff/2026-10-19/` (13 discs: E01-E12 core plus E13
  supplemental; 107 files, 21,374,529 bytes).
- Manifest SHA-256:
  `80d79c517a2dd79b7a6344a245e00ab9757f3ec7106d6f3d2841b3b0849e17e5`.
- `SHA256SUMS.txt` SHA-256:
  `da5137eb16c0b19381ff77706b9186590c1e6276373cd761e942b9a8b62416be`.
- `harness/prepare-vdp2-signoff.ps1` builds each disc through
  `build-example.ps1`, copies the `.bin`/`.cue`, validates the raw image with
  `tools/check_disc_image.py`, and validates the IP/ISO first read with
  `tools/inspect_boot_image.py --strict-ip`.
- `harness/verify-vdp2-signoff.ps1` verifies `SHA256SUMS.txt`, the manifest
  inventory and every boot report without building.
- `docs/VDP2_HARDWARE_SESSION.md` defines the console matrix, run order,
  per-row controls, minimum evidence and the PASS/FAIL/BLOCKED rules.
- Mednafen 1.32.1 dry-run: 13/13 discs BIOS-booted to their application
  scenes after a 20-second boot window; screenshots and report are under
  `harness/build/vdp2-signoff-dryrun/`.
- Ymir `-BootMode bios` is **not** a disc-boot check for these images: at 120
  fields it remained at the BIOS language screen with `entry_observed:false`.
  Ymir evidence is direct injection plus the 19/19 sweeps above.
- None of these results is hardware evidence. Phase C remains **BLOCKED** by
  the absence of physical NTSC and PAL consoles.

## M7 CRAM follow-up (2026-10-19)

- `sat_vdp2_palette_reserve/free` now use a seven-bank bitmap instead of a
  watermark counter: freed ranges are reusable, a partial/already-free range
  is refused with `SAT_ERR_NOT_FOUND`, and `sat_vdp2_palette_usage` reports
  per-bank used/free codes.
- `sat_vdp2_plan_palette_reserve/release` join the resource transaction.
  `sat_vdp2_plan_begin` snapshots the bitmap, `rollback` restores the exact
  state, `commit` keeps staged ranges, and `release_all` frees committed
  palette ranges too. A palette release is refused with `SAT_ERR_BUSY` while
  a transaction is open.
- Host tests: `test_vdp2_palette_alloc.cpp` 6/6 and the new palette cases in
  `test_vdp2_plan_api.cpp` pass.
- E08 `vdp2_vram_stress` adds a seventh scenario (rollback/reuse,
  commit/release and the eighth-full-bank refusal): **11/11 NTSC** and
  **11/11 PAL**.
- The current VDP2 sweep remains **19/19 NTSC** and **19/19 PAL** after the
  CRAM change; E01 was re-run separately at **6/6 NTSC** and **6/6 PAL**
  after the colour fix. `make test`, `make examples-all` and the package
  gates (host 3/3, package 5/5) pass.
- The cycle planner is still owned by `sat_vdp2_layer_configure` and remains
  outside the transaction, so `unified_resource_planner` stays `partial`.
  Physical hardware remains PENDING.

## Definition-of-done mapping

The checklist in section 8 of the plan is mapped here to concrete
artifacts. A box is only checked when the artifact exists; the physical box
remains unchecked by design.

| DoD item | Evidence | State |
|---|---|---|
| Scene compiles to a Saturn disc | All twelve plan examples and E13 build through `build-example.ps1`; the sweep rebuilds each one before running it. | PASS |
| Reusable public API | `docs/VDP2_SUPPORT_MATRIX.json` feature rows and the headers/APIs cited by every example README. | PASS |
| VRAM/cycle/CRAM/register ownership and refusals | Host tests for the pure policies plus the refusal counters in each example's telemetry and harness test. | PASS |
| Host regression tests | `make test`; the VDP2 host tests listed above plus the shared VDP2 suite. The Phase A run is green (`[test] all host tests passed`). | PASS |
| Interactive identification of the capability | Each example README documents its controls and the expected on-screen difference; the harness asserts the visible result where Ymir can render it. | PASS |
| Reproducible emulator acceptance | Current VDP2-only sweep: 19/19 on NTSC and 19/19 on PAL. Logs, named BIOS, probe JSON, screenshots and telemetry; the Phase B bundle adds 13/13 Mednafen BIOS disc-boot captures. | PASS |
| Measurements separate submission from display | Every harness test reads guest telemetry and, where applicable, the captured image; no hardware claim is made. | PASS |
| Stock RAM and VDP1/VDP2 budgets | The example sources contain no heap calls; buffers are fixed arrays. E07, E11 and E12 expose transfer/memory telemetry; E07's staging buffers are explicitly fixed. | PASS for the no-heap/fixed-buffer contract; per-example RAM HUDs are not universal |
| No regression in existing consumers | `make test`, `make examples-all` (exit 0) and the pre-plan VDP2 wrappers in the sweep. | PASS |
| All required tests/builds pass on `main` | Phase A/B: `make test` green; package gates 3/3 + 5/5; VDP2-only NTSC 19/19 and PAL 19/19; `make examples-all` exit 0; Phase B reproducibility 13/13. | PASS |
| Physical hardware remains PENDING | `docs/HARDWARE_ACCEPTANCE_CHECKLIST.md` rows 27-38, plus E13 row 39 as supplemental. | PENDING |
| README, manifest, matrix and checklist updated | Example READMEs, support matrix, coverage document, plan and checklist. | PASS for the current emulator scope |

## Phase A sweep repairs

The full VDP2 sweep exposed three test/harness issues that were fixed in this
phase; none changed an example's rendering path:

1. E10's sky sample sat under the swaying foreground leaves in PAL. The
   test now samples a clear sky window that is stable in both standards.
2. E11's fixed capture frames could land on the wrong stage state when the
   PAL round timing shifted. The off/on captures are now triggered by the
   `g_vdp2_stage.toggles` telemetry counter, not by absolute frame numbers.
3. `test_vdp2_hires_light.py` decoded three `uint8_t` telemetry switches as
   `uint32_t`, so it read padding (`0x01010100`) and failed. The decoder now
   matches the C struct (`>IIIiiBBB`).

The shared runner also writes PAL probe, WRAM and telemetry files under
`harness/build/<example>_pal/`, so a PAL run no longer overwrites the NTSC
evidence set.

The full 32-item NTSC sweep also ran. Beyond the fixed VDP2 hi-res decoder,
it reports three non-VDP2 failures (`save-cartridge`, `scsp-dsp` and
`sound-driver`) that are outside this slice's scope; the VDP2-only sweep is
the Phase A gate. They are not counted as VDP2 evidence.

## Open gaps that are not M6 blockers

These are recorded so they are not lost or mistaken for finished work:

1. The five `partial` support-matrix features: sprite-window source, the
   transactional CRAM/cycle owner, RBG0 window/per-dot coefficient corners,
   the RBG1 emulator pixel gap and double-density interlace.
2. E04's manual second-image blur case and E05's TPSDSL=0 negative are
   unverified on the pinned Ymir.
3. Mednafen was used as a manual visual cross-check for E09-E12, not as an
   automated gate for E01-E08.
4. Physical rows 27-38 (plus supplemental E13) require a real NTSC and PAL
   Saturn. Phase B has prepared the reproducible disc bundle and session
   protocol; Phase C is blocked only by hardware access.
5. Ymir cannot complete a BIOS disc boot for these images: `-BootMode bios`
   stops at the BIOS language screen with `entry_observed:false`. The Ymir
   evidence is direct injection plus the acceptance sweep; Mednafen provides
   the emulator disc-boot dry-run. Neither is a hardware result.
