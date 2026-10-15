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
| Batched VDP1 submission | `include/saturn/sprite_batch.h`, `src/graphics/2d/sprites/batch.cpp`, `hal::vdp1::push_sprite_batch` | One call per frame for thousands of sprites, 8 longword stores per command |
| Sprite atlas / residency | `include/saturn/sprite_atlas.h`, `src/graphics/2d/sprites/atlas.cpp` | Upload each frame once, residency groups in VRAM slots, chunked upload for streaming |
| TATE layout | `include/saturn/tate.h`, `src/graphics/2d/tate.cpp` | Logical 224x448 playfield to the screen, CW/CCW, exact edge mapping, HUD panels |
| Asynchronous pad poll | `sat_input_poll_begin` / `sat_input_poll_finish`, `hal::smpc` | The SMPC INTBACK runs while the frame is simulated |
| Simulation | `examples/bullet_hell_stress/bh_sim.[ch]`, `tests/host/test_bh_sim.cpp` | Fixed pools, 16.16 math, deterministic LCG, `sat_spatial` collision |
| Streaming | `examples/bullet_hell_stress/bh_stream.[ch]`, `tools/shmup_music.py` | Next stage's sprites and background read from disc during play; music fed from the same drive |
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

Known quality notes, not rejections: 14 green/red/orange sprites quantise to the one global palette
with mean RGB error 12-14.5 / 255; `pickup_shield_*` are intentionally two components; enemies and
bosses are placeholders by the pack author's own description.

## Rendering path

Each frame the example builds an array of `{x, y, frame}` instances and calls
`sat_sprite_batch_draw` once. Frame descriptors (`ctrl`, `pmod`, `colr`, `srca`, `size`) are
prepared once from the atlas, so the per-sprite work is a visibility test and eight longword stores.
The host test proves the commands are byte-identical to the general `push_sprite` path (the only
difference allowed is the Pclp bit, below).

- **Pre-clipping disabled for sprites wholly on screen** (CMDPMOD bit 11, manual p06_32: the per-line
  pre-clip test costs up to 5 clocks per line on small sprites). A frame flagged
  `SAT_SPRITE_FLAG_USER_CLIP` keeps pre-clipping because it can cross the user rectangle.
- **Longword stores.** Work RAM-H writes are bus cycles (the cache is write-through); writing a
  command as eight big-endian longwords instead of sixteen halfwords cut the draw stage and took the
  Mednafen 512-bullet case from 28 to 50 fps.
- **Playfield clip.** The field is a 448x224 rectangle between two 96 px HUD panels. Sprites that
  cross its edge (an enemy flying in, a shot leaving) use a user-clipped copy of their frame; bullets
  and sprites well inside use the plain frame. No measurable cost on Mednafen.
- **Command budget.** The command area holds 2048 commands. 1024 bullets plus the player, shots,
  enemies, explosions, setup and about 45 HUD glyphs use 1097; raising 2048 needs moving the texture
  base and is out of scope.

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
- 320x224 would need 5/7 scaling (`tate.h` computes it, `exact = 0`); sprites would have to be
  scaled by the batch. Not implemented in the example.
- Hardware rotation (VDP1 TVM = rotation 8/16 + VDP2 rotation parameters reading the frame buffer)
  would remove the offline rotation, but the library has no VDP1 rotation TV mode and the frame
  buffer would also need the RBG hardware. **Not built**; the software layout above is the
  supported path.

## Texture residency and streaming

`SHMUP.ATL` holds every frame once (content-hash dedupe), a shared palette and LUT table, and
residency groups whose data is 2048-byte aligned, so one stage's enemies can be read from disc by
sector while the previous stage plays and written into a spare VRAM slot with
`sat_sprite_atlas_upload_chunk`. Group 0 is the common art; groups 1-3 are the stage enemy sets.

