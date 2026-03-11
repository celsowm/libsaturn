---
name: saturn-boot
description: >
  Complete reference for the Sega Saturn BIOS boot process, IP.BIN format,
  disc layout, memory map, and region/peripheral codes required to build
  bootable Saturn discs and development libraries in SH-2 assembly.
  Use this skill whenever working on: Saturn homebrew disc creation, IP.BIN
  construction, bootloader code, SH-2 startup stubs, BIOS handoff sequences,
  disc image tooling (mkisofs/cdrecord for Saturn), memory map initialization,
  stack setup for master/slave SH-2, or any task touching how the Saturn
  BIOS finds, authenticates, and launches game code from a CD-ROM.
---

# Sega Saturn Boot & Disc Format Reference

> Detailed tables → see `references/` directory.  
> Read **all sections** of this file first; load a reference only when you
> need the full byte-level detail for that specific topic.

---

## 1. Boot Sequence Overview

```
POWER ON
   │
   ▼
BIOS ROM (0x00000000)
   │  Master SH-2 begins executing
   │  Hardware init: SCU, VDP1, VDP2, SCSP, SMPC
   │
   ▼
CD-ROM SUBSYSTEM CHECK
   │  BIOS polls HIRQ/CDREG registers via A-Bus CS2
   │  Waits for disc to spin up, reads TOC
   │
   ▼
READ SYSTEM AREA (sectors 0–15, 16 sectors × 2048 bytes = 32 KB)
   │  Loaded from LBA 0 of the data track
   │  Destination: 0x06002000 (internal BIOS buffer)
   │
   ▼
VALIDATE IP HEADER
   │  Checks magic string at offset 0x000: "SEGA SEGASATURN "
   │  Checks area symbols match console region
   │  Checks device info byte
   │
   ▼
AUTHENTICATE SECURITY CODE (offset 0x100–0x5FF in IP area)
   │  BIOS runs its own authentication routine against the
   │  embedded security block. Legitimate discs also have
   │  a physical "security ring" on the inner hub — the BIOS
   │  attempts to read sectors in that region; on pressed discs
   │  the wobble/pit pattern passes, on CD-R it typically fails
   │  unless a mod-chip or swap trick bypasses this step.
   │
   ▼
COPY IP BOOT CODE to IP Load Address (from header field 0x0D0)
   │  Default load address: 0x06004000
   │  Size field at 0x0D4 (in bytes, must be multiple of 2048)
   │
   ▼
SET UP STACKS & JUMP TO IP
   │  Master Stack → header field 0x0D8  (default 0x06002000)
   │  Slave  Stack → header field 0x0DC  (default 0x06002000)
   │  BIOS sets SP register and jumps to IP Load Address
   │
   ▼
IP (INITIAL PROGRAM) EXECUTES
   │  Responsible for: further hardware init, loading SP (Security
   │  Program) if present, then loading the first read file from
   │  the ISO filesystem.
   │  First Read Address → 0x0E0  (e.g. 0x06010000)
   │  First Read Size    → 0x0E4  (bytes)
   │
   ▼
FIRST READ FILE (main ELF / binary) LOADED & EXECUTED
   Game code runs.
```

---

## 2. Memory Map

| Address Range             | Size   | Description                        |
|---------------------------|--------|------------------------------------|
| `0x00000000–0x000FFFFF`   | 1 MB   | **BIOS ROM** (read-only)           |
| `0x00100000–0x0010FFFF`   | 64 KB  | SMPC (System Mgmt & Periph Ctrl)   |
| `0x00180000–0x0018FFFF`   | 64 KB  | Backup SRAM (battery-backed)       |
| `0x00200000–0x003FFFFF`   | 256 KB | **Low Work RAM** (LWRAM)           |
| `0x02000000–0x023FFFFF`   | 4 MB   | A-Bus CS2 (CD-ROM registers etc.)  |
| `0x05900000–0x059FFFFF`   | 1 MB   | A-Bus CS3 (cart slot)             |
| `0x05A00000–0x05AFFFFF`   | 1 MB   | SCU registers                      |
| `0x05C00000–0x05C7FFFF`   | 512 KB | VDP1 VRAM                          |
| `0x05C80000–0x05CFFFFF`   | 512 KB | VDP1 Framebuffer                   |
| `0x05D00000–0x05D0001F`   | 32 B   | VDP1 Registers                     |
| `0x05E00000–0x05EFFFFF`   | 1 MB   | VDP2 VRAM                          |
| `0x05F00000–0x05F7FFFF`   | 512 KB | VDP2 Color RAM (CRAM)              |
| `0x05F80000–0x05F9FFFF`   | 128 KB | VDP2 Registers                     |
| `0x05FE0000–0x05FEFFFF`   | 64 KB  | SCU registers (mirror)             |
| `0x05FF0000–0x05FFFFFF`   | 64 KB  | Master SH-2 internal registers     |
| `0x06000000–0x060FFFFF`   | 1 MB   | **High Work RAM** (HWRAM)          |

