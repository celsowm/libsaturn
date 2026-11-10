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

## M3 — Budget governor and scene instrumentation

**Date:** 2026-11-10
**Status:** governor API, host/target validation and the `city_walk` consumer
HUD wiring are complete. The city consumer migration and integrated run are
recorded under M4 below.

### Implementation

- Added the opt-in `sat_world_lod3d_governor_step()` API. It consumes completed
  scene statistics and caller-measured field/CPU intervals, applies persistent
  overload confirmation and cooldown, degrades in bounded steps, then recovers
  more slowly after sustained headroom. Defaults are disabled; it never changes
  collision, HUD reservation, or pinned-geometry policy.
- Independently bounds face, command, and texture budgets. VDP1 command capacity
  deducts commands that were already used before the scene flush as well as the
  explicit overlay reservation. It validates the scene snapshot before any
  output or caller state changes.
- Reports field/CPU EWMA, transitions, overload reasons, actual scene result,
  used/usable world commands, budget-blocked/clipped/fallback/rejected faces,
  and whether CPU profiling was available. Zero/unavailable profile samples do
  not count as free CPU time.
- Added the source to Make/CMake manifests and the public umbrella header.

### Executed validation

| Command | Result |
| --- | --- |
| `gmake -B -j4 build/tests/test_world_lod3d_governor && build/tests/test_world_lod3d_governor` | Passed: 9 groups, including persistent overload, cooldown, slow recovery, pause, wrapped field count, capacity reservations and malformed-stat atomicity. |
| AddressSanitizer + UndefinedBehaviorSanitizer build of the governor host test | Passed: 9 groups. |
| Public C11 and C++20 umbrella-header compile with `-Werror` | Passed. |
| `python3 tests/tools/test_library_source_manifest.py` | Passed: 174 library sources and 3 startup objects. |
| `cmake --preset host-tests && cmake --build build/host-tests --parallel 4 && ctest --preset host-tests` | Passed: 3/3. |
| Focused world planner, fade bridge, fade logic, city walk and scene API host tests | Passed: 17, 5, 34 groups/tests respectively where counted; the fade logic and scene API executables passed. Existing missing-field initializer warnings remain in `test_scene3d_faces_api.cpp`. |
| `LIBSATURN_TOOLCHAIN_ROOT=/Users/celso/dev/saturn-tools ./build-example.sh city_walk` | Passed: SH-2 ELF, boot-memory, IP, ISO and disc-image checks. |
| `LIBSATURN_TOOLCHAIN_ROOT=/Users/celso/dev/saturn-tools ./build-example.sh skybridge_3d` | Passed: SH-2 ELF, 317,952 initialized bytes, IP, ISO and disc-image checks. Existing initializer warnings remain in `audio.c` and `gems.c`. |

M3 was validated in a real consumer during M4. CPU/field telemetry is
caller-measured; no stock Saturn frame-rate claim is made.

## M1 — Pure planner and capacity-safe API

**Date:** 2026-11-10
**Status:** complete; planner is built into the library manifest and umbrella
header. The runtime is not yet wired into a game consumer; that is M4.

### Implementation

- Added `sat_world_lod3d_plan()` with fixed-width C structs and caller-owned
  object state, draw output, prefetch requests, statistics and scratch.
- Planning uses camera-relative 16.16 AABBs, conservative plane support and
  near-depth rejection, projected footprint thresholds, per-object/view
  generation hysteresis, and stable-ID tie breaks.
- Resident slot generations are checked before selection. Missing desired LODs
  are returned as explicit requests, including when a finer resident LOD must
  cover a missing target. The planner performs no IO or uploads.
- Face, VDP1 command estimate and texture-byte budgets are independent. The
  planner rechecks actual costs for non-monotonic LOD tables, degrades in
  far/low-priority order, drops only optional unpinned objects, and returns
  `SAT_ERR_CAPACITY` for an impossible protected baseline.
- Optional empty LODs require explicit object permission and a far-radius cap.
  `sat_scene_t` remains the only painter and command emitter.
- Validation and output-capacity failures leave persistent states, draw and
  request outputs, and statistics unchanged. Caller scratch is temporary and
  may be overwritten during a failed plan.
- Source audit and the SH-2 object symbol table found no allocator, VDP or IO
  dependency. The only undefined symbols in the object are the compiler's
  signed and unsigned 64-bit division helpers (`___divdi3`, `___udivdi3`);
  their cost on stock SH-2 remains a performance item to measure after consumer
  integration.

### Executed validation

