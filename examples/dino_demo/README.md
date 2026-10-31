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
It converts a GLB with the recovered original Walk into a bounded, baked animated model
and runs it through the Saturn's VDP1 painter path.

The dinosaur starts distant and dark, walks toward the camera over six seconds,
and gradually enters the light. The camera starts directly in front (yaw 0, pitch 0)
to show the approach, with the original warm texture and a compact control display. L/R rotate the camera,
UP/DOWN change the viewing angle, X zooms in, and Y zooms out. A pauses the
active clip, B resets the camera, returns to Walk and replays the entrance, and C toggles slow automatic
orbit. LEFT selects Walk and RIGHT selects Roar. The red light starts on
and Z toggles it. Camera, pause and clip controls immediately finish the entrance so
the viewer remains responsive. START opens or closes the full help overlay; the performance
counters remain visible.

The normal camera uses the original PS1 Dino's projection and target, at 75% of
its measured default distance (`DINO_DEFAULT_ZOOM_PERCENT`; set `100` for the
exact PS1 endpoint) so the model fills more of the screen. B restores that
framing.
This calibration is part of the production viewer, rather than a screenshot
override. See [endpoint validation](../../docs/DINO_CAMERA_ENDPOINT.md).

## Model and attribution

The active asset, `assets/ps1_trex_saturn_original_walk.glb`, preserves all
2724 triangles, corner UVs and 1376 logical points of the original source OBJ.
It uses the recovered original Walk, without Blender decimation or the
reconstruction's mouth/teeth additions. The six source texture pages remain
embedded. The original material names and UV layout are retained.

`assets/ps1_trex_reconstructed.glb` is an unchanged archive of the editable
Blender reconstruction by celsowm. Its reconstructed Roar displacements are
transferred to the original surface; Roar remains approximate. The former
reduced transfer input, `assets/ps1_trex_saturn.glb`, remains archived for
comparison and is no longer the active model's geometry source.

`tools/bake_ps1_dino_walk.py --original-topology` verifies the local emulator
capture over 64 updates with zero integer-coordinate error, maps every source
point, and writes standard GLB animation. Synthetic translation joints are
only an interchange format: Saturn receives baked vertex streams and does not
run the PS1 animation engine. UV and pose roundtrips are checked on export.
Independent blink and interactive deformation channels are excluded from Walk.

```sh
python3 tools/bake_ps1_dino_walk.py \
  --capture harness/build/dino_motion --obj "$DINO_SOURCE_OBJ" \
  --mesh examples/dino_demo/assets/ps1_trex_reconstructed.glb \
  --original-topology \
  --output examples/dino_demo/assets/ps1_trex_saturn_original_walk.glb \
  --report harness/build/dino_motion/transfer.json
```

Walk retains 32 poses at 24 Hz and Roar 20 at 15 Hz on the 1.333-second
interchange timeline. `DINO_WALK_SPEED_PERCENT=140` preserves the approximate
0.95-second cycle calibrated against PCSX-Redux; it is not a hardware timing
measurement. Both streams occupy 429312 bytes.

The importer recognizes shared edges across normal-split copies only when
UVs and full motion also match. It merges 825 pairs into quads, yielding
1899 VDP1 faces without reducing geometry. Estimated command headroom is 82.
Regression tests ensure UV and motion seams remain separate.

Sony's artwork is credited on screen and in [`assets/LICENSE.txt`](assets/LICENSE.txt).
It is separate from the code license. The older Sketchfab asset and its
attribution remain archived. See [the reference procedure](../../docs/DINO_PS1_REFERENCE.md).

## How it stays close to the source

- **Smooth light without hardware Gouraud.** The importer averages normals
  over shared moving points, then bakes interpolated light into each face's
  indexed texture before palette quantization. The shoulder and back get a
  stronger key light while the underside stays shaded. This is fixed in asset
  space and does not track a moving light or camera. Geometry, UVs and pose
  counts stay the same. It is an approximation of the original's volume, not
  an exact reproduction of its live Gouraud lighting.
- **An optional red light on Z.** Hi-res VDP1
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
  the T-Rex spans about 540 pixels (`MODEL_TEXEL_EXTENT`), box-filtered from
  the GLB's six 256x256 texture pages (`MODEL_SAMPLING area`): smooth at the default
  view, still detailed at full zoom.
