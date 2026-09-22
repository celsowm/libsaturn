# LibSaturn high-level parallel runtime

The high-level runtime in `saturn/parallel.h` is a small Saturn-specific
executor, not a desktop thread pool. It has one Master execution context, one
optional Slave worker, a caller-provisioned fixed queue, and the existing
`saturn/dual_sh2.h` HAL as its only hardware transport.

## Ownership and initialization

`sat_parallel_init` is the sole owner of the Slave when the high-level runtime
is enabled. It installs its own entry callback and starts the existing HAL.
Applications using this runtime must not also call
`sat_dual_sh2_configure_slave`, `sat_dual_sh2_start`, or `sat_dual_sh2_stop`.
The low-level API remains available for applications that choose the low-level
example instead. The two ownership models cannot be active at the same time.

`SAT_PARALLEL_MASTER` does not start the Slave. `SAT_PARALLEL_SLAVE` reports
startup failure to the caller. `SAT_PARALLEL_AUTO` attempts startup and falls
back to Master execution if startup is unavailable. `sat_shutdown` also shuts
down an active parallel runtime.

## Public API

```c
sat_parallel_config_t config = {
    SAT_PARALLEL_AUTO, 0, 0, 0, 60000
};
sat_anim_parallel_register();
sat_parallel_init(&config);

sat_anim_decode_job_t job = { asset, state, vertices, vertex_cap, 0 };
sat_parallel_handle_t handle;
sat_anim_decode_async(&job, &handle);

/* Independent Master work can run here. */
sat_parallel_service();
sat_parallel_wait(handle, 60000);
sat_parallel_release(handle);
```

Tasks are registered with `sat_parallel_register_task(type, process)`. A
process function is deliberately a narrow data-processing callback: it gets a
read-only input span, a writable output span, and an output byte count. The
executor never transmits arbitrary function pointers in a task message; only a
registered numeric type is sent, and the Slave dispatches through the shared
registration table.

## Queue, handles, and state transitions

The queue is bounded and uses no gameplay-time heap allocation. A slot moves
through:

```text
FREE -> QUEUED -> RUNNING -> COMPLETED
                         \-> FAILED
       QUEUED -> CANCELLED
```

Handles contain a slot index and a generation. Releasing a terminal slot
increments the generation, so an old handle cannot address a reused slot.
Because the low-level transport has one mailbox slot in each direction, only
one task is running on the Slave at a time; other submissions remain queued.

`sat_parallel_service` is non-blocking: it consumes a currently available
completion and dispatches at most the next task. `sat_parallel_wait` uses the
low-level bounded response wait only after independent Master work has been
given an opportunity to run. A running task is never cancelled or reset.

## Memory and cache contract

Input and output storage, and the task descriptor itself, are caller-owned and
must remain valid until completion and release. No task may modify its input or
reuse its output while the task is queued or running. When the Slave backend is
possible, the top-level input/output spans must be in Saturn Work RAM and must
not overlap mutable Master state. Nested pointers in a registered job have the
same lifetime rule and must point to immutable data or to the job's declared
output region.

The executor stores physical Work-RAM addresses in its descriptor. Before
dispatch it publishes the input and descriptor through the existing SH-2 cache
line operation; the Slave reads shared metadata and writes output through the
uncached P2 alias. The Master purges/invalidates the output range before
consuming it. The runtime does not touch VDP, SCU, SMPC, CD, or SCSP registers.

These rules are why a temporary stack buffer cannot be submitted and then
returned from its scope. The `parallel_runtime` example waits before its local
job leaves scope; real asynchronous code should keep jobs in persistent Work
RAM storage.

## Animation integration

`sat_anim_parallel_register` registers `SAT_PARALLEL_TASK_ANIMATION_DECODE`.
`sat_anim_decode_async` reuses the existing deterministic
`sat_anim_decode` implementation; there is no Master-specific or Slave-specific
animation solver. It writes caller-owned vertices and returns the exact same
fixed-point result as the synchronous API. Animation advancement remains a
Master-side state mutation and is therefore a natural dependency before
submission.

Geometry, final scene composition, VDP1 command buffers, global face ordering,
and hardware uploads remain Master-owned. The current integration intentionally
does not move those operations to the Slave.

## Automatic policy and diagnostics

The initial AUTO policy is conservative: it uses the Slave when startup
succeeds and otherwise uses Master execution. It does not claim an automatic
speedup or use an unmeasured workload threshold. Applications can select
MASTER or SLAVE explicitly for regression and benchmark comparisons.

`sat_parallel_stats` reports submissions, queue occupancy, completions,
failures, cancellations, and the number of tasks executed by each backend.
These are runtime counters, not fabricated cycle measurements. For complete
frame timing, measure from frame begin through frame end and report Master wait
separately, as done by the example and the Ymir instruction profiler.

## Example and validation

Build/run the high-level example with:

```powershell
.\build-example.ps1 parallel_runtime
.\run-example.ps1 parallel_runtime -Emulator mednafen -BuildFirst
```

`examples/dual_sh2` remains the low-level HAL validation. The new example
cycles through all three policies, renders 32 procedural animated objects,
submits the baked-animation decoder, performs a Master checksum while the task
is outstanding, and compares the resulting vertices against a direct Master
decode. Its success indicator is based on that data comparison and real task
counters.

Host coverage in `tests/host/test_parallel_queue.cpp` verifies generation-safe
handles, stale-handle rejection, capacity decoding, and terminal states. The
target build compiles the actual Slave worker and links it into the normal
boot image. An emulator run requiring a BIOS is still needed to claim two-CPU
execution; instruction counts must not be presented as physical frame-rate
measurements.

## Adding a future workload

1. Keep the algorithm pure and deterministic, preferably in the owning
   subsystem's existing logic header/source.
2. Define a fixed-layout caller-owned job and explicit output capacity.
3. Register one stable task type before starting the runtime.
4. Validate nested pointers and all ownership dependencies in the subsystem.
5. Submit coarse batches, not one task per vertex/contact.
6. Let the Master own hardware registers and final scene/resource commits.
7. Compare Master and Slave output before enabling AUTO for a new workload.
