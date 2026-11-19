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
