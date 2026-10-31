# vdp2_rotation_bitmap

RBG0 in bitmap format through the rotation manager: a 512x256, 256-colour
picture in VRAM-B0 that turns once every 128 frames and zooms about the middle
of the screen, with a new parameter region written every frame. There is no
pattern name map and no character data.

## VRAM

| Range | Reserved as | Contents |
|---|---|---|
| `0x3C000`, `0xE0` bytes | `SAT_VDP2_RESERVE_ROTATION_PARAMS` | the A+B parameter region, in A1 |
| `0x40000`, `0x20000` bytes | `SAT_VDP2_RESERVE_ROTATION_BITMAP` | the picture, filling claimed bank B0 |

`sat_vdp2_rotation_bitmap_bytes` gives the amount to reserve for a colour mode
and height. The plan is the ledger; `sat_vdp2_rotation_bitmap_configure` checks
the hardware rules itself, as `sat_vdp2_rotation_configure` does.

## The illegal combinations, demonstrated and recorded

`g_rotation_bitmap` records each refusal for a harness:

- `refused_misaligned`: a picture address that is not a multiple of `0x20000`;
- `refused_over_pattern`: the over-pattern mode, since a picture has no names;
- `refused_table_in_bank`: a parameter region inside the picture's bank;
- `refused_rbg1`: RBG1 enabled next to a bitmap;
- `refused_plan_unclaimed`: the plan reservation before the bank is claimed;
- `refused_plan_misaligned`: the plan reservation off the bank's edge.

## Limits

- Emulator evidence only (Ymir), and no harness script yet.
- Per-dot coefficients and the window mode are refused, as for the cell format.
