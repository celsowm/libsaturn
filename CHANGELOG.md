# Changelog

All notable changes to LibSaturn are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/). While the major version is `0`, a
minor release may change the installed package contract.

The version is defined once, in the `VERSION` file.

## [Unreleased]

### Added

- Shmup / bullet-hell support (see `docs/SHMUP_BULLET_HELL_SUPPORT.md`):
  `include/saturn/sprite_batch.h` (`sat_sprite_batch_draw`: thousands of
  prepared sprite frames in one call, a 16-byte frame whose first twelve
  bytes are the command's own, four longword stores per VDP1 command and
  a 16-byte copy of each into VRAM, pre-clipping disabled for sprites
  wholly on screen, user-clipped frames via `SAT_SPRITE_FLAG_USER_CLIP`;
  `sat_sprite_batch_begin` / `sat_sprite_batch_emit` / `sat_sprite_batch_end`
  write sprites straight from a program's own state into the frame's list), `include/saturn/sprite_atlas.h`
  (SATL atlas: content-deduplicated frames, residency groups in VRAM slots,
  chunked upload for streaming), `include/saturn/tate.h` (logical portrait
  playfield to a landscape screen, exact edge mapping, HUD panels) and
  `sat_input_poll_begin` / `sat_input_poll_finish` (the SMPC INTBACK runs
  while the frame is simulated). `tools/shmup_assets.py` rejects defective
  sprite packs and packs the atlas and scrolling backgrounds;
  `tools/shmup_music.py` renders the demo track. New example
  `bullet_hell_stress` streams the next stage and the music from disc, with
  `harness/run-bullet-hell.ps1` as its Ymir acceptance run.
- Hardware TATE: `include/saturn/tate_fb.h` (`sat_tate_fb_init`) draws a
  224 x 448 field upright in the VDP1 rotation framebuffer (512 x 512, 8 bpp)
  and has the VDP2 turn and scale it through rotation parameter A on a
  320 x 224 screen, with `sat_tate_fb_rotation_table` (pure, host-tested),
  `sat_vdp2_rbg0_set_screen_over`, and the example `tate_fb_rotation`; the
  `BH_TATE_FB=1` variant of `bullet_hell_stress` uses it. The VDP1 cannot
  erase a rotated frame buffer in the picture, so every list starts with a
  clearing polygon (~100,000 pixels).
- `include/saturn/disc_stream.h` (`sat_disc_stream`): a disc scheduler for a
  program that streams data and music from one drive. One segment in flight;
  a looping stereo music file is split into two SCSP stream rings and has
  priority; data jobs (a file extent and `begin`/`chunk`/`done` callbacks) take
  the gaps while the rings hold a reserve. Host-tested against a fake drive.
- `sat_music_use_disc_stream` (`include/saturn/disc_stream.h`): `sat_music` tracks backed by a disc
  file stream through the disc scheduler instead of waiting for the drive in `sat_music_update`
  (Mednafen, jukebox: worst update 359,709 us -> 14.6 ms, worst frame 381 -> 31 ms, 0 underruns).
  S8/S16, mono/stereo and IMA ADPCM, seek, pause and loop are all served; the feed is pipelined
  (read the next segment while delivering the last, a slice per frame). Supporting API:
  `sat_disc_stream_music_attach` / `_detach` / `_abort` / `_prime` (a pluggable music feed,
  `sat_disc_music_feed_ops_t`), `sat_cdfs_source_file`, `sat_file_backend` and the
  `disc_streamed` field of `sat_music_info_t`. The jukebox example uses it
  (`JUKEBOX_DISC_STREAM=0` builds the old blocking path for comparison).
- Core math/string helpers in `include/saturn/core.h`:
  `SAT_FX16(x)` and `SAT_FX16_ONE` for 16.16 fixed-point literals,
  `SAT_FOURCC(a,b,c,d)` for big-endian four-character tags,
  `SAT_MIN` / `SAT_MAX` / `SAT_CLAMP` macros, and
  `sat_isqrt32(v)` / `sat_triangle_wave(step, period, amp)` C functions
  with implementations in `src/core/math3d/api.cpp`. Eight examples that
  redefined local `FX(v)`/`FX_INT(v)`/`FX16_SHIFT` aliases on top of these
  now call the library helpers directly.
- Color helper alias `SAT_RGB555_RAW(r,g,b)` in
  `include/saturn/color.h`: the same value as `SAT_BGR555`, exposed so
  harness tests and telemetry probes compare against a named hook
  instead of writing `0x8000 | (...)` shifts by hand.
- `include/saturn/vdp2_alloc.h` + `src/hal/vdp2/allocator.cpp`:
  `sat_vdp2_vram_alloc(bytes, align_words)` and
  `sat_vdp2_vram_free(base)` plus
  `sat_vdp2_vram_alloc_init(start, end)`,
  `sat_vdp2_vram_alloc_reset()`,
  `sat_vdp2_vram_largest_free()` and a bank-rotating
  `sat_vdp2_palette_reserve(count, *out)` /
  `sat_vdp2_palette_free(range)` /
  `sat_vdp2_palette_reset()`. The VRAM allocator is first-fit with
  alignment-driven splitting and adjacent-block coalesce; the failure
  sentinel is `SAT_VDP2_VRAM_NO_BASE` (0xFFFFFFFFu) so a real base of
  0 does not collide with "exhausted".
