# VDP1 System Registers Reference

Base Address: **0x25D00000**

| Offset | Mnemonic | Name | Description |
|---|---|---|---|
| 0x00 | `TVMR` | TV Mode | Bit 0: VBE (VRAM Access), Bit 1: PTM (Plot Mode). |
| 0x02 | `FBCR` | Frame Buffer Control | Bit 0: FCM (Manual/Auto Change), Bit 1: FMT (Even/Odd Field). |
| 0x04 | `PTMR` | Plot Trigger | 0=Stop, 1=Execute once, 2=Auto (on VBlank). |
| 0x06 | `EWDR` | Erase Write Data | 16-bit color value used for frame buffer erase. |
| 0x08 | `EWLR` | Erase Upper Left | Upper-left coordinate for erase (X/8, Y). |
| 0x0A | `EWRR` | Erase Lower Right | Lower-right coordinate for erase (X/8, Y). |
| 0x0C | `ENDR` | Force Termination | Writing any value stops the drawing process immediately. |
| 0x10 | `EDSR` | Status (Read) | Bit 0: CEF (Draw End), Bit 1: BEF (Busy). |
| 0x12 | `LOPR` | Last Processed | Address of the last command table processed. |
| 0x14 | `COPR` | Current Processing | Address of the command table currently being processed. |
| 0x16 | `MODR` | Mode Status | Read-back of TVMR/FBCR settings. |

## Command Type Selection (CMDCTRL Bits 2-0)
- `000`: Normal Sprite (1 point)
- `001`: Scaled Sprite (2 points)
- `010`: Distorted Sprite (4 points)
- `100`: Polygon
- `101`: Polyline
- `110`: Line
- `111`: Clear (Non-drawing)

## Primitive Mode (CMDPMOD)
- **Bits 5-3**: Color Mode (0=16 colors, 1=64 colors, 2=128 colors, 3=256 colors, 4=RGB).
- **Bit 2**: Use Gouraud Shading.
- **Bit 1**: Transparency Enable.
- **Bit 0**: End Code Enable.