- **15 colors per face.** Texels are 4-bit with a VDP1 lookup table per face
  (`MODEL_TEXTURE_FORMAT lut4`), fitted to that face's patch of the texture.
- **Quads, not triangles.** Adjacent triangle pairs whose texture mapping and
  fold survive the change draw as one VDP1 quad (`MODEL_MERGE_QUADS`).
- **Original geometry within the command budget.** The active original mesh retains 2724
  triangles before VDP1 quad pairing. The
  HUD reports dispatched faces and VDP1 world commands for each frame.
- **Source materials.** The six original texture pages share the indexed
  palette. Solid-color faces use palette codes in the hi-res framebuffer.
- **One vertex per moving point.** UV-split vertex copies are welded
  (`MODEL_WELD_VERTICES`), reducing the runtime pose and projection workload.

## Display mode

The demo runs 640x448 double-density interlaced hi-res: both viewport axes
are doubled on the same 4:3 display. The VDP1
framebuffer is then 8 bits per pixel and can only hold palette codes, so the
whole model shares one palette of 252 colors (`MODEL_LUT_CODES 2-253`),
each face's lookup table holds the codes of its 15 colors, and faces bake
at `MODEL_TEXEL_EXTENT 540`. Code 0 is transparent, code 1 is the HUD's
white and 254 is the sprite shadow code. The HUD font doubles each glyph
in both axes so the text keeps its display size. Projection retains the
320:224 display aspect; 448-line coordinates are split into even/odd fields.
The library keeps automatic field swaps and two immutable command banks in
VDP1 VRAM. A root jump publishes a complete bank while the other keeps drawing;
stopping automatic drawing during a table upload would expose erased fields.
The 448-line mode requires SCU VBlank interrupts and rejects ON_SUBMIT mode.

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
The active GLB provenance, Sony attribution, legacy CC BY 4.0 attribution, and conversion details are in
[`assets/LICENSE.txt`](assets/LICENSE.txt).

