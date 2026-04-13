# SCU DSP — Complete Reference

## Overview

- Harvard architecture, VLIW pipeline
- Clock: 14.31818 MHz (half of SH2)
- 256 words × 32-bit Program RAM at 0x05FF8000
- 3 × 64 words × 32-bit Data RAM (CT0, CT1, CT2 + CT3) at 0x05FF8400
- 48-bit internal accumulator (AC)
- Up to **6 parallel operations** per cycle
- No hardware division — use reciprocal LUT
- Assembly-only: no C compiler supports it

## Data RAMs

```
Base Address:
  CT0: 0x05FF8400  (64 × 32-bit, ring buffer, pointer auto-increment)
  CT1: 0x05FF8500
  CT2: 0x05FF8600
  CT3: 0x05FF8700

Access from SH2:
  Direct read/write (slow, avoid in critical loop)
  Or via SCU DMA Ch2 (preferred)
```

Ring buffer: each read/write advances the pointer +1 automatically. Wrap at 64 entries.

## DSP Registers

| Reg   | Description                                     |
|-------|------------------------------------------------|
| AC    | 48-bit Accumulator (main result)                |
| CT0–CT3 | Ring buffer pointers (6-bit, auto-inc)       |
| P     | Partial MUL result (before MAC)                 |
| RA0   | DMA read address                               |
| WA0   | DMA write address                              |
| PC    | Program Counter                                |
| LOP   | Loop counter                                   |
| TOP   | Loop top (address for BTM)                     |

## VLIW Instruction — Parallel Fields

One DSP instruction word (32-bit) encodes up to 6 fields:
```
[D1  ][D2  ][X  ][Y  ][ALU ][CTRL]
 4-bit 4-bit 3-bit 3-bit 4-bit 12-bit
```
- **D1/D2:** Memory operations (read/write to Data RAM)
- **X/Y:** Operand selection for multiplier unit
- **ALU:** Operation on accumulator (MAC, ADD, SUB, etc.)
- **CTRL:** Branch, end of program, DMA

## Instruction Set

### ALU
```asm
CLR A           ; AC = 0
MOV [src], A    ; AC = src
ADD [s], A      ; AC += s
SUB [s], A      ; AC -= s
AD2 [s], A      ; AC = AC/2 + s  (weighted average)
AND [s], A      ; AC &= s
OR  [s], A      ; AC |= s
XOR [s], A      ; AC ^= s
LSR A           ; AC >>= 1 (logical)
ASR A           ; AC >>= 1 (arithmetic)
ABS A           ; AC = |AC|
```

### Multiply-Accumulate (most important)
```asm
MUL [s1], [s2]      ; P = s1 × s2  (doesn't change AC)
MAC [s1], [s2]      ; AC += s1 × s2
                     ; Operands can be CT0, CT1, CT2, CT3 or immediate
                     ; Example:
MAC CT0, CT1        ; AC += DataRAM[CT0] × DataRAM[CT1]
                     ; CT0++ and CT1++ automatically
```

**For fixed-point 16.16:**
- Multiplying two 16.16 values generates 32.32 result in 48-bit AC
- The 16 integer bits of the result are in AC[31:16]
- To extract 16.16 result: store AC and shift

### MOV / Load / Store
```asm
MOV [s], [d]        ; d = s
MOV A, [d]          ; Data RAM[addr] = AC (stores upper 32 bits of 48-bit AC)
MOV [s], CT0        ; CT0 = s (set pointer)
MVI #imm25, CT0     ; CT0 = immediate (25-bit sign-extended)
MVI #imm25, A       ; AC = immediate
```

### Control
```asm
NOP                 ; No operation (fill empty slots)
END                 ; End of program, triggers interrupt
ENDI                ; End of program, does NOT trigger interrupt
JMP [addr]          ; Unconditional jump
BTST [bit], [src]   ; Test bit
BTT  [addr]         ; Branch if true (after BTST)
BTF  [addr]         ; Branch if false

; Loop (hardware construction)
LPS                 ; Mark loop start (saved to TOP)
LE                  ; Loop end — decrements LOP, jumps to TOP if LOP > 0
```

## Parallelism Rules (CRITICAL)

1. **MAC** uses X and Y buses → cannot parallelize with another instruction using X or Y
2. **MOV to/from AC** uses ALU bus → cannot combine with MAC in the same cycle
3. **D1/D2 can operate simultaneously** — each has an independent bus
4. **CTRL** (branch/end) occupies the control slot; can combine with ALU/MAC

**Maximum parallelism example:**
```asm
; Single cycle executing: MAC + 1 D1 read + 1 D2 write
; Hitachi DSP syntax: multiple ops separated by '|'
MAC CT0, CT1 | MOV [addr], D2   ; MAC parallel with store
```

## Complete Example: Transform 4 Vertices by MVP Matrix

