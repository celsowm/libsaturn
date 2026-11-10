# World Visibility & LOD Implementation Results

This ledger records executed work for `WORLD_VISIBILITY_LOD_IMPLEMENTATION_PLAN.md`.
Measurements are labeled as measured, source-configured, or unavailable. Emulator
results are not inferred from host tests or static source inspection.

## M0 — Baseline and reproducibility

**Date:** 2026-11-10  
**Starting `main` SHA:** `c9374c5b65b2dc75d73c722a5db568f58bcc6fd5`  
**Status:** complete; host gates and SH-2 builds passed; Ymir captures recorded.
The baseline `city_walk` startup capacity failure was fixed after M0; see the
follow-up below.

### Environment

- macOS; CMake 4.4.4, Ninja 1.13.2, GNU Make 4.4.1, Python 3.15.0.
- SH-2 compiler: `sh2eb-elf-gcc` GCC 13.2.0 at `/Users/celso/dev/saturn-tools/bin`.
- Mednafen 1.32.1 is installed. PowerShell Core is not installed, so the `.ps1`
  wrappers could not run. A user-provided US/EU v1.01a BIOS is in ignored
  `bios/firmware/mpr-17933.bin`; neither the BIOS nor its archive is tracked.
- The Ymir harness fetched its pinned source and built in
  `harness/build/ymir-baseline`. The 4 MiB `city_walk` run used a blank emulated
  RAM cartridge and the direct-injection harness.
- POSIX shell entry points were added for example builds, Mednafen, and Ymir so
  subsequent validation does not depend on PowerShell.

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
| `cmake -S harness -B harness/build/ymir-baseline -G Ninja` | Initially blocked by sandbox DNS; succeeded after network approval and fetched pinned Ymir `5c571f680e3606de08813af4de43dc00e90cc6e3`. |
| `cmake --build harness/build/ymir-baseline --parallel 4` | Passed; built Ymir core, `probe`, and harness tests (105 build steps). Two warnings came from vendored stb sources. |
| `ctest --test-dir harness/build/ymir-baseline --output-on-failure` | Passed: 3/3 (`composited_video_observation`, `cpu_profile_accounting`, `cpu_profile_emulator`). |
| Ymir `probe` on `skybridge_3d`, 120 fields, screenshot fields 30 and 119 | Passed; guest ran 120 fields, 0 misaligned SH-2 data accesses, screenshot at field 119 shows the rendered scene. |
| Ymir `probe` on `city_walk`, 4 MiB cart, 1300 fields, screenshot fields 820 and 1250 | Probe completed, 0 misaligned SH-2 data accesses, but the game reached its fatal screen with `SAT_ERR_CAPACITY` from `sat_vdp1_vram_reserve()` in `examples/city_walk/residency.c:105` before rendering the city. This is a baseline defect to resolve during consumer migration. |
| `MEDNAFEN_HOME=/Users/celso/dev/lib-saturn/bios SDL_VIDEODRIVER=dummy mednafen build/city_walk.cue` | Loaded the BIOS and Saturn module, initialized video at nominal 60 Hz, then was interrupted before a route/capture; no default audio device was available. Ymir provided the reproducible captures instead. |
| `sh -n build-example.sh run-example.sh emulators/mednafen/run-mednafen.sh harness/run-harness.sh` | Passed. All four help paths returned usage. |
| `LIBSATURN_TOOLCHAIN_ROOT=/Users/celso/dev/saturn-tools ./build-example.sh city_walk` | Passed; invoked GNU Make and staged the up-to-date SH-2 artifacts under `build/examples/`. The actual clean build is recorded above. |
| `./harness/run-harness.sh skybridge_3d --bios bios/firmware/mpr-17933.bin --frames 120 --out harness/build/world-lod-baseline/skybridge_shell.json -- --screenshot 119:harness/build/world-lod-baseline/skybridge_shell_f119.png` | Passed; 120 Ymir fields, 0 misaligned SH-2 data accesses. Screenshot hash matched the direct probe baseline. |
| `SDL_VIDEODRIVER=dummy MEDNAFEN_HOME=/Users/celso/dev/lib-saturn/bios ./run-example.sh skybridge_3d --bios bios/firmware/mpr-17933.bin` | Passed launcher checks; Mednafen loaded the BIOS, opened the cue, and initialized video. The interactive run was stopped before a guest screenshot was recorded. |

### Baseline values and limits

- `city_walk` documents LOD face targets of 176 / 64 / 24 per chunk and a 500-face
  frame planner ceiling. The runtime's face storage caps are 256 / 64 / 24; 176 is
  the documented LOD0 quality target, not the storage capacity.
- `city_walk` reserves 711 VDP1 commands from its static formula:
  500 world-face budget + 160 HUD commands + 48 clipping slack + 3 setup commands.
  This is an admission reservation, not a measured `world_commands` count.
- Ymir reports a nominal 60 Hz mode and ran the requested 120/1300 simulation
  fields. It does not report elapsed wall time for these synchronous captures,
  so this is not an FPS or hardware cadence measurement.
- Actual world-command totals and scene statistics were not exported by this
  baseline probe. `city_walk` produced no world scene because of the capacity
  failure. The skybridge screenshot confirms visible VDP1 geometry over the VDP2
  background, but does not by itself certify fade or wall-occlusion correctness.
