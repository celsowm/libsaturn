# Parallel runtime benchmark report

This report records the reproducible Master-only, explicit-Slave, and
conservative-AUTO comparison for the high-level runtime and geometry adapter.
The example now contains a real quad face per object; older measurements that
reported `GEO FACE 0` were vacuous and are not used here.

## Reproduction

```powershell
.\harness\run-parallel-runtime-sweep.ps1 `
  -Bios .\bios\saturn_bios_us.bin -Frames 120 -BootFrames 90
```

The script builds each variant, runs Ymir, writes instruction/cycle/transfer
CSVs, and finally restores the checked-in artifact configuration: 12 geometry
objects and `AUTO`.

Each CSV contains 90 BIOS boot samples plus 120 program-frame samples. The
table sums instruction samples and averages fixed system-cycle samples for the
120 program frames only. `Slave frames` counts frames with nonzero Slave
instruction samples.

| Geometry objects | Policy | Master instructions | Slave instructions | Slave frames | Mean fixed cycles |
|---:|---|---:|---:|---:|---:|
| 12 | MASTER | 32,704,107 | 0 | 0 | 449,204 |
| 12 | SLAVE | 32,661,525 | 38,149,525 | 115 | 449,204 |
| 12 | AUTO | 32,929,641 | 37,818,549 | 115 | 449,204 |
| 48 | MASTER | 31,800,300 | 0 | 0 | 449,204 |
| 48 | SLAVE | 31,261,746 | 38,289,771 | 115 | 449,204 |
| 48 | AUTO | 31,367,807 | 37,896,218 | 115 | 449,204 |
| 96 | MASTER | 40,119,062 | 0 | 0 | 449,204 |
| 96 | SLAVE | 39,111,408 | 38,597,128 | 115 | 449,204 |
| 96 | AUTO | 39,177,644 | 37,898,242 | 115 | 449,204 |

The explicit-Slave rows prove two-processor task execution. AUTO also uses the
Slave for animation while geometry is forced to Master. The fixed cycle sample
is unchanged, so these instruction totals describe work distribution and
synchronization, not a frame-rate improvement or hardware speedup.

## Correctness checks

The example validates animation against direct synchronous decode and samples
geometry batches against synchronous preparation. Geometry comparison covers
all metrics, painter keys/order, projected/world coordinates, clipping flags,
material semantics, textures/tiled descriptors, and Gouraud values. A
near-plane case moves one object to depth 1 every other half-period, while
camera target motion exercises changing view state.

Host tests compare complete face records and scene-wide painter ordering,
including stable equal-depth order and atomic merge-capacity failure. Ymir
screenshots were used as visual smoke checks; diagnostic text differs by
policy, so no unmasked pixel diff is presented as proof.

## Interpretation and limits

AUTO remains conservative by default for geometry: a zero-initialized
`sat_scene3d_prepare_batch_t` uses
`SAT_SCENE3D_PREPARE_DISPATCH_CONSERVATIVE`, selecting Master in AUTO
based on this measured sweep. A caller may opt into
`SAT_SCENE3D_PREPARE_DISPATCH_RUNTIME` for per-batch executor selection
or `SAT_SCENE3D_PREPARE_DISPATCH_MASTER` to force local preparation.
Skybridge compile flags no longer determine the library's dispatch policy.
No unmeasured crossover threshold or adaptive scheduler is claimed.

The runtime keeps at most one active Slave dispatch (a single task or a batch of up to four) and a bounded queue, ranked by priority and dependencies. A wait timeout
does not reclaim buffers; callers must wait again or use `sat_parallel_abort`,
which stops the worker before marking the task failed and cancelling queued
tasks. Geometry and animation adapters publish/invalidate complete nested input
graphs, and the example uses double-buffered animation output.

No parallel physics, asset processing, or second renderer was implemented.
Instruction profiling proves useful Slave execution, not speedup. Raw CSVs and
probe JSONs are generated under `build/` and are intentionally not committed.

## Skybridge 360-frame workload (earlier smoke measurement)

The same 90 BIOS frames plus 360-frame `skybridge_smoke.pad` script reached
gameplay in all three policies. The table below sums only the 360 post-boot
rows from the instruction CSVs; each CSV contains 450 rows in total.

| Policy | Master instructions | Slave instructions | Slave frames |
|---|---:|---:|---:|
| MASTER | 108,995,728 | 0 | 0 |
| SLAVE | 111,506,380 | 116,181,529 | 355 |
| AUTO | 111,494,967 | 116,181,529 | 355 |

These earlier runs confirm Slave instruction activity, but their identical
SLAVE/AUTO totals did not identify which task type ran. They are instruction
totals from Ymir, not FPS or a hardware speedup claim. The dedicated validation
matrix below adds per-type counters, structured result comparison and explicit
split coverage; the older aggregate totals should not be used as evidence of a
gem partition.

## Skybridge correctness, fault and frame-time validation

Run the deterministic validation matrix with:

```powershell
.\harness\run-skybridge-parallel-validation.ps1 `
  -Bios .\bios\saturn_bios_us.bin
```