- HUD additions in `include/saturn/hud.h` +
  `src/graphics/2d/hud.cpp`: `sat_hud_init_default(hud, font)` (one
  call that inits the font and binds it; spacing 1 in 320x224 and 2 in
  hi-res), `sat_hud_line(hud, label, value)` (formats label/value and
  advances the cursor), `sat_hud_reset_cursor(hud, x, y)`, plus
  `cursor_x`, `cursor_y` and `line_height` fields on `sat_hud_t`.
- `sat_pad_toggle(pad, button, *flag)` inline in
  `include/saturn/input.h`: replaces the seven
  `if (pad.pressed & X) flag ^= 1` patterns still present in
  examples.
- `sat_f2ms(frames, hz)` and `sat_ms2f(ms, hz)` inline in
  `include/saturn/time.h`: frame<->millisecond conversions taking
  the refresh rate as a parameter so the 60Hz / 50Hz hazard
  disappears from the fifteen places that hard-coded it.
- Loading-screen bar helper `sat_loading_screen_draw_bar(screen, percent)`
  in `include/saturn/loader.h` + `src/graphics/2d/loading_screen.cpp`:
  draws the frame ring, track and filled portion described by a
  `sat_loading_screen_t` into the currently open VDP1 frame, with the
  percent clamped to [0, 100]. `sat_loading_screen_progress` is now
  a thin wrapper (validate, compute percent, open frame, call
  draw_bar, run decorate, close frame) and
  `sat_example_loading_frame(font, screen, title, stage, percent)`
  in `examples/common/example_util.h` reuses the same helper for
  its bar so the bar math lives in one place. `city_walk/main.c` and
  `examples/skybridge_3d/hud.c` construct their own
  `sat_loading_screen_t` to match the panel layout.
- `sat_video_refresh_hz()` accessor in `include/saturn/video.h` +
  `src/graphics/video.cpp`: returns 60 for NTSC timing, 50 for PAL,
  so callers stop open-coding `is_ntsc_timing() ? 60 : 50` and the
  docstring in `time.h` finally names something that exists. The
  four remaining rate sites in `examples/basic_3d_animation/main.c`
  (twice), `examples/dino_demo/main.c` and
  `examples/city_walk/player.c` use it now; the local `g_ntsc`
  caches those examples kept for the same purpose are gone.
- The five `if (pad.pressed & X) flag ^= 1` patterns in
  `examples/scene_cache_occlusion/main.c` (A, B) and
  `examples/vdp2_hires_light/main.c` (A, B, C) now call
  `sat_pad_toggle(pad, button, *flag)` from `include/saturn/input.h`.
  Two of the seven sites the original survey counted (one in
  `skybridge_3d/main.c`) were bitfield ORs, not toggles, and stay
  as-is.
- `examples/vdp2_nbg0_ground/main.c` and
  `examples/vdp2_nbg0_image/main.c` no longer poke the VDP2
  registers directly. Both call `sat_vdp2_nbg0_init` and
  `sat_vdp2_nbg0_set_scroll` from `include/saturn/vdp2.h`; the
  old `nbg0_direct_init`, `nbg0_direct_set_scroll` and the
  `VDP2_REG(off)` macro are gone. `vdp2_nbg0_ground_api` is the
  reference port.
- `examples/dma_demo/main.c` adopts the HUD overlay: the local
  `line(label, value, y)` helper now calls `sat_hud_value` and
  the title uses `sat_hud_text`, with a `sat_hud_t` carrying the
  font, palette code 0 and 8 px/cell spacing that the original
  hand-rolled `sat_ascii_font_draw_*` calls hard-coded.
- `sat_hud_init` now accepts `char_spacing == 0` (tight: the font's
  glyph width is the per-glyph advance, no extra gap), matching
  the value two examples (input_move, physics_2d) were passing to
  the open-coded `sat_ascii_font_draw_text_screen_indexed8(..., 0, ...)`
  calls. test_hud.cpp locks the new behaviour.
- HUD migrations continued: `irq_demo`, `audio_showcase`, `abus_demo`,
  `expansion_probe_demo`, `parallel_sched_demo`, `scsp_dsp_demo`,
  `input_move` and `dual_sh2` (eight more) now use `sat_hud_t` +
  `sat_hud_text` / `sat_hud_value`. `red_square` and `input_debug`
  draw their text in world space, not screen space, so they stay
  on the open-coded `sat_ascii_font_draw_text_indexed8`; `cd_streaming_jukebox`
  had already adopted the overlay before this sweep.
