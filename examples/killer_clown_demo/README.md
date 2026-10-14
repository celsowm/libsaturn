# Killer Clown Octagon fight

The next performance and presentation sequence is tracked in the
[optimization plan](../../docs/KILLER_CLOWN_OPTIMIZATION_PLAN.md), starting
with stage profiling before lighting, paired geometry and asset/LOD changes.
The [measured baseline](../../docs/KILLER_CLOWN_PERFORMANCE.md) identifies
lighting as the largest current stage.

Two independently animated clowns fight inside the supplied MMA Octagon.
Play against the deterministic CPU opponent or switch to two physical controls.
The 60 Hz combat simulation has punches, kicks, guard, jump evasion, hit reactions,
body separation, health, a 60-second round, a winner and rematches. Animation
sampling, crossfades and CPU skinning use the existing skeleton/skin APIs;
the imported arena, both actors and shadows share one scene painter.
VDP1 uses `ON_SUBMIT` so a list is displayed only after drawing completes;
the CPU does not rewrite a table while the one-cycle renderer replays it.

```powershell
.\build-example.ps1 killer_clown_demo
.\run-example.ps1 killer_clown_demo -MednafenCart none
.\harness\run-killer-clown.ps1 -Bios bios/saturn_bios_us.bin
.\harness\run-killer-clown-smoke.ps1 -Bios bios/saturn_bios_us.bin
.\harness\run-clown-profile.ps1 -Bios bios/saturn_bios_us.bin
python tools/inspect_glb.py examples/killer_clown_demo/assets/killer_clown.glb
```

| Input | Action |
| --- | --- |
| D-pad | Move around the octagon |
| A | Punch |
| B | Hold guard |
| C | Kick |
| X | Jump |
| Y (player 1) | Switch CPU / two-player mode and reset the round |
| Z (player 1) | Cycle AUTO / MESH / SOLID / CUTAWAY fence modes |
| Start (player 1) | Open pause menu; rematch after the round ends |
| L+R (player 1) | Return to the main menu |

Seven resident clips at 20 samples/s cover idle, walk, jab, kick, head hit,
knockback and jump. Action playback speeds fit the existing combat action
durations; locomotion loops and attacks clamp. The source has 22 clips. Each
actor uses a 600-face mesh and 302 shared deformation records.
Both costumes share 41,216 packed LUT4 pixel bytes and 585 RGB CLUTs
(18,720 bytes). Resident clip poses occupy 242,880 bytes. The cage has 71
faces; floor and base faces are removed from VDP1 geometry.
The crowd uses 131,072 RGB555 cell bytes in NBG0. The mat uses 65,600
indexed cell bytes in RBG0, with separate maps, priorities and VRAM banks.
No expansion cartridge or per-frame texture upload is required.

The camera tracks the fighters' midpoint from a 5.5-unit base distance.
NBG0 repeats the seamless crowd
with horizontal zoom/parallax. RBG0 projects the original mat image using
inverse-perspective rotation and per-line Kx/Ky coefficients derived from
the same camera. Double-buffered rotation/coefficient tables avoid rewriting
active data. This replaces the earlier distorted VDP1 floor triangles.
The source cage is Z-up, so its instance rotates to Y-up.
Gameplay positions remain within the straight and diagonal cage boundaries.

## Menus and spotlights

The title menu offers Arcade, Versus, Options and How to Play. Start opens
a pause menu with Resume, Restart Round and Main Menu. L+R returns to the
title menu. Options select fence coverage and toggle blue/red spotlights.

Front/above fixtures aim at each fighter's animated torso height, estimated
from the current deformed bounds, with a small orbit in X/Z
and a vertical oscillation. The light direction, cone angle and height are
evaluated at each deformed vertex; positive Lambert terms and range/cone
attenuation produce RGB corrections on camera-facing surfaces as well.
`sat_mesh_vertex_normals_scaled` exposes the shared face accumulation without
normalization; the demo normalizes bounded Q10 components with 32-bit products,
divisions and square roots. The existing full-precision unit-normal API keeps
its output unchanged. Light attenuation uses bounded Q8 arena coordinates.
Textures and RGB CLUTs stay immutable. Each submitted face owns four
corrections, copied into an eight-byte VDP1 Gouraud table. CMDPMOD selects
color mode 1 plus Gouraud, CMDCOLR selects the RGB CLUT, CMDSRCA selects
packed texels and CMDGRDA addresses the table in eight-byte units.
Blue/red ambient corrections identify the fighters. Each fog shell has eight
sides and two depth sections, with camera-facing sides submitted separately
to the painter. Continuous INDEX8 gradients replace all mesh and stochastic
coverage in the light shells. VDP2 adds their dim RGB values to the background. This is
a geometric fog approximation, not a volumetric scattering or shadow-map
renderer. The visible beam, actor shading and ground intersection share the
same spotlight source and aim; they are no longer independent 2D effects.

