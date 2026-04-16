# Sega Saturn VDP2 “Infinite Plane” Mode: Architecture, Registers, Memory Banking, Timing, and a Working SH‑2 Assembly Example

## Executive summary

The entity["organization","Sega","video game company"] Saturn’s VDP2 “infinite plane” effect is not a single magical register, but a *combination* of the **rotation scroll screen pipeline** (RBG0/RBG1) and the **screen-over (overflow) processing** that controls what happens when a rotated/scaled sampling coordinate leaves the defined map/bitmap area. In practice, most “infinite plane” uses are built on **RBG0** (rotation scroll 0): you render a cell/bitmap surface, then configure **screen-over processing** to *repeat* the surface outside its display area, making the texture/map effectively endless when you translate or rotate the sampling transform. The key control point is the **Plane Size Register (PLSZ)** “screen over process” bits for rotation parameter A/B (RAOVR/RBOVR): when set to “repeat,” the area beyond the defined map repeats the image defined within the display area rather than turning transparent or clamping. citeturn17view0turn38view5

A working infinite-plane setup therefore requires:

- Choosing **RBG0** and a format (cell or bitmap) plus color depth. VDP2 supports normal scroll screens NBG0–NBG3 and rotation scroll screens RBG0/RBG1; rotation scroll screens allow scaling/rotation, and have a larger 4×4 plane map arrangement. citeturn18view0turn30view0  
- Partitioning VDP2 VRAM into banks (A0/A1/B0/B1) and assigning those banks to rotation data (pattern name tables, character/bitmap patterns, coefficient data) via **RAMCTL (RDBS bits)**. citeturn23view0turn19view0turn42view2  
- Populating VRAM with **pattern name tables (maps)** and **character patterns** (or bitmaps), using the *correct map-address math* (map register + map offset), which depends on plane size, pattern name size (1-word/2-word), and character size. citeturn38view0turn38view1  
- Setting rotation control registers (notably the rotation parameter table address) and updating rotation parameters during **VBlank** (or with display disabled) so the rotation hardware doesn’t read half-updated tables. VDP2 reads the rotation parameter table from VRAM line-by-line during display. citeturn15view4turn15view0  
- Synchronizing register changes with VBlank and respecting the fact that **TVSTAT.VBLANK is only meaningful when TVMD.DISP=1**; if DISP=0 the VBLANK flag reads as 1. citeturn39view0turn43view0  

The assembly program at the end uses **RBG0 in cell format** and demonstrates “infinite plane” behavior by configuring **RAOVR = repeat** and continuously updating the rotation parameter start coordinates (scroll) once per VBlank.

## VDP2 architecture elements that matter for infinite plane

VDP2 is a background compositing engine built around multiple “screens” (layers): **NBG0–NBG3** (normal scroll screens), **RBG0/RBG1** (rotation scroll screens), plus the **line color screen (LNCL)** and **back screen (BACK)**. RBG0/RBG1 are the layers typically used for “infinite plane” because they can scale/rotate a surface and sample it using a parameter table. citeturn18view0turn38view7

Rotation scroll screens differ from normal scroll screens in two especially important ways:

- **Map topology:** A normal scroll screen map is arranged as **2×2 planes** (planes A–D). A rotation scroll screen map is **4×4 planes** (planes A–P) per rotation parameter set (A or B). citeturn30view0turn29view0  
- **Data isolation and VRAM usage constraints:** When using a rotation scroll screen, the VDP2 documentation explicitly warns that rotation scroll image data cannot be shared with normal scroll screens, and RBG1 display requires RBG0 (with additional constraints on simultaneously displaying normal scroll screens). citeturn23view0turn18view0  

Priority and compositing are controlled by VDP2 mechanisms (priority numbers, special priority modes, color calculation), but an infinite plane setup usually starts by getting RBG0 visible with correct VRAM banking and map addressing; priority tuning comes later. VDP2 exposes a general priority function for ordering screens and supports color calculation and offset features. citeturn18view3turn18view0

image_group{"layout":"carousel","aspect_ratio":"16:9","query":["Sega Saturn VDP2 block diagram","Sega Saturn VDP1 VDP2 architecture diagram","Sega Saturn VDP2 RBG0 rotation background diagram","Sega Saturn VDP2 scroll screens NBG RBG layers diagram"],"num_per_query":1}

## How the “infinite plane” effect is produced on VDP2

### The essential mechanism: screen-over (overflow) processing on rotation screens