- `sat_cd_mount_default(block, progress, progress_context, device,
  sector_count, volume)` in the new `include/saturn/cd_mount.h`
  + `src/storage/cd/mount.cpp`: one-call CD bring-up that does
  `sat_cd_block_init`, the optional `sat_cd_block_set_progress_service`,
  `sat_cd_block_bind_device` and `sat_cdfs_mount` in the order every
  example that boots from disc open-coded. Each output pointer
  may be NULL to skip that step, so callers that only need init
  (expansion_probe_demo's no-card probe) still get the one helper.
  Six examples migrated: city_walk, dino_demo, cd_streaming_jukebox,
  city_load_probe, exec_demo, expansion_probe_demo.
- Harness pytests migrated to `harness/scripts/telemetry.py`:
  `find_symbol` (replaces the `WRAM_HIGH = 0x06000000 + re.search`
  dance) and `load_dump(dump, map, name, fmt, count, offset)`
  (replaces the open file + slice + struct.unpack). `load_dump` now
  takes an `offset` argument for the second region of a struct
  (test_vdp2_window_dim reads the header as u32 and the span
  tables as u16 from the same symbol). Sixteen pytests adopted:
  test_abus, test_dma_demo, test_exec_demo, test_expansion_probe,
  test_input_devices, test_irq_demo, test_parallel_sched,
  test_save_cartridge, test_scsp_dsp, test_scu_dsp, test_sound_driver,
  test_vdp2_compose, test_vdp2_effects, test_vdp2_layers,
  test_vdp2_tile_overflow, test_vdp2_window_dim, plus the
  shared `harness/tests/city_telemetry.py` loader that test_city_walk
  pulls in. city_telemetry still parses the C header for its
  field names, since the demo is the one with a struct that
  grows; everything else uses a fixed FIELDS list.
- Harness wrappers slimmed: `Invoke-ExampleTest` in
  `harness/scripts/run-example-tests.ps1` extended with `-Out`,
  `-DumpAudio`, `-DumpSoundRam` and `-Pal`, then eleven `run-*.ps1`
  wrappers (irq_demo, scu_dsp, parallel_sched, vdp2_effects,
  vdp2_compose, vdp2_tile_overflow, vdp2_window_dim, exec,
  expansion_probe, scene_cache_occlusion, save_cartridge) dot-
  source the helper and call it with the per-example parameters.
  The four multi-scenario wrappers (abus, input-devices,
  vdp2-hires-light, vdp2-layers) keep their own loops because
  the helper assumes one Ymir run per invocation; the two
  sweep wrappers (acceptance-sweep, parallel-runtime-sweep)
  and the city-walk / ram-cart / skybridge wrappers stay on
  their bespoke orchestration.
- Harness: `harness/scripts/telemetry.py` exposes `WRAM_HIGH`,
  `find_symbol`, `load_dump` and `verify_fields` so the eighteen
  pytest files no longer hand-roll the same map-regex /
  offset-from-WRAM-High / `struct.unpack` dance.
  `harness/tests/test_vdp2_hires_light.py` is the first to adopt it.
- Harness: `harness/scripts/run-example-tests.ps1` exports a single
  `Invoke-ExampleTest` function (Name, Example, Frames, Screenshot,
  MisalignedReport, TestFile, DumpName, MapSuffix, ExtraEnv). The
  first adopter is `harness/run-dma-demo.ps1`, which shrank from
  38 lines of fixed scaffolding to 13 lines calling it.

- `examples/vdp2_hires_light`: a red light sweeping over hi-res (640x224)
  VDP1 polygons. Hi-res VDP1 pixels are 8-bit palette codes, so Gouraud
  shading (which needs RGB pixels) is unavailable; the VDP2 brightens them
  instead. The line colour screen (one red per line, brightest at the light's
  centre) is inserted under the sprite layer only, sprite colour calculation in
  add mode adds it, and an ellipse colour calculation window makes it a moving
  spot. Sprite type C selects S0PRIN/S1PRIN with code bit 7, so a "priority ==
  SPCCN" condition lights codes 128-255 only: a per-polygon lit flag.
  `harness/run-vdp2-hires-light.ps1` and
  `test_vdp2_hires_light.py` check the lit, unlit and background pixels and the
  sweep on Ymir; Mednafen shows the same picture.
- `sat_vdp2_cram_mode_set` / `sat_vdp2_cram_mode`: choose CRAM mode 0 or 1.
  Hi-res screens mix palette-format images by colour calculation only in mode 0
  (VDP2 manual, table 12.1).
- `sat_vdp2_hires_sprite_color_calc_set(mode, ratio)`: colour calculation for
  hi-res (type C) sprites, on every dot (`SAT_VDP2_HIRES_SPRITE_CC_ALL`) or on
  codes 128-255 only (`_HIGH_CODES`, which splits PRISA by code bit 7). Hi-res
  and CRAM mode 0 only; BUSY while the sprite colour calculation module is on,
  and that module is BUSY while this is. `sat_vdp2_sprite_set_line_color_insert`
  puts the line colour screen under the sprite layer (LNCLEN SPLCEN). Both are
  replayed by `sat_vdp2_layers_commit()`, so a per-line light on hi-res
  polygons no longer needs raw register writes ordered after the commit.
- `examples/vdp2_window_dim`: darkening limited to a window. The VDP2 colour
  offset (the fade) is per screen, so "darken everything except these
  spotlights" has no register of its own. The example puts a black full-screen
  NBG on top with colour calculation (the image below shows through at
  `ratio + 1` 32nds) and shows it only outside two line windows, which are
  rewritten every frame as two wedge-shaped beams swing; the picture below is
  untouched inside them and, with the ratio dropped midway, a half and then a
  quarter outside. `harness/run-vdp2-window-dim.ps1` and
  `test_vdp2_window_dim.py` check the library's verdicts, the beam tables, and
  the colour inside and outside the beams at both ratios on Ymir.
- `examples/vdp2_tile_overflow`: more than 512 distinct 8bpp tiles on one scroll
  screen. A 1-word pattern name holds a 10-bit character number and an 8bpp cell
  takes two of them, so one layer's 32 KiB character window shows 512 tiles and a
  GBA-style map naming tile 525 wrapped onto tile 13. The example reads tiles
  0-510 through NBG0 and 511-959 through NBG1, whose window follows NBG0's (each
  map cell is the blank last cell in the layer that does not own its tile), with a
  third 256-colour layer, NBG2, behind both. The library already planned the VRAM
  cycles for three 256-colour layers (a name read and two character reads each);
  `harness/run-vdp2-tile-overflow.ps1` and `test_vdp2_tile_overflow.py` check the
  cycle pattern, every tile's colour, and that NBG0, NBG1 and NBG2 put the same
  scroll line first.
