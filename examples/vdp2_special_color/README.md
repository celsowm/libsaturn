# `vdp2_special_color` — ratio, add and extended colour calculation on the Sega Saturn

A live tour of the VDP2's chapter-12 colour calculation: the same three-layer
stack composed three different ways, with the plain stack beside it for
comparison.

| Layer | Pattern | Priority | Role |
| - | - | - | - |
| NBG0 | yellow vertical bars | 5 | top image |
| NBG1 | red horizontal stripes | 4 | second image |
| NBG2 | green fill (solid) | 3 | third image |

A colour calculation window (W0, a rectangle over the left half) confines
every effect to x < 160, so the left half shows the active mode and the right
half shows the untouched stack at the same time.

| Mode | What's on screen | Live path |
| - | - | - |
| `PLAIN` | The plain three-layer stack. | everything off |
| `RATIO` | NBG0 blends over the image below, 15:17 at ratio 16. | `sat_vdp2_screen_color_calc_set(NBG0)` |
| `ADD` | NBG0 + second image added as they are and saturated (yellow+red -> (248,248,72)). | `sat_vdp2_color_calc_set_add` (CCMD) |
| `EXT` | The second image becomes (NBG1 + NBG2)/2 (table 12.2), then NBG0 blends over that pre-mixed pair at ratio 8; NBG1's own ratio is 31, so a stripe outside a bar shows pure NBG2 green on the effect side. | `sat_vdp2_extended_color_calc_set` (EXCCEN) |

**CRAM mode 0 is part of the demonstration.** Table 12.2 only extends a
palette-format second image (2:2:0) in CRAM mode 0; CRAM modes 1 and 2 give
such dots 4:0:0, i.e. no cascade at all. The scene calls
`sat_vdp2_cram_mode_set(0)` before uploading the palettes.

`g_special_color` records the chapter-12 refusals the harness checks:

```c
refused_ratio_range         /* ratio 32                                 -> SAT_ERR_INVALID_ARG */
refused_screen_range        /* the sprite screen is not a scroll screen -> SAT_ERR_INVALID_ARG */
refused_blur_screen         /* BOKN 3 is setting-prohibited (p12_14)    -> SAT_ERR_UNSUPPORTED */
refused_insert_behind_blur  /* line-colour insert refused while blur is on (p12_13) */
extended_clears_blur        /* the same insert succeeds once EXCCEN cleared the blur */
```

## Controls

- `A` cycles PLAIN / RATIO / ADD / EXT by hand (the demo auto-advances at
  frames 60 / 120 / 180 so the harness run is deterministic).
- `START` exits.

## Acceptance

```powershell
.\harness\run-vdp2-special-color.ps1 -Bios .\bios\saturn_bios_us.bin
```

`harness/tests/test_vdp2_special_color.py` re-derives every blended colour
from the 5-bit palette values and the manual's weight tables and checks both
screenshot halves: the effect pixels on the left, the plain stack on the
right, the refusals in `g_special_color`, the final CCCTL/CCRNA registers,
and the probe's watch-write of CCCTL at the RATIO switch. The screenshots are
keyed to `g_special_color.frames` with `--screenshot-when`, not to fixed
emulator frames.

The emulator evidence is Ymir (pinned commit plus instrumentation-only debug
patches). Physical-hardware execution is still pending; see row 30 of
`docs/HARDWARE_ACCEPTANCE_CHECKLIST.md`.

## Files

- `main.c` — the scene.
- `README.md` — this file.
- `harness/run-vdp2-special-color.ps1` — rebuilds the example, runs the
  probe for 300 frames, dumps Work RAM/probe JSON/screenshots and runs the
  acceptance test.
- `harness/tests/test_vdp2_special_color.py` — the 9-test acceptance suite.
