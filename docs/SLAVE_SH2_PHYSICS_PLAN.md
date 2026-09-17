# Slave SH-2 Physics Execution Plan

## Goal

Add a generic Slave SH-2 execution layer to LibSaturn and use it as an optional asynchronous backend for physics work.

The objective is **not** to make the Slave SH-2 a physics-only coprocessor. LibSaturn should first gain a small, deterministic job system that owns Slave SH-2 startup, synchronization, shared-memory rules, completion signaling, and failure handling. Physics should then become the first high-value subsystem built on top of that infrastructure.

The target usage is:

```c
sat_physics_world_t world;
sat_physics_world_init(&world, &config);

sat_physics_step_async(&world);

/* Master SH-2 continues independent work here. */
sat_animation_update(...);
sat_scene_prepare(...);

sat_physics_wait(&world);
```

Internally:

```text
Master SH-2                              Slave SH-2
    |                                        |
    | build immutable physics input          |
    | publish job -------------------------->|
    |                                        | integrate bodies
    | AI / animation / scene work            | broad phase
    |                                        | static collision
    |                                        | contact generation
    |<-------------------------- completion  |
    | consume transforms / events            |
    | render                                 |
```

The first implementation should prioritize **correct synchronization, deterministic results, low communication overhead, zero hidden allocation, and clean fallback to Master SH-2 execution**.

---

## Why physics is a good first Slave SH-2 workload

The current public physics API is intentionally small and mostly body-oriented:

```c
sat_body2_step(...);
sat_body2_move_tiles(...);
sat_body2_move_boxes(...);
sat_body2_separate(...);
```

The implementation already keeps the public C API thin and delegates the actual behavior to internal physics logic. That is a useful foundation because the same pure physics operations can remain shared by both CPUs.

Physics is especially suitable for Slave SH-2 execution when work is submitted in **coarse batches**:

- integration of many bodies;
- broad-phase spatial updates;
- tile/static-world collision;
- batches of raycasts or overlap tests;
- contact generation;
- particle-like physics;
- large groups of independent kinematic objects.

Physics is a poor candidate when every small operation becomes a cross-CPU call. The architecture must therefore avoid patterns such as:

```text
Master -> step body 0 -> wait
Master -> step body 1 -> wait
Master -> step body 2 -> wait
```

and instead prefer:

```text
Master -> step entire world / batch -> continue other work -> wait once
```

---

## Design principles

1. **Build a generic Slave SH-2 job system first.**
   Physics uses the job system; the job system must not depend on physics.

2. **Keep physics logic CPU-agnostic.**
   Collision, integration, broad-phase, and solver code should not contain SH-2 synchronization or SMPC code.

3. **Use coarse jobs.**
   One world step or one large batch should normally be one submitted job.

4. **Single-writer ownership for mutable state.**
   Master and Slave must not freely mutate the same body arrays at the same time.

5. **Make memory visibility explicit.**
   Shared structures, mailboxes, command queues, and result buffers must have documented cache/coherency rules.

6. **No hidden heap allocation.**
   Caller-owned or statically provisioned storage should remain the default LibSaturn model.

7. **Master fallback is mandatory.**
   Every high-level physics feature must remain usable without the Slave SH-2 backend.

8. **Determinism matters more than theoretical parallelism.**
   The Slave backend should produce the same gameplay-relevant result as the Master backend for the same inputs.

9. **Do not synchronize more than necessary.**
   The useful parallel window exists between job publication and the first game-system dependency on physics results.

10. **Measure bus contention, not only CPU time.**
    A faster Slave routine is not a win if it stalls VDP/SCSP/CPU traffic enough to worsen frame time.

---

# Phase 0 - Hardware and memory-model validation spike

Before exposing a public job API, build the smallest possible experiment that proves reliable Master/Slave communication in the environments LibSaturn supports.

Suggested example:

```text
examples/slave_sh2_smoke/
```

The experiment should:

1. start the Slave SH-2;
2. point it at a known entry routine;
3. publish a small command in shared memory;
4. have the Slave transform an input buffer;
5. publish a completion sequence number;
6. verify the result on the Master;
7. repeat for many frames;
8. verify clean behavior across reset/reinitialization where practical.

The spike must establish and document:

