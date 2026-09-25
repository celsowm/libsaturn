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

}  // namespace saturn::core::parallel::queue_logic

#endif
