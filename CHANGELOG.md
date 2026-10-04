# Changelog

All notable changes to LibSaturn are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[Semantic Versioning](https://semver.org/). While the major version is `0`, a
minor release may change the installed package contract.

The version is defined once, in the `VERSION` file.

## [Unreleased]

### Added

- `sat_vdp2_sprite_set_palette_priorities(priority[8])`: per-sprite VDP2
  priority. The sprite layer switches to sprite type 1 (palette dots only),
  and an INDEXED8 sprite picks one of eight priority selectors with
  `palette_override = (selector << 5) | bank`. RGB dots are not shown while it
  is on; sprite colour calculation and this API refuse each other with
  `SAT_ERR_BUSY`; hi-res is `SAT_ERR_UNSUPPORTED`; `NULL` restores type 0.
  `vdp2_layers_demo` draws one square in front of NBG0 and one behind it in
  the same frame, checked by `test_vdp2_layers.py`.
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

### Fixed

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