- where Slave bootstrap code lives;
- how the Slave obtains its initial stack and entry point;
- which shared-memory region is used for mailbox/control state;
- which regions are safe and practical for large job payloads;
- exact cache visibility rules required by both SH-2s;
- ordering barriers or volatile access requirements;
- how completion is observed;
- whether polling alone is sufficient for the first version;
- how Slave shutdown/reset is handled;
- emulator and hardware differences, if any.

### Acceptance criteria

- A command/result round-trip succeeds deterministically for at least thousands of iterations.
- Shared-memory visibility rules are documented in code comments and developer documentation.
- The test does not depend on undefined timing delays.
- The Master can detect initialization failure instead of hanging forever.

---

# Phase 1 - Slave SH-2 HAL and lifecycle

Create a low-level internal layer responsible only for bringing the Slave SH-2 online and exchanging synchronization state.

Suggested files:

```text
src/hal/slave_sh2.hpp
src/hal/slave_sh2.cpp
src/core/slave_sh2_api.cpp
include/saturn/slave_sh2.h
```

The public API should remain small.

Possible first shape:

```c
typedef enum sat_slave_state {
    SAT_SLAVE_OFF = 0,
    SAT_SLAVE_BOOTING,
    SAT_SLAVE_IDLE,
    SAT_SLAVE_BUSY,
    SAT_SLAVE_ERROR
} sat_slave_state_t;

sat_result_t sat_slave_init(void);
void sat_slave_shutdown(void);
sat_slave_state_t sat_slave_state(void);
int sat_slave_available(void);
```

The HAL owns:

- SMPC interaction needed to start/stop the Slave;
- bootstrap entry configuration;
- mailbox addresses;
- low-level memory-order/cache helpers;
- timeout/error detection;
- state transitions.

The high-level API must not expose raw SMPC command sequencing.

### Acceptance criteria

- `sat_slave_init()` is idempotent or clearly documents its lifecycle restrictions.
- Failure never leaves the library in an ambiguous half-initialized state.
- A game can explicitly choose not to use the Slave.
- Existing applications continue to boot exactly as before if the feature is unused.

---

# Phase 2 - Generic Slave SH-2 job system

Build a minimal fixed-capacity command system on top of the HAL.

Do not start with a fully generic function-pointer RPC mechanism. Function pointers across CPUs create unnecessary ABI, relocation, ownership, and safety complexity.

Prefer explicit job kinds with registered internal handlers:

```c
typedef enum sat_job_kind {
    SAT_JOB_NONE = 0,
    SAT_JOB_PHYSICS_STEP,
    SAT_JOB_SPATIAL_UPDATE,
    SAT_JOB_RAYCAST_BATCH,
    SAT_JOB_USER_BASE = 128
} sat_job_kind_t;
```

Possible public API:

```c
typedef uint16_t sat_job_handle_t;

typedef struct sat_job_desc {
    uint16_t kind;
    uint16_t flags;
    const void* input;
    uint32_t input_size;
    void* output;
    uint32_t output_size;
} sat_job_desc_t;

sat_result_t sat_job_submit(
    const sat_job_desc_t* desc,
    sat_job_handle_t* out_handle
);

int sat_job_done(sat_job_handle_t handle);
sat_result_t sat_job_wait(sat_job_handle_t handle);
```

For the first version, **one in-flight Slave job at a time is acceptable**. A single-worker queue with capacity greater than one can be added only when profiling justifies it.

### Internal mailbox

A likely control structure:

```c
typedef struct sat_slave_mailbox {
    uint32_t protocol_version;
    uint32_t submit_sequence;
    uint32_t complete_sequence;
    uint16_t job_kind;
    uint16_t state;
    uint32_t input_addr;
    uint32_t input_size;
    uint32_t output_addr;
    uint32_t output_size;
    int32_t result;
} sat_slave_mailbox_t;
```

Exact layout should be selected after Phase 0 hardware validation.

### Critical rule

Publishing a job must be an explicit sequence:

```text
Master writes payload
        |
        v
Master makes payload visible
        |
        v
Master publishes command sequence last
        |
        v
Slave observes new sequence
        |
        v
Slave executes
        |
        v
Slave makes output visible
        |
        v
Slave publishes completion sequence last
```

Never rely on field-write timing accidentally appearing atomic or ordered.

---

# Phase 3 - Physics world abstraction

The current per-body helpers should remain available, but asynchronous execution needs a world/batch abstraction.

