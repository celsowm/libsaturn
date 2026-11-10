# World Visibility & LOD Implementation Results

This ledger records executed work for `WORLD_VISIBILITY_LOD_IMPLEMENTATION_PLAN.md`.
Measurements are labeled as measured, source-configured, or unavailable. Emulator
results are not inferred from host tests or static source inspection.

## M0 — Baseline and reproducibility

**Date:** 2026-11-10  
**Starting `main` SHA:** `c9374c5b65b2dc75d73c722a5db568f58bcc6fd5`  
**Status:** complete; host gates and SH-2 builds passed; visual/runtime captures blocked.

### Environment

- macOS; CMake 4.4.4, Ninja 1.13.2, GNU Make 4.4.1, Python 3.15.0.
- SH-2 compiler: `sh2eb-elf-gcc` GCC 13.2.0 at `/Users/celso/dev/saturn-tools/bin`.
- Mednafen 1.32.1 is installed. `pwsh`, a Saturn BIOS (`mpr-17933.bin`), Ymir, and
  the built Ymir harness probe are unavailable in this environment.
- The normal `run-example.ps1` and `harness/run-harness.ps1` entry points could not
  run because PowerShell Core is not installed. Direct Makefile builds and direct
  Mednafen attempts were used as alternatives.

### Commands and results

| Command | Result |
| --- | --- |
| `cmake --preset host-tests` | Passed; configured `build/host-tests`. |
| `cmake --build build/host-tests --parallel 4` | Passed; no work needed. |
| `ctest --preset host-tests` | Passed: 3/3 (`header_smoke`, `public_headers_do_not_include_src`, `library_source_manifest`). |
| `gmake -B -j4 build/tests/test_city_walk build/tests/test_skin_present build/tests/test_scene3d_faces_api build/tests/test_fade3d_logic` | Passed; rebuilt all four host binaries. Existing missing-field initializer warnings appeared in the scene API test source. |
| `build/tests/test_city_walk` | Passed: 34 tests. |
| `build/tests/test_skin_present` | Passed: 3 tests. |
| `build/tests/test_scene3d_faces_api` | Passed. |
| `build/tests/test_fade3d_logic` | Passed. |
| `PATH="/Users/celso/dev/saturn-tools/bin:$PATH" gmake EXAMPLE=city_walk all` | Passed; SH-2 ELF, app binary, and disc image generated; boot-memory and ISO checks passed. |
| `PATH="/Users/celso/dev/saturn-tools/bin:$PATH" gmake EXAMPLE=skybridge_3d all` | Passed; SH-2 ELF, app binary, and disc image generated; model quality, boot-memory, and ISO checks passed. Existing initializer warnings appeared in `audio.c` and `gems.c`. |
| `pwsh -File ./run-example.ps1 -Example city_walk` | Blocked: `pwsh` is not installed (exit 127). |
| `MEDNAFEN_HOME=/private/tmp/libsaturn-world-lod-mednafen SDL_VIDEODRIVER=dummy mednafen build/city_walk.cue` | Emulator opened the disc and selected the Saturn module, then stopped because `/private/tmp/libsaturn-world-lod-mednafen/firmware/mpr-17933.bin` is missing (exit 255). |
| `MEDNAFEN_HOME=/private/tmp/libsaturn-world-lod-mednafen SDL_VIDEODRIVER=dummy mednafen build/skybridge_3d.cue` | Same BIOS blocker (exit 255). |

### Baseline values and limits

- `city_walk` documents LOD face targets of 176 / 64 / 24 per chunk and a 500-face
  frame planner ceiling. The runtime's face storage caps are 256 / 64 / 24; 176 is
  the documented LOD0 quality target, not the storage capacity.
- `city_walk` reserves 711 VDP1 commands from its static formula:
  500 world-face budget + 160 HUD commands + 48 clipping slack + 3 setup commands.
  This is an admission reservation, not a measured `world_commands` count.
- Actual visible faces, VDP1 world commands, per-frame field cadence, fade slot
  behavior, VRAM residency, and visual occlusion were not measured: both emulator
  boots stopped before guest execution. No screenshot was produced, so screenshot
  hashes are **unavailable**.
- The direct Mednafen attempt with its default state directory first hit the
  sandbox's home-directory write restriction. Redirecting `MEDNAFEN_HOME` to
  `/private/tmp` resolved that environment issue and exposed the actual BIOS
  blocker. Mednafen/ emulator timings are not treated as hardware performance.

### Build artifact hashes

These are hashes of the rebuilt guest app binaries, not screenshots:

- `build/city_walk.app.bin`: `0e3bc3fd6bf4affc364c7a487cc90ae4d6d300bdd254310e2568fa24f8ad804a`
- `build/skybridge_3d.app.bin`: `9104c7b5e8177d54eccd0ebe420d5804939202f535fe02322798e72323eefa24`

