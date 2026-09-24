# LibSaturn high-level parallel runtime

The high-level runtime in `saturn/parallel.h` is a small Saturn-specific
executor, not a desktop thread pool. It has one Master context, one optional
Slave worker, a caller-provisioned fixed queue, and the existing
`saturn/dual_sh2.h` HAL as its only hardware transport.

## Ownership and queue contract

`sat_parallel_init` owns the Slave while enabled. Applications using it must
not also call the low-level Slave lifecycle API. `MASTER` never starts the
Slave, `SLAVE` reports startup failure, and `AUTO` falls back to Master.

The bounded queue uses generation-bearing handles:

```text
FREE -> QUEUED -> RUNNING -> COMPLETED
                         \-> FAILED
       QUEUED -> CANCELLED
```

Only one task runs on the Slave at a time. `sat_parallel_service` is
non-blocking. `sat_parallel_wait` may return `SAT_ERR_TIMEOUT` while the task
is still `RUNNING`; the timeout does not cancel it or make its buffers reusable.
The caller must wait again or call `sat_parallel_abort`. Abort stops the Slave
first, then marks the task failed, invalidates its ranges, cancels queued tasks,
and returns terminal ownership. If abort itself fails, the task remains
running and all reachable storage stays reserved; the caller must not clear its
pending bit, dispatch a duplicate writer, or release the handle.
`sat_parallel_release` is valid only after a terminal state. This ordering
prevents late Slave writes from racing with a reused output buffer.

## Memory and cache contract

Input/output storage and the task descriptor are caller-owned and must remain
valid until completion and release. No task may modify its input or reuse its
output while queued or running. Slave-capable spans must be in Saturn Work RAM
and must not overlap mutable Master state.

The contract applies to the complete reachable graph, not only the top-level
job: nested descriptors, arrays, scratch spans, meshes, materials, textures,
model assets, animation state, and camera data keep the same lifetime. Adapters
publish/invalidate each reachable range before the worker reads it and use
uncached aliases for explicit shared output. The executor synchronizes task
metadata/output on completion and never touches VDP, SCU, SMPC, CD, or SCSP
registers.

## Animation integration

`sat_anim_parallel_register` registers the existing deterministic
`sat_anim_decode` implementation, so fixed-point output is equivalent. Animation
advancement remains Master-side state mutation. The example uses two output
buffers: the active read buffer is never written by the outstanding task, and
the write buffer is published only after the handle is terminal and validation
succeeds.

## Geometry integration

`SAT_PARALLEL_TASK_SCENE_GEOMETRY` reuses canonical
`sat_scene3d_prepare_batch_execute`. It is preparation only; scene merge, global
face ordering, VDP1 command generation, command-budget accounting, and hardware
submission remain Master-owned.

`sat_scene_prepare_batch_async` captures camera/view state, records a handle and
pending bit in the batch, and does not retain the scene. The batch owns its
items, per-item scratch, faces and depth keys until
`sat_scene_prepare_batch_release`. Merge checks the pending handle before
committing. All nested mesh/material/texture/scratch ranges are synchronized
before the worker reads them. The worker writes face output and depth keys
through uncached aliases; **only the final scene** allocates painter-order
scratch and performs the stable global pass, preserving source order on ties.

`sat_scene3d_prepare_batch_slice` creates a bounded source-order view for a
partitioned workload. It copies only item descriptors and camera state into
caller-owned storage; each slice has independent faces, keys and vertex
scratch buffers; only the final scene owns the painter ordering array. A caller can execute the Master slice while the other slice is
queued on the Slave, then merge the slices in their original source order.
This is the geometry pattern used by Skybridge's explicit `SLAVE` mode.

```c
sat_scene3d_prepare_batch_init(&batch, items, item_count,
                               prepared_faces, prepared_keys,
                               capacity);
/* Optional: delegate to the configured executor instead of the
 * conservative AUTO geometry default. Other independent workloads may
 * select MASTER without changing any library compile flags. */
batch.dispatch = SAT_SCENE3D_PREPARE_DISPATCH_RUNTIME;
sat_scene_prepare_batch_async(&scene, &batch, &handle);
/* Independent Master work. */
sat_parallel_wait(handle, timeout);
sat_scene_merge_prepared_batch(&scene, &batch, handle);
sat_scene_prepare_batch_release(&batch, handle);
```

## AUTO policy and validation

AUTO currently keeps geometry on the Master while animation remains eligible
for Slave dispatch. Explicit Skybridge `SLAVE` mode can partition visible gem
preparation; this is intentionally not enabled by AUTO until a measured
crossover policy exists. Runtime statistics are real queue/backend/FRT-tick
counters, not a speedup claim.

Skybridge's versioned frame telemetry and forced-split/fault profiles are
validation-build-only and do not change the public API. Its Master FRT samples
use the 16-bit `/128` counter and modular short-interval deltas. Ymir currently
does not advance the reported Slave-local task-duration accumulator, so only
Master-observed completion latency and per-type dispatch/completion counts are
usable there; no cross-CPU timer subtraction is valid.

The `parallel_runtime` example validates animation against direct decode and
sampled geometry against synchronous preparation. Geometry validation compares
metrics, depth keys, projected/world coordinates, clipping, material semantics,
texture descriptors, and Gouraud values. Host tests cover generation-safe
handles, complete face equivalence, global ordering, and atomic merge capacity
failure. `examples/dual_sh2` remains the low-level HAL validation.

Future adapters should keep algorithms pure, use fixed-layout caller-owned
jobs, validate nested pointers, submit coarse batches, keep hardware ownership
on Master, and compare Master/Slave results before enabling AUTO.
