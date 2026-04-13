# VDP1 — Draw Commands (SH-2 Assembly Examples)

Each example assumes VRAM base = 5C00000H.
Helper macro to write word to VRAM:
```asm
; r4 = current VRAM pointer (manually incremented)
.macro wword val
    mov.w   #\val, r0
    mov.w   r0, @r4
    add     #2, r4
.endm
```

---

## General Macro: Initialize Pointer

```asm
    mov.l   #5C00000H, r4       ; first command table in VRAM[0]
```

---

## 1. System Clipping Coordinate Set (required after reset)

Comm=1001B, END=0. Upper-left fixed at (0,0).

```asm
; System Clipping: 0,0 → 319,223  (Normal NTSC 320×224)
    ; CMDCTRL: JP=000(next), ZP=0, Dir=00, Comm=1001
    mov.w   #0x0009, r0
    mov.w   r0, @r4   ; +00H CMDCTRL
    add     #2, r4

    mov.w   #0x0000, r0
    mov.w   r0, @r4   ; +02H CMDLINK (ignored, jump next)
    add     #2, r4

    ; +04H..+12H: ignored (write zeros)
    .rept 8
    mov.w   #0x0000, r0
    mov.w   r0, @r4
    add     #2, r4
    .endr

    ; +14H CMDXC: lower-right X = 319
    mov.w   #319, r0
    mov.w   r0, @r4
    add     #2, r4

    ; +16H CMDYC: lower-right Y = 223
    mov.w   #223, r0
    mov.w   r0, @r4
    add     #2, r4

    ; +18H..+1EH: ignored
    .rept 4
    mov.w   #0x0000, r0
    mov.w   r0, @r4
    add     #2, r4
    .endr
```

---

## 2. Local Coordinate Set

Comm=1010B. Offset added to all subsequent draw commands.

```asm
; Local Coordinates: (160, 112) = center of 320×224 screen
    mov.w   #0x000A, r0   ; CMDCTRL: Comm=1010
    mov.w   r0, @r4   add #2, r4

    mov.w   #0, r0
    mov.w   r0, @r4   add #2, r4    ; CMDLINK

    ; zeros for +04H..+0AH
    .rept 4
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4
    .endr

    ; +0CH CMDXA: local X = 160
    mov.w   #160, r0
    mov.w   r0, @r4   add #2, r4

    ; +0EH CMDYA: local Y = 112
    mov.w   #112, r0
    mov.w   r0, @r4   add #2, r4

    ; zeros for the rest
    .rept 8
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4
    .endr
```

---

## 3. Polygon Draw (Filled Quad)

Comm=0100B, END=0.
CMDPMOD: ECD=1 (bit7), SPD=1 (bit6), ColMd=000, CC=000 → `0x00C0`
CMDCOLR: direct RGB color. MSB=1.

```asm
; Polygon: 50×50 square at (10,10), white (RGB 1F,1F,1F = FFFFH)
    ; CMDCTRL: JP=000, ZP=0, Dir=00, Comm=0100
    mov.w   #0x0004, r0   mov.w r0, @r4   add #2, r4

    ; CMDLINK
    mov.w   #0x0000, r0   mov.w r0, @r4   add #2, r4

    ; CMDPMOD: ECD=1, SPD=1, ColMd=000, CC=000 (replace)
    mov.w   #0x00C0, r0   mov.w r0, @r4   add #2, r4

    ; CMDCOLR: white RGB = FFFFH
    mov.w   #0xFFFF, r0   mov.w r0, @r4   add #2, r4

    ; CMDSRCA: ignored
    mov.w   #0x0000, r0   mov.w r0, @r4   add #2, r4

    ; CMDSIZE: ignored
    mov.w   #0x0000, r0   mov.w r0, @r4   add #2, r4

    ; CMDXA=10, CMDYA=10
    mov.w   #10, r0   mov.w r0, @r4   add #2, r4
    mov.w   #10, r0   mov.w r0, @r4   add #2, r4

    ; CMDXB=59, CMDYB=10
    mov.w   #59, r0   mov.w r0, @r4   add #2, r4
    mov.w   #10, r0   mov.w r0, @r4   add #2, r4

    ; CMDXC=59, CMDYC=59
    mov.w   #59, r0   mov.w r0, @r4   add #2, r4
    mov.w   #59, r0   mov.w r0, @r4   add #2, r4

    ; CMDXD=10, CMDYD=59
    mov.w   #10, r0   mov.w r0, @r4   add #2, r4
    mov.w   #59, r0   mov.w r0, @r4   add #2, r4

    ; CMDGRDA: no Gouraud
    mov.w   #0x0000, r0   mov.w r0, @r4   add #2, r4

    ; dummy +1EH
    mov.w   #0x0000, r0   mov.w r0, @r4   add #2, r4
```

