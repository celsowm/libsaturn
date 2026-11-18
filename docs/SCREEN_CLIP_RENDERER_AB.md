# C2 screen clip renderer qualification

The production consumer of sat_clip_quad_screen is the immediate
solid/indexed/Gouraud world renderer in rendering/indexed.cpp
(for_each_visible_piece). Skybridge uses related world rendering, but
its scene capture/queue and gameplay are not this controlled test.
This test drives the actual public immediate renderer, including
near clip, perspective projection, viewport clipping, Gouraud
interpolation and native VDP1 command emission. It does not apply
the clipper to arbitrary patterned textures.

Mode 1 and 2 opt into a matched instrumented public clip call,
original versus bounded+skipped planes. All other assembly experiments
are disabled. Default mode 0 leaves ordinary dispatch unchanged.
The fixed scene draws 29 world faces per frame with RGB flat,
uniform indexed8 and Gouraud material paths; 12 poses repeated twice,
then hold the last pose. No user input, simulation clock, HUD or audio
dependency. It is a controlled consumer scene, not a Skybridge game
benchmark. The clip incidence count named multi_piece counts calls
with more than one output primitive, not all outside/straddling faces.

Capture actual live native VDP1 list words including the END appended
by submit, and live Gouraud table words for every measured step.
Current two-bank VRAM layout uses a 64 KiB command area before Gouraud.
Use HAL frame_commands and command checkpoint. Capture before submit,
then read the END in that same bank, before the next begin_frame.
Reading/writing diagnostic buffers is outside the measured submission
and end intervals. Fixed bank sequence expected; compare literal bytes
without normalizing addresses, including unused zero capture tails.
Require complete telemetry, no panic, zero draw timeouts, identical
lists and Gouraud bytes across builds and emulators. Lossless final
screenshots plus Ymir done-triggered screenshot/stability as in B2/B3.
Measure median clip and submit/end intervals excluding the first pair.
Submit+end is measured inclusive submission/presentation work excluding
diagnostic snapshot and begin-frame wait; not FPS.

Run `python3 harness/scripts/run_screen_clip_renderer.py` and return
`harness/build/screen_clip_renderer-return.zip`. Includes both native
Mednafen modes and Ymir, full command captures, screenshots, build flags,
disassembly, local changes and failure logs. Target compile and
execution pending. Real gameplay profiling, textured cells and
near-plane optimization remain separate; no production activation.

## Matched renderer results — 2026-11-17

Checkout `a48e1e6d404019afdd8d2923e22e1346909a4539`.
Archive summary completed=true; source manifest complete and clean
(no staged/unstaged changes or nonignored untracked files).
Independently decoded all six 1184-byte big-endian telemetry records
and checked every row against the associated JSON. Both builds use
timer frequency 210540 Hz in all captures.

Six executions, 144 recorded frames, 4176 clip calls, 1920 calls
with multiple clipped pieces, 6048 emitted pieces, 6480 native
commands including frame setup and END, and 1644 Gouraud tables.
Each execution records 24 frames: 29 calls per frame, 8–16 calls
with multiple output pieces, 33–46 output pieces, 36–49 commands
and 5–14 Gouraud tables. All six 80-byte panic records zero, no
draw timeout, Ymir reported zero misaligned accesses on both CPUs.

All six complete 196608-byte command snapshots are byte-identical,
SHA256 `0f36c47d0d6c20237d7dabcff30c9c24300589855c90c82d6564ea28343e73ef`.
All six 49152-byte Gouraud snapshots byte-identical, SHA256
`ce54c1a8b9acde041d6a2b1e68b814ab9f13b0f196bb8a6155fe74a8e85dad1e`.
Independently checked each live command/table hash, END marker and
zero capture tails. Literal capture equivalence includes bank addresses;
no canonicalization. Mednafen WRAM normalized against ELF text
(swapped16); both native captures per build agree on ELF SHA256:
control `60e449a2d429e01497d13a597be17856b47a00f2ef3a47f5d16656d9b1954811`,
bounded `3c384d316e7dd7649fae77feca5c628bc76ff76850d94256a7313fe2af8c9405`.

All ten original PNGs independently decoded and compared within
each emulator: six Ymir 320x224 captures, including both done-triggered
images, and four Mednafen 682x448 captures. Zero differing RGB pixels
within either group, 140 distinct RGB colors per image. Eight manifest
comparisons cover 1508864 pixel comparisons, plus two Ymir
done-triggered comparisons. These are final frozen-pose images,
not literal raster captures of every intermediate pose. Full command
snapshots cover every measured step.

### Median timings (22 measured steps after first pair)

| Emulator / interval | Control ms | Candidate ms | Reduction |
|---|---:|---:|---:|
| Ymir clip | 1.935499 | 1.204047 | 37.79% |
| Mednafen clip | 2.006745 | 1.268168 | 36.80% |
| Ymir submit | 14.021089 | 13.204142 | 5.83% |
| Mednafen submit | 14.754916 | 13.935594 | 5.55% |
| Ymir submit+end | 14.035338 | 13.218391 | 5.82% |
| Mednafen submit+end | 14.771540 | 13.954593 | 5.53% |

Median clip FRT ticks: 407.5 -> 253.5 Ymir, 422.5 -> 267
Mednafen; submission ticks 2952 -> 2780 and 3106.5 -> 2934.
Median END interval unchanged (3 and 4 ticks respectively).
Mednafen visual and timing runs have identical reported medians.
Savings approximately 0.817/0.819 ms per submission; not total
wall-clock frame time, FPS or a physical Saturn measurement.

**Disposition:** mode 2 passes this controlled immediate-renderer
consumer gate. Keep opt-in pending broader release qualification;
next investigate bounded exact world near-plane interpolation and
depth classification separately. The measured workload is intentionally
clip-heavy, not a gameplay trace or a benchmark of the queued
Skybridge path. No claims about UV clipping, other geometry kernels,
IRQ/ABI stress, combined optimizations or physical hardware.
