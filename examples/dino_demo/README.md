# Dino Demo

An interactive Sega Saturn take on the original PlayStation T-Rex tech demo.
It converts the attributed walking GLB into a bounded, baked animated model
and runs it through the Saturn's VDP1 painter path.

The dinosaur starts in a large side view against a dark screen, with its
original warm texture and a compact control display. L/R rotate the camera,
UP/DOWN change the viewing angle, X zooms in, and Y zooms out. A pauses the
walk, B resets the camera, and C toggles slow automatic orbit. START opens or
closes the full help overlay; the performance counters remain visible.

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
  fold survive the change draw as one VDP1 quad (`MODEL_MERGE_QUADS`).
- **Lower face count for hardware.** The current target is 2140 simplified
  triangles, yielding at least 10% fewer faces than the original build. The
  HUD reports dispatched faces and VDP1 world commands for each frame.
- **Triangles where they show.** The 46 teeth and claws are a third of the
  source mesh but a few pixels on screen; `MODEL_MATERIAL_WEIGHTS` spends
  that budget on the textured body. Solid-color faces draw as RGB polygons.
- **One vertex per moving point.** UV-split vertex copies are welded
  (`MODEL_WELD_VERTICES`), about 1300 runtime vertices instead of 1900.

## Display modes

The demo defaults to 320x224 to leave the VDP1 enough time to finish each
frame. Set `DINO_HIRES=1` when building to
use 640x224: twice the horizontal pixels on the same 4:3 screen. The VDP1
framebuffer is then 8 bits per pixel and can only hold palette codes, so the
whole model shares one palette of 252 colors (`MODEL_LUT_CODES 2-253`),
each face's lookup table holds the codes of its 15 colors, and faces bake
at `MODEL_TEXEL_EXTENT 480`. Code 0 is transparent, code 1 is the HUD's
white and 254 is the sprite shadow code. The HUD font doubles each glyph
horizontally so the text keeps its size.

## Both SH-2s

The face list is split between the CPUs: the Slave prepares 40% of the faces
while the Master prepares the rest, and the Master merges both into one
painter queue. `MODEL_LOCALITY_ORDER` orders faces along the body so each
half projects only its own window of vertices. While the Master sorts and
emits, the Slave decodes the next pose. The HUD reports the measured game FPS.

Build with `.\build-example.ps1 dino_demo`. Run it in Ymir's modified
integration harness with `.\harness\run-harness.ps1 dino_demo -Bios <your-BIOS>`
(the harness hands the Slave its entry point, which the BIOS normally does;
without it everything runs on the Master at about 12 fps).
The original GLB, its CC BY 4.0 attribution, and conversion details are in
[`assets/LICENSE.txt`](assets/LICENSE.txt).

The default soundtrack is a 96-second sparse score played autonomously by the
SCSP's 68000. The build takes four short timbres from the original
`audio-src/Bone_and_Plastic.ogg`, converts them to 11.025 kHz mono PCM8, and
loads them into Sound RAM at boot. The 68000 then changes their level and
pitch on its own timer. There are no CD reads, decoding jobs or SH-2 music
updates during gameplay. The musical events remain sparse: low drone, hollow
figure, isolated impacts and a few high signals. These are samples from the
original recording rather than chip waveforms.

The full recording remains available in 22.05 kHz stereo IMA ADPCM. Set the
environment variable `DINO_MUSIC_68K=0` when building to use the CD stream
instead; its underrun counter replaces the 68000 event/loop counters on the
HUD. The build converts and stages the selected option automatically using FFmpeg.
The shared build supports S16BE, S8 and IMA ADPCM in other examples too.
