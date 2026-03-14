# VDP2 System Registers Reference (Selected)

Base Address: **0x25E00000**

## System Registers
| Offset | Mnemonic | Description |
|---|---|---|
| 0x00 | `TVMD` | TV Mode: Bit 15=DISP (Display Enable), Bits 5-4=Resolution. |
| 0x0E | `RAMCTL` | RAM Control: VRAM Bank allocation and CRAM mode. |
| 0x10 | `VRAM_CYC0`| VRAM Cycle Pattern for Bank A0. |
| 0x12 | `VRAM_CYC1`| VRAM Cycle Pattern for Bank A1. |
| 0x20 | `BGON` | Background Display Enable (NBG0-3, RBG0, EXBG). |

## Screen Control
| Offset | Mnemonic | Description |
|---|---|---|
| 0x26 | `CHCTLA` | Character Control for NBG0/NBG1 (Color mode, Tile size). |
| 0x28 | `CHCTLB` | Character Control for NBG2/NBG3. |
| 0x30 | `PNCN0` | Pattern Name Control for NBG0 (Map base address). |
| 0x32 | `PNCN1` | Pattern Name Control for NBG1. |

## Composition & Priority
| Offset | Mnemonic | Description |
|---|---|---|
| 0x44 | `PRISA` | Priority for Sprite Types 0 and 1. |
| 0x50 | `PRINA` | Priority for NBG0 and NBG1. |
| 0x52 | `PRINB` | Priority for NBG2 and NBG3. |
| 0x60 | `CLOFEN` | Color Offset Enable (Per layer). |
| 0x64 | `CLOFRL` | Color Offset Register (R/G/B offset values). |

## Scroll & Parameter Address
- `SCX0/SCY0` (0x70/0x72): Scroll X/Y for NBG0.
- `ZMXN0/ZMYN0` (0x74/0x76): Zoom X/Y for NBG0.
- `MPOFN` (0x3C): Map Offset Register (High bits of map addresses).
