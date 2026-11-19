# C3 V3 — real workload qualification

C3 V2 is a candidate, not production. Compare mode 1 against mode 3 without
changing the production flag. Preserve real example rendering, simulation,
parallelism and framebuffer modes. No new target instrumentation.

## Matrix

- dino_demo: textured geometry, animation and worker.
- skeletal_animation_3d: opt into Robot Police real skeletal viewer (not the benchmark).
- skinning_stress_3d: multi-actor skinning.
- fighter_arena_3d: fighting scene.
- world_lod3d_demo: LOD and adaptive budgets.

Run neutral input, identical assets, BIOS and cache configuration. Build both
modes, retain flags, binary hashes, disassembly and logs. Ymir injection boot
with explicit slave handoff: independent deterministic 600/1200-field checkpoints;
calculate list submissions per nominal second and deltas of draw waits, timeouts,
late swaps. Profile the same 12-field window near the second checkpoint and
retain PC attribution, observed call edges, actual CCR, sampled VDP1 draw spans,
visible completed draw counters. Exclusive interpreter cycles are not inclusive
emitter time. Counts from different-length windows must not be compared directly.
Zero target emitter hits may mean batches/inlining or that the path is unused;
do not interpret this as a speedup.

Default also runs Mednafen real-CD visual smoke captures at recorded indices
900 and 1200, with isolated homes and panic validation. This is not Mednafen
FPS profiling: the wall-clock capture stop and boot path are not synchronized to
Ymir. Use --ymir-only for the initial performance matrix.

Dynamic scenes may differ at equal fields due to pacing, animation, HUD and
adaptive LOD. A pixel match is evidence only at that capture; a mismatch
requires matched simulation and draw-list state before treating it as a
regression. Preserve full-screen images and lossless diff masks. No crop or
resampling; no promotion from this run alone. No claim of physical Saturn FPS.
A later matched-state capture and hardware validation remain mandatory.

## Run

    python3 harness/scripts/run_vdp1_emit_real_workloads.py

Optional first-pass subset:

    python3 harness/scripts/run_vdp1_emit_real_workloads.py --ymir-only --examples dino_demo skeletal_animation_3d

Return harness/build/vdp1_emit_real_workloads-return.zip, including partial
results on failures. The runner attempts remaining examples after each failure.

## V3.1 — recovery

The first V3 archive showed a Dino link failure (Master stack reserve) and
SAT_ERR_UNSUPPORTED at demo.c:220 in both original-character controls.
The renderer itself is not implicated by those failures. The file/asset API
explicitly supports direct uncached reads: only submit prefetch when cache
stats report nonzero capacity. Keep full decode/load error checking; do not
reserve 64 KiB solely for a sequential startup copy. This repairs the shared
character example, not the library API.

Dino A/B moves only g_model_textures to WRAM-L with the existing build flag;
g_material_program_storage remains in WRAM-H. Same placement in both modes;
linker stack guard remains intact. Link fit and target execution still need
confirmation. Stress starts at four requested actors using an opt-in preset,
with checkpoint count verification. Default interactive preset remains one.
World LOD Ymir uses the existing route, followed by the warmed endpoint;
Mednafen is still neutral input. Not a cross-emulator synchronized route.
Report exclusive cycles per observed call edge where available, null otherwise;
this proxy is not inclusive call latency, and boundary effects may matter.
Robot Police Mednafen matched-pose qualification remains outstanding; dynamic
pixel differences are review evidence, not a regression or a pass.

Run the same entry point and return
harness/build/vdp1_emit_real_workloads_v3_1-return.zip.

## V3.2 — mixed-mode boot and fixed-pose qualification

Dino V3.1 links but panics at init_music. Retain CDDA: Ymir LoadDisc accepts
CUE so open the retained mixed-mode CUE/BIN, not the data-only ISO. This
addresses a probable missing audio track, not a proven cause until execution.
No music bypass or production changes. Preserve full Ymir stress diff and
report a second diff excluding only the EMIT US number at x248..279,
y136..143 on the verified 320x224 raster. No Mednafen mask.

Separate opt-in Robot Police fixed-pose build uses existing 48-pose sequence
and hierarchy control for both VDP1 modes. All 48 post-submit lists (including
END and Gouraud words) are hashed; final measured list is also copied literally
into WRAM-L (80 KiB + header). Compare same-emulator A/B pose hashes, matrices,
vertices, literal list and Gouraud SHA256, and stable frozen screen pixels.
No cross-emulator raw-bank-address equivalence is assumed. Low RAM savestate
extraction validates a byte-order sentinel. Instrumented pose builds are not
used for performance conclusions; production and dynamic performance builds
leave the capture flag off. Existing V2 remains the isolated literal-list gate.

Run the same entry point; it includes the fixed-pose qualification by default
when Robot Police is selected. --pose-only runs just the separate pose test;
--skip-pose runs only dynamic workloads. --ymir-only still supported.
Return harness/build/vdp1_emit_real_workloads_v3_2-return.zip. Link fit and
Mednafen low-RAM layout need target verification. Hardware still outstanding.

