# VDP1 — Command Table (Detailed Structure)

Each command table has **1EH (30) useful bytes** + 2 dummy bytes = **20H (32) total bytes**.
They must be aligned on **20H** boundary in VRAM.
The first command table **must** start at VRAM offset `000000H`.

---

## Complete Layout

```
Offset  Name      Bits [15:0]                           Description
+00H    CMDCTRL   [15]END [14:12]JP [11:8]ZP [5:4]Dir [3:0]Comm
+02H    CMDLINK   [15:2] link_addr/8H, [1:0]=00
+04H    CMDPMOD   [15]MON [12]HSS [11]Pclp [10]Clip [9]Cmod [8]Mesh [7]ECD [6]SPD [5:3]ColMd [2:0]CC
+06H    CMDCOLR   color bank / LUT addr/8H / non-textured color
+08H    CMDSRCA   [15:2] char_addr/8H, [1:0]=00
+0AH    CMDSIZE   [12:8] sizeX/8 (1..63), [7:0] sizeY (1..255)
+0CH    CMDXA     [10:0]+sign  vertex A X
+0EH    CMDYA     [10:0]+sign  vertex A Y
+10H    CMDXB     [10:0]+sign  vertex B X (or display width XB)
+12H    CMDYB     [10:0]+sign  vertex B Y (or display width YB)
+14H    CMDXC     [10:0]+sign  vertex C X
+16H    CMDYC     [10:0]+sign  vertex C Y
+18H    CMDXD     [10:0]+sign  vertex D X
+1AH    CMDYD     [10:0]+sign  vertex D Y
+1CH    CMDGRDA   [15:0] gouraud_table/8H
+1EH    (dummy — 2 bytes ignored)
```

---

## CMDCTRL (+00H)

### END — Bit 15
- `0` = normal command (use Comm for type)
- `1` = **Draw End Command** (Comm is ignored; write `8000H` in this word)

### JP — Jump Mode (bits 14:12)

| JP  | Mode          | Behavior                                                        |
|-----|---------------|----------------------------------------------------------------|
| 000 | Jump Next     | Process and go to address+20H (CMDLINK ignored)                |
| 001 | Jump Assign   | Process and jump to CMDLINK                                     |
| 010 | Jump Call     | Process and call CMDLINK as subroutine (1 nesting level)        |
| 011 | Jump Return   | Process and return to main routine (CMDLINK ignored)           |
| 100 | Skip Next     | Don't process, go to address+20H (CMDLINK ignored)             |
| 101 | Skip Assign   | Don't process, jump to CMDLINK                                 |
| 110 | Skip Call     | Don't process, call CMDLINK as subroutine                       |
| 111 | Skip Return   | Don't process, return to main routine                          |

> Subroutines: only 1 nesting level. Don't use jump call inside a subroutine.

### ZP — Zoom Point (bits 11:8) — only for Scaled Sprite

| ZP  | Reference Point           |
|-----|--------------------------|
| 0H  | Specifies two vertices (A=upper-left, C=lower-right) |
| 5H  | Upper-left               |
| 6H  | Upper-center             |
| 7H  | Upper-right              |
| 9H  | Center-left              |
| AH  | Center-center            |
| BH  | Center-right             |
| DH  | Lower-left               |
| EH  | Lower-center             |
| FH  | Lower-right              |

For all other commands: ZP = 0H.

### Dir — Character Read Direction (bits 5:4)

| Dir | Bit 5 (V) | Bit 4 (H) | Effect                        |
|-----|-----------|-----------|------------------------------|
| 00  | 0         | 0         | Normal                       |
| 01  | 0         | 1         | Horizontal inversion         |
| 10  | 1         | 0         | Vertical inversion           |
| 11  | 1         | 1         | Vertical+horizontal inversion |

For non-textured parts: Dir = 00B.

### Comm — Command Select (bits 3:0)

| Comm | Command                           |
|------|-----------------------------------|
| 0000 | Normal Sprite Draw                |
| 0001 | Scaled Sprite Draw                |
| 0010 | Distorted Sprite Draw             |
| 0100 | Polygon Draw (filled quadrangle)  |
| 0101 | Polyline Draw (outline quadrangle)|
| 0110 | Line Draw                        |
| 1000 | User Clipping Coordinate Set      |
| 1001 | System Clipping Coordinate Set    |
| 1010 | Local Coordinate Set             |

---

## CMDLINK (+02H)

```
bits [15:2] = next command table address / 8H
bits [1:0]  = 00 (fixed, boundary 20H ÷ 8 → lower 2 bits always 0)
```

