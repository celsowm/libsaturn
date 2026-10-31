# VDP2 hardware signoff session

**Status:** Phase B kit and protocol. **No physical console has run this
protocol**; every checklist row remains **PENDING** until the artifacts from a
real session are attached.

This document is the physical Phase C companion to
`docs/VDP2_ADVANCED_EXAMPLES_IMPLEMENTATION_PLAN.md` and
`docs/HARDWARE_ACCEPTANCE_CHECKLIST.md`. It covers the twelve M6 examples
(E01-E12, checklist rows 27-38) plus E13 (`vdp2_parallax_lab`, row 39) as a
clearly marked supplemental disc.

## 1. Scope and non-claims

- The **M6 core** is E01-E12. A physical PASS on those twelve rows is required
  before M6 can be marked DONE.
- **E13 is supplemental.** It is included in the default bundle because it is
  a current VDP2 showcase, but a missing E13 run does not reopen the formal M6
  scope. Record it separately as row 39.
- Emulator screenshots, Ymir probe JSON, Mednafen captures and emulator
  timing are **not** hardware evidence. Do not copy them into a PASS row.
- The five `partial` support-matrix features are an M7 decision, not M6
  blockers: sprite-window source, transactional CRAM/cycle ownership, RBG0
  window/per-dot coefficient corners, the RBG1 emulator pixel gap and
  double-density interlace.
- A row that cannot be run is **BLOCKED**, never PASS by inference.

## 2. Build and verify the bundle

From the repository root:

```powershell
# Default: E01-E12 plus E13 supplemental
.\harness\prepare-vdp2-signoff.ps1

# Strict M6 core only
.\harness\prepare-vdp2-signoff.ps1 -CoreOnly

# Reproducibility check: build a second bundle and compare disc hashes
.\harness\prepare-vdp2-signoff.ps1 -Force -CompareTo .\dist\vdp2-signoff\<date>\manifest.json

# Verify a bundle before burning/loading it
.\harness\verify-vdp2-signoff.ps1 -Bundle .\dist\vdp2-signoff\<date>
```

The bundle contains:

- `discs/<example>.bin` and `.cue`: raw `MODE1/2352` Saturn images;
- `inspect/`: the logical ISO and application binary used for validation;
- `boot-reports/`: strict IP/ISO first-read reports;
- `logs/`: build, disc-image and boot-image checks;
- `manifest.json`, `SHA256SUMS.txt`, `README.txt` and a copy of this protocol.

Use `SHA256SUMS.txt` as the authority for the files that were actually
prepared. Record the bundle's `manifest.json` SHA-256 in the session notes.

## 3. Console matrix

At minimum, one NTSC and one PAL console must complete their assigned rows.
Record every console actually used; do not pool results from different models
into one PASS claim.

| Console ID | Region | Model / VA | BIOS | Video cable | Storage | Display | Notes |
|---|---|---|---|---|---|---|---|
| `ntsc-va1-01` | NTSC | VA1 (example) | 1.01 (example) | RGB SCART | ODE | CRT | |
| `pal-va1-01` | PAL | VA1 (example) | 1.01 (example) | RGB SCART | ODE | 50 Hz CRT/multisync | |

Recommended extra coverage, if hardware is available:

- NTSC VA0, VA1 and VA2; PAL VA1 and VA2.
- Composite, S-Video and RGB cables; record the cable with every row.
- ODE (Saroo/Satiator/Rhea/Phoebe) and a burned CD-R. Record the ODE
  firmware or the burn media.
- A CRT and an upscaler/scaler, especially for the interlaced and 256-line
  modes. The TV's own mode report is part of the E09 evidence.

Setup rules:

- Use stock RAM for E11 unless the row says otherwise. Do not insert a RAM
  cartridge for E11.
- Back up internal backup RAM before any row that formats or writes it. The
  VDP2 rows in scope do not intentionally format backup RAM.
- Use a PAL console and a 50 Hz-capable display for E09's 256-line modes.
- Keep the same console, cable and display within one row; changing them can
  alter borders, flicker and scaling.