---

## 4. Line Draw (Simple Line)

Comm=0110B. Only vertices A and B.

```asm
; Line from (0,0) to (319,223), green = 83E0H
    mov.w   #0x0006, r0   mov.w r0, @r4   add #2, r4  ; CMDCTRL

    mov.w   #0x0000, r0   mov.w r0, @r4   add #2, r4  ; CMDLINK

    ; CMDPMOD: ECD=1, SPD=1
    mov.w   #0x00C0, r0   mov.w r0, @r4   add #2, r4

    ; CMDCOLR: green = 83E0H  (R=0,G=1FH,B=0, MSB=1 → 1_00000_11111_00000 = 83E0H)
    mov.w   #0x83E0, r0   mov.w r0, @r4   add #2, r4

    ; CMDSRCA, CMDSIZE: ignored
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4

    ; CMDXA=0, CMDYA=0
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4

    ; CMDXB=319, CMDYB=223
    mov.w   #319, r0   mov.w r0, @r4   add #2, r4
    mov.w   #223, r0   mov.w r0, @r4   add #2, r4

    ; C, D, GRDA: ignored → zeros (6 words)
    .rept 6
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4
    .endr

    ; dummy
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4
```

---

## 5. Normal Sprite Draw (Textured, Color Bank 16 colors)

Comm=0000B. CMDPMOD with color mode 0 (4bpp color bank).
- Character pattern: 4bpp, 8×8 pixels = 32 bytes, in VRAM[0x0020] (after the command table)
- Color bank: e.g. 0x0000 (first 16 colors of VDP2 palette)

```asm
; Normal sprite 8×8 at (20,20), color bank 0
    mov.w   #0x0000, r0   mov.w r0, @r4   add #2, r4  ; CMDCTRL: Comm=0000

    mov.w   #0x0000, r0   mov.w r0, @r4   add #2, r4  ; CMDLINK

    ; CMDPMOD: mode 0 (ColMd=000), replace, ECD=0, SPD=0
    ; → 0x0000 (leaves end code and transparency active)
    mov.w   #0x0000, r0   mov.w r0, @r4   add #2, r4

    ; CMDCOLR: color bank = 0x0000 (lower 4 bits = 0 required)
    mov.w   #0x0000, r0   mov.w r0, @r4   add #2, r4

    ; CMDSRCA: character pattern at VRAM[0x0020] → 0x0020/8 = 0x0004
    mov.w   #0x0004, r0   mov.w r0, @r4   add #2, r4

    ; CMDSIZE: 8×8 → sizeX/8=1 (bit[8]), sizeY=8
    ; word = (1 << 8) | 8 = 0x0108
    mov.w   #0x0108, r0   mov.w r0, @r4   add #2, r4

    ; CMDXA=20, CMDYA=20 (upper-left)
    mov.w   #20, r0   mov.w r0, @r4   add #2, r4
    mov.w   #20, r0   mov.w r0, @r4   add #2, r4

    ; B, C, D: ignored for normal sprite
    .rept 6
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4
    .endr

    ; CMDGRDA: no Gouraud
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4

    ; dummy
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4
```

