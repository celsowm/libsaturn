# Dino Demo

## Runtime regression and fatal-error diagnostics

`g_dino_diagnostics` exposes completed frame count, submitted/completed pose
jobs, serial pose fallbacks, submitted/merged geometry jobs, draw overflow,
music error and Master/Slave/failed task counts. The fatal report remains in
`g_sat_last_panic`; the runtime gate checks both structures on stock RAM.

```powershell
.\harness\run-dino-runtime.ps1 -Bios bios/saturn_bios_us.bin
.\run-example.ps1 dino_demo -MednafenCart none
```

The automated gate builds through the project script, exercises pause/resume,
orbit, zoom, light toggles and reset for 1,200 Ymir fields, and checks successful
pose/geometry jobs, zero task failures, draw overflow, music error and panic.
Artifacts and fresh captures use `harness/build/dino_runtime*`. A snapshot may
contain one in-flight pose or geometry job; the analyzer allows that ownership
boundary instead of requiring all counters to match during a frame.

The generic executor now retains Slave ownership until its completion message
is consumed. An early terminal slot cannot be read/released or satisfy a
dependency before that acknowledgment. This fixes a race exposed by continuous
character verification. It does not establish that every reported dino fatal
has the same cause: preserve the report's file, line and result code when a
new failure occurs. A fresh build opened in both emulators, and Ymir passed the current
control scenario. Audio format, pitch and mixer settings are unchanged.

An interactive Sega Saturn take on the original PlayStation T-Rex tech demo.
It converts the attributed walking GLB into a bounded, baked animated model
and runs it through the Saturn's VDP1 painter path.

The dinosaur starts in a large side view against a dark screen, with its
original warm texture and a compact control display. L/R rotate the camera,
UP/DOWN change the viewing angle, X zooms in, and Y zooms out. A pauses the
walk, B resets the camera, and C toggles slow automatic orbit. Z toggles the
red light. START opens or closes the full help overlay; the performance
counters remain visible.

## How it stays close to the source

- **A red light passing over the body, like the PS1 demo's.** Hi-res VDP1
  pixels are palette codes, so the VDP1 cannot Gouraud-shade them; the VDP2
  lights them after they are drawn. Its line colour screen holds one red per
  screen line (a soft bell that sweeps up and down and pulses), inserted as
  the second image under the sprite layer only, and add-mode sprite colour
  calculation adds it to the model. The black background and the white HUD
  never change. The light varies only from line to line, so it is a
  horizontal band; a spot would need a window, whose edge is hard.
  Hi-res mixes palette images only in CRAM mode 0, so the demo selects it.
  `examples/vdp2_hires_light` shows the technique with a spot and a
  per-polygon lit flag.

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

## Display mode

The demo always runs 640x224 hi-res: twice the horizontal pixels on the same
4:3 screen. The VDP1
framebuffer is then 8 bits per pixel and can only hold palette codes, so the
whole model shares one palette of 252 colors (`MODEL_LUT_CODES 2-253`),
each face's lookup table holds the codes of its 15 colors, and faces bake
at `MODEL_TEXEL_EXTENT 480`. Code 0 is transparent, code 1 is the HUD's
white and 254 is the sprite shadow code. The HUD font doubles each glyph
horizontally so the text keeps its size.

## Loading bar

Setting up the font, uploading the model and reading the soundtrack samples
from the disc takes a couple of seconds, so the demo shows the stock
`sat_loading_screen_progress` bar meanwhile instead of a black screen. The
8-bit hi-res VDP1 cannot draw RGB, so the bar is given palette codes: white
and a track blue live in the CRAM bank after the model's, which the sprite
palette selects until loading ends. The music files (the slow part) advance
the bar through `dino_music_68k_start`'s progress callback. The
`made using celsowm/libsaturn` credit is drawn at the bottom of both the
loading screen (through the screen's `decorate` hook) and the demo, whether or
not the help overlay is open.

## Both SH-2s

The face list is split between the CPUs: the Slave prepares 40% of the faces
while the Master prepares the rest, and the Master merges both into one
painter queue. `MODEL_LOCALITY_ORDER` orders faces along the body so each
half projects only its own window of vertices. While the Master sorts and
emits, the Slave decodes the next pose.

See the [shared renderer refactor and measurements](../../docs/SHARED_RENDERER_PERFORMANCE.md)
for the common visibility improvement and reproducible animated/paused controls.

The demo runs in `SAT_VDP1_FRAME_ON_SUBMIT` mode (build with
`-DDINO_FRAME_ON_SUBMIT=0` for the 1-cycle default), so each list is shown only
once it has fully drawn. The HUD's `LATE SWAPS` is
`sat_vdp1_frame_stats().late_swaps`, the VBlanks at which a list was still
drawing. There is no FPS readout. Measure a warmed interval with the harness
and inspect an actual Mednafen CD recording; cadence depends on the workload,
cache settings and emulator model. CPU submissions and distinct presentation
are different measurements.

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

## Satiator boot investigation

Set `DINO_LOADING_SCREEN=0` in the build environment to isolate loading-screen
startup work. The default is `1`. Use
`./harness/build-dino-satiator-triage.ps1` to prepare controlled ISO comparisons.
The official default boot waits for `HIRQ.CMOK` before reading CD command
results. `legacy_poll.iso` preserves the previous counter-based hook for
regression comparison. Findings, download hashes and hardware comparison
instructions are in [the investigation](../../docs/DINO_SATIATOR_BOOT_INVESTIGATION.md).
Ymir runtime captures skip the BIOS executable load and do not certify Satiator boot.

The scene partitions use immutable topology bindings initialized after material
setup. Decoded pose buffers alternate independently of those bindings, so both
SH-2s skip repeated index/material-map validation. Recreate the bindings if the
index arrays, material table or face-material map change. See the shared
[renderer measurements](../../docs/SHARED_RENDERER_PERFORMANCE.md) for measured
control and animation cadence.
