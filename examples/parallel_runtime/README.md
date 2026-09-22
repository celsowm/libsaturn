# parallel_runtime

This example is the high-level dual-SH-2 demonstration. It registers the
existing baked-animation decoder with `sat_parallel`, submits one immutable
batch, performs a master-side checksum while the Slave works, and compares the
result with the direct Master decoder.

The A button cycles `MASTER`, `SLAVE`, and `AUTO`. `AUTO` falls back to Master
when the Slave cannot start. The displayed wait time is the time spent waiting
after the independent Master work, not the isolated Slave instruction count.

Build and run:

```powershell
.\build-example.ps1 parallel_runtime
.\run-example.ps1 parallel_runtime -Emulator mednafen -BuildFirst
```

The Ymir harness can be used for actual two-CPU execution evidence in the same
way as `examples/dual_sh2`; instruction counts are not a frame-rate claim.
