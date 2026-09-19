# voxel_display_probe

**Experimental video presentation benchmark**, not a finished renderer.
A moving 160x112 INDEX8 diagnostic image is displayed in two modes:

- VDP1: dynamic texture upload + 320x224 scaled sprite;
- VDP2: write the top-left 160x112 of a 512x256 RBG0 bitmap in VRAM-A0,
  then use an identity rotation table with kx=ky=0.5 to display at 320x224.

Both modes use the **same generated indexed pixels and palette**. Press A to
switch; START exits. The HUD reports CPU pattern-generation time, VRAM update
time and elapsed display VBlanks per produced frame. No external assets,
ROM changes, RAMCart or additional SH-2 worker are needed.

Build with `make EXAMPLE=voxel_display_probe all`; host regression:
`make build/tests/test_voxel_display_probe && ./build/tests/test_voxel_display_probe`.

**Known limitation:** this first RBG0 path uses 112 individual CPU VRAM writes
per produced frame while RBG0 display remains enabled. It does not guarantee
that every write finishes during VBlank. Tearing, CPU/VDP2 fetch contention
and a low frame rate are **potential outcomes to measure**, not success
criteria that have been verified. The probe deliberately does not advertise
a public VDP2 framebuffer/presenter API until a synchronized implementation
has been tested on a Saturn.

VRAM reservation for this isolated example: A0 [0x00000,0x0FFFF] is the
RBG0 512x256 INDEX8 bitmap; A1 0x10000..0x1002F is the rotation table.
The example does not configure NBG0 or the VDP2 sprite color-calc tables.
Palette bank 0 is reserved for RBG0 *before* allocating the logical VDP1
texture, preventing the high-level texture palette registry from reusing it.
The VDP1 font uses bank 2. Keep the VDP1 framebuffer erase transparent or
it will cover the RBG0 image.

Manual acceptance matrix: compare a fixed pattern and moving pattern on
an emulator and real hardware, at 160x112 then other resolutions, with
RBG0 enabled/disabled. Inspect tearing along horizontal bands, color order,
VRAM range safety, mode toggles and HUD visibility. Do not infer visual
correctness merely from host tests or a successful SH-2 cross-build.

See `docs/VOXEL_TERRAIN_PLAN.md`.