- Chain-loading (`saturn/exec.h`): `sat_exec` replaces the running program
  with another program file on the disc, as the BIOS starts `0.BIN`, so one
  disc can hold a menu and the games it starts.
  - **Order:** it checks the file and the arguments first, so an error returns
    with nothing stopped. Then it stops what the running program left going:
    interrupts back to the BIOS vectors, the Slave, SCU DMA and DSP, the SCSP
    slots and the sound CPU, VDP1 plotting, and the VDP2 registers and
    display.
  - **Load:** it stages the image in Work RAM Low. A 40-byte copy routine at the
    top of Work RAM Low then moves the image to `0x06004000`, purges the cache
    and jumps.
  - **Argument:** up to `SAT_EXEC_ARG_MAX` bytes reach the next program through
    `sat_exec_arg`.
  - **Read failure:** reads retry; a read that still fails resets through the
    SMPC (`SYSRES`).
  - **Reserved memory:** the last 512 bytes of Work RAM Low are reserved;
    `saturn.ld` asserts that no `.wram_l` reaches them.
  - **Tests:** `test_exec` (argument and size checks, staging, chunked reads,
    retries, the hand-off) and `harness/run-exec.ps1`. In the harness run,
    `examples/exec_demo` execs itself four times with the Slave and VBlank
    interrupts running; Mednafen boots the same disc through the BIOS.
- `saturn::hal::smpc::system_reset` (SYSRES) and `saturn::hal::cd::bound_block`
  (the CD block behind a bound device), both internal.
