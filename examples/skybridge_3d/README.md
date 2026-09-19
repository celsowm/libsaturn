# Skybridge 3D

A **playable, stock Sega Saturn 3D platformer example**. The cube is the avatar; the sea, sky and platforms demonstrate using the machine's distinct video processors together instead of trying to rasterize the whole world on the CPU.

## Play

Move with the digital D-pad (camera-relative), press **A** to jump (release early for a shorter jump), press **B / C** to rotate the follow camera by 15 degrees, and press **START** to pause/resume. Reach the gold arch on the tenth platform after collecting the eight golden pickups. The fourth and seventh platforms are checkpoints. Falling into the sea respawns you at the last checkpoint, retaining collectibles. The fifth platform moves, and the eighth bridge flashes before temporarily collapsing. After finishing, START restarts the course.

This demo has no mandatory RAM cartridge, external art downloads, controller extension, CD streaming, game save, or third-party model assets. The generated palette, ocean, sky and PCM all come from C source. A startup loading panel reports completed initialization work and tracks real ocean rows generated, not a fixed timer.

## Build

From the repository's MSYS2 environment with the SH-2 cross toolchain installed:

```sh
make EXAMPLE=skybridge_3d IP_TEMPLATE_KIND=sbl all
make test
```

From PowerShell, use the project wrapper:

```powershell
.\build-example.ps1 skybridge_3d
.\harness\run-harness.ps1 skybridge_3d -Bios .\bios\saturn_bios_us.bin -Frames 240 -BootFrames 90 -PadScript .\harness\scripts\skybridge_smoke.pad -Screenshot '200:skybridge.png'
```

The BIOS is your own dump and is not included in this repository. Consult `harness/README.md` for the direct-injection harness limitations. An emulator screenshot is a **visual diagnostic**, not proof of correctness on physical hardware.

## How it works

- `game.h` has a pure, deterministic 60 Hz 16.16 fixed-point gameplay loop and a compact ten-platform stage. It is tested directly by `tests/host/test_skybridge_game.cpp`. Collision checks use a bounded array of ten world AABBs and per-axis movement; no hidden heap or physics engine.
- `main.c` creates a VDP2 NBG0 512×128 indexed sky (palette bank 1) and RBG0 512×256 indexed sea (palette bank 0). RBG0 coefficients make sea rows above the 96px horizon transparent. The 48-word rotation table is updated for the player's camera and animated water sampling, without rebuilding or uploading the 128 KiB bitmap every frame.
- VDP1 draws the solid platforms, the player cube, pickups, checkpoints and finish arch as RGB polygons with Gouraud-shaded tops and a shared INDEX8 16×16 platform-inset texture (palette bank 3). Basic per-object depth sorting is used because VDP1 has **no depth buffer**. The contact shadow is a checkerboard-mesh polygon, not true alpha. Font glyphs use palette bank 2 and are drawn after the world.
- The frame uses a transparent VDP1 erase and calls `sat_vdp2_layers_commit` before `sat_begin_frame`. Do not replace that with `sat_app_frame_begin`, which clears VDP1 to opaque and can hide the VDP2 scenery.
- Startup first creates the font, then shows progress while preparing the indexed platform texture, sky pixels, ocean rows, VDP2 composition and audio. The scene only starts after these tasks succeed. Five generated PCM S8 sound effects are registered as logical assets through `sat_asset_register` and loaded via `sat_sound_load`; the looping music remains a direct `sat_sound_create` because the current typed sound loader does not preserve the source's loop setting. No asset prefetch is triggered for embedded arrays: `sat_asset_prefetch_submit` is specifically for nonresident, filesystem-backed data, and its cooperative update must be serviced explicitly by a future CD-backed game.
- The loading screen is drawn with VDP1 before VDP2 is configured and covers the screen with an opaque rectangle while keeping the *erase* transparent, avoiding the usual `sat_app_frame_begin` layering trap.

## Current scope and trade-offs

This is the **first playable implementation**, not the full production acceptance of `docs/SKYBRIDGE_3D_PLATFORMER_EXAMPLE_PLAN.md`. It deliberately uses simple low-poly geometry, no GLB assets, no bespoke runtime alpha engine, no optional Slave SH-2, DMA, cart cache, save files, or advanced per-line water effects. The current broad-phase is a ten-AABB scan, not the large-world spatial grid. The follow camera is fixed-distance and can intersect arbitrary additional geometry if you modify the level; the world is laid out with generous sight lines. The simple center-based painter ordering may show incorrect overlap where meshes intersect. Fixed 60 Hz game ticks are capped at three catch-up ticks per displayed iteration.

Performance, collision traversability and visual/audio behavior **must be validated in a Saturn emulator and ideally on original hardware** before making frame-rate or visual-correctness claims. Tests cover discrete gameplay invariants; they do not substitute for playing the entire course. See `AGENTS.md` and the published plan for the next acceptance gates.
