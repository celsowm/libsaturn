---
name: vdp1-sega-saturn
description: >
  Complete reference for the VDP1 (Video Display Processor 1) of the Sega Saturn for
  SH-2 assembly development. Use this skill whenever the user needs to
  program graphics on the Saturn, write command tables in VRAM, configure
  system registers, draw sprites/polygons/lines, implement color
  calculation (Gouraud shading, half-transparency, shadow), control the frame
  buffer, manage clipping, or any task involving low-level VDP1.
  Also use when the user mentions: draw commands, CMDCTRL, CMDPMOD,
  frame buffer flip, erase/write, plot trigger, textured/non-textured parts.
---

# VDP1 — Sega Saturn Drawing Processor

VDP1 is the sprite drawing IC of the Sega Saturn. It reads command tables from VRAM
and writes pixels to the frame buffer (DRAM), which is then displayed via VDP2.

> **For details on a specific topic, load the corresponding reference file:**
> - `references/system-registers.md` — Registers, addresses, bits
> - `references/command-table.md`    — Command table structure and fields
> - `references/commands.md`         — All drawing commands with ASM examples
> - `references/color-tables.md`     — Color bank, LUT, Gouraud shading, RGB modes

---

## 1. Address Map (absolute, base 5C00000H)

```
Relative Address   Absolute         Contents
000000–07FFFF       5C00000–5C7FFFF  VRAM (4 Mbit) — command/char/LUT/Gouraud tables
080000–0BFFFF       5C80000–5CBFFFF  Frame Buffer 0 (2 Mbit)
0C0000–0FFFFF       5CC0000–5CFFFFF  Frame Buffer 1 (2 Mbit)
100000–1FFFFF       5D00000–5DFFFFF  System Registers (word access only)
```

**Rule:** absolute address = relative + 5C00000H

---

## 2. System Registers — Quick Summary

| Abbr | Absolute Addr | R/W    | Function                              |
|------|---------------|--------|---------------------------------------|
| TVMR  | 5D00000H     | W-only | TV mode (TVM) + V-blank erase (VBE) |
| FBCR  | 5D00002H     | W-only | Frame buffer change mode            |
| PTMR  | 5D00004H     | W-only | Plot trigger (start drawing)        |
| EWDR  | 5D00006H     | W-only | Erase/write fill data               |
| EWLR  | 5D00008H     | W-only | Erase area upper-left (X1,Y1)       |
| EWRR  | 5D0000AH     | W-only | Erase area lower-right (X3,Y3)      |
| ENDR  | 5D0000CH     | W-only | Force-terminate drawing (write 0)   |
| EDSR  | 5D00010H     | R-only | Draw end status (CEF/BEF bits)      |
| LOPR  | 5D00012H     | R-only | Last processed command address /8H  |
| COPR  | 5D00014H     | R-only | Current command address /8H         |
| MODR  | 5D00016H     | R-only | Mirror of write-only registers |

> Always access in **word (16-bit)**. Never use DMA burst on system registers.
> See `references/system-registers.md` for detailed bit descriptions.

---

## 3. Minimal Initialization (ASM template)

```asm
; Base constants
VDP1_BASE   equ 5D00000H      ; System registers absolute base
VRAM_BASE   equ 5C00000H      ; VRAM base

; ─── Step 1: TV Mode (Normal NTSC 320x224, 16bpp, no rotation) ───────────────
    mov.w   #0000H, r0
    mov.l   #(VDP1_BASE + 0), r1   ; TVMR
    mov.w   r0, @r1

; ─── Step 2: Frame Buffer Change Mode (1-cycle, automatic 60fps) ────────────
    mov.w   #0000H, r0
    mov.l   #(VDP1_BASE + 2), r1   ; FBCR
    mov.w   r0, @r1

; ─── Step 3: Erase/Write Data (fill with black = 0) ──────────────────────
    mov.w   #0000H, r0
    mov.l   #(VDP1_BASE + 6), r1   ; EWDR
    mov.w   r0, @r1

; ─── Step 4: Erase area (320x224 in 16bpp: X1=0→reg=0 prohibited, use 1=8px) ─
; EWLR: bit14-9 = X1/8, bit8-0 = Y1
;   X1=0, Y1=0 → word = 0x0000  (X1=0 is prohibited! use 0 anyway, VDP1 forces 8)
    mov.w   #0x0000, r0
    mov.l   #(VDP1_BASE + 8), r1   ; EWLR
    mov.w   r0, @r1

; EWRR: bit15-9 = X3/8, bit8-0 = Y3
;   X3=319 → reg=40 (40*8-1=319), Y3=223
    mov.w   #((40 << 9) | 223), r0
    mov.l   #(VDP1_BASE + 0AH), r1 ; EWRR
    mov.w   r0, @r1

; ─── Step 5: Write command tables to VRAM ─────────────────────────────────
;   (see section 4 and file references/commands.md)

; ─── Step 6: Plot Trigger (start automatic drawing every frame) ──────────
    mov.w   #0002H, r0             ; PTM = 10B = auto-start
    mov.l   #(VDP1_BASE + 4), r1   ; PTMR
    mov.w   r0, @r1
```