---

## 6. Distorted Sprite (Rotation, Scaling, Deformation)

Comm=0010B. Four vertices define the final shape.

```asm
; 32×32 sprite rotated ~45° (4-vertex approximation)
; Character at VRAM[0x0040]: address/8 = 8
; Vertices (A=UL, B=UR, C=LR, D=LL) of a 40×40 diamond centered at (160,112):
;   A=(160,92), B=(180,112), C=(160,132), D=(140,112)

    mov.w   #0x0002, r0   mov.w r0, @r4   add #2, r4  ; CMDCTRL: Comm=0010

    mov.w   #0x0000, r0   mov.w r0, @r4   add #2, r4  ; CMDLINK

    ; CMDPMOD: ColMd=101 (RGB 16bpp), replace, ECD=1, SPD=1
    ; bits: 0000 0000 1100 1101 = 00CDH  → ColMd=101→bits[5:3]=101, ECD=1, SPD=1
    ; = 0b_0000_0000_1100_1101 = 0x00CD... let's calculate:
    ; MON=0, HSS=0, Pclp=0, Clip=0, Cmod=0, Mesh=0, ECD=1(bit7), SPD=1(bit6),
    ; ColMd=101(bits5:3), CC=000(bits2:0) → 0b00000000_11001000 = 0x00C8? 
    ; ColMd=101 → bits5:3 = 1,0,1 → bit5=1,bit4=0,bit3=1 → +0x28
    ; ECD=bit7=1 → +0x80, SPD=bit6=1 → +0x40
    ; Total: 0x40+0x80+0x28 = 0x00E8
    mov.w   #0x00E8, r0   mov.w r0, @r4   add #2, r4

    ; CMDCOLR: ignored in RGB mode
    mov.w   #0x0000, r0   mov.w r0, @r4   add #2, r4

    ; CMDSRCA: char at VRAM[0x0040] → 0x0040/8 = 8
    mov.w   #8, r0   mov.w r0, @r4   add #2, r4

    ; CMDSIZE: 32×32 → sizeX/8=4, sizeY=32 → (4<<8)|32 = 0x0420
    mov.w   #0x0420, r0   mov.w r0, @r4   add #2, r4

    ; CMDXA=160, CMDYA=92  (vertex A)
    mov.w   #160, r0   mov.w r0, @r4   add #2, r4
    mov.w   #92,  r0   mov.w r0, @r4   add #2, r4

    ; CMDXB=180, CMDYB=112 (vertex B)
    mov.w   #180, r0   mov.w r0, @r4   add #2, r4
    mov.w   #112, r0   mov.w r0, @r4   add #2, r4

    ; CMDXC=160, CMDYC=132 (vertex C)
    mov.w   #160, r0   mov.w r0, @r4   add #2, r4
    mov.w   #132, r0   mov.w r0, @r4   add #2, r4

    ; CMDXD=140, CMDYD=112 (vertex D)
    mov.w   #140, r0   mov.w r0, @r4   add #2, r4
    mov.w   #112, r0   mov.w r0, @r4   add #2, r4

    mov.w   #0, r0   mov.w r0, @r4   add #2, r4  ; CMDGRDA
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4  ; dummy
```

---

## 7. User Clipping Coordinate Set

Comm=1000B. Defines user clipping area.

```asm
; User clip: area (50,50) → (269,173)
    mov.w   #0x0008, r0   mov.w r0, @r4   add #2, r4  ; CMDCTRL

    mov.w   #0x0000, r0   mov.w r0, @r4   add #2, r4  ; CMDLINK

    ; +04H..+0AH: ignored (6 bytes = 3 words)
    .rept 3
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4
    .endr

    ; CMDXA=50 (upper-left X)
    mov.w   #50, r0   mov.w r0, @r4   add #2, r4
    ; CMDYA=50 (upper-left Y)
    mov.w   #50, r0   mov.w r0, @r4   add #2, r4

    ; CMDXB, CMDYB: ignored
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4

    ; CMDXC=269 (lower-right X)
    mov.w   #269, r0   mov.w r0, @r4   add #2, r4
    ; CMDYC=173 (lower-right Y)
    mov.w   #173, r0   mov.w r0, @r4   add #2, r4

    ; CMDXD, CMDYD, CMDGRDA, dummy: ignored
    .rept 4
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4
    .endr
```