- Ymir uses a software VDP renderer; its frame timing cannot certify stock Saturn
  pixel fill-rate. Mednafen's state directory was redirected into ignored `bios/`
  to avoid the sandbox home-directory write restriction.

### Screenshot hashes

Screenshots are retained under the ignored `harness/build/world-lod-baseline/`.

| Capture | SHA-256 | Observation |
| --- | --- | --- |
| `city_walk_f30.png` | `7ce3ea96c86ce5cab9112ffa03e31e9a88416218c0ddb4ce65818119ef967ca2` | `LOADING 2`. |
| `city_walk_f820.png` | `4126edc7c6872dcb7f9cd5cac93d44823820ba3bc11f7e402c072e015f8b53e6` | Disc loading progress at 91%. |
| `city_walk_f1250.png` | `c77b71f9d09799cf0617fa3ed56ccdd9fd1c8f91da86effeb1a686488ef7cfaa` | Fatal capacity screen during texture VRAM reservation. |
| `skybridge_3d_f30.png` | `fad174f3e723b8803babbffe4099bd4fe7b013811e1481317221cc43cf0aed74` | Ocean generation/loading. |
| `skybridge_3d_f119.png` | `b267bd6edfd91e4c98cb8e02d73dae43f9e9414b0c01e1a5b95dc938d2dcb812` | Rendered scene with VDP1 bridge/avatar and VDP2 ocean/sky. |
| `skybridge_shell_f119.png` | `b267bd6edfd91e4c98cb8e02d73dae43f9e9414b0c01e1a5b95dc938d2dcb812` | Shell wrapper reproduction; byte-identical to the direct probe capture. |

### Build artifact hashes

These are hashes of the rebuilt guest app binaries, not screenshots:

- `build/city_walk.app.bin`: `0e3bc3fd6bf4affc364c7a487cc90ae4d6d300bdd254310e2568fa24f8ad804a`
- `build/skybridge_3d.app.bin`: `9104c7b5e8177d54eccd0ebe420d5804939202f535fe02322798e72323eefa24`

## Post-M0 bug fix — city VDP1 texture capacity

The first post-baseline Ymir retry still failed at `residency.c:105`. Reducing
the 83 city slot caps to the values already described by the example (16 / 5 /
1.5 KiB) brought their fixed reservation to 350,720 bytes, but the city also
kept a separate 8x8 texture for each of 220 solid palette materials, in
addition to the 96-glyph 8x8 font. Those earlier allocations left too little
space for the city pool.

The solid-material pool now stores each uniform fill as an 8x1 INDEX8 texture,
which is sufficient for a distorted solid sprite and reduces each material
from 64 to 8 VRAM bytes. Runtime caps and the offline archive generator now
agree at 16,384 / 5,120 / 1,536 bytes per LOD. For this archive:

| VDP1 texture allocation | Bytes |
| --- | ---: |
| City fixed residency spans | 350,720 |
| 96 ASCII glyphs, 8x8 INDEX8 | 6,144 |
| 220 solid colors, 8x1 INDEX8 | 1,760 |
| Total | 358,624 |
| Available after command/Gouraud banks | 360,416 |
| Remaining | 1,792 |

### Executed validation

| Command | Result |
| --- | --- |
| `gmake -B -j4 build/tests/test_city_walk && build/tests/test_city_walk` | Passed: 34 tests. |
| `build/tests/test_city_walk build/generated/city_walk/iso/CITY.BIN` | Passed: 301 blobs (301 textured, 15,534 textures), 467 empty entries, worst coordinate magnitude 99.9 units. |
| `gmake -B -j4 build/tests/test_scene3d_material_pool && build/tests/test_scene3d_material_pool` | Passed; checks one-row solid texture uploads and palette behavior. |
| `LIBSATURN_TOOLCHAIN_ROOT=/Users/celso/dev/saturn-tools ./build-example.sh city_walk` | Passed; regenerated the archive and completed the SH-2 ELF, boot-memory, ISO, and disc-image checks. |
| `./harness/run-harness.sh city_walk --bios bios/firmware/mpr-17933.bin --ram-cart 4m --frames 1400 --out harness/build/world-lod-baseline/city_walk_vram_fix2.json -- --screenshot 1000:harness/build/world-lod-baseline/city_walk_vram_fix2_f1000.png --screenshot 1350:harness/build/world-lod-baseline/city_walk_vram_fix2_f1350.png` | Passed: 1,400 fields, 0 misaligned SH-2 data accesses. The fatal capacity screen is gone; the scene and HUD are visible. The HUD sample shows 160 faces and 111 commands. |

The generated archive reports maximum texture blocks of exactly 16,384 / 5,120
/ 1,536 bytes. Its SHA-256 is
`4c1b853a689fa50c8efa390d0174a77e1eaab60e46281ad3fd53b5b73b59484b`; the
rebuilt application SHA-256 is
`45ca7c3b85516bea93f9942bc1eec6c2c3237cdc76867966e793f46f172bc639`.
Screenshots at fields 1,000 and 1,350 are byte-identical at SHA-256
`dd11401b9e1ab3a8bf991a0d50116e0d70c9da2c846ba915b1e43d06fbde980a`, showing
the stationary initial viewpoint without temporal flicker. This is an emulator
render check, not a stock-hardware fill-rate or frame-cadence measurement.

