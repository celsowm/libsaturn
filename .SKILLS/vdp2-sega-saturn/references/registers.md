# VDP2 Complete Register Map

All addresses are offsets from `0x05F80000` (absolute = `0x05F80000 + offset`).  
All registers are **16-bit (word)**. Access with `mov.w` only.  
All clear to `0x0000` on reset unless noted. "RO" = read-only.

---

## Table of Contents

1. [TV / Sync Registers](#1-tv--sync-registers)
2. [RAM Control](#2-ram-control)
3. [VRAM Cycle Pattern](#3-vram-cycle-pattern-registers)
4. [Screen Display Control](#4-screen-display-control)
5. [Character / Bitmap Control](#5-character--bitmap-control)
6. [Pattern Name Control](#6-pattern-name-control)
7. [Plane / Map Registers](#7-plane--map-registers)
8. [Scroll Value Registers](#8-scroll-value-registers)
9. [Zoom / Line Scroll](#9-zoom--line-scroll-registers)
10. [Rotation Scroll (RBG)](#10-rotation-scroll-registers)
11. [Window Registers](#11-window-registers)
12. [Sprite Control](#12-sprite-control)
13. [Priority Registers](#13-priority-registers)
14. [Color Calculation](#14-color-calculation-registers)
15. [Color Offset](#15-color-offset-registers)
16. [Shadow / Special](#16-shadow--special-registers)

---

## 1. TV / Sync Registers

### TVMD — TV Screen Mode `+0x0000` R/W
```
15: DISP     display enable (set during V-blank)
 8: BDCLMD   border: 0=black 1=back screen
 7: LSMD1  ┐ interlace mode
 6: LSMD0  ┘   00=none 10=single 11=double
 5: VRESO1 ┐ vertical res
 4: VRESO0 ┘   00=224 01=240 10=256
 2: HRESO2 ┐
 1: HRESO1 │ horizontal res
 0: HRESO0 ┘   000=320A 001=352B 010=640A 011=704B
               100=Excl320A 101=Excl352B 110=Excl640A 111=Excl704B
```

### EXTEN — External Enable `+0x0002` R/W
```
 9: EXLTEN   HV counter latch: 0=on-reg-read, 1=via ext signal
 8: EXSYEN   external sync: 0=off, 1=on
 1: DASEL    display area: 0=set-area, 1=full standard area
 0: EXBGEN   external screen input enable
```

### TVSTAT — TV Status `+0x0004` RO
```
 9: EXLTFG   HV counter latched (clears on read)
 8: EXSYFG   ext sync locked (clears on read)
 3: VBLANK   1=in V-blank
 2: HBLANK   1=in H-blank
 1: ODD      1=odd field
 0: PAL      1=PAL standard
```

### VRSIZE — VRAM Size `+0x0006` R/W (bit 15 R/W, bits 3–0 RO)
```
15: VRAMSZ   0=4 Mbit, 1=8 Mbit
 3-0: VER    version (read-only)
```

### HCNT — H Counter `+0x0008` RO
```
 9-0: HCT    horizontal counter value (latched via EXLTEN)
```

### VCNT — V Counter `+0x000A` RO
```
 9-0: VCT    vertical counter value
```

### VRAMCH — VRAM Change `+0x000C` R/W
```
 8: VRAMCE   VRAM change enable
 0: VRAMSL   VRAM select: 0=VRAM-A as CPU, 1=VRAM-B as CPU
```

---

## 2. RAM Control

### RAMCTL `+0x000E` R/W
```
15: CRKTE    Color RAM coefficient table enable
13: CRMD1  ┐ Color RAM mode
12: CRMD0  ┘   00=mode0(1024) 01=mode1(2048) 10=mode2(1024+coeff)
 9: VRBMD    VRAM-B partition: 0=no split, 1=B0+B1
 8: VRAMD    VRAM-A partition: 0=no split, 1=A0+A1
 7: RDBSB11┐
 6: RDBSB10│ RBG0 data bank (VRAM-B bank select for rotation surface B)
 5: RDBSB01│
 4: RDBSB00┘
 3: RDBSA11┐
 2: RDBSA10│ RBG0 data bank (VRAM-A bank select for rotation surface A)
 1: RDBSA01│
 0: RDBSA00┘
```

---

## 3. VRAM Cycle Pattern Registers

Each register = 4 × 4-bit time slots. T0 in bits 15–12 → T3 in bits 3–0.

| Register | Offset   | Bank          | Slots  |
|----------|----------|---------------|--------|
| CYCA0L   | `+0x0010`| VRAM-A0/A     | T0–T3  |
| CYCA0U   | `+0x0012`| VRAM-A0/A     | T4–T7  |
| CYCA1L   | `+0x0014`| VRAM-A1       | T0–T3  |
| CYCA1U   | `+0x0016`| VRAM-A1       | T4–T7  |
| CYCB0L   | `+0x0018`| VRAM-B0/B     | T0–T3  |
| CYCB0U   | `+0x001A`| VRAM-B0/B     | T4–T7  |
| CYCB1L   | `+0x001C`| VRAM-B1       | T0–T3  |
| CYCB1U   | `+0x001E`| VRAM-B1       | T4–T7  |

**Access command values (4 bits per slot):**
```
0x0  NBG0 Pattern Name Table read
0x1  NBG1 Pattern Name Table read
0x2  NBG2 Pattern Name Table read
0x3  NBG3 Pattern Name Table read
0x4  NBG0 Character Pattern Data read
0x5  NBG1 Character Pattern Data read
0x6  NBG2 Character Pattern Data read
0x7  NBG3 Character Pattern Data read
0xC  NBG0 Vertical Cell Scroll Table read
0xD  NBG1 Vertical Cell Scroll Table read
0xE  CPU read/write
0xF  No access (idle)
```

**Rules:**
- Normal mode: T0–T7 active (8 slots per bank per line)
- Hi-Res/Exclusive: T0–T3 only (4 slots)
- Each PNT read needs 1–2 slots depending on screen width
- RBG0 pattern/char reads use full bank (set via RAMCTL, not cycle registers)
- RBG1 auto-uses VRAM-B0 (char) and VRAM-B1 (PNT) — those bank cycle registers are ignored

---

## 4. Screen Display Control

### BGON — Screen Display Enable `+0x0020` R/W
```
15: R0TPON  RBG0 transparent code: 0=active, 1=disabled
14: N3TPON  NBG3 transparent code
13: N2TPON  NBG2 transparent code
12: N1TPON  NBG1 transparent code
11: N0TPON  NBG0 transparent code
 4: R0ON    RBG0 enable
 3: N3ON    NBG3 enable
 2: N2ON    NBG2 enable
 1: N1ON    NBG1 enable
 0: N0ON    NBG0 enable
```

### MZCTL — Mosaic Control `+0x0022` R/W
```
15-12: MZSZV  vertical mosaic size (1–16)
11- 8: MZSZH  horizontal mosaic size (1–16)
    4: R0MZE  RBG0 mosaic
    3: N3MZE  NBG3 mosaic
    2: N2MZE  NBG2 mosaic
    1: N1MZE  NBG1 mosaic
    0: N0MZE  NBG0 mosaic
```

---

## 5. Character / Bitmap Control

### CHCTLA — Char Control A `+0x0028` R/W (NBG0, NBG1)
```
14-12: N1CHCN  NBG1 color: 000=16pal 001=256pal 010=2048pal 011=32Krgb
11-10: N1BMSZ  NBG1 bitmap size (00=512×256 01=512×512 10=1024×256 11=1024×512)
    9: N1BMEN  NBG1 bitmap mode
    8: N1CHSZ  NBG1 cell size: 0=1×1, 1=2×2
 6- 4: N0CHCN  NBG0 color: 000=16pal 001=256pal 010=2048pal 011=32K 100=16M rgb
 3- 2: N0BMSZ  NBG0 bitmap size
    1: N0BMEN  NBG0 bitmap mode
    0: N0CHSZ  NBG0 cell size
```

### CHCTLB — Char Control B `+0x002A` R/W (NBG2, NBG3, RBG0)
```
14-12: R0CHCN  RBG0 color (same as N0CHCN encoding)
    9: R0BMEN  RBG0 bitmap
    8: R0CHSZ  RBG0 cell size
    5: N3CHCN  NBG3 color: 0=16pal, 1=256pal
    4: N3CHSZ  NBG3 cell size
    1: N2CHCN  NBG2 color: 0=16pal, 1=256pal
    0: N2CHSZ  NBG2 cell size
```

### BMPNA — Bitmap Palette NBG0/NBG1 `+0x002C` R/W
```
13: N1BMPR  NBG1 bitmap special priority
12: N1BMCC  NBG1 bitmap special color calc
10- 8: N1BMP  NBG1 bitmap palette number
 5: N0BMPR  NBG0 bitmap special priority
 4: N0BMCC  NBG0 bitmap special color calc
 2- 0: N0BMP  NBG0 bitmap palette number
```

### BMPNB — Bitmap Palette RBG0 `+0x002E` R/W
```
 5: R0BMPR  RBG0 special priority
 4: R0BMCC  RBG0 special color calc
 2-0: R0BMP  RBG0 bitmap palette number
```

---

## 6. Pattern Name Control

### PNCN0 — Pattern Name NBG0 `+0x0030` R/W
### PNCN1 — Pattern Name NBG1 `+0x0032` R/W
### PNCN2 — Pattern Name NBG2 `+0x0034` R/W
### PNCN3 — Pattern Name NBG3 `+0x0036` R/W
### PNCR  — Pattern Name RBG0 `+0x0038` R/W

All same format:
```
15: xxPNB    pattern name data size: 0=2 words, 1=1 word
14: xxCNSM   char number supplement mode:
             0=10-bit char# + flip bits selectable
             1=12-bit char# (no flip)
 9: xxSPR    supplementary special priority bit value
 8: xxSCC    supplementary special color calc bit value
 7-5: xxSPLT supplementary palette number (bits [6:4])
 4-0: xxSCN  supplementary character number (bits [4:0] when 1-word)
```

---

## 7. Plane / Map Registers

### PLSZ — Plane Size `+0x003A` R/W
```
 7-6: N3PLSZ  NBG3 plane: 00=1H×1V 01=2H×1V 10=2H×2V
 5-4: N2PLSZ  NBG2 plane
 3-2: N1PLSZ  NBG1 plane
 1-0: N0PLSZ  NBG0 plane
```

### MPOFN — Map Offset Normal `+0x003C` R/W
```
14-12: N3MP   NBG3 map offset (bits [17:15] of VRAM address)
10- 8: N2MP   NBG2 map offset
 6- 4: N1MP   NBG1 map offset
 2- 0: N0MP   NBG0 map offset
```

### MPOFR — Map Offset Rotation `+0x003E` R/W
```
 2- 0: R0MP   RBG0 map offset
```

### Normal Screen Map Registers

Each register holds **Plane A** (bits 5–0) and **Plane B** (bits 13–8):

| Screen | Planes A,B | Planes C,D |
|--------|-----------|-----------|
| NBG0   | `+0x0040` | `+0x0042` |
| NBG1   | `+0x0044` | `+0x0046` |
| NBG2   | `+0x0048` | `+0x004A` |
| NBG3   | `+0x004C` | `+0x004E` |

**Address formula:**  
`PNT_absolute = 0x05E00000 + ((MapOffset[2:0] << 17) | (MapReg[5:0] << 11))`

### Rotation Screen Map Registers `+0x0050` – `+0x006E`

RBG0 has 16 planes (A–P), each with its own 6-bit address field.  
Registers at `+0x0050` through `+0x006E` (16 registers × 2 bytes).

---

## 8. Scroll Value Registers

### NBG0 Scroll (20-bit fixed-point)
```
N0SCX  +0x0070  (high: bits 10-0 = integer)
       +0x0072  (low: bits 10-8 = frac/8)
N0SCY  +0x0074  (high)
       +0x0076  (low)
```

### NBG1 Scroll
```
N1SCX  +0x0080 / +0x0082
N1SCY  +0x0084 / +0x0086
```

### NBG2 Scroll (11-bit integer only)
```
N2SCX  +0x0090  bits 10-0
N2SCY  +0x0092  bits 10-0
```

### NBG3 Scroll (11-bit)
```
N3SCX  +0x0094
N3SCY  +0x0096
```

---

## 9. Zoom / Line Scroll Registers

### Zoom (Coordinate Increment) — NBG0
```
N0ZMXI  +0x0078  integer part H
N0ZMXF  +0x007A  fractional part H (bits 10-8)
N0ZMYI  +0x007C  integer part V
N0ZMYF  +0x007E  fractional part V
```

### Zoom — NBG1
```
N1ZMXI  +0x0088 / N1ZMXF +0x008A
N1ZMYI  +0x008C / N1ZMYF +0x008E
```

Normal value (no zoom) = `1.0` → integer=1, frac=0.

### ZMCTL — Reduction Enable `+0x0098` R/W
```
 9-8: N1ZMQT,N1ZMHF  NBG1 reduction: 00=none 01=1/2 10=1/4
 1-0: N0ZMQT,N0ZMHF  NBG0 reduction
```

### SCRCTL — Line/VCell Scroll Control `+0x009A` R/W
```
13-12: N1LSS    NBG1 line scroll interval
   11: N1LZMX   NBG1 line zoom H
   10: N1LSCY   NBG1 line scroll Y
    9: N1LSCX   NBG1 line scroll X
    8: N1VCSC   NBG1 vertical cell scroll
 5- 4: N0LSS    NBG0 line scroll interval
    3: N0LZMX   NBG0 line zoom H
    2: N0LSCY   NBG0 line scroll Y
    1: N0LSCX   NBG0 line scroll X
    0: N0VCSC   NBG0 vertical cell scroll
```

Line scroll interval: `00=1line 01=2lines 10=4lines 11=8lines`

### Line Scroll Table Address — NBG0 `+0x00A0`/`+0x00A2` R/W
### Line Scroll Table Address — NBG1 `+0x00A4`/`+0x00A6` R/W
### Vertical Cell Scroll Table Address `+0x009C`/`+0x009E` R/W

---

## 10. Rotation Scroll Registers

### RPMD — Rotation Parameter Mode `+0x00B0` R/W
```
 1-0: RPMD   00=Param A only, 01=Param B only, 10=A+B swap per window, 11=A+B per coefficient
```

### RPRCTL — Rotation Parameter Read Control `+0x00B2` R/W
```
 8: R0RPRD   RBG0 rotation parameter read enable
```

### KTCTL — Coefficient Table Control `+0x00B4` R/W
```
 8: R0KTEF   use coefficient table for RBG0
 4: R0KTLCL  coefficient data type (line color or zoom k)
```

### KTAOF — Coefficient Table Address Offset `+0x00B6` R/W

### OVPNRA — Screen-Over Pattern Name A `+0x00B8` R/W
### OVPNRB — Screen-Over Pattern Name B `+0x00BA` R/W

### RPTA — Rotation Parameter Table Address `+0x00BC`/`+0x00BE` R/W

**Rotation Parameter Table** (stored in VRAM):  
64 bytes per parameter set (A or B), 16 32-bit values:

| Offset | Parameter | Description                          |
|--------|-----------|--------------------------------------|
| 0x00   | Xst       | Screen start X (16.16 fixed-point)   |
| 0x04   | Yst       | Screen start Y                       |
| 0x08   | Zst       | Screen start Z (not used for 2D)     |
| 0x0C   | ΔXst      | X increment per screen X             |
| 0x10   | ΔYst      | Y increment per screen X             |
| 0x14   | ΔXst2     | X increment per screen Y             |
| 0x18   | ΔYst2     | Y increment per screen Y             |
| 0x1C   | C         | Matrix coefficient C                 |
| 0x20   | F         | Matrix coefficient F                 |
| 0x24   | Px        | Rotation center X                    |
| 0x28   | Py        | Rotation center Y                    |
| 0x2C   | Pz        | Rotation center Z                    |
| 0x30   | Cx        | View point X                         |
| 0x34   | Cy        | View point Y                         |
| 0x38   | Cz        | View point Z                         |
| 0x3C   | Mx/My     | Screen offset X/Y                    |

---

## 11. Window Registers

### WPSX0/WPEX0 — Window 0 H position `+0x00C0`/`+0x00C2` R/W
### WPSY0/WPEY0 — Window 0 V position `+0x00C4`/`+0x00C6` R/W
### WPSX1/WPEX1 — Window 1 H `+0x00C8`/`+0x00CA` R/W
### WPSY1/WPEY1 — Window 1 V `+0x00CC`/`+0x00CE` R/W

Start/end horizontal: 10-bit pixel position  
Start/end vertical: 10-bit line position

### WLTA0/WLTA1 — Line Window Table Address 0/1 `+0x00D0`/`+0x00D4` (2 words each)

### SPCTL — Sprite Control `+0x00E0` R/W (also controls sprite window)
```
 8: SPWINEN  sprite window enable (from sprite data type)
 4: SPTYPE   sprite type (0=type0…7 per bits 3-0, extended)
 3-0: SPTYPE sprite type selection (0x0–0xF)
```

### WCTLA — Window Control A `+0x00D8` R/W (NBG0, NBG1)
### WCTLB — Window Control B `+0x00DA` R/W (NBG2, NBG3)
### WCTLC — Window Control C `+0x00DC` R/W (RBG0)
### WCTLD — Window Control D `+0x00DE` R/W (sprite, color calc)

Per-screen window control bits (NBG0 shown, others shift by 8):
```
 7: N0LOG    window logic: 0=AND, 1=OR
 6: N0SWE    sprite window enable
 5: N0SWA    sprite window area: 0=inside=effective, 1=outside
 4: N0W1E    window 1 enable
 3: N0W1A    window 1 area
 2: N0W0E    window 0 enable
 1: N0W0A    window 0 area: 0=inside, 1=outside effective
```

---

## 12. Sprite Control

### SPCTL `+0x00E0` R/W

```
15-12: SPCCCS  sprite color calc condition select
    8: SPWINEN sprite window enable
 4- 0: SPTYPE  sprite type (0x0–0xF, defines bit assignment in VDP1 framebuffer)
```

**Sprite types 0–F** define how VDP1 framebuffer bits map to priority/color-calc/color-data. Types 0–7 are standard; 8–F are compact. See full sprite type table in VDP1 manual.

### SDCTL — Shadow Control `+0x00E2` R/W
```
 8: TPSDSL  transparent shadow sprite enable
 5: BKSDEN  back screen shadow
 4: R0SDEN  RBG0 shadow
 3: N3SDEN  NBG3 shadow
 2: N2SDEN  NBG2 shadow
 1: N1SDEN  NBG1 shadow
 0: N0SDEN  NBG0 shadow
```

---

## 13. Priority Registers

### PRINA `+0x00F8` R/W
```
10-8: N1PRIN  NBG1 priority (0–7)
 2-0: N0PRIN  NBG0 priority (0–7)
```

### PRINB `+0x00FA` R/W
```
10-8: N3PRIN  NBG3 priority
 2-0: N2PRIN  NBG2 priority
```

### PRIR `+0x00FC` R/W
```
 2-0: R0PRIN  RBG0 priority
```

### SPPR0–SPPR7 — Sprite Priority (in SPCTL area, see VDP1 manual for sprite-side; VDP2 reads from framebuffer)

### SPRINUM — Priority Number Register `+0x00F0`/`+0x00F2`/`+0x00F4`/`+0x00F6`
Up to 8 sprite priority levels mapped via SPCTL type selection.

---

## 14. Color Calculation Registers

### CCCTL — Color Calculation Control `+0x00E4` R/W
```
 8: BOKEN     gradation enable
 7: LCCEN     line color screen in color calc
 6: R0CCEN    RBG0 color calc enable
 5: N3CCEN    NBG3
 4: N2CCEN    NBG2
 3: N1CCEN    NBG1
 2: N0CCEN    NBG0
 1: SPCCEN    sprite color calc enable
```

### SFPRMD — Special Priority Mode `+0x00EA` R/W
```
 5-4: N1SPRM  NBG1 special priority mode: 00=per screen 01=per char 10=per dot
 3-2: N0SPRM  NBG0 special priority mode
 1-0: R0SPRM  RBG0 special priority mode
```

### CCMD — Color Calculation Mode (extended) `+0x00E6` R/W
```
 8: EXCCEN  extended color calc enable (up to 4 layers)
 7: LCCMD   line color calc mode
```

### SFCCMD — Special Color Calculation Mode `+0x00EE` R/W
```
 9-8: R0SCCM  RBG0: 00=per-screen 01=per-char 10=per-dot 11=per-MSB
 7-6: N3SCCM  NBG3
 5-4: N2SCCM  NBG2
 3-2: N1SCCM  NBG1
 1-0: N0SCCM  NBG0
```

### CCRTNA `+0x0108` R/W — Color Calc Ratio NBG0/NBG1
```
12-8: N1CCRT  NBG1 ratio (0=31:1 … 31=0:32)
 4-0: N0CCRT  NBG0 ratio
```

### CCRNB `+0x010A` — NBG2/NBG3
### CCRR  `+0x010C` — RBG0 (bits 4–0)
### CCRLB `+0x010E` — LNCL (bits 4–0), BACK (bits 12–8)

### Sprite Color Calc Ratio `+0x0100`/`+0x0102`/`+0x0104`/`+0x0106`
4 registers for up to 8 sprite color calc ratios.

---

## 15. Color Offset Registers

### CLOFEN `+0x0110` R/W — Enable
```
 6: SPCOEN  sprite
 5: BKCOEN  back
 4: R0COEN  RBG0
 3: N3COEN  NBG3
 2: N2COEN  NBG2
 1: N1COEN  NBG1
 0: N0COEN  NBG0
```

### CLOFSL `+0x0112` R/W — Select A or B
Same bit layout as CLOFEN; 0=use offset-A, 1=use offset-B.

### Color Offset Values (9-bit signed, bit 8 = sign)
```
COAR  +0x0114   Offset A — Red
COAG  +0x0116   Offset A — Green
COAB  +0x0118   Offset A — Blue
COBR  +0x011A   Offset B — Red
COBG  +0x011C   Offset B — Green
COBB  +0x011E   Offset B — Blue
```

Value range: −255 (0x1FF) to +255 (0x0FF). Clamped on output.

---

## 16. Shadow / Special Registers

### LCTA — Line Color Screen Table Address `+0x0120`/`+0x0122` R/W

### BKTA — Back Screen Table Address `+0x0124`/`+0x0126` R/W
```
Bit 31 of 32-bit value: 0=single color, 1=per-line color table
Bits 25-0: VRAM address of table
```

### SFSEL — Special Function Code Select `+0x0128` R/W
### SFCODE — Special Function Code `+0x012A` R/W

### VCSTAU/VCSTAL — Vertical Cell Scroll Table `+0x009C`/`+0x009E`

### RACTL — RAM Address Control (extra) `+0x012C`

---

## Quick-Reference: Most-Used Register Addresses

| Register | Abs Address   | Purpose                           |
|----------|---------------|-----------------------------------|
| TVMD     | `0x05F80000`  | TV mode, display enable, resolution |
| EXTEN    | `0x05F80002`  | External signals                  |
| TVSTAT   | `0x05F80004`  | V-blank/H-blank status (RO)       |
| VRSIZE   | `0x05F80006`  | VRAM size select                  |
| VRAMCH   | `0x05F8000C`  | VRAM change                       |
| RAMCTL   | `0x05F8000E`  | VRAM partition + Color RAM mode   |
| CYCA0L   | `0x05F80010`  | VRAM-A0 cycle T0–T3               |
| BGON     | `0x05F80020`  | Screen enable + transparent       |
| MZCTL    | `0x05F80022`  | Mosaic                            |
| CHCTLA   | `0x05F80028`  | Char control NBG0/NBG1            |
| CHCTLB   | `0x05F8002A`  | Char control NBG2/NBG3/RBG0       |
| PNCN0    | `0x05F80030`  | Pattern name ctrl NBG0            |
| PLSZ     | `0x05F8003A`  | Plane size                        |
| MPOFN    | `0x05F8003C`  | Map offset normal screens         |
| N0MAP    | `0x05F80040`  | NBG0 plane A/B map                |
| SCRCTL   | `0x05F8009A`  | Line/VCell scroll control         |
| RPMD     | `0x05F800B0`  | Rotation scroll mode              |
| SPCTL    | `0x05F800E0`  | Sprite type + window              |
| SDCTL    | `0x05F800E2`  | Shadow enable                     |
| CCCTL    | `0x05F800E4`  | Color calc enable                 |
| PRINA    | `0x05F800F8`  | Priority NBG0/NBG1                |
| PRINB    | `0x05F800FA`  | Priority NBG2/NBG3                |
| PRIR     | `0x05F800FC`  | Priority RBG0                     |
| CCRTNA   | `0x05F80108`  | Color calc ratio NBG0/NBG1        |
| CLOFEN   | `0x05F80110`  | Color offset enable               |
| COAR     | `0x05F80114`  | Offset A Red                      |
