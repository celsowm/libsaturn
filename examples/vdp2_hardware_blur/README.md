# `vdp2_hardware_blur` — the VDP2's own horizontal blur

A live A/B of CCCTL BOKEN/BOKN, the hardware blur described in VDP2 manual
`p12_13.md` and `p12_14.md`. The checker on NBG0 is a worst case for a blur:
one pixel on, one pixel off, everywhere.

| Layer | Pattern | Priority | Role |
| - | - | - | - |
| NBG0 | one-pixel black/white checker | 5 | top image and BOKN target |
| NBG1 | solid blue | 3 | second image |

The blur result is forced into the **second image**, so the scene puts the
blurred screen on top and enables colour calculation on it. A colour
calculation window (W0, the left half) confines the blend, which leaves the
right half as the control: it must stay byte-identical when the blur toggles.

| Mode | Left half | Right half |
| - | - | - |
| `PLAIN` | sharp checker | sharp checker |
| `CC` | checker mixed 15:17 with the blue below at ratio 16 | sharp checker |
| `BLUR` | checker mixed 15:17 with its own 2:1:1 horizontal average | sharp checker |
| `BLUR 32` | ratio 31 makes the second image everything, so this is the blurred checker alone | sharp checker |

The blur averages the display dot and the two dots to its left,
`(2*current + previous1 + previous2) / 4` (p12_13). The test recomputes that
average and the 15:17 mix from the palette values; it does not compare a
golden image.

`g_blur` records the chapter-12 refusals and live checks:

```c
refused_bokn3                /* BOKN 3 is setting-prohibited (p12_14) */
refused_bokn7                /* BOKN 7 is setting-prohibited (p12_14) */
refused_cram1                /* BOKEN needs CRAM mode 0 (p12_13) */
refused_line_color           /* a live line-colour insert refuses BOKEN */
blur_accepts_after_line_color /* clearing the insert makes BOKEN legal again */
blur_accepts_after_extended  /* EXCCEN then BOKEN is accepted */
extended_accepts_after_blur  /* BOKEN then EXCCEN is accepted (clears blur) */
```

The hi-res refusal cannot be exercised in one run because the video mode is
fixed by `sat_init`; `tests/host/test_vdp2_compose_logic.cpp` covers it as
pure policy.

## Controls

- `A` cycles PLAIN / CC / BLUR / BLUR 32 by hand (the demo auto-advances
  at frames 60, 120 and 180 so the harness run is deterministic).
- `START` exits.

## Acceptance

```powershell
.\harness\run-vdp2-hardware-blur.ps1 -Bios .\bios\saturn_bios_us.bin
```

`harness/tests/test_vdp2_hardware_blur.py` checks:

- the sharp right half is identical between the CC and BLUR screenshots;
- the left half matches the manual's 2:1:1 average and 15:17 mix in both
  modes, including the alternating white/black columns;
- `BLUR 32` shows the averaged second image directly, so the 2:1:1 formula
  is checked without the ratio mix;
- the final CCCTL carries BOKEN + BOKN 2 + N0CCEN, CCRNA's N0 ratio is 16,
  and the probe's watch-write sees the BOKEN bit at the BLUR switch;
- the struct, every refusal and live toggle check, and the absence of
  misaligned accesses.

The evidence is Ymir at the pinned commit plus the harness patches, including
`ymir_gradation_sse_fix.patch` (a documented one-line correctness fix for
Ymir's SSE2 blur mask; the AVX2 and scalar paths were already correct).
Physical-hardware execution is still pending; see row 31 of
`docs/HARDWARE_ACCEPTANCE_CHECKLIST.md`.

## Files

- `main.c` — the scene.
- `README.md` — this file.
- `harness/run-vdp2-hardware-blur.ps1` — rebuilds the example, runs the
  probe for 260 frames, dumps Work RAM/probe JSON/screenshots and runs the
  acceptance test. The `solo.png` capture is `BLUR 32` at frame 250.
- `harness/tests/test_vdp2_hardware_blur.py` — the acceptance suite.