## 4. Run order

Run the core rows in checklist order so the session log matches the
acceptance table. E13 is supplemental and can run last.

| Order | ID | Disc | Checklist row | Standard |
|---:|---|---|---:|---|
| 1 | E08 | `vdp2_vram_stress` | 27 | NTSC, PAL smoke |
| 2 | E02 | `vdp2_rotation_coefficients` | 28 | NTSC |
| 3 | E01 | `vdp2_dual_rotation` | 29 | NTSC |
| 4 | E03 | `vdp2_special_color` | 30 | NTSC |
| 5 | E04 | `vdp2_hardware_blur` | 31 | NTSC |
| 6 | E06 | `vdp2_special_priority` | 32 | NTSC |
| 7 | E05 | `vdp2_sprite_shadows` | 33 | NTSC |
| 8 | E07 | `vdp2_dynamic_tile_stream` | 34 | NTSC long run |
| 9 | E09 | `vdp2_video_modes` | 35 | NTSC five modes, PAL eight modes |
| 10 | E10 | `vdp2_parallax_world` | 36 | NTSC long run, PAL smoke |
| 11 | E11 | `vdp2_fighting_stage` | 37 | NTSC stock RAM, PAL smoke |
| 12 | E12 | `vdp2_neon_city` | 38 | NTSC long run, PAL smoke |
| 13 | E13 | `vdp2_parallax_lab` | 39 | NTSC, PAL smoke (supplemental) |

For PAL, E09's eight-mode matrix is mandatory. The other PAL rows are 50 Hz
smoke checks for cadence, borders, mode detection and visible tearing.

## 5. Per-row protocol

Controls are the ones printed by the example. If a pad does not respond,
record it as a harness/input failure; do not silently substitute an emulator.
The detailed pass criteria remain the checklist row; this table is the
session checklist and artifact plan.

