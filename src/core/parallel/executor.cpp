#include "src/core/parallel/executor.hpp"

#include <stddef.h>

#include "saturn/dual_sh2.h"
#include "src/core/parallel/queue_logic.hpp"
#include "src/hal/dual_sh2/memory.hpp"
#include "src/hal/sh2/cache.hpp"
#include "src/hal/sh2/cpu.hpp"
#include "src/hal/sh2/frt.hpp"

namespace saturn::core::parallel::executor {

namespace {

constexpr uint32_t kCommandExecute = 0x50524C31u; /* PRL1 */
constexpr uint32_t kCommandComplete = 0x50524C32u; /* PRL2 */
constexpr uint16_t kMaxTasks = 16u;
constexpr uint16_t kDefaultSlots = 8u;
constexpr uint32_t kDefaultStartupTimeout = 60000u;

struct Registration {
    sat_parallel_task_type_t type;
    sat_parallel_process_fn process;
};

struct Runtime {
    uint8_t initialized;
    uint8_t slave_started;
    sat_parallel_mode_t mode;
    sat_parallel_mode_t backend;
    sat_parallel_task_slot_t* slots;
    uint16_t capacity;
    uint16_t active_index;
    uint8_t active;
    sat_result_t active_error;
    uint16_t completion_start_tick;
    Registration registrations[kMaxTasks];
    uint16_t registration_count;
    sat_parallel_stats_t stats;
};

static sat_parallel_task_slot_t g_default_slots[kDefaultSlots];
static Runtime g_runtime = {};

uint32_t elapsed_ticks(uint16_t start) {
    return static_cast<uint16_t>(saturn::hal::sh2::frt::counter() - start);
}

bool range_ok(uint32_t address, uint32_t size) {
    return size != 0u && saturn::hal::sh2::cache::is_supported_work_ram(address, size);
}

uint32_t physical_address(const void* pointer) {
    const uintptr_t raw = reinterpret_cast<uintptr_t>(pointer);
    if (raw > 0xFFFFFFFFu) return 0u;
    const uint32_t address = static_cast<uint32_t>(raw);
    const uint32_t physical = saturn::hal::dual_sh2::memory::cached_address(address);
    return physical != 0u ? physical : address;
}

void publish_range(uint32_t address, uint32_t size) {
    if (size == 0u) return;
    const uint32_t end = address + size;
    if (end < address) return;
    for (uint32_t line = address & ~(saturn::hal::sh2::cache::kLineBytes - 1u);
         line < end; line += saturn::hal::sh2::cache::kLineBytes) {
        (void)saturn::hal::sh2::cache::invalidate_line(line);
    }
    saturn::hal::sh2::compiler_barrier();
}

void consume_range(uint32_t address, uint32_t size) {
    publish_range(address, size);
}

Registration* registration_for(sat_parallel_task_type_t type) {
    for (uint16_t i = 0u; i < g_runtime.registration_count; ++i) {
        if (g_runtime.registrations[i].type == type) return &g_runtime.registrations[i];
    }
    return nullptr;
}

sat_parallel_task_slot_t* slot_for(sat_parallel_handle_t handle,
                                   uint16_t* out_index = nullptr) {
    uint16_t index = 0u;
    uint16_t generation = 0u;
    if (!queue_logic::decode_handle(handle, g_runtime.capacity, &index, &generation)) {
        return nullptr;
    }
    sat_parallel_task_slot_t* slot = &g_runtime.slots[index];
    if (slot->generation != generation || slot->state == SAT_PARALLEL_FREE ||
        slot->token != handle) return nullptr;
    if (out_index != nullptr) *out_index = index;
    return slot;
}

void mark_failed(sat_parallel_task_slot_t& slot, sat_result_t result) {
    if (queue_logic::terminal(static_cast<sat_parallel_task_state_t>(slot.state))) {
        return;
    }
    slot.result = static_cast<int32_t>(result);
    slot.state = SAT_PARALLEL_FAILED;
    ++g_runtime.stats.failed;
}

sat_result_t finish_slave_message(const sat_dual_sh2_message_t& message) {
    if (g_runtime.active == 0u) return SAT_ERR_BUSY;
    if (message.command != kCommandComplete) {
        g_runtime.active_error = SAT_ERR_VERIFY_FAILED;
        return g_runtime.active_error;
    }
    if (message.argument0 != g_runtime.slots[g_runtime.active_index].token) {
        g_runtime.active_error = SAT_ERR_VERIFY_FAILED;
        return g_runtime.active_error;
    }
    sat_parallel_task_slot_t& slot = g_runtime.slots[g_runtime.active_index];
    /* Geometry jobs publish completion metadata in their input descriptor;
     * invalidate that span as well as the explicit output buffer. */
    consume_range(slot.input_address, slot.input_size);
    consume_range(slot.output_address, slot.output_capacity);
    consume_range(physical_address(&slot), sizeof(slot));
    if (slot.state == SAT_PARALLEL_RUNNING) {
        slot.result = static_cast<int32_t>(static_cast<sat_result_t>(message.argument1));
        slot.state = slot.result == SAT_OK ? SAT_PARALLEL_COMPLETED : SAT_PARALLEL_FAILED;
        if (slot.state == SAT_PARALLEL_COMPLETED) ++g_runtime.stats.completed;
        else ++g_runtime.stats.failed;
        g_runtime.stats.last_task_ticks = slot.task_ticks;
        g_runtime.stats.slave_task_ticks += slot.task_ticks;
    }
    g_runtime.stats.completion_ticks += elapsed_ticks(g_runtime.completion_start_tick);
    g_runtime.active = 0u;
    g_runtime.active_error = SAT_OK;
    return static_cast<sat_result_t>(slot.result);
}

sat_result_t execute_master(sat_parallel_task_slot_t& slot) {
    Registration* registration = registration_for(slot.type);
    if (registration == nullptr || registration->process == nullptr) {
        mark_failed(slot, SAT_ERR_UNSUPPORTED);
        return SAT_ERR_UNSUPPORTED;
    }
    if (g_runtime.stats.queued != 0u) --g_runtime.stats.queued;
    slot.state = SAT_PARALLEL_RUNNING;
    const uint16_t task_start = saturn::hal::sh2::frt::counter();
    uint32_t output_size = 0u;
    const sat_result_t result = registration->process(
        reinterpret_cast<const void*>(static_cast<uintptr_t>(slot.input_address)),
        slot.input_size,
        reinterpret_cast<void*>(static_cast<uintptr_t>(slot.output_address)),
        slot.output_capacity,
        &output_size);
    slot.output_size = output_size;
    slot.task_ticks = elapsed_ticks(task_start);
    slot.result = static_cast<int32_t>(result);
    slot.state = result == SAT_OK ? SAT_PARALLEL_COMPLETED : SAT_PARALLEL_FAILED;
    if (result == SAT_OK) {
        ++g_runtime.stats.completed;
        ++g_runtime.stats.master_tasks;
        g_runtime.stats.last_task_ticks = slot.task_ticks;
        g_runtime.stats.master_task_ticks += slot.task_ticks;
    } else {
        ++g_runtime.stats.failed;
    }
    return result;
}

sat_result_t dispatch_one() {
    if (g_runtime.active != 0u) return SAT_OK;
    for (uint16_t i = 0u; i < g_runtime.capacity; ++i) {
        sat_parallel_task_slot_t& slot = g_runtime.slots[i];
        if (slot.state != SAT_PARALLEL_QUEUED) continue;
        if (slot.force_master != 0u || g_runtime.backend == SAT_PARALLEL_MASTER) {
            return execute_master(slot);
        }
        if (g_runtime.slave_started == 0u || sat_dual_sh2_available() == 0u) {
            if (g_runtime.mode == SAT_PARALLEL_AUTO) {
                return execute_master(slot);
            }
            mark_failed(slot, SAT_ERR_NOT_CONNECTED);
            if (g_runtime.stats.queued != 0u) --g_runtime.stats.queued;
            return SAT_ERR_NOT_CONNECTED;
        }
        slot.state = SAT_PARALLEL_RUNNING;
        if (g_runtime.stats.queued != 0u) --g_runtime.stats.queued;
        publish_range(slot.input_address, slot.input_size);
        publish_range(physical_address(&slot), sizeof(slot));
        const uint32_t slot_address = physical_address(&slot);
        const sat_result_t sent = sat_dual_sh2_send(
            kCommandExecute,
            saturn::hal::dual_sh2::memory::uncached_address(slot_address),
            slot.token);
        if (sent == SAT_ERR_BUSY) {
            slot.state = SAT_PARALLEL_QUEUED;
            ++g_runtime.stats.queued;
            return SAT_OK;
        }
        if (sent != SAT_OK) {
            mark_failed(slot, sent);
            return sent;
        }
        g_runtime.active = 1u;
        g_runtime.active_index = i;
        g_runtime.completion_start_tick = saturn::hal::sh2::frt::counter();
        ++g_runtime.stats.slave_tasks;
        return SAT_OK;
    }
    return SAT_OK;
}

}  // namespace

sat_result_t register_task(sat_parallel_task_type_t type,
                           sat_parallel_process_fn process) {
    if (type == 0u || process == nullptr) return SAT_ERR_INVALID_ARG;
    Registration* existing = registration_for(type);
    if (existing != nullptr) {
        existing->process = process;
        return SAT_OK;
    }
    if (g_runtime.registration_count >= kMaxTasks) return SAT_ERR_CAPACITY;
    g_runtime.registrations[g_runtime.registration_count++] = {type, process};
    return SAT_OK;
}

sat_result_t cache_sync_range(const void* address, uint32_t size) {
    if (address == nullptr || size == 0u) return SAT_ERR_INVALID_ARG;
    publish_range(physical_address(address), size);
    return SAT_OK;
}

void* uncached_address(const void* address) {
    if (address == nullptr) return nullptr;
    const uint32_t physical = physical_address(address);
    if (physical == 0u) return const_cast<void*>(address);
    const uint32_t uncached =
        saturn::hal::dual_sh2::memory::uncached_address(physical);
    return reinterpret_cast<void*>(static_cast<uintptr_t>(
        uncached != 0u ? uncached : physical));
}

sat_result_t init(const sat_parallel_config_t* config) {
    if (g_runtime.initialized != 0u) return SAT_ERR_BUSY;
    sat_parallel_config_t defaults = {SAT_PARALLEL_AUTO, nullptr, 0u, 0u,
                                      kDefaultStartupTimeout};
    if (config == nullptr) config = &defaults;
    if (config->mode < SAT_PARALLEL_MASTER || config->mode > SAT_PARALLEL_AUTO) {
        return SAT_ERR_INVALID_ARG;
    }
    if (config->slot_capacity == 0u) {
        g_runtime.slots = g_default_slots;
        g_runtime.capacity = kDefaultSlots;
    } else {
        if (config->slots == nullptr || config->slot_capacity > 0xFFu) {
            return SAT_ERR_INVALID_ARG;
        }
        g_runtime.slots = config->slots;
        g_runtime.capacity = config->slot_capacity;
    }
    for (uint16_t i = 0u; i < g_runtime.capacity; ++i) {
        g_runtime.slots[i] = {};
        g_runtime.slots[i].generation = 1u;
    }
    g_runtime.mode = config->mode;
    g_runtime.backend = SAT_PARALLEL_MASTER;
    g_runtime.active = 0u;
    g_runtime.active_error = SAT_OK;
    g_runtime.stats = {};
    g_runtime.initialized = 1u;
    if (config->mode == SAT_PARALLEL_MASTER) return SAT_OK;
    if (sat_dual_sh2_state() != SAT_DUAL_SH2_OFFLINE) {
        g_runtime.initialized = 0u;
        return SAT_ERR_BUSY;
    }
    const uint32_t timeout = config->startup_timeout_ticks != 0u
        ? config->startup_timeout_ticks : kDefaultStartupTimeout;
    sat_result_t result = sat_dual_sh2_configure_slave(&slave_entry, nullptr);
    if (result == SAT_OK) result = sat_dual_sh2_start(timeout);
    if (result != SAT_OK) {
        g_runtime.slave_started = 0u;
        if (config->mode == SAT_PARALLEL_SLAVE) {
            g_runtime.initialized = 0u;
            return result;
        }
        g_runtime.backend = SAT_PARALLEL_MASTER;
        return SAT_OK;
    }
    g_runtime.slave_started = 1u;
    g_runtime.backend = SAT_PARALLEL_SLAVE;
    return SAT_OK;
}

sat_result_t shutdown(uint32_t timeout_ticks) {
    if (g_runtime.initialized == 0u) return SAT_OK;
    if (g_runtime.active != 0u) return SAT_ERR_BUSY;
    sat_result_t result = SAT_OK;
    if (g_runtime.slave_started != 0u) {
        result = sat_dual_sh2_stop(timeout_ticks);
        if (result != SAT_OK) return result;
    }
    for (uint16_t i = 0u; i < g_runtime.capacity; ++i) {
        g_runtime.slots[i].state = SAT_PARALLEL_FREE;
        g_runtime.slots[i].token = 0u;
    }
    g_runtime.slave_started = 0u;
    g_runtime.initialized = 0u;
    g_runtime.registration_count = 0u;
    return SAT_OK;
}

sat_result_t service() {
    if (g_runtime.initialized == 0u) return SAT_ERR_NOT_INITIALIZED;
    if (g_runtime.active_error != SAT_OK) return g_runtime.active_error;
    if (g_runtime.active != 0u) {
        sat_dual_sh2_message_t message = {};
        const sat_result_t received = sat_dual_sh2_receive(&message);
        if (received == SAT_OK) return finish_slave_message(message);
        else if (received != SAT_ERR_NOT_FOUND) {
            /* A mailbox error is not proof that the worker stopped. Keep the
             * slot RUNNING and block dispatch/release until the caller either
             * receives the completion or uses sat_parallel_abort(). */
            g_runtime.active_error = received;
            return received;
        }
    }
    return dispatch_one();
}

uint8_t initialized() { return g_runtime.initialized; }

sat_parallel_mode_t mode() { return g_runtime.mode; }

sat_parallel_mode_t backend() { return g_runtime.backend; }

uint8_t slave_available() {
    return g_runtime.slave_started != 0u && sat_dual_sh2_available() != 0u ? 1u : 0u;
}

sat_result_t submit_impl(sat_parallel_task_type_t type, const void* input,
                         uint32_t input_size, void* output,
                         uint32_t output_capacity,
                         sat_parallel_handle_t* out_handle,
                         uint8_t force_master) {
    if (g_runtime.initialized == 0u || out_handle == nullptr) return SAT_ERR_NOT_INITIALIZED;
    if (type == 0u || registration_for(type) == nullptr) return SAT_ERR_UNSUPPORTED;
    if ((input == nullptr && input_size != 0u) ||
        (output == nullptr && output_capacity != 0u)) return SAT_ERR_INVALID_ARG;
    const uint32_t input_address = input_size != 0u ? physical_address(input) : 0u;
    const uint32_t output_address = output_capacity != 0u ? physical_address(output) : 0u;
    if (input_size != 0u && !range_ok(input_address, input_size)) return SAT_ERR_INVALID_ARG;
    if (output_capacity != 0u && !range_ok(output_address, output_capacity)) {
        return SAT_ERR_INVALID_ARG;
    }
    for (uint16_t i = 0u; i < g_runtime.capacity; ++i) {
        sat_parallel_task_slot_t& slot = g_runtime.slots[i];
        if (slot.state != SAT_PARALLEL_FREE) continue;
        if (slot.generation == 0u) slot.generation = 1u;
        slot.token = queue_logic::make_handle(i, slot.generation);
        slot.type = type;
        slot.input_address = input_address;
        slot.input_size = input_size;
        slot.output_address = output_address;
        slot.output_capacity = output_capacity;
        slot.output_size = 0u;
        slot.task_ticks = 0u;
        slot.force_master = force_master;
        slot.result = SAT_ERR_BUSY;
        slot.state = SAT_PARALLEL_QUEUED;
        *out_handle = slot.token;
        ++g_runtime.stats.submitted;
        ++g_runtime.stats.queued;
        const uint16_t submission_start = saturn::hal::sh2::frt::counter();
        (void)service();
        g_runtime.stats.submission_ticks += elapsed_ticks(submission_start);
        return SAT_OK;
    }
    return SAT_ERR_CAPACITY;
}

sat_result_t submit(sat_parallel_task_type_t type, const void* input,
                    uint32_t input_size, void* output, uint32_t output_capacity,
                    sat_parallel_handle_t* out_handle) {
    return submit_impl(type, input, input_size, output, output_capacity,
                       out_handle, 0u);
}

sat_result_t submit_master(sat_parallel_task_type_t type, const void* input,
                           uint32_t input_size, void* output,
                           uint32_t output_capacity,
                           sat_parallel_handle_t* out_handle) {
    return submit_impl(type, input, input_size, output, output_capacity,
                       out_handle, 1u);
}

sat_parallel_task_state_t state(sat_parallel_handle_t handle) {
    if (g_runtime.initialized != 0u) (void)service();
    sat_parallel_task_slot_t* slot = slot_for(handle);
    return slot == nullptr ? SAT_PARALLEL_FREE
                           : static_cast<sat_parallel_task_state_t>(slot->state);
}

uint8_t done(sat_parallel_handle_t handle) {
    return queue_logic::terminal(state(handle)) ? 1u : 0u;
}

sat_result_t result(sat_parallel_handle_t handle, uint32_t* out_size) {
    sat_parallel_task_slot_t* slot = slot_for(handle);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    const sat_parallel_task_state_t task_state =
        static_cast<sat_parallel_task_state_t>(slot->state);
    if (!queue_logic::terminal(task_state)) return SAT_ERR_BUSY;
    if (out_size != nullptr) *out_size = slot->output_size;
    return static_cast<sat_result_t>(slot->result);
}

sat_result_t wait(sat_parallel_handle_t handle, uint32_t timeout_ticks) {
    if (slot_for(handle) == nullptr) return SAT_ERR_INVALID_ARG;
    if (timeout_ticks == 0u) return SAT_ERR_TIMEOUT;
    for (;;) {
        SAT_TRY(service());
        if (g_runtime.active_error != SAT_OK) return g_runtime.active_error;
        if (done(handle) != 0u) return result(handle, nullptr);
        if (g_runtime.active != 0u) {
            const uint16_t wait_start = saturn::hal::sh2::frt::counter();
            sat_dual_sh2_message_t message = {};
            const sat_result_t waited = sat_dual_sh2_wait_response(&message, timeout_ticks);
            g_runtime.stats.master_wait_ticks += elapsed_ticks(wait_start);
            if (waited == SAT_ERR_TIMEOUT) return waited;
            if (waited != SAT_OK) {
                g_runtime.active_error = waited;
                return waited;
            }
            const sat_result_t completed = finish_slave_message(message);
            if (completed != SAT_OK && g_runtime.active_error != SAT_OK) {
                return g_runtime.active_error;
            }
        } else {
            /* Master-only work completes in service(); a queued task that
             * cannot be dispatched is an explicit backend failure. */
            return SAT_ERR_BUSY;
        }
    }
}

sat_result_t abort_task(sat_parallel_handle_t handle, uint32_t timeout_ticks) {
    uint16_t index = 0u;
    sat_parallel_task_slot_t* slot = slot_for(handle, &index);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    if (g_runtime.active == 0u || index != g_runtime.active_index ||
        slot->state != SAT_PARALLEL_RUNNING) return SAT_ERR_BUSY;
    const sat_result_t stopped = sat_dual_sh2_stop(timeout_ticks);
    if (stopped != SAT_OK) return stopped;

    /* stop() only returns after the Slave callback has returned and no longer
     * touches shared Work RAM. It is now safe to invalidate/reclaim storage. */
    consume_range(slot->input_address, slot->input_size);
    consume_range(slot->output_address, slot->output_capacity);
    consume_range(physical_address(slot), sizeof(*slot));
    mark_failed(*slot, SAT_ERR_TIMEOUT);
    g_runtime.active = 0u;
    g_runtime.active_error = SAT_OK;
    g_runtime.slave_started = 0u;
    g_runtime.backend = SAT_PARALLEL_MASTER;
    for (uint16_t i = 0u; i < g_runtime.capacity; ++i) {
        sat_parallel_task_slot_t& queued = g_runtime.slots[i];
        if (queued.state != SAT_PARALLEL_QUEUED) continue;
        queued.state = SAT_PARALLEL_CANCELLED;
        queued.result = SAT_ERR_BUSY;
        if (g_runtime.stats.queued != 0u) --g_runtime.stats.queued;
        ++g_runtime.stats.cancelled;
    }
    return SAT_OK;
}

sat_result_t cancel(sat_parallel_handle_t handle) {
    sat_parallel_task_slot_t* slot = slot_for(handle);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    if (slot->state != SAT_PARALLEL_QUEUED) return SAT_ERR_BUSY;
    slot->state = SAT_PARALLEL_CANCELLED;
    slot->result = SAT_ERR_BUSY;
    ++g_runtime.stats.cancelled;
    if (g_runtime.stats.queued != 0u) --g_runtime.stats.queued;
    return SAT_OK;
}

sat_result_t release(sat_parallel_handle_t handle) {
    uint16_t index = 0u;
    sat_parallel_task_slot_t* slot = slot_for(handle, &index);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    if (!queue_logic::terminal(static_cast<sat_parallel_task_state_t>(slot->state))) {
        return SAT_ERR_BUSY;
    }
    slot->state = SAT_PARALLEL_FREE;
    slot->token = 0u;
    slot->generation = queue_logic::next_generation(slot->generation);
    (void)index;
    return SAT_OK;
}

sat_result_t stats(sat_parallel_stats_t* out_stats) {
    if (out_stats == nullptr) return SAT_ERR_INVALID_ARG;
    *out_stats = g_runtime.stats;
    return SAT_OK;
}

void slave_entry(void*) {
    for (;;) {
        sat_dual_sh2_slave_heartbeat();
        if (sat_dual_sh2_slave_stop_requested() != 0u) return;
        sat_dual_sh2_message_t message = {};
        if (sat_dual_sh2_slave_receive(&message) != SAT_OK) continue;
        if (message.command != kCommandExecute) continue;
        const uint32_t physical_slot =
            saturn::hal::dual_sh2::memory::cached_address(message.argument0);
        if (!saturn::hal::sh2::cache::is_supported_work_ram(
                physical_slot, sizeof(sat_parallel_task_slot_t))) continue;
        volatile sat_parallel_task_slot_t* const shared_slot =
            reinterpret_cast<volatile sat_parallel_task_slot_t*>(
                saturn::hal::dual_sh2::memory::uncached_address(physical_slot));
        if (shared_slot->token != message.argument1 ||
            shared_slot->state != SAT_PARALLEL_RUNNING) continue;
        Registration* registration = registration_for(shared_slot->type);
        sat_result_t result = SAT_ERR_UNSUPPORTED;
        uint32_t output_size = 0u;
        const uint16_t task_start = saturn::hal::sh2::frt::counter();
        if (registration != nullptr && registration->process != nullptr) {
            const uint32_t input = shared_slot->input_address;
            const uint32_t output = shared_slot->output_address;
            result = registration->process(
                reinterpret_cast<const void*>(static_cast<uintptr_t>(
                    saturn::hal::dual_sh2::memory::uncached_address(input))),
                shared_slot->input_size,
                reinterpret_cast<void*>(static_cast<uintptr_t>(
                    saturn::hal::dual_sh2::memory::uncached_address(output))),
                shared_slot->output_capacity,
                &output_size);
        }
        shared_slot->task_ticks = static_cast<uint16_t>(
            saturn::hal::sh2::frt::counter() - task_start);
        shared_slot->output_size = output_size;
        shared_slot->result = static_cast<int32_t>(result);
        shared_slot->state = result == SAT_OK ? SAT_PARALLEL_COMPLETED : SAT_PARALLEL_FAILED;
        saturn::hal::sh2::compiler_barrier();
        while (sat_dual_sh2_slave_send(
                   kCommandComplete, message.argument1,
                   static_cast<uint32_t>(result)) == SAT_ERR_BUSY) {
            sat_dual_sh2_slave_heartbeat();
            if (sat_dual_sh2_slave_stop_requested() != 0u) return;
        }
        (void)sat_dual_sh2_slave_signal_master();
    }
}

}  // namespace saturn::core::parallel::executor
