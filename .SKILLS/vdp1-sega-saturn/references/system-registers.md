# VDP1 — System Registers (Complete Reference)

All registers are in the absolute address space **5D00000H–5D0001FH**.
Access is **word (16-bit) only**. Never use DMA burst transfer.
Unused bits must be written as 0.

---

## TVMR — TV Mode Selection Register
**Address:** 5D00000H | **Access:** Write-only

```
bit: 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
       0  0  0  0  0  0  0  0  0  0  0  0 VBE TVM[2:0]
```

### VBE — V-Blank Erase/Write Enable (bit 3)
- `0` = erase/write during display period (normal)
- `1` = erase/write during V-blank
  - Can only be set when FCM=1 and FCT=1 (FBCR)
  - Must be set immediately after V-blank IN interrupt
  - Forbidden: from the first H-blank IN after V-blank IN until the next H-blank IN

### TVM — TV Mode Select (bits 2:0)

| TVM | Name             | Display Resolution   | FB Size    | bpp | Interlace      |
|-----|-----------------|---------------------|------------|-----|----------------|
| 000 | Normal NTSC/PAL | 320×224, 320×240, 352×224, 352×240 | 512H×256V | 16 | Yes (incl. double) |
| 001 | High Resolution | 640×224 … 704×240   | 1024H×256V | 8  | Yes            |
| 010 | Rotation 16     | 320×224 … 352×240   | 512H×256V  | 16 | Single only    |
| 011 | Rotation 8      | 320×224 … 352×240   | 512H×512V  | 8  | Single only    |
| 100 | HDTV/31KC       | 320×240, 352×240    | 512H×256V  | 16 | No             |

- Bit 2: `0`=NTSC/PAL, `1`=HDTV/31KC
- Bit 1: `0`=no rotation, `1`=rotation
- Bit 0: `0`=16bpp, `1`=8bpp

**Timing:** TVM must be written between the 2nd H-blank IN after V-blank IN and the H-blank IN after V-blank OUT.

---

## FBCR — Frame Buffer Change Mode Register
**Address:** 5D00002H | **Access:** Write-only

```
bit: 15 14 13 12 11 10  9  8  7  6  5  4   3   2   1   0
       0  0  0  0  0  0  0  0  0  0  0 EOS DIE DIL FCM FCT
```

### FCM/FCT — Frame Buffer Change Mode/Trigger (bits 1, 0)

| VBE | FCM | FCT | Mode              | When it switches               |
|-----|-----|-----|-------------------|-------------------------------|
|  0  |  0  |  0  | 1-cycle (normal)  | Automatic, 60fps             |
|  0  |  0  |  1  | FORBIDDEN         | —                             |
|  0  |  1  |  0  | Manual (erase)    | Erases on next field          |
|  0  |  1  |  1  | Manual (change)   | Switches on next field        |
|  1  |  1  |  1  | Manual (erase+change) | Erases at V-blank + switches |

**Timing:** FCM/FCT must be written immediately after V-blank OUT interrupt.

### DIE — Double Interlace Enable (bit 3) / DIL — Draw Line (bit 2)

| DIE | DIL | Mode                     |
|-----|-----|--------------------------|
|  0  |  0  | Non/Single interlace     |
|  1  |  0  | Double interlace (even lines) |
|  1  |  1  | Double interlace (odd lines)  |

In double interlace: FCM=FCT=0 (1-cycle mode).

### EOS — Even/Odd Coordinate Select (bit 4)
Used with High Speed Shrink (HSS=1 in CMDPMOD):
- `0` = sample pixels at even coordinates
- `1` = sample pixels at odd coordinates

---

## PTMR — Plot Trigger Register
**Address:** 5D00004H | **Access:** Write-only
**Reset value:** 00B (idle)

```
bit: 15..2 = 0 | 1:0 = PTM
```

| PTM | Behavior                                                       |
|-----|---------------------------------------------------------------|
| 00  | Idle — does not start drawing                                 |
| 01  | Starts immediately on write (manual trigger)                  |
| 10  | Auto-start every frame change (normal mode)                   |
| 11  | FORBIDDEN                                                     |

**Note:** When writing PTM=01B, it restarts from the first command table even if already drawing.
To change only the mode without redrawing: write 00B, then 01B on the next frame.

---

## EWDR — Erase/Write Data Register
**Address:** 5D00006H | **Access:** Write-only

