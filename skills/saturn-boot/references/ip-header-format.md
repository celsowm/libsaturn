# IP.BIN Header Format — Complete Reference

Full 256-byte header structure for the Sega Saturn Initial Program area.
All multi-byte integer fields are **big-endian**.

---

## Byte Map (0x000–0x0FF)

```
Offset  Len  Field                   Content / Encoding
──────────────────────────────────────────────────────────────────────
0x000    16  Hardware Identifier     ASCII, must be "SEGA SEGASATURN "
                                     (note trailing space, no null)

0x010    16  Maker ID                ASCII, padded with spaces (0x20)
                                     Official Sega:  "SEGA ENTERPRISES"
                                     Licensed T-xxx: "SEGA TP T-XXXXXX"
                                     Homebrew use:   "SEGA ENTERPRISES" or
                                                     any 16-byte ASCII string
                                                     (BIOS does not verify)

0x020    10  Product Number          ASCII, e.g. "GS-9000   "
                                     Format: XX-NNNN where XX = 2-char
                                     publisher code, NNNN = number.
                                     Pad with spaces to 10 bytes.

0x02A     6  Version                 ASCII, e.g. "V1.000"
                                     Format: V + major.minor (3 decimal
                                     digits). Exactly 6 bytes.

0x030     8  Release Date            ASCII YYYYMMDD, e.g. "19950101"
                                     Month and day are zero-padded.

0x038     1  Disc Type               0x00 = CD-ROM (pressed)
                                     0x80 = CD-R
                                     0x40 = DVD (rare, unofficial)

0x039     1  Disc Count              Number of discs in set: 0x01–0x09
                                     Single-disc games: 0x01

0x03A     2  (reserved)              Set to 0x00 0x00

0x03C    10  Area Symbols            One ASCII char per region slot:
                                     Slot 0: J = Japan
                                     Slot 1: T = Asia (NTSC, Taiwan/Korea)
                                     Slot 2: (reserved) → space
                                     Slot 3: (reserved) → space
                                     Slot 4: U = USA / Canada
                                     Slot 5: B = Brazil
                                     Slot 6: (reserved) → space
                                     Slot 7: E = Europe / PAL / Oceania
                                     Slot 8: (reserved) → space
                                     Slot 9: (reserved) → space
                                     Space (0x20) = region not supported.
                                     Example all-region: "JT  UB E  "

0x046     6  Peripheral Support      One ASCII char per supported device.
                                     Positions 0–5, space-padded.
                                     See peripheral code table below.

0x04C     4  (reserved)              Fill with 0x20 (spaces)

0x050   112  Game Title              ASCII, space-padded to 112 bytes.
                                     Max 112 chars; BIOS may display first
                                     32 chars in some error screens.
                                     Do NOT null-terminate; pad with 0x20.

0x0C0    16  (reserved)              Fill with 0x00

0x0D0     4  IP Load Address         uint32 BE. Address in HWRAM where
                                     BIOS will copy the IP executable.
                                     Default / recommended: 0x06004000.
                                     Must be 4-byte aligned.

0x0D4     4  IP Size                 uint32 BE. Size of IP executable
                                     in bytes (NOT including the 256-byte
                                     header and 1280-byte security block).
                                     Must be a multiple of 2048.

0x0D8     4  Master Stack Address    uint32 BE. Initial SP for master SH-2.
                                     Default: 0x06002000 (grows downward).
                                     Must not overlap IP code region.

0x0DC     4  Slave Stack Address     uint32 BE. Initial SP for slave SH-2.
                                     Default: 0x06000000 or 0x06002000.
                                     Slave SH-2 is held in reset by BIOS;
                                     your code must release it explicitly.

0x0E0     4  First Read Address      uint32 BE. Address where BIOS/IP will
                                     load the "first read" file from the
                                     ISO filesystem.
                                     Typical: 0x06010000.

0x0E4     4  First Read Size         uint32 BE. Size in bytes of the first
                                     binary to load. Must match the actual
                                     file size on disc. 0 = nothing loaded.

0x0E8    24  (reserved)              Fill with 0x00
──────────────────────────────────────────────────────────────────────
0x100         *** End of 256-byte header ***
```

---

## Security Block (0x100–0x5FF, 1280 bytes)

The BIOS authentication routine reads bytes from this region and
verifies them against an internal checksum/key. The exact algorithm
is proprietary and has never been officially published.

**Practical options for homebrew / dev:**

1. **Copy from a real IP.BIN** (extracted from a licensed disc).
   Many open-source Saturn tools ship `ip.bin` files freely usable
   for non-commercial dev. The BIOS checks this block but most
   emulators skip or relax the check.

2. **Use the open-source `BOOT_IP.BIN`** from Jo Engine, Yaul, or
   KallistiOS Saturn ports — these contain a security block derived
   from Sega's SDK samples that passes BIOS checks.

3. **Hardware**: A mod-chip or fenrir ODE bypasses the check entirely,
   so any security block (even all-zeros) will work on modded hardware.

> **Do NOT distribute copyrighted security blocks** from commercial games.

---

## Executable Code Region (0x600 onward)

Actual SH-2 machine code begins immediately after the security block.

```
0x000 ─── 256 bytes  ─── IP header
0x100 ─── 1280 bytes ─── Security block
0x600 ─── N bytes    ─── SH-2 IP executable (your boot code)
          (pad total file to multiple of 2048)
```

