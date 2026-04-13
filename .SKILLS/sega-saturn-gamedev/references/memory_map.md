# Complete Memory Map — Sega Saturn

## SH2 Address Space (32-bit, Big-Endian)

```
Address           Size     Region                     Bus / Speed
────────────────────────────────────────────────────────────────────────
0x00000000        512 KB   Boot ROM (BIOS)            A-Bus CS0, ~70ns
0x00100000        4 KB     SMPC I/O Area
0x00180000        32 KB    Backup RAM (internal)      B-Bus
0x00200000        1 MB     WRAM-L (DRAM)              B-Bus, ~130ns
0x00300000        ...      (mirror of WRAM-L)
0x01000000        8 MB     A-Bus CS0 (cartridge)      A-Bus
0x01800000        8 MB     A-Bus CS1 (cartridge)
0x02000000        512 MB   A-Bus CS2 (CD-ROM block)
0x04000000        32 MB    A-Bus CS3 (external SDRAM)
0x05900000        4 KB     SSH2 On-Chip Registers
0x05A00000        4 KB     SCU Registers
0x05B00000        4 KB     SMPC Registers
0x05C00000        512 KB   VDP1 VRAM                  VDP1 Bus
0x05D00000        512 KB   VDP1 Registers (write only)
0x05E00000        512 KB   VDP2 VRAM                  VDP2 Bus
0x05F00000        4 KB     VDP2 Color RAM (4096 × 16b)
0x05F80000        4 KB     VDP2 Registers
0x05FE0000        ...      (system area)
0x05FF8000        1 KB     SCU DSP Program RAM
0x05FF8400        1 KB     SCU DSP Data RAM (CT0–CT3)
0x06000000        1 MB     WRAM-H (SDRAM)              SH2 Bus, ~70ns, cached
────────────────────────────────────────────────────────────────────────
0x20000000+       Mirror   Cache-Through (addr | 0x20000000)
                           Same space above but without SH2 cache
```

## Access Speeds (SH2 Cycles)

| Memory          | Direct Access | Via Cache | Notes                        |
|-----------------|:------------:|:---------:|------------------------------|
| WRAM-H (SDRAM)  | 2–3 cycles   | 1 cycle   | Code + hot data here         |
| WRAM-L (DRAM)   | 5–8 cycles   | N/A       | Source for SCU DMA           |
| Boot ROM        | 5 cycles     | 1 cycle   | Read-only, BIOS              |
| VDP1 VRAM       | 4–6 cycles   | N/A       | Direct writes during VBLANK  |
| VDP2 VRAM       | 4–6 cycles   | N/A       | Tiles, planes                |
| VDP1/VDP2 Regs  | 4–8 cycles   | N/A       | Heavy polling = stall        |
| SCU Registers   | 8+ cycles    | N/A       | DMA setup only               |
| SMPC Registers  | 32+ cycles   | N/A       | Only during VBlank           |

## Access Rules by Subsystem

### WRAM-H (0x06000000)
- **Master/Slave SH2:** full access (with and without cache)
- **SCU DMA:** ❌ NOT ACCESSIBLE as DMA source/destination!
- **Use:** code (.text), hot data, math tables, frame buffers

### WRAM-L (0x00200000)
- **Master/Slave SH2:** full access (slower, no efficient cache)
- **SCU DMA:** ✅ Source and destination of DMA
- **SCU DSP:** ✅ Read/write via DMA level 2
- **Use:** static meshes, DMA buffers, audio data, texture staging

### VDP1 VRAM (0x05C80000)
- **SH2:** direct write ✅ (but slow; prefer SCU DMA)
- **SCU DMA:** ✅ DMA destination (channel 0 or 1)
- **VDP1:** exclusive read during rasterization
- **Recommended layout:**
  ```
  0x05C80000 – 0x05C80FFF  Command list (4KB = ~128 commands)
  0x05C81000 – 0x05CFFFFF  Textures (remaining 512KB minus cmd area)
  ```
- **Mandatory alignment:** texture must be at address multiple of 8 bytes