| Command | Result |
| --- | --- |
| `gmake -B -j4 build/tests/test_world_lod3d build/tests/test_city_walk build/tests/test_scene3d_material_pool && build/tests/test_world_lod3d && build/tests/test_city_walk && build/tests/test_scene3d_material_pool` | Passed: 17 planner groups, 34 city planner tests, and material-pool tests. Includes the missing-target request regression. |
| ASan/UBSan build and run of `tests/host/test_world_lod3d.cpp` with `world_lod.cpp` | Passed: 17 planner groups; no sanitizer findings. |
| `cmake --preset host-tests && cmake --build build/host-tests --parallel 4 && ctest --preset host-tests` | Passed: CTest 3/3. |
| Standalone C11 compile of `saturn/world_lod3d.h` and C++20 compile of `saturn/saturn.h` with `-Wall -Wextra -Werror` | Passed. |
| `python3 tests/tools/test_library_source_manifest.py` | Passed: 172 library sources, 3 startup objects. |
| `LIBSATURN_TOOLCHAIN_ROOT=/Users/celso/dev/saturn-tools ./build-example.sh city_walk` | Passed: SH-2 ELF, boot-memory, ISO and disc-image checks. |
| `LIBSATURN_TOOLCHAIN_ROOT=/Users/celso/dev/saturn-tools ./build-example.sh skybridge_3d` | Passed: SH-2 ELF, boot-memory, ISO and disc-image checks. Existing example initializer warnings remain. |
| `sh2eb-elf-nm -u .../world_lod.o` | Only `___divdi3` and `___udivdi3`; no allocation, VDP or IO symbols. |

Ymir visual validation was not repeated for M1 because neither game submits
through this planner yet; a screenshot would not exercise the new runtime path.
The next integrated visual check belongs with fade composition and the M4
`city_walk` migration. The SH-2 build confirms target compilation, not timing.

## M2 — Fade bridge and temporal stability

**Date:** 2026-11-10
**Status:** complete; the reusable adapter is exercised by host tests and
`skybridge_3d` now routes platform fade selection through it.

### Implementation

- Added `sat_world_lod3d_fade_apply()`. It consumes a selected planner draw and
  an existing `sat_fade3d_slots_t`, delegates slot mapping and hysteresis to
  `sat_fade3d_slot()`, and returns a material copy for `sat_scene_t`.
- Fade slots are offered only for valid INDEXED8 solid, textured or tiled
  materials with color calculation configured and a nonzero VDP2 backdrop
  priority below the reserved fade selector. RGB polygons, LUT4 output,
  invalid priority composition and unconfigured color calculation select the
  caller's explicit opaque or authorized cull fallback.
- The adapter never writes VDP2 registers and never returns
  `SAT_FADE3D_SLOT_CULLED` as a material slot. Fade history is caller-owned;
  object/view generation changes reset it, while LOD swaps preserve it.
- `skybridge_3d` now uses the adapter for platform selection, increments object
  generation at course reset and view generation after its fall-camera
  teleport. Its existing VDP2 setup remains one startup configuration, with
  normal sprite priority 7, fade selector priority 6 and sea priority 5.
- Ten caller-owned platform histories use 120 bytes (12 bytes each), 110 bytes
  more than the previous ten-byte history array.
- Hardened the underlying slot hysteresis comparisons to use 64-bit bounds,
  fixing signed overflow when valid 16.16 endpoints are near `INT32_MIN/MAX`.

### Executed validation

| Command | Result |
| --- | --- |
| `gmake -B -j4 build/tests/test_world_lod_fade3d build/tests/test_fade3d_logic build/tests/test_world_lod3d build/tests/test_city_walk build/tests/test_scene3d_faces_api` and all five binaries | Passed: 5 fade-bridge groups, fade logic including extreme-value overflow regression, 17 planner groups, 34 city tests, and scene face API tests. Existing aggregate-initializer warnings remain in `test_scene3d_faces_api.cpp`. |
| ASan/UBSan builds and runs for fade bridge and fade logic | Passed with no sanitizer findings. |
| `cmake --preset host-tests && cmake --build build/host-tests --parallel 4 && ctest --preset host-tests` | Passed: CTest 3/3. |
| `python3 tests/tools/test_library_source_manifest.py` | Passed: 173 library sources, 3 startup objects. |
| Standalone C11 bridge-header and C++20 umbrella-header compiles with `-Wall -Wextra -Werror` | Passed. |
| `LIBSATURN_TOOLCHAIN_ROOT=/Users/celso/dev/saturn-tools ./build-example.sh skybridge_3d` | Passed: SH-2 ELF, no-static-constructors, boot-memory, ISO and disc-image checks. The app binary is 317,952 bytes, 1,152 bytes above the M0 binary. Existing missing-field initializer warnings remain in `audio.c` and `gems.c`. |
| `LIBSATURN_TOOLCHAIN_ROOT=/Users/celso/dev/saturn-tools ./build-example.sh city_walk` | Passed: SH-2 ELF, boot-memory, ISO and disc-image checks; city behavior is not yet migrated to the planner. |
| `./harness/run-harness.sh skybridge_3d --bios bios/firmware/mpr-17933.bin --frames 120 --out harness/build/world-lod-m2/skybridge-final.json -- --screenshot 30:harness/build/world-lod-m2/skybridge_final_f30.png --screenshot 119:harness/build/world-lod-m2/skybridge_final_f119.png --vdp2-trace harness/build/world-lod-m2/vdp2_final.csv` | Passed: 120 fields, 0 misaligned SH-2 accesses. Final VDP2 state was CCCTL `0x0040`, SPCTL `0x1620`, PRISA `0x0607`. The trace wrote the eight ratios as `0x0400`, `0x0C08`, `0x1410`, `0x1F18` (slots 0..7: 0,4,8,12,16,20,24,31). |