| Row | Disc | Controls | Required observation / counters | Evidence |
|---:|---|---|---|---|
| 27 | `vdp2_vram_stress` | **A** next scenario, **B** debug overlay, **START** exit. Scenarios also auto-advance. | Four colour bands stay drawn through the capacity, busy and fragmentation refusals; the bars reshape after the live reclaim/reconfigure; refusal count climbs. Record scenario bitmask, refusal count, per-bank free/reserved KiB, last status. | Full frame at the committed layout and after reclaim; close-up of bars/overlay; 30 s video. |
| 28 | `vdp2_rotation_coefficients` | **A** skips scenes, **B** debug overlay, **START** exit. | SWELL, WAVE, YAW B and MIX show the perspective floor, per-line displacement, the 30-degree yaw and the split picture. The three illegal combinations (window mode, window configure, bit-7 region) are refused. Record scene, refusals, coefficient/parameter values from the overlay. | Full frames of all four scenes; close-up of the MIX boundary; 30 s video. |
| 29 | `vdp2_dual_rotation` | **A** skips modes, **B** debug overlay, **START** exit. | A-ONLY, B-ON-RBG0 and DUAL show the grid, the yawed checker through the grid holes, then RBG1 superimposed. All six refusal codes are 1. Record mode, refusal codes, RBG0/RBG1 bank state. | Full frames of the three modes; close-up of transparent holes; 30 s video. |
| 30 | `vdp2_special_color` | **A** cycles PLAIN/RATIO/ADD/EXT, **START** exit. | Left half changes per mode; right half stays plain. RATIO is 15:17, ADD saturates, EXT shows `(NBG1+NBG2)/2` under the bars and pure NBG2 outside. Five chapter-12 refusals are 1. Record CCCTL/CCRNA and the mode. | Full frames of all four modes; close-up of the left/right boundary. |
| 31 | `vdp2_hardware_blur` | **A** cycles PLAIN/CC/BLUR/BLUR 32, **START** exit. | The right half is byte-identical in every mode. BLUR shows the 2:1:1 horizontal average mixed 15:17; BLUR 32 shows the blurred checker alone. BOKEN/BOKN/CCRNA read back; refused line-colour insert and live re-enable are 1. | Full frames of all four modes; close-up of checker and right-half control. |
| 32 | `vdp2_special_priority` | **A** cycles SCREEN/CHAR/DOT/MSB, **START** exit. | Inside the magenta bar and above it, SCREEN, CHAR, DOT and MSB follow the checklist row. SFPRMD/SFCCMD/SFCODE/SFSEL/CCCTL read back; all refusals are 1. | Full frames of all four modes; close-up of bar edge and special cells. |
| 33 | `vdp2_sprite_shadows` | **A** cycles NORMAL/MSB/VDP1/TARGETS OFF/OFF, **START** exit. | NORMAL halves the ground under the silhouette; MSB also halves the self-shadow; VDP1 gives the 5-bit 120 path; TARGETS OFF leaves the ground full; OFF restores it. SDCTL=0x0101 is observed at the switch; final state restored; refusals are 1. | Full frames of all five states; close-up of the ground values; 30 s video. |
| 34 | `vdp2_dynamic_tile_stream` | Automatic; **START** exits. | Run 10 minutes. The road has no seam at a ring switch, model and VRAM CRC match every 60 fields, zero 0xFFFF sentinel cells, no illegal interrupt/timeout. Record ring switches, max pending, staged/committed words, SCU-DMA and CPU transfers from the HUD. | Full frame; 60 s video covering at least one switch; HUD close-ups at start/end. |
| 35 | `vdp2_video_modes` | **A** next mode, **C** previous mode, **START** exit; auto-advances every 60 fields. | NTSC: 320x224, 352x240, 640x240, 320x480, 640x480. PAL: those plus 704x256, 320x512, 640x512. Border/grid/checker/diagonal visible; `REFUSED double/width/height/pal` all 1. Record TV mode report, overscan and interlaced flicker. | Full frame of every mode, including the TV's mode display if possible; 30 s video of interlaced modes. |
| 36 | `vdp2_parallax_world` | **A / R** toggle the pre-rendered VDP1 reference; **START** exit. | Five layers in order; shore ripples; leaves sway; reference toggle replaces the background and returns. Run 10 minutes. Record HUD frames, VDP2 update time and VDP1 command count for both paths; no seam in the repeating sea. | Full frames of both paths; 60 s video; HUD close-ups. |
| 37 | `vdp2_fighting_stage` | Pad; **A/C** punch/kick, **B** guard, **X** jump, **Y** mesh detail, **Z** debug, **L** stage on/off, **START** restart. Stage auto-toggles at frames 70/120. | Fighters, mat, shadows and HUD identical with stage off/on; only backdrop changes. Crowd drifts; banners sway/dim; spotlights move; stage-off is flat back colour. Record VDP1 command count, geometry time and perceived cadence. Stock RAM, no RAM cart. | Full frames of stage off/on/on; 60 s video including a round; no-overlap close-up. |
| 38 | `vdp2_neon_city` | **B** pause street stream, **R** reset camera/resume, **START** exit. | Sign rotates via hardware; street switches rings without a seam; light pool follows the window; skyline/rain colours shift. Run 10 minutes. Record ring switches, VDP2 update time, VDP1 command count, scene cadence, zero sentinel cells and equal model/VRAM CRC. | Full frames of ring 0/1 and light pool; 60 s video; HUD close-ups. |
| 39 | `vdp2_parallax_lab` | **A** pause/resume pan, **START** exit. | Four layers move at 8/24/65/100 percent; horizon aligned; alpha cutouts reveal the layer below; pan reverses before the non-seamless edge; no visible sea stutter. Record pan/frame/HUD cadence and VDP1 command count (HUD only). | Full frames at pan extremes and mid-pan; 60 s video. Supplemental. |

## 6. Minimum evidence rule

For every row:

1. A **full-screen capture that includes the HUD or on-screen counters**.
   A crop that hides the counters is not sufficient.
2. A **close-up** when the pass criterion is a pixel value, colour, alias or
   boundary. Name it `closeup.png`.
3. A **30-60 s video** when the criterion is motion, flicker, tearing, seam
   behaviour or cadence. Name it `motion.mp4`.
