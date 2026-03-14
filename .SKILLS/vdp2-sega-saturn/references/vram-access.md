# VDP2 VRAM Access Patterns

## How the Cycle Pattern Works

VDP2 accesses VRAM during each horizontal line in repeating 8-slot cycles (Normal mode) or 4-slot cycles (Hi-Res/Exclusive mode). Each bank (A0, A1, B0, B1) has its own independent pattern.

When VRAM is **not partitioned** (VRAMD=0, VRBMD=0):
- VRAM-A behaves as one unit; use CYCA0L/U registers only.
- VRAM-B behaves as one unit; use CYCB0L/U registers only.
- A1 and B1 cycle registers are ignored.

When VRAM is **partitioned** (VRAMD=1 or VRBMD=1):
- Each bank (A0, A1, B0, B1) independently configured.
- CPU access should **not** be used during display when partitioned.

---

## Bandwidth Budget

Normal mode (320×224): 8 slots × 2 banks = **16 accesses per line**  
Hi-Res mode (640×xxx): 4 slots × 2 banks = **8 accesses per line**

Each scroll screen needs per line:
- **1 PNT slot** per plane column (most configurations = 1 PNT per bank)
- **1–2 CPD slots** depending on color depth and cell size

---

## Example Configurations

### Single layer NBG0 (16-color, cell, 320px, no bank split)

```
VRAM-A (used for PNT + CPD):
  T0=0x0 (NBG0 PNT)   T1=0x4 (NBG0 CPD)
  T2=0xE (CPU r/w)     T3=0xF (idle)
  T4=0x4 (NBG0 CPD)   T5=0xF (idle)
  T6=0xE (CPU r/w)     T7=0xF (idle)

VRAM-B: all 0xE/0xF (CPU + idle)

CYCA0L = 0x04EF
CYCA0U = 0x4FEF   (or 0x4FFF if no CPU access needed)
CYCB0L = 0xEFEF
CYCB0U = 0xFFFF
```

### Two layers NBG0 + NBG1 (16-color each, Normal mode, no bank split)

Separate PNT and CPD across the two banks:
```
VRAM-A (NBG0 PNT + CPD):
  T0=NBG0-PNT T1=NBG0-CPD T2=NBG0-CPD T3=CPU
  T4=NBG0-CPD T5=NBG0-CPD T6=CPU       T7=idle

VRAM-B (NBG1 PNT + CPD):
  T0=NBG1-PNT T1=NBG1-CPD T2=NBG1-CPD T3=CPU
  T4=NBG1-CPD T5=NBG1-CPD T6=CPU       T7=idle

CYCA0L = 0x0455   CYCA0U = 0x44EF
CYCB0L = 0x1566   CYCB0U = 0x55EF
```

### Two layers + NBG0 Line Scroll (adds VCS read slot requirement)

Line scroll reads from the line scroll table each line:
```
VRAM-A (NBG0 PNT + CPD + VCST):
  T0=NBG0-PNT T1=NBG0-CPD T2=NBG0-VCS(0xC) T3=NBG0-CPD
  T4=NBG0-CPD T5=CPU       T6=idle           T7=idle

CYCA0L = 0x044C  CYCA0U = 0x44EF
```

### Four layers NBG0–NBG3 (16-color each, partitioned banks)

With VRAMD=1 and VRBMD=1 (4 banks active):
```
VRAM-A0: NBG0 PNT + NBG0 CPD
VRAM-A1: NBG1 PNT + NBG1 CPD
VRAM-B0: NBG2 PNT + NBG2 CPD
VRAM-B1: NBG3 PNT + NBG3 CPD

CYCA0L = 0x04FF  (T0=N0PNT T1=N0CPD T2-T3=idle)
CYCA1L = 0x15FF  (T0=N1PNT T1=N1CPD T2-T3=idle)
CYCB0L = 0x26FF  (T0=N2PNT T1=N2CPD T2-T3=idle)
CYCB1L = 0x37FF  (T0=N3PNT T1=N3CPD T2-T3=idle)
```

Note: **No CPU r/w during display when using all 4 banks**. Write VRAM only during V-blank or H-blank.

---

## RBG0 VRAM Setup

RBG0 rotation screens use a dedicated bank approach. The rotation parameter table and character pattern data are assigned to specific VRAM banks via RAMCTL (RDBSA/RDBSB bits), not via cycle pattern registers.

```
RAMCTL[3:0] = RDBSA00..RDBSA11 → which VRAM-A bank holds RBG0 surface A data
RAMCTL[7:4] = RDBSB00..RDBSB11 → which VRAM-B bank holds RBG0 surface B data
```

Rotation parameter table address is set in RPTA registers (+0x00BC/+0x00BE).

---

## CPU Access During Display

- **Non-partitioned, Normal mode**: CPU can access during T2/T6 slots (or wherever `0xE` is assigned).
- **Partitioned mode**: Do not use CPU slots during display — causes visual artifacts.
- **Best practice**: Buffer all VRAM writes, flush during V-blank.

### V-blank safe write pattern (SH-2):
```asm
    ; Poll for V-blank start
_vb_start:
    mov.w   @vdp2_tvstat, r0
    tst     #0x08, r0
    bt      _vb_start           ; wait until bit3=1

    ; Write VRAM data here (safe window)
    ; ... mov.w / mov.l writes to 0x05E00000 range ...

    ; Wait until V-blank ends (display starts)
_vb_end:
    mov.w   @vdp2_tvstat, r0
    tst     #0x08, r0
    bf      _vb_end             ; wait until bit3=0
```

---

## Data Layout in VRAM

### Pattern Name Table

Size per plane = `plane_cells_H × plane_cells_V × entry_size`

| Plane Size | Cells           | 1-word entries | 2-word entries |
|------------|-----------------|----------------|----------------|
| 1H × 1V   | 32×32 = 1024    | 2 KB           | 4 KB           |
| 2H × 1V   | 64×32 = 2048    | 4 KB           | 8 KB           |
| 2H × 2V   | 64×64 = 4096    | 8 KB           | 16 KB          |

A **map** consists of 4 planes (A, B, C, D) for normal screens, or 16 planes (A–P) for rotation screens.

### Character Pattern Data

Location in VRAM is determined by the character number in the PNT entry.

`CPD_address = (char_number × cell_bytes) + CPD_base`

Where `CPD_base` is implicitly defined by where you place the CPD in VRAM (must be boundary-aligned to total CPD size).

### Alignment Requirements

| Data                  | Boundary                                   |
|-----------------------|--------------------------------------------|
| Pattern Name Table    | (plane_size_bytes) boundary                |
| Character Pattern Data| (total_CPD_size) boundary — power of 2     |
| Line Scroll Table     | 4-byte (longword) boundary                 |
| Rotation Parameter    | 64-byte boundary                           |
| Line Window Table     | 4-byte boundary                            |
| Back Screen Table     | 4-byte boundary                            |
