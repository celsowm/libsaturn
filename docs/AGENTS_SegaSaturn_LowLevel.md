# Low-Level Sega Saturn Development Guide (for `libsaturn`)

> **Purpose:** This document is written in an **AGENTS.md style**: it is optimized for an AI coding agent (Codex/Cline/etc.) to assist you in building a **high-performance, low-level Sega Saturn library** (`libsaturn`) without depending on higher-level/less-optimized stacks like SGL.

---

## 0) Project goals for `libsaturn`

### Goals
- **Direct hardware control** (memory-mapped I/O, registers, VRAM layouts) with **thin, zero-cost abstractions**.
- **Predictable performance**: prefer DMA, cache-aware code, minimal hidden work.
- **Dual SH-2 utilization**: make it *easy* to use the Slave CPU correctly (shared memory + signaling + job dispatch).
- **Complete subsystem coverage**: VDP1, VDP2, SCU (DMA + DSP), SMPC, CD block, SCSP + 68k, memory map, interrupts.
- **Modern build workflow**: sh-elf-gcc toolchain, reproducible builds, emulator-first (Mednafen), strong debug/profiling loop.
- **Surpass YAUL / Jo Engine / Saturn Ring Lib** by combining:
  - YAUL’s low-level power with better “ergonomics” and tooling
  - Jo Engine’s approachability without sacrificing correctness (audio, 3D math, VDP2 features)
  - SRL’s “modern workflow” ideas without inheriting SGL constraints

### Non-goals (optional, but recommended)
- No “full engine” (entities/scene graph/physics) inside `libsaturn`.
- No mandatory C++ runtime, exceptions, RTTI, heavy STL.
- No hard dependency on BIOS routines (optional wrappers allowed).

---

## 1) Saturn system overview

The Saturn is effectively **multiple computers** wired together:
- 2× **SH-2** (Master/Slave)
- **VDP1** (sprites + textured quads/polygons)
- **VDP2** (background planes + effects + priority/blend + output)
- **SCU** (DMA + interrupt controller + DSP)
- **SMPC** (system manager + pads + power/reset)
- **68k + SCSP** (audio CPU + 32-channel PCM/FM + effects)
- **CD block** (drive controller / registers via A-bus)

**Design rule:** treat each unit as an independent processor with its own memory and timing. Your job is orchestration.

---

## 2) Memory map (must-know)

> Use **uncached** mappings for device memory and shared CPU-to-CPU mailbox data.

### Key regions (physical/uncached access recommended)
| Region | Address Range | Size | Notes |
|---|---:|---:|---|
| BIOS ROM | 0x00000000–0x0007FFFF | 512 KB | Boot ROM (mirrors in some maps) |
| SMPC regs | 0x00100000–0x0010007F | 128 B | COMREG/IREG/OREG etc. |
| Backup RAM | 0x00180000–0x0018FFFF | 64 KB | Save memory mapping quirks |
| Work RAM Low | 0x00200000–0x002FFFFF | 1 MB | 16-bit DRAM; CPU-friendly; less contended |
| Work RAM High | 0x06000000–0x060FFFFF | 1 MB | 32-bit SDRAM; shared/bus contended |
| CD block regs | 0x05800000–... | — | A-bus mapped regs |
| Sound RAM | 0x05A00000–0x05A7FFFF | 512 KB | 68k + SCSP RAM |
| SCSP regs | 0x05B00000–0x05B00FFF | 4 KB | Audio registers |
| VDP1 VRAM | 0x05C00000–0x05C7FFFF | 512 KB | Commands + textures |
| VDP1 FB | 0x05C80000–0x05CFFFFF | 256 KB | One framebuffer mapped region |
| VDP1 regs | 0x05D00000–0x05D00FFF | 4 KB | PTMR, FBCR, EDSR, etc. |
| VDP2 VRAM | 0x05E00000–0x05E7FFFF | 512 KB | Tile/pattern/maps/rotation |
| VDP2 CRAM | 0x05F00000–0x05F00FFF | 4 KB | Color RAM (palettes) |
| VDP2 regs | 0x05F80000–0x05F807FF | 2 KB | TVMD, BGON, priorities, etc. |
| SCU regs | 0x05FE0000–0x05FE00FF | 256 B | DMA, DSP, interrupts |