Example: command table at VRAM offset 0x0040:
```asm
    mov.w   #(0x0040 >> 3), r0   ; = 0x0008
    mov.w   r0, @(CMDLINK_ADDR)
```

---

## CMDPMOD (+04H)

### MON — MSB ON (bit 15)
- `1` = Sets MSB of all pixels drawn in the frame buffer (for shadow/window on VDP2)
- Use with Replace (CC=000). Don't combine with color calculation.
- In mesh mode: MSB is set at "mesh on" positions.

### HSS — High Speed Shrink (bit 12)
- Valid only for scaled/distorted sprite
- `0` = precision (samples all positions)
- `1` = speed (samples only even or odd coordinates, according to EOS on FBCR)
- With HSS=1: end code is ignored (even when ECD=0)
- With HSS=1 on reduction: must use ECD=1

### Pclp — Pre-Clipping Disable (bit 11)
- `0` = pre-clipping active (detects completely out-of-area lines → doesn't draw)
- `1` = no pre-clipping (useful for small sprites where overhead > gain)

### Clip — User Clipping Enable (bit 10)
### Cmod — Clipping Mode (bit 9)

| Clip | Cmod | Behavior                            |
|------|------|-------------------------------------|
|  0   |  0   | User clipping disabled              |
|  0   |  1   | FORBIDDEN                          |
|  1   |  0   | Inside drawing mode (draw inside)   |
|  1   |  1   | Outside drawing mode (draw outside)  |

> System clipping is always active. User clipping is additional.

### Mesh (bit 8)
- `1` = mesh processing: only pixels where (X+Y) is even are drawn
- Caution: lines at 45° starting at odd coordinates may draw nothing

### ECD — End Code Disable (bit 7)
- `0` = end code active: when reading 2 end codes horizontally, that line terminates
- `1` = disabled: end code treated as normal color
- **ECD=1 required** for polygons, polylines, lines
- With HSS=1 and reduction: use ECD=1

End codes by color mode:
| Mode | End Code |
|------|----------|
| 0,1  | FH (4 bits)  |
| 2,3,4| FFH (8 bits) |
| 5    | 7FFFH (16 bits) |

### SPD — Transparent Pixel Disable (bit 6)
- `0` = transparency active: pixels with transparent color code are not drawn
- `1` = disabled
- **SPD=1 required** for polygons, polylines, lines
- When SPD=0 and ECD=0: maximum 14 usable colors (mode 0)

Transparent color codes:
| Mode | Transparent Code |
|------|------------------|
| 0,1  | 0H              |
| 2,3,4| 00H             |
| 5    | 0000H           |

### Color Mode Bits (bits 5:3)

| Bits | Mode | Colors   | Type       | bpp |
|------|------|----------|------------|-----|
| 000  |  0   | 16       | Color bank | 4   |
| 001  |  1   | 16       | LUT        | 4   |
| 010  |  2   | 64       | Color bank | 8   |
| 011  |  3   | 128      | Color bank | 8   |
| 100  |  4   | 256      | Color bank | 8   |
| 101  |  5   | 32768    | RGB        | 16  |

For non-textured parts: ColMd = 000B.

### Color Calculation Bits (bits 2:0)

| Bits | Mode                      | Original   | Background | Restriction         |
|------|---------------------------|------------|------------|---------------------|
| 000  | Replace                   | 1×         | —          | None                |
| 001  | Shadow                    | —          | ½ (if MSB=1) | RGB background   |
| 010  | Half-Luminance            | ½          | —          | RGB original        |
| 011  | Half-Transparent          | ½          | ½ (if MSB=1) | RGB original     |
| 100  | Gouraud                   | Gouraud    | —          | RGB original        |
| 101  | FORBIDDEN                 |            |            |                     |
| 110  | Gouraud + Half-Lum        | Gouraud×½  | —          | RGB original        |
| 111  | Gouraud + Half-Transp     | Gouraud×½  | ½ (if MSB=1) | RGB both          |

> Shadow and Half-Transparent are 6× slower. Not possible in 8bpp (use replace).

---

## CMDCOLR (+06H)

| Part Type         | Color Mode | CMDCOLR Contents                      |
|-------------------|------------|---------------------------------------|
| Textured (sprite) | Bank (0,2,3,4) | Color bank (lower 4 bits = 0)      |
| Textured (sprite) | LUT (1)    | LUT address / 8H (lower 2 bits=00)   |
| Textured (sprite) | RGB (5)    | Ignored                               |
| Non-textured      | any        | Direct color (16-bit, written to FB)  |

**Color bank by mode:**
- Mode 0 (4bpp): bits[15:4]=color bank, bits[3:0]=0000
- Mode 2 (6bpp): bits[15:6]=color bank, bits[5:0]=000000  → in practice lower 4 bits = 0
- Mode 3 (7bpp): bits[15:7]=color bank, bits[6:0]=0
- Mode 4 (8bpp): bits[15:8]=color bank, bits[7:0]=0

**Non-textured color (RGB):** MSB=1, B[14:10], G[9:5], R[4:0]
- Pure red: 0x801F | Pure green: 0x83E0 | Pure blue: 0xFC00 | White: 0xFFFF | Black: 0x8000

---

## CMDSRCA (+08H)

```
bits [15:2] = character pattern address in VRAM / 8H
bits [1:0]  = 00 (boundary 20H ÷ 8 → lower 2 bits 0, but boundary requires lower 3 bits = 0 for 20H/8)
```

Boundary: character patterns must be aligned on **20H bytes**.
Address 0x000000 is reserved for the command table → character patterns starting at 0x000020 (minimum).

---

## CMDSIZE (+0AH)

```
bits [12:8] = sizeX / 8   → 1..63 → 8..504 horizontal pixels
bits  [7:0] = sizeY        → 1..255 vertical pixels
bits [15:13] = 00 (fixed)
```

Example: 32×32 sprite:
```asm
    mov.w   #((4 << 8) | 32), r0   ; sizeX/8=4→32px, sizeY=32
    mov.w   r0, @CMDSIZE_ADDR
```

---

## CMDXA..CMDYD (+0CH..+1AH)

Coordinates in **two's complement, 11-bit with sign-extension**.
Valid range: −1024 ≤ coord ≤ 1023.
Bits [15:11] must replicate bit 10 value (sign extension).

```asm
; Writing negative coordinate (-50):
; -50 in two's complement 11-bit = 0x7CE
; sign-extended to 16-bit: 0xFFCE
    mov.w   #-50, r0    ; SH-2: immediate sign-extended automatically
    mov.w   r0, @CMDXA_ADDR
```

**Usage by command:**

| Command             | XA,YA           | XB,YB             | XC,YC           | XD,YD    |
|---------------------|-----------------|-------------------|-----------------|----------|
| Normal Sprite       | Vertex A (UL)   | —                 | —               | —        |
| Scaled (2 coords)   | Vertex A (UL)   | —                 | Vertex C (LR)   | —        |
| Scaled (zoom point) | Zoom point coord| Display width X,Y | —               | —        |
| Distorted Sprite    | Vertex A (UL)   | Vertex B (UR)     | Vertex C (LR)   | Vertex D (LL) |
| Polygon             | Vertex A        | Vertex B          | Vertex C        | Vertex D |
| Polyline            | Vertex A        | Vertex B          | Vertex C        | Vertex D |
| Line                | Vertex A        | Vertex B          | —               | —        |
| User Clipping       | UL coord        | —                 | LR coord        | —        |
| System Clipping     | —               | —                 | LR coord        | —        |
| Local Coord         | Local offset    | —                 | —               | —        |

---

## CMDGRDA (+1CH)

```
bits [15:0] = Gouraud shading table address / 8H
```

Valid only when CC bits indicate Gouraud (100, 110, 111).
Gouraud table: 8H bytes, aligned on 8H-byte boundary.

```
Table addr + 0: RGB vertex A (UL for sprites)
Table addr + 2: RGB vertex B (UR)
Table addr + 4: RGB vertex C (LR)
Table addr + 6: RGB vertex D (LL)
```

RGB format in Gouraud table: MSB ignored, B[14:10], G[9:5], R[4:0].
Value 10H = no modification. 00H = −10H brightness. 1FH = +0FH.

---

## Fields Ignored by Command Type

| Command           | Ignored                               |
|-------------------|---------------------------------------|
| Normal Sprite     | CMDXB..CMDYD (ZP must be 0)           |
| Scaled (2 coords) | CMDXB,CMDYB, CMDXD,CMDYD              |
| Scaled (zoom)     | CMDXC..CMDYD                          |
| Polygon/Polyline  | CMDSRCA, CMDSIZE (non-textured)       |
| Line              | CMDXC..CMDYD, CMDSRCA, CMDSIZE        |
| System Clipping   | CMDPMOD, CMDCOLR, CMDSRCA, CMDSIZE, CMDXA..CMDXB, CMDYB, CMDXD..CMDYD |
| User Clipping     | CMDPMOD, CMDCOLR, CMDSRCA, CMDSIZE, CMDXB, CMDYB, CMDXD, CMDYD |
| Local Coord       | CMDPMOD, CMDCOLR, CMDSRCA, CMDSIZE, CMDXB..CMDYD |
| Draw End          | everything except CMDCTRL (+00H = 8000H) |