The route selects the second course and is indexed by game-side pad polls so
all policies receive identical input despite different emulator pacing. The
runner builds/runs normal MASTER, SLAVE and AUTO, then MASTER/SLAVE/AUTO with a
validation-only forced gem split, followed by isolated executor-failure
profiles. Normal AUTO remains unchanged: its geometry stays on Master. The
forced-split option and executor hooks are compiled only for this validation
build and do not alter the public LibSaturn API.

The report requires 360 contiguous gameplay samples per normal and forced
policy. It checks that frames with at least four visible gems really partition
items between processors, that per-type counters prove animation and geometry
dispatch/completion, and that ordered gameplay state, animation pose and merge
hashes match the MASTER baseline. Separate fault runs check rejected submit,
worker error, timeout plus successful abort, failed abort while work remains
active, and release failure after completion. In unsafe recovery cases the
report requires the task and its buffers to remain pinned, with no later
duplicate submission or merge.

The completed validation run passed all five isolated fault profiles. In
particular, the timeout/abort record showed the task transition to `FAILED`
after the Slave stopped, then synchronous preparation of the two-item suffix
and a two-batch merge; failed abort and failed release retained their pending
handles and produced no later submit or merge.

The run used for this report collected 360 matched gameplay records for each
normal policy. Values below are guest timer milliseconds; the `>17 ms` count is
the report's nominal 60 Hz budget indicator, not an emulated or hardware frame
drop assertion.

| Policy | CPU ms median/p95/max | Frame ms median/p95/max | Frames >17 ms | Master instructions | Slave instructions | Geometry Slave frames | Animation Slave frames | Mean Master cycles |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| MASTER | 13 / 14 / 17 | 15 / 16 / 19 | 4 | 76,601,927 | 0 | 0 | 0 | 449,204 |
| SLAVE | 13 / 15 / 18 | 16 / 17 / 20 | 12 | 76,393,421 | 118,681,093 | 10 | 350 | 449,203 |
| AUTO | 13 / 15 / 18 | 16 / 17 / 20 | 9 | 76,354,381 | 118,681,105 | 0 | 360 | 449,203 |

On this workload the guest timer samples are mostly in the 13–20 ms range and
are quantized to whole milliseconds. The explicit-Slave policy placed the
gem-geometry task on Slave on the 10 frames with four or more visible gems;
AUTO dispatched animation on Slave but kept all gem geometry on Master. The
measurements show work distribution, but do not show a timing improvement over
MASTER. They are not a physical Saturn performance result.

The validation-only v5 telemetry also records per-frame game/pose hashes,
ordered merge hash, visible and partitioned items/faces, scene command hash and
count, per-type submit/complete/failure counters, per-CPU dispatch counters,
frame CPU/frame duration, and guest/emulator cycle and instruction samples.
The map-resolved telemetry block is isolated to validation builds. The forced
split profile is separate from these normal-policy timing figures; production
AUTO remains conservative and does not split gem geometry.

Timing caveats: `frame_cpu_ms` and `frame_ms` use the guest millisecond clock
and are quantized to 1 ms. The validation-only Master FRT counter is 16-bit,
uses `/128`, rolls over at 65,536 ticks (about 292 ms at nominal SH-2 clock),
and measured read overhead is at most 3 ticks per sample in this harness.
Short frame intervals use modular deltas. Task completion latency is timed on
the Master clock; per-CPU FRT counters are never subtracted from one another.
In the Ymir run recorded here, the Slave-local task-duration accumulator
remained zero while Master-observed completion latency advanced. That was an
executor bug, not the emulator's timer: the Slave publishes COMPLETED into the
shared slot before signalling, and the Master only accounted a completion it
still saw as RUNNING, so `completed` and `slave_task_ticks` never moved
(fixed 2026-09-25; dino_demo now reports about 2,990 Slave FRT ticks per task
against about 4,800 ticks of Master-observed latency). The numbers in this
report predate the fix. Emulator instruction and fixed-cycle counts are not
physical FPS or proof of speedup.

During development, ordered merge hashes first diverged only on four-gem split
frames. This exposed that geometry preparation normalized and mutated painter
keys relative to each batch's local depth range. The implementation now derives
the bucket order without modifying those keys; whole-batch and sliced host
regressions pass, and the matched 360-sample structured hashes agree across
normal and forced-split profiles.

Generated artifacts live in `build/skybridge_parallel_validation/`:
`frame_times.csv` has per-frame game/pose/geometry/task telemetry and matching
Ymir instruction/cycle observations; `summary.json` gives median, p95, maximum
`frame_cpu_ms`/`frame_ms`, over-budget counts, per-processor instructions and
cycle averages. The guest timer is quantized to one millisecond. Ymir's
instruction and cycle statistics describe emulator work, not physical Saturn
FPS, so the report does not impose a speedup threshold or claim a speedup.

## Executor cost and expanded geometry matrix

