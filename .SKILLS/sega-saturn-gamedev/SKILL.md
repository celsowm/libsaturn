---
name: sega-saturn-gamedev-baremetal
description: >
  Use this skill for any library development, engine, framework, or
  low-level code task for Sega Saturn WITHOUT SGL and WITHOUT libyaul or any external SDK.
  100% bare-metal: your own cross GCC compiler, your own crt0.s, your own linker script.
  Triggers: "Saturn lib", "Saturn engine", "homebrew Saturn", "VDP1", "VDP2", "SH2",
  "SCU DSP", "SCSP", "Saturn without SGL", "Saturn bare-metal", "Saturn 3D engine",
  "Saturn game library", any request for direct Saturn hardware access.
  Always use this skill when the user mentions Sega Saturn in the context of development.
---

# Sega Saturn — Bare-Metal Game Library (No SGL, No libyaul)

Completely independent approach with no external SDK dependencies. Only:
- **sh2eb-elf-gcc** cross-compiler built from scratch
- Hand-written **crt0.s** and **linker script**
- Direct hardware register access
- Modern C++ + inline/pure SH2 assembly

Subsystem details are in the reference files at `references/`.

---

## 1. Why Not SGL and Not libyaul?

**SGL:**
- ~500 quads/frame limit is a software restriction, not hardware
- Pure C with no pipeline optimization, no SCU DSP usage, no Slave SH2 usage
- Forces use of internal structures incompatible with modern C++

**libyaul:**
- No relevant updates since ~2022; GCC locked to old version
- Too much abstraction: prevents fine-grained access to VDP1 command list
- Complex environment dependency that frequently breaks on new distros

**Solution:** own compiler + zero dependencies on external runtime.

---

## 2. Hardware in 30 Seconds

| Chip      | Function                       | Clock       |
|-----------|--------------------------------|-------------|
| SH2 Master| Main logic, rendering          | 28.63 MHz   |
| SH2 Slave | Parallel geometry              | 28.63 MHz   |
| SCU DSP   | Batch matrix multiplication    | 14.31 MHz   |
| VDP1      | Rasterizer (command list)      | 28.63 MHz   |
| VDP2      | Background planes, compositing | 28.63 MHz   |
| M68k+SCSP | PCM/FM audio, 32 channels     | 11.3 MHz    |
| SMPC      | Controllers, reset, clock      | 4 MHz       |

Full memory map → `references/memory_map.md`

---

## 3. Toolchain: Building the Cross-Compiler from Scratch

**Only dependency:** Host GCC + binutils. No Saturn-specific packages.

### 3.1 Building sh2eb-elf-gcc

```bash
#!/usr/bin/env bash
# build-toolchain.sh — creates sh2eb-elf-gcc in ~/saturn-tools/

set -e

PREFIX="$HOME/saturn-tools"
TARGET="sh2eb-elf"
GCC_VER="13.2.0"
BINUTILS_VER="2.41"
NEWLIB_VER="4.3.0.20230120"

JOBS=$(nproc)
mkdir -p "$PREFIX" build-binutils build-gcc

# ---- Binutils ----
[ ! -f "binutils-$BINUTILS_VER.tar.xz" ] && \
  wget "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VER.tar.xz"
tar xf "binutils-$BINUTILS_VER.tar.xz"

cd build-binutils
../binutils-$BINUTILS_VER/configure \
  --target=$TARGET \
  --prefix=$PREFIX \
  --with-sysroot \
  --disable-nls \
  --disable-werror
make -j$JOBS && make install
cd ..

# ---- GCC (no newlib first — C/C++ compiler only) ----
[ ! -f "gcc-$GCC_VER.tar.xz" ] && \
  wget "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VER/gcc-$GCC_VER.tar.xz"
tar xf "gcc-$GCC_VER.tar.xz"
cd gcc-$GCC_VER && ./contrib/download_prerequisites && cd ..

mkdir -p build-gcc && cd build-gcc
../gcc-$GCC_VER/configure \
  --target=$TARGET \
  --prefix=$PREFIX \
  --without-headers \
  --disable-nls \
  --disable-shared \
  --disable-multilib \
  --disable-decimal-float \
  --disable-threads \
  --disable-libatomic \
  --disable-libgomp \
  --disable-libquadmath \
  --disable-libssp \
  --disable-libvtv \
  --disable-libstdcxx \
  --enable-languages=c,c++ \
  --with-endian=big \
  --with-cpu=sh2
make -j$JOBS all-gcc all-target-libgcc
make install-gcc install-target-libgcc
cd ..

echo "Toolchain ready at $PREFIX/bin/${TARGET}-gcc"
```

Add to PATH:
```bash
export PATH="$HOME/saturn-tools/bin:$PATH"
# Or permanently in ~/.bashrc
echo 'export PATH="$HOME/saturn-tools/bin:$PATH"' >> ~/.bashrc
```

