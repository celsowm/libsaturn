# `vdp2_sprite_shadows` — normal shadows, MSB shadows and a VDP1 control

A live tour of VDP2 manual 14.1. A 32x32 silhouette moves over a scrolling
stripe ground; the same scene also shows the two shadow forms the manual
defines and a VDP1 shadow control.

## Layers and registers

| Piece | Setting |
| - | - |
| NBG0 ground | 16-colour cell layer, type 1-word names, priority 3, scrolls 2 px/frame |
| Sprite layer | type 2, palette only, ordinary dots and the transparent MSB dot at S0PRIN=4, normal-shadow dots at S1PRIN=7 |
| SDCTL | NBG0 target; TPSDSL set in MSB mode |
| Normal shadow dot | `0x47FE` (bit 15 clear, low 11 bits `0x7FE`, selector 1) |
| Transparent MSB dot | `0x8000` (all 15 non-MSB bits zero, so it uses S0PRIN) |
| Sprite self-shadow | VDP1 `SAT_SPRITE_FLAG_MSB_ON` over the already-drawn sprite |

VDP1 cannot express `0x47FE` through CMDCOLR's `palette << 8` form: the low
nibble of the 12-bit color bank is lost. The example uploads a 16-entry LUT4
table with `sat_vdp1_upload_lut`, puts the shadow word in entry 1, and draws a
32x32 LUT4 silhouette. Code 0 stays transparent; code 1 writes the shadow
word to the frame buffer. The public encoders are
`sat_vdp2_shadow_normal_word`, `sat_vdp2_shadow_transparent_msb_word` and
`sat_vdp2_shadow_msb_word` in `saturn/vdp2_sprite_shadow.h`.

The normal-shadow sprite is excluded from the VDP2 layer stack and halves the
top image under it; SDCTL bit 0 gates NBG0. The transparent MSB shadow does
the same with bit 15 set and all lower bits zero. A VDP1 MSB-on pass keeps the
palette dot's lower 15 bits and sets bit 15, so VDP2 fetches the sprite's own
colour and halves it (a sprite shadow).

## Timeline

| Frame | Mode | What it shows |
| - | - | - |
| 44 | `NORMAL` | `0x47FE` normal shadow over NBG0: 248 -> 124 |
| 104 | `MSB` | `0x8000` transparent shadow + MSB-on sprite self-shadow (124) |
| 164 | `VDP1` | RGB panel + VDP1 shadow: 31 -> 15 -> 120 (5-bit path) |
| 224 | `TARGETS OFF` | normal silhouette drawn, SDCTL=0: ground stays full |
| 284 | `OFF` | shadow disabled: ground stays full |

The auto sequence advances at frames 60/120/180/240; `A` cycles by hand and
`START` exits. The captures are two frames after the counter values 42, 102,
162, 222 and 282, which is why the table lists 44..284.

## Acceptance

```powershell
.\harness\run-vdp2-sprite-shadows.ps1 -Bios .\bios\saturn_bios_us.bin
```

`harness/tests/test_vdp2_sprite_shadows.py` recomputes every expected pixel
from the 5-bit ground palette, the scroll and the manual's rules — no golden
images:

- normal shadow: ground under the silhouette halves (for example blue
  `0,0,248` -> `0,0,124`), while the same stripe above the silhouette stays
  full and the ground keeps scrolling;
- transparent MSB: the ground halves and the MSB-on palette sprite
  self-shadows to `124,124,124`;
- VDP1 control: the RGB panel is `0,248,248` outside the shadow and
  `0,120,120` inside it. VDP1 halves the 5-bit destination (31 -> 15 -> 120),
  which is intentionally different from VDP2's 8-bit halving (248 -> 124);
- `TARGETS OFF` writes SDCTL=0 and the ground stays full under the drawn
  silhouette; `OFF` also leaves it full;
- the final SDCTL/SPCTL/PRISA state is restored, every refusal is 1,
  `SDCTL=0x0101` is watched at the NORMAL -> MSB switch, and the struct dump
  and screenshot-when negatives are checked.

The host test (`tests/host/test_vdp2_sprite_shadow.cpp`) covers the encoders
and every accepted/refused configuration; `tests/host/test_vdp1_logic.cpp`
covers the MSB-on PMOD bit and its colour-calculation exclusion.

## Limitations

- The shadow mode switches the whole sprite layer to type 2 palette-only. The
  library cannot inspect VDP1 draws, so RGB dots drawn while the mode is
  active would be read as shadow/window data; the example uses palette and
  LUT4 dots only. Hi-res/8-bit, the palette-priority/window/colour
  calculation owners and the VDP1 sprite colour-calculation module are
  refused.
- The VDP1 control is a separate mode, not a split screen: shadow mode pins
  the whole sprite layer to palette format, so the RGB panel cannot coexist
  with the VDP2 shadow sprites.
- Normal shadows can only target the selected scroll/back screens, as the
  manual says. A sprite shadow requires the VDP1 MSB-on pass over an existing
  sprite dot.
- TPSDSL is encoded and set by the example, but pinned Ymir does not gate the
  transparent MSB shadow on it; a TPSDSL=0 negative remains unverified.
- The evidence is Ymir at the pinned commit plus the harness patches. Physical
  hardware execution is still pending; see row 33 of
  `docs/HARDWARE_ACCEPTANCE_CHECKLIST.md`.

## Files

- `main.c` — the scene.
- `README.md` — this file.
- `harness/run-vdp2-sprite-shadows.ps1` — rebuild, run the 320-frame probe,
  dump screenshots/Work RAM/probe JSON and run the test.
- `harness/tests/test_vdp2_sprite_shadows.py` — the acceptance suite.
