# VDP2 Rotation Scroll (RBG0 / RBG1)

## Overview

RBG0 and RBG1 are full rotation/scaling scroll planes. Unlike NBG0–NBG3 which use simple X/Y offsets, rotation screens use a **64-byte rotation parameter table** stored in VRAM to define an affine transformation.

| Feature       | RBG0            | RBG1                          |
|---------------|-----------------|-------------------------------|
| Registers     | own set         | shares **NBG0** registers     |
| Planes        | 16 (A–P)        | 16                            |
| Parameters    | A or B (or both)| A only                        |
| Coefficient   | yes             | no                            |
| When active   | NBG0–3 still ok | NBG0–3 unavailable            |

---

## Rotation Parameter Table (64 bytes, VRAM-aligned)

Set address via `RPTA` registers at `+0x00BC` / `+0x00BE`.

Each parameter set is 16 × 4-byte (longword) values. All are **20.12 fixed-point** (20-bit integer, 12-bit fraction) unless noted.

| Byte offset | Symbol  | Description                                    |
|-------------|---------|------------------------------------------------|
| `0x00`      | Xst     | Scroll surface X start coordinate              |
| `0x04`      | Yst     | Scroll surface Y start coordinate              |
| `0x08`      | Zst     | Z start (usually 0 for 2D)                     |
| `0x0C`      | ΔXst    | X-per-screen-pixel increment (horizontal)      |
| `0x10`      | ΔYst    | Y-per-screen-pixel increment (horizontal)      |
| `0x14`      | ΔXst2   | X-per-screen-line increment (vertical)         |
| `0x18`      | ΔYst2   | Y-per-screen-line increment (vertical)         |
| `0x1C`      | C       | Matrix element (usually 0 for simple rotate)   |
| `0x20`      | F       | Matrix element (usually 0)                     |
| `0x24`      | Px      | Center of rotation X (screen coords)           |
| `0x28`      | Py      | Center of rotation Y                           |
| `0x2C`      | Pz      | Center of rotation Z (usually 0)               |
| `0x30`      | Cx      | Viewpoint X                                    |
| `0x34`      | Cy      | Viewpoint Y                                    |
| `0x38`      | Cz      | Viewpoint Z                                    |
| `0x3C`      | Mx/My   | Screen offset X (high word) / Y (low word)     |

### Fixed-Point Format
All 32-bit values use **20.12** format: bit 31 = sign, bits 30–12 = integer, bits 11–0 = fraction.

```
To convert float to 20.12:   int32 = (int)(value * 4096.0f)
To convert back:             float = raw_int32 / 4096.0f
```

---

## Simple Rotation Setup

To rotate surface by angle θ around screen center (Px, Py):

```
cos_theta = cos(θ) in 20.12 = (int)(cos(θ) * 4096)
sin_theta = sin(θ) in 20.12 = (int)(sin(θ) * 4096)

ΔXst  = cos_theta      (X advances cos per screen column)
ΔYst  = sin_theta      (Y advances sin per screen column)
ΔXst2 = -sin_theta     (X retreats sin per screen row)
ΔYst2 = cos_theta      (Y advances cos per screen row)

Px = Py = screen_center (e.g. 160, 112 for 320×224)

; Start coords (top-left of screen):
Xst = Px - cos_theta*Px + sin_theta*Py  (in 20.12)
Yst = Py - sin_theta*Px - cos_theta*Py  (in 20.12)
```

### Scale-only (no rotation):

```
ΔXst  = 1/scaleX  (in 20.12)    e.g. 2× zoom = 0x0800
ΔYst  = 0
ΔXst2 = 0
ΔYst2 = 1/scaleY  (in 20.12)
```

---

## Parameter Mode (`RPMD` register `+0x00B0` bits 1–0)

| RPMD | Mode       | Description                                  |
|------|------------|----------------------------------------------|
| 00   | Param A    | Only parameter set A is used                 |
| 01   | Param B    | Only parameter set B is used                 |
| 10   | A+B Window | Window 0 selects A; outside window uses B    |
| 11   | A+B Coeff  | Coefficient table selects A or B per line    |

---