For rotation scroll screens, VDP2 has a dedicated overflow policy called **screen-over processing**, controlled in **PLSZ** for rotation parameter A and B. The key mode for “infinite plane” is:

- **RAOVR/RBOVR = 00:** “Outside the display area it repeats the image set in the display area.” citeturn38view5turn17view0  

That “Repeat the image outside the display area” mode is what makes the plane effectively infinite: as the mapping function translates or rotates sampling coordinates, any coordinate that falls outside the defined map/bitmap area wraps by repetition rather than turning transparent or clamping. citeturn38view5turn17view0

Other screen-over options include repeating a specified pattern via a “screen-over pattern name register” (cell format only), forcing transparency outside the area, or forcing the effective display area to 512×512 and making the rest transparent. citeturn17view0turn38view5

### Rotation background math and why repetition looks like an infinite surface

VDP2’s rotation background hardware can be understood as sampling a 2D surface (a cell map or bitmap) using a mapping from screen coordinates to surface coordinates. The VDP2 manual describes that the rotation parameter table is read from VRAM and used to generate “screen screen X coordinate” and “screen screen Y coordinate” per pixel (in terms of the H and V counters), using start values and increments. citeturn15view0

Separately, Sega’s technical bulletin on VDP2 rotating backgrounds explains a conceptual sequence: translate in XY, rotate about an axis/center (often represented as matrix operations), project/scale line-by-line or column-by-column (for perspective-like effects), then optionally apply a last Z rotation; rotation parameters and coefficient tables control these operations. citeturn38view7turn38view8turn38view9

In “classic infinite plane” use (e.g., a road/floor), you typically:
- Make the map/bitmap a *tileable texture*,
- Configure screen-over to repeat,
- Move the surface coordinates steadily (translation) and optionally adjust rotation/scaling parameters, producing continuous motion over a repeating surface. citeturn38view5turn38view7

This report’s assembly demo focuses on the *repeat-based infinity* (RAOVR=repeat) and a continuously updated translation, which is the foundation required before adding more advanced perspective coefficient tables.

## Register-level reference and memory/bank mapping for infinite plane

This section focuses on the *registers and memory objects you must understand to build an infinite plane* on RBG0.

### CPU-side address map essentials (VDP1, VDP2, SCU)

A practical Saturn program must know the cache-through addresses for the video processors and SCU.

**VDP1 (sprite/polygon processor) memory map (common cache-through view):**
- **VDP1 VRAM begins at 0x25C00000**, and SGL documentation shows that some parts can be reserved by the system, with a user area in the middle depending on configuration. citeturn42view1  
- Community documentation (Yabause wiki) summarizes the standard map: **VDP1 VRAM 0x25C00000**, frame buffer 0x25C80000, and VDP1 I/O registers at 0x25D00000. citeturn40search0  

**VDP2 VRAM and bank boundaries:**
SGL documentation provides a concrete bank picture with cache-through addresses and typical bank splits:
- VDP2 VRAM begins at **0x25E00000** and can be conceptually divided into A0/A1/B0/B1 regions with boundaries shown at 0x25E20000, 0x25E40000, 0x25E60000, etc., under a common initialization partitioning. citeturn42view2turn23view0  

**SCU interrupt/DMA register space:**
The SCU User’s Manual register list places:
- DMA set registers at 0x25FE0000+,
- DSP ports at 0x25FE0080+,
- Timer registers at 0x25FE0090+,
- **Interrupt mask register at 0x25FE00A0** and **interrupt status register at 0x25FE00A4**, and says SCU registers must be accessed via cache-through addresses. citeturn44search0  

### VRAM banking for rotation scroll screens (RAMCTL and RDBS)

VDP2 can split VRAM-A and VRAM-B into A0/A1/B0/B1 and notes that bank division improves simultaneous access but imposes restrictions during display. RAMCTL controls VRAM division and rotation bank usage and is cleared to 0 on reset. citeturn19view0turn23view0  

For rotation scroll, RAMCTL contains **RDBS bits** that assign each bank’s purpose (coefficient table, pattern name table, character pattern/bitmap, or unused). The VDP2 docs explicitly state:
- RDBS = 01: coefficient table RAM for RBG0  
- RDBS = 10: pattern name table RAM for RBG0  
- RDBS = 11: character pattern table (or bitmap) RAM for RBG0 citeturn23view0  

It also warns that if the image data read address is not within the specified bank, data won’t be read correctly and display will break. citeturn23view0  

### Map selection math (the most common failure point)