### Caching and shared data
- Use **uncached** address mirrors for:
  - **device memory** (VDP1/2 VRAM, regs, SCU regs, SMPC regs)
  - **shared Master<->Slave mailboxes/flags**
- If you *must* use cached memory for shared buffers:
  - Implement explicit cache flush/invalidate
  - Keep coherence rules strict (single writer / single reader; double buffering)

---

## 3) Dual SH-2 programming model

### Hard truth
- Two CPUs share buses; naive parallelism can **slow you down**.
- You need:
  - clean partition of workloads
  - minimal shared memory traffic
  - predictable sync points

### Starting the Slave SH-2
- Use SMPC command `SSHON` to release/reset Slave.
- Ensure Slave reset vector points at safe code (loop/dispatcher).

### Recommended patterns

#### Pattern A: Slave “job dispatcher”
- Slave runs an infinite loop:
  - waits for a mailbox value or input-capture signal
  - reads a function pointer + args from shared uncached memory
  - executes it
  - posts completion flag
- Master posts jobs:
  - **render list build**
  - **transform batch**
  - **decompression**
  - **DMA setup orchestration**

#### Pattern B: Frame pipeline
- Master: game logic + input + high-level scheduling
- Slave: build VDP1 list + prepare background updates + staging DMA
- Sync once per frame:
  - barrier at end of list build
  - Master triggers VDP1 draw

### Inter-CPU signaling (recommended)
- Use the SH-2 FRT **input capture** signaling region:
  - write to 0x01000000–0x017FFFFF to signal **Slave**
  - write to 0x01800000–0x01FFFFFF to signal **Master**
- Treat as “soft interrupt” or wake-up mechanism.

### Shared memory rules
- Use a dedicated mailbox struct in **uncached WRAM**:
  - command id
  - function pointer
  - arg pointer
  - status (idle/busy/done)
- Keep mailbox hot and tiny to reduce bus hits.

---

## 4) VDP1 (sprites + textured quads/polygons)

### What VDP1 does
- Consumes a **command list** in VDP1 VRAM
- Renders into one of two **framebuffers**
- Reports completion via `EDSR` or interrupt (SCU)

### Command list basics
- Each command = 32 bytes
- Commands link by `CMDLINK` (usually sequential)
- List ends with an **End** command

### Core registers (VDP1 regs)
- `TVMR` — mode (interlace, etc.)
- `FBCR` — framebuffer control (buffer select, erase mode)
- `PTMR` — plot trigger (start draw)
- `EWDR/EWLR/EWRR` — erase color and rect
- `EDSR` — end draw status (poll)

### Recommended `libsaturn` design
- Provide a **command builder** that writes to:
  - staging buffer in WRAM (preferred) and DMA to VRAM
  - or directly to VDP1 VRAM (simple, but bus pressure)
- Provide explicit control over:
  - command order (sorting)
  - clipping/local coordinate commands
  - per-command draw mode bits

### Transparency and priority
- VDP1 transparency is limited; VDP2 does most blending.
- Your API should make it clear:
  - “this is half-transparent” (VDP2 color calc enabled)
  - ordering/priorities are handled by VDP2 priority system

---

## 5) VDP2 (background planes + effects + final output)

### What VDP2 does
- Drives the display timing and resolution
- Renders up to:
  - NBG0–NBG3 (tile/bitmap planes)
  - RBG0 (rotation/zoom/perspective plane)
  - sprite layer (VDP1 output)
- Performs:
  - priority resolution
  - transparency/color calculation (blending)
  - window/mask and line scroll tricks