4. A **notes file** with console ID, date, BIOS, cable, storage, display,
   controls used, counters read and the PASS/FAIL/BLOCKED decision.
5. **SHA-256 of every capture and video** in `sha256.txt`.

Suggested layout:

```
session/<console-id>/<row-id>-<example>/
  full.png
  closeup.png
  motion.mp4
  notes.md
  sha256.txt
```

Record timestamps in local time and state the timezone. Do not reuse a capture
from another console or standard.

## 7. Session procedure

### Before the session

- Verify the bundle with `harness/verify-vdp2-signoff.ps1`.
- Record the bundle manifest SHA-256.
- Confirm the console region and BIOS; note the model/VA and video cable.
- Back up save data. Use stock RAM for E11.
- Confirm the display handles the required modes; for PAL 256-line modes use
  a 50 Hz-capable display.
- Label every disc/ODE entry with its E-number and example name.

### NTSC pass

Run rows 27-38 in order, then E13 if in scope. For each row:

- capture the required full frame/close-up/video;
- read the HUD counters, not just the picture;
- write the PASS/FAIL/BLOCKED decision and the measured values;
- record any anomaly even if the row passes.

### PAL pass

Run E09's eight-mode matrix first. Then run the 50 Hz smoke rows E10-E13.
Record borders, flicker, the TV's own mode report and any overscan. A PAL
smoke PASS does not replace the NTSC long-run evidence for E07/E10/E12.

### After the session

- Hash the captures and archive the session directory with the bundle.
- Update rows 27-39 in `docs/HARDWARE_ACCEPTANCE_CHECKLIST.md` with
  PASS/FAIL/BLOCKED, measured values and artifact links.
- Update `docs/VDP2_EVIDENCE_LEDGER.md`.
- Mark M6 DONE only when all twelve core rows have physical PASS evidence and
  all build/test/package gates are green. Keep M6 open on any FAIL or
  BLOCKED core row.

## 8. Failure handling

- Retry a failed row once with the same console, cable and disc. Record both
  attempts.
- If the failure is environmental (no 50 Hz display, missing ODE, unreadable
  media), mark **BLOCKED** and name the missing item.
- If the failure is a visual/register contract failure, mark **FAIL**, attach
  the artifact, and keep M6 open.
- Never replace a hardware row with an emulator result. Emulator dry-runs are
  recorded separately and labelled as such.

## 9. Emulator dry-run boundary

The Phase B dry-run uses two different paths:

- **Mednafen 1.32.1**: real BIOS disc boot of every bundle disc, one
  representative frame per disc. This is an emulator check, not hardware
  evidence.
- **Ymir**: the pinned probe cannot complete a BIOS disc boot for these
  images. At 120 fields it remains at the BIOS language screen
  (`entry_observed:false`), so the Ymir evidence is **direct application
  injection**, plus the full VDP2 acceptance sweep. It is not a disc-boot
  check.

### Phase B dry-run results (2026-10-18, emulator only)

- Bundle: 13 discs (E01-E12 core plus E13 supplemental), 107 files;
  every `.bin`/`.cue` pair validated against its ISO and every boot report
  passed `--strict-ip`.
- A second clean build reproduced all 13 disc hashes (`13/13`).
- Mednafen 1.32.1 BIOS-booted all 13 discs; every screenshot showed the
  application scene, not the BIOS shell. Captures:
  `harness/build/vdp2-signoff-dryrun/`.
- Ymir `-BootMode bios` did **not** boot these discs; it remained at the BIOS
  language screen with `entry_observed:false`. Ymir evidence is therefore
  direct injection plus the VDP2 acceptance sweep.
- The current VDP2 acceptance set passed **19/19 NTSC** and **19/19 PAL**,
  including E13.
- `make test`, `make examples-all` (exit 0) and the package gates
  (host 3/3, package 5/5) passed.

These results are emulator and build checks. They do not replace any Phase C
row. The exact bundle manifest hash and artifact paths are recorded in
`docs/VDP2_EVIDENCE_LEDGER.md`.
