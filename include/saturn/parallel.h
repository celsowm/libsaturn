#ifndef SATURN_PARALLEL_H
#define SATURN_PARALLEL_H

#include <stddef.h>
#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum sat_parallel_mode {
    SAT_PARALLEL_MASTER = 0,
    SAT_PARALLEL_SLAVE = 1,
    SAT_PARALLEL_AUTO = 2
} sat_parallel_mode_t;

typedef enum sat_parallel_task_state {
    SAT_PARALLEL_FREE = 0,
    SAT_PARALLEL_QUEUED = 1,
    SAT_PARALLEL_RUNNING = 2,
    SAT_PARALLEL_COMPLETED = 3,
    SAT_PARALLEL_FAILED = 4,
    SAT_PARALLEL_CANCELLED = 5
} sat_parallel_task_state_t;

typedef uint16_t sat_parallel_task_type_t;
typedef uint32_t sat_parallel_handle_t;

enum {
    SAT_PARALLEL_TASK_ANIMATION_DECODE = 1u,
    SAT_PARALLEL_TASK_SCENE_GEOMETRY = 2u,
    /* Reserved: the library's own range task behind sat_parallel_for(). */
    SAT_PARALLEL_TASK_RANGE = 3u
};

/* Queue priority: 0..3, higher runs first, equal priorities run in submission
 * order. sat_parallel_submit() uses SAT_PARALLEL_PRIORITY_NORMAL. */
#define SAT_PARALLEL_PRIORITY_LOW ((uint8_t)0)
#define SAT_PARALLEL_PRIORITY_NORMAL ((uint8_t)1)
#define SAT_PARALLEL_PRIORITY_HIGH ((uint8_t)2)
#define SAT_PARALLEL_PRIORITY_URGENT ((uint8_t)3)

typedef sat_result_t (*sat_parallel_process_fn)(
    const void* input,
    uint32_t input_size,
    void* output,
    uint32_t output_capacity,
    uint32_t* output_size
);

/* The slot is caller-owned when supplied in sat_parallel_config_t.  The
 * fields are public only so applications can provision static storage; they
 * must not be changed after a task is submitted. */
typedef struct sat_parallel_task_slot {
    uint16_t generation;
    uint8_t state;
    uint8_t priority;
    uint16_t type;
    uint16_t reserved2;
    uint32_t token;
    uint32_t input_address;
    uint32_t input_size;
    uint32_t output_address;
    uint32_t output_capacity;
    uint32_t output_size;
    uint32_t task_ticks;
    uint8_t force_master;
    uint8_t reserved3[3];
    int32_t result;
    uint32_t depends_on;   /* handle this task waits for, 0 for none */
    uint32_t order;        /* submission sequence (FIFO within a priority) */
} sat_parallel_task_slot_t;

typedef struct sat_parallel_config {
    sat_parallel_mode_t mode;
    sat_parallel_task_slot_t* slots;
    uint16_t slot_capacity;
    uint16_t reserved;
    uint32_t startup_timeout_ticks;
} sat_parallel_config_t;

typedef struct sat_parallel_stats {
    uint32_t submitted;
    uint32_t queued;
    uint32_t completed;
    uint32_t failed;
    uint32_t cancelled;
    uint32_t master_tasks;
    uint32_t slave_tasks;
    uint32_t master_wait_ticks;
    uint32_t last_task_ticks;
    uint32_t submission_ticks;
    uint32_t completion_ticks;
    uint32_t master_task_ticks;
    uint32_t slave_task_ticks;
    uint32_t slave_signals;        /* execute messages sent to the Slave */
    uint32_t batched_tasks;        /* tasks that travelled in a multi-task batch */
    uint32_t dependency_cancels;   /* tasks cancelled because their dependency failed */
} sat_parallel_stats_t;

sat_result_t sat_parallel_init(const sat_parallel_config_t* config);
sat_result_t sat_parallel_shutdown(uint32_t timeout_ticks);
uint8_t sat_parallel_initialized(void);
sat_parallel_mode_t sat_parallel_mode(void);
sat_parallel_mode_t sat_parallel_backend(void);
uint8_t sat_parallel_slave_available(void);

sat_result_t sat_parallel_register_task(
    sat_parallel_task_type_t type, sat_parallel_process_fn process);