Radial INDEX8 pools sit on the ground at the cone/plane intersection. VDP2
sprite color calculation adds these marked sprites to the RBG0 mat,
using `SAT_VDP2_SPRITE_ADD`. Ordinary RGB fighter/HUD pixels retain priority selector
zero and bypass this calculation. The pools use VDP2 composition rather
than VDP1 half-transparency; the mat detail remains visible through them.
The light geometry draws in pass zero before the cage, shadows and actors
in pass one. This preserves fighter pixels; their lighting comes from
Gouraud. VDP2 cannot blend with or recover overwritten VDP1 geometry.
Overlapping light sprites do not accumulate with each other in the VDP1
framebuffer; the last visible indexed pixel adds to the VDP2 background.
Color gradients still have the hardware's RGB555 quantization.
See SEGA's [sprite color calculation](../../docs/sega_saturn_hardware/hard/vdp2/hon/p09_20.md).

The primary source is SEGA's *VDP1 User's Manual*, ST-013-R3-061694:
[complete manual](https://manualzilla.com/doc/5653132/vdp1-user-s-manual),
[color calculation](../../docs/sega_saturn_hardware/hard/vdp1/hon/p06_37.md)
and [Gouraud tables](../../docs/sega_saturn_hardware/hard/vdp1/hon/p05_30.md).
RGB CLUT/LUT4 Gouraud works in the 16-bit framebuffer. This API rejects
color-bank/INDEX8 and the 8-bit high-resolution framebuffer. The
[minimal comparison](../lut4_gouraud/README.md) isolates the same path.
Generated skeletal C assets support LUT4; SKA1 archives retain their INDEX8
layout and explicitly reject LUT4 export rather than dropping CLUT data.
Animated/static-rest and skeletal LUT4 exports preserve masked texels as
code zero. Invisible colors do not consume CLUT entries; fully masked
patches remain transparent. The GLB reader applies `alphaCutoff` for
`alphaMode: MASK` before texture baking.

## Resident 68000 sound effects

Punch, kick and damage events play distinct short resident PCM8 effects.
A metallic double bell rings on round start and completion. Stereo pan
follows the fighter's horizontal position. `tools/generate_combat_audio.py`
builds the deterministic sample bank and standalone WAV previews; there
are no downloaded audio dependencies or per-frame sample uploads.

The SH-2 submits `SAT_SOUND_PLAY_AT_TICK` requests one tick ahead. The
resident SCSP 68000 driver performs key-on independently of rendering;
SCSP slots reproduce the samples. Both bell strikes are in one waveform,
avoiding a future FIFO entry that would delay intervening combat sounds.
Voice priorities favor damage and the bell over swings. Timed-play queue
capacity is checked before stealing/configuring a voice in the library.
Audio telemetry exposes effect counts, failures, actual 68000 executed
events, lateness and active/peak voices. See
[the SCSP guide](../../docs/SCSP_AUDIO_STREAMING_GUIDE.md).

## Dynamic fence coverage

The wire texture uses LUT4 cutout: code zero leaves genuine holes, with
SPD=0, ECD=1 and mesh disabled. AUTO hides camera-side wire faces between
the camera and either fighter, with a 1.5-unit depth margin. Rear cage
faces stay visible. MESH holds the front at 50% checkerboard coverage, SOLID uses
full wire coverage and CUTAWAY removes the front. MESH is an optional
screen-door effect on top of the holes; AUTO relies on the cutout itself.
Visibility/material selection changes without uploading textures again.
VDP1 half-transparency averages RGB pixels already in its own framebuffer;
it does not directly blend a fence with NBG0/RBG0. Cutout holes preserve both
previously drawn fighter pixels and the lower VDP2 layers where no sprite
pixel exists. Rear/side cutout faces remain in the shared painter order.

Generic `sat_scene3d_material_t.sprite_flags` and the indexed render parameters
support `SAT_SPRITE_FLAG_MESH` through projected, world/fallback and capture paths.
The reference is [VDP1 mesh processing](../../docs/sega_saturn_hardware/hard/vdp1/hon/p06_33.md).
The stage importer reads diffuse maps/factors from
[KHR_materials_pbrSpecularGlossiness](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Archived/KHR_materials_pbrSpecularGlossiness/README.md),
including alpha-mask cutoff. Source material baking and runtime Lambert
corrections approximate lighting; this does not implement a PBR shader.

## Validation

The profile runner records extended-FRT stage durations in a 192-row WRAM-L
ring, then analyzes fixed idle, walk and combat timelines. Reports are
`harness/build/clown_profile_*_profile.json`. Each uses two seconds of warm-up
and a ten-second NTSC measurement window. It reports median/p95/max timings,
submission throughput, simulation cadence and late swaps. Parent/child stage
timings overlap and must not be added together. `pose_dispatch` includes
synchronous Master skinning; `skin_join` measures residual wait and release.
The renderer's existing painter/emission measurements use a 16-bit counter.
These are instrumented loop measurements, not unique displayed-image FPS.
Set the build environment variable `CLOWN_PROFILE=0` to omit per-loop timing
and record writes for an overhead comparison; the library profile remains
enabled in both builds so that comparison isolates the example instrumentation.
The option does not change gameplay or renderer scheduling.
Use `-OutPrefix harness/build/clown_lighting_final` to preserve an earlier
baseline. `-Workloads ambient -SkipNoRecord` checks the lights-off path,
including zero normal-accumulation time and constant fighter Gouraud tables.
Compare matched runs with:

```powershell
python harness/tests/clown_profile.py harness/build/clown_lighting_final --compare-before harness/build/clown_lighting_before/clown_profile
```

The shared spotlight helper uses an exact reciprocal/correction table in
8 KiB of WRAM-L, preserving integer normalization while avoiding repeated
software divisions. See the [lighting measurements](../../docs/KILLER_CLOWN_PERFORMANCE.md#exact-lighting-arithmetic-follow-up).

The automatic runner exercises menus, spotlight toggles, CPU combat, body
contacts, all fence modes, pause/resume, identical paused images and a real
second-port pad timeline. It checks NBG0/RBG0 enable bits and actual
LUT4/Gouraud command fields.
It also captures stereo SCSP output, checks sample bytes in Sound RAM,
compares effect counts with events actually executed by the 68000, and
rejects silent/clipped output or queue failures. The analysis exports
`harness/build/killer_clown_fight.wav` for listening.
It checks guest panic, draw errors, body overlap, hits, blocks and worker jobs
without expansion RAM. Host collision tests cover coincident spawns at all four
diagonal corners. Submission telemetry counts completed lists, not unique
displayed images. Ymir injects the application after BIOS initialization;
Mednafen tests disc boot. Physical Saturn/Satiator acceptance remains pending.

Late swaps preserve the previous image while a list finishes; they do not
represent failed frames or unique presented-image FPS. Captures and matching
telemetry are under `harness/build/killer_clown_fight*`.

The 2,300-field gate passes menus, two controls, every fence mode and pause
equivalence with 270 completed submissions, 1,750 simulation ticks and zero
panic, draw errors, body overlap or VDP1 timeouts. The 68000 executes 75
effects, with no queue failure; captured stereo peak is 5,203/32,767.
Bounded normal normalization raises submissions from 233 to 270 over the
same input timeline. The final AUTO occlusion change is additionally checked
over 700 fields: 30 non-mesh cutout commands, 490 textured Gouraud commands,
35 executed sound events, no panic/errors/timeouts and peak 5,135/32,767.
Those figures precede the 3D spotlight follow-up. The new light gate also
checks camera-facing illuminated vertices, actor Gouraud correction ranges
in VDP1 RAM and two VDP2-blended floor pools. The 30-FPS presentation
target remains open. These are emulator measurements, not hardware acceptance
or unique displayed-image FPS. Final default captures and audio are under
`harness/build/clown_default*`; Mednafen disc boot is captured in
`harness/build/clown_final_mednafen.png`.

The continuous additive spotlight follow-up passes the 700-field smoke with jump/attack/guard
input: the final frame has 145 red-lit and 130 blue-lit camera-facing vertices,
16 visible fog-shell faces and two additive ground pools. Actor Gouraud tables
reach correction codes 28 in red and blue. No panic, draw error, body
overlap, sound-queue failure or VDP1 timeout occurs. That pre-optimization window reports
5.4 submissions/s, so the added lighting cost still needs optimization.
The current Mednafen capture is `harness/build/clown_add_mednafen.png`.
The smaller [colored light cubes](../colored_light_cubes/README.md) isolate
the same cone mathematics, LUT4 Gouraud and VDP2 additive composition.

The subsequent exact-normalization optimization reduces measured lighting
medians by 8–9%, with sustained full-light throughput at 5.97–6.04 submissions/s.
The updated 2,300-field fight gate passes all modes, pause and two controls
with zero panic, body overlap, draw error, timeout or sound-queue failure.
Its 57 late swaps preserve previous images. See the
[performance follow-up](../../docs/KILLER_CLOWN_PERFORMANCE.md#exact-lighting-arithmetic-follow-up)
for the matched inputs, timing limits and remaining targets. Current Mednafen
captures are `harness/build/clown_lighting_mednafen.png` and
`harness/build/clown_lighting_cube_mednafen.png`.

## Geometry preparation modes

The two fighters use separate projection/world scratch. With
`CLOWN_PARALLEL_GEOMETRY=1`, the second fighter is prepared by the existing
scene adapter on Slave while Master submits the first fighter and shadows.
Both skin handles are joined and released before this stage. Gouraud and
material inputs remain immutable until the geometry handle is joined, merged
and released. Stage, shadows and actor faces retain their original source
order; sorting and VDP1 command emission stay on Master.

The companion face/key arrays use WRAM-L. No allocation or additional texture
upload occurs during rendering. With no Slave available, preparation falls
back to the serial path. Set `CLOWN_PARALLEL_GEOMETRY=0` to force serial code.
`CLOWN_VERIFY_GEOMETRY=1` compares every worker result with serial preparation:
admitted/culled/clipped counts, painter keys, projected coordinates, material
fields, Gouraud values and world coordinates for clipping fallbacks. This
validation mode intentionally repeats work and is excluded from speed claims.

Reproduce the matched profiles with:

```powershell
.\harness\run-clown-profile.ps1 -Bios bios/saturn_bios_us.bin -GeometryMode Serial -OutPrefix harness/build/clown_geometry_serial -SkipNoRecord
.\harness\run-clown-profile.ps1 -Bios bios/saturn_bios_us.bin -GeometryMode Parallel -OutPrefix harness/build/clown_geometry_parallel -SkipNoRecord
.\harness\run-clown-profile.ps1 -Bios bios/saturn_bios_us.bin -GeometryMode Verify -Workloads combat -OutPrefix harness/build/clown_geometry_verify -SkipNoRecord
python harness/tests/clown_profile.py harness/build/clown_geometry_parallel --compare-before harness/build/clown_geometry_serial
```

The retained paired path reduces geometry medians from 37.9-38.5 ms to
26.5-26.6 ms, raising matched throughput from 5.97-6.04 to 6.41-6.49
submissions/s. These count completed lists, not unique displayed images.
The full 2,300-field verification gate compares 222 pairs with zero
mismatches, body overlap, draw error or timeout; menu, fence, pause, both
pads and 68000 sound gates pass. See the
[paired geometry report](../../docs/KILLER_CLOWN_PERFORMANCE.md#paired-geometry-preparation-follow-up).
The default build uses 324,732 bytes WRAM-L and leaves 3,688 bytes beyond
the reserved 16 KiB Master stack in WRAM-H.

The profiler restores the previous environment variables after each run.
Use the ordinary build script afterward to restore the default binary.

## Lighting preparation modes

`CLOWN_PARALLEL_LIGHTING=1` uses the existing range adapter to calculate one
actor per SH-2. Normal accumulation and per-vertex shading have separate
joined ranges; skin jobs finish first and geometry follows both joins.
Workers read fixed pose/camera/light inputs and write only their actor's
normal, Gouraud and counter output. Master combines the counters afterward.
The reciprocal normalization table is initialized on Master by aiming the
lights before any shading range. Concurrent first-use initialization is
unsupported. No worker draws or accesses graphics registers.

Matched idle/walk/combat profiles now measure 38.3-38.4 ms lighting and
8.50-8.54 submitted loops/s, a 31-33% throughput gain over paired geometry
with serial lighting. Meshes, texture pixels and lighting equations are
unchanged. This is submission throughput, not unique displayed-image FPS.
See the [parallel lighting report](../../docs/KILLER_CLOWN_PERFORMANCE.md#parallel-lighting-and-narrow-normal-arithmetic)
for stage timings, calibration quantization and remaining targets.

`CLOWN_PARALLEL_LIGHTING=0` forces serial lighting.
`CLOWN_VERIFY_LIGHTING=1` compares the second actor's actual output against
serial recomputation on every full-light update. The verification buffer
uses 4,248 bytes WRAM-L and the repeated work is excluded from speed claims.
The profiler accepts an independent `-LightingMode Parallel`, `Serial` or
`Verify` option and restores the previous environment afterward. For example:

```powershell
.\harness\run-clown-profile.ps1 -Bios bios/saturn_bios_us.bin -LightingMode Verify -Workloads combat -OutPrefix harness/build/clown_lighting_verify -SkipNoRecord
```

## Source provenance

The source was supplied locally as `Downloads/killer_clown.glb`. Its SHA-256 is
`677cbbfdb2096b0070b083f69f850c81b34e480a59ccd600a6ae18736dd17baf`.
The embedded metadata identifies `THREE.GLTFExporter r185`, but provides no
author, source URL or license. Attribution and redistribution terms are therefore
unverified; this document does not assign a license to the model.

"[MMA Octagon](https://skfb.ly/oJM7W)" by
[wesamtufail](https://sketchfab.com/wesamtufail) is licensed under
[Creative Commons Attribution 4.0](http://creativecommons.org/licenses/by/4.0/).
See [the stage attribution and modifications](assets/OCTAGON_ATTRIBUTION.md).
The user also supplied `plateia_3d_vdp2_rgb555.png` as the seamless crowd backdrop.
