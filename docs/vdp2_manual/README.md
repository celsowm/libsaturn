# VDP2 User's Manual - Chapter Index

Sega Saturn Video Display Processor 2 Reference Manual

Converted from `vdp2_sega_manual.pdf` (Version 1.1, Doc. #ST-58-R2-060194).

## Chapters

| # | Chapter | File | Description |
|---|---------|------|-------------|
| 0 | Front Matter | [00_front_matter.md](00_front_matter.md) | Title, notices, preface, table of contents |
| 1 | VDP2 Functions | [01_vdp2_functions.md](01_vdp2_functions.md) | Overview, system configuration, address map |
| 2 | TV Screen | [02_tv_screen.md](02_tv_screen.md) | Display modes, interlace, resolutions |
| 3 | RAM | [03_ram.md](03_ram.md) | VRAM/CRAM organization, bank partitioning |
| 4 | Scroll Screen | [04_scroll_screen.md](04_scroll_screen.md) | Cells, patterns, planes, maps, bitmaps |
| 5 | Normal Scroll Screen | [05_normal_scroll_screen.md](05_normal_scroll_screen.md) | NBG0-NBG3 scrolling, scaling, line scroll |
| 6 | Rotation Scroll Screen | [06_rotation_scroll_screen.md](06_rotation_scroll_screen.md) | RBG0/RBG1 rotation, perspective, parameters |
| 7 | Line Screen | [07_line_screen.md](07_line_screen.md) | Line color screen, back screen |
| 8 | Windows | [08_windows.md](08_windows.md) | Rectangular windows, line windows, sprite windows |
| 9 | Sprite Data | [09_sprite_data.md](09_sprite_data.md) | VDP1 sprite format, priority, color calculation |
| 10 | Pixels | [10_pixels.md](10_pixels.md) | Palette format, RGB format, special function codes |
| 11 | Priority Function | [11_priority_function.md](11_priority_function.md) | Screen priority, special priority |
| 12 | Color Calculations | [12_color_calculations.md](12_color_calculations.md) | Color blending, gradation, special calculations |
| 13 | Color Offset Function | [13_color_offset_function.md](13_color_offset_function.md) | Color offset for fade effects |
| 14 | Shadow Function | [14_shadow_function.md](14_shadow_function.md) | Normal shadow, MSB shadow |
| 15 | How to Use VDP2 | [15_how_to_use_vdp2.md](15_how_to_use_vdp2.md) | Operation flow, RAM usage, bit configuration |
| 16 | Quick Reference | [16_quick_reference.md](16_quick_reference.md) | Register map, bit lists, table references |

## Quick Reference - Key Registers

### VDP2 Base Address
- **Relative**: `0x000000`
- **Absolute**: `0x05F80000`

### Commonly Used Registers
| Register | Offset | Description |
|----------|--------|-------------|
| TVMD | 0x000 | TV Mode / Display Enable |
| TVSTAT | 0x004 | TV Status (VBlank flag) |
| RAMCTL | 0x00E | RAM Control (VRAM partition, CRAM mode) |
| BGON | 0x020 | Background Enable (NBG0-NBG3, RBG0) |
| CHCTLA | 0x028 | Character Control A (NBG0/NBG1) |
| CHCTLB | 0x02C | Character Control B (RBG0) |
| SCXIN0 | 0x070 | NBG0 Scroll X Integer |
| SCYIN0 | 0x074 | NBG0 Scroll Y Integer |
| PRINA | 0x0F8 | NBG Priority |

### VRAM Address
- **Relative**: `0x000000`
- **Absolute (uncached)**: `0x25E00000`

### CRAM Address
- **Relative**: `0x100000`
- **Absolute (uncached)**: `0x25F00000`

## Notes

- This manual was automatically split from the original `vdp2_sega_manual.md` (9841 lines).
- Chapter 16 (Quick Reference) contains 2318 lines of register documentation.
- For RBG0 rotation plane implementation, see Chapter 6.
- For NBG0/1/2/3 normal scrolling, see Chapter 5.