The rendered field-119 screenshot SHA-256 is
`b267bd6edfd91e4c98cb8e02d73dae43f9e9414b0c01e1a5b95dc938d2dcb812`.
This matches the recorded M0 field-119 capture exactly. The field-30 capture
also matches its M0 hash. This is a stationary composition regression check;
the emulator run does not establish stock-hardware cadence or fill-rate.

## M4 — `city_walk` migration and outdoor acceptance scene

**Date:** 2026-11-10
**Status:** complete. The city consumer and the original procedural outdoor
free-camera consumer both exercise the generic planner and shared painter.

### Implementation

- Replaced `city_plan_frame`'s LOD and budget selection in `city_grid.h` with
  `city_world_plan_frame()` in `examples/city_walk/world_lod.c`, an
  allocation-free adapter from the city's fixed residency rings to
  `sat_world_lod3d_plan()`. Grid bounds, authored 3x3/5x5/7x7 LOD targets,
  residency, collision and chunk IO remain city-owned.
- Preserved the legacy draw sequence with adaptive governance disabled over 96
  pinned position/visibility/face-budget traces. The draw-item hash remains
  `0x628A3DB3`.
- Each level charges its faces separately from estimated commands. The city
  estimate uses `faces + ceil(faces / 16)` and a 128-command clip allowance;
  `sat_scene_stats()` remains the source of true emitted commands. Texture
  bytes are zero per object because residency pre-reserves fixed VDP1 spans.
- Connected the optional governor to completed scene stats, field cadence and
  caller-measured CPU FRT ticks. It defaults off; its minimum preserves a
  176-face near LOD0 chunk. Y toggles it, A toggles fade and resets fade
  hysteresis, and X teleports and resets both planner and fade state.
- Added planner/governor diagnostics to the city HUD and telemetry, including
  wanted/selected distributions, callback and generic frustum rejects,
  residency misses, drops/degrades, configured budgets and actual scene
  commands. Increased the explicit HUD reserve to 256 commands.

### Executed validation

| Command | Result |
| --- | --- |
| `gmake -B -j4 build/tests/test_city_walk && build/tests/test_city_walk` | Passed: 36 test groups, including command-estimate independence and the pinned 96-trace hash. |
| `gmake test` | Passed: all supported host tests and tool suites. One parallel ABI test reported its documented macOS address-mapping skip; existing scene API missing-field initializer warnings remain. |
| `LIBSATURN_TOOLCHAIN_ROOT=/Users/celso/dev/saturn-tools ./build-example.sh city_walk` | Passed: SH-2 ELF, no-static-constructors, 156,620 initialized bytes, IP, ISO and disc-image checks. |
| Ymir `city_walk`, 4 MiB cart, 1,400 fields, scripted Y/A/X, WRAM telemetry dump | Passed: RUNNING state, status 0, 372 game frames / 365 measured, 0 misaligned SH-2 accesses, 0 decode failures, 0 stale-generation submissions, HUD/scene errors 0. The script produced 3 observed held reports; final telemetry had governor enabled, fade disabled, 350 governor samples, and no overload transitions. |
| Ymir `city_walk`, no cart, 250 fields | Passed refusal path: state `NO_CART`, status -9, zero game frames, zero decode/HUD errors. |
| Ymir `city_walk`, 4 MiB cart, stationary fade-on, 1,400 fields, screenshots 1,000 and 1,350 | Both captures are byte-identical at SHA-256 `1eab1a6a68f9daf950f790cd66974a5c725b286e7c00a42ea357fdf5e0767975`; 0 misaligned SH-2 accesses. |

### Measured emulator observations

