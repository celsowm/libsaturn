# Shmup / bullet-hell support

What LibSaturn gained to carry a vertical shoot-'em-up with 128-1024 simultaneous projectiles at
60 fps (the target is a future DoDonPachi-class port), what was measured, and what is still open.
Everything below is emulator evidence (Ymir and Mednafen). Nothing was run on physical hardware.

The demonstrator is `examples/bullet_hell_stress/`: three stages, a boss, pickups, a HUD, a
deterministic simulation and a scripted benchmark. All game logic stays in the example; the library
pieces are generic.

## What was added

| Piece | Where | Purpose |
| --- | --- | --- |
| Asset gate + packer | `tools/shmup_assets.py`, `tests/test_shmup_assets.py` | Reject defective sprite packs; build the SATL atlas and SBG1 backgrounds |
| Batched VDP1 submission | `include/saturn/sprite_batch.h`, `src/graphics/2d/sprites/batch.cpp`, `hal::vdp1::push_sprite_batch` | One call per frame for thousands of sprites, or a direct-write window (`sat_sprite_batch_begin`/`emit`/`end`); 4 longword stores per command |
| Sprite atlas / residency | `include/saturn/sprite_atlas.h`, `src/graphics/2d/sprites/atlas.cpp` | Upload each frame once, residency groups in VRAM slots, chunked upload for streaming |
| TATE layout | `include/saturn/tate.h`, `src/graphics/2d/tate.cpp` | Logical 224x448 playfield to the screen, CW/CCW, exact edge mapping, HUD panels |
| Hardware TATE | `include/saturn/tate_fb.h`, `src/graphics/vdp2/tate_fb.cpp`, `sat_tate_fb_rotation_table`, `examples/tate_fb_rotation` | VDP1 rotation framebuffer drawn upright, turned and scaled by VDP2 rotation parameter A; every 2D call works in the 224x448 surface |
| Asynchronous pad poll | `sat_input_poll_begin` / `sat_input_poll_finish`, `hal::smpc` | The SMPC INTBACK runs while the frame is simulated |
| Simulation | `examples/bullet_hell_stress/bh_sim.[ch]`, `tests/host/test_bh_sim.cpp` | Fixed pools, 16.16 math, deterministic LCG, `sat_spatial` collision |
| Disc scheduler | `include/saturn/disc_stream.h`, `src/storage/cd/disc_stream.cpp` | One drive, one segment in flight: streamed music first, data jobs (file extent + sink callbacks) in the gaps |
| Streaming | `examples/bullet_hell_stress/bh_stream.[ch]`, `tools/shmup_music.py` | Next stage's sprites and background read from disc during play through the scheduler; music fed from the same drive |
| Acceptance | `harness/run-bullet-hell.ps1`, `harness/tests/test_bullet_hell.py` | Ymir run, telemetry and screenshot checks |

## Asset quality gate

`python tools/shmup_assets.py validate <pack>` exits 1 on any defect:

- opaque pixels inside the 2 px transparent border (a clipped wing), non-binary alpha (halo), stray
  connected components (neighbour fragments), off-centre pivots, widths that are not multiples of 8
  or heights above 255 (VDP1 character rules);
- a raw VDP1 `index8` file that disagrees with its PNG (size, transparency mask, index >= 254);
- manifest/file mismatches, overlapping atlas rects, LUT file sizes, palette sanity;
- colour quantisation error per sprite against a budget (reported, never silently accepted).

Measured: the v3 ship pack is rejected (12 sprites, 33 defects: clipped wings, fragments, alpha halo,
off-centre pivots); the v4 pack, 146 runtime sprites, passes with 0 defects and is vendored (runtime
subset only, `reference_only/` sheets excluded) under `examples/bullet_hell_stress/assets/`. The
tests prove the gate rejects synthetic defects (clipped wing, fragment, bleed, bad bin).

