# World Visibility & LOD Implementation Results

This ledger records executed work for `WORLD_VISIBILITY_LOD_IMPLEMENTATION_PLAN.md`.
Measurements are labeled as measured, source-configured, or unavailable. Emulator
results are not inferred from host tests or static source inspection.

## M0 — Baseline and reproducibility

**Date:** 2026-11-10  
**Starting `main` SHA:** `c9374c5b65b2dc75d73c722a5db568f58bcc6fd5`  
**Status:** complete; host gates and SH-2 builds passed; Ymir captures recorded. A
pre-existing `city_walk` startup capacity failure prevents a world-scene baseline.

### Environment

- macOS; CMake 4.4.4, Ninja 1.13.2, GNU Make 4.4.1, Python 3.15.0.
- SH-2 compiler: `sh2eb-elf-gcc` GCC 13.2.0 at `/Users/celso/dev/saturn-tools/bin`.
- Mednafen 1.32.1 is installed. PowerShell Core is not installed, so the `.ps1`
  wrappers could not run. A user-provided US/EU v1.01a BIOS is in ignored
  `bios/firmware/mpr-17933.bin`; neither the BIOS nor its archive is tracked.
- The Ymir harness fetched its pinned source and built in
  `harness/build/ymir-baseline`. The 4 MiB `city_walk` run used a blank emulated
  RAM cartridge and the direct-injection harness.

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

### Build artifact hashes

These are hashes of the rebuilt guest app binaries, not screenshots:

- `build/city_walk.app.bin`: `0e3bc3fd6bf4affc364c7a487cc90ae4d6d300bdd254310e2568fa24f8ad804a`
- `build/skybridge_3d.app.bin`: `9104c7b5e8177d54eccd0ebe420d5804939202f535fe02322798e72323eefa24`