`bh_stream` is the disc scheduler for the demo (it is example code, not library API):

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
| 128 | 291 | 225 | 187 | 243 | 117 | 1.00 | 222 |
| 256 | 284 | 327 | 259 | 382 | 181 | 1.00 | 349 |
| 512 | 292 | 481 | 406 | 663 | 309 | 1.03 | 602 |
| 1024 | 396 | 717 | 675 | 1200 | 565 | 1.54 | 1097 |

At 1024 bullets the CPU alone is about 3550 ticks, right at one field, so the frame sits on the edge
of 60/30 fps and the field count varies between builds (1.28 to 1.54 measured). 128-512 hold 60 fps.

Frame rate on Mednafen (HUD readout, steady state):

| Bullets | FPS | Notes |
| ---: | ---: | --- |
| 128 | 59-60 | |
| 256 | 54-58 | |
| 512 | 50-52 | was 28 before the longword stores |
| 1024 | 26 | VDP1 fill: about 224 px per 16x14 bullet, ~1 clock per pixel |

Mednafen holds a frame whose list is still drawing at the next VBlank (no tearing), so a late frame
costs a whole field: 26-28 fps is two fields in three.

### What was tried

- **Frame mode.** `SAT_VDP1_FRAME_ON_SUBMIT` erases with a full-screen polygon at the start of every
  list: about 143k VDP1 pixels per frame in 640x224. The default frame mode (hardware erase) is free,
  and gave 128: 52 -> 60, 256: 28 -> 55, 512: 27 -> 28 (before the other fixes), 1024: 17 -> 20.
  The example uses the default; `BH_FRAME_ON_SUBMIT=1` selects the polygon erase.
- **VBlank erase (TVMR VBE).** Rejected: in 640x224 the 8 bpp erase of the 1024-wide buffer takes
  about 4 ms and does not finish inside the ~2.5 ms VBlank. Mednafen showed stale pixels in the
  bottom strip and torn HUD text, even though the frame rate rose to 58 / 52 for 128 / 256.
- **Asynchronous pad poll.** Mednafen keeps the SMPC busy for about 2.6 ms per INTBACK; the blocking
  poll was 545 of the ~850 service ticks. Splitting it (`sat_input_poll_begin` after the frame is
  submitted, `sat_input_poll_finish` next frame) removed that from the frame.
- **Instance build.** Which frames are resident is decided once per shape and colour, and the CW view
  is two additions with no multiplies: build stage 1010 -> 723 ticks at 1024 bullets.
- **SCU-DMA command list** (`sat_dma_set_command_list`): no gain, END 4000-4148 vs 3474 ticks.
  DMA paths stay opt-in (Ymir and Mednafen disagree on DMA timing).
- **Keeping the pre-clip bit**: worse (END 3876 vs 3474). **SPD/pmod tweaks**: no change.
- Drawing the bullets was the bottleneck: without drawing them 512 bullets ran at 60 and 1024 at
  55-57, so the simulation and build paths are not the limit. Larger bullets cost more (a frame
  of 16x16 rings ran at 15 fps at 1024; a frame of 16x8 needles cut the END stage from 3474 to 2697 ticks).

### Verdict

- 128-512 projectiles: hold or nearly hold 60 fps on both emulators.
- 1024 projectiles at 16x14 px each: VDP1 fill bound on Mednafen (26 fps); the CPU side fits one
  field on Ymir only barely. A real port reaching 1024 at 60 fps needs smaller bullet sprites, fewer
  drawn pixels (cull overdraw, draw the densest streams as half-size), or splitting the build across
  the Slave SH-2. The batch API and 2048-command ceiling are not the limit.

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
- Hi-res 640x224 is the only exact TATE mode; it costs 8 bpp, one shared palette and no VDP1 colour
  effects. LUT4 is a 320-wide feature in this library.
- Hardware-rotated TATE (VDP1 rotation framebuffer + VDP2 rotation) is not implemented.
- Enemies and bosses are the pack's placeholders; 14 green/red/orange sprites quantise with a visible
  mean error.
- The disc scheduler lives in the example; `sat_music` still reads synchronously.
- CD data-port transfer cost on the CPU is as modelled by the emulators.