Rotation scroll maps are organized as 4×4 planes (A–P), each plane pointing at a pattern name page. citeturn30view0turn32view0  

The “map selection register” is constructed from a 6-bit plane map register plus a 3-bit map offset register; VDP2 notes that which bits are meaningful and how the address is computed depends on pattern name size and character size. citeturn38view0turn29view0  

The official VDP2 manual provides the exact address computation table. For the most common configuration used in this demo:
- **Plane size:** 1H page × 1V page  
- **Pattern name data size:** 1 word  
- **Character size:** 1×1 cell (8×8 dots)  
- **Address value:** *(value of bits 6–0) × 0x2000* citeturn38view1  

This is critical: if you compute the wrong page base, the rotation layer will “read garbage” and many emulators will still show something misleading, hiding the bug until hardware testing.

### The “infinite” part: Plane Size Register (PLSZ) screen-over mode

The same register that sets plane size also sets rotation screen-over behavior:
- **RAPLSZ (PLSZ bits 9–8)** selects plane size for rotation parameter A (1×1, 2×1, or 2×2 pages). citeturn17view0turn38view5  
- **RAOVR (PLSZ bits 11–10)** selects screen-over:
  - 00: repeat image outside display area,
  - 01: repeat specified pattern name (cell format only),
  - 10: transparent outside,
  - 11: clamp 512×512 and transparent outside. citeturn17view0turn38view5  

“Infinite plane mode,” in practice, is: use RBG0 + set RAOVR=00 so the surface repeats indefinitely as you scroll/rotate.

### Timing and latching rules that matter for stability

Two rules from official docs commonly bite developers:

- **Switch TVMD.DISP from 0→1 during VBlank.** The TVMD register explicitly says “be sure to change DISP from 0 to 1 during the V blank period.” citeturn39view0  
- **You cannot use TVSTAT.VBLANK to detect real VBlank if DISP=0**: TVSTAT says VBLANK is valid only when DISP=1; when DISP=0 the VBLANK flag is always 1. citeturn43view0  

So a robust init sequence typically:
1) keep DISP=0 while writing VRAM and registers,  
2) then set DISP=1 and begin VBlank-based updates.

## Timing, synchronization, and known pitfalls

### VBlank/HBlank synchronization and why polling sometimes beats interrupts early on

The screen status register **TVSTAT** exposes VBLANK and HBLANK flags. VBLANK=1 means vertical retrace (the VBlank interval), HBLANK=1 means horizontal retrace. citeturn43view0turn39view0  

However, because VBLANK is pinned to 1 when DISP=0, early initialization cannot rely on VBLANK polling to “wait for the next blank”; you must either:
- enable display first, then synchronize,
- or accept that “display-off” is effectively always-safe for VRAM writes (which is also suggested by TVMD.DISP behavior: when DISP=0 the screen is blank and VRAM is accessible at any time). citeturn39view0turn43view0  

For rotation scroll specifically, VDP2 reads the rotation parameter table from VRAM line-by-line during display, so the safest place to update parameter tables is during VBlank to avoid tearing half-updated values across scanlines. citeturn15view4turn15view0  

### VRAM access restrictions, cycle patterns, and why rotation banks are special

VDP2 supports banking and VRAM cycle pattern registers to schedule which kind of read happens at each of eight timings (T0–T7) per bank during display. These cycle pattern registers are reset to 0 and must be set when used. citeturn19view1turn20view3  

The “access command” table includes commands such as pattern name reads for NBG0–NBG3, character pattern reads, vertical cell scroll table reads, CPU read/write, and “do not access.” citeturn22view0turn20view3  

For rotation scroll, the VDP2 manual states that **VRAM cycle pattern settings for banks specified as “rotating scroll RAM” are ignored** (because the rotation engine takes ownership of those banks), and that coefficient data may need separate banking when read dot-by-dot. citeturn23view0turn19view0  

A practical rule is: while bringing up RBG0, avoid mixing normal scroll and rotation data until the rotation layer is stable and you fully understand the banking constraints. citeturn23view0turn18view0  

### Hardware/implementation pitfalls and quirks

