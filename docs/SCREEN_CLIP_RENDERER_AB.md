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
