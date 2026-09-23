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

AUTO is intentionally conservative: geometry uses
`sat_parallel_submit_master()` because the sweep does not show a fixed-cycle
benefit from moving it to the Slave, while animation remains eligible for
Slave dispatch. There is no unmeasured workload threshold or adaptive
scheduler.

The runtime still has one active Slave task and a bounded queue. A wait timeout
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
In the Ymir run, the Slave-local task-duration accumulator remained zero even
after explicitly selecting `/128`, while Master-observed completion latency
and frame intervals advanced. The report does not interpret that Slave
duration field as measured work; this is an emulator timer limitation still
requiring confirmation on hardware or a reliable independent clock. Emulator
instruction and fixed-cycle counts are not physical FPS or proof of speedup.

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