---

## 4. Command Table — Structure (32 bytes, boundary 20H)

Each command table occupies **1EH useful bytes + 2 dummy bytes = 20H bytes**.
The first command table **must** be at VRAM offset 000000H.

```
Offset  Field      Bits         Description
+00H    CMDCTRL   [15]END      1=Draw End Command
                  [14:12]JP    Jump mode (000=next, 001=assign, 010=call, 011=return,
                               100=skip-next, 101=skip-assign, 110=skip-call, 111=skip-return)
                  [11:8]ZP     Zoom point (scaled sprite only; 0=two-coords mode)
                  [5:4]Dir     Character read direction (bit5=V-invert, bit4=H-invert)
                  [3:0]Comm    Command select (see table below)

+02H    CMDLINK   [15:2]       Link address / 8H (lower 2 bits = 00)

+04H    CMDPMOD   [15]MON      MSB ON (VDP2 shadow/window)
                  [12]HSS      High Speed Shrink (scaled/distorted only)
                  [11]Pclp     Pre-clipping disable
                  [10]Clip     User clipping enable
                  [9]Cmod      Clipping mode (0=inside, 1=outside)
                  [8]Mesh      Mesh/tiling enable
                  [7]ECD       End Code Disable
                  [6]SPD       Transparent Pixel Disable
                  [5:3]ColMd   Color mode (000=16col bank, 001=16col LUT,
                               010=64col bank, 011=128col bank, 100=256col bank,
                               101=32768col RGB)
                  [2:0]CC      Color Calculation (000=replace, 001=shadow,
                               010=half-lum, 011=half-transp, 100=Gouraud,
                               110=Gouraud+half-lum, 111=Gouraud+half-transp)

+06H    CMDCOLR              Color bank / LUT address/8H / non-textured color

+08H    CMDSRCA   [15:2]       Character address / 8H (lower 2 bits = 00)

+0AH    CMDSIZE   [12:8]       Char size X / 8  (1–63 → 8–504 pixels)
                  [7:0]        Char size Y       (1–255 pixels)

+0CH    CMDXA     [10:0]+sign  Vertex A X (sign-extended 11-bit, -1024..1023)
+0EH    CMDYA                  Vertex A Y
+10H    CMDXB                  Vertex B X  (or display width XB for scaled)
+12H    CMDYB                  Vertex B Y  (or display width YB for scaled)
+14H    CMDXC                  Vertex C X
+16H    CMDYC                  Vertex C Y
+18H    CMDXD                  Vertex D X
+1AH    CMDYD                  Vertex D Y

+1CH    CMDGRDA   [15:0]       Gouraud shading table address / 8H

+1EH    (dummy — 2 bytes, ignored by VDP1)
```

**Command Table (CMDCTRL[3:0] = Comm, with END=0):**

| Comm | Command                            |
|------|------------------------------------|
| 0000 | Normal sprite draw                 |
| 0001 | Scaled sprite draw                 |
| 0010 | Distorted sprite draw              |
| 0100 | Polygon draw (filled quad)         |
| 0101 | Polyline draw (outline quad)       |
| 0110 | Line draw                          |
| 1000 | User clipping coordinate set       |
| 1001 | System clipping coordinate set     |
| 1010 | Local coordinate set               |
| END=1| Draw end command (CMDCTRL=8000H)   |

---

## 5. Per-Frame Drawing Flow

```
1. CPU writes character pattern tables → VRAM
2. CPU writes color lookup tables → VRAM
3. CPU writes Gouraud shading tables → VRAM
4. CPU writes command tables → VRAM (starting at 000000H)
5. VDP1 starts automatically on frame swap (PTM=10B)
   or manually (write PTM=01B to PTMR)
6. VDP1 reads command tables sequentially, draws to back frame buffer
7. On reading Draw End Command → sets CEF=1 in EDSR and generates interrupt
8. On next frame buffer swap → drawn buffer becomes display
```

**Check drawing completion (polling):**
```asm
    mov.l   #5D00010H, r1       ; EDSR
.wait:
    mov.w   @r1, r0
    tst     #2, r0              ; CEF = bit 1
    bt      .wait               ; loop while CEF=0
```

---

## 6. Example: Simple Polygon (RGB, replace)

