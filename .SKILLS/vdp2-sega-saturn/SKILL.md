---
name: sega-saturn-vdp2
description: >
  Complete hardware reference for the Sega Saturn VDP2 (Video Display Processor 2),
  covering scroll screens (NBG0–NBG3, RBG0, RBG1), VRAM layout, Color RAM, all
  registers with addresses and bit fields, priority/color-calculation/shadow functions,
  and practical SH-2 assembly initialization sequences.
  Use this skill whenever working on: Saturn homebrew graphics, VDP2 register setup,
  scroll screen initialization, tile/bitmap rendering, palette setup, color RAM,
  rotation scroll (RBG0/RBG1), line scroll, VRAM bank partitioning, window functions,
  sprite-VDP2 priority interaction, color offset/fade effects, or any assembly task
  that touches the VDP2 hardware at address 0x05E00000–0x05FBFFFF.
  Also trigger for questions about: NBG layers, cell format, pattern name tables,
  character patterns, mosaic, shadow, color calculation transparency effects, or
  any Saturn display/graphics programming topic.
---

# Sega Saturn VDP2 — Assembly Programming Reference

> Detailed register tables → `references/registers.md`  
> VRAM access patterns → `references/vram-access.md`  
> Rotation scroll (RBG) math → `references/rotation-scroll.md`  
> Read **this file fully first**; load a reference only for byte-level detail.

---

## 1. VDP2 Overview

VDP2 drives **scroll planes and display priority**. It is separate from VDP1 (which handles sprites/polygons). VDP2 reads VRAM autonomously during scan and composites layers.

**Key facts:**
- Connected to 4 Mbit or 8 Mbit VRAM (VRAM-A + VRAM-B, each split into bank 0/1)
- Contains 32 Kbit internal Color RAM (CRAM)
- All registers are **16-bit, word-access only** (no byte access)
- All multi-byte register values are **big-endian**
- Most registers clear to 0 on power-on/reset → **must be initialized explicitly**
- VDP2 read has priority over CPU/DMA VRAM access → CPU access inserts wait states

---

## 2. Address Map (Absolute on Saturn Bus)

| Absolute Address       | Size     | Region           |
|------------------------|----------|------------------|
| `0x05E00000–0x05EFFFFF`| 1 MB     | **VDP2 VRAM**    |
| `0x05F00000–0x05F7FFFF`| 512 KB   | **VDP2 Color RAM** |
| `0x05F80000–0x05F9FFFF`| 128 KB   | **VDP2 Registers** |

