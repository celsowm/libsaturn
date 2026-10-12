# Changelog

All notable changes to LibSaturn are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/). While the major version is `0`, a
minor release may change the installed package contract.

The version is defined once, in the `VERSION` file.

## [Unreleased]

### Added

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

### Fixed

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
