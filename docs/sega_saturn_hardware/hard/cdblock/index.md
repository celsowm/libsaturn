# Saturn CD Block notes

> Hand-written, not produced by `download_docs_md.py`. The downloaded manuals in this folder describe
> the CD Block only as an overview (Overview Manual, section 3.6, and the SMPC "CD block ON" state);
> the register and command documentation is Sega's *CD communication I/F* programmer's guide, which
> is a separate document. This page collects both, plus what libsaturn does with the block and what
> has only been measured on emulators.

Read it before changing `src/hal/cd/block.cpp`, `src/storage/cd/disc_stream.cpp`, or anything that
times a disc read.

## What the manuals in this folder say **[Sega, local]**

From [Overview Manual 3.6 CD-ROM](../intr/hon/p03_60.md) (Tables 3.10 and 3.11), with the sub-CPU and
CD buffer described in [Overview Manual 2](../intr/hon/p02_10.md):

| Item | Value |
| --- | --- |
| Drive | 2x speed CD-ROM drive; sub-CPU interprets host commands, controls the drive and the CD buffer |
| Read speed | standard 75 sectors/s = 150 KB/s; double speed **150 sectors/s = 300 KB/s** |
| Rotation | 620-1680 rpm standard, 1240-3360 rpm double |
| Average 1/3-stroke access time (from PLAY command to PLAY status, mean of 200 trips) | double speed **500 ms** (700 ms after the reliability test); standard speed **800 ms** (1000 ms) |
| Memory | 512 KB RAM for the CD buffer, 64 KB BIOS ROM, 512 KB RAM for MPEG (the Sega CD guide below says the buffer holds 200 sectors) |
| Host data transfer rate | up to **8 MB/s** (4 MB/s while MPEG runs) |
| Register width | 16 bits |
| Drive features | play by track/index or FAD, resume (cancel pause), repeat, scan, subcode; CD-DA and CD-ROM share one command format; multi-session; CD-ROM XA decoding with subheader recognition, ECC and read retry |
| Main-system features | stream selection (data selected by type in the CD buffer, set by commands), several selectors in parallel, MPEG |

The [SMPC manual](../smpc/hon/p01_10.md) lists "CD block ON" as the state in which the CD Block is
reachable from the SH-2. Values are "for reference only" in the manual.

What these figures mean for libsaturn (arithmetic on the numbers above, not extra measurements):

- A scheduler segment is 8 sectors = 16 KiB = **53 ms of drive time at 2x** (107 ms at 1x).
- The 22.05 kHz stereo S16 music of the jukebox needs 88.2 KB/s, about 43 sectors/s: **29% of the 2x
  rate, 57% of the 1x rate**. Throughput is not the limit; the drive has room for data jobs too.
- The average access time of **500 ms at 2x / 800 ms at 1x is longer than the 371 ms the default
  `music_reserve` (8192 frames at 22.05 kHz) represents**, and the whole 16384-frame ring is 743 ms.
  Emulators model a seek as almost free, so every emulator run in this repository is optimistic here:
  on a real drive, a data job that sits away from the music file costs a seek out and a seek back per
  turn. Short seeks are cheaper than the 1/3-stroke average, but the manual gives no figure for them.
  See "Seek time on real hardware" below.

## Sources and how far to trust them

| Source | Used for | Trust |
| --- | --- | --- |
| This folder: [Overview Manual 3.6](../intr/hon/p03_60.md), [2](../intr/hon/p02_10.md), [SMPC](../smpc/hon/p01_10.md) | read speed, access time, buffer RAM, host transfer rate, feature list | Official (values "for reference only") |
| Sega, *Programmer's Guide: CD communication I/F (CD part)*, 1997 (mirror: `https://www.infochunk.com/saturn/segahtml_en/prgg/sysg/cdif/`, chapters `hon/p01_10.htm` .. `p08_19.htm`) | registers, HIRQ bits, command/response rules, drive states, buffer and selector model, function list | Official; the primary source for everything marked **[Sega]** |
| Sega, *Saturn CD Communication Interface External Specification*, ST-38-R1-121093, 1994 (`segaretro.org`, PDF) | the hardware-level spec behind the guide | Official, **not read in full** here; the command-code tables live there |
| Sega programmer FAQ, `https://www.infochunk.com/saturn/segahtml_en/faq/prog/` (p03: CD buffer, p07: transfer times) | 200-sector buffer use, CPU/DMA transfer timing for VDP1/VDP2/SCSP | Official; the transfer-time table does **not** include the CD Block |
| SCU User's Manual (`../scu_`) and the FAQ note on SCU-DMA | DMA restrictions | Official |
| libsaturn host tests, Mednafen and Ymir runs | per-poll cost, underrun and frame numbers | **[measured]**: emulator timing, no physical hardware run |