### Key Addresses for Boot Code

```
0x06002000  BIOS internal buffer / Master stack default top
0x06004000  Default IP load address (entry point after BIOS handoff)
0x06010000  Typical first-read (game binary) load address
0x060FFFF0  Top of HWRAM (keep stacks away from this)
```

> **SH-2 reset vector**: address `0x00000000` (BIOS ROM). The SH-2
> reads the initial SP from `[VBR+0]` and initial PC from `[VBR+4]`.
> At cold boot, VBR = 0x00000000, so PC starts at `[0x00000004]`.

---

## 3. Disc Layout

```
LBA  0  ┌──────────────────────────────────┐
         │  Sector 0   IP.BIN header        │  ← 256-byte header
         │  Sector 0   Security code start  │  ← offsets 0x100–0x5FF
         │  Sectors 0–15  System Area       │  32 KB total (IP area)
LBA 16  ├──────────────────────────────────┤
         │  Primary Volume Descriptor       │  ISO 9660 PVD
LBA 17  ├──────────────────────────────────┤
         │  Volume Descriptor Set Terminator│
LBA 18  ├──────────────────────────────────┤
         │  ISO 9660 Path Table (LE)        │
         │  ISO 9660 Path Table (BE)        │
         │  Root Directory Record           │
         │  Files ...                       │
         └──────────────────────────────────┘
```

### Sector Format

- **Mode 1, 2048-byte user data** (standard data CD)
- Raw sector: 2352 bytes (sync + header + 2048 data + ECC/EDC)
- Tools like `mkisofs` with `-sysarea ip.bin` and `-V "TITLE"` handle
  injecting IP.BIN into sectors 0–15 automatically.

### Critical mkisofs Flags for Saturn

```bash
mkisofs \
  -sysarea ip.bin \        # injects IP.BIN into system area (sectors 0-15)
  -V "GAME_TITLE     " \   # volume label, pad to 32 chars with spaces
  -G ip.bin \              # boot record (same file)
  -joliet \                # optional, Saturn doesn't use but harmless
  -l \                     # allow 31-char filenames
  -o disc.iso \
  ./cd_root/
```

---

## 4. IP.BIN Header Format (Bytes 0x000–0x0FF)

For the complete byte-by-byte table → read `references/ip-header-format.md`

### Quick Reference (critical fields)

| Offset | Size | Field                  | Example / Notes                            |
|--------|------|------------------------|--------------------------------------------|
| 0x000  | 16   | Hardware Identifier    | `"SEGA SEGASATURN "` ← **exact, required** |
| 0x010  | 16   | Maker ID               | `"SEGA ENTERPRISES"` or licensed T-code    |
| 0x020  | 10   | Product Number         | `"GS-9000   "`                             |
| 0x02A  | 6    | Version                | `"V1.000"`                                 |
| 0x030  | 8    | Release Date           | `"19950101"` (YYYYMMDD)                    |
| 0x038  | 4    | Device Information     | `0x00CD 0x0001` → CD-ROM, 1 disc           |
| 0x03C  | 10   | Area Symbols           | `"JUE       "` → see §6                    |
| 0x046  | 6    | Peripheral Support     | `"JKLAMTPRS"` → see §7                     |
| 0x04C  | 4    | (reserved)             | fill with spaces `0x20`                    |
| 0x050  | 112  | Game Title             | padded with spaces to 112 bytes            |
| 0x0C0  | 16   | (reserved)             | fill with `0x00`                           |
| 0x0D0  | 4    | IP Load Address        | `0x06004000` (big-endian)                  |
| 0x0D4  | 4    | IP Size                | bytes, multiple of 2048                    |
| 0x0D8  | 4    | Master Stack Address   | `0x06002000`                               |
| 0x0DC  | 4    | Slave  Stack Address   | `0x06000000` (or same as master)           |
| 0x0E0  | 4    | First Read Address     | `0x06010000`                               |
| 0x0E4  | 4    | First Read Size        | bytes of first binary to load              |
| 0x0E8  | 24   | (reserved)             | fill with `0x00`                           |

