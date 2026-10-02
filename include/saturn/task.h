#ifndef SATURN_TASK_H
#define SATURN_TASK_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A small gameplay task scheduler: a fixed set of caller-owned slots, run once per game step in
 * a deterministic order. It is not sat_parallel (which spreads work across the two SH-2s) and no
 * terrain, camera or physics module needs it; a game uses it for things that live a while and
 * want an update each step (a spawner, a timer, a scripted door).
 *
 * Order: ascending priority, and tasks of equal priority run in the order they were created.
 * The order is stable: it does not depend on slot numbers, on how many tasks died, or on the
 * handle values.
 *
 * Handles are generation-checked. A handle goes stale the moment its task is destroyed, even
 * when the slot is reused afterwards, and every call with a stale handle answers
 * SAT_ERR_NOT_FOUND.
 *
 * Safe lifecycle during a run:
 *   - a task may destroy itself; the rest of its update function still runs, but the scheduler
 *     will not call it again;
 *   - a task may destroy a peer; a peer that has not run yet this step is skipped, one that has
 *     already run is just gone;
 *   - a task may replace its own update function or a peer's; the replacement is the function
 *     called from the next time that task is reached (this step for a peer later in the order);
 *   - tasks created during a run are queued and first run in the next step;
 *   - the slot of a task destroyed during a run counts against the capacity until the run ends.
 *
 * The task's data is the caller's: the scheduler stores the pointer and hands it back, and never
 * allocates, copies or frees it. The optional destroy function runs exactly once per task, at the
 * moment the task is destroyed (by sat_task_destroy, by sat_task_scheduler_clear, or by another
 * task), with the data pointer; a destroy function must not run or clear the scheduler. */

typedef struct sat_task_scheduler sat_task_scheduler_t;

typedef struct sat_task_handle {
    uint16_t index;      /* slot number; meaningful only together with `generation` */
    uint16_t generation; /* 0 never names a task, so a zeroed handle is always stale */
} sat_task_handle_t;

typedef void (*sat_task_update_fn)(sat_task_scheduler_t* scheduler, sat_task_handle_t self, void* data);
typedef void (*sat_task_destroy_fn)(void* data);

typedef struct sat_task_desc {
    sat_task_update_fn update;   /* required */
    sat_task_destroy_fn destroy; /* optional */
    void* data;                  /* caller-owned, may be NULL */
    int16_t priority;            /* lower runs first */
    uint16_t reserved;
} sat_task_desc_t;

typedef struct sat_task_slot {
    sat_task_update_fn update;
    sat_task_destroy_fn destroy;
    void* data;
    int16_t priority;
    uint16_t generation;
    uint8_t state;
    uint8_t reserved[3];
} sat_task_slot_t;

typedef struct sat_task_stats {
    uint32_t runs;
    uint32_t tasks_run;
    uint32_t created;
    uint32_t destroyed;
    uint32_t peak_live;
} sat_task_stats_t;

struct sat_task_scheduler {
    sat_task_slot_t* slots;
    uint16_t* order; /* `capacity` entries: the sorted live tasks, then the ones created during a run */
    uint16_t capacity;
    uint16_t sorted;  /* entries of `order` that are sorted */
    uint16_t pending; /* entries queued behind them */
    uint16_t live;    /* tasks that exist (sorted + pending, not counting the dying) */
    uint8_t busy;     /* 1 while a run, a clear or a destroy is in progress */
    uint8_t clearing; /* 1 while sat_task_scheduler_clear is in progress */
    uint8_t reserved[2];
    sat_task_stats_t stats;
};

/* Bytes of slot storage for `capacity` tasks (the caller's `order` array is capacity * 2 bytes). */
uint32_t sat_task_scheduler_slot_bytes(uint32_t capacity);

/* `slots` and `order` hold `capacity` entries each (1..65535) and must outlive the scheduler.
 * SAT_ERR_INVALID_ARG for a missing pointer or a capacity out of range. */
sat_result_t sat_task_scheduler_init(sat_task_scheduler_t* scheduler, sat_task_slot_t* slots, uint16_t* order,
    uint16_t capacity);

/* SAT_ERR_INVALID_ARG for a missing scheduler, description or update function; SAT_ERR_CAPACITY
 * when no slot is free; SAT_ERR_BUSY while sat_task_scheduler_clear is running. `out` may be NULL. */
sat_result_t sat_task_create(sat_task_scheduler_t* scheduler, const sat_task_desc_t* desc, sat_task_handle_t* out);

/* Destroys the task and calls its destroy function. SAT_ERR_NOT_FOUND for a stale handle. */
sat_result_t sat_task_destroy(sat_task_scheduler_t* scheduler, sat_task_handle_t handle);

/* Replaces the update function. SAT_ERR_NOT_FOUND for a stale handle, SAT_ERR_INVALID_ARG for NULL. */
sat_result_t sat_task_set_update(sat_task_scheduler_t* scheduler, sat_task_handle_t handle, sat_task_update_fn update);

int sat_task_is_alive(const sat_task_scheduler_t* scheduler, sat_task_handle_t handle);

/* The task's data pointer. SAT_ERR_NOT_FOUND for a stale handle. */
sat_result_t sat_task_data(const sat_task_scheduler_t* scheduler, sat_task_handle_t handle, void** out);

/* One step: every task that existed when the call began, in order. SAT_ERR_BUSY when called from a
 * task or a destroy function. */
sat_result_t sat_task_scheduler_run(sat_task_scheduler_t* scheduler);

/* Destroys every task, in run order, calling each destroy function; every handle goes stale. A
 * destroy function must not create tasks here (SAT_ERR_BUSY). SAT_ERR_BUSY when called from a task. */
sat_result_t sat_task_scheduler_clear(sat_task_scheduler_t* scheduler);

/* Tasks that exist now (queued ones included). */
uint32_t sat_task_scheduler_count(const sat_task_scheduler_t* scheduler);

sat_task_stats_t sat_task_scheduler_stats(const sat_task_scheduler_t* scheduler);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_TASK_H */
