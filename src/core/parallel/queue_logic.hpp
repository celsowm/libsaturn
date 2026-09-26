#ifndef SATURN_CORE_PARALLEL_QUEUE_LOGIC_HPP
#define SATURN_CORE_PARALLEL_QUEUE_LOGIC_HPP

#include <stdint.h>

#include "saturn/parallel.h"

namespace saturn::core::parallel::queue_logic {

constexpr uint32_t kInvalidHandle = 0u;

inline uint16_t next_generation(uint16_t generation) {
    const uint16_t next = static_cast<uint16_t>(generation + 1u);
    return next == 0u ? 1u : next;
}

inline sat_parallel_handle_t make_handle(uint16_t index, uint16_t generation) {
    return (static_cast<uint32_t>(generation) << 16u) |
           static_cast<uint32_t>(index + 1u);
}

inline bool decode_handle(sat_parallel_handle_t handle, uint16_t capacity,
                          uint16_t* out_index, uint16_t* out_generation) {
    if (handle == 0u || out_index == nullptr || out_generation == nullptr) return false;
    const uint32_t raw_index = handle & 0xFFFFu;
    const uint16_t generation = static_cast<uint16_t>(handle >> 16u);
    if (raw_index == 0u || raw_index - 1u >= capacity || generation == 0u) return false;
    *out_index = static_cast<uint16_t>(raw_index - 1u);
    *out_generation = generation;
    return true;
}

inline bool handle_matches(const sat_parallel_task_slot_t& slot,
                           sat_parallel_handle_t handle,
                           uint16_t index,
                           uint16_t capacity) {
    uint16_t decoded_index = 0u;
    uint16_t generation = 0u;
    return decode_handle(handle, capacity, &decoded_index, &generation) &&
           decoded_index == index && slot.generation == generation &&
           slot.state != SAT_PARALLEL_FREE && slot.token == handle;
}

inline bool terminal(sat_parallel_task_state_t state) {
    return state == SAT_PARALLEL_COMPLETED || state == SAT_PARALLEL_FAILED ||
           state == SAT_PARALLEL_CANCELLED;
}

/* Accounts a Slave's completion message for the active slot. The Slave
 * publishes COMPLETED/FAILED into the shared slot BEFORE it signals, so the
 * Master finds either RUNNING or that terminal state; both are this task's
 * own completion. (Abort clears the active task first, so a late message
 * never reaches here.) The message's result is authoritative. */
inline bool apply_slave_completion(sat_parallel_task_slot_t& slot, sat_result_t result,
                                   sat_parallel_stats_t& stats) {
    const auto state = static_cast<sat_parallel_task_state_t>(slot.state);
    if (state != SAT_PARALLEL_RUNNING && state != SAT_PARALLEL_COMPLETED &&
        state != SAT_PARALLEL_FAILED) return false;
    slot.result = static_cast<int32_t>(result);
    slot.state = result == SAT_OK ? SAT_PARALLEL_COMPLETED : SAT_PARALLEL_FAILED;
    if (result == SAT_OK) ++stats.completed;
    else ++stats.failed;
    stats.last_task_ticks = slot.task_ticks;
    stats.slave_task_ticks += slot.task_ticks;
    return true;
}

/* ------------------------------------------------------------------ */
/* Scheduling: priorities, dependencies and batches                     */
/* ------------------------------------------------------------------ */

constexpr uint32_t kBatchMax = 4u;           /* tasks the Slave takes in one signal */
constexpr uint8_t kPriorityMax = 3u;         /* 0 lowest .. 3 highest */
constexpr uint8_t kPriorityDefault = 1u;

/* What a queued task is waiting on. `Done` and `None` are satisfied;
 * `Pending` is a dependency that has not finished; `Failed` one that failed,
 * was cancelled or is gone. */
enum class Dep : uint8_t { None = 0, Done = 1, Pending = 2, Failed = 3 };

struct Candidate {
    uint16_t slot;        /* slot index */
    uint8_t priority;
    uint32_t order;       /* submission sequence: FIFO within a priority */
    bool master_only;     /* forced to the Master, or the backend is the Master */
    Dep dep;
    int32_t dep_slot;     /* slot index of the dependency when it is queued, else -1 */
};

inline bool satisfied(Dep dep) {
    return dep == Dep::None || dep == Dep::Done;
}

/* Is `candidate` eligible after the `chosen` slots were picked before it in the
 * same batch? A dependency still queued counts if it is already in the batch:
 * the Slave runs a batch in order, so it finishes first. */
inline bool eligible(const Candidate& candidate, const uint16_t* chosen, uint32_t chosen_count,
                     bool allow_in_batch) {
    if (satisfied(candidate.dep)) return true;
    if (candidate.dep != Dep::Pending || !allow_in_batch || candidate.dep_slot < 0) return false;
    for (uint32_t i = 0u; i < chosen_count; ++i) {
        if (chosen[i] == static_cast<uint16_t>(candidate.dep_slot)) return true;
    }
    return false;
}

/* Higher priority first, then earlier submission. */
inline bool before(const Candidate& a, const Candidate& b) {
    return a.priority != b.priority ? a.priority > b.priority : a.order < b.order;
}

/* Picks up to `max` tasks for one Slave batch, in execution order. Master-only
 * tasks are never chosen. Returns how many; out_slots gets their slot indices. */
inline uint32_t pick_batch(const Candidate* candidates, uint32_t count, uint32_t max,
                           uint16_t* out_slots) {
    bool taken[64] = {};
    uint32_t chosen = 0u;
    if (count > 64u) count = 64u;
    while (chosen < max) {
        int32_t best = -1;
        for (uint32_t i = 0u; i < count; ++i) {
            if (taken[i] || candidates[i].master_only) continue;
            if (!eligible(candidates[i], out_slots, chosen, true)) continue;
            if (best < 0 || before(candidates[i], candidates[best])) best = static_cast<int32_t>(i);
        }
        if (best < 0) break;
        taken[best] = true;
        out_slots[chosen++] = candidates[best].slot;
    }
    return chosen;
}

/* The Master's next task: the best-ranked Master-only candidate whose
 * dependency is finished. Returns its position in `candidates`, or -1. */
inline int32_t pick_master(const Candidate* candidates, uint32_t count) {
    int32_t best = -1;
    for (uint32_t i = 0u; i < count; ++i) {
        if (!candidates[i].master_only || !satisfied(candidates[i].dep)) continue;
        if (best < 0 || before(candidates[i], candidates[best])) best = static_cast<int32_t>(i);
    }
    return best;
}

}  // namespace saturn::core::parallel::queue_logic

#endif
