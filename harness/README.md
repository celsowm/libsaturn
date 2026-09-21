# libsaturn integration harness (GPL-3.0 — separate from the rest of this repo)

## License boundary — read this first

**Everything under `harness/` is licensed GPL-3.0** (see `harness/LICENSE`),
**not** the MIT license that covers the rest of this repository (see the root
`LICENSE`). This directory exists specifically to link against
[Ymir](https://github.com/StrikerX3/Ymir), a GPL-3.0 Sega Saturn emulator
core, to drive end-to-end assertions against built ROMs.

Rules that keep the two licenses from mixing:

1. **Nothing under `harness/` is compiled into `libsaturn.a`.** The root
   `Makefile` only globs `src/core/*.cpp` and `src/hal/*.cpp` — `harness/` is
   invisible to it by construction.
2. **Nothing under `harness/` may `#include` a libsaturn header or link
   `libsaturn.a`.** The harness treats built `.iso` files as opaque black
   boxes — input to the emulator, nothing more. If you find yourself wanting
   to reuse a libsaturn type or constant here, duplicate the value instead of
   including the header.
3. **Ymir's source is never vendored into this repo.** `CMakeLists.txt`
   pulls it via `FetchContent` at build time, pinned to a specific commit
   (see that file for the pin and the reason it's not tracking `main`).
4. Code under `examples/`, `src/`, and `include/` must never depend on
   anything in `harness/`.

If you're only building/using the Saturn library or examples, you can ignore
this directory entirely — it has no effect on that build.

## What this is

A headless probe that boots a real Saturn BIOS inside Ymir's emulator core,
runs it for `--boot-frames` frames of hardware init, then **injects the
example's `.bin` directly into work RAM and jumps to it** — the ISO is still
loaded (for its IP.BIN header, which supplies the load address) but the BIOS
is never relied on to read the program off the emulated disc. It then runs
for `--frames` more frames and dumps VDP1/VDP2 registers, requested VRAM
ranges, and the VDP1 framebuffer to JSON. Python `unittest` assertions run
against that JSON.

**Why direct injection instead of a real disc boot.** A full investigation
(see git history / project memory around 2026-08-01) found that Ymir's CD
block does not currently complete a BIOS disc boot for these images: the BIOS
gets through disc authentication and CD read-filter setup once, then stalls
indefinitely in its own CD driver loop (observed spinning at a fixed PC for
900+ frames, ~2.7 minutes of emulated time, never reaching our code). A local
patch to Ymir's `CDBlock::OnDiscLoaded()` — mirroring real hardware's
autonomous TOC recognition on disc insertion — measurably improved things
(the BIOS got further before stalling) but did not finish the boot, and
fixing it further would mean permanently maintaining a patched Ymir fork
instead of tracking upstream via plain `FetchContent`, which this harness
deliberately avoids.

The disc/CD-boot path itself is **not validated by this harness** as a
result: this repository's automated acceptance path is the modified Ymir
direct-injection flow. What this harness validates is everything downstream
of the program actually running:
register setup, VRAM contents, VDP1/VDP2 state — which is what it was built
for. `harness/tests/test_rbg0_ground.py`'s `setUpClass` enforces this
boundary itself: it fails immediately (before any other assertion) if
`pc_after_run` isn't inside the injected program's address range, so a run
where injection silently didn't take can never read as a pass.

**Design bias: assert on registers and VRAM contents, not rendered pixels.**
An emulator is not hardware truth, and VDP2 rotation (the area this harness
was built for) is exactly the kind of under-tested corner where Ymir's own
rendering could be wrong. Assertions are derived from the Sega hardware
manuals in `docs/sega_saturn_hardware/`, which hold regardless of Ymir's
accuracy. Golden-image / pixel comparison is still deliberately out of scope as an
*assertion* mechanism, for the reason above.

Screenshots themselves are now supported, though: `VDP::SetSoftwareRenderCallback`
hands over the software renderer's composited output buffer (VDP1 sprites over
VDP2 layers), so `--screenshot FRAME:PATH` writes the real picture as a PNG.
An earlier pass recorded that only a "frame finished" notification existed;
that was wrong. Use screenshots to *look* at a run -- they catch whole classes
of problem that register assertions cannot, such as a leftover VDP2 colour
offset that renders everything at a tenth of its intended brightness -- but
keep the pass/fail assertions on registers and VRAM.

### Frame pacing

The probe runs one emulated frame per `RunFrame()`, and a libsaturn program
paced by `sat_wait_vblank` advances one program frame per emulated frame. If a
run seems to make no progress, measure that ratio before suspecting the
harness: a program frame that costs dozens of emulated frames means the guest
is spinning somewhere, and `--profile-pc` plus `tools/pcprof.py` will say
where. This is not hypothetical -- it was how a null VDP2 register pointer,
which made every frame burn the full VBlank poll timeout and run ~85x slow,
was finally identified.

## Layout

```
harness/
  LICENSE          GPL-3.0 (verbatim, from gnu.org)
  README.md        this file
  CMakeLists.txt    FetchContents Ymir (pinned commit), applies debug metrics, builds the probe app
  ymir_debug_metrics.patch  reproducible Ymir write/cycle instrumentation
  src/probe_main.cpp  links ymir-core; boots an ISO, dumps state to JSON
  src/png_writer.hpp  dependency-free PNG writer for --screenshot
  scripts/*.pad       input timelines for --pad-script
  tests/*.py        unittest assertions over the emitted JSON
  run-harness.ps1    Windows wrapper matching build-example.ps1 conventions
```

## Building and running

```powershell
cmake -S harness -B harness/build   # first run also fetches + builds Ymir
cmake --build harness/build
.\harness\run-harness.ps1 vdp2_rbg0_ground
python -m unittest discover harness/tests
```

A Sega Saturn BIOS/IPL image is required and is **not** committed here (same
policy as the rest of the repo's emulator tooling — see `emulators/README.md`
and `run-example.ps1 -BiosProfile`). Point `run-harness.ps1` at one with
`-Bios <path>` or the `LIBSATURN_BIOS` environment variable. It's still
required even though the disc boot is bypassed — the probe needs it to reach
a stable hardware-init state before injection, and it's what real Saturn
software would run under.

`-BootFrames` (default 90) controls how many frames of BIOS-only execution
run before injection; `-Frames` (default 60) controls how many frames run
afterward, with our code in control.

For deterministic before/after measurements, the wrapper can pass through
`-ProfilePc`, `-ProfileCycles`, `-ProfileInstructions` and
`-ProfileTransfers`. The last option writes CSV columns for VDP1 VRAM, VDP2
VRAM and VDP2 CRAM words submitted in each frame. The counters are supplied by
the pinned-Ymir patch at the central VDP memory-write boundary; they are not
pixel-difference estimates. `-ProfileInstructions` enables Ymir's SH-2 debug
tracer and counts executed master/slave instructions, while the cycle profile
reports the fixed emulated system-clock budget for context.

### Pad polarity

Ymir reports pad state active-low: `peripheral_report.hpp` documents the field
as "Button states (1=released, 0=pressed)" and the `Button` enum's `Default` is
`All`. `Button::None` therefore means *every button held*. `probe_main.cpp`
converts through `pad_report()`, so `--pad-script` and `--pad-button` take
buttons in the obvious sense: `LEFT` means left is down and nothing else is.

This was inverted before 2026-09-13, which silently held START (restarting any
example that watches for it) on every scripted run. To check the conversion,
run `examples/input_debug` under a script whose current line is `NONE` and
confirm every HELD bit reads 0.

## Backup RAM / save persistence

The probe can attach a real file-backed 32 KiB internal Backup RAM image with
`--backup-ram <path>`. The PowerShell wrapper exposes the same option as
`-BackupRam`. Ymir maps the image write-through, so writes made by the guest
survive after `probe.exe` exits and can be observed by a fresh emulator
process.

When a Backup RAM image is attached, `probe.json` contains a
`backup_memory` object with:

- header validity, byte/block capacity and used blocks;
- the logical BUP directory (filename, comment, language, date and block use);
- exported payload size, FNV-1a hash and the first 64 payload bytes for each
  record.

That data is an emulator-side observation used to prove persistence; guest code
still reaches storage through the Saturn Boot ROM BUP library. The harness does
not bypass LibSaturn's save implementation to create `LIBSAT_DEMO`.

For the end-to-end two-process acceptance test:

```powershell
.\harness\run-save-persistence.ps1 -Bios .\bios\saturn_bios_us.bin
```

The script deletes its test Backup RAM image, launches
`examples/save_backup_demo` once, exits the emulator, then launches a second
fresh probe against the same image. The Python assertion requires the guest's
versioned `LIBSAT_DEMO` record to contain boot count 1 after the first process
and boot count 2 after the second. A same-process write/read check alone is not
accepted as persistence proof.

`run-harness.ps1 save_backup_demo` automatically uses
`harness/build/save_backup_demo.bup` when `-BackupRam` is omitted, which is
convenient for manual restart testing. Delete that file to simulate a new
formatted Ymir Backup RAM image.

## Backup Memory cartridge inspection (read-only fixture)

The probe accepts `--backup-cart <existing-path>` (PowerShell:
`-BackupCart <existing-path>`) **in addition to** `--backup-ram`. This is
an initial read-only diagnostic for the persistent Backup Memory cartridge,
**not** support for cartridge writes in the LibSaturn save API.

The cartridge image must already exist and have a Ymir-supported size (start
with an isolated 512 KiB test image). Ymir loads it via `LoadFrom` with
`copyOnWrite=true`, then attaches a `BackupMemoryCartridge` before guest
execution. The probe never calls `CreateFrom` on this path and never
resizes/formats an existing image. Guest writes, if any, remain private to
the emulated process and cannot change the supplied file.

The `backup_cartridge` JSON object is independent of the internal
`backup_memory` object. It reports image metadata, file names, raw-memory
hashes before/after guest execution, and whether the guest changed its private
copy. **A cartridge-visible file in Ymir JSON does not prove the guest used
LibSaturn's cartridge save API.** That requires a separate BIOS-backed guest
write/verify/read acceptance test, after the BUP device selector and internal
persistence test are validated.

Use an isolated copy of a cart image, never an image containing valued saves.
No proprietary BIOS or personal backup image belongs in Git.

## Audio / SCSP regressions

For audio work, read [`docs/SCSP_AUDIO_STREAMING_GUIDE.md`](../docs/SCSP_AUDIO_STREAMING_GUIDE.md)
before adding assertions. The probe still does not capture final DAC samples, so
do not treat a successful screenshot as evidence that PCM playback is correct.

The probe now exports LibSaturn's reserved stream slots (28-31) under
`scsp.stream_slots`. It records slot activity, SA/LSA/LEA, current sample,
PCM width, loop mode, OCT/FNS, TL, DISDL/DIPAN and hashes of both 4096-sample
Sound RAM halves for the continuous S16 streaming layout.

Use `--scsp-trace` directly or `-ScspTrace` through `run-harness.ps1` to
capture the same state every program frame under `scsp.trace`.
`cd_streaming_jukebox` enables this trace automatically. Its acceptance test
compares consecutive Sound RAM hashes and fails if a refill rewrites both halves
or rewrites the half currently containing `curr_sample`.

That test intentionally uses Ymir's hardware state through
`saturn.SCSP.GetProbe().GetSlots()` plus `SCSP::DumpWRAM()`, rather than
inferring audio correctness from VDP state.



## Volatile DRAM expansion cartridge

The RAM-expansion mode is independent of the persistent Backup Memory cartridge.
Run the same demo with three configurations:

```powershell
.\harness\run-harness.ps1 ram_cart_demo -Bios .\bios\saturn_bios_us.bin -RamCart 4m -Frames 60
.\harness\run-harness.ps1 ram_cart_demo -Bios .\bios\saturn_bios_us.bin -RamCart 1m -Frames 60
.\harness\run-harness.ps1 ram_cart_demo -Bios .\bios\saturn_bios_us.bin -RamCart none -Frames 60
```

The JSON `ram_cartridge.bank_edge_verified` becomes true when the demo's eight-byte
pattern spans both physical banks correctly. You cannot use `-BackupCart` and
`-RamCart 4m/1m` at the same time, but internal `-BackupRam` remains independent.
Provide your own legally obtained Saturn BIOS image.


### Three-mode RAM expansion acceptance

Run `harness/run-ram-cart-acceptance.ps1 -Bios <your-BIOS-path>`.
It runs `ram_cart_demo` with none, 1 MiB, and 4 MiB configurations,
and checks the headless Ymir JSON for both detected capacity and successful
writes/reads over the boundary between DRAM0 and DRAM1.
The test requires a user-supplied BIOS and is intentionally not run in CI.