### VDP2 VRAM (0x05E00000)
- Banks A (0x05E00000) and B (0x05E80000)
- **Recommended layout:**
  ```
  Bank A: Pattern name tables (tilemap)
  Bank B: Cell data (graphic tiles)
  ```

## SH2 Cache

Each SH2 has 4 KB of 4-way set-associative cache for instruction + data.

```
Cache line: 16 bytes (4 longs)
Cache hit:  1 cycle
Cache miss: 2-8 cycles (WRAM-H), 5-13 cycles (WRAM-L)
```

**Cache-Through (addr | 0x20000000):**
- Bypasses SH2 cache
- Writes go directly to memory
- Reads always fetch from memory
- **Required for data shared between Master and Slave SH2**
- Without cache-through → one SH2 reads stale data from cache while the other wrote

## SCU Registers (0x05A00000)

```
0x05A00000  D0R   DMA Ch0 Read Address
0x05A00004  D0W   DMA Ch0 Write Address
0x05A00008  D0C   DMA Ch0 Byte Count
0x05A0000C  D0AD  DMA Ch0 Address Add
0x05A00010  D0EN  DMA Ch0 Enable / Status
0x05A00014  D0MD  DMA Ch0 Mode
0x05A00020  D1R   DMA Ch1 Read Address
0x05A00024  D1W   DMA Ch1 Write Address
0x05A00028  D1C   DMA Ch1 Byte Count
0x05A0002C  D1AD  DMA Ch1 Address Add
0x05A00030  D1EN  DMA Ch1 Enable / Status
0x05A00034  D1MD  DMA Ch1 Mode
0x05A00040  D2R   DMA Ch2 Read Address (lowest priority, for DSP)
0x05A00044  D2W   DMA Ch2 Write Address
0x05A00048  D2C   DMA Ch2 Byte Count
0x05A0004C  D2AD  DMA Ch2 Address Add
0x05A00050  D2EN  DMA Ch2 Enable / Status
0x05A00054  D2MD  DMA Ch2 Mode
0x05A00060  DSTP  DMA Stop
0x05A00070  DSTA  DMA Status
0x05A0001C  IST   Interrupt Status
0x05A00020  AIACK Interrupt Acknowledge
0x05A00024  ASR0  Interrupt Mask (SCU)
0x05A00028  RSEL  Interrupt Route Select
0x05A0007C  VER   SCU Version
```

## D0AD — DMA Increment Control

```
Bits 9:8 = Destination increment: 00=+0, 01=+2, 10=+4, 11=+8? (see manual)
Bits 1:0 = Source increment:      00=+0, 01=+2, 10=+4, 11=invalid

Typical value to copy buffer: 0x00000101 (src+4, dst+4)
Value for broadcast:           0x00000100 (src+4, dst fixed)
```

## SCU Interrupt Vector (IST bits)

```
Bit  0: VBlank-IN   (most important — frame synchronization)
Bit  1: VBlank-OUT  (VDP1 finished rasterizing)
Bit  2: HBlank-IN
Bit  3: Timer 0
Bit  4: Timer 1
Bit  5: DSP End     (SCU DSP finished program)
Bit  6: Sound Request (M68k requested attention)
Bit  7: System Manager (SMPC)
Bit  8: Pad Interrupt
Bit  9–15: A-Bus (CD-ROM, cartridge)
```

## SH2 On-Chip Registers (Internal to SH2, accessible at 0xFFFFF000)

```
0xFFFFFF00  DVSR    Hardware Divider — Divisor
0xFFFFFF04  DVDNT   Dividend Low / Result
0xFFFFFF08  DVCR    Divider Control
0xFFFFFF0C  VCRDIV  Interrupt vector for divider
0xFFFFFF10  DVDNTH  Dividend High (start 32/32 division: write DVDNTH before DVDNT)
0xFFFFFF14  DVDNTL  (mirror)
0xFFFFFF40  DMAC0   Internal DMAC Ch0 (don't confuse with SCU DMA!)
0xFFFFFF80  ITU     Timer Unit (FRT - Free-Running Timer)
0xFFFFFE00  Cache Control Register
```
