# SH-2 Assembly Optimization Master Plan

Status: **planned; historical kernels integrated, new work not implemented**  
Review baseline: `c5a33e2627c85fb75268ce93576838069a33886b` (2026-11-17)  
Scope: generic LibSaturn, stock Saturn; consumer games are qualification workloads, not architecture dependencies.

## 1. Objective

Turn the successful hand-scheduled SH-2 kernels into a repeatable, measurable optimization program for the entire SDK. Improve CPU throughput, worst-case frame work and memory pressure without changing public fixed-point semantics, visual fidelity, collision decisions, simulation frequency, or resource ownership. Prefer algorithm and layout improvements over instruction substitution. Keep C/C++ as the SDK architecture and use assembly for measured, bounded hot loops.

Targets are engineering gates, not promised results:
- Establish a current, matched baseline for each workload before changing code.
- Aim for at least 15% reduction in isolated hot-kernel time, or a material whole-workload win (at least 2% CPU work or 0.5 sustained images/s when applicable); smaller wins need an explicit justification such as removal of a field-boundary stall, memory savings or worse-case improvement.
- Preserve exact output, avoid regressions in other consumers, and preserve stock-RAM headroom. For 3D games, pursue 30 distinct presented frames/s at 60 Hz simulation; explore higher throughput where drawing capacity allows. Do not equate uncapped submissions with displayed FPS.
- Do not promise 60 FPS or aggregate unrelated historical improvements. Report confidence and raw measurements.

## 2. Inventory and historical evidence

| Existing module | Mechanism | Contract |
|---|---|---|
| `src/graphics/3d/animation/rigid_project_sh2.s` | MAC.L, X/Y divider overlap | Leading run of safe Q15 rigid records; stop before unsupported vertex |
| `src/graphics/3d/animation/blend_project_sh2.s` | MAC.L, exact half-weight blend, divider overlap | Two equal influences only; general path handles all others |
| `src/graphics/3d/animation/clip_joints_sh2.s` | 64-bit MAC, transposed scratch, checked sums | Clip X/Y/W joint rows with placement; reject invalid ranges |
| `src/graphics/3d/rendering/paint_keys_sh2.s` | range, spread, quantise | Preserve skipped keys, bucket rules, depth caps and group bits |
| `src/graphics/3d/scene/screen_cull_sh2.s` | scheduled loads, packed entry writes, shaded/unshaded loops | Only eligible preprojected faces, preserve painter keys and face numbering |
| `src/graphics/3d/scene/projected_bounds_sh2.s` | signed packed XY and out-of-line updates | Nonempty bounds, nearest W |
| `src/graphics/3d/scene/program_run_sh2.s` | eight longword writes per VDP1 command | Ordered program runs, direct/indirect indices, per-face Gouraud |
| `src/graphics/3d/animation/skin.cpp` | inline transform_mac, clip_rows, clip_row | General transforms and projection arithmetic |
| `src/audio/music/codecs/adx_sh2.cpp` | MULS.W, register-resident predictors, unrolled step table | Big-endian frame input/output, exact predictor and clamp |
| `src/core/startup/*.s` | boot, initial state, slave entry | Correct stack/vector/CPU ownership |
| `src/hal/scsp/sound_driver.m68k` | timestamped SCSP register writes | Autonomous timer-driven 68000 playback |
| `src/hal/scu/dsp_transform*.dsp` | 48-bit ALU, 23-instruction vector loop; optional DMA | Integer vectors multiplied by Q16 matrix; bounded batches |
| `src/core/math3d/logic.hpp` | on-chip DIVU with range proof | Exact signed division; C fallback when not representable |

Recent commit evidence (historical Tekken fight rates, not physical console measurements):