/* Cache publication/consumption helpers for task adapters. They operate on
 * the current CPU and are no-ops for non-Work-RAM addresses, which lets
 * immutable ROM descriptors remain valid task inputs. */
sat_result_t sat_parallel_cache_sync_range(const void* address, uint32_t size);
void* sat_parallel_uncached_address(const void* address);

sat_result_t sat_parallel_submit(
    sat_parallel_task_type_t type,
    const void* input,
    uint32_t input_size,
    void* output,
    uint32_t output_capacity,
    sat_parallel_handle_t* out_handle);

/* Submission with a priority and a dependency. `depends_on` (0 for none) is a
 * handle from this runtime that must stay unreleased until this task is
 * finished: the task does not start before it completed, and is CANCELLED with
 * SAT_ERR_NOT_FOUND if it failed, was cancelled or is gone. Ready tasks are
 * taken in priority order, and up to four go to the Slave in one signal (a
 * batch it runs in order, so a dependency queued in the same batch runs first).
 * force_master keeps the task on the Master. */
typedef struct sat_parallel_submit_desc {
    sat_parallel_task_type_t type;
    uint8_t priority;              /* SAT_PARALLEL_PRIORITY_*, 0..3 */
    uint8_t force_master;
    const void* input;
    uint32_t input_size;
    void* output;
    uint32_t output_capacity;
    sat_parallel_handle_t depends_on;
} sat_parallel_submit_desc_t;

sat_result_t sat_parallel_submit_ex(
    const sat_parallel_submit_desc_t* desc, sat_parallel_handle_t* out_handle);

/* Data-parallel loop: runs fn over [begin, end) split between the two CPUs, the
 * Master taking the first half and the Slave the second, and returns when both
 * are done. Ranges smaller than 2 * min_grain, or a runtime without a Slave,
 * run entirely on the Master. Rules for fn: it is called as fn(context, first,
 * last) for a sub-range, must touch only data belonging to that sub-range, and
 * must not call library functions the Slave may not use (drawing, VDP and SCU
 * access). Memory written by the Slave lands in Work RAM (the SH-2 cache is
 * write-through); pass the written span as (output, output_bytes) and the
 * Master drops its stale cache lines for it after the join. Data read by fn that
 * the Master wrote just before the call is safe: the caches are write-through and
 * the Slave's cache is purged first when it is enabled. Returns the Slave task's
 * error (the Master half has still run) if the Slave half failed. */
typedef void (*sat_parallel_range_fn)(void* context, uint32_t begin, uint32_t end);

sat_result_t sat_parallel_for(
    uint32_t begin, uint32_t end, uint32_t min_grain,
    sat_parallel_range_fn fn, void* context,
    void* output, uint32_t output_bytes);

/* Explicit Master dispatch used by subsystem policies whose measured AUTO
 * crossover is not favorable. It still returns a normal terminal handle. */
sat_result_t sat_parallel_submit_master(
    sat_parallel_task_type_t type,
    const void* input,
    uint32_t input_size,
    void* output,
    uint32_t output_capacity,
    sat_parallel_handle_t* out_handle);

/* Services the bounded queue and consumes at most the currently available
 * completion. It never waits for a task. */
sat_result_t sat_parallel_service(void);
uint8_t sat_parallel_done(sat_parallel_handle_t handle);
sat_parallel_task_state_t sat_parallel_state(sat_parallel_handle_t handle);
sat_result_t sat_parallel_result(sat_parallel_handle_t handle, uint32_t* out_size);
sat_result_t sat_parallel_wait(sat_parallel_handle_t handle, uint32_t timeout_ticks);
/* Safely aborts one running Slave task. The Slave is stopped first, so the
 * task's input/output storage is not reclaimed while it can still be writing.
 * Queued tasks are cancelled as part of this runtime-wide recovery operation. */
sat_result_t sat_parallel_abort(sat_parallel_handle_t handle, uint32_t timeout_ticks);
sat_result_t sat_parallel_cancel(sat_parallel_handle_t handle);
sat_result_t sat_parallel_release(sat_parallel_handle_t handle);
sat_result_t sat_parallel_stats(sat_parallel_stats_t* out_stats);

#ifdef __cplusplus
}
#endif

#endif