Anything the guide does not state is marked **[not documented]**. Do not turn a **[measured]** figure
into a hardware claim.

## What the CD Block is **[Sega]**

- A subsystem with its own controller; the SH-2 is the *host*. Host and block talk through 16-bit
  registers: commands with responses, plus a separate data-transfer register.
- Two groups of functions: the CD part (communication, drive control, CD buffer, a file system layer
  for ISO9660) and the MPEG part (separate library).
- The CD buffer holds **200 sectors** (about 460 KB) and is split into **24 selectors**, each an
  aperture (filter) plus a buffer partition. CD-ROM XA sectors and streams from other devices (host,
  MPEG) are handled uniformly.
- The block polls the drive itself. The drive supports CD-ROM XA and multi-session discs and switches
  between standard and double speed; CD-DA always plays at standard speed.
- The guide's interface (`CDC_*`) maps nearly one-to-one onto block commands. It does **not** move
  data: that is the job of CPU software transfer, CPU DMA or SCU DMA.

## Registers and interrupt flags **[Sega]**

All communication registers are 16 bits wide.

| Register | Role |
| --- | --- |
| `DATATRNS` | data transfer FIFO (`SAT_CD_BLOCK_DTR`) |
| `HIRQREQ` | interrupt-cause flags, set by the block, cleared **only** by the host (`SAT_CD_BLOCK_HIRQ`) |
| `HIRQMSK` | interrupt mask (`SAT_CD_BLOCK_HIRQ_MASK`) |
| command / response (`CR1`..`CR4`) | command in, response out (`SAT_CD_BLOCK_CR1..CR4`) |
| `MPEGRGB` | MPEG image data (not used by the library) |

`HIRQREQ` bits (the library defines the ones it uses in `include/saturn/cd_block.h`):

| Bit | Name | Meaning |
| --- | --- | --- |
| 0 | CMOK | a command can be issued (the previous response is set) |
| 1 | DRDY | data transfer ready |
| 2 | CSCT | one sector read completed (set per sector stored or erased) |
| 3 | BFUL | CD buffer full (the drive pauses until room is freed) |
| 4 | PEND | end of CD playback |
| 5 | DCHG | disc changed |
| 6 | ESEL | selector setting finished |
| 7 | EHST | host input/output finished |
| 8 | ECPY | copy/move finished |
| 9 | EFLS | file system processing finished |
| 10 | SCDQ | subcode Q updated |
| 11 | MPED | MPEG processing finished |
| 12 | MPCM | end of MPEG malfunction section |
| 13 | MPST | MPEG interrupt status |

Rules worth remembering:

- A response is one status byte plus a 7-byte report. Status can be a drive state, a data-transfer
  request, `REJECT` (`FFh`; the report is invalid) or `WAIT` (only the drive status and CD report are
  valid). The library maps `FFh` to `SAT_ERR_IO` and the busy bit to `SAT_ERR_BUSY`.
- Clearing a flag is done by writing 0 to it and 1 to the others. Never clear `CMOK` (the
  communication layer owns it) and do not clear `DCHG` during init: the BOOT ROM clears it during its
  disc check.
- Commands that share an end flag (ESEL, EHST, ECPY, EFLS, MPED) must wait for the previous one to
  finish. The library clears the wanted flags before it writes a command and then waits on them.

## Command timing rules **[Sega]**

- All interrupts are disabled during a command/response exchange, and consecutive commands can delay
  responses. A response that does not arrive in time is a communication timeout.
- Limits: at most **30 commands in any 6.7 ms**, **60 in any 16.7 ms**; subcode acquisition at most
  34 in 13.3 ms.
- **Continuous command polling is prohibited.** Use the periodic response and the `HIRQREQ` flags
  (`SCDQ`, `CSCT`) together with VSYNC timing instead.