Known quality notes, not rejections: 19 green/red/orange sprites quantise to the one global palette
with mean RGB error 12-14.5 / 255 (budget 16; `validate` prints each above 12 as a `warn` line, and
`test_committed_v4_subset_passes` pins that they are the only findings; the shared palette is the
supplier's, and re-fitting it would shift every sprite, so it is left as delivered); `pickup_shield_*` are intentionally two components; enemies and
bosses are placeholders by the pack author's own description.

## Rendering path

The example builds an array of `{x, y, frame}` instances for everything but the enemy bullets and
calls `sat_sprite_batch_draw`; the bullets, which are most of the list, are written straight into
the frame's command list through a window (`sat_sprite_batch_begin`, `sat_sprite_batch_emit` per
bullet, `sat_sprite_batch_end`), one pass over the pool with no instance array in between. Both
paths write the same commands, in the order they are emitted (the host test compares them byte for
byte). Frame descriptors are prepared once from the atlas.

- **A frame is 16 bytes** (`sat_sprite_frame_t`): its first twelve bytes are the VDP1 command's own
  (`ctrl`, `link`, `pmod`, `colr`, `srca`, `size`), then the pivot. A command is three longwords
  copied from the frame plus one longword for the position; the width and height come from `size`
  (`sat_sprite_frame_width/height`). The vertices B, C and D are not written: a normal sprite
  does not read them (VDP1 manual 7.4), and CMDGRDA only matters with Gouraud shading.
- **The copy to VRAM stores 16 bytes per sprite command** (`src/hal/vdp1/submit_copy.hpp`, tested
  against a reference image), not 32. Every store to VDP1 VRAM is a slow bus cycle, so this halves
  the copy that ends the frame. Any command that is not a plain normal sprite is copied whole.
- **Pre-clipping disabled for sprites wholly on screen** (CMDPMOD bit 11, manual p06_32: the per-line
  pre-clip test costs up to 5 clocks per line on small sprites). A frame flagged
  `SAT_SPRITE_FLAG_USER_CLIP` or flipped keeps pre-clipping.
- **Longword stores.** Writing a command as longwords instead of halfwords cut the draw stage and took
  the Mednafen 512-bullet case from 28 to 50 fps.
- **Playfield clip.** The field is a 448x224 rectangle between two 96 px HUD panels. Sprites that
  cross its edge (an enemy flying in, a shot leaving) use a user-clipped copy of their frame; bullets
  and sprites well inside use the plain frame. No measurable cost on Mednafen.
- **Command budget.** The command area holds 2048 commands. 1024 bullets plus the player, shots,
  enemies, explosions, setup and about 45 HUD glyphs use 1097; raising 2048 needs moving the texture
  base and is out of scope.
- **SH-2 traps found on the way.** `x >> 8` on a signed value is a libgcc call (there is no
  arithmetic shift by 8), twice per bullet; the bullet builder biases the value and uses a logical
  shift. A window struct whose address escapes lives in memory; the loop works on a local copy.
  Frame longword reads need 4-byte alignment (`sat_sprite_frame_t` is declared aligned): a
  misaligned longword read blanked the screen on Mednafen.

## TATE

Logical playfield 224x448, upright. `sat_tate_layout_init` maps it to the screen and returns up to
two side panels; coordinates are pixel *edges* so rectangles land on exactly the pixels of their
logical pixels and neighbours touch with no gap. Sprites and backgrounds are rotated offline
(`--orientation cw`), so drawing is the same VDP1/VDP2 path as an upright game and a bullet maps with
two additions.

Limits found for 640x224 (the only mode that holds the whole 448-line field 1:1):

- Hi-res forces 8 bpp VDP1 (palette codes only): one shared sprite palette, no tint, no Gouraud, no
  RGB LUT entries. LUT4 sprites are therefore not usable there; the v4 pack's single global
  palette was built for exactly this.
- CRAM mode 0 gives four banks of 256 colours, shared between the sprite palette and the
  background slots (the stage background is a second 8 bpp palette, swapped with the stage).
- The field is 448x224 with 96 px panels; the top panel is 224 px long (24 glyphs of the 9 px HUD
  font), which bounds what the benchmark overlay can show.
