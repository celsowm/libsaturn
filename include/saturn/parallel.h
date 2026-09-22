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
    SAT_PARALLEL_TASK_SCENE_GEOMETRY = 2u
};

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
    uint8_t reserved;
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