```asm
; SCU DSP: transform_batch
; Input (CT0 Data RAM):
;   [0..15]  = MVP Matrix (4×4 × int32, 16.16 fixed-point), loaded in CT0
;   [16..19] = Vertex 0 (x,y,z,w in int32)
;   [20..23] = Vertex 1
;   [24..27] = Vertex 2
;   [28..31] = Vertex 3
; Output (CT2 Data RAM):
;   [0..3]  = Transformed Vertex 0 (x,y,z,w)
;   [4..7]  = Transformed Vertex 1
;   etc.
;
; Note: result of MAC(16.16, 16.16) = 32.32 in 48-bit AC
;       We need bits [47:16] → these are bits [31:0] of AC >> 16

    ; Configure pointers
    MVI #0,  CT0    ; CT0 points to start: matrix
    MVI #16, CT1    ; CT1 points to vertex 0
    MVI #0,  CT2    ; CT2 = output destination
    
    ; Loop over 4 vertices
    MVI #3, LOP     ; LOP = 3 (4 iterations: 0,1,2,3)
    LPS             ; Mark loop start

    ; ── Calculate transformed X ──────────────────────────────
    CLR A
    MVI #0, CT0     ; Reset CT0 to row 0 of matrix
    MAC CT0, CT1    ; AC += M[0] × V[x]
    MAC CT0, CT1    ; AC += M[1] × V[y]
    MAC CT0, CT1    ; AC += M[2] × V[z]
    MAC CT0, CT1    ; AC += M[3] × V[w]
    MOV A, CT2      ; CT2[out++] = AC (upper 32 bits = correct 16.16 result)

    ; ── Calculate transformed Y ──────────────────────────────
    CLR A
    MVI #4, CT0     ; CT0 points to row 1 of matrix
    MAC CT0, CT1    ; AC += M[4] × V[x]  (CT1 was advanced, needs reset)
    ; ... (CT1 needs to be reset to the current vertex before each row)
    ; In practice: load CT1 with the current vertex address
    MOV A, CT2

    ; ── Calculate Z and W the same way ───────────────────────
    ; ... (repeat for rows 2 and 3 of matrix)

    LE              ; Loop end — decrements LOP, returns to LPS if LOP > 0
    ENDI            ; End without interrupt
```

> **Practical note:** The biggest difficulty with SCU DSP is that CT1 (pointer to vertex) advances automatically with each MAC. To transform Y (row 2 of matrix), CT1 will already be at the *next* vertex. The solution is either: (a) duplicate vertex data 4× in Data RAM, or (b) use MVI to reset CT1 before each row (costs 1 cycle per reset).

## Load DSP Program from SH2

```cpp
// Copy compiled program (.dsp assembly → binary words) to DSP Program RAM
void dsp_load_program(const uint32_t* prog, uint32_t word_count) {
    auto* dst = reinterpret_cast<volatile uint32_t*>(0x05FF8000);
    for (uint32_t i = 0; i < word_count; ++i) dst[i] = prog[i];
}

// Load data to Data RAM CT0 via SCU DMA Ch2
void dsp_load_data_ct0(const uint32_t* data, uint32_t word_count) {
    // NOTE: 'data' must be in WRAM-L (0x002xxxxx)
    volatile uint32_t* D2R  = reinterpret_cast<volatile uint32_t*>(0x05A00040);
    volatile uint32_t* D2W  = reinterpret_cast<volatile uint32_t*>(0x05A00044);
    volatile uint32_t* D2C  = reinterpret_cast<volatile uint32_t*>(0x05A00048);
    volatile uint32_t* D2EN = reinterpret_cast<volatile uint32_t*>(0x05A00050);

    *D2R  = reinterpret_cast<uint32_t>(data);
    *D2W  = 0x05FF8400;           // CT0 Data RAM
    *D2C  = word_count * 4;
    *D2EN = 0x01000001;           // Start DMA
    while (*D2EN & 0x01000000);   // Wait
}

// Execute DSP and wait
void dsp_run_sync() {
    volatile uint32_t* PPAF = reinterpret_cast<volatile uint32_t*>(0x05A00000);
    *PPAF = 0x00000101;           // Execute from PC=0
    while (*PPAF & 0x00010000);   // Wait for EXEC bit to clear
}
```

## DSP Assembler

The Hitachi DSP assembler (`dspasm`) is part of the official SBL/SGL toolkit. Alternative:
- **SNASM DSP** (Psygnosis, found in Saturn doc archive at antime.kapsi.fi)
- Write the 32-bit words manually according to the SCU manual encoding (SCU User's Manual, Third Version)

## Known Errata

1. **Outdated opcode documentation** in some manual versions. Cross-reference with Yabause/Kronos disassembler output.
2. **Bus contention:** If both SH2s are active on the B-bus while DSP runs, stall or crash may occur. Coordinate: disable slave SH2 or pause during critical DSP execution.
3. **AC truncation:** MOV A stores the upper 32 bits of the 48-bit AC. For 16.16 × 16.16 the result is already correct at that position (integer in bits 31:16, fraction in 15:0).
4. **Silent ring buffer wrap:** CT pointer wraps at 64 entries without warning. Data beyond position 63 silently corrupts.