| Commit | Change | Images/s before -> after |
|---|---|---|
| [caf48860](https://github.com/celsowm/libsaturn/commit/caf48860) | Bounds and paired order fill | 30.36 -> 30.51 |
| [9a6d86f0](https://github.com/celsowm/libsaturn/commit/9a6d86f0) | Rigid projection | 30.51 -> 30.92 |
| [63344df5](https://github.com/celsowm/libsaturn/commit/63344df5) | Clip joint arithmetic | 30.92 -> 31.76 |
| [4b9bf265](https://github.com/celsowm/libsaturn/commit/4b9bf265) | VDP1 program run | 31.76 -> 32.17 |
| [bd027d66](https://github.com/celsowm/libsaturn/commit/bd027d66) | Painter range/spread | 32.17 -> 32.41 |
| [733f355a](https://github.com/celsowm/libsaturn/commit/733f355a) | Single program writer | 32.41 -> 32.48 |
| [ce7e8bfe](https://github.com/celsowm/libsaturn/commit/ce7e8bfe) | Q15 bounds and cache purge (mixed C++ changes) | 32.51 -> 33.06 |
| [6c5437e5](https://github.com/celsowm/libsaturn/commit/6c5437e5) | Half-weight blend projection | 33.06 -> 33.88 |
| [3ded8883](https://github.com/celsowm/libsaturn/commit/3ded8883) | Depth quantisation | 33.88 -> 34.08 |
| [9968d987](https://github.com/celsowm/libsaturn/commit/9968d987) | Divider scheduling | 34.08 -> 34.27 |
| [a19a227c](https://github.com/celsowm/libsaturn/commit/a19a227c) | Shaded screen-cull path | unshaded control 34.32, secondary rate 32.55 |

The historical sequence moves from 30.36 to 34.32 images/s (~13%); it includes non-assembly changes and separate comparisons. It is not an isolated assembly speedup or a new baseline. Frozen pixels matched in the cited optimization commits where reported. ADX commit [357c080d](https://github.com/celsowm/libsaturn/commit/357c080d) records 3,000 matching random frames in Ymir and 29.82 -> 30.08 images/s. The ADX source estimates 38 -> 23 instructions/sample in the inner loop, not measured cycle savings.

### Negative evidence: do not repeat blindly

- [SKINNING_MAC_BENCH.md](SKINNING_MAC_BENCH.md): isolated MAC.L substitution was 38.60% slower; earlier Q15 specialization improved a synthetic control but regressed real Paul execution.
- [MATRIX_MAC_BENCH.md](MATRIX_MAC_BENCH.md): combined MAC.W/32-bit conversion improved the selected comparison by 11.43%, below the experiment's 20% gate.
- [SKELETAL_ANIMATION_PERFORMANCE_STUDY.md](SKELETAL_ANIMATION_PERFORMANCE_STUDY.md): four-pass stable radix sorting, fused preliminary scan and normal shortcut rejected.
- [CHARACTER_HANDOFF_PERFORMANCE.md](CHARACTER_HANDOFF_PERFORMANCE.md): SCU-DMA command transfer regressed the historical list-upload workload; direct VRAM construction superseded that design.
- [SHARED_RENDERER_PERFORMANCE.md](SHARED_RENDERER_PERFORMANCE.md): unsigned-magnitude projection candidate regressed; an earlier divider overlap experiment differed across emulators. Current pipeline must be measured independently.
- [LIBSATURN_CURRENT_COVERAGE.md](LIBSATURN_CURRENT_COVERAGE.md): SH-2 DMAC is not automatically preferable to CPU RAM copies; SCU DSP timing is approximate and no speedup is established.

A rejected technique may be revisited only with a materially different data layout, workload, scheduling mechanism or demonstrated new bottleneck. Record the difference before spending time.

## 3. Non-negotiable design rules

1. Internal assembly alongside a typed contract header; follow the seven existing GAS sources. Keep reference C++ for host and test-only target comparisons; do not link redundant reference loops into consumer releases. Use inline assembly only for small intrinsic-like operations where inlining demonstrably matters.
2. Keep checks, dispatch, ownership, allocation and cache publication in C++ unless a measured full-batch kernel benefits from absorbing a specific check. A Job descriptor must state alignment, sizes, ranges, aliasing, endianness, minimum count, output on failure and progress semantics. Pin every used offset with static_assert; preserve public ABI.
3. Preserve exact Q15/Q16 and signed rounding, saturation, painter tie order, Gouraud numbering, command bytes, clipping, and near/far behavior. No approximate reciprocal or reduced precision in the default exact path. No silently dropped geometry, frames, ticks, collision pairs or audio samples.
4. Document register map, clobbers, callee-saved registers, stack bytes, MACH/MACL, SR.T/S, branch delay slots, and MMIO side effects. Confirm compiler ABI and generated disassembly. Respect SH-2 branch/literal reach and alignment.
5. Treat the 39-cycle DIVU latency as a hardware contract to verify with the existing latency probe; don't assume multiplication, memory and bus accesses cost one cycle. Each CPU owns its divider; define interrupt policy and forbid conflicting ISR use unless proven safe. Validate SR.S state on both CPUs, not just reset assumptions.
6. Keep cache coherence explicit for Slave jobs and DMA ownership. Distinguish compiler barriers from cache maintenance; don't add blanket purges. Avoid extra VRAM writes, preserve VDP1 bank lifecycle and SCSP deadline behavior.
7. Optimize for 4 KiB per-CPU cache and real bus contention: track code size, data footprint, worst stack, cold/warm behavior, high/low WRAM and parallel execution. Unrolling is conditional on measurements.
8. Use the in-repo Sega/Renesas manuals under `docs/sega_saturn_hardware/` as primary semantics. Before audio/CD changes read AGENTS.md's required hardware guides. No new external runtime dependency or second SDK prefix.

## 4. Phase A — Baseline and differential infrastructure (P0)

### A1. Reproducible baseline

- [ ] Freeze current source SHA, compiler/binutils versions and flags, ELF/ISO hashes, BIOS hash, video standard, cache CCR per CPU, cart configuration, asset hash, emulator version, input script/hash and capture window.
- [ ] Reuse `harness/src/cpu_profile.hpp`, `harness/scripts/analyze_cpu_profile.py`, nested profiling and the existing division, matrix, skinning and memory-copy probes. Attribute exclusive/inclusive modeled cycles, counts and sampled PCs to each CPU separately; retain disassembly and map files.
- [ ] Profile uninstrumented timing separately from instrumented attribution. Capture FRT ticks, p50/p95/p99/max, submissions, distinct presented images where observable, simulation ticks, VDP1 busy/wait, DMA, underruns, memory and stack high water. Never compare wall-clock emulator speed with guest timing.
- [ ] Qualify repeated matched captures (minimum five intervals for microbenchmarks, three deterministic scenario runs for whole workloads); report variation and reject unstable comparisons. Keep automation time and disk within AGENTS.md limits.

### A2. Target differential runner

- [ ] Add a focused target test example/harness, proposed name `sh2_kernel_verify`, not a production API. Compile actual .s routines and independent reference math in the same diagnostic ELF; explicitly invoke both. Use deterministic PRNG seeds, canaries, result hashes, mismatch records and first failing input.
- [ ] Run every existing kernel on Master and Slave, independently and concurrently with distinct storage; check callee-saved registers, stack guards, output lengths and publication.
- [ ] Cover zero/one/max legal sizes, unsupported leading/middle/trailing records, negative and extreme fixed values, W near boundaries, asymmetric blends, pointer alignments, depth ties, skip sentinel, clamping, Gouraud and direct/indirect command paths. Invalid inputs outside the kernel contract must be rejected by caller tests, not passed blindly to assembly.
- [ ] Test interruption at multiple phases, especially MAC and outstanding DIVU; inspect ISR prologues, check SR.S state and explicit ownership. Capture actual target output, not merely host references.
- [ ] Add host guard/layout tests and Make/CMake source-list parity. Use existing CI conventions; distinguish host-only, cross-build and emulator gates. Physical hardware remains a separate acceptance stage.

Exit: reproducible baseline and exact current kernel verification with zero unexplained mismatches; report any existing correctness issue separately before optimization.

## 5. Phase B — High leverage generic rendering (P0)

### B1. Static/general projection

Files: `src/graphics/3d/rendering/logic.hpp`, `api.cpp`, `src/graphics/3d/scene/transform.cpp`.

- [ ] Profile sat_project_vertices and callers for dense static scenes, sparse side camera, general rotated camera, mixed clipping, and indexed/instanced geometry. Inspect compiler output, identify libgcc helpers and per-vertex spills. Existing sparse-camera optimization must remain the control.
- [ ] Prototype a batched exact kernel with camera classification once, specialized general/sparse loops only when justified; compute X/W, start divide, compute Y and preload next vertex; carry W between axes. Preserve existing rounding per product where required (do not reuse skinning's different rounding mechanically).
- [ ] Fast-path eligible vertices; stop and fall back for near plane, clamp, overflow or unsupported transform. Measure break frequency and dispatch overhead. Compare against a C++ batch/control first.
- [ ] Evaluate fusing bounds reduction with projection only when it removes a measured pass without register-pressure regressions. Compare memory traffic and cold instruction cache.
- [ ] Gate with cube ladder, Dino, city walk, world LOD and reference camera sweeps, including fully visible, offscreen and near-plane objects. Match all projected records and frozen frame/command hashes.

### B2. Skeleton hierarchy and matrix composition

Files: `src/graphics/3d/animation/skeleton.cpp`, `logic.hpp`, `src/core/math3d/logic.hpp`.

- [ ] Separate pose sampling, local matrix construction, parent concatenation, placement and clip-joint costs. Clip-joint assembly already exists; do not redo it.
- [ ] Compare a C++ layout/batch control, scheduled DMULS.L, MAC.L, identity/root/sparse-affine paths. Revisit transposed scratch or offline parent-order only if benchmark justifies it; preserve world matrix outputs and parent validation.
- [ ] Focus on common affine transforms and contiguous parent runs, without changing public matrix layout or requiring new asset formats.
- [ ] Test 1/22/66+ joints, branching hierarchies, root transforms, rotation/scale extremes, invalid parents and full animation sequences. Distinguish exact arithmetic from any optional future format redesign.
- [ ] Gate with character examples and unchanged consumer models; report complete skeleton time, not just matrix microbenchmarks.

### B3. General skinning and mixed weights

Files: `skin.cpp`, `skin_parallel.cpp`, rigid/blend headers and assembly.

- [ ] Establish distribution of rigid, half-weight pair, uneven pair, three-influence and fallback vertices in real assets. Record segmentation, joint locality and output ownership.
- [ ] Profile general transform_mac/clip_rows/clip_row; compare preloaded coordinates and fused weighted sum schedules. Preserve per-influence rounding and Q15 floor/nearest behavior exactly.
- [ ] Prototype separate batch kernels for uneven pairs and three influences only where coverage and cost support them. Consider grouping runs in the offline exporter without changing vertex indices/face order; account for remapping and asset bytes.
- [ ] Compare SR.S/overflow boundaries, fallback after partial output, mixed records, seams and deterministic parallel splits. Do not reintroduce the previously rejected generic MAC replacement.
- [ ] Gate with blend-heavy synthetic stress, Dino, Killer Clown and real consumer scenes; retain exact deformation and projected output.

## 6. Phase C — Painter, clipping and VDP1 (P1)

### C1. Final bucket distribution

Files: `rendering/api.cpp`, `paint_keys_sh2.*`, `scene/faces_flush.hpp`.

- [ ] Attribute histogram, prefix sum, final stable placement and order consumption individually. Range/spread/quantise already optimized.
- [ ] Compare stable forward/reverse placement matching current tie semantics, paired halfword stores when aligned, pointer walking and fewer passes. Test multiple bucket occupancy distributions, all-skipped input, equal keys, pass boundaries, near-full queues.
- [ ] Do not revive radix sort simply because it has favorable asymptotic complexity; prove savings for actual Saturn counts. Verify exact queue/order arrays and command bytes.

### C2. Clipping, geometry and culling

Files: `core/geometry/mesh_logic.hpp`, `scene/faces.cpp`, rendering logic.

- [ ] Profile general/non-program paths separately from existing screen_cull. Isolate small-bound cross products, near-plane intersection, world visibility and texture-grid partial cells.
- [ ] Investigate batched orientation tests and bounded fixed-point intersections; use signed range proofs before narrowing. Preserve double-sided rules, degenerate faces, UV/texture cells and Gouraud.
- [ ] Test inside/straddling/offscreen, extreme coordinate bounds, shared seams, and dense occlusion scenes. Require exact visibility decisions and no lost geometry.

### C3. Sprite/command emission

- [ ] Profile generic distorted/scaled sprite batches, mixed materials, UI/particles and remaining command emitters. Extend existing program_run style only where per-command overhead dominates.
- [ ] Compare precompiled command prefixes, longword stores and contiguous output windows. Preserve links, END commands, overlay reservation, color modes, bank fences, interlaced handoff and direct-VRAM list ownership.
- [ ] Validate commands word for word and exercise capacity boundaries, transparent/tinted/Gouraud paths, automatic/explicit framebuffer modes and overlap with active drawing.

## 7. Phase D — Other subsystems (P1/P2, profile-gated)

| ID | Candidate / file | Experiment | Acceptance |
|---|---|---|---|
| D1 | Animation decode/interpolation: animation/clip.cpp | Batch key decoding, fixed-point lerp, common channel specialization | Exact poses, loop boundaries, blend transitions |
| D2 | VDP2 perspective: graphics/vdp2/floor.cpp | Incremental line tables, divider overlap; existing div_s64_fast remains control | coefficient words, near/far, PAL/NTSC and visual parity |
| D3 | Collision: physics/2d and physics/3d | AABB sweeps, overlap masks, common bounds, broadphase traversal | exact ordered contacts, tunnelling cases, gameplay hash |
| D4 | LZ: storage/compression/lz.cpp and lz_stream.hpp | Batched copies and token decode, overlap-safe backreferences | exact bytes, malformed/truncated data and guards |
| D5 | IMA ADPCM: audio/music/codecs/ima.* | register history, table lookup, clamp fast paths | exact sample bytes and predictor continuity |
| D6 | ADX: audio/music/codecs/adx_sh2.cpp | move to .s with byte-identical routine; then separately measure nibble scheduling/stack table alternatives | byte-identical migration, sample exactness, no underruns |
| D7 | Memory/cache: core/memory/bytes.h and hal/sh2/cache.cpp | profile size/alignment-specific copies and purge loops; compare existing benchmark | no unsafe MMIO aliasing, no stale cache or data corruption |

Each candidate is independent. Do not modify streaming, scheduling or data formats to mask a regression. For audio capture actual PCM, SCSP position, ring low-water, underruns and worst refill time; for physics keep the simulation step rate and exact gameplay behavior.

## 8. Phase E — SCU DSP and concurrency (experimental P2)

The existing DSP kernel uses batches of up to 21 vectors, with CPU-fed and DMA-fed variants. Exactness has been demonstrated for the existing limited arithmetic; speedup has not.

- [ ] Measure setup, input DMA, DSP compute, output DMA, cache sync, join and contention separately. Include Master and Slave work while DSP runs. Compare both SH-2 serial and dual CPU controls.
- [ ] Test asynchronous double-buffered batches of static geometry or other bounded vector transforms, with caller-owned buffers and explicit READY/IN_FLIGHT/COMPLETE ownership. Do not block both SH-2 waiting for each batch. Require timeout and deterministic CPU fallback with no late overwrite.
- [ ] Investigate DSP memory tiling and matrix reuse. Preserve 48-bit accumulator limits and exact semantics; no premature use for arbitrary Q16 skinning. DMA addition 2 is the existing proven longword stride.
- [ ] Keep the independent DSP assembler, simulator and generated header freshness checks. Test on both emulators and real hardware before claiming latency or throughput benefit.

## 9. Workload matrix and gates

| Suite | Purpose |
|---|---|
| matrix_mac_bench, skin_mac_bench, division_latency_probe, memory_copy_bench | isolated reference and instruction/bus behavior |
| cube_pipeline_bench, dino_demo, skeletal_animation_3d, skinning_stress_3d, fighter_arena_3d, killer_clown_demo | dense/sparse, rigid/blend, 1/2+ actors, lit/unlit |
| city_walk, world_lod3d_demo, skybridge_3d | large static geometry, HLOD, camera motion |
| physics3_bench, high_speed_platformer, bullet_hell_stress | collision and sprite stress |
| adx_cd_music, cd_streaming_jukebox, scu_dsp_demo, sound_driver_pages_demo | audio deadlines, DSP and 68000 |
| Tekken 3 Saturn; optional SM64DS Saturn, Sonic R Adventure, KOF98 Saturn | private-source consumer qualification if available; no game-specific assumptions in SDK |

Use NTSC and PAL where relevant, stock memory, both CPUs, serial and parallel paths, cold and warmed caches, light and heavy scenes. Keep workload/asset/input identical between candidate and control; distinguish inaccessible consumer assets from failed tests. Do not transfer copyrighted consumer assets into LibSaturn.

For every candidate record:
- Correctness: zero unexpected differential mismatches, guard corruption, timing-induced races, dropped work, changed fixed-frame RGB or command hashes. For intentional nondeterministic capture timing use the same simulation tick, not arbitrary display field.
- Performance: median and tail latency, absolute and relative change, per-CPU modeled cycles, total guest work, real displayed cadence where observable, and variance. Require the objective in section 1 or document the exception with evidence. No unexplained >2% regression on representative control workloads; investigate noise before deciding.
- Resources: .text/.rodata/.data/.bss, WRAM-H/L, stack high-water and cache footprint, VRAM/Sound RAM allocations. No new fixed reserve without explicit budget/review.
- Build: Make and CMake source parity, host tests, target build/link, Ymir inspection and Mednafen timing; physical Saturn status separately PENDING/PASS/FAIL.
- Release: preserve C/C++ public API, library genericity, no persistent experimental toggles or duplicate production implementations. Keep reference/test-only sources, evidence and reproducible commands.

## 10. Execution order and tracking

| Order | Work package | Dependencies | Status |
|---|---|---|---|
| 1 | A1 baseline and hotspot census | none | TODO |
| 2 | A2 differential target/ABI suite | A1 | TODO |
| 3 | B1 static projection candidate | A2 | TODO |
| 4 | B2 skeleton matrix candidate | A2 | TODO |
| 5 | B3 general blend kernel | A2, B2 measurements | TODO |
| 6 | C1 painter placement | A2 | TODO |
| 7 | C2 clipping; C3 sprite emission | A2, profile evidence | TODO |
| 8 | D1–D7 independent subsystem candidates | A2, relevant profiling | TODO |
| 9 | E DSP concurrency | A1/A2, ownership tests | TODO |
| 10 | Release qualification and hardware | accepted candidates | TODO |

Within each package: profile -> inspect disassembly -> define invariant and budget -> C++ candidate -> assembly candidate (if warranted) -> differential -> emulator A/B -> accept or reject. One independently reversible commit per candidate. Do not stack unmeasured optimizations. Rebase results on current main if concurrent changes alter the baseline. Keep rejected approaches in the evidence ledger, not in production code.

Proposed study directory: `harness/studies/sh2-assembly/` with a manifest, compact per-candidate JSON results, explanatory README and scripts (do not commit BIOS, commercial game data or large captures). Include field names: baseline_sha, candidate_sha, toolchain, scenario, input_hash, asset_hash, emulator, video_mode, cpu, samples, timing_units, p50/p95/p99/max, frames/submissions, simulation_ticks, bytes_by_section, image_hash, command_hash, mismatch_count, hardware_status, verdict and reason. Log exact commands and exit status in each experiment; follow AGENTS.md's POSIX/Windows runners and single SDK disk budget.

## 11. Definition of done

- Current kernels have documented contracts, verified target ABI/state and reproducible regression coverage.
- Hotspots across static and animated 3D, painter, VDP1, animation, physics, memory and audio have been measured; candidates accepted or rejected with evidence.
- Accepted kernels demonstrably improve representative workloads without changing outputs or violating resource budgets.
- Documentation separates emulator results from hardware results and release FPS from submission rates.
- No unresolved correctness regression, hidden fallback mutation, duplicated target algorithm or consumer-specific hack.
- Physical-hardware acceptance is explicitly tracked, never fabricated.

This document is a roadmap. Its thresholds, priorities and proposed artifact names are planning choices; all new performance claims require fresh evidence. No implementation or benchmark was performed as part of writing this plan.

## 12. Disposition of the B/C candidates (2026-11-19)

The opt-in switches (`SAT_*_EXPERIMENT`, `SAT_SKIN_BLEND_BENCH`,
`SAT_VDP1_POSE_CAPTURE`, `LIBSATURN_VDP1_HYBRID_EMIT`), their bench and
renderer examples, harness runners and schemas were removed. Every winner
below is exact, so it is now the only path; the reports stay as the record.

| Package | Candidate | Outcome | Evidence |
|---|---|---|---|
| B1 | static projection SH-2 kernel | removed: slower on empty/singleton sets, no unconditional win | `STATIC_PROJECTION_SH2_EXPERIMENT.md` |
| B2 | affine hierarchy compose (SH-2 leaf, `sat_mat4_multiply` fallback) | **default** | `HIERARCHY_CHARACTER_AB.md`: hierarchy -31.7% Mednafen |
| B3 | weighted pair as b + w(a-b) (V2, `subv` guard) | **default** for every deform store | `WEIGHTED_CHARACTER_AB.md`: skin -4.0% Mednafen; the triple form (V1) regressed and is gone |
| C1 | SH-2 prefix/placement, V2 equal-key and top-bucket shortcuts | removed: no gain in the Tekken fight | `PAINTER_ORDER_V2_EXPERIMENT.md`, `PAINTER_DISTRIBUTION_SH2_EXPERIMENT.md` |
| C2 | screen clip mode 2 (edge mask, 32-bit intersection) | **default** | `SCREEN_CLIP_RENDERER_AB.md`: clip -36.8%, submit -5.5% Mednafen |
| C2 | near clip V2-A/B | removed: wins axial cameras, regresses oblique ones (the game case) | `NEAR_CLIP_V2_QUALIFICATION.md` |
| C2 | world cull wide/dot leaves | removed: matched Sonic renderer regressed 1.4-1.7% | `WORLD_CULL_RENDERER_AB.md` |
| C3 | hybrid longword emitter | removed: correct, but no Tekken coverage or gain | `VDP1_EMIT_C3_EXPERIMENT.md`, `VDP1_EMIT_C3_REAL_WORKLOADS.md` |

The isolated MAC studies (`MATRIX_MAC_BENCH.md`, `SKINNING_MAC_BENCH.md`)
lost their examples and runners too; history keeps them.