The default soundtrack is the complete clean Demo 1 music recording selected
by the user: [Demo 1 - T-Rex Demo Music (OST)](https://www.youtube.com/watch?v=N1LF8YjifOc).
It plays from audio track 2 as 44.1 kHz stereo 16-bit CD-DA. The source is
already YouTube AAC; the build adds no lossy encoding, gain changes, remix,
trim or crossfade. It preserves the complete recording and pads the final CD
sector with 320 silent stereo frames (about 7.3 ms).

Launch the CUE, which includes the audio track. The ISO contains only the data
filesystem. The drive repeats an explicit FAD range obtained from its TOC;
SCSP EXTS0/1 route left and right at unity. No PCM decoding, score execution or
data reads are needed during the scene. This matches the supplied recording,
not a bit-exact pristine Sony disc master. Source provenance is in
`audio-src/ps1_demo1_reference.provenance.json`; digital-output validation is
recorded in `harness/studies/dino-demo1-audio.json`.

The former 96-second sparse score remains available with
`DINO_MUSIC_CDDA=0 DINO_MUSIC_68K=1`. It runs autonomously on the SCSP's 68000.
The build takes four short timbres from
`audio-src/Bone_and_Plastic.ogg`, converts them to 11.025 kHz mono PCM8, and
loads them into Sound RAM at boot. The 68000 then changes their level and
pitch on its own timer. There are no CD reads, decoding jobs or SH-2 music
updates during gameplay. The musical events remain sparse: low drone, hollow
figure, isolated impacts and a few high signals. These are samples from the
original recording rather than chip waveforms.

The former full recording remains available in 22.05 kHz stereo IMA ADPCM. Set
`DINO_MUSIC_CDDA=0 DINO_MUSIC_68K=0` when building to use that CD stream
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


## Entrance presentation

The opening translates the model from behind the orbit target toward its final
position along its forward (+Z) axis. A six-second smoothstep uses video fields,
independent of render throughput; the first second stays dark and the next four
raise the model palette to full brightness. The optional red additive light uses the
same reveal gain, while the background stays black and HUD palette code 1
stays white. These are presentation timings, not recovered PS1 script timings.

Both SH-2 face submissions have separate world-transform scratch buffers during
the entrance. At its endpoint the instances return to the original identity
path. The paused benchmark configuration skips the entrance. Stock RAM and
640x448 interlaced hi-res remain enabled (earlier entrance captures used 224 lines).

Ymir artifacts are under `harness/build/dino_intro/`: `entrance-stages.png`
shows the reveal, `replay_*.png` covers B replay and camera interruption, and
`runtime_analysis.json` records the 1200-field control regression. The neutral
capture starts with zero nonblack model pixels and reaches over 17000; both
normal completion and camera interruption end at full brightness with the
intro inactive. The previous reduced-mesh runtime validation completed 520 frames with zero panic,
draw overflow, failed tasks or music errors.


## Original-surface validation

The current original-surface build uses 1376 animated points, 2724 source
triangles and 1899 VDP1 faces. Texture density is 540, with 250696 bytes of
texture VRAM. The 960-density option exceeded the VRAM gate; 720 fit VRAM
but exceeded High WRAM. Density 540 passes both memory gates while retaining
the original geometry and the existing 32/20 pose counts on stock RAM.

The neutral Ymir capture verifies yaw/pitch `(0, 0)` in guest RAM. The same
1200-field control sequence completed 467 frames, 467 pose jobs and 934 Slave
tasks with zero panic, draw overflow, task failures or music errors. The
previous reduced three-quarter build completed 520 frames; the added original
geometry has a throughput cost, and the view has also changed. These are
emulator observations, not hardware performance figures.

Screenshots and runtime analysis are in `harness/build/dino_quality/`.
The renderer uses its indexed hi-res palette with smooth light baked into
the textures. The red VDP2 sweep is off initially and available on Z. It does
not reproduce the PS1 demo's live Gouraud lighting or identical rasterization.

The [lighting study](../../docs/DINO_TEXTURE_LIGHTING.md) shows matched-pose
before/after captures and records the final combined music/render validation.


## Native desktop preview

On macOS/Linux, `scripts/run-dino-mednafen.sh` launches the existing disc with
SDL software presentation, normal speed throttling, aspect correction and 2x window scale. Display interpolation is disabled
by default so the 640x448 console raster remains visible. `MEDNAFEN_BIN` can
select an installed executable; extra arguments override defaults. This is the
native fallback when PowerShell is unavailable. The Windows launcher accepts
the same Mednafen options through `-ExtraArgs`.

The earlier 640x224 build used a filtered preview to soften enlarged pixels;
that display-only workaround did not add console detail. The current build
renders distinct even/odd lines. Retina window captures still have twice their
logical window dimensions and must not be mistaken for console resolution.

Interlaced movement can show comb edges with weave deinterlacing, especially
when a new animation pose arrives between fields. This is separate from blank
fields or text flashing. Hardware output has not been tested on a Saturn.

## 448-line validation

The 1200-field Ymir control run completed 410 render loops with zero command
overflows, task failures, panics or VDP1 wait timeouts. A pending final pose can
remain in flight at the end of the capture. The earlier 224-line full-surface
run completed 467 loops under the same scripted fields; these are emulator
observations, not physical Saturn performance measurements.

Mednafen 1.32.1 internal recording was checked over 240 consecutive fields:
HUD, credits and model regions never became blank. The recording uses the SDL
software backend; the final interactive launch uses that backend too. Raw large
recordings were removed after extracting the four-second preview and counts.
Artifacts are in `harness/build/dino_interlace/`; the persistent result and app
hash are recorded in `harness/studies/dino-448i.json`.

`ps1-saturn-profile.png` compares lateral views at the same 4:3 display size,
using nearest-neighbor scaling. The Saturn camera was verified at yaw 90,
pitch 0 for this reference capture, then restored to yaw 0 for the shipped
entrance. Animation phases and projection differ; it is not a pixel-error test.

The newer [controlled comparison](../../docs/DINO_FAIR_COMPARISON.md) matches
four original Walk poses in front and profile views, using the original GTE
camera projection. It includes full viewports, matching detail crops and fresh
timing samples of the unmodified build. Images and checks are under
`harness/build/dino_fair_comparison/`.

The publication wait uses the interrupt-driven VBlank counter, which continues
while the CPU builds a scene. The SCU `frame_counter()` counts explicit waits
and cannot safely represent a field that elapses inside this publication wait.

The launcher explicitly passes `-nothrottle 0`. Mednafen persists command-line
settings, so a previous no-throttle capture can otherwise accelerate a later
interactive launch when audio output is disabled. This setting changes emulator
pacing only; the recovered Walk timing remains unchanged.
The native launcher also explicitly enables sound, so a muted test configuration
cannot silently carry over into the next preview.
