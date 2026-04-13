# VDP1 — Color Tables (Color Bank, LUT, Gouraud, RGB)

---

## 1. Character Pattern Table

Sprite pixels stored in VRAM. Boundary: **20H bytes**.
Address 0x000000 is reserved for command tables. Minimum: VRAM[0x000020].

### Size by mode

| Color Mode | bpp | 8px×1 line | 8px×8 lines |
|------------|-----|------------|-------------|
| 0,1 (16 colors) | 4 | 4 bytes | 32 bytes |
| 2,3,4 (64/128/256) | 8 | 8 bytes | 64 bytes |
| 5 (32768 RGB)  | 16 | 16 bytes | 128 bytes |

Formula: `bytes = (sizeX × sizeY × bpp) / 8`

### Memory layout (4bpp, 8×3 pixels)

```
VRAM offset:  +0H    +1H    +2H    +3H
pixel row 0:  [0][1] [2][3] [4][5] [6][7]

              +4H    +5H    +6H    +7H
pixel row 1:  [8][9] [A][B] [C][D] [E][F]

              +8H    +9H    +AH    +BH
pixel row 2: [10][11][12][13][14][15][16][17]
```

Each nibble = 1 pixel (upper nibble = left pixel).

### Layout 8bpp (8×3 pixels)

```
+0H: pixel 0   +1H: pixel 1   +2H: pixel 2   ...  +7H: pixel 7
+8H: pixel 8   ...
+10H: pixel 16 ...
```

### Layout 16bpp (8×1 pixels)

```
+0H,+1H: pixel 0 (MSB in low byte)
+2H,+3H: pixel 1
...
+EH,+FH: pixel 7
```

---

## 2. Color Lookup Table (LUT)

Used when Color Mode = 1 (lookup table mode). Size: **20H bytes** (20H boundary).
Defines 16 colors as 16-bit values. The 4 bits of the character pattern index the table.

```
VRAM offset   Contents
+00H          16-bit color code for index 0H (transparent if SPD=0)
+02H          16-bit color code for index 1H
+04H          16-bit color code for index 2H
...
+1CH          16-bit color code for index EH
+1EH          16-bit color code for index FH (end code if ECD=0)
```

The 16-bit values can be:
- **Color bank code** (MSB=0): processed by VDP2 with color RAM
- **RGB code** (MSB=1): direct RGB, bypassing VDP2 color RAM

The LUT address is specified in CMDCOLR as `addr/8H`.
Since boundary is 20H: `CMDCOLR = LUT_VRAM_ADDR >> 3` (lower 2 bits = 00)

```asm
; LUT at VRAM[0x0060]
; CMDCOLR = 0x0060 >> 3 = 0x000C
    mov.w   #0x000C, r0
    mov.w   r0, @CMDCOLR_ADDR
```

---

## 3. Gouraud Shading Table

Size: **8H bytes** (4 words). Boundary: **8H bytes**.
Defines RGB brightness variation for 4 vertices of the part.

```
Table offset  Contents
+0H           RGB data for Vertex A (upper-left in sprites)
+2H           RGB data for Vertex B (upper-right)
+4H           RGB data for Vertex C (lower-right)
+6H           RGB data for Vertex D (lower-left)
```

For lines: only +0H (start) and +2H (end) are used; +4H and +6H ignored.

### RGB Format in Gouraud table

```
bit: [15] ignored | [14:10] B | [9:5] G | [4:0] R
```

### Value → correction mapping

| Hex value | Applied correction |
|------------|-------------------|
| 00H       | −10H (max darken) |
| 08H       | −08H              |
| 0FH       | −01H              |
| **10H**   | **0 (no change)** |
| 11H       | +01H              |
| 18H       | +08H              |
| 1FH       | +0FH (max lighten) |

- Result < 00H → clamp to 00H
- Result > 1FH → clamp to 1FH
- For "white Gouraud" (brightness only, no hue change): use same value for R=G=B

### Example: gradient from dark to light (left to right)