```
16bpp: bits[15:0] = fill data (uniform)
8bpp:  bits[15:8] = fill data for odd X | bits[7:0] = fill data for even X
```

- The frame buffer is filled with this value before each frame
- For black background in 16bpp: 0x0000
- For black background in RGB (avoid transparency on VDP2): 0x8000

---

## EWLR — Erase/Write Upper-Left Coordinate
**Address:** 5D00008H | **Access:** Write-only

```
bit: 15  14:9   8:0
       0   X1     Y1
```

- X1: bits [14:9] = register value. Real X = register × 8 (16bpp) or × 16 (8bpp)
- Y1: bits [8:0] = real Y coordinate (1 per line)
- X1=0 (register=0) is **forbidden** — use register=1 for X1=8

---

## EWRR — Erase/Write Lower-Right Coordinate
**Address:** 5D0000AH | **Access:** Write-only

```
bit: 15:9   8:0
       X3     Y3
```

- X3: bits [15:9]. Real X = register × 8 − 1 (16bpp) or × 16 − 1 (8bpp)
- Y3: bits [8:0]

**Examples for Normal 16bpp 320×224:**
```
EWLR = (1 << 9) | 0    → X1=8, Y1=0    (or use 0 for both, but X=0 forbidden)
EWRR = (40 << 9) | 223 → X3=319, Y3=223
```

Must satisfy X1 < X3 and Y1 ≤ Y3.

---

## ENDR — Draw Forced Termination Register
**Address:** 5D0000CH | **Access:** Write-only

```asm
    mov.w   #0x0000, r0
    mov.l   #5D0000CH, r1
    mov.w   r0, @r1         ; force-terminate in ~30 clock cycles
```

- Terminates current drawing in ≈30 cycles after write
- Interrupted drawing **cannot be resumed** normally
- Used in the "pseudo draw continuation" technique (see COPR)

---

## EDSR — Transfer End Status Register
**Address:** 5D00010H | **Access:** Read-only

```
bit: 15:2 = 0 | 1 = CEF | 0 = BEF
```

### CEF — Current End Bit Fetch Status (bit 1)
- `0` = Draw End Command not yet fetched in current frame
- `1` = Draw End Command was fetched → drawing finished

### BEF — Before End Bit Fetch Status (bit 0)
- `0` = previous frame ended normally
- `1` = previous frame ended (end command reached)
- `0` after V-blank without end = **transfer-over** (data excess)

**Polling loop:**
```asm
    mov.l   #5D00010H, r1
.loop:
    mov.w   @r1, r0
    and     #2, r0
    tst     r0, r0
    bt      .loop           ; wait for CEF=1
```

---

## LOPR — Last Operation Command Address Register
**Address:** 5D00012H | **Access:** Read-only

Value = address of the last command table processed in the previous frame, divided by 8H.
Lower 2 bits always = 00B.

```asm
; Recover real address of last command table from previous frame:
    mov.w   @(LOPR), r0
    shll2   r0          ; ×4
    shll    r0          ; ×8 → real address / 8H × 8 = address
    mov.l   #VRAM_BASE, r1
    add     r1, r0      ; + absolute VRAM base
```

---

## COPR — Current Operation Command Address Register
**Address:** 5D00014H | **Access:** Read-only

Same as LOPR but for the current frame in progress. Updated continuously.
Used for "pseudo draw continuation":
1. Force-terminate with ENDR
2. Read COPR to know where it stopped
3. Write a jump at the top of VRAM pointing to that address
4. Trigger PTM=01B to continue

---

## MODR — Mode Status Register
**Address:** 5D00016H | **Access:** Read-only

```
bit: 15:12=VER | 11:9=--- | 8=PTM1 | 7=EOS | 6=DIE | 5=DIL | 4=FCM | 3=VBE | 2:0=TVM
```

Mirror of write-only registers. VER=0001B (version 1 of VDP1).
Mainly for debug — values may differ from internal signals.

---

## Timing Summary

| When to do it             | What                              |
|---------------------------|-----------------------------------|
| After reset               | TVMR, FBCR, PTMR, EWDR, EWLR, EWRR |
| After V-blank IN          | VBE (if using erase&change)       |
| After V-blank OUT         | FCM, FCT                          |
| Between 2nd H-blank after VBI and H-blank after VBO | TVM |
| Any time                  | Write command/char tables in VRAM |
| Before accessing VRAM      | Verify VDP1 is not drawing (poll EDSR) |