- The block updates a *periodic response* (status plus report, readable without a command) every
  **13.3 ms at standard speed, 6.7 ms at double speed, 16.7 ms otherwise**. The update pauses while a
  command/response is running and resumes at the next drive-communication timing. Reading it right
  after a command can return "not ready" (`CDC_ERR_PERI`): call again.
- A soft reset needs about 3 ms in-function and forbids every command until `ESEL` is set.
- Drive commands answer `<BUSY>` unless rejected; a later command may overwrite an earlier one if
  they are issued in rapid succession, so wait for a state other than `<BUSY>` first.

**[not documented]** per-command execution times and per-sector transfer times (the Overview Manual
only gives the 8 MB/s peak host rate). Nothing in the guide or the FAQ gives them, so any "ms per
command" is a measurement of one emulator or one drive.

## The drive **[Sega]**

States: `BUSY 00h, PAUSE 01h, STANDBY 02h, PLAY 03h, SEEK 04h, SCAN 05h, OPEN 06h, NODISC 07h,
RETRY 08h, ERROR 09h, FATAL 0Ah`. Everything passes through `<BUSY>`; transitions to `<PLAY>` or
`<SCAN>` may pass through `<SEEK>`.

- `CDC_CdPlay` plays CD-DA and *reads sector data* in the CD-ROM area: the same command, so a data
  read is a "play" over a sector range, positions given as FAD or track/index. FAD = LBA + 150.
- The sector at the current FAD is not in the buffer; the host can read up to the sector before it.
- If a new play range still contains the current position, the drive keeps playing without
  interruption. Out of range ends playback (`PEND`) or repeats (repeat count 0..14, maximum 0..15).
- **Buffer full:** the drive pauses, sets `BFUL`, and resumes by itself when the host frees sectors.
  A stream device that is not CD-ROM errors on a full buffer instead.
- `CDC_CdSeek` with no position stops (home seek) or pauses (unchanged position). A stop makes the
  next access slower; use pause if you will keep reading.
- Fatal/error recovery: after a stop, wait for `SCDQ` to update twice before the next drive command.
- Retry and ECC behaviour are set by `CDC_CdInit` (retry count `00h` stops at `<ERROR>`, `01h-0Fh`
  retries, `80h` retries forever; ECC repeat count similarly).

The CD guide gives no read-speed or seek figures; the Overview Manual does (see the first table: 75 / 150
sectors/s and 500 / 800 ms average 1/3-stroke access). Spin-up time and short-seek time are
**[not documented]**.

## Buffer, selectors and sector formats **[Sega]**

- Sectors are always **2352 bytes** in the buffer. The host picks the length it fetches: 2048 to 2352
  bytes (`CDC_SetSctLen`). The FAQ quotes 2048 bytes for Mode 1 / Mode 2 Form 1 and 2324 for Form 2.
- Selector = aperture (filter on FAD range and subheader; matching sectors take the *true* output into
  the partition, the rest the *false* output, which can chain to another aperture) + buffer partition.
  After reset, aperture *n* feeds partition *n*.
- Each stored or erased sector raises `CSCT`; with multiple ECC passes the interrupts are not evenly
  spaced and several sectors can arrive at once.
- Selector changes take effect when `ESEL` is set. During playback they apply within one FAD of the
  reported one: issue the command at least **10 sectors** before the target FAD.
- Sector operations: fetch (get), erase, fetch-and-erase, copy, move. A request for 0 sectors or an
  empty partition answers `WAIT`; check `CDC_GetSctNum` first.
- FAQ: with a file loaded resident (200 sectors), seek and read inside those 200 sectors never
  touches the disc; beyond them the data is read from the CD again.

## Data transfer procedure **[Sega]**

1. Issue the command that transfers data (get/get-and-delete a sector range, or put).
2. Wait for `DRDY` (`CDC_DataReady`; it clears the flag). Writing with no free sector sets `EHST` and
   fails.
3. Read (or write) words through `DATATRNS`, pointer from `CDC_GetDataPtr`.
4. End the transfer (`CDC_DataEnd`). Ending before `DRDY` is not allowed even if no data moves; it
   reports how many words the block actually transferred (interrupted, complete or padded).