- SLZ1 compressed disc files (`saturn/lz.h`), made by `tools/satlz.py`
  (installed with the package's tools). The drive reads ~300 KB/s and tile or
  sprite data shrinks to 20-35%, so a compressed file loads in about a third
  of the time.
  - `sat_lz_decode` decodes from memory. `sat_load_lz` (`saturn/loader.h`)
    reads a file to the end of its destination and decodes it there in place,
    so it needs no buffer beyond the header's `decoded + margin` bytes.
  - `sat_cart_overlay_load_lz` does the same into a cart overlay slot.
  - The decode runs behind the read: after each batch of sectors the CD Block
    reports where the data ends (`sat_cd_set_arrival_observer`, `saturn/cd.h`),
    and the loader decodes the sequences already there while the drive reads
    on. A transport that does not report arrivals is decoded at the end.
  - Covered by `test_lz` (corrupt and short input, in-place decoding, input
    arriving byte by byte), `test_loader` (a five-sector file decoded during
    its read), `test_cart_overlay`, and `tests/tools/test_satlz.py` (round
    trips, and a check that in-place decoding never overwrites unread input).
- Background CD reads (`saturn/cd_block.h`): `sat_cd_stream_begin` starts the
  drive on up to `SAT_CD_STREAM_MAX_SECTORS` sectors and returns;
  `sat_cd_stream_poll` moves the sectors that have arrived (never waiting), once
  a frame, so a game reads data while it runs instead of behind a black screen.
  One stream at a time: any `sat_cd_block_read_sectors` (so any file read)
  finishes an open stream first, and `sat_cd_stream_finish` waits for it.
  `sat_cdfs_file_sectors` (`saturn/cdfs.h`) maps a file range to LBA and sector
  count. `sat_cd_block_t` gained `stream_active`, `stream_remaining` and
  `stream_destination` (the old `reserved` byte is now `stream_active`).
  Argument and bookkeeping checks are in `test_cd_block_api` and
  `test_cdfs_logic`; the drive side runs on the SH-2 only.
- RBG0 in cell format under the NBG layer manager (`saturn/vdp2_layers.h`,
  `sat_vdp2_rotation_*`), for tilted floors and rotating or zooming maps.
  - RBG0 takes whole banks for its pattern names and characters (RAMCTL RDBS).
    The NBG cycle planner keeps out of those banks, and a configuration that
    would leave an NBG without its bank fails with `SAT_ERR_CAPACITY`.
  - `sat_vdp2_rotation_write_params` encodes rotation parameter A (manual 6.3,
    16.16 in, the VDP2's bit widths out) at a table address. The new table is
    switched in at the next `sat_vdp2_layers_commit`, so two tables can be
    used in turn.
  - Per-line 2-word coefficient tables (`sat_vdp2_rotation_coefficient`,
    `_write_coefficients`) set kx/ky, kx, ky or Xp, with the per-line
    transparent bit. They may sit in a bank with no RBG0 role (p06_20). KTAOF
    is set for tables past 256 KiB.
  - Covered by `test_vdp2_rotation_logic`: validation, register composition,
    NBG planning round reserved banks, and parameter and coefficient encoding
    against figures 6.2 and 6.7.

- `saturn/sampler.h`: SCSP voices the caller drives every frame, for a
  sequencer that runs its own envelopes, bends and pans (e.g. a port of
  another console's sound engine). `sat_sampler_init` claims a range of slots
  and of Sound RAM. Each voice owns two slots and starts a new note on the one
  not sounding. Key-ons and key-offs are staged, and `sat_sampler_commit`
  applies them with one KYONEX. `sat_sampler_voice_set` takes a rate in Hz
  and linear left/right levels and writes OCT/FNS, TL and DIPAN only when
  they change. `sat_sampler_mute_range` silences every slot reading a region
  before new samples are loaded into it.
  - While the sampler runs, resident voices skip its slots and resident
    sounds and effect rings stay out of its RAM. A stream whose slot or
    buffer it claimed is refused with `SAT_ERR_BUSY`. `sat_audio_shutdown`
    ends it.
  - Covered by `test_sampler_api`, which runs against a slot model that
    asserts no slot is reprogrammed while it is heard.
- `sat_vdp2_sprite_set_palette_priorities(priority[8])`: per-sprite VDP2
  priority. The sprite layer switches to sprite type 1 (palette dots only),
  and an INDEXED8 sprite picks one of eight priority selectors with
  `palette_override = (selector << 5) | bank`. RGB dots are not shown while it
  is on; sprite colour calculation and this API refuse each other with
  `SAT_ERR_BUSY`; hi-res is `SAT_ERR_UNSUPPORTED`; `NULL` restores type 0.
  `vdp2_layers_demo` draws one square in front of NBG0 and one behind it in
  the same frame, checked by `test_vdp2_layers.py`.
- `sat_vdp2_sprite_set_palette_color_calc(ratio[4])`: semi-transparent
  palette sprites while palette priorities are on. Dots whose CRAM colour has
  bit 15 set are mixed with the image below (SPCTL condition "colour MSB",
  CCCTL SPCCEN) by the ratio their colour calculation bits 12-11 select
  (CCRSA-B); other dots stay opaque, so a program blends a sprite by drawing it
  with a copy of its palette that has bit 15 set. Replayed with the layer
  registers; turning palette priorities off turns it off. Covered by
  `test_vdp2_compose_logic`.
- `sat_vdp2_color_calc_set_add(add)`: the VDP2 add mode (CCCTL CCMD) for
  per-screen and palette sprite colour calculation: the two images are summed
  and saturated (GBA alpha blending with EVA = EVB = 16) instead of mixed by
  ratio. Refused with BUSY while the sprite colour calculation module owns
  the bit.
- `sat_vdp2_sprite_set_palette_window(enabled)` and the `sw` field of
  `sat_vdp2_screen_window_t`: a sprite window drawn by palette sprites. It
  switches palette sprites to type 3 (same selectors 0-3, colour calculation
  bits and dot colour as type 1) with SPWINEN; dots with bit 15 set make the
  window and are hidden on the sprite screen (WCTLC SPSWE), and other screens
  are shown inside or outside it like W0/W1. `sw` is a new last member: code
  that fills the struct field by field must set it (brace initialisers leave it
  off), or start from `sat_vdp2_screen_window_default(screen, &config)`, which
  sets every window off. Covered by `test_vdp2_compose_logic`.
- `sat_vdp2_screen_set_cram_bank(screen, bank)`: the CRAM bank (CRAOFA/CRAOFB,
  256 colours each in CRAM modes 0/1) of NBG0-NBG3, RBG0 or the sprites, so a
  layer can draw from another copy of its palette (a blend-scaled or faded set)
  without touching tiles. Replayed by `sat_vdp2_layers_commit()`;
  `sat_vdp2_cram_banks_apply()` rewrites them from an interrupt handler that
  switched banks mid-picture. `sat_init` now clears CRAOFA, which the hardware
  leaves undefined after reset. Covered by `test_vdp2_compose_logic`.
- `SAT_SPRITE_FLAG_FLIP_H` / `SAT_SPRITE_FLAG_FLIP_V` for normal, scaled and
  distorted sprite commands: the VDP1 reads the pattern mirrored (CMDCTRL
  Dir), so one texture serves every orientation. Polygons refuse them.
  `vdp2_layers_demo` draws its front square flipped both ways.
- `sat_vdp1_set_user_clip(x0, y0, x1, y1)` and `SAT_SPRITE_FLAG_USER_CLIP`:
  VDP1 user clipping for the plain sprite commands (normal, scaled,
  distorted), until now reachable only through `sat_render2d_set_clip`. The
  rectangle is in screen dots, cut to the screen (wholly off it:
  `SAT_ERR_INVALID_ARG`, nothing appended), and render2d re-sends its own
  clip afterwards. It confines a distorted sprite to a box, as a rotated
  hardware sprite without a double-size area is on the GBA; without it the
  quad's corners spilled past the box.
- Harness probe `--dump-mem ADDR:BYTES:PATH` (repeatable): dumps any bus
  range (Low WRAM, the RAM cartridge, ...) after the run, read without side
  effects.
- Harness probe `--screenshot-when ADDR:VALUE[+DELAY]:PATH` (repeatable):
  shoots DELAY frames after the big-endian word at ADDR first reaches VALUE.
  Keyed on a guest frame counter, a shot lands on the same game frame however
  fast the program runs, which `--screenshot FRAME:PATH` cannot do.
- Harness probe `--misaligned-report PATH` (and `run-harness.ps1
  -MisalignedReport`): lists the SH-2 data accesses whose address is not a
  multiple of their size, one line per distinct PC, and the probe always
  prints their count. On hardware each is a CPU address error; Ymir aligns the
  address down and goes on, so such code ran on the probe with data shifted by
  a halfword. Counted by `harness/ymir_misaligned_access.patch`, applied to the
  pinned Ymir like the debug metrics patch. `test_vdp2_layers.py` requires
  none in the library, the BIOS boot and the demo.
- `sat_vdp1_set_frame_mode(SAT_VDP1_FRAME_ON_SUBMIT)`: the VDP1 draws a list as
  soon as `sat_end_frame` submits it (PTMR=1) and the VBlank-IN after the
  list's END asks for the swap (manual change, FBCR FCM=FCT=1, the manual's
  "change when CPU processing is finished" sequence), so sprites appear in the
  same field as the VDP2 state committed in that VBlank instead of one field
  later. A list still drawing at a VBlank is never shown half drawn: the
  previous picture stays up a field longer, and the next `sat_end_frame` waits
  for the outstanding swap before plotting again. `sat_wait_vblank` returns
  only in the VBlank where the last list's swap was asked for (polling the
  list's END while that VBlank lasts), so VDP2 state committed right after it
  never runs a field ahead of the sprites. The erase becomes a
  screen-size polygon at the start of each list (none while erase is
  disabled). Applied by the next `sat_begin_frame`; `SAT_VDP1_FRAME_AUTO`
  restores the default.
- `sat_vdp1_frame_stats()`: `late_swaps` (VBlanks at which an ON_SUBMIT list
  had not ended), `draw_waits` and `draw_timeouts` (CPU waits on the VDP1
  before reusing its VRAM).
- `saturn/loader.h`: boot loading in one call. `sat_load_files()` validates a
  list of `sat_load_item_t` (disc path, expected size, offset/bytes, RAM
  destination) before copying anything, then reads them in sector-multiple
  chunks (default 256 KiB) and reports `done/total` bytes to a progress
  callback. `sat_loading_screen_progress()` is a ready-made callback that draws
  a progress bar (one VDP1 frame per update) on a flat background colour. Host
  test: `test_loader.cpp`.
- `saturn/cart_overlay.h`: overlays in the RAM cartridge, for data that does
  not all fit the cart (a game's levels). A slot (`sat_cart_overlay_init`, a
  word-aligned range of the 4 MiB cart window) holds one disc file at a time;
  `sat_cart_overlay_load(slot, volume, id, path, size, progress, ctx)` does
  nothing if `id` is already resident, otherwise refuses a file larger than
  the slot (`SAT_ERR_CAPACITY`) before writing, reads it through the
  cache-through mirror and then purges the CPU cache, so the program's cached
  reads never see the previous overlay's lines. A failed read leaves the slot
  empty. Host test: `test_cart_overlay.cpp`.

- RAM-cart overlay build helpers (`cmake/LibSaturnFunctions.cmake`) for programs whose read-only data
  does not all fit the cart. `libsaturn_cart_overlay_script()` writes an ld `OVERLAY` block (members
  share one slot address, each section word-aligned, size asserted),
  `libsaturn_inject_link_script()` puts it and any resident cart block into a copy of `saturn.ld`
  (ld `INSERT` cannot augment a `-T` script), and `libsaturn_split_cart_image()` writes the program
  image without the cart sections plus one flat file per overlay member and for the resident cart
  section, ready for `sat_cart_overlay_load`. The package-consumer test links and splits a two-member
  overlay program and checks the bytes of every output.

### Changed

- Sound RAM from 0x3000 up is free for samples (was 0xB000): 32 KiB more for
  resident sounds, effect rings and `sat_sampler`. The sound driver's stack
  moved from 0xA000 to the 1 KiB between its mailbox and its score (top
  0x1000), so the driver uses nothing above the score's end.
  `kSystemReservedBytes` and `SAT_SAMPLER_RAM_BASE` are now 0x3000.
  - `static_assert`s tie the reserved size to the driver layout, and
    `test_sound_driver.py` checks the stack and score addresses against the
    assembly.
- Programs that never call `sat_music_*` or the asset API no longer link the
  music slots (~297 KB) or the built-in asset cache blocks (~269 KB) into
  Work RAM Low. `sat_audio_init`/`sat_audio_shutdown` reach the music reset
  through a weak reference. The cache blocks moved to `asset_cache.cpp`, and
  the first cache fill configures them (`sat_asset_cache_stats` still
  reports the default capacity before that). The package-consumer test fails
  if either comes back.
- `sat_loading_screen_t` gained an optional `decorate` hook (and
  `decorate_context`), called inside each progress frame after the bar, so a
  program can add a logo or caption. `sat_loading_screen_default` leaves it NULL.
- `dino_demo` runs in `SAT_VDP1_FRAME_ON_SUBMIT` mode (`-DDINO_FRAME_ON_SUBMIT=0`
  restores the 1-cycle default), so a half-rewritten command table is never
  shown. It is hi-res (640x224) only now (the 320x224 `DINO_HIRES=0` mode is
  gone), its HUD no longer shows an `FPS` figure (it differed 2x between
  emulators), and a new `LATE SWAPS` line reports
  `sat_vdp1_frame_stats().late_swaps`. Its startup shows the stock
  `sat_loading_screen_progress` bar (palette codes in a spare CRAM bank, since
  hi-res VDP1 cannot draw RGB) instead of two seconds of black screen. A
  PS1-style red alarm light (Z toggles it) sweeps up and down the body: a soft
  band of per-line red from the VDP2 line colour screen, added to the sprite
  layer only by add-mode colour calculation, in CRAM mode 0 (the
  `vdp2_hires_light` technique without its window, so the light has no hard
  edge).

### Fixed

- The first `sat_vdp2_layers_commit()` switched CRAM to mode 0 (1024 colours)
  in any program that used neither the legacy NBG0 text layout nor RBG0: the
  RBG0 replay wrote RAMCTL from a shadow that defaulted to 0. CRAM banks 4-7
  then wrapped onto 0-3 (the hi-res "sprite bank 7 draws black" seen in
  `dino_demo`). Every RAMCTL write now keeps the selected CRAM mode, mode 1
  unless `sat_vdp2_cram_mode_set` chose otherwise.
- `sat_vdp2_sprite_set_priority` read PRISA back to update its shadow. VDP2
  registers are write-only (hardware and Mednafen read 0; Ymir returns the
  write, which hid it), so on real hardware the next `sat_vdp2_layers_commit()`
  replayed priority 0 and every sprite vanished. It now records the value it
  computed.
- The Slave SH-2 never started under a real BIOS (hardware, Mednafen, or the
  probe without `--slave-reset-entry`). After SSHON the BIOS takes the Slave's
  start address from vector 0x94 of the Master table (0x06000250), but
  `sat_dual_sh2_start` wrote it into the Slave table (0x06000650). The start
  timed out, and `sat_parallel_init(SAT_PARALLEL_AUTO)` quietly ran every task
  on the Master.

- Nothing kept the Master stack out of `.bss`. The stack starts at the top
  of Work RAM High and grows down, so a `.bss` that ended 212 bytes under the
  top linked fine, and the stack then overwrote the RAM cart allocator's
  state (a consumer's next `sat_ram_cart_alloc` failed). `saturn.ld` now
  asserts that `.bss` leaves `__stack_reserve` bytes (default 16 KiB;
  override with `-Wl,--defsym=__stack_reserve=N`). The package-consumer test
  checks that an impossible reserve is rejected at link time.

- VDP2 init cleared PRISB (0x0F2) instead of SFCCMD (0x0EE), the special
  colour calculation mode register.
- `test_vdp1_logic.cpp`'s `sprite_pmod_shadow_flag` was defined but never
  run; it is now.
- `test_vdp1_clip`, `test_vdp1_checkpoint` and `test_vdp1_vram_region` no
  longer linked (the VDP1 HAL reads TVSTAT and the frame counter); stale
  binaries hid it. Their clock stubs moved to
  `tests/host_stubs/stub_vdp1_clock.cpp`.
- `libsaturn_add_disc()` with more than one `ROOT_FILES` (or `ROOT_DIRS`)
  entry failed: the list was joined with `\;`, which reached the disc script
  verbatim and turned the entries into one missing path. Entries now travel
  `|`-separated; the package consumer test puts two `ROOT_FILES` on its disc.
- NBG2/NBG3 were drawn shifted down when any layer setter ran during the
  picture: every setter rewrote SCYN2/SCYN3 at once, and a write to those
  registers restarts the layer's line counter at the current scanline (the
  layer showed its line 0 there). The other scroll and zoom registers had the
  same problem in a milder form: they apply from the current scanline, so a
  layer scrolled mid-picture tore into two positions. All NBG scroll and zoom
  registers are now written only in VBlank: by `sat_vdp2_layers_commit()`, or
  by a setter that runs in VBlank. `vdp2_layers_demo` sets NBG2 to (4, 0) at
  scanline ~101 and back to (0, 0) at ~140 every frame, and
  `test_vdp2_layers.py` checks that every line shows the (0, 0) position.

## [0.1.0]

First installable release: LibSaturn can now be consumed as a CMake package
instead of being copied into a game repository.

### Added

- Root CMake project exporting `LibSaturn::Saturn`, `LibSaturn::Core` and
  `LibSaturn::Startup`, with a relocatable, version-aware `LibSaturnConfig.cmake`.
- `libsaturn_configure_executable()` (link contract, link map, post-link
  static-constructor guard) and `libsaturn_add_binary()` for package consumers.
- `libsaturn_add_disc()`: ISO/BIN/CUE from a linked program through the shipped,
  checked disc tools (needs Python 3 and mkisofs/genisoimage/xorrisofs).
- `cmake/toolchains/sh2eb-elf.cmake`, shipped in the install tree.
- Single runtime source manifest (`cmake/LibSaturnSources.cmake`) shared by CMake
  and the Makefile.
- `saturn/version.h` with `SATURN_VERSION_*` macros, generated from `VERSION`.
- Conan 2 recipe, `os=baremetal`/`arch=sh2eb` settings extension and the
  `saturn-sh2eb` host profile; `test_package` consumer.
- CMake presets: `saturn-debug`, `saturn-release`, `host-tests`, `package-consumer`.
- Package gates: header smoke tests, installed-package consumer test,
  Makefile/CMake flag-parity test; `scripts/test-package.{sh,ps1}`.
- CI workflow for the package gates and a tag-driven release workflow producing a
  source archive, a prebuilt prefix, a Conan cache and `SHA256SUMS`.
- `LibSaturn::Sim2D`: the hardware-free 2D runtime modules shipped as sources
  (`share/libsaturn/sim`) so a consumer can compile its game logic and run host tests with its own
  compiler, and `LIBSATURN_STAGE2D_TOOL`, the installed `stage2d_tool.py` (with its `stage2d/`
  package) that turns a stage spec into C data.
- Generic 2D runtime modules (Terrain2, Character2, Physics2, Path2, Follow Camera2D, stage map,
  entity streaming, sprite clips, task scheduler); see
  `docs/SONIC_CLASS_2D_RUNTIME_REFACTOR_PLAN.md`.

### Changed

- `saturn.h` includes `saturn/version.h`.
- `example_util.h` moved from the public `include/saturn/` tree to
  `examples/common/`; examples include it as `"example_util.h"` (the Makefile adds
  `-Iexamples/common` to example builds only).
- `saturn.ld` keeps `.preinit_array`, `.init_array` and `.ctors` sections
  (empty, so free) so the post-link constructor guard can see them.

### Fixed

- `assets/boot/ip_yaul_template.bin`, the Makefile's default IP.BIN template, was
  git-ignored (`*.bin`) and never committed, so a clean checkout could not build any
  disc image. It is now tracked.
- The static-constructor guard could never fire: `--gc-sections` discarded the
  constructor tables before the check ran, so a global needing a constructor
  passed silently while staying null on the console.

### Removed

- The `ikemen_saturn` example, its converters, tests and upstream-oracle tooling
  moved to the independent consumer repository
  [celsowm/ikemen-saturn](https://github.com/celsowm/ikemen-saturn), which builds
  against the installed package. LibSaturn keeps no Ikemen-specific build or test
  knowledge.
- Stray repository-root artifacts (`ip.bin`, `probe.json`, `build_and_log.ps1`);
  `$tmp` helper scripts moved to `scripts/dev/`.

- The `high_speed_platformer` example and its host tests moved to the independent consumer repository
  [celsowm/high-speed-platformer-saturn](https://github.com/celsowm/high-speed-platformer-saturn),
  which builds against the installed package (or the Conan package) only.

### Not supported

- vcpkg. See `docs/VCPKG_FEASIBILITY.md`.
