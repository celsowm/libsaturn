# parallel_runtime

This example is the high-level dual-SH-2 demonstration. It registers the
existing baked-animation decoder and the canonical scene geometry batch with
`sat_parallel`, submits coarse immutable work, performs Master-side checksums
while the Slave works, and compares the results with the direct Master
algorithms. Geometry is merged into the normal scene-wide painter before VDP1
submission; it is not a second renderer.

The A button cycles `MASTER`, `SLAVE`, and `AUTO`. In `AUTO`, the measured
geometry batch is pinned to Master while the animation decoder remains
eligible for Slave dispatch; the whole mode falls back to Master when the
Slave cannot start. The displayed wait time is the time spent waiting after
the independent Master work, not the isolated Slave instruction count.

Build and run:

```powershell
.\build-example.ps1 parallel_runtime
.\run-example.ps1 parallel_runtime -Emulator mednafen -BuildFirst
```

The Ymir harness can be used for actual two-CPU execution evidence in the same
way as `examples/dual_sh2`; instruction counts are not a frame-rate claim. The
geometry workload is configurable without source edits:

```powershell
.\build-example.ps1 parallel_runtime -GeometryObjects 48 -ParallelMode SLAVE
.\harness\run-harness.ps1 parallel_runtime -Bios .\bios\saturn_bios_us.bin `
  -Frames 120 -BootFrames 90
```

For the expanded executor/geometry evidence matrix, run
`harness/run-parallel-runtime-sweep.ps1`. It defaults to 360 frames for each
of MASTER, SLAVE and AUTO, with increasing object counts and 1/2/4 faces per
object. It writes raw per-frame telemetry, Ymir instruction/cycle CSVs and a
validated JSON summary under `build/`, then restores the normal 12-object
AUTO artifact. Supply `-GeometryCases` to select a subset, for example
`@('12:1','48:1','96:1')`.

The validation-only build runs four executor cases (small/large input-output
payload crossed with light/heavy work). It records direct callback time,
submit-call time, Master-observed completion/wait, full submit-to-consume
pipeline, task dispatch counts and the worker's raw FRT start/end values. No
timestamps from the two SH-2s are subtracted. The synchronous geometry
preparation+merge and asynchronous submit/overlap/wait/merge/release paths are
timed separately with the Master FRT. These validation-profile frame cycles
include diagnostic work and must not be presented as normal frame performance;
compare them with the uninstrumented mode runs, and treat the FRT measurements
as emulator characterization rather than physical Saturn FPS.

The report rejects baseline mismatches, unexpected input face counts, and
executor output mismatches. It marks a workload as a crossover candidate only
when the median complete asynchronous path is lower than the median direct
prepare+merge path; this is a candidate, not a scheduler threshold or a
speedup claim. The report and raw data are documented in
`docs/PARALLEL_RUNTIME_BENCHMARKS.md`.
