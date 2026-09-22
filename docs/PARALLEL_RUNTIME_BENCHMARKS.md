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

## Skybridge smoke workload

The same 90 BIOS frames plus 360-frame `skybridge_smoke.pad` script reached
gameplay in both modes. The explicit-Slave run recorded 111,754,871 Master
instructions and 116,181,529 Slave instructions over the 360 program frames;
355 frames had nonzero Slave samples. This confirms real animation/geometry
dispatch through the Skybridge runtime. It is not an FPS result: screenshots
and cycle counts are emulator diagnostics, while physical frame-time and
visual parity still require a Saturn or a validated emulator capture.