Add a higher-level physics world without breaking the existing simple API.

Suggested public types:

```c
typedef struct sat_physics_world_config {
    uint16_t max_bodies;
    uint16_t max_contacts;
    uint16_t max_static_boxes;
    uint16_t flags;
} sat_physics_world_config_t;

typedef struct sat_physics_body_state {
    sat_box2_t box;
    sat_vec2_t vel;
    uint16_t flags;
    uint16_t user_id;
} sat_physics_body_state_t;

typedef struct sat_physics_contact {
    uint16_t body_a;
    uint16_t body_b;
    sat_vec2_t normal;
    sat_fx16_t penetration;
    uint16_t flags;
} sat_physics_contact_t;
```

The opaque or caller-storage-backed world should own logical collections of:

- dynamic bodies;
- static colliders;
- tilemap reference/configuration;
- broad-phase structures;
- output transforms;
- contact/event buffers.

Do not require games that only use `sat_body2_step()` to create a world.

### Acceptance criteria

- Current per-body API remains source compatible.
- World execution on Master SH-2 can be implemented and tested before any Slave backend is enabled.
- World data has no hidden dynamic allocation.

---

# Phase 4 - Master backend first

Before running the world on the Slave, implement the complete world pipeline synchronously on the Master.

Suggested execution stages:

```text
1. validate / begin step
2. integrate velocities
3. update broad phase
4. resolve static/tile collision
5. generate dynamic pairs
6. narrow phase
7. resolve contacts
8. write transforms
9. emit contact/events
```

Possible API:

```c
sat_result_t sat_physics_world_step(
    sat_physics_world_t* world,
    uint16_t steps
);
```

This creates the reference implementation used for correctness comparison with the Slave backend.

### Why this phase is mandatory

Without a Master reference backend, bugs in the Slave version become difficult to classify as:

- physics bugs;
- cache bugs;
- synchronization bugs;
- bootstrap bugs;
- data ownership bugs.

The CPU-independent physics pipeline must be stable first.

---

# Phase 5 - Immutable input snapshot + output buffer

Do not let the Slave mutate arbitrary game-owned objects.

Introduce explicit input and output representations.

Recommended model:

```text
Game state
    |
    v
Physics input snapshot       owned by physics step
    |
    v
Slave executes
    |
    v
Physics output buffer
    |
    v
Master applies transforms/events to game state
```

The first version can use double-buffered world state:

```text
Frame N

Master writes Input A
Slave reads Input A / writes Output A

Master prepares unrelated frame work

Master consumes Output A
```

Later, if necessary:

```text
Input A / Output A
Input B / Output B
```

can support stronger pipelining.

### Rules

- Input is immutable after publication.
- Output is Slave-owned until completion.
- Game logic does not directly mutate in-flight physics state.
- Stable IDs are used to associate output bodies/events with game objects.
- Pointers stored in jobs must refer only to memory explicitly valid for both CPUs.

---

# Phase 6 - Slave physics backend

Add an execution backend selector.

Possible API:

```c
typedef enum sat_physics_backend {
    SAT_PHYSICS_BACKEND_MASTER = 0,
    SAT_PHYSICS_BACKEND_SLAVE,
    SAT_PHYSICS_BACKEND_AUTO
} sat_physics_backend_t;

sat_result_t sat_physics_world_set_backend(
    sat_physics_world_t* world,
    sat_physics_backend_t backend
);
```

Synchronous call:

```c
sat_physics_world_step(&world, 1);
```

should work regardless of backend.

Asynchronous API:

```c
sat_result_t sat_physics_step_async(
    sat_physics_world_t* world,
    uint16_t steps
);

int sat_physics_step_done(
    const sat_physics_world_t* world
);

sat_result_t sat_physics_wait(
    sat_physics_world_t* world
);
```

Semantics:

```text
MASTER backend
    step_async() may execute immediately and mark complete

SLAVE backend
    step_async() snapshots/publishes and returns
    wait() synchronizes and exposes results

AUTO backend
    choose Slave when available and workload is large enough
    otherwise use Master
```

Do not make `AUTO` aggressively offload tiny worlds. A threshold should be empirically tuned.

---

# Phase 7 - Broad-phase and spatial-system integration

Broad phase is a particularly good Slave workload because it is repetitive and can be processed over arrays.