- The controlled run observed a maximum of 182 submitted world faces and 178
  actual VDP1 world commands. Its final scene snapshot was 178 world commands,
  180 total commands used, 2,048 command capacity, and a 256-command overlay
  reservation. The plan budgets were 500 faces and 628 estimated commands.
- The measured field histogram contained 364 two-field frames and one three-
  field frame: median and p95 were both two fields. Mean loop time was 7,025
  FRT ticks and mean planner/adaptation time was 707 FRT ticks over 365 measured
  frames. These are Ymir observations, not stock hardware timing.
- The final plan snapshot considered 49 authored cells, callback-frustum
  rejected 19, had no missing resident LODs, and selected 2/3/3 cells at LOD0/1/2.
- The VDP2 snapshot recorded `BGON=273`, `PRIR=5`, `PRISA=1543`, `PRINA=2`,
  `PRINB=3`, and `CCCTL=64`. The screenshots show VDP1 buildings composed over
  the VDP2 street/sky layers; this does not certify hardware fill rate.

### Outdoor free-camera acceptance scene

- Added `examples/world_lod3d_demo`, an original procedural tower, tree,
  stationary avatar, VDP2 sky/ground, and independently moving free camera.
  It owns fixed storage for 25 objects, 73 LOD records, meshes, planner scratch,
  per-object LOD/fade history, and scene submission buffers; no frame allocates.
- Every selected mesh goes through `sat_scene_submit_instance()` and the
  shared `sat_scene_t` painter. The planner uses camera-relative bounds and
  six normalized conservative frustum planes. The avatar is pinned and the
  camera focus clamps to +/-384 world units to keep fixed-point bounds safe.
- Reused `sat_world_lod3d_fade_apply()` / `sat_fade3d_slot()` for supported
  indexed8 materials. Fade-off selects the same mesh plan with opaque slots.
  VDP2 composition is sky priority 2, ground priority 5, faded sprite selector
  6, normal sprite priority 7; CRAM banks 0/1/3/4/5/6 are kept distinct for
  ground, sky, HUD and the three world material palettes.
- The 400-field controlled route moves/pitches/yaws the camera, checks a rapid
  yaw reversal, resets to a fixed pose, compares fade disabled/enabled at that
  same pose, and enables the governor. `harness/run-world-lod3d-demo.sh` is a
  POSIX runner; it builds with `build-example.sh`, runs Ymir with the local
  ignored BIOS, captures screenshots/VDP2 trace/WRAM, and runs the telemetry
  and image assertions in `harness/tests/test_world_lod3d_demo.py`.

### Outdoor executed validation

| Command | Result |
| --- | --- |
| `LIBSATURN_TOOLCHAIN_ROOT=/Users/celso/dev/saturn-tools ./harness/run-world-lod3d-demo.sh` | Passed. Rebuilt the SH-2 disc; 120,768 initialized bytes matched the ELF; no static constructors; IP/ISO/disc checks passed. Ymir ran 400 fields with the scripted camera route, captured seven 320x224 screenshots and VDP2 trace, and dumped High WRAM. Both master/slave misaligned-access counts were zero. |
| `harness.tests.test_world_lod3d_demo` (invoked by the runner) | Passed: 4 checks for bounded multi-LOD planning, protected camera reset/fade/governor/profile state, camera movement screenshot, and a stationary-pose fade comparison. |

### Outdoor measured Ymir observations

- Final telemetry: 192 render-loop frames, all 192 spanning two emulated
  fields; mean CPU loop time 4,794 FRT ticks (920,430 total / 192 frames).
  These are Ymir observations, not a stock Saturn timing claim.
- Final plan: 25 considered, 3 frustum rejected, 0 missing resident LODs,
  0 optional drops; wanted LOD counts 18/4/0 and selected counts 2/16/4.
  It submitted 252 faces against a 256-face ceiling and 138 world commands
  against a 384-command estimate budget. Governor enabled with 38 samples and
  no overload transitions; 19 objects selected a fade slot.
- Final VDP2 state: `BGON=0x0111`, `PRIR=5`, `PRISA=0x0607`, `PRINA=2`,
  `PRINB=3`, `CCCTL=0x0040`, and `KTAOF=0`. This records the Ymir snapshot,
  not physical hardware validation.
- Same-pose fade-off/fade-on captures (fields 220 and 240) have SHA-256
  `2292d40e69717b0387beacb99643fc9137c5fe70e54429f41c9eddb49b007209` and
  `751958d4d0dda1390dbb3b8777f9a5fd2549da68ddde7c60b160b55f5cd82e8d`.
  The automated comparison confirms the supported world sprites change while
  the camera is stationary; it does not imply VDP1-on-VDP1 alpha blending.