**Bytes 0x100–0x5FF**: Security Code block (1280 bytes).  
**Bytes 0x600 onward**: Actual IP executable code (SH-2 instructions).

---

## 5. IP Boot Code Entry Point (SH-2 Assembly)

After the BIOS jumps to `IP Load Address` (default `0x06004000`), the
SH-2 master CPU executes your IP code. Minimum viable IP stub:

```asm
; ip_boot.s  —  Minimal IP for Sega Saturn
; Assembled with: sh-elf-as / sh-elf-gcc or custom SH-2 assembler
; Load address: 0x06004000

.section .text
.global _ip_start

_ip_start:
    ; Disable interrupts
    mov     #0xF0, r0
    ldc     r0, sr

    ; Set up master stack pointer
    mov.l   master_stack, r15

    ; Clear BSS (if any)
    mov.l   bss_start_addr, r4
    mov.l   bss_end_addr,   r5
    mov     #0, r6
_clear_bss:
    cmp/ge  r5, r4
    bt      _bss_done
    mov.b   r6, @r4
    add     #1, r4
    bra     _clear_bss
    nop
_bss_done:

    ; Jump to C main (or your ASM main)
    mov.l   main_addr, r0
    jsr     @r0
    nop

    ; Should never return — loop forever
_hang:
    bra     _hang
    nop

.align 4
master_stack:   .long 0x06002000
main_addr:      .long 0x06010000   ; address of first-read binary
bss_start_addr: .long _bss_start
bss_end_addr:   .long _bss_end
```

---

## 6. Area Symbols (Region Codes)

The 10-byte field at offset `0x03C` controls which consoles will boot
the disc. Each position is either the region letter or a space (`0x20`).

| Position | Region         | Letter |
|----------|----------------|--------|
| 0        | Japan          | `J`    |
| 1        | Asia (NTSC)    | `T`    |
| 2        | (reserved)     | space  |
| 3        | (reserved)     | space  |
| 4        | USA / Canada   | `U`    |
| 5        | Brazil         | `B`    |
| 6        | (reserved)     | space  |
| 7        | Europe / Pal   | `E`    |
| 8        | (reserved)     | space  |
| 9        | (reserved)     | space  |

**Multi-region example**: `"JUE       "` → boots in Japan, USA, Europe.  
**Dev/homebrew**: use `"JUET      "` or all regions to maximize compatibility.

---

## 7. Peripheral Support Codes

The 6-byte field at `0x046`. Each present peripheral gets its letter;
unused positions are space-padded.

| Code | Peripheral                         |
|------|------------------------------------|
| `J`  | Standard Joystick                  |
| `G`  | Gamepad (digital, 3/6-button)      |
| `K`  | Keyboard                           |
| `M`  | Mouse                              |
| `S`  | Analog Steering Controller         |
| `T`  | Multitap                           |
| `A`  | Analog Joystick                    |
| `L`  | Light Gun (Stunner)                |
| `P`  | Printer                            |
| `R`  | RAM Cartridge                      |
| `C`  | CD-ROM (always include)            |
| `F`  | Floppy (rare)                      |

Typical value: `"JKMT  "` (Joystick, Keyboard, Mouse, Multitap).

---

## 8. Device Information Field (0x038, 4 bytes)