The BIOS copies the **entire IP.BIN** (header + security + code) to
the IP Load Address and then jumps to that address. So your code at
file offset `0x600` lands at `IP_LOAD_ADDR + 0x600` in RAM.

**Entry point offset trick**: Place a branch at the start of the
IP Load Address that skips over the header data:

```asm
; At file offset 0x000 (but this is just header data read by BIOS)
; BIOS copies everything and jumps to IP_LOAD_ADDR (0x06004000)
; So offset 0x000 = address 0x06004000 in RAM.
; The 256-byte header occupies 0x06004000–0x060400FF.
; Security block: 0x06004100–0x060445FF.
; Your code starts at: 0x06004600.
;
; Solution: At IP_LOAD_ADDR (very first word), place a jump to 0x06004600:

.org 0x06004000
    bra     ip_code_start    ; +4 byte branch delay slot
    nop
    ; ... header bytes 0x002–0x5FF are non-code, BIOS uses them ...
    
.org 0x06004600
ip_code_start:
    ; real boot code here
```

Alternatively, build `ip.bin` with the code starting at offset 0,
but set `IP Load Address` such that `addr + 0x600 = entry`.
KallistiOS and Jo Engine use this approach.

---

## Worked Binary Example (first 64 bytes)

```
Offset  Hex dump (16 bytes per row)       ASCII
0x000:  53 45 47 41 20 53 45 47 41 53 41 54 55 52 4E 20  SEGA SEGASATURN 
0x010:  53 45 47 41 20 45 4E 54 45 52 50 52 49 53 45 53  SEGA ENTERPRISES
0x020:  48 42 2D 30 30 30 31 20 20 20 56 31 2E 30 30 30  HB-0001   V1.000
0x030:  32 30 32 35 30 31 30 31 30 30 43 44 30 31 30 30  20250101 00CD0100  (*)
0x03C:  4A 54 20 20 55 42 20 45 20 20 4A 47 4D 54 20 20  JT  UB E  JGMT  
```

(*) Row 0x030: release date "20250101", then device info 0x00 (CD-ROM),
0x01 (1 disc), 0x00 0x00 (reserved), then area symbols start at 0x03C.

---

## Peripheral Code Table

| Code | Peripheral                               |
|------|------------------------------------------|
| `J`  | Standard 3-button Joystick               |
| `G`  | Standard Gamepad (3 or 6 button digital) |
| `A`  | Analog Joystick (Mission Stick)          |
| `M`  | Mouse                                    |
| `K`  | Keyboard (Saturn Keyboard)               |
| `S`  | Analog Steering Wheel                    |
| `T`  | Multitap (allows 4/6/8 controllers)      |
| `L`  | Light Gun (Virtua Gun / Stunner)         |
| `P`  | Printer                                  |
| `R`  | RAM/ROM Cartridge                        |
| `C`  | CD-ROM (include for all disc-based games)|
| `F`  | Floppy Disk Drive (rare)                 |
| `D`  | Data Link (Netlink modem)                |

Fill positions with `0x20` (space) if fewer than 6 peripherals listed.

---

## Minimum Viable IP.BIN Construction (pseudo-code)

```python
def build_ip_bin(title, area, first_read_addr, first_read_size,
                 security_block_bytes, ip_code_bytes):

    header = bytearray(256)

    # Hardware ID
    header[0x000:0x010] = b"SEGA SEGASATURN "

    # Maker ID
    header[0x010:0x020] = b"SEGA ENTERPRISES"

    # Product number (10 bytes)
    header[0x020:0x02A] = b"HB-0001   "

    # Version (6 bytes)
    header[0x02A:0x030] = b"V1.000"

    # Release date (8 bytes YYYYMMDD)
    header[0x030:0x038] = b"20250101"

    # Device info: CD-ROM, 1 disc
    header[0x038] = 0x00
    header[0x039] = 0x01

    # Area symbols (10 bytes)
    area_field = area.ljust(10)[:10].encode('ascii')
    header[0x03C:0x046] = area_field

    # Peripherals
    header[0x046:0x04C] = b"JGMT  "

    # Title (112 bytes)
    title_field = title.ljust(112)[:112].encode('ascii')
    header[0x050:0x0C0] = title_field

    # IP Load Address (BE)
    struct.pack_into('>I', header, 0x0D0, 0x06004000)

    # IP Size (size of ip_code_bytes, rounded up to 2048)
    ip_size = (len(ip_code_bytes) + 2047) & ~2047
    struct.pack_into('>I', header, 0x0D4, ip_size)

    # Master stack
    struct.pack_into('>I', header, 0x0D8, 0x06002000)

    # Slave stack
    struct.pack_into('>I', header, 0x0DC, 0x06000000)

    # First read address and size
    struct.pack_into('>I', header, 0x0E0, first_read_addr)
    struct.pack_into('>I', header, 0x0E4, first_read_size)

    # Assemble
    ip_code_padded = ip_code_bytes.ljust(ip_size, b'\x00')
    security_padded = security_block_bytes[:1280].ljust(1280, b'\x00')

    raw = header + security_padded + ip_code_padded

    # Pad total to multiple of 2048
    total = (len(raw) + 2047) & ~2047
    return raw.ljust(total, b'\x00')
```
