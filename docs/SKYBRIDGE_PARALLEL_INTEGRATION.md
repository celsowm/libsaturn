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
scratch, batch `pending` bit, and handle remain protected. A successful abort
invalidates the task result after the Slave is confirmed offline—even if the
callback published `COMPLETED` while honoring the stop request—so Skybridge
recomputes that partition synchronously rather than merging aborted output. A
failed abort or release enters an explicit safe recovery state and does not
dispatch or reuse those buffers. A rejected submission has no worker ownership,
so its partition is retried synchronously; each partition is marked ready only
after a successful preparation, and failed partitions are never merged.

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

The regular regression coverage includes delayed animation dispatch after
stack activity, invalid geometry descriptors and capacity failures, and
pending/abort/release ownership. The dedicated Skybridge validation build adds
versioned per-frame telemetry and can force the geometry split even in `AUTO`;
these switches and executor fault hooks are absent from normal builds. The
deterministic `skybridge_parallel_validation.pad` route selects the second
course and drives through gameplay until it produces frames with at least four
visible gems. The report rejects a run if it does not observe a real
Master/Slave partition and successful per-type task counters.

Run the full validation matrix (normal policies, forced-split policies, then
each executor failure in a separate emulator process) with:

```powershell
.\harness\run-skybridge-parallel-validation.ps1 `
  -Bios .\bios\saturn_bios_us.bin
```

The runner writes per-frame telemetry, instruction/cycle samples, and probe
JSON under `build/skybridge_parallel_validation/`. `frame_times.csv` contains
the matched first 360 gameplay samples for MASTER, SLAVE, and AUTO, including
game/animation/merge and VDP1 scene-command hashes, item/face partitions,
per-type task counters, `frame_cpu_ms`, `frame_ms`, and Ymir instruction/cycle
counts. `summary.json` contains median, p95, maximum and over-budget counts. The
report compares ordered structured results against MASTER and fails on missing
split work or any mismatch. Fault runs separately exercise rejected
submission, worker error, timeout with successful abort, abort failure while
the worker is still active, and release failure after completion.

The hashes compare structured gameplay and geometry outcomes; they are not a
pixel-equivalence assertion. `sat_time_ms()` has one-millisecond resolution,
and emulator instruction/cycle counts are not Saturn wall-clock timings. Treat
the measurements as workload and scheduling evidence, not as proof of FPS or
speedup. Production AUTO remains conservative and does not force geometry
splitting.

The validation-only Master FRT is 16-bit at `/128`; modular deltas are used for
short intervals below its approximately 292 ms rollover at nominal SH-2 clock.
The measured read overhead is at most 3 ticks per sample. Completion latency is
measured on the Master only; CPU-local timer readings are never subtracted
across processors. Slave-local task-duration ticks read zero before
2026-09-25 because of an executor accounting bug (completions the Slave had
already marked COMPLETED were skipped), not an emulator limit; runs recorded
before that date carry the zero.
