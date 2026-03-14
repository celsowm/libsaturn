---
name: sega-saturn-vdp-technical-reference
description: >
  Deep technical reference for Sega Saturn VDP1 (Sprites/Polygons) and VDP2 (Backgrounds).
  Use for precise hardware-level programming, register configuration, and command list generation.
  Triggers: "VDP1", "VDP2", "TVMR", "NBG0", "NBG1", "RBG0", "Command Table", "Sega Saturn video".
---

# Sega Saturn VDP Technical Reference

## 1. VDP1 — Sprite & Polygon Processor

VDP1 is a command-driven rasterizer. It processes a **Command Table** stored in VRAM.

### 1.1 VRAM Layout (Base: 0x25C00000)
- **0x00000 - 0x7FFFF**: VRAM (512KB). Stores Command Tables, Character Patterns, and Lookup Tables.
- **0x80000 - 0x9FFFF**: Frame Buffer (2x 128KB).

### 1.2 Command Table Entry (32 Bytes)
| Offset | Name | Description |
|---|---|---|
| 0x00 | `CMDCTRL` | Command Selection & Jump Mode |
| 0x02 | `CMDLINK` | Next Command Address (Address >> 3) |
| 0x04 | `CMDPMOD` | Primitive Mode (Color/Alpha/Gouraud) |
| 0x06 | `CMDCOLR` | Color Data / Palette Offset |
| 0x08 | `CMDSRCA` | Character Address (Address >> 3) |
| 0x0A | `CMDSIZE` | Character Size (Width/8, Height) |
| 0x0C | `CMDXA` | Vertex A X-coordinate |
| 0x0E | `CMDYA` | Vertex A Y-coordinate |
| 0x10 | `CMDXB` | Vertex B X-coordinate (or Delta X) |
| 0x12 | `CMDYB` | Vertex B Y-coordinate (or Delta Y) |
| 0x14 | `CMDXC` | Vertex C X-coordinate |
| 0x16 | `CMDYC` | Vertex C Y-coordinate |
| 0x18 | `CMDXD` | Vertex D X-coordinate |
| 0x1A | `CMDYD` | Vertex D Y-coordinate |
| 0x1C | `CMDGRDA` | Gouraud Shading Table Address (Address >> 3) |

### 1.3 Key Registers (Base: 0x25D00000)
- `TVMR` (0x00): TV Mode (Resolution, Frame Buffer Mode).
- `FBCR` (0x02): Frame Buffer Control (Erase, Change).
- `PTMR` (0x04): Plot Trigger (0=Stop, 1=Start, 2=Auto).
- `EWDR` (0x06): Erase Write Data (Color for clear).
- `EDSR` (0x10) [READ]: Status (CEF=Transfer End, BEF=Busy).

---

## 2. VDP2 — Background & Composition Processor

VDP2 manages up to 5 background layers (NBG0-3, RBG0) and performs final composition.

### 2.1 Background Layers
- **NBG0/NBG1**: Normal Scroll (Bitmap/Cell). Supports scaling/rotation (NBG0).
- **NBG2/NBG3**: Normal Scroll (Cell only).
- **RBG0**: Rotation Scroll (Mode 7 style).
- **Back Screen**: Solid color or gradient background.

### 2.2 Memory Management (Base: 0x25E00000)
- **VRAM (512KB)**: Usually split into 4 banks (A0, A1, B0, B1).
- **CRAM (4KB)**: Color Lookup Tables (Palettes).

### 2.3 Critical Registers
- `TVMD` (0x00): Display enable + Resolution (320x224, 640x448, etc.).
- `VRAM_CYC` (0x10-0x17): VRAM Cycle Patterns (Crucial for bandwidth allocation).
- `BGON` (0x20): Enable/Disable background layers.
- `CHCTLA/B` (0x26-0x2A): Character Control (Color depth, Pattern size).
- `PNCN0-3` (0x30-0x36): Pattern Name Control (Tile map address).
- `PRISA-D` (0x44-0x4A): Priority Control (Depth sorting of layers vs VDP1).

### 2.4 Priority System
Each layer (NBG, RBG, VDP1) is assigned a priority (0-7). Higher values appear in front.
- `PRINA`: Priority for NBG0/NBG1.
- `PRINB`: Priority for NBG2/NBG3.
- `PRISA`: Priority for VDP1 sprites (by type).

---

## 3. Reference Material
- Detailed register bitfields → `references/vdp1_registers.md`, `references/vdp2_registers.md`.
- VRAM Cycle Optimization → `references/vram_cycles.md`.