```asm
; Draws a filled quadrilateral in pure red (RGB 1F,00,00 = FC00H + MSB = BC00H)
; VRAM address: 5C00000H (offset 000000H)

    mov.l   #5C00000H, r4   ; VRAM base

    ; CMDCTRL: END=0, JP=000(next), ZP=0, Dir=00, Comm=0100(polygon)
    mov.w   #0x0004, r0 ;  0000 0000 0000 0100
    mov.w   r0, @r4

    ; CMDLINK: not used (next jump ignores CMDLINK)
    add     #2, r4
    mov.w   #0x0000, r0
    mov.w   r0, @r4

    ; CMDPMOD: MON=0, HSS=0, Pclp=0, Clip=0, Cmod=0, Mesh=0, ECD=1, SPD=1,
    ;          ColorMode=000, CC=000 (replace)
    ; bits: 0000 0000 1100 0000 = 00C0H
    add     #2, r4
    mov.w   #0x00C0, r0
    mov.w   r0, @r4

    ; CMDCOLR: non-textured color = red RGB (1,31,0,0) = 8400H | 8000H = 8400H
    ; RGB format: MSB=1, B[4:0], G[4:0], R[4:0]
    ; Pure red: R=1FH, G=00H, B=00H → 1_00000_00000_11111 = 801FH
    add     #2, r4
    mov.w   #0x801F, r0
    mov.w   r0, @r4

    ; CMDSRCA: not used for polygon (ignored)
    add     #2, r4
    mov.w   #0x0000, r0
    mov.w   r0, @r4

    ; CMDSIZE: not used for polygon (ignored)
    add     #2, r4
    mov.w   #0x0000, r0
    mov.w   r0, @r4

    ; CMDXA: Vertex A (10, 10)
    add     #2, r4
    mov.w   #10, r0 ; X=10
    mov.w   r0, @r4
    add     #2, r4
    mov.w   #10, r0 ; Y=10
    mov.w   r0, @r4

    ; CMDXB: Vertex B (100, 10)
    add     #2, r4
    mov.w   #100, r0
    mov.w   r0, @r4
    add     #2, r4
    mov.w   #10, r0
    mov.w   r0, @r4

    ; CMDXC: Vertex C (100, 80)
    add     #2, r4
    mov.w   #100, r0
    mov.w   r0, @r4
    add     #2, r4
    mov.w   #80, r0
    mov.w   r0, @r4

    ; CMDXD: Vertex D (10, 80)
    add     #2, r4
    mov.w   #10, r0
    mov.w   r0, @r4
    add     #2, r4
    mov.w   #80, r0
    mov.w   r0, @r4

    ; CMDGRDA: not used (Gouraud disabled)
    add     #2, r4
    mov.w   #0x0000, r0
    mov.w   r0, @r4

    ; Dummy +1EH (2 bytes)
    add     #2, r4
    mov.w   #0x0000, r0
    mov.w   r0, @r4

    ; ─── Draw End Command at +20H ─────────────────────────────────────────────
    add     #2, r4
    mov.w   #0x8000, r0     ; END=1
    mov.w   r0, @r4
```

---

## 7. Coordinates and Clipping

- **Frame buffer plane:** −1024 ≤ X ≤ 1023, −1024 ≤ Y ≤ 1023
- **System clipping** must be configured before first draw (undefined after reset):
  - Upper-left: fixed at (0,0)
  - Lower-right: specified via CMDXC/CMDYC in System Clipping Set Command
- **Local coordinates** are added to draw command coordinates
- Parts outside the frame buffer plane are simply not drawn

```asm
; System Clipping: area 0,0 → 319,223
; Command table for system clipping (Comm=1001B)
    mov.w   #0x0009, r0     ; CMDCTRL: END=0, JP=next, Comm=1001
    mov.w   r0, @r4
    ; ... skip ignored fields (write zeros) ...
    ; CMDXC offset +14H: lower-right X = 319
    ; CMDYC offset +16H: lower-right Y = 223
```

> For complete details on each register and bit, see `references/system-registers.md`
> For all commands with complete examples, see `references/commands.md`

---

## 8. Critical Assembly Tips

1. **VRAM address / 8H:** CMDSRCA, CMDGRDA, CMDLINK, and LOPR/COPR store `address/8H`. Always divide by 8 before writing.
2. **Sign-extended coordinates:** bits [15:11] must replicate bit 10 (sign). Use `exts.w` on SH-2.
3. **Boundary 20H:** every command table must be aligned to 32 bytes. Use `.align 5` or equivalent.
4. **Character pattern boundary 20H:** char patterns also on 20H boundary. Address 0 is reserved for the first command table.
5. **Word-only access on system registers:** never byte or longword.
6. **Don't access frame buffer being displayed:** only the back buffer (draw) is accessible.
7. **Color bank:** lower 4 bits must be 0 (e.g.: CMDCOLR for 16-color bank mode).
8. **RGB format:** MSB=1, bits [14:10]=B, [9:5]=G, [4:0]=R. Black RGB = 8000H (not 0000H, which is transparent!).
9. **After reset:** system/user clipping coordinates are undefined — always configure before drawing.
10. **ECD=1, SPD=1** required for polygons, polylines, and lines (they don't use character data).