Register access: CPU DMA and software transfers must access `DATATRNS` as **32-bit longwords**
(upper word first); an odd word count leaves the last half undefined. Declare the pointer
`volatile`. libsaturn reads 16-bit words (`transfer_words` in `src/hal/cd/block.cpp`); that works on
Mednafen and Ymir, but the guide's longword rule has not been tried, and nothing has run on hardware.

**SCU-DMA:** SCU-DMA *indirect mode must not be used* for the CD Block data register; only a 4-byte
address increment is possible there, and direct mode has no such limit (SCU manual note and CDC
guide). The library has no DMA path from the CD register yet (see the DMA notes below).

## Transfer-time figures that exist **[Sega FAQ]**

The FAQ measures WORKRAM-H to SCSP/VDP1/VDP2 (256 bytes, VBlank, no VDP1 drawing), not the CD
Block: program 78.1 / 36.8 / 36.6 us, CPU-DMA 77.7 / 29.6 / 18.0 us, SCU-DMA 63.9 / 5.4 / 5.2 us.
SCU-DMA has 20+ clocks of set-up and 12+ clocks of tear-down and runs in burst mode (the CPU stalls
on a cache miss until it ends). Use these only for the SCSP/VDP side of a pipeline, e.g. the sound
RAM write of a delivered segment, never as a CD Block number.

## The CD Block function list **[Sega]**

Common: `GetCurStat, GetLastStat, GetPeriStat, GetHwInfo, TgetToc (102 entries / 408 bytes), GetSes,
CdInit, CdOpen, DataReady, DataEnd`. Drive: `CdPlay, CdSeek, CdScan`. Subcode: `TgetScdQch,
TgetScdRwch` (R-W: at most 16 packs per frame, a pack within 3.2 ms; Q updates every frame).
CD device: `CdSetCon, CdGetCon, CdGetLastBuf`. Selector: `SetFiltRange/Subh/Mode/Con` (+ get),
`ResetSelector`. Buffer info: `GetBufSiz` (200 sectors, 24 partitions, free size where 0 does not
always mean full), `GetSctNum, CalActSiz, GetActSiz, GetSctInfo, ExeFadSearch, GetFadSearch`. Buffer
I/O: `SetSctLen, GetSctData, DelSctData, GetdelSctData, PutSctData, CopySctData, MoveSctData,
GetCopyErr`. File system: `ChgDir, ReadDir, GetFileScope, TgetFileInfo, ReadFile, AbortFile`. Register
access: `GetDataPtr, GetHirqReq, ClrHirqReq, GetHirqMsk, SetHirqMsk, GetMpegPtr`.

## What libsaturn does with it

`src/hal/cd/block.cpp` talks to the registers at `SAT_CD_BLOCK_BASE` (`0x25890000`). The command words
below are the ones the code writes; the role names are matched to the guide's function list by
purpose, since the guide pages we read do not print the numeric command codes (those are in the
external specification).

Starting a read (`start_play`), once per segment: set the sector length to 2048 (`0x60`), reset the
selector (`0x48`), connect the CD device to filter 0 (`0x30`), clear stale `CSCT`/`PEND`, then `CdPlay`
(`0x10`) over `[lba + 150, + count)`. Each of these waits on `CMOK`, and the three setup commands also
on `ESEL`.

Moving what has arrived (`take_sectors`, per poll): ask how many sectors are in the partition
(`0x51`, response `CR4`), then get-and-delete that many (`0x63`), wait `DRDY`, read
`count * 2048 / 2` words from `DATATRNS`, end the transfer (`0x06`). A poll is therefore about three
commands plus the data when something has arrived, one command when nothing has.

Layers above it:

- `sat_cd_block_read_sectors`: blocking read; `sat_cd_block_set_progress_service` runs a hook inside
  the waits (the hook must not touch the disc).
- `sat_cd_stream_begin / poll / finish`: the same command sequence split into a background play and
  non-blocking polls. One poll moves whatever has arrived, because on the emulators the cost is per
  command, not per sector.
- `sat_disc_stream` (`include/saturn/disc_stream.h`): the only reader that should own the drive while
  music plays. One segment is `SAT_DISC_STREAM_SEGMENT_SECTORS` (8) sectors; contiguous segments are
  meant to continue without a re-seek, since the guide says a new play range that still contains the
  current position keeps playing (not separately verified on a drive). See
  `docs/SCSP_AUDIO_STREAMING_GUIDE.md`, "Disc-scheduled music".

