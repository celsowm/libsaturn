# Experimental effects compositor

`water_lab_probe` owns its VDP1 command table and framebuffer registers after
startup. It verifies the 160×112 region at (320, 0), boundary pixels and
alternating drawing banks. The primary viewport remains 320×224. The probe
observes the current command's END and EDSR.CEF before reading the drawing
bank. It requests a change only after both pieces of the next pair are ready,
then observes the drawing-bank tag change.

The synthetic RGB16 probe uses NBG0 at 2× magnification. It checks transparent
zero, a bit-15-clear nonzero word, covered RGB black and RGB color. The
alternating real-readback probe uses two reserved indexed8 bitmaps with a
reserved CRAM bank. Palette index zero is transparent and index one
represents covered black. Conversion uses a fixed RGB222 probe palette.
The normal VDP2 backdrop table occupies the final VRAM word, which
conflicts with the second full-size RGB16 reservation in this arrangement.

The standalone probe captures the previous pair during an injected draw
delay, upload delay, and a partial write to the inactive bitmap. The
partial write returns a simulated transfer error after 12 rows. The
probe waits for DMA to settle before rewriting that bitmap. After pair 32
it drains the draw and upload, disables NBG0, and releases the bitmaps
and palette. It reacquires them before publishing pair 33. The probe's
HUD marker occupies a transparent part of the effect layer. A VDP1 HUD
overlapping a covered NBG0 effect takes part in the blend with this
priority setup.

## Scene integration

The Water Lab multipass mode uses `saturn/fx_compositor.h`. The experimental
API claims NBG0, two 128 KiB indexed8 bitmaps, one 256-code CRAM range, and
SCU DMA channel 0 during transfers. The application supplies the workspace
and brackets the scene flush with VDP1 command checkpoints. It gives the
compositor the bounds of any HUD regions that need to stay above the effect.

The compositor copies the prepared coordinates into the auxiliary draw region.
The copy follows the original painter order. An RGB half-transparency command
writes its color into the effect bitmap and is skipped in the visible VDP1
pass. Opaque RGB polygons, Gouraud RGB polygons, and INDEX8 distorted sprites
with transparent pixels disabled clear the effect's coverage. Faces that
precede the first effect command need no mask. The copy restores local
coordinates and both clip modes before the next main-screen command.

This is one effect layer at a fixed VDP2 ratio. When multiple effect faces
cover the same reduced pixel, the last effect face sets its color. An ordinary
VDP1 HUD needs explicit rectangular masks, rounded out to a 2×2 boundary.
Opaque edges share the same geometry on both passes. Sampling them at half
resolution can leave a one-pixel fringe compared with the native painter.
Subdividing intersecting geometry keeps the existing painter's ordering
meaningful. Cutout textures, mesh occluders, and Gouraud translucent effects
are unsupported in the compositor. A refusal preserves the original
commands, so the caller can present the native frame with the layer hidden.

Use `reserve` after video initialization, with ordinary 320×224 16-bit
ON_SUBMIT frames. Retain the workspace and leave NBG0, its VRAM and CRAM
reservations, and its color calculation to the compositor during ownership.
Call `prepare` for each effect frame, then call `present` to submit the frame
and publish the uploaded bitmap with the VDP1 swap. Transient transfer errors
leave the front image in place and permit another `present` call. Present a
native frame through the same API before releasing an active layer. The
application can resume normal frame submission after `release` succeeds.

The optimized upload covers the union of the new coverage and the last
coverage in the inactive bitmap. Each bitmap starts out requiring a full
visible-region upload. Updating the old coverage clears pixels when the
camera moves or the water shrinks.

## Physical console evidence

Use the probe disc with a stock Saturn and a bootable disc or ODE
setup. Its opening test shows four equally wide vertical strips.
The first two expose the backdrop, the third blends covered black
with it, and the fourth blends cyan. The main test shows a field
whose left and right halves change color together and a fixed white
HUD marker at the upper right. At the scripted mode change both
halves temporarily match. Capture every video field from boot and
record the probe BIN hash, console region, refresh rate and capture
device. The two serial colors occur at (159, 40) and (160, 40) in
digital 320×224 coordinates. Check both sides during the brief holds
and after the effect resumes.

For a lossless capture already normalized to 320×224, pass headerless RGB24
with one frame per console field to the verifier. It checks at least 3,600
consecutive scene/effect pairs, the held pairs, disabled layer, HUD marker
and split boundary. Keep the original recording, capture settings, console
details and verified disc hash with the JSON output. The exact RGB
comparison expects console-aligned pixels and the probe's digital color
values.

```sh
python3 harness/scripts/verify_water_lab_capture.py capture.rgb --refresh 60
```

The hardware bundle includes the same script at the root of the archive.

In Water Lab, use the X button to cycle to Multipass. Check the front ledge,
island and actors as the orbit moves, switch to the low camera with C,
toggle the diagnostics with Z, and switch back to Rippling Water. Record
any stale effect, tearing, visible edge fringe or missed field independently
of the standalone probe results.

[Audit](WATER_LAB_AUDIT.md) · [Benchmark evidence](WATER_BENCHMARKS.md) · [Probe source](../examples/water_lab_probe/main.c)