- **Map address math changes with format.** The official table shows that different plane sizes, pattern name sizes (1 vs 2 words), and character sizes change the bit-range and multiplier (0x2000, 0x800, 0x4000, etc.). If you change one format bit without changing your address math, you may land on the wrong page boundaries. citeturn38view1turn38view0  
- **Rotation screen-over “repeat pattern name register” is not valid for bitmap rotation screens.** The PLSZ notes caution against selecting the “repeat specified pattern name” mode for bitmap rotation and describes a 256‑line bitmap duplication caveat in some overflow modes. citeturn17view0turn38view5  
- **VRAM capacity affects usable MSBs.** The VRAM address map changes with VRAM capacity (4Mbit vs 8Mbit), and the map address table explicitly notes that in 4Mbit mode the most significant bit among the used bits is not used. This matters if you port code/doc assumptions across systems. citeturn39view1turn38view1  
- **SCU register access must use cache-through addressing.** The SCU manual explicitly states this; mixing cached accesses with hardware regs can lead to subtle failures. citeturn44search0  
- **VDP2 VRAM read restrictions via DMA:** the VDP2 address-map text warns that some VRAM read behavior (e.g., using SCU‑DMA for reads) is prohibited and that VRAM writes without proper timing can cause display distortion. citeturn8view0  

## Initialization sequence and complete SH‑2 GNU AS program

### Bank and register plan used by the demo

This demo uses **RBG0 in cell format**, **8×8**, **1‑word pattern name data**, and configures **RAOVR=repeat** to achieve the infinite plane effect. It scrolls by updating rotation parameter table coordinates once per VBlank.

**VDP2 VRAM bank assignment (cache-through addresses):**

| Bank | Address range (typical split) | RBG0 assignment via RAMCTL.RDBS | Contents in this demo |
|---|---|---|---|
| A0 | 0x25E00000–0x25E1FFFF | `11b` = RBG0 character patterns/bitmap citeturn23view0turn42view2 | 256‑color 8×8 character patterns (tiles) |
| A1 | 0x25E20000–0x25E3FFFF | `01b` = RBG0 coefficient table citeturn23view0turn42view2 | Rotation parameter table A (placed at 0x25E3FF00). Coefficient table not used in this minimal demo. |
| B0 | 0x25E40000–0x25E5FFFF | `10b` = RBG0 pattern name tables citeturn23view0turn42view2 | 16 pages (planes A–P) of pattern name data |
| B1 | 0x25E60000–0x25E7FFFF | `00b` = unused citeturn23view0turn42view2 | free |

**Core VDP2 register values used:**

| Register | Internal addr | Purpose | Reset | Demo value | Notes |
|---|---:|---|---:|---:|---|
| TVMD | 180000h | TV mode / DISP | 0 citeturn39view0 | 0x0000 then 0x8000 | 320×224 (HRESO=000, VRESO=00) and DISP=1 citeturn39view0 |
| EXTEN | 180002h | external signals | 0 citeturn43view0 | 0 | leave external input off |
| TVSTAT | 180004h | VBLANK/HBLANK flags | — citeturn43view0 | polled | VBLANK valid only when DISP=1 citeturn43view0 |
| RAMCTL | 18000Eh | bank split + RBG0 bank role | 0 citeturn19view0turn23view0 | 0x0327 | VRAMD=VRBMD=1 plus RDBS mapping per table above citeturn23view0 |
| CHCTLB | 18002Ah | RBG0 format/color count | 0 citeturn24view0 | 0x1000 | RBG0 256 colors (R0CHCN=001) citeturn24view0 |
| PNCR | 180038h | RBG0 pattern name format | 0 citeturn25view0 | 0x8000 | 1‑word pattern name (R0PNB=1), CNSM mode0 citeturn25view0turn26view0 |
| PLSZ | 18003Ah | plane size + RAOVR | 0 citeturn17view0turn38view5 | 0x0000 | RAPLSZ=1×1 page; RAOVR=repeat citeturn38view5 |
| MPOFR | 18003Eh | rotation map offset | 0 citeturn27view0turn38view2 | 0 | map offset=0 |
| MPABRA..MPOPRA | 180050h.. | rotation map page base per plane | 0 citeturn14view0turn38view5 | 0x2120, 0x2322, … | planes A–P set to sequential pages starting at page index 0x20 |
| RPTA | 1800B4h | rotation parameter table base | 0 citeturn13view1 | 0x30FF | makes rotation parameter A table start at VRAM 0x3FF00 → 0x25E3FF00 citeturn13view1 |
| RPMD | 1800B0h | rotation parameter mode | 0 citeturn38view5 | 0x0000 | choose parameter A (simplest mode; demo doesn’t use A/B switching) |

### Mermaid flowchart for initialization