### Minimum initialization checklist
1. Set `TVMD` (resolution + display enable)
2. Configure `BGON` to enable needed planes + sprite layer
3. Set priority registers for each plane + sprites
4. Configure character control (color depth, tile size)
5. Map VRAM usage: pattern data, maps, rotation tables
6. Load CRAM palettes
7. Set scroll values (SCX/SCY)

### Recommended `libsaturn` design
- `vdp2_init(mode)` presets common resolutions
- `vdp2_layer_config(layer, params)`:
  - tile mode vs bitmap
  - plane size
  - pattern/map base addresses
  - color depth + palette selection
- `vdp2_priority_set(layer, prio)`
- `vdp2_colorcalc_config(...)`:
  - enable per-layer blending
  - half-luminance / shadow features where relevant

---

## 6) SCU (DMA + interrupts + DSP)

### DMA (must use)
Use SCU DMA for:
- uploading VDP1 command lists
- uploading textures to VDP1 VRAM
- uploading tiles/maps to VDP2 VRAM
- bulk memcpy between WRAM blocks

**Goal:** reduce CPU cycles wasted moving memory.

Recommended features for `libsaturn`:
- `scu_dma_copy(dst, src, bytes, mode)`
- `scu_dma_chain(table)` for indirect/linked transfers
- simple “VRAM upload” helpers:
  - `vdp1_upload_texture(...)`
  - `vdp2_upload_tiles(...)`

### Interrupt routing
SCU controls interrupt distribution and status.
Provide:
- `scu_irq_enable(mask)`
- `scu_irq_set_handler(vec, fn)`
- helpers for:
  - VBlank in/out
  - VDP1 end draw
  - DMA completion

### DSP (optional but powerful)
- SCU DSP can do fixed-point vector/matrix math.
- Complex to program, but if you can:
  - offload transform batches
  - do clip/cull operations
- Start with CPU math; only adopt DSP when needed.

---

## 7) SMPC (pads + system control)

### Core usage
- Issue commands via `COMREG`, pass params in `IREG`, read results from `OREG`.
- Poll status flags until command completion.

### Must-have commands for a library
- `INTBACK`: read controllers into OREGs
- `SSHON/SSHOFF`: control Slave SH2
- `SNDON/SNDOFF`: control sound CPU
- `CDON/CDOFF`: control CD block
- `SYSRES`: system reset (dev/testing)

Recommended `libsaturn` API:
- `smpc_init()`
- `smpc_intback_read(pad_state*)` (decode raw bits into stable struct)
- `smpc_slave_on(start_vector)` (with caution; ensure correct vector setup)

---

## 8) Audio (68k + SCSP)

### Hardware facts
- 68k runs sound driver code
- SCSP provides 32 channels:
  - PCM sample playback or FM operator use
  - built-in effect DSP (reverb etc.)
- Sound RAM is separate (512 KB)

### Recommended approach
- Run a **minimal 68k driver**:
  - receives commands from SH2 via shared RAM mailbox
  - updates SCSP channel regs
- Keep SH2 audio API thin:
  - `snd_init()`
  - `snd_load_sample(id, data, len)`
  - `snd_play(id, volume, pan, pitch)`
  - `snd_stop(channel or id)`

### Big warning
- Avoid trying to mix everything on SH2 unless you have to.
- Let SCSP do the mixing whenever possible.

---

## 9) CD-ROM (CD block)

### Practical stance
Implementing full CD stack at bare metal is large work.
Recommended staged approach:
1. **Dev workflow first:** build bootable CUE/BIN images for Mednafen.
2. **BIOS-assisted reads (optional):** wrap BIOS routines if acceptable.
3. **Native CD block driver (later):** only if you truly need it.

`libsaturn` should at least include:
- image building tooling (IP.BIN + binary + cue/bin)
- sector read helpers (even if BIOS-backed initially)

---

## 10) Toolchain and build workflow

### Toolchain
- `sh-elf-gcc` targeting SH-2 (`-m2`)
- `binutils` for `sh-elf-as`, `sh-elf-ld`, `sh-elf-objcopy`
- Optional: `m68k-elf` tools if building a 68k driver

