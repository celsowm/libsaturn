# Skybridge dual-SH2 integration

Skybridge uses the existing `sat_parallel` runtime; it does not create a
second renderer or call the low-level Slave lifecycle directly. The build-time
mode is selected with `-ParallelMode MASTER|SLAVE|AUTO` and defaults to
`AUTO`.

## Frame ownership

```text
Master: input -> fixed-step game -> camera -> platform submission
                    |                         |
                    |                         +-- Slave mode: split visible gems
                    |                             Master slice + Slave slice
                    +-- animation decode when geometry does not occupy Slave

Master: wait/validate -> merge in source order -> global painter -> VDP1/VDP2
```

The Slave receives only fixed-layout, caller-owned jobs. Animation writes an
alternate pose buffer. Geometry preparation writes private faces, keys, order,
and per-instance scratch. The Master owns gameplay mutation, scene merge,
global ordering, command-budget accounting, VDP1 submission, VDP2 state, and
audio. A timeout never recycles a running buffer; the handle is waited again or
aborted before release.

The executor has one active Slave task. Therefore Skybridge gives geometry
priority when at least four gems are visible in explicit `SLAVE` mode; on those
frames the Master prepares its source-order half while the Slave prepares the
other half. When that split is not selected, animation is eligible for Slave
decode. `AUTO` remains conservative: animation may use the Slave, while gem
geometry stays on the Master until a measured crossover policy exists.

## Reproduce

```powershell
.\build-example.ps1 skybridge_3d -ParallelMode MASTER
.\harness\run-harness.ps1 skybridge_3d -Bios .\bios\saturn_bios_us.bin `
  -Frames 360 -BootFrames 90 -PadScript .\harness\scripts\skybridge_smoke.pad `
  -Out .\build\skybridge_master.json `
  -ProfileInstructions .\build\skybridge_master_instr.csv

.\build-example.ps1 skybridge_3d -ParallelMode SLAVE
.\harness\run-harness.ps1 skybridge_3d -Bios .\bios\saturn_bios_us.bin `
  -Frames 360 -BootFrames 90 -PadScript .\harness\scripts\skybridge_smoke.pad `
  -Out .\build\skybridge_slave.json `
  -ProfileInstructions .\build\skybridge_slave_instr.csv
```

The harness supplies Ymir's generic Slave reset-vector handoff for Skybridge
as well as for the dedicated dual-SH2 examples. This is an emulator adapter;
the guest still exercises the real `SSHON`, shared mailbox, task dispatch,
completion, and `SSHOFF` lifecycle.

The instruction CSV is evidence of work distribution, not a speedup claim.
`sat_time_ms()` is a coarse one-millisecond guest timer, and Ymir's fixed-cycle
window does not represent a physical Saturn frame-time measurement. Compare
the same gameplay script and inspect task counts, wait/timeout/failure counts,
prepared faces, rendered faces, VDP1 commands, and over-budget frames together.

Press `Y` during play to show the existing face/command budget plus the
parallel frame row (`MS`, `WAIT`, `TASK`, `ERR`). The row is diagnostic only;
it does not change scheduling or rendering.
