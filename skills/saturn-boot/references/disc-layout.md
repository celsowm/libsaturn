# Sega Saturn — CD-ROM Disc Layout Reference

---

## Physical Disc Layers

```
┌─────────────────────────────────────────────────────┐
│  Lead-in area (TOC — Table of Contents)             │  ~4 500 sectors
├─────────────────────────────────────────────────────┤
│  Security Ring (inner data area, pressed discs only)│  ~500 sectors
│  BIOS reads this to verify the disc is pressed      │
│  CD-Rs lack the proper pit pattern → auth fails     │
├─────────────────────────────────────────────────────┤
│  Program Area (data track + optional audio tracks)  │
│    LBA 0–15     : System Area (IP.BIN)              │
│    LBA 16       : ISO 9660 PVD                      │
│    LBA 17       : Volume Set Terminator             │
│    LBA 18+      : Path tables, directory records,   │
│                   and all game files                │
└─────────────────────────────────────────────────────┘
│  Lead-out area                                      │
└─────────────────────────────────────────────────────┘
```

---

## Sector Structure (Mode 1 Data)

```
Byte offset  Size   Field
─────────────────────────────────────────────────────
0x000         12   Sync pattern: 00 FF FF FF FF FF FF FF FF FF FF 00
0x00C          3   Header: MM SS FF (minute, second, frame)
0x00F          1   Mode byte: 0x01 (Mode 1)
0x010       2048   User data  ← this is what LBA addressing refers to
0x810          4   EDC (Error Detection Code)
0x814          8   Zeroed (intermediate field)
0x81C        172   ECC P-parity
0x8C8        104   ECC Q-parity
─────────────────────────────────────────────────────
Total: 2352 bytes per raw sector
```

**LBA 0 = first sector after lead-in; user data starts at byte 0x010
of the raw sector.** Most tools work with the 2048-byte user-data view.

---

## System Area (LBA 0–15): Sector-by-Sector

| LBA | Content                                          |
|-----|--------------------------------------------------|
| 0   | IP.BIN bytes 0x0000–0x07FF (2048 bytes of IP)   |
| 1   | IP.BIN bytes 0x0800–0x0FFF                       |
| 2   | IP.BIN bytes 0x1000–0x17FF                       |
| 3   | IP.BIN bytes 0x1800–0x1FFF                       |
| … | … continuing IP.BIN contents …                  |
| 15  | IP.BIN bytes 0x7800–0x7FFF (last sector of IP)  |

**Total system area: 16 × 2048 = 32,768 bytes (32 KB)**

`mkisofs -sysarea ip.bin` injects `ip.bin` verbatim into LBA 0–15.
If `ip.bin` is smaller than 32 KB, it is zero-padded. If larger, it
is truncated — so always size your IP.BIN exactly to a multiple of
2048 bytes ≤ 32768.

---

## ISO 9660 Area (LBA 16+)

### LBA 16: Primary Volume Descriptor (PVD)

```
Offset  Size  Field (all strings ASCII, space-padded)
──────────────────────────────────────────────────────
0x000     1   Type code: 0x01 (Primary Volume Descriptor)
0x001     5   Standard Identifier: "CD001"
0x006     1   Version: 0x01
0x007     1   Unused: 0x00
0x008    32   System Identifier (e.g., "SEGA SATURN         ")
0x028    32   Volume Identifier (game title, matches IP header title)
0x048     8   Unused
0x050     8   Volume Space Size (total LBA count, LE+BE pair)
0x058    32   Unused
0x078     4   Volume Set Size (usually 1)
0x07C     4   Volume Sequence Number (usually 1)
0x080     4   Logical Block Size (2048 = 0x0800)
0x084     8   Path Table Size (bytes, LE+BE)
0x08C     4   Location of Type-L Path Table (LBA, little-endian)
0x090     4   Optional Type-L Path Table location
0x094     4   Location of Type-M Path Table (LBA, big-endian)
0x098     4   Optional Type-M Path Table location
0x09C    34   Root Directory Record (inline)
0x0BE   128   Volume Set Identifier
0x13E   128   Publisher Identifier
0x1BE   128   Data Preparer Identifier
0x23E   128   Application Identifier
0x2BE    37   Copyright File Identifier
0x2E3    37   Abstract File Identifier
0x300    37   Bibliographic File Identifier
0x337    17   Volume Creation Date (ASCII: YYYYMMDDHHmmsscc+TZ)
0x348    17   Volume Modification Date
0x359    17   Volume Expiration Date
0x36A    17   Volume Effective Date
0x37B     1   File Structure Version: 0x01
```

### LBA 17: Volume Descriptor Set Terminator

- Type byte `0xFF`, followed by `"CD001"`, version `0x01`, rest zero.

### LBA 18–21: Path Tables

- Little-endian path table (LBA 18)
- Optional LE path table (LBA 19, often same)
- Big-endian path table (LBA 20)
- Optional BE path table (LBA 21, often same)

Saturn's BIOS uses standard ISO 9660; it reads the filesystem normally
to locate the first-read file specified by the IP header's
`First Read Address` / `First Read Size` — **the BIOS does not load
the first-read file by filename**; the IP code is responsible for
using the CD command registers to locate and DMA the file.

---

## CD-ROM Command Interface (A-Bus CS2)

To load files from disc in your IP or game code:

```
Register (16-bit access)   Address       Description
──────────────────────────────────────────────────────
HIRQ                       0x02000000   Host IRQ status flags
HIRQ mask                  0x02000002   Mask bits (1=enable)
CR1                        0x02000004   Command/Response 1
CR2                        0x02000006   Command/Response 2
CR3                        0x02000008   Command/Response 3
CR4                        0x0200000A   Command/Response 4
```

### Key HIRQ Bits

| Bit | Name   | Meaning                                    |
|-----|--------|--------------------------------------------|
| 0   | CMOK   | Command accepted (ok to send next command) |
| 1   | DRDY   | Data ready in CD buffer                    |
| 2   | CSCT   | Sector buffered                            |
| 3   | BFUL   | CD buffer full                             |
| 4   | PEND   | Status pending                             |
| 5   | DCHG   | Disc change detected                       |
| 6   | ESEL   | Subcode extraction complete                |
| 8   | EFLS   | Filter subheader set                       |
| 9   | ECPY   | Data copy complete                         |
| 10  | EOTH   | Others complete                            |
| 11  | EFIS   | Filter activated complete                  |
| 12  | EQER   | Soft reset complete                        |
| 13  | EHST   | Host transfer complete                     |

### Typical File-Read Sequence (from IP code)

```asm
; 1. Seek to LBA of file (FAD = LBA + 150 for CD addressing)
; 2. Issue "Seek" command (CR1=0x0002, CR2=FAD>>16, CR3=FAD&0xFFFF, CR4=0)
; 3. Wait for CMOK
; 4. Issue "Set Mode" command (set normal seek + read)
; 5. Issue "Read Sectors" command with count
; 6. Wait for DRDY / CSCT
; 7. Issue "Get Copy Error" to check for read errors
; 8. Transfer data via SCU-DMA from CD buffer (0x02020000) to HWRAM
```

The BIOS provides a set of built-in subroutines (accessible via
`TRAPA #n` syscalls) that wrap these steps. Refer to the BIOS
function table below.

---

## BIOS System Call Interface (TRAPA)

The Saturn BIOS exposes functions via `TRAPA` instructions. Call
convention: arguments in R4–R7, function code in R6 or via pre-set
registers depending on the function set.

**BIOS jump table base: `0x06000A00`** (copied there by BIOS at boot)

| Function Name    | TRAPA # | Description                          |
|------------------|---------|--------------------------------------|
| `CdInit`         | 0x02    | Initialize CD subsystem              |
| `CdPlay`         | 0x03    | Play track                           |
| `CdSeek`         | 0x04    | Seek to FAD                          |
| `CdScan`         | 0x05    | Fast scan                            |
| `CdSetMode`      | 0x06    | Set read mode                        |
| `CdGetHirq`      | 0x07    | Get HIRQ status                      |
| `CdGetStatus`    | 0x08    | Get CD status                        |
| `CdGetToc`       | 0x09    | Read TOC                             |
| `CdOpenTray`     | 0x0A    | Open disc tray                       |
| `CdClose`        | 0x0B    | Close disc tray                      |
| `CdGetBuf`       | 0x0C    | Transfer sectors to memory           |
| `SysInit`        | 0x10    | Full system init (calls CdInit etc.) |

> Many Saturn homebrew frameworks (Jo Engine, Yaul, KallistiOS port)
> provide C wrappers around these TRAPA calls. In pure assembly, issue
> `trapa #N` with the correct register setup.

---

## Audio Tracks

If your disc has audio tracks (CD-DA):

- Audio tracks must come **after** the data track in the TOC.
- Standard CD structure: Track 1 = data, Tracks 2-N = audio.
- Use `CdPlay` BIOS call with track number to start playback.
- The SCSP/CD-ROM routes audio directly to the DAC — no RAM transfer.

```
TOC layout example:
  Track 1: Data (Mode 1)     — LBA 0 → game data
  Track 2: Audio (CD-DA)     — game music 1
  Track 3: Audio (CD-DA)     — game music 2
  ...
```

`mkisofs` + `cdrdao` (with `.toc` file) is the standard way to build
multi-track Saturn disc images:

```
# example.toc
CD_ROM_XA

// Track 1: data
TRACK MODE1
  NO_COPY
  NO_PRE_EMPHASIS
  DATAFILE "game.iso" 0

// Track 2: audio
TRACK AUDIO
  NO_COPY
  NO_PRE_EMPHASIS
  TWO_CHANNEL_AUDIO
  FILE "music01.wav" 0

// Track 3: audio
TRACK AUDIO
  FILE "music02.wav" 0
```

---

## Multi-Session Discs

Saturn does **not** support multi-session CD-ROMs. Always write a
single-session disc. Multi-session images may cause the BIOS to reject
the disc or fail to read the system area correctly.

---

## Common Disc Build Errors

| Error                              | Cause                               | Fix                                  |
|------------------------------------|-------------------------------------|--------------------------------------|
| BIOS shows "disc unsuitable" screen| IP header magic wrong or corrupt    | Verify bytes 0x000–0x00F exactly     |
| BIOS shows "disc unsuitable" screen| Region symbols don't match console  | Add correct region letter (J/U/E)    |
| File not found after boot          | ISO path table corrupt              | Rebuild with mkisofs `-l` flag       |
| System area too large              | IP.BIN > 32,768 bytes               | Trim or compress IP boot code        |
| Auth passes on emulator, fails HW  | Security ring not on pressed disc   | Use mod-chip or ODE for CD-R dev     |
| DMA transfer corrupted data        | CD buffer overrun                   | Check HIRQ BFUL before each transfer |