```asm
; Gouraud table at VRAM[0x0080]
; A(UL): R=G=B=08H → dark   → 0x1108 (B=02,G=02,R=08? let's calculate correctly)
; Format: bit14:10=B, bit9:5=G, bit4:0=R
; Value 08H for R,G,B: B=08<<10=0x2000, G=08<<5=0x0100, R=08=0x0008
; → 0x2108  (but MSB is ignored, so ok)
; Value 18H for R,G=B=18H → B=18<<10=0x6000, G=18<<5=0x0300, R=18=0x0018 → 0x6318
; A: dark (08,08,08)
; B: light (18,18,18)

    mov.l   #(VRAM_BASE + 0x0080), r1
    mov.w   #0x2108, r0   ; Vertex A: R=8,G=8,B=8 → 000 01000 01000 01000
    ; Recalculating: bit14:10=B=08→01000, bit9:5=G=08→01000, bit4:0=R=08→01000
    ; = 0b0_01000_01000_01000 = 0x2108
    mov.w   r0, @r1
    add     #2, r1

    mov.w   #0x6318, r0   ; Vertex B: R=18,G=18,B=18
    ; bit14:10=18→11000, bit9:5=18→11000, bit4:0=18→11000
    ; = 0b0_11000_11000_11000 = 0x6318
    mov.w   r0, @r1
    add     #2, r1

    mov.w   #0x6318, r0   ; Vertex C: same as B
    mov.w   r0, @r1
    add     #2, r1

    mov.w   #0x2108, r0   ; Vertex D: same as A
    mov.w   r0, @r1
```

---

## 4. Color Bank Mode

Used with Color Mode 0, 2, 3, 4. The character pattern pixel data has N bits.
The upper bits of the color bank are concatenated to form the 16-bit address
in VDP2 color RAM.

### Frame buffer data = color bank + pixel data

| Mode | bpp | Pixel data bits | Color bank bits | Total |
|------|-----|-----------------|-----------------|-------|
|  0   |  4  | [3:0]           | [15:4] (12 bits)| 16    |
|  2   |  8  | [5:0]           | [15:6] (10 bits)| 16    |
|  3   |  8  | [6:0]           | [15:7] (9 bits) | 16    |
|  4   |  8  | [7:0]           | [15:8] (8 bits) | 16    |

**CMDCOLR lower 4 bits must be 0** (they are OR'd with pixel data).

```asm
; Color bank for mode 0 (16 colors), using VDP2 palette 3
; Palette 3 starts at color RAM offset 0x30 (16 colors × 2 bytes × 3 = 0x60 if direct bank)
; Depends on how VDP2 is configured. Generic example:
    mov.w   #0x0030, r0   ; bank code (lower 4 bits = 0 required)
    mov.w   r0, @CMDCOLR_ADDR
```

### In 8bpp (high-res or rotation):
Only the lower byte of the 16 bits is written to the frame buffer.
For mode 0: upper 8 bits ignored → only 4 bits of palette code written.

---

## 5. Table Addressing Summary

| Table              | Boundary | Minimum Address | How to specify in command table |
|--------------------|----------|-----------------|--------------------------------|
| Command Table      | 20H      | 000000H         | Automatic (VDP1 starts at 0)  |
| Character Pattern  | 20H      | 000020H         | CMDSRCA = addr / 8H            |
| Color Lookup Table | 20H      | 000020H         | CMDCOLR = addr / 8H            |
| Gouraud Table      | 8H       | 000008H         | CMDGRDA = addr / 8H            |
| (any table)        | —        | max = 07FFE0H   | Don't exceed 080000H           |

**Never define tables beyond VRAM[07FFFFH].**

---

## 6. Color Calculation — Implementation Details

### Half-Transparent (CC=011)
- Background pixel must have MSB=1 for transparency to occur
- If MSB=0: normal replace is applied
- Formula: `result = (original + background) / 2`
- **6× slower** than replace
- Caution with polylines (pixels drawn 2× at vertices → double half-transp)

### Shadow (CC=001)
- Background with MSB=1: `background_RGB = background_RGB / 2`
- Background with MSB=0: no modification
- **6× slower**
- For 1/4 shadow: write the same command table 2× in VRAM

### Gouraud Shading (CC=100)
- Only in RGB mode (color mode 5 or LUT with RGB codes)
- Interpolates brightness correction between the 4 vertices
- Each R, G, B adjusted independently (can change hue)
- To keep hue: use R=G=B in the Gouraud table

### MSB ON (CMDPMOD bit 15)
- Sets MSB=1 on all drawn pixels
- Transparent (0000H) → 8000H (black with MSB set)
- Use with Replace (CC=000)
- Enables shadow/window on VDP2 for that area
