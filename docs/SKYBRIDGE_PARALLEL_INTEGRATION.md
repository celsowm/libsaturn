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

The Slave receives only fixed-layout, caller-owned jobs. The animation job,
its immutable per-frame `sat_anim_state_t` snapshot, and its alternate pose
buffer are static storage in the example and remain live through terminal
completion and handle release. The asset graph is immutable: the asset, model,
selected clip and position data are published by the animation adapter before
the Slave reads them. The Master never mutates the submitted snapshot.
Geometry preparation writes private faces, keys, order, and per-instance
scratch. The Master owns gameplay mutation, scene merge, global ordering,
command-budget accounting, VDP1 submission, VDP2 state, and audio.

Every gameplay frame decodes the current pig state. When explicit `SLAVE`
geometry is split across four or more visible gems, the Master decodes the pig
while the Slave prepares the gem suffix. Otherwise the animation may be the
Slave task. The completed pose is selected only after successful task
completion, so a busy gem task cannot freeze the character.

A timeout does not mean that the worker stopped. Until completion or a
successful `sat_parallel_abort`, the input descriptor, nested graph, output,
scratch, batch `pending` bit, and handle remain protected. A failed abort or
release enters an explicit safe recovery state and does not dispatch or reuse
those buffers. A rejected submission has no worker ownership, so its partition
is retried synchronously; each partition is marked ready only after a successful
preparation, and failed partitions are never merged.

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
parallel frame row (`MS`, `WAIT`, `TASK`, `ERR`). `MS` is the completed
CPU-side frame-preparation sample shown for that frame and `WAIT` is measured
with the coarse millisecond timer around Master wait calls. The executor's
`master_wait_frt_ticks` remains a separate raw 16-bit FRT delta; it is never
added to milliseconds or displayed as milliseconds. The runtime's FRT
prescaler is configured by the existing SH-2/SCU startup path, and no stable
conversion is assumed here, so wraparound and prescaler-specific conversion are
left explicit in the raw counter. The millisecond timer does not claim
sub-millisecond precision. The row is diagnostic only; it does not change
scheduling or rendering.

The follow-up regression coverage includes delayed animation dispatch after
stack activity, invalid geometry descriptors and capacity failures, static
source checks for pending/abort/release ownership, and equivalent 360-frame
MASTER/SLAVE/AUTO smoke scripts. The harness checks boot, frame completion,
Slave activity, and task profiles; it does not provide a reliable automatic
pixel-by-pixel framebuffer comparator, so the smoke runs are not claimed as
pixel equivalence proof.