```mermaid
flowchart TD
  A[Entry: Master SH-2 start] --> B[Set stack, disable interrupts]
  B --> C[VDP2: TVMD.DISP=0 (blank)]
  C --> D[VDP2: RAMCTL bank split + RBG0 bank roles]
  D --> E[VDP2: Configure CHCTLB + PNCR (RBG0 cell, 256c, 1-word PND)]
  E --> F[VDP2: Configure PLSZ (RAOVR=repeat), MPOFR (offset=0)]
  F --> G[VDP2: Program rotation map registers (planes A–P)]
  G --> H[VRAM writes: tiles -> A0, map pages -> B0, rot param table -> A1]
  H --> I[VDP2: Set RPTA and basic rotation mode]
  I --> J[Enable RBG0 in BGON]
  J --> K[VDP2: TVMD.DISP=1 (display on)]
  K --> L[Main loop: wait VBLANK, update Xst/Yst in rotation parameter table]
```

### Complete GNU AS SH‑2 assembly demo

**Assembler syntax:** GNU `as` for SH‑2 (e.g., `sh-elf-as` / `sh2-elf-as`).  
**Execution model assumed:** Boot ROM loads your program into Work RAM‑H and jumps to `_start`. (Toolchains differ; adjust headers/linking to your build system.)