- 320x224 needs 5/7 scaling (`tate.h` computes it, `exact = 0`): done by the hardware backend below,
  not by the software layout.

### Hardware TATE (`sat_tate_fb_init`)

The second backend keeps the 320x224 screen and draws the field **upright**. `sat_tate_fb_init`
(`include/saturn/tate_fb.h`) puts the VDP1 in rotation-8 mode (TVMR TVM = 011b: a 512x512 frame
buffer of 8 bits/pixel), and the VDP2 reads it back through rotation parameter A, turned a quarter and
scaled 5/7, so the 224x448 field shows as 320x160 centred in the 224 lines. The parameter table comes
from the pure function `sat_tate_fb_rotation_table` (host-tested against `sat_tate_unmap_edge`).
Afterwards `sat_video_width()/height()` are 224/448 with the origin at the centre, so the sprite
batch, clip, HUD and 2D polygon calls need no change; sprites need no offline rotation
(`--orientation none`).

How it is built, what was learned:

- The VDP2 evaluates rotation tables only while RBG0 or RBG1 is on, so the library enables a dummy
  RBG0 (128 KiB bitmap, cleared; transparent code on; screen-over transparent, because a 512x256 bitmap
  leaves rows beyond 256 undefined).
- SPCTL has to be sprite type C with an 8 bit frame buffer; the library's SPCTL writes
  (`hal::vdp2::set_sprite_fb8`) know about it, so `sat_vdp2_layers_commit` keeps it.
- **Erase.** The VDP1 manual (4.1) says a rotated frame buffer can only be erased or written during
  vertical retrace, and that areas the erase cannot cover must be filled with polygons. Mednafen
  erases only part of a 512-wide window (rows below ~244 kept stale pixels), which showed as a black
  block outside the field; Ymir erases everything. A hardware erase window of exactly the field was
  tried and still left stale pixels outside it on Mednafen. The library therefore turns the hardware
  erase off (EWRR: a 16x1 window) and starts each list with a clearing polygon over the whole field
  (and, for the first four frames, over the visible 320 px to wipe the boot-time frame buffer).
- **Cost.** That polygon is ~100,000 pixels of VDP1 fill per frame (~22% of the budget).
- 8 bpp: palette codes only, one shared palette, no Gouraud, no colour calculation, no tint (the 2D
  layer rejects it), no LUT4 sprites; the same limits as hi-res.
- 5/7 scaling is nearest-neighbour, so small sprites decimate; a 1:1 field would not.

Measured with `BH_TATE_FB=1` (the bullet-hell stress with the field drawn upright, no scrolling
background, the HUD inside the field):

| Bullets | Mednafen fps (hardware TATE) | Mednafen fps (hi-res software TATE) |
| --- | --- | --- |
| 128 | 59 | 60 |
| 256 | 60 | 60 |
| 384 | 58 | - |
| 448 | 28 | - |
| 512 | 28 | 53 |
| 640 | - | 43 |
| 1024 | ~28, HUD at the end of the list cut off | 28 |

Ymir (256 bullets): 363 commands, 0 late swaps, 0 music underruns, 60 fps. The field-sized clearing
polygon moves the knee from ~640 bullets (hi-res) down to ~400, so hardware TATE suits games that
need the 320-wide mode, fewer simultaneous bullets, or no offline-rotated art; the software hi-res
path is the better choice for 512 and above.

Limits of the hardware backend: single-density (320x224 `sat_init`) only, 128 KiB of VDP2 VRAM for
the dummy bitmap, the field-sized erase polygon, 5/7 decimation, and no scrolling VDP2 background in
the `BH_TATE_FB` variant (RBG0 is taken; a game would put the backdrop in NBG layers, which this
example does not do).

## Texture residency and streaming

