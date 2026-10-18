# LibSaturn Current Coverage

This document is a snapshot of the hardware and runtime coverage currently exposed by LibSaturn.

**Snapshot date:** 2026-10-16
**Baseline:** `main` at `a7ea2c5`

The VDP2, sound and tooling rows were re-audited against the current sources,
headers and harness scripts on the baseline above; earlier snapshot claims
that contradicted the implemented API (notably "VDP2 advanced raster effects
NOT EXPOSED") have been corrected.

The goal is not to measure a percentage of the Sega Saturn hardware. Instead, this document answers a more useful question:

> Which Saturn subsystems already have a usable LibSaturn abstraction, which are only partially exposed, and which are still essentially unexplored by the library?

Bundled Sega hardware documentation does **not** count as implementation coverage. A subsystem is considered covered only when there is runtime code and, preferably, a public API, test, example, or tool that exercises it.

## Status legend

| Status | Meaning |
|---|---|
| **SUBSTANTIAL** | Usable public abstraction exists and the subsystem is already a meaningful part of LibSaturn. |
| **PARTIAL** | Useful functionality exists, but only a subset of the hardware or expected runtime abstraction is covered. |
| **MINIMAL** | Only enough support exists for bootstrap/basic operation. |
| **NOT EXPOSED** | No meaningful public runtime API was found in the snapshot. |
| **DOCS ONLY** | Hardware documentation exists in the repository, but there is no corresponding supported runtime subsystem. |

## Executive summary

LibSaturn is already strongest in the areas needed to render and run small-to-medium bare-metal games:

- bare-metal startup and frame lifecycle;
- VDP1 2D primitives and textured rendering;
- a growing VDP1-oriented 3D stack;
- VDP2 NBG0 to NBG3 and RBG0 layers, plus windows, mosaic and the per-line
  raster/line-colour family;
- sprite/VDP2 color-calculation support used by distance fading;
- fixed-point math, models, animation, collision, spatial and physics helpers;
- PCM playback, disc-streamed bounded music (`sat_music_*` through the library
  disc scheduler) and echo/reverb effects through the SCSP DSP, with a resident
  68000 sound driver for timestamped register writes;
- controller input across the SMPC peripheral family (digital and 3D pads, mouse, keyboard, multitap) and the RTC;
- host-side asset conversion, atlas packing, model baking and city/stage pipelines;
- ELF -> BIN -> ISO build flow.

The largest Saturn hardware areas still missing as first-class LibSaturn subsystems are:

- ROM cartridges and other A-Bus expansion hardware beyond detection and raw reads;
- broader Slave SH-2 subsystem offload (physics, renderer preparation) beyond the managed scheduler;
- more SCU DSP kernels (only a batch 3x3 transform ships);
- SCSP effects beyond the shipped echo and reverb, slot synthesis, and a 68000 driver beyond timed register writes;
- the remaining VDP2 rotation and special-effect territory: the rotation window and per-dot coefficient corners of parameter B;
- double-density interlace and the dedicated-monitor video modes (single-density interlace and 224/240/256-line output are exposed);
- NetLink modem layer and MPEG decoding (only a UART driver and a card probe/start exist, unverified on hardware).

## Public API surface

The umbrella header `include/saturn/saturn.h` is the authoritative list: it
pulls **82 subsystem headers** transitively as of the baseline above (the 3D
stack, 2D runtime, physics, input, storage, audio and VDP families listed
there). The coverage matrix below cites headers by name; see the umbrella
header itself rather than duplicating the list here.

A second, smaller group of subsystem headers is **public but opt-in**: it is
not pulled by the umbrella and a program includes it directly when it needs
that subsystem:

```text
abus.h            cart_overlay.h     cd_mount.h         disc_stream.h
loader.h          lz.h               mpeg.h             physics3_transform.h
sampler.h         scsp_dsp.h         scu_dsp.h          sound_driver.h
tate_fb.h         uart.h             vdp2_alloc.h       vdp2_color_offset.h
vdp2_compose.h
```

This is still a useful high-level picture of what LibSaturn treats as
supported public runtime functionality.

## Coverage matrix

| Area | Status | Current coverage | Major gaps / next territory | Evidence |
|---|---|---|---|---|
| Bare-metal startup/runtime | **SUBSTANTIAL** | Startup, linker/runtime integration, initialization and game-oriented frame lifecycle. | More system services and richer platform introspection. | `src/core/startup`, `src/core/runtime`, `include/saturn/core.h`, `include/saturn/app.h` |
| Frame timing / VBlank | **SUBSTANTIAL** | Begin/end frame; VBlank wait driven by the VBlank-IN interrupt (polling fallback when a host never delivers it); exact display-frame count; `sat_time_ms` observed every frame, so it stays exact through long busy work. PAL: `sat_init` accepts `SAT_VIDEO_PAL` (50 frames per second time base) and `SAT_VIDEO_AUTO` (the console's own standard from VDP2 TVSTAT; `sat_app_init_default` and the examples use it), `sat_video_is_pal()` reads the console; `sat_time_ms`, the audio clock and frame-based durations follow it. Acceptance wrappers run on Ymir in both standards (`harness/run-acceptance-sweep.ps1 [-Pal]`). | 240/256-line output and single-density interlace are available through `sat_init` (`saturn/video.h`; E09 covers the NTSC and PAL mode matrix). A PAL console still runs 224-line modes with borders by design; 256 lines is refused on NTSC, double-density interlace is refused, and nothing has run on a real PAL console (`docs/HARDWARE_ACCEPTANCE_CHECKLIST.md`). | `include/saturn/video.h`, `include/saturn/time.h`, `src/hal/scu/scu.*` |
| VDP1 sprites | **SUBSTANTIAL** | Normal, scaled and distorted sprites; screen/native coordinate helpers. | Additional command/state abstractions and broader hardware modes. | `include/saturn/vdp1.h` |
| VDP1 primitives | **SUBSTANTIAL** | Polygon, polyline, line and rectangle helpers. | More generalized render-state control. | `include/saturn/vdp1.h` |
| VDP1 textures/palettes | **SUBSTANTIAL** | Indexed-8 texture upload, palette upload, texture VRAM management paths. | More source formats and runtime streaming/upload strategies. | `include/saturn/vdp1.h` |
| VDP1 Gouraud | **SUBSTANTIAL** | Managed Gouraud tables and Gouraud polygon/polyline/line drawing; near/screen-clipped RGB faces keep per-corner Gouraud. | Gouraud needs RGB pixels, so palette textures cannot use it (tint goes through palette variants instead), nor can hi-res (8-bit) VDP1 output; there the VDP2 lights sprites per line instead (`sat_vdp2_hires_sprite_color_calc_set` + `sat_vdp2_sprite_set_line_color_insert`, `examples/vdp2_hires_light`). | `include/saturn/vdp1.h`, `include/saturn/render3d.h` |
| Sprite color calculation and blending | **SUBSTANTIAL** | Ratio slots with nearest-ratio snap (strict opt-in) for alpha and distance fade; add mode for `SAT_BLEND_ADD` with a per-frame ratio/add claim; VDP1 shadow as `SAT_BLEND_SUBTRACT`; VDP2 colour offset A/B per layer; per-sprite RGB tint through palette variant banks. RGB sprite pixels stay visible with colour calc configured. | Per-line ratios. A per-sprite dst - src is not a hardware operation. | `include/saturn/vdp1_color_calc.h`, `include/saturn/vdp2_color_calc.h`, `include/saturn/vdp2_color_offset.h`, `include/saturn/render2d.h`, `docs/TRANSPARENCY_ALPHA.md` |
| 3D math | **SUBSTANTIAL** | Fixed-point game/3D math abstractions used by renderer and gameplay systems. | SIMD-like/parallel acceleration paths where useful. | `include/saturn/math3d.h` |
| Mesh/model pipeline | **SUBSTANTIAL** | Mesh/model runtime plus offline OBJ/MTL/image baking for VDP1-compatible faces, with a caller-owned camera/model scene facade. | Runtime asset streaming, broader import formats and richer material model. | `include/saturn/mesh3d.h`, `include/saturn/model3d.h`, `include/saturn/scene3d.h`, `tools/import_model.py` |
| 3D renderer | **SUBSTANTIAL** | Dedicated render layer on top of VDP1-oriented geometry; per-pass painter with 24-bit depth keys; UV-preserving near clip of textured faces on an N x N cell grid with cut-cell fill; Gouraud-preserving clip; bake-time and runtime intersecting-face splits; capture of direct draws into the painter. | VDP1 has no Z-buffer: non-intersecting cyclic overlaps are not resolved. Command upload by SCU DMA and Slave-prepared command lists (phase D). | `include/saturn/render3d.h`, `include/saturn/scene3d_faces.h`, `tools/model_pipeline/intersections.py` |
| 3D animation | **SUBSTANTIAL** | Public animation subsystem integrated with the 3D stack. | More advanced animation tooling/runtime features as needed. | `include/saturn/anim3d.h` |
| Distance fade | **SUBSTANTIAL** | Public 3D distance-fade abstraction backed by Saturn sprite color calculation. | Additional policies and integration with future LOD/streaming systems. | `include/saturn/fade3d.h`, `include/saturn/vdp2_color_calc.h` |
| Collision / spatial / physics | **PARTIAL** | 2D and 3D collision helpers plus spatial/physics public modules. | Broader solver/body/scene features and optional future use of the low-level Slave API. | `include/saturn/collide2d.h`, `include/saturn/collide3d.h`, `include/saturn/spatial.h`, `include/saturn/physics.h` |
| VDP2 NBG0 | **PARTIAL** | NBG0 initialization, scroll, priority, enable/disable, indexed-8 tiled upload and map writes. | Superseded for multi-layer work by `saturn/vdp2_layers.h`; this API stays for single-layer programs. | `include/saturn/vdp2.h` |
| VDP2 RBG0 | **PARTIAL** | Bitmap configuration, rotation parameters, coefficient control, matrix/viewpoint/center/scaling helpers and Mode-7-style setup. | More complete rotation modes, coefficient workflows and advanced compositing. | `include/saturn/vdp2.h` |
| VDP2 windows / mosaic / per-line raster effects | **SUBSTANTIAL** | Line scroll per line or every 2/4/8 lines (horizontal, vertical, zoom) with a sine-wave helper, vertical cell scroll tables, per-line back screen and line colour screen, and per-layer/per-sprite line colour insert (`saturn/vdp2_layers.h`). Windows W0/W1 as rectangles or per-line tables (ellipse helper), per-screen window control with OR/AND logic including sprite-window selection through palette sprites with window dots, mosaic per screen (refused with `SAT_ERR_UNSUPPORTED` where the manual forbids it, e.g. NBG0/NBG1 vertical cell scroll), per-screen colour calculation ratios, CRAM mode 0/1 and hi-res palette-sprite colour calculation (`saturn/vdp2_compose.h`). Ymir and Mednafen acceptance: `harness/run-vdp2-effects.ps1`, `harness/run-vdp2-compose.ps1`. | RBG0 rotation covers parameter sets A and B with per-line coefficient tables and all three non-window switching modes (`harness/run-vdp2-rotation-coefficients.ps1`); RBG1 is a first-class layer in this slice with the manual's separate-image-bank contract and the normal-screen exclusion (`harness/run-vdp2-dual-rotation.ps1`); the rotation window mode (RPMD 3), per-dot coefficients and the coefficient line-colour composition remain refused/undocumented; extended colour calculation is public and accepted by `vdp2_special_color` (`harness/run-vdp2-special-color.ps1`: CCCTL/CCRNA registers, refusals and manual-recomputed ratio/add/cascade pixels), and hardware blur by `vdp2_hardware_blur` (`harness/run-vdp2-hardware-blur.ps1`: CCCTL/watch-write, a byte-identical sharp control half and manual-recomputed 2:1:1 averages; the harness pins the documented Ymir SSE2 blur fix); special priority and special colour calculation by `vdp2_special_priority` (`saturn/vdp2_special.h`, `harness/run-vdp2-special-priority.ps1`: SFPRMD/SFCCMD/SFCODE/SFSEL/CCCTL, watch-write, per-character/per-dot priority and 15:17 pixels with 2-word names), and VDP2 sprite shadows by `vdp2_sprite_shadows` (`saturn/vdp2_sprite_shadow.h`, `harness/run-vdp2-sprite-shadows.ps1`: SDCTL targets/TPSDSL, type-2 normal and MSB words, VDP1 MSB-on self-shadow, a VDP1 5-bit-shadow control, manual-recomputed pixels and the target-off control); the stand-alone VRAM free-list allocator (`vdp2_alloc.h`) and the cycle planner are not yet one transactional resource owner; the video-mode contract now covers 320/352/640/704 by 224/240/256 and single-density interlace; double-density interlace and dedicated-monitor modes remain refused. | `include/saturn/vdp2_layers.h`, `include/saturn/vdp2_compose.h`, `include/saturn/vdp2_special.h`, `include/saturn/vdp2_sprite_shadow.h`, `include/saturn/vdp2_alloc.h`, `harness/run-vdp2-effects.ps1`, `harness/run-vdp2-compose.ps1`, `harness/run-vdp2-rotation-coefficients.ps1`, `harness/run-vdp2-dual-rotation.ps1`, `harness/run-vdp2-special-priority.ps1`, `harness/run-vdp2-sprite-shadows.ps1` |
| VDP2 sprite shadows | **SUBSTANTIAL** | `saturn/vdp2_sprite_shadow.h` owns SDCTL (six target enables and TPSDSL), switches the sprite layer to type 2 palette-only, and encodes the normal `0x47FE` and transparent-MSB `0x8000` dot words plus the MSB-on form; VDP1's MSB-on command is exposed as `SAT_SPRITE_FLAG_MSB_ON`. Ymir acceptance by `harness/run-vdp2-sprite-shadows.ps1`: a scrolling ground, normal/MSB shadow pixels recomputed from the palette and scroll, a VDP1 MSB-on self-shadow, a VDP1 5-bit shadow control, the SDCTL target-off control and a watch-write of `0x0101`. Host tests cover the encoders and every refused configuration. | The mode is palette-only by construction; the library cannot inspect VDP1 draws, so RGB dots drawn while it is active are the caller's responsibility. The palette priority/window/CC features and the VDP1 sprite colour-calculation module refuse to coexist with shadow mode, and selector 1 survives a colour-calc commit. Normal shadows can only target scroll/back screens; a sprite shadow needs the MSB-on pass over an existing palette dot. TPSDSL is host-tested and set, but pinned Ymir does not gate the transparent MSB shadow on it, so the TPSDSL=0 negative is unverified. Physical hardware PENDING (checklist row 33). | `include/saturn/vdp2_sprite_shadow.h`, `include/saturn/vdp1.h`, `harness/run-vdp2-sprite-shadows.ps1`, `harness/tests/test_vdp2_sprite_shadows.py` |
| VDP2 dynamic tile/map streaming | **SUBSTANTIAL** | `saturn/stage_map2.h` keeps a bounded 45x33-cell resident window, stages only entering cells into caller-owned storage and refuses capacity without changing state. E07 adds `sat_vdp2_layer_set_pattern_name_base` for a VBlank-staged one-plane ring switch and `sat_vdp2_vram_read_words` for CRC diagnostics. `vdp2_dynamic_tile_stream` alternates two 64x64 pattern-name rings, watches MPABN0 0x0000/0x0202, verifies model == VRAM CRC with zero sentinel cells, recomputes the frozen CRC from Ymir's VRAM dump, and tracks SCU-DMA vs CPU transfers. The 36,000-frame `-Stress` run passed 8/8: frozen at 33,000 after 34,905 guest frames and 32,999 switches, CRC 0x517824CD, max pending 1,485, 756,900 staged/committed words, 192 SCU-DMA and 33,659 CPU transfers. | One-plane cell maps only for the live base switch; the caller composes the double buffer from two public streamers. The example's map is procedural, so CD/VFS chunk loading is not demonstrated. Physical hardware PENDING (checklist row 34). | `include/saturn/stage_map2.h`, `include/saturn/vdp2_layers.h`, `include/saturn/vdp2.h`, `harness/run-vdp2-dynamic-tile-stream.ps1`, `harness/tests/test_vdp2_dynamic_tile_stream.py` |
| VDP2 video modes (E09) | **SUBSTANTIAL** | `saturn/video.h` exposes the resolved mode, `sat_video_mode_supported()`, `sat_video_interlace_mode()`, `sat_video_tvmd()` and `sat_video_tvstat()`. `sat_init` accepts 320/352/640/704 by 224/240/256 and writes TVMD's HRESO/VRESO/LSMD through `src/hal/video_mode_logic.hpp`. 256 lines is PAL-only; single-density interlace is legal at every height and produces a twice-tall picture; double-density interlace is refused with `SAT_ERR_UNSUPPORTED` because it needs per-field VDP1 pictures. `examples/vdp2_video_modes` exercises every supported mode; the harness proves five modes on NTSC and all eight on PAL, with the expected output sizes and TVMD bits. | A mode change is a full `sat_shutdown()` + `sat_init()`; the library has no live TVMD switch (the manual requires the BIOS routine and a reset when leaving dedicated-monitor modes). Double-density interlace and dedicated-monitor modes remain absent. Physical hardware PENDING (checklist row 35). | `include/saturn/video.h`, `src/hal/video_mode_logic.hpp`, `tests/host/test_video_mode.cpp`, `harness/run-vdp2-video-modes.ps1`, `harness/tests/test_vdp2_video_modes.py` |
| VDP2 showcase integration (E10-E13) | **SUBSTANTIAL** | `vdp2_parallax_world` combines four NBGs, RBG0, line scroll, back/line-colour tables, window W0 and add-mode colour calculation, and includes a measured VDP1 pre-rendered reference path. `vdp2_fighting_stage` puts a four-NBG stage behind the existing two-fighter character demo through `examples/characters/stage.c`, with a live stage on/off comparison of the same simulation. `vdp2_neon_city` integrates a rotating RBG0 sign, `sat_stage_map2` double-buffered street streaming, a moving colour-calculation/line-colour light pool and per-layer colour offsets, with model/VRAM CRC and SCU-DMA/CPU telemetry. `vdp2_parallax_lab` (E13) drives two 8bpp bitmap NBGs and two two-word cell NBGs from a dedicated asset converter, panning the four art layers at independent speeds. All four use public layer/compose/colour APIs, no raw register pokes. | The stage is an E11-only compilation of the shared character demo; `fighter_arena_3d` keeps its legacy NBG0 background. The neon city's light pool is an artistic approximation of a wet reflection, not a physically based one. E13's source art is an external four-layer lab, not seamless, and is reduced/quantized to 512x256; alpha is thresholded. All timings are emulator-side; physical hardware PENDING (checklist rows 36-38 and E13). | `examples/vdp2_parallax_world`, `examples/vdp2_fighting_stage`, `examples/vdp2_neon_city`, `examples/vdp2_parallax_lab`, `harness/run-vdp2-parallax-world.ps1`, `harness/run-vdp2-fighting-stage.ps1`, `harness/run-vdp2-neon-city.ps1`, `harness/run-vdp2-parallax-lab.ps1` |
| VDP2 remaining normal backgrounds | **SUBSTANTIAL** | `saturn/vdp2_layers.h` manages NBG0 to NBG3 together: cell format (1- or 2-word names, 1x1 or 2x2 characters, 1x1 to 2x2 pages per plane, 16 to 16.77 M colours, NBG2/NBG3 at 16 or 256) and bitmap format (NBG0/NBG1, four sizes), map planes A-D, priority, transparency, scroll, zoom (1/4 to 8x, with the horizontal-reduction bandwidth accounted for). A pure allocator places every layer's pattern-name, character and vertical-cell-scroll reads into the eight timings of each VRAM bank under the manual's rules and refuses (`SAT_ERR_CAPACITY`, nothing changed) when they cannot all be fetched; an independent checker validates its output against the manual's own Figure 3.8 example. Ymir and Mednafen draw all four layers in the right order (`harness/run-vdp2-layers.ps1`). Raster effects: line scroll per line or every 2/4/8 lines (horizontal, vertical, zoom), vertical cell scroll tables, per-line back screen and line colour screen, with a sine-wave helper; Ymir shows the wave on all 224 lines and a per-column ripple (`harness/run-vdp2-effects.ps1`). Composition (`saturn/vdp2_compose.h`): windows W0/W1 as rectangles or per-line tables (an ellipse helper), per-screen window control with OR/AND, a colour calculation window, mosaic per screen, and per-screen colour calculation ratios; Ymir and Mednafen agree on the picture (`harness/run-vdp2-compose.ps1`). | The sprite window is opt-in rather than universal: its window dots must be palette sprites (`sat_vdp2_sprite_set_palette_window`), so the library's default RGB sprite path cannot serve as the window source; RBG0 and the older NBG0 calls own the same registers, so the two managers refuse each other (`SAT_ERR_BUSY`); high-resolution modes (4 timings per cycle) are not planned. The transactional resource plan (`sat_vdp2_plan_*`, `vdp2_alloc.h`) owns reservations and whole-bank claims over the free list with rollback and per-bank telemetry (`harness/run-vdp2-vram-stress.ps1`); CRAM ranges and the cycle planner are not yet part of that transaction. | `include/saturn/vdp2_layers.h`, `src/hal/vdp2/nbg_logic.hpp` |
| Digital pad | **SUBSTANTIAL** | Held/pressed/released state and digital Saturn pad buttons for every port and every tap behind a multitap; connect/disconnect and button events with the tap in the event; device discovery (`sat_input_device_info`). One INTBACK now serves both ports. | Devices are read on `sat_input_poll` / `sat_pad_poll_port`, not from an SMPC interrupt. | `include/saturn/input.h`, `src/hal/smpc/peripheral_logic.hpp` |
| 3D Control Pad / analog | **SUBSTANTIAL** | `sat_pad_analog`: X, Y and the two analog triggers of a 3D Control Pad in analog mode (digital mode reads as a plain pad), the third axis of a three-axis stick, axis events. Ymir (scripted axis values) and Mednafen (device kinds) acceptance in `harness/run-input-devices.ps1`. | Racing wheel and six-axis Mission Stick data are parsed as generic axes only; nothing reads the pad's mode switch. | `sat_pad_analog`, `sat_analog_state_t` |
| Multitap / peripheral family | **PARTIAL** | Sega 6-player adapter and Sega Tap: the full INTBACK with CONTINUE requests reads every 32-byte chunk and files up to six devices per port (`sat_pad_tap_state`, `tap` in events). Shuttle Mouse and keyboard (`sat_mouse_read` accumulates motion, `sat_keyboard_read` reports keys and locks). Host tests on recorded byte streams; Ymir mouse; Mednafen 6-player adapter (`TAPS 111111`), mouse and keyboard kinds. | Light guns are reported as `SAT_DEVICE_UNKNOWN` (their protocol runs in SH-2 direct mode and needs the VDP2 latch); no emulator here drives a keyboard's keys or a multitap's individual pads with scripted input, so those decoders are host-tested only. | `include/saturn/input.h` |
| SMPC system services | **SUBSTANTIAL** | `saturn/smpc.h`: status block (RTC, cartridge and area code, system status), `sat_rtc_get` / `sat_rtc_set` (SETTIME with the weekday derived and impossible dates refused before any write), the four battery-backed bytes (SETSMEM and a status read-back), reset enable/disable, sound and Slave on/off. Ymir round trips restore the clock and SMEM. | The manual marks MSHON, CDON/CDOFF, SYSRES and CKCHG352/320 as prohibited for applications, so they are deliberately not exposed; NMIREQ is not exposed either (the Master has no NMI handler of ours). | `include/saturn/smpc.h`, `src/hal/smpc/*` |
| SCU interrupts | **SUBSTANTIAL** | All 14 internal sources (VBlank-IN/OUT, HBlank-IN, timers 0/1, DSP end, sound request, SMPC, PAD, DMA 0-2 end, DMA illegal, sprite draw end) with per-source handlers and counts, IMS shadow restored by every handler, SR mask lowered only to the lowest enabled level, timer 0 line / timer 1 dot setup. Ymir and Mednafen acceptance (`harness/run-irq-demo.ps1`). | A-bus external interrupts (cartridge devices) stay masked. Handlers run in interrupt context: no frame or draw calls. | `include/saturn/irq.h`, `src/hal/scu/irq.*`, `examples/irq_demo` |
| SCU DMA | **SUBSTANTIAL** | `sat_dma_*`: levels 0-2, direct and indirect (up to 16 entries per start), asynchronous start / busy / wait with the DMA-end interrupt, forced stop and `SAT_ERR_TIMEOUT` when a transfer never ends (so `NEEDS_RECOVERY` is reachable again). The routes the SCU manual forbids (A-bus writes, VDP2 reads, Work RAM-L, RAM to RAM, B-bus to B-bus) are refused with `SAT_ERR_UNSUPPORTED`; `sat_dma_copy` falls back to the CPU for them and for copies under 64 bytes. Library uploads go through it: VDP1 textures, VDP2 colour-RAM palettes, VDP2 VRAM blocks and Sound RAM samples. Destination Work RAM is invalidated in the cache after a transfer. Host tests for the routing/encoding logic and the API; Ymir (9/9) and Mednafen acceptance (`harness/run-dma-demo.ps1`). Mednafen: 512 KiB into VDP1 VRAM takes 108 ms by CPU and 11 ms by DMA. | The SH-2 on-chip DMAC is exposed only as the explicit `sat_dma_copy_sh2` (Work RAM to Work RAM, including Work RAM-L): Mednafen measures it slower than the CPU loop (53 ms against 38 ms for 512 KiB), so `sat_dma_copy` never picks it. DSP-side DMA; the CD block / A-bus as a DMA source is untested; the per-frame VDP1 command and Gouraud tables go by DMA only when `sat_dma_set_command_list(1)` is set (default off: see the master plan for the measurements). Ymir charges DMA no CPU time, so only Mednafen gives a timing. | `include/saturn/dma.h`, `src/hal/scu/dma*`, `examples/dma_demo` |
| SCU DSP | **PARTIAL** | `saturn/scu_dsp.h`: program and data RAM access through the ports (refused while a program runs), start/stop/status/wait, and a built-in batch 3x3 transform (integer vectors times a 16.16 matrix, 21 vectors per run) in two forms: the CPU moves the data, or the DSP DMAs it in and out of Work RAM itself so the SH-2 only writes a few parameters. `tools/scu_dsp.py` assembles Sega mnemonics (parallel ALU/X/Y/D1 operations, MVI, DMA/DMAH with address addition, JMP, BTM/LPS, END/ENDI, ORG/EQU/IF/IFDEF, `-D`) and simulates programs on the host with the Ymir core semantics. The transform matches the SH-2 int64 path word for word on the host simulator, on Ymir and on Mednafen (`harness/run-scu-dsp.ps1`). | Only one kernel; no speed claim (emulator DSP timing is approximate: with the CPU moving the data the DSP took about 1.7-2x the SH-2's time, with DSP DMA about the same, but the SH-2 is free during a run); DSP DMA to A-Bus/B-Bus is not exercised; the DSP-end interrupt is available through `sat_irq` but the API polls. | `include/saturn/scu_dsp.h`, `src/hal/scu/dsp*`, `src/core/runtime/scu_dsp.cpp`, `tools/scu_dsp.py`, `examples/scu_dsp_demo` |
| SCSP PCM playback | **PARTIAL** | PCM S8/S16 sounds, resident sound data, bounded music streams (including disc-scheduler-streamed tracks with spread SCSP refills and standby prefetch through the `sat_music_*` family), looping, voices, volume, pan, pitch and voice statistics/stealing. | Slot envelope generators, LFO/modulation and synthesis-oriented APIs; long-run/CD hardware validation (see also the SCSP DSP row below for effects). | `include/saturn/audio.h`, `src/hal/scsp.*`, `src/audio/playback/music.cpp` |
| SCSP DSP / effects | **PARTIAL** | `saturn/scsp_dsp.h`: an echo (1-1000 ms, feedback, wet and input gains) and a four-comb reverb, loaded into the SCSP DSP with a delay line taken from the Sound RAM pool (`sat_effect_echo_start`, `sat_effect_reverb_start`, `sat_effect_set_gains`, `sat_effect_stop`). Voices send to the DSP through slot send levels (`sat_effect_set_default_send`, `sat_voice_set_effect_send`) and the wet signal returns on EFREG 0 with a level and pan (`sat_effect_set_return`); `sat_effect_read_ring` reads the delay line back. The microprograms are generated by `tools/scsp_dsp.py` (field encoder plus a simulator with the Ymir DSP semantics, header freshness test). Verified on Ymir and Mednafen (`harness/run-scsp-dsp.ps1 [-Mednafen]`): the recorded output shows the echo repeating every 100 ms at 0.3 then 0.15 of the click, the delay line in Sound RAM holds the writes one delay apart, and the reverb repeats at its four delays (1687/2053/2411/2851 samples) at 0.0875 of the click; both emulators agree with the simulator to within a sample. | Two fixed programs (mono return on EFREG 0, no chorus, flanger, filters or stereo reverb), linear (non-floating) memory access only, one effect at a time; every voice sends to MIXS 0; loading takes about 10 samples plus the ring clear, and a program half-written into a running DSP would write garbage into the ring, so the loader writes it with zero gains and clears the ring afterwards. Not measured on a real Saturn. | `include/saturn/scsp_dsp.h`, `src/hal/scsp/{dsp.*,dsp_logic.hpp,scsp_dsp_presets.h}`, `src/audio/effects/api.cpp`, `tools/scsp_dsp.py`, `examples/scsp_dsp_demo` |
| SCSP synthesis / envelopes / LFO | **NOT EXPOSED** | Public audio API is centered on PCM voices. | Hardware envelope generators, LFO/modulation and richer slot synthesis. | `include/saturn/audio.h` |
| 68000 sound CPU | **PARTIAL** | `saturn/sound_driver.h`: a resident 68000 program (`src/hal/scsp/sound_driver.m68k`, assembled by `tools/build_sound_driver.py` into a committed image with a freshness test) loaded into Sound RAM. It wakes on SCSP timer A (one tick per 256 samples, 172.27 Hz), runs a 63-slot ring of timestamped SCSP register writes and logs the tick and lateness of each. `sat_sound_play` with `SAT_SOUND_PLAY_AT_TICK` hands its key-on to the driver, so a sound starts on its tick whatever the SH-2 is doing. Verified on Ymir (`harness/run-sound-driver.ps1`): every event ran within one tick of its due tick, the tick rate matched the emulated frame rate within 3 ticks over 180 frames, and the keyed voices reached the SCSP slots. | The driver schedules register writes only: no sequencer, no sample streaming and no SFX mixing on the 68000; a voice scheduled for a later tick and stopped before then is still keyed on when the tick arrives. `STOP` is avoided since Ymir does not resume from it. | `include/saturn/sound_driver.h`, `src/hal/scsp/{driver.*,driver_logic.hpp,sound_driver.m68k}`, `src/audio/driver/api.cpp`, `examples/sound_driver_demo` |
| CD Block runtime I/O | **PARTIAL** | Hardware-specific synchronous 2048-byte sector reads, bounded command polling, LBA/FAD conversion, a `sat_cd_device_t` adapter, plus modified-Ymir BIOS-harness validation of both a PVD probe and CD-resident PCM playback are exposed. Above the reader, the library disc scheduler (`saturn/disc_stream.h`) timeshares the one drive between music and data jobs, with a standby track and gapless handoff (`sat_music_use_disc_stream`). | Authentication policy, asynchronous sector I/O at the `cd_block.h` layer itself, and richer error/status reporting. Seek/read scheduling exists at the `disc_stream.h` layer, not per-command. | `include/saturn/cd_block.h`, `include/saturn/disc_stream.h`, `src/hal/cd/block.cpp`, `examples/cd_block_probe`, `examples/cd_streaming_jukebox` |
| CDFS / VFS | **PARTIAL** | Read-only logical paths, bounded handles, caller-backed blobs, caller-owned `read_at` backends, ISO9660 PVD/directory lookup, CDFS-to-VFS file adapters, non-resident music refill and cooperative asset prefetch/cache through the VFS path are public. `cd_streaming_jukebox` stages public-domain PCM as ISO files and joins this complete route. | Transparent manifest-driven CD registration and a genuinely non-blocking storage backend remain outside the current synchronous CD contract. | `include/saturn/cd.h`, `include/saturn/cdfs.h`, `include/saturn/file.h`, `include/saturn/asset.h`, `src/storage/cd/filesystem.cpp`, `src/resources/assets.cpp`, `examples/cd_streaming_jukebox` |
| Asset streaming | **PARTIAL** | File reads support partial backend transfers; typed resident loading, bounded embedded/non-resident logical music streams, fixed-block cache, cache diagnostics and cooperative prefetch require no whole-file allocation. | Texture/model streaming and general typed non-resident loaders; VDP2 map-name streaming is covered by the stage-map row. | `include/saturn/file.h`, `include/saturn/asset.h`, `src/resources/assets.cpp`, `src/audio/playback/music.cpp` |
| Backup RAM / save data | **PARTIAL** | Public internal-Backup-RAM save API over the Boot ROM BUP library: init, status/free-space, directory listing, read, write/overwrite, verify, delete and explicit format. Mutating BIOS calls are protected by SMPC reset-disable/reset-enable critical sections. `SAT_SAVE_BACKUP_CARTRIDGE` drives the same calls at the cartridge's BUP_Init configuration index (unit ID 2); with no cartridge every call is refused before any BIOS call. Verified with a fresh Ymir process reopening a throwaway 4 Mbit cartridge image (record count 1 then 2, internal image byte-identical, `harness/run-save-cartridge.ps1`) and on Mednafen. BUP_Dir has no wildcard: an empty name lists everything, and the old "*" listed nothing. | Backup Memory cartridge: partition selection (the one-partition case works), hardware acceptance on a real cartridge. RTC convenience metadata and optional versioned/CRC payload helpers. | `include/saturn/save.h`, `src/storage/save/api.cpp`, `src/hal/storage/backup.*`, `tests/host/test_save_api.cpp` |
| RAM cartridge | **PARTIAL** | Volatile 1 MiB / 4 MiB ID detection, two-bank arenas and offset-based logical buffers, optional cart-backed asset cache/VFS bridge, demo and host tests. | Emulator / physical-hardware acceptance; multi-bank cache and DMA tuning. | `include/saturn/ram_cart.h`, `src/hal/storage/ram_cart.cpp`, `src/storage/cartridge/api.cpp`, `docs/RAM_EXPANSION_CARTRIDGE.md` |
| Generic cartridge / A-Bus | **PARTIAL** | `saturn/abus.h`: ID-byte detection (none, 1 MiB / 4 MiB DRAM, Backup Memory with its capacity, unknown), bounds-checked byte reads of chip selects 0 and 1, writes only into the DRAM banks of a RAM expansion. A Backup Memory cartridge is never touched raw. Ymir with an empty slot, both DRAM carts and a backup cartridge (`harness/run-abus.ps1`); Mednafen agrees. | ROM cartridge drivers, other expansion hardware (CS2 belongs to the CD block). | `include/saturn/abus.h`, `src/storage/cartridge/abus*`, `src/hal/storage/abus.*`, `examples/abus_demo` |
| Slave SH-2 | **SUBSTANTIAL** | Public low-level lifecycle, dedicated Slave entry, FRT signaling, cache-through shared control block, directional one-slot mailbox, timeout/restart state, example and host protocol tests. On top of it `saturn/parallel.h` schedules work: four priority levels, task dependencies (a failed dependency cancels its dependents), batched dispatch (up to four ready tasks reach the Slave in one signal and run in order, so a dependency queued in the same batch runs first), Master-only tasks that wait for Slave tasks, and `sat_parallel_for`, a loop split between the two CPUs. Ymir runs both CPUs for real: priorities and batching hold, the split loop is bit-exact and 1.94x faster (`harness/run-parallel-sched.ps1`). | Interrupt-driven reception, physical Saturn validation, physics and renderer work on the Slave. | `include/saturn/dual_sh2.h`, `src/hal/dual_sh2/`, `src/hal/sh2/`, `examples/dual_sh2`, `docs/DUAL_SH2_LOW_LEVEL.md` |
| Runtime filesystem-independent asset API | **PARTIAL** | Bounded logical asset handles, metadata, caller-owned partial reads, typed texture/font/sound loaders, bounded non-resident music refill, fixed cache/prefetch service, and generated C registration for embedded/physical manifest entries are independent of the current storage representation. | Concrete RAM-cart backend and non-resident typed texture/model/map loaders. | `include/saturn/asset.h`, `src/resources/assets.cpp`, `src/audio/playback/music.cpp`, `tools/generate_asset_manifest.py`, `tools/generate_asset_registry.py` |
| Formatting / fonts / utility drawing | **PARTIAL** | Formatting, font and grid helpers are public. | Broader UI/text layout and asset-backed fonts if desired. | `include/saturn/fmt.h`, `include/saturn/font.h`, `include/saturn/grid.h` |
| NetLink / modem / communications | **PARTIAL (UNVERIFIED)** | `saturn/uart.h`: a driver for the 16550-family UART the NetLink cartridge is built around, over any register window (memory-mapped with stride 1/2/4, or callbacks): divisor and word format, FIFO detection, a scratch-register and loopback self-test on open (`SAT_ERR_NOT_CONNECTED` on an empty bus), sending with a poll limit, non-blocking receive and line-error counters. Host-tested against a model of the 16550; on Ymir and Mednafen the open on an empty A-Bus fails its self-test and nothing else is touched (`harness/run-expansion-probe.ps1`). | No NetLink address, modem (AT command/PPP) layer or interrupt-driven receive: neither emulator has a NetLink and the repository holds no NetLink documentation, and the CD Block shares that part of the A-Bus, so the caller must supply the address. A working modem is not claimed. | `include/saturn/uart.h`, `src/hal/comm/uart*.{cpp,hpp}`, `examples/expansion_probe_demo` |
| MPEG Card | **PARTIAL (UNVERIFIED)** | `saturn/mpeg.h`: `sat_mpeg_probe` (CD Block Get Hardware Info: MPEG, hardware and drive versions; Is Device Authenticated for the MPEG device) and `sat_mpeg_start` (authenticate, wait, MPEG Init), through the new generic `sat_cd_block_command`. Command layouts follow the Ymir CD Block core. Host-tested against a model of the CD Block for a present card (authentication polling, refusal, timeout); on Ymir the probe reports no card (MPEG version 0, hardware version 2, drive 6.0) and `sat_mpeg_start` returns `SAT_ERR_NOT_CONNECTED` having sent nothing, and Mednafen shows the same (`harness/run-expansion-probe.ps1`). | No decoding, stream connection, display or interrupt handling, and the present-card path has never run against a card: neither emulator emulates one. The authentication result value (2) is taken from Ymir's model; real hardware may differ. | `include/saturn/mpeg.h`, `src/hal/cd/mpeg*.{cpp,hpp}`, `examples/expansion_probe_demo` |

## Hardware coverage by major Saturn block

### Master SH-2

The Master SH-2 is where essentially all current game/runtime code executes. LibSaturn already provides a useful C API over an internal C++ implementation, a fixed-point 3D/game stack, rendering, collision, animation and general frame control.

The main CPU-side growth direction is not more Master SH-2 execution features, but **offloading broader subsystems (physics, renderer preparation) to the second SH-2 through the managed scheduler**.

### Slave SH-2

Current status: **SUBSTANTIAL — lifecycle, mailbox and a scheduling layer (priorities, dependencies, batches, a data-parallel loop)**.

The former "future job system" target exists now: `saturn/parallel.h`
schedules Slave work with four priority levels, task dependencies, batched
dispatch and `sat_parallel_for`, over the low-level `saturn/dual_sh2.h`
lifecycle/mailbox layer. Games do not build their own cache/synchronization
protocol.

The open part is which workloads actually run there. Likely first candidates:

- animation transforms;
- collision broad/narrow phase;
- decompression;
- visibility/culling;
- world update batches;
- asset preparation that does not touch VDP registers directly.

### SCU

Current status: **SUBSTANTIAL** for interrupts and frame timing (`saturn/irq.h`) and for SCU DMA (`saturn/dma.h`); the SCU DSP has a first API (`saturn/scu_dsp.h`) and a batch transform kernel.

The two major opportunities are:

1. **SCU DMA** — done for uploads (`saturn/dma.h`); the per-frame VDP1 command list is an opt-in switch (`sat_dma_set_command_list`), because the measurements disagree between emulators.
2. **SCU DSP** — exposed through `saturn/scu_dsp.h` and `tools/scu_dsp.py`; more kernels are the open part.

The repository already contains Sega SCU documentation for DMA, interrupts and DSP; the APIs and a first kernel exist, so the open piece is more kernels and broader DSP use, not reference material.

### VDP1

Current status: **SUBSTANTIAL**.

This is one of LibSaturn's strongest hardware areas. The public API already covers the primitives needed for 2D and a meaningful VDP1-based 3D renderer:

- normal sprites;
- scaled sprites;
- distorted sprites;
- polygons;
- polylines;
- lines;
- indexed textures and palettes;
- Gouraud shading;
- screen/native coordinate helpers;
- color-calculation integration;
- model/mesh/render abstractions built above the raw VDP1 layer.

The current 3D asset pipeline also handles one of the Saturn's important limitations: source UV-mapped faces are baked offline into VDP1-compatible rectangular face textures instead of pretending VDP1 supports arbitrary per-vertex UV coordinates.

### VDP2

Current status: **SUBSTANTIAL for the normal-layer and raster/composition
family; PARTIAL overall** — the rotation and special-effect territory is what
remains open.

The strongest covered areas are the NBG0 to NBG3 layer manager with its VRAM
cycle planner, the per-line raster effects, window/mosaic composition, and
sprite/VDP2 colour-calculation coordination. RBG0 has a meaningful high-level
path for infinite/Mode-7-style ground rendering (rotation parameter A) and a
live A/B parameter-set and coefficient-mode workflow
(`vdp2_rotation_coefficients`), and
`vdp2_alloc.h` provides the stand-alone VRAM free-list and CRAM range
allocators.

The largest VDP2 territory still open is the console's more unusual rotation
and special-effect features:

- RBG0 rotation parameter B and parameter/coefficient-mode switching: the
  first half landed since `e212a41` — both parameter sets, all three
  switching modes the hardware defines without the rotation window, and the
  whole per-line coefficient contract (2-word and 1-word entries), proven
  live by `vdp2_rotation_coefficients`; still open are the window switching
  mode (RPMD 3), per-dot coefficients and the coefficient line-colour
  composition;
- one transactional owner for VRAM bank/cycle, CRAM and table reservations:
  the first half landed (`sat_vdp2_plan_*` in `vdp2_alloc.h` — transactional
  byte/table reservations and whole-bank claims with rollback and telemetry,
  over the free list, demonstrated by `vdp2_vram_stress`), the open half is
  bringing the CRAM range ring and the cycle planner into the same
  transaction;
- double-density interlace and the dedicated-monitor video modes (E09 covers single-density interlace and PAL 240/256);
- physical-hardware verification (the acceptance checklist stays pending).

These are especially attractive because they can create Saturn-specific
visual effects without spending VDP1 polygon budget.

### SCSP + Motorola 68000

Current status: **PARTIAL**.

LibSaturn already has a useful PCM-oriented API, direct SCSP slot support, a
disc-scheduler-backed bounded music streaming family (`sat_music_*`), two SCSP
DSP effect programs (echo and four-comb reverb through `saturn/scsp_dsp.h`,
generated and host-simulated by `tools/scsp_dsp.py`), and a resident
Motorola 68000 sound driver (`saturn/sound_driver.h`) that runs a timestamped
ring of SCSP register writes, so a sound can be keyed on a timer tick whatever
the SH-2 is doing.

However, the Saturn sound subsystem is much larger than that. The main missing
layers are:

- hardware slot envelope generators and LFO/modulation;
- synthesis-oriented APIs beyond PCM voices;
- more DSP effect programs (chorus, flanger, stereo reverb, filters) beyond
  the two fixed ones;
- a 68000 sequencer/sample mixer rather than timed register writes;
- streamed audio coordination beyond the bounded music rings;
- long-run and physical-hardware validation.

A fuller 68k-backed sound service would also let audio continue with less
Master SH-2 involvement.

### SMPC and peripherals

Current status: **SUBSTANTIAL**.

`saturn/input.h` identifies what is plugged in (`sat_input_device_info`: digital pad, 3D Control Pad, mouse, keyboard, unknown), reads a 3D pad's analog stick and triggers, a mouse's motion and a keyboard's keys, and addresses every device behind a multitap by port and tap. The SMPC parser is pure (`src/hal/smpc/peripheral_logic.hpp`) and host-tested on recorded byte streams; the HAL runs one INTBACK for both ports and continues through as many 32-byte chunks as the SMPC reports. `saturn/smpc.h` covers the real-time clock, cartridge and area code, and the four battery-backed bytes.

Still not covered: light guns (SH-2 direct mode with the VDP2 latch), racing wheel and Mission Stick beyond generic axes, the multitap individual-pad and keyboard-key paths on an emulator with scripted input (only host-tested), and the SMPC commands the manual prohibits for applications.

### CD Block

Current status: **PARTIAL**.

LibSaturn can build an ISO, but building an ISO and using the Saturn CD Block at runtime are separate capabilities.

The runtime has a hardware-specific synchronous CD Block reader with a
storage-neutral `sat_cd_device_t` adapter, plus the bounded ISO9660/CDFS parser
and VFS `read_at` callback. The reader intentionally leaves disc
authentication to the BIOS/platform startup path. The modified
Ymir BIOS harness validates the CD Block transport by reading LBA 16 and
checking the ISO9660 `CD001` signature. Logical non-resident assets also have
a fixed four-block cache and a cooperative prefetch queue serviced by
`sat_asset_prefetch_update()`; no worker thread is implied.

On top of that, the library disc scheduler (`saturn/disc_stream.h`,
`sat_music_use_disc_stream`) now shares one drive between music and data
jobs: two disc-streamed tracks can be open at once (the second waits as the
standby behind the playing track), `sat_music_prefetch` fills a paused
track's rings, and `sat_music_play` hands off between them gaplessly while
the SCSP refill is spread. CD Block timings follow
`docs/sega_saturn_hardware/hard/cdblock/`; emulator timings are not treated
as hardware facts.

A complete storage path could grow in layers:

```text
CD Block sector I/O
        |
        v
ISO9660 / CDFS
        |
        v
VFS / logical asset paths
        |
        v
cooperative read + prefetch + cache
        |
        v
texture / model / map / audio streaming
```

This would remove the assumption that important game assets need to be compiled into the executable or permanently resident in RAM.

### Backup RAM and cartridge expansion

Current status: **PARTIAL** for internal Backup RAM, the external Backup Memory cartridge and volatile RAM expansion. The cartridge is emulator-verified on Ymir and Mednafen; real hardware is pending the checklist.

The internal save path now wraps the Saturn Boot ROM BUP library rather than
reimplementing Sega's on-media allocation format. The public API can inspect
capacity, enumerate records, read/write/verify/delete them and explicitly
format the device. Initialization never formats automatically.

Two distinct areas remain separate in the architecture:

- **Backup RAM/save storage**: persistent game data, directory records, free-space handling and robust versioned saves.
- **RAM cartridge**: volatile expansion memory usable for caches/assets/game-specific data.

Backup Memory cartridge support belongs to the save subsystem and reuses the internal operations at the cartridge's BIOS selector. The volatile 1 MiB / 4 MiB RAM-expansion driver provides detection, two-bank allocations, bounded asset-cache backing and a VFS bridge, and `saturn/abus.h` is the small shared layer that identifies whatever is in the slot without coupling the two.

## Host-side tooling coverage

LibSaturn's host tooling is already an important part of its effective hardware abstraction.

Current useful areas include:

- SH-2 toolchain/bootstrap scripts;
- ELF -> BIN -> ISO build flow, IP.BIN generation and boot-image/ISO checkers
  (`tools/gen_ip_bin.py`, `tools/check_ip_bin.py`, `tools/check_iso.py`,
  `tools/memory_layout.py`);
- indexed-8 image conversion and asset baking (`tools/convert_indexed8.py`,
  `tools/bake-assets.py`, `tools/generate_asset_manifest.py`,
  `tools/generate_asset_registry.py`, `tools/satlz.py`);
- the shmup packer with its hard quality gate, SATL atlas and scrolling
  backgrounds (`tools/shmup_assets.py`, `tools/shmup_music.py`);
- the 3D model pipeline (OBJ/glTF import, Draco, LOD, skeletal baking:
  `tools/model_pipeline/`, `tools/import_model.py`, `tools/city_chunker.py`)
  and the 2D stage pipeline (`tools/stage2d/`);
- SCU DSP and SCSP DSP program tooling with host simulators
  (`tools/scu_dsp.py`, `tools/scsp_dsp.py`) and the committed 68000 sound
  driver image with its freshness test (`tools/build_sound_driver.py`);
- emulator launch/acceptance flows: the headless modified-Ymir probe
  (`harness/`), Mednafen/Kronos/YabaSanshiro launchers, capture and profiling
  helpers (`tools/capture-mednafen.ps1`, `tools/pcprof.py`,
  `tools/fb2png.py`).

The most important tooling gaps are tied to the runtime gaps above. Examples:

- offline generation of VDP2 line/rotation/coefficient tables when a scene
  needs them deterministic (today every table is built by runtime code);
- NetLink/MPEG-oriented tooling (none exists);
- scripted emulator input for the peripheral corners no emulator drives
  (multitap individual pads, keyboard keys, light guns).

## Biggest unexplored areas

From the current snapshot, the largest new capability domains are:

1. **Advanced VDP2 rotation and special-effect composition** — the
   rotation window and per-dot coefficient corners of parameter B, and
   completing the transactional resource owner (the VRAM half
   landed with `sat_vdp2_plan_*` + `vdp2_vram_stress`; parameter sets A/B
   and the coefficient contract landed with
   `vdp2_rotation_coefficients`; the RBG1 first-class layer landed with
   `vdp2_dual_rotation` (its follow-up moved the parameter region to
   B0's tail so the planner's `claim_rbg1_banks` could keep the rotation
   table in a None-role bank without exposing B0/B1 to RBG0 image data,
   and switched the B-set plane-map emission to fire for any B-set
   reader — RBG1 on, RBG0 in mode B, or coefficient switching); the
   CRAM ring and the
   cycle planner still join the transaction) — the subject of
   `docs/VDP2_ADVANCED_EXAMPLES_IMPLEMENTATION_PLAN.md`
2. **SCSP synthesis, envelopes/LFO and richer DSP effect programs**
3. **More SCU DSP kernels**
4. **Double-density interlace and dedicated-monitor video modes**
5. **Advanced SCSP + 68000 sequencer/mixing runtime**
6. **Light guns and the remaining SMPC peripheral corners**
7. **External cartridge hardware beyond detection/raw reads; NetLink/communications and optional MPEG hardware**
8. **Physical-hardware validation across every subsystem** (the acceptance
   checklist stays pending until real-console runs are recorded)

This ordering is a practical engineering sequence, not a statement that later hardware is less interesting. The first items unlock capabilities that can be reused broadly by many games and by compatibility layers.

## Suggested architectural boundaries

Future work should avoid exposing raw Saturn hardware quirks directly through high-level game APIs when a reusable boundary is possible.

A useful layering model is:

```text
Game / compatibility layer
        |
        v
High-level LibSaturn subsystem
        |
        v
Runtime service / scheduler / allocator
        |
        v
HAL
        |
        v
Saturn registers / BIOS service / bus
```

Examples:

- `sat_asset_*` should not care whether bytes came from embedded data, CD or RAM cart.
- `sat_input_*` should expose devices/axes/buttons without making game code speak raw SMPC protocol.
- Future application subsystems may build on `saturn/dual_sh2.h`; this low-level layer intentionally does not define job semantics.
- `sat_dma_*` should own SCU DMA channel state and completion rules.
- `sat_audio_*` should be able to evolve from direct PCM voices toward 68k/DSP-backed services without invalidating game code.

## Known design constraints

These are not gaps in coverage but documented limits that affect how
the rest of the library is used. They are recorded here so a reader does
not mistake the absence of a feature for an oversight.

- **`sat_video_width()` / `sat_video_height()` return 0 before `sat_init()`.**
  The runtime state is zero-initialised in `.wram_l` (see
  `src/core/runtime/state.cpp`), so a `sat_video_config_t video = { sat_video_width(), sat_video_height(), ... }`
  initialiser would always read `(0, 0, ...)`. Examples therefore keep a
  private `SCREEN_W` / `SCREEN_H` literal that matches the mode they ask
  `sat_init` for. The accessors are still useful for hi-res-aware code
  (`sat_hud_init_default`, `sat_video_is_hires`) once `sat_init` has
  resolved the timing.
- **`sat_hud_line` cannot be called before `sat_hud_init*`.** The cursor
  fields are owned by the HUD; the helper assumes the user has already
  bound a font. Examples that want a one-line boot screen therefore init
  the HUD right after `sat_init`.

## What does not count as coverage

For this document, none of the following alone marks a subsystem as implemented:

- a Sega manual copied into `docs/`;
- a register definition that is never part of a supported flow;
- a one-off register write during initialization;
- host tooling with no corresponding Saturn runtime path;
- an example-specific hack that has not become a reusable subsystem.

This distinction is important for SCSP synthesis, the remaining VDP2 rotation corners and several optional Saturn devices: the repository already contains the documentation needed to implement them, but that is different from LibSaturn supporting them.

## Updating this document

When a subsystem changes meaningfully:

1. update its status in the coverage matrix;
2. add the relevant public header/HAL/example as evidence;
3. update the snapshot baseline SHA;
4. move the subsystem out of the unexplored list when it becomes a supported reusable path;
5. prefer describing the actual usable abstraction rather than individual register-level features;
6. for VDP2, keep `docs/VDP2_SUPPORT_MATRIX.json` in step with the code:
   `tests/test_vdp2_support_matrix.py` (run by `make test`) validates its
   statuses, cited chapters, evidence and recorded constants against the
   headers, so the matrix and the test change in the same commit.

The intent is for this file to remain a lightweight hardware/runtime coverage audit as LibSaturn grows from a bare-metal rendering/game library into a broader Sega Saturn development platform.