```
Byte 0x038: High byte — disc type
  0x00 = CD-ROM (pressed)
  0x80 = CD-R  (burned)
  0x40 = DVD   (rare)

Byte 0x039: Low byte — number of discs in set (1–9)

Bytes 0x03A–0x03B: Reserved, set to 0x00
```

Example: `0x00 0x01 0x00 0x00` → pressed CD-ROM, single disc.

---

## 9. SH-2 Specifics for Boot Context

### Register State at BIOS Handoff (IP entry)

| Register | Value at Entry       | Notes                              |
|----------|----------------------|------------------------------------|
| `PC`     | IP Load Address      | 0x06004000 default                 |
| `R15`    | Master Stack Addr    | from IP header 0x0D8               |
| `SR`     | 0x000000F0           | Interrupts masked (I=15)           |
| `VBR`    | 0x00000000           | Points to BIOS vectors initially   |
| `GBR`    | undefined            | set before using GBR-relative ops  |
| `MACH`   | undefined            |                                    |

**First thing your IP should do**: set `SR` to mask interrupts, set
up your own `VBR`, initialize `SP (R15)`.

### Setting your own VBR

```asm
    mov.l   vbr_table, r0
    ldc     r0, vbr        ; relocate vector base register
    ...
vbr_table:  .long 0x06004000   ; your exception vector table starts here
```

The SH-2 vector table layout (at VBR):

| Offset | Vector               |
|--------|----------------------|
| +0x00  | Power-on SP (unused after boot) |
| +0x04  | Power-on PC (unused after boot) |
| +0x08  | Reserved             |
| +0x0C  | Reserved             |
| +0x10  | General Illegal Instr|
| +0x18  | Slot Illegal Instr   |
| +0x20  | CPU Address Error    |
| +0x24  | DMA Address Error    |
| +0x28  | NMI                  |
| +0x2C  | User Break           |
| +0x40–0x5F | Traps (TRAPA #0–#31) |
| +0x60–0xFF | External interrupts |

---

## 10. Building a Minimal Bootable Image (Step-by-Step)

```
1. Write IP.BIN
   ├─ Build 256-byte header (fields above)
   ├─ Append 1280-byte security block (copy from SDK or open-source)
   └─ Append your SH-2 IP boot stub (compiled, position-dependent)
      Total must be multiple of 2048 bytes (pad with 0x00)

2. Build CD filesystem root (./cdroot/)
   └─ Place FIRST.BIN (your main binary) here

3. Create ISO
   mkisofs -sysarea ip.bin -G ip.bin -V "MY_GAME        " \
           -l -o game.iso ./cdroot/

4. Burn / Test in emulator
   cdrecord -v speed=4 dev=/dev/sr0 game.iso
   (or load game.iso in Mednafen/SSF/Yaba Sanshiro)
```

---

## 11. Common Pitfalls

| Mistake | Effect | Fix |
|---------|--------|-----|
| Magic string wrong/missing | BIOS shows "disc unsuitable" screen | Must be exactly `"SEGA SEGASATURN "` (16 bytes, trailing space) |
| IP size not multiple of 2048 | BIOS loads garbage | Pad IP.BIN to next 2048-byte boundary |
| Stack pointers overlap code | Corruption at runtime | Set master stack ≥ 0x06002000, code at ≥ 0x06004000 |
| Interrupts enabled too early | Random crashes during init | Keep SR I-bits = 0xF until init is complete |
| VBR not relocated | BIOS exception vectors still active | `ldc r0, vbr` to your own table before enabling IRQs |
| First Read size = 0 | Nothing loaded | Must match actual binary size in bytes |
| Area symbols empty | Region-locked consoles reject disc | Include `J`, `U`, or `E` as needed |
| BE vs LE confusion | Load addresses corrupted | All multi-byte fields in IP header are **big-endian** |

---

## Reference Files

- **`references/ip-header-format.md`** — Full byte-by-byte IP header table
  with binary encoding notes and worked example.
- **`references/memory-map.md`** — Expanded memory map with sub-regions
  (SCU, VDP1/2, SCSP, SH-2 internal), bus widths, and access timing notes.
- **`references/disc-layout.md`** — Detailed CD-ROM sector layout, TOC
  format, track types, multi-session notes, and audio track handling.
