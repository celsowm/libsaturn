# Dino Demo

An interactive Sega Saturn take on the original PlayStation T-Rex tech demo.
It converts the attributed walking GLB into a bounded, baked animated model
and runs it through the Saturn's VDP1 painter path.

The dinosaur starts in a large side view against a dark screen, with its
original warm texture and a compact control display. L/R rotate the camera,
UP/DOWN change the viewing angle, X zooms in, and Y zooms out. A pauses the
walk, B resets the camera, and C toggles slow automatic orbit. START hides or
restores the text.

## How it stays close to the source

- **Texture filtered to screen size.** The VDP1 reads one texel per pixel
  with no filtering or mipmaps, so a face holding more texels than it covers
  on screen shows the skin as speckle noise. Each face bakes at its size when
  the T-Rex spans about 400 pixels (`MODEL_TEXEL_EXTENT`), box-filtered from
  the GLB's 512x512 texture (`MODEL_SAMPLING area`): smooth at the default
  view, still detailed at full zoom.
- **15 colors per face.** Texels are 4-bit with a VDP1 lookup table per face
  (`MODEL_TEXTURE_FORMAT lut4`), fitted to that face's patch of the texture.
- **Quads, not triangles.** Adjacent triangle pairs whose texture mapping and
  fold survive the change draw as one VDP1 quad (`MODEL_MERGE_QUADS`): 2400
  simplified triangles become about 1670 commands.
- **Triangles where they show.** The 46 teeth and claws are a third of the
  source mesh but a few pixels on screen; `MODEL_MATERIAL_WEIGHTS` spends
  that budget on the textured body. Solid-color faces draw as RGB polygons.
- **One vertex per moving point.** UV-split vertex copies are welded
  (`MODEL_WELD_VERTICES`), about 1300 runtime vertices instead of 1900.

## Hi-res

The demo runs the Saturn's 640x224 hi-res mode (`DINO_HIRES`, on by
default): twice the horizontal pixels on the same 4:3 screen. The VDP1
framebuffer is then 8 bits per pixel and can only hold palette codes, so the
whole model shares one palette of 252 colors (`MODEL_LUT_CODES 2-253`),
each face's lookup table holds the codes of its 15 colors, and faces bake
at `MODEL_TEXEL_EXTENT 480`. Code 0 is transparent, code 1 is the HUD's
white and 254 is the sprite shadow code. The HUD font doubles each glyph
horizontally so the text keeps its size. It costs about 1-2 fps against
the 320x224 build, which `DINO_HIRES=0` restores.

## Both SH-2s

The face list is split between the CPUs: the Slave prepares 40% of the faces
while the Master prepares the rest, and the Master merges both into one
painter queue. `MODEL_LOCALITY_ORDER` orders faces along the body so each
half projects only its own window of vertices. While the Master sorts and
emits, the Slave decodes the next pose. In Ymir the hi-res build runs at about
18 fps from the side and 15 fps in the busiest views (320x224: 20 and 15).

Build with `.\build-example.ps1 dino_demo`. Run it in Ymir's modified
integration harness with `.\harness\run-harness.ps1 dino_demo -Bios <your-BIOS>`
(the harness hands the Slave its entry point, which the BIOS normally does;
without it everything runs on the Master at about 12 fps).
The original GLB, its CC BY 4.0 attribution, and conversion details are in
[`assets/LICENSE.txt`](assets/LICENSE.txt).