## C3 V4 — SDK candidate, not default promotion

V3.2 clean checkout 9511145: 5/5 dynamic applications; unchanged nominal
submission cadence (15, 12, 15, 29.9, 30/s); exclusive selected-emitter
cycles reduced 3.43%, 3.53%, 3.45%, 2.85% in Robot, Stress, Fighter and
World respectively. Dino had no attributed target cycles and 150 waits
per 150 submissions in both modes. Robot fixed-pose qualification passed
in Ymir and Mednafen, 48 list hashes and final literal 858 commands,
27,456 bytes; no Gouraud tables in that scene. V2 qualified Gouraud.
No physical hardware result; dynamic Mednafen poses not universally matched.

Remove mode 2 from active HAL and runner. Only the three selected emitters
retain packed paths; mode 0 shipping scalar, 1 explicit scalar control,
3 candidate. Old V1/V2 artifacts are reproducible via historical commits.
CMake installed runtime supports LIBSATURN_VDP1_HYBRID_EMIT=ON (default OFF);
Make examples use SAT_VDP1_EMIT_EXPERIMENT=3. No public header/API change.
No runtime switch. SDK options are compile-time and require a fresh build.

Run host focused A/B: python3 harness/scripts/run_vdp1_emit_host_v4.py
(2048 literal 32-byte records and clipping/capacity/rollback/bank tests in
both modes); then gmake test for full host suite; then
python3 harness/scripts/run_vdp1_emit_renderer.py for SH-2 A/B and
python3 harness/scripts/run_vdp1_emit_real_workloads.py for dynamic/fixed pose.
Return *v4-return.zip archives. Historical zip names are not overwritten.

Tekken 3 uses installed CMake SDKs. Build separate clean scalar and hybrid
prefixes from the SAME revision, use an existing Release game cache with
original assets and identical T3 options, change only LibSaturn_DIR.
Use scripts/saturn_probe.py with each build's disc/music.cue when present,
identical input tape, 3000/8000/30000 fields. Compare actual simulation
ticks, frame serial, draw waits/timeouts, rejected faces, health/combat
counters and screenshots at matched state (not merely equal fields).
Do not use raw pointer/padding hashes as logical-state equality. A/B SDK
archives must be distinct and hashes retained. No neutral boot or frozen
diagnostic is a combat qualification. Test physical Saturn stock RAM,
both 224-line fight and 448-line menu, repeated combat/round restart,
LUT4 textures and audio; no promotion until accepted. The SDK remains OFF.

### Automated Tekken SDK A/B

Use clean committed checkouts of both projects and a previously working
optimized Tekken Release build (private assets still local). Supply a real
combat device tape accepted by Tekken's saturn_probe.py; the script does not
invent inputs or alter the game repository. It reuses the game's cache
options, installs two independent SDKs, runs the same checkpoints with each,
checks SDK and workload hashes, and returns a report with sampled state and
full-screen diff images. Results are diagnostic by default; explicitly choose
--workload paul-law-release or two-paul-release only for a matching build.

    python3 harness/scripts/run_vdp1_emit_tekken_v4.py --tekken ../tekken3-saturn \\
      --baseline /path/to/working/release-build --bios bios/firmware/mpr-17933.bin \\
      --device-script /path/to/combat.devices

Return harness/build/vdp1_emit_tekken_v4-return.zip. A completed run is not a
promotion gate: review combat activity, state and frame cadence; a mismatch
at equal fields may need matched tick captures. Physical test still required.

### Tekken result (2026-11-19): correct, no coverage, stays opt-in

Full report: tekken3-saturn `docs/C3_HYBRID_EMIT_TEKKEN_QUALIFICATION.md`.
The run used a simulation-tick input fixture, frozen-tick captures
(`--freeze-ticks`), a separate profile window (`--profile-frames`) and
`inspect_vdp1_emit_sdk.py` provenance.

- **Provenance.** The SDKs differ only in `vdp1.cpp.obj` (1 of 183 archive
  members). Each game links its own SDK. The hybrid emitters drop from 7–8
  `mov.w` stores to 1–2.
- **Correctness.** Two fighter pairs, frozen at ticks 900, 1800, 2700 and
  3600 (and 1800 for the second pair), show identical logical state,
  bank-normalised command lists, framebuffer and screenshot. Repeats are
  bit-identical. Live state is identical to 60,000 fields, with no timeouts
  or rejected faces. Memory and stack are unchanged.
- **Coverage.** Tekken's fighter, stage and Force faces use
  `sat_scene3d_program_run_sh2`. Its Force floor uses
  `sat_sprite_batch_emit_distorted`. Profile windows show 0 emitter cycles
  in fight and Force. Only hit-spark flipbooks reach `push_distorted_sprite`:
  267 sprites in 3,600 ticks, about 0.08 per image.
- **Cadence.** Image counts move by 1–2 per 4,800 fields. That matches the
  8-byte `.text` layout shift, not the emitters.

Promotion needs a consumer that issues many generic distorted or projected
sprites per frame. Tekken is not one.