Integrate with the existing spatial/collision modules rather than creating a second independent broad-phase implementation.

Potential stages:

```text
body AABB update
        |
        v
spatial grid insertion
        |
        v
candidate pair generation
        |
        v
pair deduplication
        |
        v
narrow-phase input list
```

Target APIs can remain internal initially.

Important constraints:

- fixed-capacity pair buffers;
- deterministic overflow behavior;
- no hidden allocation;
- stable ordering if ordering affects gameplay determinism;
- explicit statistics for dropped/overflowed pairs.

Potential stats:

```c
typedef struct sat_physics_stats {
    uint16_t body_count;
    uint16_t candidate_pairs;
    uint16_t contacts;
    uint16_t pair_overflows;
    uint32_t master_wait_cycles;
    uint32_t slave_work_cycles;
} sat_physics_stats_t;
```

---

# Phase 8 - Tile and static-world collision batching

The current API can call a user-provided tile callback:

```c
sat_tile_fn tile_fn;
```

A raw callback into arbitrary Master-side game logic is not suitable for Slave execution.

The world API therefore needs a Slave-safe representation for tile/static collision.

Preferred options:

### Option A - Direct tilemap data

```c
typedef struct sat_physics_tilemap {
    const uint8_t* tiles;
    uint16_t width;
    uint16_t height;
    sat_fx16_t tile_size;
} sat_physics_tilemap_t;
```

### Option B - Precomputed collision cells

Games convert custom map formats into a compact LibSaturn collision layer before stepping physics.

### Option C - Master callback backend only

Existing callback-based APIs remain Master-only while the world/Slave path requires structured collision data.

**Recommended first implementation: Option C + A.** Preserve callback compatibility and introduce a structured tilemap specifically for asynchronous world execution.

Do not attempt arbitrary cross-CPU callbacks.

---

# Phase 9 - Contact/event output instead of gameplay callbacks

The Slave must not call gameplay code when a collision occurs.

Instead generate fixed-size events:

```c
typedef enum sat_physics_event_kind {
    SAT_PHYSICS_EVENT_BEGIN_CONTACT,
    SAT_PHYSICS_EVENT_STAY_CONTACT,
    SAT_PHYSICS_EVENT_END_CONTACT,
    SAT_PHYSICS_EVENT_TRIGGER
} sat_physics_event_kind_t;

typedef struct sat_physics_event {
    uint16_t kind;
    uint16_t body_a;
    uint16_t body_b;
    uint16_t flags;
} sat_physics_event_t;
```

Flow:

```text
Slave detects collision
        |
        v
append event to output buffer
        |
        v
complete step
        |
        v
Master consumes events
        |
        v
gameplay callbacks / damage / sounds / scripts
```

This preserves a clean CPU ownership boundary.

---

# Phase 10 - Master/Slave overlap strategy

The performance gain comes from **overlap**, not merely from executing physics on a second CPU.

A recommended frame schedule:

```text
VBlank / frame start
        |
        v
input sampling
        |
        v
apply player commands to physics input
        |
        v
sat_physics_step_async()
        |
        +------------------------------+
        |                              |
        | Master SH-2                  | Slave SH-2
        |                              |
        | gameplay not requiring       | physics world step
        | new transforms               | broad phase
        | animation prep               | collision
        | asset/scene management       | contact generation
        | render list preparation      |
        |                              |
        +---------------+--------------+
                        |
                        v
                sat_physics_wait()
                        |
                        v
                consume transforms
                consume contact events
                final render submission
```

Avoid placing `sat_physics_wait()` immediately after `sat_physics_step_async()`, which would turn the Slave backend into synchronous RPC and erase most benefit.

---

# Phase 11 - Job scheduling policy

Start deliberately small.

## MVP

```text
one Slave worker
one in-flight job
one physics world step per frame
polling completion
```

## Later

Only after profiling:

```text
small fixed queue
priority classes
multiple internal job kinds
job chaining
frame deadlines
```

Do **not** initially add:

- work stealing;
- arbitrary dependency graphs;
- preemption;
- dynamic task allocation;
- recursive task spawning;
- user-provided cross-CPU function pointers.

Those features would add complexity disproportionate to the Saturn's two-SH-2 topology.

---

# Phase 12 - Cache/coherency ownership helpers

The library should centralize all required memory visibility operations.

Possible internal helpers:

```cpp
slave_publish_range(ptr, size);
slave_consume_range(ptr, size);
slave_memory_barrier();
```

The exact implementation depends on the memory model established in Phase 0.

Do not scatter cache-manipulation assembly throughout physics code.

Physics should only express ownership transitions:

```text
Master-owned -> published -> Slave-owned
Slave-owned  -> completed -> Master-owned
```

This also makes the job system reusable for future workloads such as decompression or animation processing.

---

# Phase 13 - Determinism and backend parity tests

Every physics test that can run on the host should remain hardware-independent.

Add backend parity fixtures where possible:

```text
same initial bodies
same parameters
same static world
same fixed number of steps

Master result == Slave result
```

Compare:

- positions;
- velocities;
- body flags;
- contact pairs;
- event counts/order where defined;
- overflow flags;
- fixed-point saturation behavior.

The Slave implementation should execute the same core physics functions rather than carrying a forked solver.

### Determinism rule

Do not allow pair processing order to vary casually between backends if that changes collision resolution results.

If parallel-friendly ordering requires different semantics later, introduce it as an explicit mode, not an accidental backend difference.

---

# Phase 14 - Profiling and offload threshold

Add measurements before enabling `SAT_PHYSICS_BACKEND_AUTO` by default.

Measure at least:

```text
Master-only physics cycles
Slave physics cycles
Master submit cost
Master wait time
bytes shared per step
body count
candidate-pair count
contact count
frame time
```

Benchmark scenes should include:

1. 1 body against tiles;
2. 16 bodies;
3. 64 bodies;
4. 128+ simple bodies where memory allows;
5. many static colliders;
6. dense body-body collision;
7. broad-phase-heavy sparse scene;
8. mixed render + audio load to expose bus contention.

The AUTO backend should use a simple threshold based on measurable workload, for example body count and enabled collision features.

Do not assume Slave execution is faster for tiny jobs.

---

# Phase 15 - Failure handling

The Master must never spin forever waiting for a failed Slave.

Required states:

```text
IDLE
BUSY
DONE
ERROR
TIMEOUT
```

Possible policy:

```c
sat_result_t sat_physics_wait(...)
```

returns an error if the Slave fails to complete within a documented guard interval.

In debug builds, retain diagnostics such as:

- submitted job sequence;
- completed job sequence;
- current job kind;
- last Slave error;
- timeout count.

Do not silently rerun a half-completed physics step on the Master unless the ownership/state model proves that retry is safe.

---

# Phase 16 - Example and visual validation

Add a focused example:

```text
examples/slave_physics/
```

Suggested scene:

- 64-128 bouncing or platforming bodies;
- static/tile collision;
- body-body collision;
- on-screen backend indicator;
- toggle Master/Slave backend at runtime if safe;
- body count display;
- Master wait-time display;
- overflow/error indicators.

The example should make it visually obvious that both backends produce equivalent behavior.

A second benchmark-only example can be added later if needed.

---

# Proposed file layout

```text
include/saturn/slave_sh2.h
include/saturn/jobs.h
include/saturn/physics.h

src/hal/slave_sh2.hpp
src/hal/slave_sh2.cpp

src/core/jobs_api.cpp
src/core/jobs_logic.hpp

src/core/physics_api.cpp
src/core/physics_logic.hpp
src/core/physics_world.cpp
src/core/physics_world.hpp
src/core/physics_slave.cpp

src/slave/entry.cpp
src/slave/job_dispatch.cpp

examples/slave_sh2_smoke/
examples/slave_physics/

tests/test_jobs.cpp
tests/test_physics_world.cpp
tests/test_physics_backend_parity.cpp
```

Exact placement of Slave binary/startup code should follow the existing linker/build architecture rather than forcing this tentative layout.

---

# Proposed public API shape

A possible end state:

```c
/* Slave runtime */
sat_result_t sat_slave_init(void);
void sat_slave_shutdown(void);
int sat_slave_available(void);

/* Generic jobs */
sat_result_t sat_job_submit(
    const sat_job_desc_t* desc,
    sat_job_handle_t* out_handle
);
int sat_job_done(sat_job_handle_t handle);
sat_result_t sat_job_wait(sat_job_handle_t handle);

/* Physics */
sat_result_t sat_physics_world_init(
    sat_physics_world_t* world,
    const sat_physics_world_config_t* config,
    void* storage,
    uint32_t storage_size
);

sat_result_t sat_physics_world_set_backend(
    sat_physics_world_t* world,
    sat_physics_backend_t backend
);

sat_result_t sat_physics_world_step(
    sat_physics_world_t* world,
    uint16_t steps
);

sat_result_t sat_physics_step_async(
    sat_physics_world_t* world,
    uint16_t steps
);

int sat_physics_step_done(
    const sat_physics_world_t* world
);

sat_result_t sat_physics_wait(
    sat_physics_world_t* world
);
```