### Linker script requirements
- Place `.text` at a stable address in WRAM-H (common: 0x06004000).
- Define stack, BSS, and optionally a shared mailbox section in uncached RAM.
- Ensure alignment and CD image constraints (sector alignment).

### Makefile conventions (suggested)
- `make all` → build ELF + BIN + ISO/CUE
- `make run` → run Mednafen with generated image
- `make debug` → run Mednafen, open debugger quickly

---

## 11) Mednafen debugging workflow

### Enter debugger
- `Alt + D` toggle debugger UI
- Useful views:
  - CPU disasm
  - memory editor
  - graphics/VRAM views
  - log/console

### Debugging practices
- Compile with `-g` in debug build.
- Use breakpoints on:
  - your init code
  - VBlank handlers
  - VDP1 trigger writes
- Inspect VRAM to verify:
  - command list correct
  - textures correctly uploaded
  - palette entries correct

---

## 12) Profiling and performance techniques

### Frame timing budget
- NTSC: ~16.67ms per frame.
- Use VBlank interrupts + timers to measure.

### SH-2 FRT timing (recommended)
- read timer before/after hot section
- accumulate stats, print on-screen or store in RAM

### Performance principles
- Prefer DMA for bulk transfers.
- Keep command list compact.
- Keep hot loops in cache; avoid bus contention.
- Partition workloads between WRAM-L and WRAM-H to reduce collisions.
- Use fixed-point math for transforms.

---

## 13) `libsaturn` module layout (suggested)

```
libsaturn/
  include/
    saturn/
      hw/          # register maps, bit masks
      vdp1.h
      vdp2.h
      scu.h
      smpc.h
      scsp.h
      cd.h
      sh2.h        # cache/timer/irq helpers
      types.h
  src/
    vdp1/
    vdp2/
    scu/
    smpc/
    audio/
    cd/
    sh2/
  examples/
    00_boot
    01_vdp1_sprite
    02_vdp2_tile
    03_dma_upload
    04_dual_sh2_jobs
    05_audio_pcm
  tools/
    make_cd_image.py
    ipbin/
      IP.BIN
```

### Library policies
- “thin wrappers” by default; make it easy to drop to raw regs.
- Provide safe defaults + explicit opt-in complexity:
  - dual SH-2 (opt-in)
  - DSP (opt-in)
  - CD native driver (later/opt-in)

---

## 14) How to surpass existing libs (practical checklist)

### Surpass Jo Engine
- Provide **working audio** (stable 68k driver + SCSP control).
- Provide true VDP2 support: multiple layers, priorities, blending.
- Provide optional 3D pipeline building blocks (not necessarily a full engine).

### Surpass YAUL
- Keep the same low-level power, but ship:
  - consistent module conventions
  - a robust image build workflow
  - a dual-CPU job dispatcher template
  - clear docs like this one

### Surpass Saturn Ring Library / SGL
- Avoid SGL constraints:
  - build your own VDP1 list
  - handle sorting and transform in your own code
  - reduce overhead / memory footprint
- Provide modern tooling and ergonomic APIs without hidden costs.

---

## 15) Next actions (recommended roadmap)

1. **Boot + interrupt baseline**
   - clean init, VBlank handler, stable framebuffer clear
2. **VDP1 sprite**
   - draw textured quad from a known texture in VRAM
3. **VDP2 layer**
   - enable NBG0 tile layer + palette + scroll
4. **DMA upload**
   - upload textures and command list via SCU DMA
5. **Dual SH-2**
   - bring up Slave safely + implement job dispatcher
6. **Audio**
   - minimal 68k driver + PCM channel play
7. **CD (optional)**
   - BIOS-backed sector reads + simple file loader

---