## Measured on emulators **[measured, not hardware]**

Mednafen (the 60 s CD jukebox, 22.05 kHz stereo S16, `JUKEBOX_DISC_STREAM=1`):

| Quantity | Value |
| --- | --- |
| Whole-batch poll, audio hook disabled | 4.3 ms worst |
| One delivery slice (4 KiB of PCM to the SCSP rings) | 1.8 ms worst |
| `sat_audio_update` half refill (audio driver, not CD) | ~11.8 ms |
| Worst `sat_music_update` / worst frame | 14.6 ms / 31 ms |
| Blocking 64 KiB read in the legacy path | ~200 ms (worst update 359,709 us, frame 381 ms) |
| Underruns | 0 |

What did not help: capping sectors per poll (cap of 1 underran), and issuing one command per call
(per-call cost no lower, and the jukebox's ~30 Hz service rate starved the stream: 109-117
underruns). One sector costs about as much as eight, which fits "per command, not per sector".

Ymir and Mednafen model timing differently (Mednafen charges SMPC pad INTBACK and VDP1 erase costs
that Ymir ignores; a Ymir-only run hid a 28 fps frame), so only the Mednafen figures above are
quoted for frame cost. The Ymir harness is used for correctness (bytes, rings, underruns).

## Seek time on real hardware **[Sega, local] + analysis**

The Overview Manual puts the average 1/3-stroke access at 500 ms (2x) and 800 ms (1x), and gives
nothing for short seeks. The emulators this repository runs on answer a seek almost at once, so the
scheduler's behaviour next to a real drive has not been seen. What follows is arithmetic on the manual's
numbers, to be confirmed on hardware (record it in `docs/HARDWARE_ACCEPTANCE_CHECKLIST.md`):

- `sat_disc_stream` serves music first and a data job only in the gaps while both rings hold at least
  `music_reserve` frames (default 8192 = 371 ms of 22.05 kHz audio, ring capacity 16384 = 743 ms).
  Starting a data segment means a seek to the data file, one 53 ms segment, and a seek back to the
  music. Two seeks at the manual's average already cost 1 s at 2x, more than the ring holds. An
  underrun on hardware is therefore possible even though no emulator shows one.
- The music file itself is read sequentially, so contiguous music segments need no seek. Only the
  switches between files do.
- Ways to keep the music alive, none of them tried on hardware yet: lay the data files next to the
  music on the disc (the order the files are given to xorriso, so seeks are short); read data in
  longer bursts per switch (several consecutive segments) while the rings are full; lower the music
  rate or use 8-bit/mono rings (the same 16384 frames then last twice as long, or the SCSP ring RAM
  is halved); raise `music_reserve` and the ring size together; or pre-load the data before the music
  starts (what the bullet-hell demo does for stage 1 while the screen is dark).
- Keep seeks and `sat_cdfs_lookup` away from playback for the same reason: each can move the pickup
  (`sat_music_play` now only schedules the first reads).

## Opportunities the documentation suggests (not done)

- **Do not poll with a command when nothing can have arrived.** The guide forbids continuous command
  polling and recommends the `CSCT`/periodic-response flags. `get_ready_sectors` issues a command on
  every poll; gating it on `HIRQREQ.CSCT` (a register read, no command) would make an idle poll free
  and keep the command rate well under 30 per 6.7 ms. It does not change the worst poll (the one that
  moves sectors), so the worst frame stays as measured.
- **Bigger or fewer segments.** The 200-sector buffer is far larger than the 8-sector segment, so a
  deeper read-ahead is possible; the limit is the SCSP ring size, not the block.
- **SCU-DMA from `DATATRNS`.** Allowed in direct mode only. The project already found Ymir and
  Mednafen disagree on DMA timing, so any such path stays opt-in until both agree.
- **Longword reads of `DATATRNS`** as the guide requires for CPU-DMA/software transfers; check on
  hardware.

## Not covered anywhere we could read

Physical drive timing (seek, spin-up, 1x/2x throughput), per-command latency, the numeric command
code tables (external specification PDF), CD-DA/MPEG behaviour of the library, and every figure on
real hardware. `docs/HARDWARE_ACCEPTANCE_CHECKLIST.md` is the place to record a hardware run.