`SHMUP.ATL` holds every frame once (content-hash dedupe), a shared palette and LUT table, and
residency groups whose data is 2048-byte aligned, so one stage's enemies can be read from disc by
sector while the previous stage plays and written into a spare VRAM slot with
`sat_sprite_atlas_upload_chunk`. Group 0 is the common art; groups 1-3 are the stage enemy sets.

The scheduler is library code, `sat_disc_stream` (`include/saturn/disc_stream.h`); `bh_stream` in the
example only says what this game's data is for: a data job is a file extent plus three callbacks
(`begin` names the extent, `chunk` takes the payload a segment at a time, `done` reports the end),
and the demo has two, the atlas group (to `sat_sprite_atlas_upload_chunk`) and the SBG1 background
(to CRAM and VDP2 VRAM). The rules:

- One 16 KiB segment (8 sectors) is in flight at a time; once a frame the program lands what has
  arrived and starts the next. Nothing waits for the drive.
- The music (a looping 22.05 kHz 8-bit stereo track, 564,480 bytes) is fed to two SCSP stream rings
  from the same drive. It has priority: a data segment only starts while both rings hold at least
  8192 frames (docs/SCSP_AUDIO_STREAMING_GUIDE.md: the SCSP half is 4096 samples).
- `sat_music` itself reads the disc synchronously, so a refill waits for the drive; that alone took
  the demo from 60 to about 40 fps, which is why the music is fed here.
- Stage 2's group and background are queued during stage 1 and swapped at the transition: the
  background lands in the other VDP2 bank and CRAM bank, scroll and bank switch happen in VBlank.

Measured on Ymir with the stage-skip pad script: 1 stage swap, 0 transition stalls, 0 stream errors,
0 late lists, **0 audio underruns**, the rings never below 7808 frames of 16384, 44 music segments
and one loop in 1100 fields, 7 stream jobs (514,048 bytes), group mask 0b101 (common + stage 2).
The dumped audio of a longer run (900 fields) is continuous with no silent runs.

The scheduler has its own host test (`tests/host/test_disc_stream.cpp`, a fake drive that delivers a
segment after a settable number of polls and fake SCSP rings the test drains). It checks the music
payload of both sample formats byte for byte through two loops, the data payload and its clipping,
that a second segment is never started while one is in flight, that a data segment only starts
while the rings hold the reserve (and is counted in `data_deferred` while they do not), that
a bad job or a transport error drops the job without jamming the queue, the queue wrap, and that
`sat_disc_stream_finish` pumps between segments without an underrun. Moving the scheduler into the
library changed no measurement: Ymir acceptance 12/12 with 0 underruns, and Mednafen 256 bullets
60 fps with the same stage timings.

## Performance

### Method

- **Ymir** (`harness/run-bullet-hell.ps1`): real program, FRT ticks per stage (210,540 Hz, one NTSC
  field = 3509 ticks) read from the telemetry struct. Ymir has **no VDP1 fill-rate model**, so these
  are CPU-side numbers (simulation, instance build, command submission, service) and cannot show a
  fill-bound frame.
- **Mednafen** (`BH_AUTOBENCH=1` build, F9 screenshots through `tools/capture-mednafen.ps1`): the
  demo's own HUD shows FPS and the same per-stage tick means. Mednafen models more of the VDP1 and
  SMPC cost, so it is the evidence for frame rate. The per-benchmark `BH_BENCH_LOOP=k` build option
  repeats one benchmark forever so a steady state can be read.

### Results (final build, frame mode AUTO, 640x224 TATE)

CPU ticks per frame on Ymir, 300 frames each (service, simulation, build, draw, end; the field is
3509 ticks):

| Bullets | Serv | Sim | Build | Draw | End | Fields per frame | Commands |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 291 | 224 | 116 | 180 | 73 | 1.00 | 222 |
| 256 | 283 | 326 | 123 | 288 | 75 | 1.00 | 349 |
| 512 | 279 | 481 | 138 | 505 | 127 | 1.01 | 602 |
| 1024 | 304 | 716 | 143 | 923 | 229 | 1.06 | 1097 |