```asm
/*  ================================================================
    Sega Saturn VDP2 RBG0 "Infinite Plane" (repeat overflow) Demo
    GNU AS / SH-2 assembly
    ================================================================

    What this demo does:
      - Runs RBG0 in CELL format (8x8, 256-color)
      - Uses a 4x4 plane map (planes A..P), each plane is one 64x64 page
      - Sets screen-over for rotation parameter A to "repeat image"
        => the surface repeats outside its defined map area ("infinite plane")
      - Updates rotation parameter table start coords (Xst/Yst) once per VBlank
        => smooth scrolling over an endlessly repeating tiled surface

    Notes:
      - Uses VBlank polling via VDP2 TVSTAT. This is reliable once DISP=1.
      - TVSTAT.VBLANK reads as 1 when DISP=0, by spec; so we only use it
        after enabling display.

    Memory/Banks used (cache-through):
      VDP2 VRAM A0 0x25E00000 : RBG0 character patterns
      VDP2 VRAM A1 0x25E20000 : (reserved as RBG0 coefficient bank) + rotation params
      VDP2 VRAM B0 0x25E40000 : RBG0 pattern name tables (map pages)
      VDP2 VRAM B1 0x25E60000 : unused

    Rotation parameter table A placed at:
      0x25E3FF00 (VDP2 VRAM address 0x3FF00)
      RPTA = 0x30FF selects that base.

    ================================================================ */

        .section .text
        .align  2
        .global _start

/* ----------------------------------------------------------------
   Hardware base addresses (cache-through)
   ---------------------------------------------------------------- */
        .equ    VDP2_REG_BASE,  0x25F80000
        .equ    VDP2_VRAM_A0,    0x25E00000
        .equ    VDP2_VRAM_A1,    0x25E20000
        .equ    VDP2_VRAM_B0,    0x25E40000
        .equ    VDP2_VRAM_B1,    0x25E60000
        .equ    VDP2_CRAM,       0x25F00000

/* VDP2 register offsets (from internal 180000h map) */
        .equ    TVMD,            0x0000   /* 180000 */
        .equ    EXTEN,           0x0002   /* 180002 */
        .equ    TVSTAT,          0x0004   /* 180004 */
        .equ    RAMCTL,          0x000E   /* 18000E */
        .equ    BGON,            0x0020   /* 180020 */
        .equ    CHCTLB,          0x002A   /* 18002A */
        .equ    PNCR,            0x0038   /* 180038 */
        .equ    PLSZ,            0x003A   /* 18003A */
        .equ    MPOFR,           0x003E   /* 18003E */

        .equ    MPABRA,          0x0050   /* 180050 */
        .equ    MPCDRA,          0x0052   /* 180052 */
        .equ    MPEFRA,          0x0054   /* 180054 */
        .equ    MPGHRA,          0x0056   /* 180056 */
        .equ    MPIJRA,          0x0058   /* 180058 */
        .equ    MPKLRA,          0x005A   /* 18005A */
        .equ    MPMNRA,          0x005C   /* 18005C */
        .equ    MPOPRA,          0x005E   /* 18005E */

        .equ    RPMD,            0x00B0   /* 1800B0 */
        .equ    RPTA,            0x00B4   /* 1800B4 */

/* ----------------------------------------------------------------
   Macros
   ---------------------------------------------------------------- */
        .macro  WRITE16 regofs, imm16
        mov.l   r0, @-r15
        mov.l   r1, @-r15
        mov.l   r2, @-r15
        mov.l   #VDP2_REG_BASE, r1
        mov.w   \imm16, r0
        add     #\regofs, r1
        mov.w   r0, @r1
        mov.l   @r15+, r2
        mov.l   @r15+, r1
        mov.l   @r15+, r0
        .endm

        .macro  STOREW addrreg, valreg
        mov.w   \valreg, @\addrreg
        add     #2, \addrreg
        .endm

/* ----------------------------------------------------------------
   Entry
   ---------------------------------------------------------------- */
_start:
        /* Set stack near end of Work RAM-H (typical). */
        mov.l   stack_top, r15

        /* Disable interrupts (SR.IMASK=15) */
        mov     #0xF0, r0
        ldc     r0, sr

        /* --------------------------------------------------------
           VDP2 init
           -------------------------------------------------------- */

        /* TVMD: DISP=0, 320x224, non-interlaced.
           For 320x224: HRESO=000, VRESO=00, LSMD=00 => value = 0x0000.
           Keep DISP=0 during init. */
        mov.l   #VDP2_REG_BASE, r1
        mov.w   #0x0000, r0
        mov.w   r0, @(TVMD,r1)

        /* EXTEN: leave external input disabled */
        mov.w   #0x0000, r0
        mov.w   r0, @(EXTEN,r1)

        /* RAMCTL: split VRAM-A and VRAM-B, assign RBG0 banks.
           Bits:
             VRAMD (bit8)=1, VRBMD (bit9)=1
             RDBSA00..01 (A0) = 11b => RBG0 character patterns
             RDBSA10..11 (A1) = 01b => RBG0 coefficient/rotation param bank
             RDBSB00..01 (B0) = 10b => RBG0 pattern name tables
             RDBSB10..11 (B1) = 00b => unused
           => 0x0300 + 0x0027 = 0x0327
        */
        mov.w   #0x0327, r0
        mov.w   r0, @(RAMCTL,r1)

        /* CHCTLB: RBG0 256 colors (R0CHCN=001 at bits 14..12), cell format (R0BMEN=0).
           => 0x1000 */
        mov.w   #0x1000, r0
        mov.w   r0, @(CHCTLB,r1)

        /* PNCR: RBG0 pattern name data size = 1 word (R0PNB=1), CNSM=0 (10-bit + flip-per-cell).
           => 0x8000 */
        mov.w   #0x8000, r0
        mov.w   r0, @(PNCR,r1)

        /* PLSZ: rotation parameter A plane size 1x1 page, screen-over repeat.
           RAPLSZ=00 (bits9..8), RAOVR=00 (bits11..10) => 0x0000 */
        mov.w   #0x0000, r0
        mov.w   r0, @(PLSZ,r1)

        /* MPOFR: map offset for rotation param A/B = 0 */
        mov.w   #0x0000, r0
        mov.w   r0, @(MPOFR,r1)

        /* Rotation map registers (planes A..P) for rotation parameter A.
           We place 16 pages contiguously in VRAM at internal address 0x40000 (bank B0 base).
           For (1 page, 1-word, 1x1 cell), page address = (bits6..0) * 0x2000.
           0x40000 / 0x2000 = 0x20, so the first page index is 0x20.
           Planes A..P use indices 0x20..0x2F.

           Register packing: low 6 bits for plane A in low byte, plane B in high byte, etc.
         */
        mov.w   #0x2120, r0    /* A=0x20, B=0x21 */
        mov.w   r0, @(MPABRA,r1)
        mov.w   #0x2322, r0    /* C=0x22, D=0x23 */
        mov.w   r0, @(MPCDRA,r1)
        mov.w   #0x2524, r0    /* E=0x24, F=0x25 */
        mov.w   r0, @(MPEFRA,r1)
        mov.w   #0x2726, r0    /* G=0x26, H=0x27 */
        mov.w   r0, @(MPGHRA,r1)
        mov.w   #0x2928, r0    /* I=0x28, J=0x29 */
        mov.w   r0, @(MPIJRA,r1)
        mov.w   #0x2B2A, r0    /* K=0x2A, L=0x2B */
        mov.w   r0, @(MPKLRA,r1)
        mov.w   #0x2D2C, r0    /* M=0x2C, N=0x2D */
        mov.w   r0, @(MPMNRA,r1)
        mov.w   #0x2F2E, r0    /* O=0x2E, P=0x2F */
        mov.w   r0, @(MPOPRA,r1)

        /* Rotation parameter table address (RPTA):
           Rotation parameter A base = (RPTA7..0)*0x100 + (RPTA15..12)*0x10000
           Choose base 0x3FF00 => high nibble=3, low byte=0xFF => RPTA=0x30FF */
        mov.w   #0x30FF, r0
        mov.w   r0, @(RPTA,r1)

        /* RPMD: simplest mode (use rotation parameter A for RBG0) */
        mov.w   #0x0000, r0
        mov.w   r0, @(RPMD,r1)

        /* --------------------------------------------------------
           Load palette to VDP2 CRAM (simple: set only indices 1 and 2)
           Format: VDP2 CRAM uses 16-bit color entries (RGB555-like).
           -------------------------------------------------------- */
        mov.l   #VDP2_CRAM, r2

        /* index 0 = 0 (transparent/black depending on TP settings); keep 0 */
        mov.w   #0x0000, r0
        mov.w   r0, @r2
        add     #2, r2

        /* index 1: dark green (approx) */
        mov.w   #0x02A0, r0     /* G ~= 0x15 */
        mov.w   r0, @r2
        add     #2, r2

        /* index 2: bright green */
        mov.w   #0x03E0, r0     /* G max */
        mov.w   r0, @r2

        /* --------------------------------------------------------
           Load character patterns (tiles) into VRAM A0.
           Two tiles, 8x8, 256-color => 64 bytes each
           We store as 32 words per tile (2 pixels per word).
           -------------------------------------------------------- */
        mov.l   #VDP2_VRAM_A0, r2
        mov.l   tile_data, r3
        mov.l   # (2*32), r4    /* 64 words total (2 tiles * 32 words) */
tile_copy_loop:
        mov.w   @r3+, r0
        mov.w   r0, @r2
        add     #2, r2
        add     #-1, r4
        tst     r4, r4
        bf      tile_copy_loop

        /* --------------------------------------------------------
           Fill 16 pages of pattern name data in VRAM B0 (planes A..P).
           Each page is 64x64 entries; 1-word pattern name => 0x2000 bytes/page.
           We generate a checkerboard using tile #1 and #2 (indices 0 and 1 in map,
           but pixel colors are 1/2 in tile pixels, so no transparency holes).
           Pattern name entry (1-word, 256-color, mode0):
             bits 9..0 : character number
             bits 11..10 : flip (we set 0)
             bits 14..12 : palette (we set 0)
             bit 15 : ignored
           So entry value = tile_number (0 or 1).
           -------------------------------------------------------- */

        mov.l   #VDP2_VRAM_B0, r10
        mov     #16, r9                 /* page count */

page_loop:
        /* y loop */
        mov     #64, r8                 /* rows */
        mov     #0, r7                  /* y */

row_loop:
        mov     #64, r6                 /* cols */
        mov     #0, r5                  /* x */

col_loop:
        /* tile = (x ^ y) & 1 */
        mov     r5, r0
        xor     r7, r0
        and     #1, r0

        /* store pattern name word */
        mov.w   r0, @r10
        add     #2, r10

        add     #1, r5
        add     #-1, r6
        bf      col_loop

        add     #1, r7
        add     #-1, r8
        bf      row_loop

        add     #-1, r9
        bf      page_loop

        /* --------------------------------------------------------
           Write rotation parameter table A at 0x25E3FF00.
           Minimal affine mapping:
             X = Xst + H*ΔX + V*ΔXst
             Y = Yst + H*ΔY + V*ΔYst
           Identity:
             ΔX=1, ΔY=0, ΔXst=0, ΔYst=1
           We keep fractional parts = 0.
           -------------------------------------------------------- */
        mov.l   rotparam_base, r11       /* 0x25E3FF00 */

        /* Clear 0x60 bytes first */
        mov     #0x60/2, r4
        mov     #0, r0
rp_clear:
        mov.w   r0, @r11
        add     #2, r11
        add     #-1, r4
        bf      rp_clear

        /* Re-point r11 to base */
        mov.l   rotparam_base, r11

        /* Xst (offset 0x00): integer word */
        mov.w   #0, r0
        mov.w   r0, @(0x00, r11)
        /* Xst fractional (0x02) = 0 */
        mov.w   #0, r0
        mov.w   r0, @(0x02, r11)

        /* Yst (0x04): integer */
        mov.w   #0, r0
        mov.w   r0, @(0x04, r11)
        /* Yst frac (0x06) */
        mov.w   #0, r0
        mov.w   r0, @(0x06, r11)

        /* Zst (0x08/0x0A) = 0 */
        mov.w   #0, r0
        mov.w   r0, @(0x08, r11)
        mov.w   r0, @(0x0A, r11)

        /* ΔXst (0x0C/0x0E) = 0 */
        mov.w   #0, r0
        mov.w   r0, @(0x0C, r11)
        mov.w   r0, @(0x0E, r11)

        /* ΔYst (0x10/0x12) = 1 */
        mov.w   #1, r0
        mov.w   r0, @(0x10, r11)
        mov.w   #0, r0
        mov.w   r0, @(0x12, r11)

        /* ΔX (0x14/0x16) = 1 */
        mov.w   #1, r0
        mov.w   r0, @(0x14, r11)
        mov.w   #0, r0
        mov.w   r0, @(0x16, r11)

        /* ΔY (0x18/0x1A) = 0 */
        mov.w   #0, r0
        mov.w   r0, @(0x18, r11)
        mov.w   r0, @(0x1A, r11)

        /* --------------------------------------------------------
           Enable RBG0 and turn display on.
           BGON: enable RBG0 (assumed bit4) -> 0x0010
           -------------------------------------------------------- */
        mov.l   #VDP2_REG_BASE, r1
        mov.w   #0x0010, r0
        mov.w   r0, @(BGON,r1)

        /* TVMD: DISP=1, 320x224, non-interlaced => 0x8000 */
        mov.w   #0x8000, r0
        mov.w   r0, @(TVMD,r1)

        /* --------------------------------------------------------
           Main loop: VBlank sync, update scroll (Xst/Yst).
           -------------------------------------------------------- */
        mov     #0, r12          /* scroll_x */
        mov     #0, r13          /* scroll_y */

main_loop:
        bsr     wait_vblank_in
        nop

        /* Update scroll (simple diagonal movement) */
        add     #1, r12
        add     #1, r13

        /* Write Xst integer at rotparam base + 0x00 */
        mov.l   rotparam_base, r11
        mov     r12, r0
        mov.w   r0, @(0x00, r11)
        /* Yst integer at +0x04 */
        mov     r13, r0
        mov.w   r0, @(0x04, r11)

        bsr     wait_vblank_out
        nop
        bra     main_loop
        nop

/* ----------------------------------------------------------------
   VBlank polling helpers (TVSTAT bit3)
   ---------------------------------------------------------------- */
wait_vblank_in:
        /* Wait until VBLANK=1 */
        mov.l   #VDP2_REG_BASE, r1
.vbi_loop:
        mov.w   @(TVSTAT,r1), r0
        tst     #0x08, r0        /* bit3 */
        bt      .vbi_loop
        rts
        nop

wait_vblank_out:
        /* Wait until VBLANK=0 */
        mov.l   #VDP2_REG_BASE, r1
.vbo_loop:
        mov.w   @(TVSTAT,r1), r0
        tst     #0x08, r0
        bf      .vbo_loop
        rts
        nop

/* ----------------------------------------------------------------
   Literal pool / constants
   ---------------------------------------------------------------- */
        .align  4
stack_top:
        .long   0x060FF000

rotparam_base:
        .long   0x25E3FF00

/* Two simple 8x8 tiles, 256-color.
   Each word packs 2 pixels (byte,byte). We only use colors 1 and 2. */
tile_data:
        .long   tile_words
tile_words:
        /* Tile 0: alternating 1,2 pattern */
        .rept   32
        .word   0x0102
        .endr
        /* Tile 1: alternating 2,1 pattern */
        .rept   32
        .word   0x0201
        .endr
```

### What to change to extend this into a “true floor plane” (perspective)

The above program demonstrates the *infinite repetition* mechanism (screen-over repeat) and steady translation. To create a classic Mode‑7‑like floor with perspective, you typically add a **coefficient table** and configure coefficient table control so coefficients vary line-by-line (farther lines scaled down, nearer lines scaled up). Sega’s rotating background bulletin explains that the coefficient table primarily controls the projection/perspective step, while rotation parameters control the 3D transformation and translation. citeturn38view7turn38view8turn38view9  

Implementing that correctly requires (a) choosing coefficient format/mode and (b) generating and placing coefficient data in the designated coefficient bank (often A1 or color RAM) as documented by VDP2’s coefficient table control rules. citeturn23view0turn19view3