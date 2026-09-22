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
way as `examples/dual_sh2`; instruction counts are not a frame-rate claim.
The measured comparison is documented in
`docs/PARALLEL_RUNTIME_BENCHMARKS.md`.
