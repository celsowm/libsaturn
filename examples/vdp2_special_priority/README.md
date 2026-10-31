# `vdp2_special_priority` — per-character and per-dot priority / colour calculation

A live tour of VDP2 manual 11.2 (special priority) and 12.2 (special colour
calculation). The scene uses **2-word pattern names**, where each cell carries
its own palette, flip bits, special-priority bit and special-colour-calculation
bit (manual figure 4.9).

| Layer | Pattern | Priority | Role |
| - | - | - | - |
| NBG0 | 2-word names, dot codes 1/6 | 4 (+ special bit) | the band under test |
| NBG1 | solid blue, 1-word names | 2 | second image behind the band |
| Sprite | magenta VDP1 bar | 4 (one value for the VDP1 layer) | the depth probe |

Cells alternate: even cells are normal (`PR=0`, `CC=0`), odd cells below the
HUD rows are special (`PR=1`, `CC=1`). The character tile alternates dot code
1 (white, CRAM bit 15 clear) and code 6 (orange, CRAM bit 15 set). SFCODE A bit
3 selects codes 6/7.

At the bar (y 80..120) a normal cell has priority 4 and loses to the priority-4
sprite (sprites win ties in manual table 11.1); a special cell has priority 5
and rises above the bar. At the same time, a special cell's colour calculation
is enabled, so it mixes with the second image — which is the bar itself where
it is above it, and NBG1 elsewhere. That is the visible A/B.

| Mode | SFPRMD | SFCCMD | What decides |
| - | - | - | - |
| `SCREEN` | 0 | 0 | the screen priority and CCCTL enable |
| `CHAR` | 1 | 1 | the PR/CC bits of each 2-word name |
| `DOT` | 2 | 2 | PR/CC=1 **and** the dot code in SFCODE A (bit 3 = codes 6/7) |
| `MSB` | 0 | 3 | CRAM colour bit 15 |

`MSB` is the colour-data-MSB mode: `SAT_RGB555` sets bit 15, `SAT_RGB555_RAW`
clears it. The tile's orange dots use the former, the white dots the latter.

## Controls

- `A` cycles SCREEN / CHAR / DOT / MSB by hand (the demo auto-advances at
  frames 60, 120 and 180 so the harness run is deterministic).
- `START` exits.

## Acceptance

```powershell
.\harness\run-vdp2-special-priority.ps1 -Bios .\bios\saturn_bios_us.bin
```

`harness/tests/test_vdp2_special_priority.py` checks, per mode:

- inside the bar: SCREEN shows the bar everywhere; CHAR leaves normal cells
  under the bar and raises the special cells (mixed 15:17 with the bar); DOT
  raises only the orange code-6 dots; MSB keeps priority 0 and shows the bar;
- above the bar: SCREEN mixes every cell with blue; CHAR leaves normal cells
  pure and mixes special cells; DOT mixes only code-6 dots in special cells;
  MSB mixes only the CRAM bit-15 orange dots;
- SFPRMD/SFCCMD/SFCODE/SFSEL/CCCTL read back, the SFCCMD watch-write sees
  mode 3, the struct and refusals match, and there are no misaligned accesses.

Expected colours are recomputed from the palette values and the manual's
15:17 ratio; the bar is a pixel-level priority control, not a golden image.

The host side (`tests/host/test_vdp2_special.cpp`) covers the 2-word encoder
and every accepted/refused mode/format combination. Limitations, recorded in
the support matrix: per-character/dot modes require 2-word names (the 1-word
PNCN auxiliary PR/CC bits and the bitmap BMPNA bits are not exposed), per-dot
is refused for RGB colour formats, and RBG0's SFPRMD/SFCCMD bits are not
exposed because `sat_vdp2_layer_t` covers NBG0-NBG3.

The evidence is Ymir at the pinned commit plus the harness patches. Physical
hardware execution is still pending; see row 32 of
`docs/HARDWARE_ACCEPTANCE_CHECKLIST.md`.

## Files

- `main.c` — the scene.
- `README.md` — this file.
- `harness/run-vdp2-special-priority.ps1` — rebuilds the example, runs the
  probe for 300 frames, dumps Work RAM/probe JSON/screenshots and runs the
  acceptance test.
- `harness/tests/test_vdp2_special_priority.py` — the acceptance suite.