### 3.2 Required Compilation Flags

```makefile
# Makefile — critical flags
CC  = sh2eb-elf-gcc
CXX = sh2eb-elf-g++
AS  = sh2eb-elf-as
LD  = sh2eb-elf-ld
OBJCOPY = sh2eb-elf-objcopy

# -m2       → SH2 ISA
# -mb       → Big-endian (Saturn is big-endian!)
# -O2       → Enables MAC instruction, pipeline scheduling
# -fno-exceptions -fno-rtti  → Removes C++ overhead
# -ffreestanding → No host libc
# -fomit-frame-pointer → R14 free for general use
# -fipa-ra  → Inter-procedure register allocation
CFLAGS  = -m2 -mb -O2 -ffreestanding -fno-exceptions \
           -fomit-frame-pointer -fipa-ra -Wall
CXXFLAGS = $(CFLAGS) -fno-rtti -std=c++20
ASFLAGS = --isa=sh2 -big
```

---

## 4. Complete Startup from Scratch

The Saturn has no OS. We need:
1. **crt0.s** — program entry, configures stack, zeroes BSS, calls `main`
2. **saturn.ld** — linker script with correct memory map
3. **IP.BIN** — boot header (16 fixed sectors; use the one from SGL sample/sys or generate with isomaker)

### 4.1 crt0.s — SH2 Startup Assembly

```asm
! crt0.s — Saturn bare-metal startup
! Runs after IP.BIN transfers control to 0x06004000

    .section .text.start
    .global _start
    .align 2

_start:
    ! ── Disable interrupts ─────────────────────────────────
    mov.l   sr_val, r0
    ldc     r0, sr              ! SR = 0xF0 (IPM mask = 15)

    ! ── Master SH2 stack pointer ─────────────────────────────────
    mov.l   stack_top, r15      ! SP = top of WRAM-H (0x060FFFFC)

    ! ── Clear registers ─────────────────────────────────────
    xor     r0, r0
    xor     r1, r1
    xor     r2, r2
    xor     r3, r3
    xor     r4, r4
    xor     r5, r5
    xor     r6, r6
    xor     r7, r7

    ! ── Copy .data section from ROM to WRAM-H ────────────────────
    mov.l   data_lma, r0        ! Source: LMA in binary
    mov.l   data_vma, r1        ! Destination: WRAM-H
    mov.l   data_end, r2
    cmp/eq  r1, r2
    bt      bss_zero            ! Nothing to copy if equal
copy_data:
    mov.l   @r0+, r3
    mov.l   r3, @r1
    add     #4, r1
    cmp/hs  r2, r1
    bf      copy_data

    ! ── Clear .bss section ─────────────────────────────────────────
bss_zero:
    mov.l   bss_start, r0
    mov.l   bss_end,   r1
    xor     r2, r2
    cmp/eq  r0, r1
    bt      call_ctors
zero_loop:
    mov.l   r2, @r0
    add     #4, r0
    cmp/hs  r1, r0
    bf      zero_loop

    ! ── C++ constructors (if any) ─────────────────────────────
call_ctors:
    mov.l   ctors_start, r0
    mov.l   ctors_end,   r1
    cmp/eq  r0, r1
    bt      jump_main
ctor_loop:
    mov.l   @r0+, r2
    jsr     @r2
    nop
    cmp/hs  r1, r0
    bf      ctor_loop

    ! ── Call main ──────────────────────────────────────────────
jump_main:
    mov.l   main_addr, r0
    jsr     @r0
    nop

    ! ── Never return — infinite loop ──────────────────────────
hang:
    bra     hang
    nop

    .align 4
sr_val:     .long 0x000000F0    ! IPM=15 (all ints masked)
stack_top:  .long 0x060FFFFC    ! Top of WRAM-H (Master SH2)
data_lma:   .long __data_load
data_vma:   .long __data_start
data_end:   .long __data_end
bss_start:  .long __bss_start
bss_end:    .long __bss_end
ctors_start:.long __ctors_start
ctors_end:  .long __ctors_end
main_addr:  .long main
```

### 4.2 saturn.ld — Linker Script