---

## 8. Draw End Command

```asm
; Required at the end of each command frame
    mov.w   #0x8000, r0     ; END=1, rest ignored
    mov.w   r0, @r4
    add     #2, r4
    ; fill +02H..+1EH (15 words) with 0 (good practice)
    .rept 15
    mov.w   #0, r0   mov.w r0, @r4   add #2, r4
    .endr
```

---

## 9. Jump/Subroutine Pattern

```asm
; Sequence with subroutine call:
; Main table at 0x0000: call table at 0x0060
; Sub table at 0x0060: two polygons, then return

; MAIN TABLE (0x0000): Comm=polygon, JP=010 (call), CMDLINK=0x0060/8=0x000C
    mov.w   #0x5004, r0   ; CMDCTRL: JP=010(call), Comm=0100(polygon)
    mov.w   r0, @r4   add #2, r4
    mov.w   #0x000C, r0   ; CMDLINK = 0x60/8 = 0xC
    mov.w   r0, @r4
    ; ... rest of main polygon ...

; SUB TABLE (0x0060): first polygon, JP=000(next)
; ... define polygon normally ...

; SUB TABLE (0x0080): second polygon, JP=011(return)
    mov.w   #0x6004, r0   ; CMDCTRL: JP=011(return), Comm=0100
    ; ... define second polygon ...
```

---

## RGB Color Reference

| Color        | Hex    | Binary MSB-B-G-R           |
|--------------|--------|----------------------------|
| Black        | 0x8000 | 1_00000_00000_00000        |
| Red          | 0x801F | 1_00000_00000_11111        |
| Green        | 0x83E0 | 1_00000_11111_00000        |
| Blue         | 0xFC00 | 1_11111_00000_00000        |
| White        | 0xFFFF | 1_11111_11111_11111        |
| Yellow       | 0x83FF | 1_00000_11111_11111        |
| Cyan         | 0xFFE0 | 1_11111_11111_00000        |
| Magenta      | 0xFC1F | 1_11111_00000_11111        |
| Gray 50%     | 0x8C63 | 1_01000_11000_10001 (≈)    |
| Transparent  | 0x0000 | (VDP2 treats as transparent) |

> **WARNING:** 0x0000 is transparent color! Correct black RGB = 0x8000.

---

## Quick CMDPMOD Calculation

```
CMDPMOD = (MON << 15) | (HSS << 12) | (Pclp << 11) | (Clip << 10) |
          (Cmod << 9) | (Mesh << 8) | (ECD << 7) | (SPD << 6) |
          (ColMd << 3) | CC
```

Common values:
```
0x00C0 → Non-textured (ECD=1,SPD=1,ColMd=0,CC=replace)
0x00C4 → Non-textured + Gouraud (ECD=1,SPD=1,ColMd=0,CC=100)
0x00E8 → RGB sprite (ECD=1,SPD=1,ColMd=101=RGB,CC=replace)
0x00EC → RGB sprite + Gouraud (ECD=1,SPD=1,ColMd=101,CC=100)
0x00EA → RGB sprite + Half-Transparent (ECD=1,SPD=1,ColMd=101,CC=011)
0x00C2 → Non-textured + Half-Luminance (ECD=1,SPD=1,CC=010)
0x0000 → 16col color bank sprite, replace, ECD/SPD enabled
0x0008 → 16col color bank sprite, ColMd=001 (LUT mode)
```