The CPU needs two thirds of a field at 1024 bullets on Ymir (before this round it was a field and a
half). Build is the non-bullet instances only; the bullets are written during "draw".

Frame rate on Mednafen (HUD readout, steady state; stage means in FRT ticks):

| Bullets | FPS | Serv | Sim | Build | Draw | End |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 128 | 60 | 345 | 342 | 142 | 277 | 491 |
| 256 | 60 | 296 | 410 | 142 | 430 | 754 |
| 512 | 53 | 366 | 540 | 137 | 732 | 1314 |
| 640 | 43 | 434 | 528 | 130 | 876 | 1590 |
| 768 | 28 | 567 | 599 | 128 | 1025 | 1758 |
| 896 | 28 | 542 | 681 | 131 | 1181 | 2030 |
| 1024 | 28 | 542 | 754 | 134 | 1336 | 2274 |

Mednafen holds a frame whose list is still drawing at the next VBlank (no tearing), so a late frame
costs a whole field. There is a cliff between 640 and 768 bullets, and past it the frame takes two
fields (28 fps).

### What was tried

- **Frame mode.** `SAT_VDP1_FRAME_ON_SUBMIT` erases with a full-screen polygon at the start of every
  list: about 143k VDP1 pixels per frame in 640x224. The default frame mode (hardware erase) is free,
  and gave 128: 52 -> 60, 256: 28 -> 55, 512: 27 -> 28 (before the other fixes), 1024: 17 -> 20.
  The example uses the default; `BH_FRAME_ON_SUBMIT=1` selects the polygon erase. Re-measured with
  1024 bullets on the final build: 20 fps, and the time from submit to the list's END flag (EDSR
  CEF) is about 2640 ticks at 512 bullets and 3800 at 1024.
- **VBlank erase (TVMR VBE).** Rejected: in 640x224 the 8 bpp erase of the 1024-wide buffer takes
  about 4 ms and does not finish inside the ~2.5 ms VBlank. Mednafen showed stale pixels in the
  bottom strip and torn HUD text, even though the frame rate rose to 58 / 52 for 128 / 256.
- **Asynchronous pad poll.** Mednafen keeps the SMPC busy for about 2.6 ms per INTBACK; the blocking
  poll was 545 of the ~850 service ticks. Splitting it (`sat_input_poll_begin` after the frame is
  submitted, `sat_input_poll_finish` next frame) removed that from the frame.
- **Instance build.** Which frames are resident is decided once per shape and colour, and the CW view
  is two additions with no multiplies: build stage 1010 -> 723 ticks at 1024 bullets. Writing the
  bullets straight into the command list then took the build+draw of 1024 bullets from 1844 to 1500
  ticks.
- **Compact commands.** 16-byte frames and a 16-byte copy per command: draw 1973 -> 1070 and the
  512-bullet Mednafen case 50 -> 54 fps; the 768 case did not move.
- **Pre-clip bit and SPD/pmod tweaks**: keeping the pre-clip bit is worse (END 3876 vs 3474 ticks);
  the others changed nothing.
- **SCU-DMA command list** (`sat_dma_set_command_list`): worse on Mednafen at 1024 (23 fps, copy
  2332 ticks vs 1314), and no gain earlier. DMA paths stay opt-in.
- **Delta copy** (a shadow of the VRAM list, store only the words that changed): the copy got slower
  (1741 vs 1314 ticks) because the reads of VRAM cost more than the stores they save. Removed.
- **Call/return templates** (a bullet as a two-longword local-coordinate command that CALLs a
  sprite table kept in VRAM, so the CPU stores 8 bytes instead of 16): drew correctly on Mednafen,
  but END went from 2248 to 2534 ticks at 1024 and the frame rate did not move, because the VDP1 runs two
  commands per bullet. Removed.