The follow-up harness separates two evidence sets. For every geometry case
and policy it first builds a normal, uninstrumented example and collects 360
program frames of Ymir Master/Slave instruction counts and Master cycle
samples. It then builds a validation-only profile (120 frames by default)
that compares synchronous preparation+merge against the full asynchronous
submit/independent-Master-work/wait/merge/release path, and measures four
executor cases crossing 16/512-byte payloads with 64/16,384 callback
iterations. Set `-DiagnosticFrames` to change only the validation-profile
sample count. The regular-policy runs and diagnostic runs have distinct file
names and must not be combined as if they had the same overhead.

The default geometry set is 12/48/96/192 objects at one face each, plus
48/96/192 objects at two faces and 24/48/96 objects at four faces. The
validation profile checks each asynchronous result against its synchronous
baseline every frame and rejects unexpected source-face counts or any
microbenchmark output mismatch. Raw `*.telemetry.csv`, `*.cycles.csv` and
`*.instructions.csv` remain in `build/`; the per-workload report is
`build/parallel_runtime.validation_summary.json`. The report gives median,
p95 and maximum FRT ticks for complete direct and asynchronous geometry paths,
and for direct/submit/completion/pipeline microbenchmark components. It reports
a crossover candidate only if a workload's asynchronous median is lower; it
does not automatically change `AUTO` or claim a speedup.

The executor probe returns the Slave-local FRT values in its result buffer,
then validates the modular start/end delta. This avoids subtracting
unsynchronized CPU clocks. The current Ymir run now observes advancing
worker-local FRT deltas for every measured callback in SLAVE and AUTO; this
validates that the worker timer is readable in this harness, not that its
calibration exactly matches physical hardware. The timer is the Master/worker
16-bit FRT at `/128`, so intervals must remain below rollover and short
intervals are quantized. Direct callback time and asynchronous pipeline time
include different executor/publication costs by design; neither alone is a
pure count of useful geometry arithmetic.

## Ymir result snapshot (2026-09-23)

The reduced workload sweep used identical generated geometry in all modes:
12 objects × 1 face, 48 × 2, and 96 × 4. Each normal run recorded 360
post-boot emulator frames; each validation profile produced 38, 28, and 18
complete application-frame telemetry records, respectively. All 252 records
matched the synchronous geometry baseline; the SLAVE and AUTO executor probes
dispatched measured callbacks to the Slave, and the Slave-local FRT advanced
in every callback sample. The full raw data and all nine mode/workload results
are in `build/parallel_runtime.validation_summary.json` and the adjacent CSVs.

The geometry columns are Master-local FRT ticks for synchronous prepare+merge
and the complete asynchronous submit/overlap/wait/merge/release interval. Each
cell is median / p95 / maximum. `Δ med` is async median minus direct median.

| Load | Mode | Samples | Direct ticks med/p95/max | Async ticks med/p95/max | Δ med |
|---|---|---:|---:|---:|---:|
| 12×1 | MASTER | 38 | 383.5 / 387 / 387 | 953.5 / 958 / 958 | +570 |
| 12×1 | SLAVE | 38 | 383.5 / 387 / 387 | 856 / 865.15 / 866 | +472.5 |
| 12×1 | AUTO | 38 | 383.5 / 387 / 387 | 833 / 837 / 837 | +449.5 |
| 48×2 | MASTER | 28 | 1488 / 1496 / 1496 | 3358 / 3371 / 3371 | +1870 |
| 48×2 | SLAVE | 28 | 1488 / 1496 / 1496 | 3418 / 3439.5 / 3445 | +1930 |
| 48×2 | AUTO | 28 | 1488 / 1496 / 1496 | 3241 / 3253.65 / 3254 | +1753 |
| 96×4 | MASTER | 18 | 4049 / 4066.1 / 4078 | 8038 / 8065.45 / 8085 | +3989 |
| 96×4 | SLAVE | 18 | 4048.5 / 4066.1 / 4078 | 8768.5 / 8794 / 8794 | +4720 |
| 96×4 | AUTO | 18 | 4048.5 / 4066.1 / 4078 | 8014.5 / 8042.6 / 8063 | +3966 |

No geometry crossover appeared: the measured asynchronous path was slower than
the direct path in all nine combinations. Increasing the load raised both
costs, but did not amortize dispatch, waiting and result handling in this
example. This supports keeping AUTO geometry on Master; it is not evidence
that parallelism cannot benefit a different, larger/coarser task.

The isolated microbenchmark also confirms worker-time measurement. At 96×4 in
SLAVE, the four callback cases (16/512-byte payload × 64/16,384 iterations)
had direct medians of 8.5, 104, 1156 and 1252 ticks; full submit-to-consume
pipeline medians were 81, 303.5, 1192 and 1451 ticks. Corresponding Slave-local
FRT medians were 8, 112, 1156 and 1259 ticks (18 samples each). Even the
heaviest callback did not make the complete pipeline faster than direct
execution in this matrix.

Across the uninstrumented 360-frame runs, Ymir reported Master-cycle medians
of 449,204 and p95 values of 449,206 for all cases; maxima were 449,207–449,209.
Those counts are per fixed emulator video frame and are nearly constant by
construction, so they are not an elapsed-frame performance metric. The
post-boot Master/Slave instruction totals are retained in the JSON report to
show work distribution, not speedup. These are emulator observations, not
physical Saturn FPS.
