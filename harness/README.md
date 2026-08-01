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
result — `run-example.ps1` against mednafen/Kronos remains the check that an
IP.BIN and ISO actually boot on something that reads a real disc. What this
harness validates is everything downstream of the program actually running:
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
accuracy. Golden-image / pixel comparison is deliberately out of scope here —
see the root plan history for why (`vdp_configs.hpp` in Ymir only exposes a
"frame finished" notification, not a composited-output buffer, so capturing a
final image would need more investigation than this pass covers).

## Layout

```
harness/
  LICENSE          GPL-3.0 (verbatim, from gnu.org)
  README.md        this file
  CMakeLists.txt    FetchContents Ymir (pinned commit), builds the probe app
  src/probe_main.cpp  links ymir-core; boots an ISO, dumps state to JSON
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