What the measurements say about the cliff. The CPU stages of a 1024-bullet frame add up to about
2800 ticks, so on their own they fit a field. The cost that does not fit is the copy of the list
into VDP1 VRAM: uncontended it is about 300-400 ticks (a second copy of the same list inside one
submit took 274 ticks), but while the VDP1 is plotting it takes 1300-2300, because the CPU's VRAM
stores compete with the VDP1's own VRAM reads. A micro-benchmark of 4096 longwords gives 385 ticks
for stores to VRAM, 192 to Work RAM-H. The VDP1 is busy for most of a field at 768+ bullets
(the plot time of the list measured in `FRAME_ON_SUBMIT` mode grows by about 2.3 ticks per bullet),
so the copy lands in it and the frame misses its VBlank. This corrects the earlier conclusion that
the frame was simply fill-bound: the fill alone (about 2300 ticks of 3509 at 1024) fits a field; the
fill plus the contended copy does not. Copying inside the VBlank does not escape it: the VDP1 is
still busy there on Mednafen (END 1308 at 1024 bullets, 726 at 512).

### Verdict

- 128 and 256 projectiles hold 60 fps on Mednafen, 512 holds 53 (50-54), 640 43; 768 and above fall
  to 28 fps (two fields per frame). On Ymir all sizes up to 1024 fit in 1.06 fields of CPU.
- 1024 at 60 fps with this art (about 224 px per bullet, 1.2 VDP1 clocks per pixel) does not fit
  Mednafen's VDP1 model: plotting plus the copy exceeds a field. Ways forward that were not built:
  smaller or fewer drawn pixels (half-size sprites for the densest streams, culling overdraw), or
  keeping the command list in VRAM across frames and patching positions only while the VDP1 idles
  (needs a double-buffered list). The batch API and the 2048-command ceiling are not the limit, and
  the Master SH-2's own work (about 0.8 field at 1024 bullets on Mednafen) is not either.

## Determinism and tests

- `tests/host/test_bh_sim.cpp`: golden state hash over 3000 frames (seed 7), bullet-vs-player and
  shot-vs-enemy collision against an O(n^2) brute force on random sets including strict edge
  touches, pool capacity behaviour, and the inline view map against `sat_tate_edge`.
- `tests/host/test_sprite_batch.cpp`, `test_sprite_atlas.cpp`, `test_tate.cpp`,
  `test_input_api.cpp`: command words identical to `push_sprite`, dedupe/swap/malformed atlases,
  TATE round trips, the async poll contract.
- `tests/test_shmup_assets.py` (29 tests) and `tests/test_shmup_music.py` (6 tests).
- `harness/run-bullet-hell.ps1 -Bios ...`: 12 acceptance tests on Ymir (setup, stage swap without a
  disc wait, zero underruns, command count and exact collision-test count per bullet count, no late
  lists, CPU time bounds, screenshot content).

Run the example with `.\run-example.ps1 bullet_hell_stress` (Mednafen); the autopilot plays.
A build with `BH_AUTOBENCH=1` starts the 128/256/512/1024 benchmark after three seconds.

## Remaining limitations

- No physical-hardware run; the fill-rate figures come from Mednafen's model and from experiments,
  not from a VDP1 timing reference.
- 1024 simultaneous bullets run at 28 fps on Mednafen (60 fps holds to about 512, see Verdict).
- Hi-res 640x224 is the only exact TATE mode; it costs 8 bpp, one shared palette and no VDP1 colour
  effects. LUT4 is a 320-wide feature in this library.
- Hardware-rotated TATE exists (`sat_tate_fb_init`) but holds 60 fps only to ~384 bullets on Mednafen
  because of the full-field clearing polygon (see Hardware TATE); it was checked on Ymir and Mednafen,
  not on real hardware, where the VDP1 erase restrictions of the manual apply as described.
- Enemies and bosses are the pack's placeholders; 19 green/red/orange sprites quantise with a visible
  mean error.
- `sat_music` still reads synchronously; a program that streams data from the disc while music plays
  feeds the music through `sat_disc_stream_music_start` instead.
- `sat_disc_stream` reads one extent per job and has no decompression stage: a compressed group is
  the sink's work.
- CD data-port transfer cost on the CPU is as modelled by the emulators.