## Appendix A) “Do not do this” list
- Don’t write shared flags in cached memory without flush/invalidate.
- Don’t build VDP1 lists directly into VRAM in a way that starves VDP2 fetch.
- Don’t do huge CPU memcpy loops when DMA is available.
- Don’t attempt SCU DSP before you have a stable CPU baseline.
- Don’t rely on undefined register states; always set what you use.

---

## Appendix B) Minimal register mapping style (template)

```c
// Example: VDP1 register mapping (uncached)
#define VDP1_REG_BASE   0x25D00000u
#define VDP1_TVMR       (*(volatile uint16_t*)(VDP1_REG_BASE + 0x0000))
#define VDP1_FBCR       (*(volatile uint16_t*)(VDP1_REG_BASE + 0x0002))
#define VDP1_PTMR       (*(volatile uint16_t*)(VDP1_REG_BASE + 0x0004))
#define VDP1_EDSR       (*(volatile uint16_t*)(VDP1_REG_BASE + 0x0010))
```

**Rule:** keep these in `include/saturn/hw/*.h` and never duplicate.

---

## 16) Deep Dive: Hardware Structures & Constraints

### 16.1 VDP1 Command Structure (The "Quad" format)

VDP1 commands are 32 bytes aligned. The command list acts as a linked list, but CMDLINK is a relative word offset, not a pointer.

```c
// VDP1 Command Table (32 bytes)
typedef struct {
    uint16_t cmdctrl;   // Command Control (Jump/Zoom/Dir/Flip)
    uint16_t cmdlink;   // Link: (Next_Addr - Curr_Addr) / 4
    uint16_t pmode;     // Draw Mode (Color mode, transparency, mesh)
    uint16_t cmdcolr;   // Color Control (Palette bank or solid color)
    uint16_t cmdsrca;   // Texture Source Address (in VRAM / 8)
    uint16_t cmdsize;   // Texture Size (W, H)
    int16_t  xa, ya;    // Vertex A (Top-Left)
    int16_t  xb, yb;    // Vertex B (Top-Right)
    int16_t  xc, yc;    // Vertex C (Bottom-Right)
    int16_t  xd, yd;    // Vertex D (Bottom-Left)
    uint16_t grda;      // Gouraud Shading Table Addr
    uint16_t reserved;
} Vdp1Cmd;
```

Important CMDCTRL Bits (0x0000):
- #define CMDCTRL_END_BIT     0x8000
- #define CMDCTRL_JP_JUMP     0x1000 // Jump to address (subroutine)
- #define CMDCTRL_JP_LINK     0x0000 // Normal link
- #define CMDCTRL_ZOOM_ON     0x0100
- #define CMDCTRL_DIR_NORMAL  0x0000
- #define CMDCTRL_DIR_HFLIP   0x0010
- #define CMDCTRL_DIR_VFLIP   0x0020

Implementation Tip: When building the command list in WRAM, calculate cmdlink carefully. For a sequential list, cmdlink is usually 0x0008 (skipping 8 words / 32 bytes forward).

### 16.2 VDP2 Cycle Patterns (The "Garbage Screen" Preventer)

VDP2 VRAM is split into banks (A0, A1, B0, B1). If you try to read too much from one bank in a single scanline (e.g., reading character pattern + map + rotation data all from Bank A0), the VDP2 will output garbage.

You must configure the Cycle Pattern Registers (CYCA0-CYCB1) to allocate "access slots" (T0-T7) to specific VRAM banks.

Valid Cycle Pattern Example (Standard):

VRAM A0: NBG0 Pattern Data

VRAM A1: NBG1 Pattern Data

VRAM B0: NBG0 Map Data

VRAM B1: NBG1 Map Data

```c
// Register: CYCA0L (0x25F800B0) - Defines access for Bank A0
// Value: 0xFFF2 (Slots T0-T3 = free, T4 = NBG0 Pattern fetch)
```

Constraint: You cannot have two layers reading from Bank A0 at the same time slot.

Fix: Distribute your assets (Patterns vs Maps) across A0, A1, B0, B1 physically.

### 16.3 SCU DMA Indirect Mode