## Bank Select (RAMCTL bits 7–0)

RBG0 uses a dedicated VRAM bank for its character pattern + PNT data. Set in `RAMCTL` (`+0x000E`):

```
bits 7-4: RDBSB (VRAM-B bank assignment for surface B):
  0001 = VRAM-B0
  0010 = VRAM-B1
  
bits 3-0: RDBSA (VRAM-A bank assignment for surface A):
  0001 = VRAM-A0
  0010 = VRAM-A1
```

When coefficient table is enabled (`CRKTE=1` in RAMCTL), do NOT assign those banks to other data.

---

## Coefficient Table

Used when `RPMD=11` to select rotation parameter A or B per screen line.

The coefficient table is stored in Color RAM (second half, addresses `0x05F00800`–`0x05F00FFF`) when `CRKTE=1` and Color RAM mode 1.

Coefficient data format (each entry = 32-bit):
```
Bit 31:     MGB (most-significant bit, used for line color screen insertion)
Bits 30-16: scale coefficient (k) in 1.14 fixed-point
Bit  0:     0=use parameter A, 1=use parameter B
```

Enable via `KTCTL` register `+0x00B4`.

---

## RBG0 Initialization Example (SH-2 Assembly, simple mode — Param A only)

```asm
; --- RBG0 Init: 320×224, 16-color cell, no rotation (identity transform) ---

VDP2_BASE   .equ 0x05F80000
VDP2_VRAM   .equ 0x05E00000

; 1. Set VRAM-A bank for RBG0 data (RDBSA=0001 → VRAM-A0)
;    Assumes RAMCTL already set for no partition
    mov.w   @(0x000E, r8), r1
    or      #0x01, r1          ; RDBSA00=1 → VRAM-A0 for surface A
    mov.w   r1, @(0x000E, r8)  ; RAMCTL

; 2. RBG0 character control: 16-color, cell format
    mov.w   @(0x002A, r8), r1  ; CHCTLB (read)
    ; R0CHCN=000 (16pal), R0BMEN=0, R0CHSZ=0 — default 0 is fine

; 3. Pattern name control: 1-word
    mov     #0x8000, r0
    mov.w   r0, @(0x0038, r8)  ; PNCR

; 4. Rotation parameter mode: A only
    mov     #0x0000, r0
    mov.w   r0, @(0x00B0, r8)  ; RPMD=00

; 5. Set rotation parameter table address in VRAM
;    Store at VRAM offset 0x000000 (start of VRAM-A0)
    mov     #0x0000, r0
    mov.w   r0, @(0x00BC, r8)  ; RPTA high
    mov.w   r0, @(0x00BE, r8)  ; RPTA low

; 6. Write identity rotation parameter table to VRAM
;    (no rotation, scale 1:1, display from scroll (0,0))
    mov.l   vram_base, r2
    ; Xst = 0
    mov     #0, r0
    mov.l   r0, @r2             ; offset 0x00: Xst
    mov.l   r0, @(4, r2)        ; offset 0x04: Yst
    mov.l   r0, @(8, r2)        ; offset 0x08: Zst
    ; ΔXst = 1.0 (4096 in 20.12)
    mov     #0x1000, r0
    mov.l   r0, @(12, r2)       ; ΔXst = 1.0
    mov     #0, r0
    mov.l   r0, @(16, r2)       ; ΔYst = 0
    mov.l   r0, @(20, r2)       ; ΔXst2 = 0
    mov     #0x1000, r0
    mov.l   r0, @(24, r2)       ; ΔYst2 = 1.0
    ; remaining parameters (C,F,Px,Py,Pz,Cx,Cy,Cz,Mx/My) = 0
    mov     #0, r0
    mov.l   r0, @(28, r2)
    ; ... (fill remaining 7 longwords with 0) ...

; 7. Enable RBG0
    mov     #0x0010, r0
    mov.w   r0, @(0x0020, r8)  ; BGON: R0ON=1

; 8. Priority
    mov     #0x0001, r0
    mov.w   r0, @(0x00FC, r8)  ; PRIR: RBG0 priority=1

.align 4
vram_base: .long 0x05E00000
```