```ld
/* saturn.ld — Bare-metal linker script for Sega Saturn
   Program loaded to WRAM-H starting at 0x06004000
   (IP.BIN occupies 0x06000000..0x06003FFF) */

OUTPUT_FORMAT("elf32-sh", "elf32-sh", "elf32-sh")
OUTPUT_ARCH(sh)
ENTRY(_start)

MEMORY {
    /* WRAM-H: 1MB fast SDRAM. Reserve 16KB for IP.BIN */
    WRAMH (rwx) : ORIGIN = 0x06004000, LENGTH = 0x000FC000
    /* WRAM-L: 1MB slow DRAM. For DMA data and static meshes */
    WRAML (rw)  : ORIGIN = 0x00200000, LENGTH = 0x00100000
}

SECTIONS {
    /* ── Code + read-only constants ── */
    .text 0x06004000 : {
        *(.text.start)      /* crt0 always first */
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } > WRAMH

    /* ── Initialized data ── */
    __data_load = LOADADDR(.data);
    .data : AT(__data_load) {
        __data_start = .;
        *(.data .data.*)
        . = ALIGN(4);
        __data_end = .;
    } > WRAMH

    /* ── C++ constructors/destructors ── */
    .ctors : {
        __ctors_start = .;
        KEEP(*(SORT(.ctors.*)))
        KEEP(*(.ctors))
        __ctors_end = .;
    } > WRAMH

    .dtors : {
        __dtors_start = .;
        KEEP(*(SORT(.dtors.*)))
        KEEP(*(.dtors))
        __dtors_end = .;
    } > WRAMH

    /* ── BSS (does not occupy space in binary) ── */
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > WRAMH

    /* ── WRAM-L: static meshes, DMA buffers ── */
    .wram_l (NOLOAD) : {
        *(.wram_l)
        . = ALIGN(4);
    } > WRAML

    /* Stacks are allocated at end of WRAM-H via initial SP in crt0 */
    /* Master SH2: 0x060FFFFC (grows downward) */
    /* Slave SH2:  0x060BFFFC (grows downward) — set via SMPC before activating */

    /* Discard unnecessary sections */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.ARM.*)
    }
}
```

### 4.3 Complete Makefile

```makefile
# Makefile — bare-metal Saturn project

CC      := sh2eb-elf-gcc
CXX     := sh2eb-elf-g++
AS      := sh2eb-elf-as
LD      := sh2eb-elf-ld
OBJCOPY := sh2eb-elf-objcopy
MKISOFS := mkisofs

CFLAGS   := -m2 -mb -O2 -ffreestanding -fno-exceptions \
            -fomit-frame-pointer -fipa-ra -Wall -Wextra
CXXFLAGS := $(CFLAGS) -fno-rtti -std=c++20
ASFLAGS  := --isa=sh2 -big
LDFLAGS  := -T saturn.ld --no-gc-sections

SRC_C   := $(wildcard src/**/*.c src/*.c)
SRC_CXX := $(wildcard src/**/*.cpp src/*.cpp)
SRC_AS  := $(wildcard src/**/*.s src/*.s) crt0.s

OBJ := $(SRC_C:.c=.o) $(SRC_CXX:.cpp=.o) $(SRC_AS:.s=.o)

TARGET := game

all: $(TARGET).iso

$(TARGET).elf: $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $^

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).iso: $(TARGET).bin
	mkdir -p iso_root
	cp $(TARGET).bin iso_root/0.BIN   # "first file" = entry point
	$(MKISOFS) \
	  -sysid "SEGA SATURN" \
	  -volid "GAME" \
	  -publisher "STUDIO" \
	  -l -iso-level 1 \
	  -joliet \
	  -o $(TARGET).iso \
	  ip.bin                           # IP.BIN as special track
	  # (see section 5 about IP.BIN)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.s
	$(CC) $(ASFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET).elf $(TARGET).bin $(TARGET).iso

.PHONY: all clean
```

---

## 5. IP.BIN — Boot Header

IP.BIN occupies the first 16 sectors (32 KB) of the CD. It is read by the Saturn BIOS before your code. You have two options:

**Option A — Use the IP.BIN from SGL samples (recommended for getting started:**
The file `sample/sys/ip.bin` bundled with SBL/SGL is widely redistributed and works without modifications. It configures basic hardware and jumps to `0x06004000`.

**Option B — Minimal IP.BIN written from scratch:**
```
IP.BIN Structure — System ID (32KB = 0x8000 bytes):
  Offset 0x000–0x00F : Hardware ID        "SEGA SEGASATURN " (16 bytes)
  Offset 0x010–0x01F : Maker ID           (16 bytes)
  Offset 0x020–0x029 : Product Number     (10 bytes)
  Offset 0x02A–0x02F : Version            (6 bytes, e.g.: "V1.000")
  Offset 0x030–0x037 : Release Date       YYYYMMDD (8 bytes)
  Offset 0x038–0x03F : Device Info        (8 bytes, e.g.: "CD-1/1  ")
  Offset 0x040–0x049 : Area Symbols       (10 bytes, e.g.: "JTUE      ")
  Offset 0x04A–0x04F : (padding)          (6 bytes)
  Offset 0x050–0x05F : Peripherals        (16 bytes)
  Offset 0x060–0x0CF : Game Title         (112 bytes ASCII)
  Offset 0x0D0–0x0DF : (reserved)
  Offset 0x0E0–0x0E3 : IP Size            (4 bytes, binary big-endian)
  Offset 0x0E8–0x0EB : Master Stack       (4 bytes, binary BE, e.g.: 0x060FFFFC)
  Offset 0x0EC–0x0EF : Slave Stack        (4 bytes, binary BE)
  Offset 0x0F0–0x0F3 : 1st Read Address   (4 bytes, binary BE, e.g.: 0x06004000)
  Offset 0x0F4–0x0F7 : 1st Read Size      (4 bytes, binary BE)
  Offset 0x100–0x7FFF: Security/boot code area
```

For practical purposes: **use the ip.bin from SBL sample/sys**. It is the same across all 1st party games and does not contain new proprietary code.

---

## 6. VDP1 — Direct Access (Without SGL)

VDP1 is **command list** oriented: you write a list of 32-byte commands to VDP1 VRAM and trigger execution.

### 6.1 VDP1 Registers

```cpp
// hal/vdp1.hpp — direct access via volatile pointers
namespace VDP1 {
    inline auto& TVMR = *reinterpret_cast<volatile uint16_t*>(0x05D00000);
    inline auto& FBCR = *reinterpret_cast<volatile uint16_t*>(0x05D00002);
    inline auto& PTMR = *reinterpret_cast<volatile uint16_t*>(0x05D00004);
    inline auto& EWDR = *reinterpret_cast<volatile uint16_t*>(0x05D00006);
    inline auto& EWLR = *reinterpret_cast<volatile uint16_t*>(0x05D00008);
    inline auto& EWRR = *reinterpret_cast<volatile uint16_t*>(0x05D0000A);
    inline auto& EDSR = *reinterpret_cast<volatile uint16_t*>(0x05D0000E); // read

    constexpr uint32_t VRAM_BASE = 0x05C80000;

    void init() {
        TVMR = 0x0000; // 16-bit color, 512px wide, non-rotate
        FBCR = 0x0000; // Manual erase
        PTMR = 0x0002; // Auto-draw on VBLANK
        EWDR = 0x0000; // Erase color = black
        EWLR = 0x0000;
        EWRR = static_cast<uint16_t>(((320 / 8) << 9) | 224); // Erase area 320×224
    }
}
```

### 6.2 Command Structure (32 bytes)

```cpp
struct alignas(4) Vdp1Cmd {
    uint16_t ctrl;   // Command type + options
    uint16_t link;   // Next command: address >> 3 (0 = sequential)
    uint16_t pmod;   // Draw mode: color mode, transparency
    uint16_t colr;   // Palette base or direct color
    uint16_t srca;   // Texture address >> 3 (relative to VRAM)
    uint16_t size;   // (width/8 << 8) | height
    int16_t  xa, ya; // Vertex A (top-left)
    int16_t  xb, yb; // Vertex B (top-right)
    int16_t  xc, yc; // Vertex C (bottom-right)
    int16_t  xd, yd; // Vertex D (bottom-left)
    uint16_t grda;   // Gouraud table >> 3
    uint16_t _pad;
};
static_assert(sizeof(Vdp1Cmd) == 32);

// ctrl bits:
//   15:14 = type: 00=normal sprite, 10=distorted quad (3D!), 11=solid-color polygon
//   13    = end of list
//    2    = use Gouraud shading
// pmod bits:
//    6:4  = color mode: 011=256 colors, 100=RGB555
//    2    = transparency (color 0 = transparent)
```

### 6.3 Texture in VDP1 VRAM

```cpp
// Simple texture manager (linear cursor)
namespace TexCache {
    static uint32_t cursor = 0x1000; // First 4KB reserved for cmd list

    // Returns value for srca field (offset >> 3)
    uint16_t upload(const void* data, uint32_t bytes) {
        cursor = (cursor + 7) & ~7u; // Align to 8 bytes
        auto* dst = reinterpret_cast<volatile uint16_t*>(VDP1::VRAM_BASE + cursor);
        auto* src = static_cast<const uint16_t*>(data);
        for (uint32_t i = 0; i < bytes / 2; ++i) dst[i] = src[i];
        uint16_t srca = static_cast<uint16_t>(cursor >> 3);
        cursor += bytes;
        return srca;
    }
}
```

### 6.4 Building 3D Quads

```cpp
// Writes a textured quad to the command list
inline void write_quad(Vdp1Cmd* cmd,
                       int16_t xa, int16_t ya,
                       int16_t xb, int16_t yb,
                       int16_t xc, int16_t yc,
                       int16_t xd, int16_t yd,
                       uint16_t srca, uint16_t w8, uint16_t h,
                       uint16_t palette) {
    cmd->ctrl = 0x0004;  // Distorted sprite (arbitrary quad)
    cmd->link = 0;
    cmd->pmod = 0x00C0;  // 256 colors, end-codes on, transparency on
    cmd->colr = palette;
    cmd->srca = srca;
    cmd->size = static_cast<uint16_t>((w8 << 8) | h);
    cmd->xa = xa; cmd->ya = ya;
    cmd->xb = xb; cmd->yb = yb;
    cmd->xc = xc; cmd->yc = yc;
    cmd->xd = xd; cmd->yd = yd;
    cmd->grda = 0;
    cmd->_pad = 0;
}

// End command list (required!)
inline void end_list(Vdp1Cmd* cmd) {
    cmd->ctrl = 0x8000; // Bit 15 = End of List
}
```

### 6.5 List Upload and Trigger

```cpp
void frame_submit(const Vdp1Cmd* cmds, uint32_t count) {
    // Copy list from WRAM-H to VDP1 VRAM via direct write
    // (for large N, use SCU DMA — see SCU DMA section)
    auto* dst = reinterpret_cast<volatile uint32_t*>(VDP1::VRAM_BASE);
    auto* src = reinterpret_cast<const uint32_t*>(cmds);
    uint32_t dwords = (count * sizeof(Vdp1Cmd)) / 4;
    for (uint32_t i = 0; i < dwords; ++i) dst[i] = src[i];

    // Trigger: swap framebuffers + erase + draw
    VDP1::FBCR = 0x0003;
    VDP1::PTMR = 0x0001;
}
```

---

## 7. VDP2 — Backgrounds

```cpp
namespace VDP2 {
    // Register access macro by index (base 0x05F00000)
    template<uint32_t Offset>
    inline auto& REG = *reinterpret_cast<volatile uint16_t*>(0x05F00000 + Offset);

    inline auto& TVMD   = REG<0x00>;
    inline auto& RAMCTL = REG<0x0E>;
    inline auto& BGON   = REG<0x10>;
    inline auto& CHCTLA = REG<0x18>;
    inline auto& PNCN0  = REG<0x20>;
    inline auto& PRISA  = REG<0x98>; // Sprite priority
    inline auto& PRINA  = REG<0xA0>; // NBG0/NBG1 priority

    // 320×224 NTSC, VDP1 sprites at priority 6, NBG0 at 1
    void init_320x224_ntsc() {
        TVMD   = 0x8110; // Display on, 320×224, NTSC
        RAMCTL = 0x1F00;
        BGON   = 0x0003; // Sprites (VDP1) + NBG0
        CHCTLA = 0x0002; // NBG0: 256 colors, 8×8 tiles
        PRISA  = 0x0006; // Sprites type 0 = priority 6
        PRINA  = 0x0001; // NBG0 = priority 1
    }
}
```

VDP2 complete registers → `references/vdp2_regs.md`

---

## 8. Interrupts via SCU

```cpp
// Interrupt vectors (BIOS table in WRAM-H)
// The Saturn BIOS reads these addresses during initialization
constexpr uint32_t* IVT = reinterpret_cast<uint32_t*>(0x06000300);

#define SCU_IST  (*reinterpret_cast<volatile uint32_t*>(0x05A0001C))
#define SCU_IMS  (*reinterpret_cast<volatile uint32_t*>(0x05A00024))
#define INT_VBLANK_IN  (1u << 0)

static volatile uint32_t g_frame = 0;

// VBlank handler — must be linked in .text section
extern "C" void vblank_in_handler() {
    SCU_IST &= ~INT_VBLANK_IN; // Clear flag
    ++g_frame;
}

void interrupts_init() {
    IVT[0x40] = reinterpret_cast<uint32_t>(vblank_in_handler); // VBlank-IN
    SCU_IMS &= ~INT_VBLANK_IN; // Unmask

    // Enable interrupts in SH2 SR
    asm volatile(
        "stc  sr, r0    \n"
        "and  #0x0F, r0 \n" // IPM = 0 (accept all)
        "ldc  r0, sr    \n"
        ::: "r0"
    );
}

// Wait for next VBlank (frame synchronization)
inline void wait_vblank() {
    uint32_t prev = g_frame;
    while (g_frame == prev);
}
```

---

## 9. Dual SH2 — Parallel Slave

```cpp
// Enable Slave SH2 via SMPC
namespace SMPC {
    inline auto& COMREG = *reinterpret_cast<volatile uint8_t*>(0x20100001);
    inline auto& SF     = *reinterpret_cast<volatile uint8_t*>(0x20100063);

    void wait() { while (SF & 0x01); }
    void slave_on()  { wait(); COMREG = 0x02; wait(); }
    void slave_off() { wait(); COMREG = 0x03; wait(); }
}

// Job structure (in WRAM-L so both SH2s can see via cache-through)
struct alignas(16) SlaveJob {
    void (*func)(void*);
    void* arg;
    volatile int32_t done;
    uint32_t _pad;
};

// Place in WRAM-L and access via cache-through address (|0x20000000)
static SlaveJob _sjob __attribute__((section(".wram_l")));
#define SJOB (*(reinterpret_cast<volatile SlaveJob*>( \
               reinterpret_cast<uint32_t>(&_sjob) | 0x20000000u)))

// Slave SH2 main loop (must be in binary — called by slave startup)
extern "C" [[noreturn]] void slave_main() {
    // Mask interrupts on slave
    asm volatile("ldc %0, sr" :: "r"(0xF0));
    for (;;) {
        if (SJOB.func) {
            auto f = SJOB.func;
            auto a = SJOB.arg;
            SJOB.func = nullptr;
            f(a);
            SJOB.done = 1;
        }
    }
}

// Trigger job on slave and wait
void slave_run_sync(void (*f)(void*), void* arg) {
    SJOB.done = 0;
    SJOB.arg  = arg;
    SJOB.func = f; // Slave sees this and executes
    while (!SJOB.done);
}
```

---

## 10. Fixed-Point Math (Never Float!)

The SH2 has no FPU. Any `float` becomes a software library call (~50× slower).

```cpp
// math/fixed.hpp
using fx16 = int32_t; // 16.16 fixed-point: 1.0 = 0x00010000
using fx32 = int64_t; // Intermediate for multiplications

constexpr fx16 FX_ONE = 0x00010000;
constexpr fx16 fx_from_float(float f) { return static_cast<fx16>(f * 65536.0f); }
constexpr fx16 fx_int(int n)          { return n << 16; }
constexpr int  fx_toint(fx16 f)       { return f >> 16; }

// Multiplication: (a * b) >> 16
inline fx16 fx_mul(fx16 a, fx16 b) {
    return static_cast<fx16>((static_cast<fx32>(a) * b) >> 16);
}

// Division using SH2 hardware divider (36 cycles, pipelined!)
namespace SH2Div {
    inline auto& DVSR   = *reinterpret_cast<volatile int32_t*>(0xFFFFFF00);
    inline auto& DVDNT  = *reinterpret_cast<volatile int32_t*>(0xFFFFFF04);
    inline auto& DVDNTH = *reinterpret_cast<volatile int32_t*>(0xFFFFFF10);
}

// Start division — read result 36+ cycles later
inline void fx_div_start(fx16 a, fx16 b) {
    SH2Div::DVSR   = b;
    SH2Div::DVDNTH = a >> 16;
    SH2Div::DVDNT  = a << 16;
    // Do other calculations here while HW divides...
}
inline fx16 fx_div_read() { return static_cast<fx16>(SH2Div::DVDNT); }

// Vec3 and Mat4 → references/math_3d.md
```

---

## 11. Complete 3D Pipeline

```
[Model verts, fixed16] ──► [Slave SH2: MVP matrix × vertex]
                                    │
                           [Master: frustum cull]
                                    │
                           [Perspective projection]
                                    │
                           [Back-face culling (CCW 2D cross)]
                                    │
                           [Z-sort: insertion sort by avg_z]
                                    │
                           [Build Vdp1Cmd[] in WRAM-H]
                                    │
                           [SCU DMA → VDP1 VRAM]
                                    │
                           [Trigger VDP1]
```

```cpp
// Perspective projection (no float!)
constexpr fx16 FOCAL = fx_from_float(200.f);
constexpr int  CX = 160, CY = 112;

struct Screen { int16_t x, y; };

Screen project(fx16 vx, fx16 vy, fx16 vz) {
    if (vz < fx_int(1)) vz = fx_int(1); // Clip near

    // Start two divisions in sequence, reading between them for pipelining
    fx_div_start(fx_mul(vx, FOCAL), vz);
    // 36 cycles latency — use to calculate vy*focal before reading:
    fx16 ynum = fx_mul(vy, FOCAL);
    fx16 sx = fx_div_read();
    fx_div_start(ynum, vz);
    fx16 sy = fx_div_read();

    return {
        static_cast<int16_t>(fx_toint(sx) + CX),
        static_cast<int16_t>(fx_toint(sy) + CY)
    };
}

// Back-face cull (2D cross product, discard clockwise)
inline bool is_backface(Screen a, Screen b, Screen c) {
    return (int32_t(b.x - a.x) * (c.y - a.y)
          - int32_t(b.y - a.y) * (c.x - a.x)) <= 0;
}
```

---

## 12. SCU DMA — Transfers Without CPU

```cpp
namespace SCUDMA {
    // Channel 0 (highest priority — use for cmd list → VDP1 VRAM)
    inline auto& D0R  = *reinterpret_cast<volatile uint32_t*>(0x05A00000);
    inline auto& D0W  = *reinterpret_cast<volatile uint32_t*>(0x05A00004);
    inline auto& D0C  = *reinterpret_cast<volatile uint32_t*>(0x05A00008);
    inline auto& D0AD = *reinterpret_cast<volatile uint32_t*>(0x05A0000C);
    inline auto& D0EN = *reinterpret_cast<volatile uint32_t*>(0x05A00010);
    inline auto& D0MD = *reinterpret_cast<volatile uint32_t*>(0x05A00014);

    // Copy src (WRAM-L!) → destination, without CPU
    // WARNING: src MUST be in WRAM-L (0x002xxxxx). WRAM-H is not accessible by SCU DMA!
    void transfer(uint32_t src_wram_l, uint32_t dst, uint32_t bytes) {
        D0R  = src_wram_l;
        D0W  = dst;
        D0C  = bytes;
        D0AD = 0x00000101; // src +4, dst +4
        D0MD = 0x00000000; // Direct mode
        D0EN = 0x01000001; // Start
        while (D0EN & 0x01000000); // Poll until done
    }

    // Shortcut: cmd list from WRAM-L to VDP1 VRAM
    void cmd_to_vdp1(uint32_t src_wram_l, uint32_t cmd_count) {
        transfer(src_wram_l, 0x05C80000, cmd_count * 32);
    }
}
```

> **Important:** SCU DMA only accesses WRAM-L (0x002xxxxx). For DMA of data in WRAM-H, first copy to WRAM-L, then trigger DMA.

---

## 13. SCU DSP — Batch Transforms

The SCU DSP is assembly-only (VLIW, 6 ops/cycle). Ideal for batch matrix multiplication.

Complete details, instruction set, and examples → `references/scu_dsp.md`

Basic usage:
```cpp
namespace SCUDSP {
    inline auto& PPAF  = *reinterpret_cast<volatile uint32_t*>(0x05A00000); // DSP ctrl
    // DSP program at 0x05FF8000 (1KB)
    // Data RAM CT0-CT3 at 0x05FF8400 (1KB per CT, 64 words ring buffer)

    void execute_from_pc0() {
        PPAF = 0x00000101; // Execute from PC=0
    }
    void wait_done() {
        while (PPAF & 0x00010000); // Wait for EXEC flag
    }
}
```

---

## 14. SMPC — Controller Reading

```cpp
// SMPC addresses (access as cache-through: | 0x20000000)
#define SMPC_IREG(n)  (*reinterpret_cast<volatile uint8_t*>(0x20100001 + (n)*4))
#define SMPC_OREG(n)  (*reinterpret_cast<volatile uint8_t*>(0x20100021 + (n)*4))
#define SMPC_COMREG   (*reinterpret_cast<volatile uint8_t*>(0x20100001))
#define SMPC_SF       (*reinterpret_cast<volatile uint8_t*>(0x20100063))

enum PadBtn : uint16_t {
    BTN_A = 1<<2, BTN_B = 1<<4, BTN_C = 1<<5,
    BTN_X = 1<<6, BTN_Y = 1<<7, BTN_Z = 1<<8,
    BTN_UP = 1<<12, BTN_DOWN = 1<<13, BTN_LEFT = 1<<14, BTN_RIGHT = 1<<15,
    BTN_START = 1<<3, BTN_L = 1<<9, BTN_R = 1<<10
};

struct PadState { uint16_t held, pressed, released; };

// SMPC returns data in OREG after GETPERIPHERAL command
// Execution must occur during VBlank (16ms window)
uint16_t pad_read_raw() {
    while (SMPC_SF & 0x01);        // Wait for SMPC idle
    SMPC_COMREG = 0x08;            // Command: INTBACK (poll peripherals)
    while (SMPC_SF & 0x01);        // Wait for completion
    // OREG[0] and OREG[1] contain pad 1 data in digital format
    uint8_t hi = SMPC_OREG(0);
    uint8_t lo = SMPC_OREG(1);
    return static_cast<uint16_t>((hi << 8) | lo) ^ 0xFFFF; // Invert: 1=pressed
}
```

---

## 15. Audio — SH2 ↔ M68k Communication

```cpp
// Communication area in Sound RAM (accessible by both)
#define SCOMM_BASE 0x05A01000

namespace Audio {
    inline auto& CMD  = *reinterpret_cast<volatile uint16_t*>(SCOMM_BASE + 0x00);
    inline auto& CH   = *reinterpret_cast<volatile uint16_t*>(SCOMM_BASE + 0x02);
    inline auto& ADRL = *reinterpret_cast<volatile uint16_t*>(SCOMM_BASE + 0x04);
    inline auto& ADRH = *reinterpret_cast<volatile uint16_t*>(SCOMM_BASE + 0x06);
    inline auto& LENL = *reinterpret_cast<volatile uint16_t*>(SCOMM_BASE + 0x08);
    inline auto& LENH = *reinterpret_cast<volatile uint16_t*>(SCOMM_BASE + 0x0A);
    inline auto& FREQ = *reinterpret_cast<volatile uint16_t*>(SCOMM_BASE + 0x0C);

    // Wait for M68k to process previous command
    void wait() { while (CMD != 0); }

    void play_pcm(uint8_t ch, uint32_t addr, uint32_t len, uint16_t freq) {
        wait();
        CH   = ch;
        ADRL = static_cast<uint16_t>(addr & 0xFFFF);
        ADRH = static_cast<uint16_t>(addr >> 16);
        LENL = static_cast<uint16_t>(len & 0xFFFF);
        LENH = static_cast<uint16_t>(len >> 16);
        FREQ = freq;
        CMD  = 1; // M68k executes when it sees CMD != 0
    }

    void stop(uint8_t ch) {
        wait();
        CH  = ch;
        CMD = 3;
    }
}
```

M68k driver and SCSP register map → `references/scsp_audio.md`

---

## 16. Recommended Project Structure

```
my-game/
├── build-toolchain.sh       ← Compile sh2eb-elf-gcc from scratch
├── Makefile
├── crt0.s                   ← Startup assembly
├── saturn.ld                ← Linker script
├── ip.bin                   ← IP.BIN (from SBL sample/sys)
├── hal/
│   ├── vdp1.hpp             ← Direct VDP1 access
│   ├── vdp2.hpp             ← Direct VDP2 access
│   ├── scu.hpp              ← SCU DMA + interrupts
│   ├── scu_dsp.hpp/.s       ← SCU DSP programs
│   ├── sh2_slave.hpp        ← Slave SH2 job dispatcher
│   ├── smpc.hpp             ← Controllers + system control
│   └── audio.hpp            ← SH2-side audio interface
├── math/
│   ├── fixed.hpp            ← fixed16: mul, div, sincos
│   ├── vec3.hpp             ← Vec3 operations
│   └── mat4.hpp             ← Mat4 × Vec3 (uses SH2 MAC.L inline asm)
├── gfx/
│   ├── render3d.hpp/.cpp    ← Complete 3D pipeline
│   ├── zsort.hpp            ← Painter's algorithm
│   ├── tex_cache.hpp        ← Texture upload → VDP1 VRAM
│   └── sprite2d.hpp         ← Simple 2D sprites
├── audio/
│   ├── m68k_driver.s        ← M68k assembly driver (68000)
│   └── audio_api.hpp        ← SH2-side API
├── src/
│   └── main.cpp             ← Game entry point
└── tools/
    ├── make_iso.sh          ← mkisofs wrapper
    └── palette_conv.py      ← Convert images → Saturn format
```

---

## 17. Critical Pitfalls

| Error | Cause | Solution |
|-------|-------|---------|
| Stale data between SH2s | Cache coherency | Use addresses `|0x20000000` for shared data |
| Corrupted texture | Width not multiple of 8 | VDP1 requires: width % 8 == 0 |
| SCU DMA not working | Source in WRAM-H | SCU DMA only reads WRAM-L (0x002xxxxx) |
| Terrible performance | Float usage | Never use `float`; use `fx16` (fixed16) |
| Crash on slave SH2 | Bus contention | Use cache-through and avoid simultaneous WRAM-L access |
| VDP1 not drawing | Cmd list missing End | Always end with ctrl = 0x8000 |
| Big-endian reversed | Cross-compiling from x86 | All shorts/longs are big-endian in binary |
| IP.BIN not jumping | Wrong entry point | Confirm 0x06004000 is in linker script |

---

## 18. Emulators for Testing

```bash
# Mednafen — more accurate for exact timing
mednafen -ss.bios_path bios.bin game.cue

# Kronos — better for debug (based on Yabause, more active)
# https://github.com/FCare/Kronos

# Yabause — simpler alternative
yabause -b bios.bin -i game.iso
```

Image build:
```bash
# Generate bootable ISO (requires mkisofs/genisoimage)
mkisofs \
  -sysid "SEGA SATURN" \
  -volid "MYGAME" \
  -publisher "STUDIO" \
  -l -iso-level 1 \
  -o game.iso \
  -x game.iso \
  ip.bin \
  iso_root/

# ip.bin must be the FIRST file (IP.BIN = Track1 sectors 0-15)
# Game binary must be the first file in iso_root/
```

---

## 19. Reference Files

Read when you need details on a specific subsystem:

- `references/memory_map.md` — Complete memory map with bus speeds
- `references/scu_dsp.md` — SCU DSP instruction set, VLIW rules, examples
- `references/vdp1_cmds.md` — All VDP1 command table fields
- `references/vdp2_regs.md` — VDP2 register map, scroll planes, RBG0
- `references/math_3d.md` — Vectors, 4×4 matrices, sin/cos, MAC.L inline asm
- `references/scsp_audio.md` — SCSP registers, M68k assembly driver, SH2 API