All register offsets in this document are **relative to `0x05F80000`** (matching the manual's `180000H` notation where `180000H` = `0x05F80000`).

### VDP2 VRAM Banks (4 Mbit system)

```
0x05E00000 – 0x05E1FFFF   VRAM-A0  (128 KB)
0x05E20000 – 0x05E3FFFF   VRAM-A1  (128 KB, same as A0 if not partitioned)
0x05E40000 – 0x05E5FFFF   VRAM-B0  (128 KB)
0x05E60000 – 0x05E7FFFF   VRAM-B1  (128 KB, same as B0 if not partitioned)
```

### VDP2 VRAM Banks (8 Mbit system)

```
0x05E00000 – 0x05E3FFFF   VRAM-A0  (256 KB)
0x05E40000 – 0x05E7FFFF   VRAM-A1  (256 KB)
0x05E80000 – 0x05EBFFFF   VRAM-B0  (256 KB)
0x05EC0000 – 0x05EFFFFF   VRAM-B1  (256 KB)
```

---

## 3. Scroll Screens

VDP2 provides six scroll planes plus two special screens:

| Name   | Type             | Max Colors   | Scale | Rotate | Line Scroll | Notes                          |
|--------|------------------|--------------|-------|--------|-------------|--------------------------------|
| NBG0   | Normal Scroll    | 16M (RGB)    | Yes   | No     | Yes         | Shares regs with RBG1 when RBG1 active |
| NBG1   | Normal Scroll    | 32K (RGB)    | Yes   | No     | Yes         | Unavailable when EXBG active   |
| NBG2   | Normal Scroll    | 256 (pal)    | No    | No     | No          |                                |
| NBG3   | Normal Scroll    | 256 (pal)    | No    | No     | No          |                                |
| RBG0   | Rotation Scroll  | 16M (RGB)    | Any   | Yes    | No          | Full rotation/zoom             |
| RBG1   | Rotation Scroll  | 32K (RGB)    | Any   | Yes    | No          | Uses NBG0 registers            |
| LNCL   | Line Color       | —            | —     | —      | —           | Color-calc only, no characters |
| BACK   | Back Screen      | —            | —     | —      | —           | Shows when all others transparent |

**Simultaneous display constraints:**
- Two rotation screens (RBG0 + RBG1) → NBG0–NBG3 unavailable
- One rotation screen → normal screens still available
- EXBG active → NBG1 unavailable

---

## 4. Essential Register Quick Reference

> All offsets relative to `0x05F80000`. Write 16-bit words only.

### 4.1 TV Screen Mode Register — `TVMD` @ `+0x0000`

```
Bit 15: DISP    — 0=blank, 1=display (change 0→1 during V-blank only!)
Bit  8: BDCLMD  — border: 0=black, 1=back screen color
Bit 7-6: LSMD  — interlace: 00=none, 10=single, 11=double
Bit 5-4: VRESO — vertical res: 00=224, 01=240, 10=256 lines
Bit 2-0: HRESO — horizontal res:
         000=320px Normal-A   001=352px Normal-B
         010=640px Hi-Res-A   011=704px Hi-Res-B
         100=320px Excl-A     101=352px Excl-B (Hi-Vision)
         110=640px Excl-A     111=704px Excl-B (Hi-Vision)
```

### 4.2 VRAM Size Register — `VRSIZE` @ `+0x0006`

```
Bit 15: VRAMSZ — 0=4 Mbit, 1=8 Mbit  (MUST set before any VRAM write)
Bit 3-0: VER   — VDP2 version (read-only)
```

### 4.3 RAM Control Register — `RAMCTL` @ `+0x000E`

```
Bit 15:    CRKTE  — Color RAM coefficient table enable
Bit 13-12: CRMD   — Color RAM mode: 00=mode0(1024col), 01=mode1(2048col), 10=mode2
Bit 9:     VRBMD  — VRAM-B bank partition: 0=no, 1=split into B0+B1
Bit 8:     VRAMD  — VRAM-A bank partition: 0=no, 1=split into A0+A1
Bit 7-0:   RDBSxx — RBG0 data bank select (see references/rotation-scroll.md)
```

### 4.4 Screen Display Enable Register — `BGON` @ `+0x0020`

```
Bit 15: R0TPON  — RBG0 transparent enable
Bit 14: N3TPON  — NBG3 transparent enable
Bit 13: N2TPON  — NBG2 transparent enable
Bit 12: N1TPON  — NBG1 transparent enable (bit12 in manual = N1 index 9 → check refs)
Bit 11: N0TPON  — NBG0 transparent: 0=transparent code active, 1=disabled
Bit  4: R0ON    — RBG0 enable
Bit  3: N3ON    — NBG3 enable
Bit  2: N2ON    — NBG2 enable
Bit  1: N1ON    — NBG1 enable
Bit  0: N0ON    — NBG0 enable
```

### 4.5 Character Control Registers — `CHCTLA` @ `+0x0028`, `CHCTLB` @ `+0x002A`

**CHCTLA** (NBG0 + NBG1):
```
Bit 14-12: N1CHCN  — NBG1 color: 00=16pal 01=256pal 10=2048pal 11=32K RGB
Bit 11-10: N1BMSZ  — NBG1 bitmap size (if bitmap)
Bit  9:    N1BMEN  — NBG1 bitmap enable
Bit  8:    N1CHSZ  — NBG1 char size: 0=1×1 cell, 1=2×2 cells
Bit  6-4:  N0CHCN  — NBG0 color: 000=16pal 001=256pal 010=2048pal 011=32K 100=16M RGB
Bit  3-2:  N0BMSZ  — NBG0 bitmap size
Bit  1:    N0BMEN  — NBG0 bitmap enable
Bit  0:    N0CHSZ  — NBG0 char size
```

**CHCTLB** (NBG2, NBG3, RBG0):
```
Bit 14-12: R0CHCN  — RBG0 color count (same encoding as N0CHCN)
Bit  9:    R0BMEN  — RBG0 bitmap enable
Bit  8:    R0CHSZ  — RBG0 char size
Bit  5:    N3CHCN  — NBG3 color: 0=16pal, 1=256pal
Bit  4:    N3CHSZ  — NBG3 char size
Bit  1:    N2CHCN  — NBG2 color: 0=16pal, 1=256pal
Bit  0:    N2CHSZ  — NBG2 char size
```

### 4.6 Pattern Name Control Registers (cell format addressing)

| Screen | Register | Address Offset |
|--------|----------|----------------|
| NBG0   | PNCN0    | `+0x0030`      |
| NBG1   | PNCN1    | `+0x0032`      |
| NBG2   | PNCN2    | `+0x0034`      |
| NBG3   | PNCN3    | `+0x0036`      |
| RBG0   | PNCR     | `+0x0038`      |

Each register:
```
Bit 15: xxPNB   — pattern name data size: 0=2 words, 1=1 word
Bit 14: xxCNSM  — char num supplement mode: 0=10-bit+flip, 1=12-bit no flip
Bit  9: xxSPR   — supplementary special priority bit
Bit  8: xxSCC   — supplementary special color calculation bit
Bit 7-5: xxSPLT — supplementary palette number (bits 6-4 of palette)
Bit 4-0: xxSCN  — supplementary character number (bits 4-0 when 1-word mode)
```

### 4.7 Plane Size Register — `PLSZ` @ `+0x003A`

```
Bit 7-6: N3PLSZ  — NBG3 plane size
Bit 5-4: N2PLSZ  — NBG2 plane size
Bit 3-2: N1PLSZ  — NBG1 plane size
Bit 1-0: N0PLSZ  — NBG0 plane size
Values: 00=1H×1V page, 01=2H×1V pages, 10=2H×2V pages
```

### 4.8 Map Offset Register — `MPOFN` @ `+0x003C`, `MPOFR` @ `+0x003E`

Each scroll screen uses a 3-bit map offset added to the upper bits of the map register to form the full VRAM address. See `references/registers.md` for per-screen breakdown.

### 4.9 Normal Scroll Map Registers

| Screen | Planes A,B    | Planes C,D    |
|--------|---------------|---------------|
| NBG0   | `+0x0040`     | `+0x0042`     |
| NBG1   | `+0x0044`     | `+0x0046`     |
| NBG2   | `+0x0048`     | `+0x004A`     |
| NBG3   | `+0x004C`     | `+0x004E`     |

Each register holds two plane addresses (6 bits each):
```
Bit 13-8: Plane B address
Bit  5-0: Plane A address
```

### 4.10 Screen Scroll Value Registers

| Screen | X Integer/Frac           | Y Integer/Frac           |
|--------|--------------------------|--------------------------|
| NBG0   | `+0x0070`, `+0x0072`    | `+0x0074`, `+0x0076`    |
| NBG1   | `+0x0080`, `+0x0082`    | `+0x0084`, `+0x0086`    |
| NBG2   | `+0x0090` (11-bit only) | `+0x0092` (11-bit only) |
| NBG3   | `+0x0094` (11-bit only) | `+0x0096` (11-bit only) |

NBG0/NBG1 scroll is 20-bit fixed-point (3-bit fraction in lower register bits 10–8):
```
High word bits 10-0 = integer part
Low  word bits 10-8 = fractional part (1/8 pixel)
```

---

## 5. Color RAM Modes

| Mode | Encoding      | Colors  | Notes                              |
|------|---------------|---------|-------------------------------------|
| 0    | RGB 5-5-5 LSB | 1024    | Bit 0 unused; default after reset   |
| 1    | RGB 5-5-5 MSB | 2048    | Bit 15 = color-calc MSB enable      |
| 2    | RGB 5-5-5 LSB | 1024    | Upper 1 KB used for coefficient tbl |

Set via `RAMCTL[13:12]` (CRMD1, CRMD0).

**Color RAM absolute address:** `0x05F00000`  
Format per entry (mode 0/1/2): `XBBBBBGGGGGRRRRR` (word, 15-bit RGB)

---

## 6. VRAM Cycle Pattern (Timing)

VDP2 reads VRAM in cycles of 8 time-slots (T0–T7) in Normal mode, 4 slots (T0–T3) in Hi-Res/Exclusive. Each bank (A0/A1/B0/B1) has its own cycle pattern register.

| Register | Address Offset | Bank        |
|----------|----------------|-------------|
| CYCA0L   | `+0x0010`     | VRAM-A0 T0–T3 |
| CYCA0U   | `+0x0012`     | VRAM-A0 T4–T7 |
| CYCA1L   | `+0x0014`     | VRAM-A1 T0–T3 |
| CYCA1U   | `+0x0016`     | VRAM-A1 T4–T7 |
| CYCB0L   | `+0x0018`     | VRAM-B0 T0–T3 |
| CYCB0U   | `+0x001A`     | VRAM-B0 T4–T7 |
| CYCB1L   | `+0x001C`     | VRAM-B1 T0–T3 |
| CYCB1U   | `+0x001E`     | VRAM-B1 T4–T7 |

Each register = 4×4-bit access command fields (T0 in bits 15–12, T3 in bits 3–0):

```
Access commands:
0x0 = NBG0 pattern name read
0x1 = NBG1 pattern name read
0x2 = NBG2 pattern name read
0x3 = NBG3 pattern name read
0x4 = NBG0 character pattern read
0x5 = NBG1 character pattern read
0x6 = NBG2 character pattern read
0x7 = NBG3 character pattern read
0xC = NBG0 vertical cell scroll table read
0xD = NBG1 vertical cell scroll table read
0xE = CPU read/write
0xF = No access (idle)
```

**Typical 2-layer setup (NBG0 + NBG1, Normal mode, 4 Mbit, no bank split):**
```
VRAM-A:  T0=NBG0-PNT T1=NBG0-CPD T2=NBG1-PNT T3=NBG1-CPD T4=CPU T5=NBG0-CPD T6=NBG1-CPD T7=idle
VRAM-B:  all CPU/idle
CYCA0L = 0x0154  CYCA0U = 0xE5EF   (example only — tune to your layout)
```

---

## 7. Priority Function

Priority is a 3-bit number (0–7). Higher number = drawn on top.

### Priority Number Registers

| Screen | Register | Address | Bits  |
|--------|----------|---------|-------|
| NBG0   | PRINA    | `+0x00F8` | 10–8 |
| NBG1   | PRINA    | `+0x00F8` | 2–0  |
| NBG2   | PRINB    | `+0x00FA` | 10–8 |
| NBG3   | PRINB    | `+0x00FA` | 2–0  |
| RBG0   | PRIR    | `+0x00FC` | 2–0  |
| SP0–SP7| SPCTL   | `+0x00E0` | varies (see references/registers.md) |

When two layers share the same priority number, the fixed order applies:
SP > RBG0 > NBG0 > NBG1 > NBG2 > NBG3 > BACK

---

## 8. Color Calculation (Transparency/Alpha)

Color calculation blends the **top image** with the **second image** below it using a 32-step ratio.

Key registers:
- `CCCTL` @ `+0x00E4` — enable per screen + extended mode
- `CCRTNA` @ `+0x0108` — ratio for NBG0 (bits 4–0) and NBG1 (bits 12–8)
- `CCRNB`  @ `+0x010A` — ratio for NBG2/NBG3
- `CCRR`   @ `+0x010C` — ratio for RBG0
- `CCRLB`  @ `+0x010E` — ratio for Line Color + Back

Ratio encoding: `00000` = 31:1 (top almost opaque), `10000` = 16:16 (50%), `11111` = 0:32 (fully transparent).

---

## 9. Color Offset (Fade In/Out)

Two independent offset sets (A and B), each with signed 9-bit R/G/B values.

| Register | Address | Purpose             |
|----------|---------|---------------------|
| CLOFEN   | `+0x0110` | Enable per screen  |
| CLOFSL   | `+0x0112` | Select A or B per screen |
| COAR     | `+0x0114` | Offset A — Red     |
| COAG     | `+0x0116` | Offset A — Green   |
| COAB     | `+0x0118` | Offset A — Blue    |
| COBR     | `+0x011A` | Offset B — Red     |
| COBG     | `+0x011C` | Offset B — Green   |
| COBB     | `+0x011E` | Offset B — Blue    |

Values are signed (bit 8 = sign). Range: −255 to +255. Clamped to 0x00–0xFF.

---

## 10. Shadow Function

Normal shadow: sprite writes a special dot pattern (all color bits = 1, LSB = 0). The scroll screen below has its brightness halved.

MSB shadow (types 2–7 only): set MSB of framebuffer data → creates shadow or transparent shadow.

Shadow enable register: `SDCTL` @ `+0x00E2`
```
Bit  8: TPSDSL — transparent shadow enable
Bit  5: BKSDEN — back screen shadow
Bit  4: R0SDEN — RBG0 shadow
Bit  3: N3SDEN — NBG3 shadow
Bit  2: N2SDEN — NBG2 shadow
Bit  1: N1SDEN — NBG1 shadow
Bit  0: N0SDEN — NBG0 shadow
```

---

## 11. Minimal VDP2 Init Sequence (SH-2 Assembly)

```asm
; -------------------------------------------------------
; VDP2 Minimal Initialization
; Assumes: Master SH-2, interrupts already disabled
; -------------------------------------------------------

VDP2_BASE   .equ 0x05F80000

    mov.l   vdp2_base_addr, r8      ; r8 = VDP2 register base

    ; Step 1: Set VRAM size (4 Mbit)
    mov     #0x0000, r0
    mov.w   r0, @(0x0006, r8)       ; VRSIZE: 4 Mbit (bit15=0)

    ; Step 2: Set TV mode: 320x224, no interlace, display on
    mov     #0x0100, r1             ; HRESO=000(320), VRESO=00(224), DISP=0 initially
    mov.w   r1, @(0x0000, r8)       ; TVMD — display still off

    ; Step 3: VRAM bank: no partition, Color RAM mode 1
    mov     #0x1000, r0             ; CRMD=01 (mode 1 = 2048 colors), no bank split
    mov.w   r0, @(0x000E, r8)       ; RAMCTL

    ; Step 4: Set VRAM cycle patterns (NBG0 only, Normal mode)
    ; VRAM-A: T0=NBG0-PNT(0), T1=NBG0-CPD(4), T2=CPU(E), T3=idle(F)
    ;         T4=NBG0-CPD(4), T5=idle(F), T6=idle(F), T7=idle(F)
    mov     #0x04EF, r0             ; wait, use long load
    mov.l   vram_cycleA_lo, r0
    mov.w   r0, @(0x0010, r8)       ; CYCA0L
    mov.l   vram_cycleA_hi, r0
    mov.w   r0, @(0x0012, r8)       ; CYCA0U
    ; VRAM-B: all CPU/idle
    mov     #0xEEEE, r0
    mov.w   r0, @(0x0018, r8)       ; CYCB0L
    mov     #0xFFFF, r0
    mov.w   r0, @(0x001A, r8)       ; CYCB0U

    ; Step 5: Configure NBG0 (16 colors, cell format, 1×1 cell)
    mov     #0x0000, r0
    mov.w   r0, @(0x0028, r8)       ; CHCTLA: N0CHCN=000(16pal), N0CHSZ=0

    ; Step 6: Pattern name: 1-word, char supplement mode 0
    mov     #0x8000, r0             ; N0PNB=1(1-word)
    mov.w   r0, @(0x0030, r8)       ; PNCN0

    ; Step 7: Plane size 1×1
    mov     #0x0000, r0
    mov.w   r0, @(0x003A, r8)       ; PLSZ

    ; Step 8: Map register — NBG0 planes point to VRAM address
    ; Pattern Name Table at VRAM offset 0x000000 (bank A0 base)
    ; Map value = (VRAM_offset >> 9) for 1-word PNT
    mov     #0x0000, r0
    mov.w   r0, @(0x0040, r8)       ; NBG0 planes A,B
    mov.w   r0, @(0x0042, r8)       ; NBG0 planes C,D

    ; Step 9: Set NBG0 scroll to 0,0
    mov     #0x0000, r0
    mov.w   r0, @(0x0070, r8)       ; N0SCX high
    mov.w   r0, @(0x0072, r8)       ; N0SCX low
    mov.w   r0, @(0x0074, r8)       ; N0SCY high
    mov.w   r0, @(0x0076, r8)       ; N0SCY low

    ; Step 10: Priority — NBG0 at priority 1
    mov     #0x0001, r0
    mov.w   r0, @(0x00F8, r8)       ; PRINA: NBG1=0, NBG0=1

    ; Step 11: Enable NBG0 display
    mov     #0x0001, r0
    mov.w   r0, @(0x0020, r8)       ; BGON: N0ON=1

    ; Step 12: Wait for V-blank, then enable display output
    ; (poll TVSTAT bit 3 = VBLANK)
_wait_vblank:
    mov.w   @(0x0004, r8), r0       ; TVSTAT
    tst     #0x08, r0               ; test VBLANK bit
    bf      _wait_vblank
_wait_active:
    mov.w   @(0x0004, r8), r0
    tst     #0x08, r0
    bt      _wait_active            ; wait until VBLANK clears
    ; Actually flip: wait for VBLANK rising edge
    ; Then set DISP
    mov     #0x8100, r0             ; DISP=1, BDCLMD=1, 320×224 no interlace
    mov.w   r0, @(0x0000, r8)       ; TVMD — display on

    rts
    nop

.align 4
vdp2_base_addr:  .long 0x05F80000
vram_cycleA_lo:  .long 0x04EF      ; T0=PNT0 T1=CPD0 T2=CPU T3=idle
vram_cycleA_hi:  .long 0x4FFF      ; T4=CPD0 T5=idle T6=idle T7=idle
```

---

## 12. Cell Format Data Layout

A **cell** is 8×8 pixels. Color count determines bytes per cell:

| Color Count | Bits/pixel | Bytes/cell |
|-------------|-----------|------------|
| 16          | 4         | 32         |
| 256         | 8         | 64         |
| 2048        | 11        | 128 (word per pixel) |
| 32768 (RGB) | 15        | 128        |
| 16M (RGB)   | 24        | 256        |

**Character (tile) data layout** (16-color, 4bpp):
```
Each row = 4 bytes (8 pixels × 4 bits)
Row 0: byte0=[px0 hi|px0 lo][px1...], byte1=[px2|px3], byte2=[px4|px5], byte3=[px6|px7]
...
Row 7: same pattern
Total: 32 bytes per 8×8 tile
```

**Pattern Name Table entry** (1-word mode):
```
Bit 15:    vertical flip
Bit 14:    horizontal flip
Bit 13-12: palette number (upper 2 bits)
Bit 11-10: special function bits (priority/color-calc)
Bit  9-0:  character number (tile index)
```

---

## 13. Common Pitfalls

| Mistake | Effect | Fix |
|---------|--------|-----|
| Byte-accessing VDP2 registers | Incorrect values / bus error | Always use `mov.w` (word) or `mov.l` (longword) |
| Writing registers before VRAMSZ set | VRAM layout undefined | Set VRAMSZ first |
| Setting DISP=1 outside V-blank | Visible glitch/tear | Toggle DISP only during V-blank |
| VRAM cycle conflict | Screen corruption/missing data | Ensure each PNT + CPD bank access is unique per cycle slot |
| Color RAM byte access | Bus error | Color RAM requires word/longword access only |
| Forgetting to enable screen (BGON) | Black plane | Set NxON=1 in BGON register |
| Using NBG0 regs while RBG1 enabled | Register conflict | RBG1 shares NBG0 registers |
| Pattern name table misaligned | Wrong tiles displayed | PNT must be on boundary = (plane size × 0x800) bytes |
| Map offset wrong | Random tile data | Calculate: address = (MapReg[5:0] << 9) \| (MapOffset[2:0] << 17) |

---

## 14. V-Blank Polling (SH-2)

```asm
; Wait for start of V-blank
_vblank_wait:
    mov.l   tvstat_addr, r1
    mov.w   @r1, r0
    tst     #0x08, r0           ; VBLANK bit (bit 3)
    bt      _vblank_wait        ; loop while VBLANK=0
    rts
    nop

.align 4
tvstat_addr: .long 0x05F80004   ; TVSTAT register
```

---

## Reference Files

Load these for deeper detail when needed:

- **`references/registers.md`** — Complete register map (all 80+ registers with addresses, bit fields, and values)
- **`references/vram-access.md`** — VRAM cycle pattern selection rules, access command tables, example configurations for 1–4 layer setups
- **`references/rotation-scroll.md`** — RBG0/RBG1 rotation parameter table format, matrix math, coefficient table, bank select bits