Direct DMA is simple (Src->Dst). Indirect DMA is faster for command lists because the CPU sets up a table of transfers and the SCU processes them without waking the CPU.

```c
// Indirect DMA Table Format (Uncached WRAM)
typedef struct {
    uint32_t len_and_flags; // Bit 31: Indirect End Bit. Bits 0-19: Length
    uint32_t src_addr;      // Absolute Source Address
    uint32_t dst_addr;      // Absolute Destination Address
} ScuDmaTableEntry;

// Usage:
// 1. Build table in WRAM.
// 2. Set SCU DMA Register (D0R) to point to this table.
// 3. Trigger DMA channel 0 (Indirect Mode bit set).
```

### 16.4 SH-2 Cache Management (Vital)

The SH-2 does not have bus snooping. If SCU DMA writes to WRAM, the CPU cache will hold stale data.

How to Purge/Invalidate: You typically cannot "purge" (write-back) individual lines easily on Saturn SH-2 without complex assembly. The Strategy:

Invalidate All: Write to the Cache Control Register (CCR) to flush the entire cache if you suspect widespread dirty data (heavy).

Uncached Access: Always read DMA destinations using the 0x20000000 offset (uncached window) to bypass the cache entirely.

```c
#define CCR_ADDR 0xFFFFFE92
#define CACHE_INV 0x08 // Cache Invalidate bit
// void cache_flush() { *CCR_ADDR |= CACHE_INV; }
```

### 16.5 SMPC Handshake Protocol

Sending commands to the System Manager (SMPC) requires a strict handshake.

Write Parameters: Write to IREG0...IREG6.

Issue Command: Write command ID to COMREG.

Set Flag: SMPC sets SF (Status Flag) register to 1 (busy).

Wait: Loop until SF becomes 0 (even/finished).

```c
// Pseudo-code
void smpc_cmd(uint8_t cmd) {
    while(SMPC_SF & 0x01); // Wait if busy
    SMPC_SF = 1;           // Set flag manually (some modes require this)
    SMPC_COMREG = cmd;     // Fire
    while(SMPC_SF & 0x01); // Wait for finish
}
```

## 17) Critical Interrupts & Vector Table (VBR)

You cannot use the BIOS defaults for high performance. You must point the VBR (Vector Base Register) to your own table in WRAM.

Key Vector Offsets: | Vector | Offset | Source | Purpose | | :--- | :--- | :--- | :--- | | 64 | 0x100 | V-Blank IN | Start of VBlank (Update VDP regs now) | | 65 | 0x104 | V-Blank OUT | End of VBlank (Game logic start) | | 66 | 0x108 | H-Blank | Raster effects (Palette swaps per line) | | 71 | 0x11C | VDP1 End | Sprite draw finished (Swap buffers now) | | 74 | 0x128 | SCU DMA 0 | Level 0 DMA done |

SCU Mask Register (0x25FE00A0): You must unmask these interrupts in the SCU to allow them to reach the CPU.

Bit 0: V-Blank IN

Bit 1: V-Blank OUT

Bit 2: H-Blank

Bit 5: VDP1 End

## 18) Bus Arbitration Hierarchy

When multiple devices try to access memory, the SCU arbitrates. Priority Order (Highest to Lowest):

VDP1 / VDP2 (Video refresh—if this is blocked, screen gltiches)

SCU DSP

SCU DMA

CPU

Performance Impact: If you run heavy SCU DMA transfers during active display time, the CPU will stall significantly when trying to access WRAM-H. Tip: Schedule heavy DMA transfers (texture uploads) during V-Blank to avoid starving the CPU.

Low-Level Resource Video
... VDP1 & VDP2 Architecture Analysis ...

This video provides a visual breakdown of the VDP1 command tables and VDP2 layers referenced in section 16, reinforcing the relationship between the command lists and the resulting frame output.

[Sega Saturn Game Development] SGL Tutorial - 001 - Basics & VDP1

Emerald Nova · 4,4 mil visualizações