The final API should prefer LibSaturn's established naming and storage conventions over this exact draft if they differ.

---

# What should stay on the Master SH-2

At least initially:

- player/controller input interpretation;
- gameplay scripts/state machines;
- collision-trigger gameplay consequences;
- audio commands generated by collisions;
- render submission;
- arbitrary user callbacks;
- world mutation that is not represented in the published physics snapshot.

This prevents the Slave physics path from gradually becoming a second game thread with uncontrolled access to engine state.

---

# What can migrate to the Slave later

Once the job system is stable, other coarse workloads can reuse it:

```text
physics world step
spatial-grid rebuild
raycast batch
animation pose batch
software skinning, if ever useful
asset decompression
procedural generation chunks
visibility/culling batches
```

Each workload should first prove that computation saved exceeds synchronization and shared-bus cost.

---

# Non-goals for the first implementation

Do not include in the MVP:

- general-purpose threading API;
- POSIX-like threads;
- arbitrary shared mutable game state;
- mutex-heavy programming model;
- preemptive scheduling;
- dynamic allocator shared between CPUs;
- function-pointer RPC supplied by games;
- per-body cross-CPU calls;
- a new physics solver written specifically for the Slave;
- floating-point physics;
- automatic migration of every physics call to the Slave.

LibSaturn should expose a predictable Saturn-specific execution model rather than pretending the hardware is a modern SMP desktop.

---

# Recommended implementation order

```text
Phase 0  Slave communication spike
    |
Phase 1  Slave HAL/lifecycle
    |
Phase 2  one-job generic worker
    |
Phase 3  physics world abstraction
    |
Phase 4  synchronous Master world backend
    |
Phase 5  snapshot/output ownership model
    |
Phase 6  Slave world backend
    |
Phase 7  spatial/broad-phase integration
    |
Phase 8  structured tile/static collision
    |
Phase 9  contact/event buffers
    |
Phase 10 real Master/Slave overlap
    |
Phase 13 backend parity/determinism
    |
Phase 14 profiling + AUTO threshold
    |
Phase 16 example and documentation
```

Phases 11, 12, and 15 are cross-cutting infrastructure concerns and should be implemented alongside the stages that first require them.

---

# Definition of done

The feature is considered production-ready when all of the following are true:

- Slave SH-2 startup and shutdown are reliable.
- Shared-memory/cache rules are explicitly documented and centralized.
- A generic one-worker LibSaturn job mechanism exists.
- Existing per-body physics API remains compatible.
- A physics world can execute entirely on the Master.
- The same world can execute on the Slave without changing gameplay code.
- `sat_physics_step_async()` permits useful Master work before synchronization.
- Master and Slave backends pass deterministic parity tests.
- Tile/static collision has a Slave-safe structured representation.
- Contact callbacks are replaced by output events across the CPU boundary.
- No hidden heap allocation is required.
- Failure/timeout handling cannot deadlock the game silently.
- Benchmarks show a measurable frame-time benefit for sufficiently large physics workloads.
- Small workloads remain on the Master when offload overhead would be worse.
- A real example demonstrates both correctness and measurable overlap.

---

## Final architectural target

```text
                         Game
                           |
                 sat_physics_world_*
                           |
              +------------+------------+
              |                         |
              v                         v
       Master backend             Slave backend
              |                         |
              |                    sat_job_*
              |                         |
              |                 Slave SH-2 worker
              |                         |
              +------------+------------+
                           |
                           v
                 shared physics logic
                           |
              +------------+------------+
              |                         |
           spatial                   collision
              |                         |
              +------------+------------+
                           |
                       fixed point
```

The important boundary is that **physics remains one subsystem and Slave SH-2 execution remains another**. The physics world may choose the Slave as a backend, but its public semantics should not depend on which SH-2 performed the work.
