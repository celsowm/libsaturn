#include "src/core/parallel/executor.hpp"

#include <stddef.h>

#include "saturn/dual_sh2.h"
#include "src/core/parallel/queue_logic.hpp"
#include "src/core/parallel/test_faults.h"
#include "src/core/startup/memory_layout.hpp"
#include "src/hal/dual_sh2/memory.hpp"
#include "src/hal/sh2/cache.hpp"
#include "src/hal/sh2/cpu.hpp"
#include "src/hal/sh2/frt.hpp"

#ifndef SAT_PARALLEL_TEST_FAULT
#define SAT_PARALLEL_TEST_FAULT 0
#endif
#ifndef SAT_PROFILE_METRICS
#define SAT_PROFILE_METRICS 0
#endif

namespace saturn::core::parallel::executor {

namespace {

constexpr uint32_t kCommandExecute = 0x50524C31u; /* PRL1 */
constexpr uint32_t kCommandComplete = 0x50524C32u; /* PRL2 */
constexpr uint32_t kCommandBatch = 0x50524C33u; /* PRL3 */
constexpr uint32_t kCommandBatchComplete = 0x50524C34u; /* PRL4 */
constexpr uint32_t kBatchMax = queue_logic::kBatchMax;
constexpr uint32_t kMaxCandidates = 64u;
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
    uint8_t active;                       /* tasks in flight on the Slave */
    uint16_t active_index[kBatchMax];
    uint32_t active_token;                /* batch sequence (a single task's own token when alone) */
    uint32_t next_order;
    uint32_t batch_sequence;
    uint32_t progress;                    /* bumped whenever the queue moved */
    sat_result_t active_error;
    uint16_t completion_start_tick;
    Registration registrations[kMaxTasks];
    uint16_t registration_count;
    sat_parallel_stats_t stats;
};

/* What the Slave drains in one signal: the slots of a batch, as uncached
 * addresses, and their tokens. Written by the Master before the message. */
struct BatchBlock {
    volatile uint32_t count;
    volatile uint32_t slot_address[kBatchMax];
    volatile uint32_t token[kBatchMax];
};

static sat_parallel_task_slot_t g_default_slots[kDefaultSlots];
alignas(16) static BatchBlock g_batch;
static Runtime g_runtime = {};
#if SAT_PROFILE_METRICS || SAT_PARALLEL_TEST_FAULT != 0
static volatile uint32_t g_parallel_test_fault_fired = 0u;
#endif

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
    if (saturn::hal::sh2::cache::invalidate_range(address, size)) return;
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

/* Takes back what the Slave wrote for one task: drop the Master cache lines
 * for its input, output and slot before reading any of them. */
void consume_slot(sat_parallel_task_slot_t& slot) {
    /* Geometry jobs publish completion metadata in their input descriptor;
     * invalidate that span as well as the explicit output buffer. */
    consume_range(slot.input_address, slot.input_size);
    consume_range(slot.output_address, slot.output_capacity);
    consume_range(physical_address(&slot), sizeof(slot));
}

void finish_active() {
    g_runtime.stats.completion_ticks += elapsed_ticks(g_runtime.completion_start_tick);
    g_runtime.active = 0u;
    g_runtime.active_token = 0u;
    g_runtime.active_error = SAT_OK;
    ++g_runtime.progress;
}

sat_result_t finish_slave_message(const sat_dual_sh2_message_t& message) {
    if (g_runtime.active == 0u) return SAT_ERR_BUSY;
    if (message.command == kCommandBatchComplete) {
        if (g_runtime.active < 2u || message.argument0 != g_runtime.active_token) {
            g_runtime.active_error = SAT_ERR_VERIFY_FAILED;
            return g_runtime.active_error;
        }
        for (uint8_t i = 0u; i < g_runtime.active; ++i) {
            sat_parallel_task_slot_t& slot = g_runtime.slots[g_runtime.active_index[i]];
            consume_slot(slot);
            /* Each task has its own result in its slot; a task the Slave never
             * got to still reads RUNNING/BUSY and fails with that. */
            (void)queue_logic::apply_slave_completion(
                slot, static_cast<sat_result_t>(slot.result), g_runtime.stats);
        }
        finish_active();
        return SAT_OK;
    }
    if (message.command != kCommandComplete || g_runtime.active != 1u) {
        g_runtime.active_error = SAT_ERR_VERIFY_FAILED;
        return g_runtime.active_error;
    }
    if (message.argument0 != g_runtime.slots[g_runtime.active_index[0]].token) {
        g_runtime.active_error = SAT_ERR_VERIFY_FAILED;
        return g_runtime.active_error;
    }
    sat_parallel_task_slot_t& slot = g_runtime.slots[g_runtime.active_index[0]];
    consume_slot(slot);
    (void)queue_logic::apply_slave_completion(
        slot, static_cast<sat_result_t>(message.argument1), g_runtime.stats);
    finish_active();
    return static_cast<sat_result_t>(slot.result);
}

sat_result_t execute_master(sat_parallel_task_slot_t& slot) {
    Registration* registration = registration_for(slot.type);
    if (registration == nullptr || registration->process == nullptr) {
        mark_failed(slot, SAT_ERR_UNSUPPORTED);
        return SAT_ERR_UNSUPPORTED;
    }
    if (g_runtime.stats.queued != 0u) --g_runtime.stats.queued;
    ++g_runtime.progress;
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

queue_logic::Dep dependency_state(const sat_parallel_task_slot_t& task, int32_t* dep_slot) {
    using queue_logic::Dep;
    *dep_slot = -1;
    if (task.depends_on == 0u) return Dep::None;
    uint16_t index = 0u;
    const sat_parallel_task_slot_t* dep = slot_for(task.depends_on, &index);
    if (dep == nullptr) return Dep::Failed;
    switch (static_cast<sat_parallel_task_state_t>(dep->state)) {
        case SAT_PARALLEL_COMPLETED: return Dep::Done;
        case SAT_PARALLEL_FAILED:
        case SAT_PARALLEL_CANCELLED: return Dep::Failed;
        case SAT_PARALLEL_QUEUED:
            *dep_slot = static_cast<int32_t>(index);
            return Dep::Pending;
        default: return Dep::Pending;   /* running on the Slave */
    }
}

/* A queued task whose dependency failed, was cancelled or is gone can never
 * run: cancel it (and, in turn, whatever waited on it). */
void cancel_failed_dependents() {
    bool changed = true;
    while (changed) {
        changed = false;
        for (uint16_t i = 0u; i < g_runtime.capacity; ++i) {
            sat_parallel_task_slot_t& slot = g_runtime.slots[i];
            int32_t dep_slot = -1;
            if (slot.state != SAT_PARALLEL_QUEUED ||
                dependency_state(slot, &dep_slot) != queue_logic::Dep::Failed) continue;
            slot.state = SAT_PARALLEL_CANCELLED;
            slot.result = SAT_ERR_NOT_FOUND;
            if (g_runtime.stats.queued != 0u) --g_runtime.stats.queued;
            ++g_runtime.stats.cancelled;
            ++g_runtime.stats.dependency_cancels;
            ++g_runtime.progress;
            changed = true;
        }
    }
}

/* The queued tasks, ranked in the pure scheduler terms. A task that needs the
 * Slave in SLAVE mode when none is running fails here, as it always has. */
uint32_t gather_candidates(queue_logic::Candidate* out, uint32_t max) {
    uint32_t count = 0u;
    for (uint16_t i = 0u; i < g_runtime.capacity && count < max; ++i) {
        sat_parallel_task_slot_t& slot = g_runtime.slots[i];
        if (slot.state != SAT_PARALLEL_QUEUED) continue;
        bool master_only = slot.force_master != 0u || g_runtime.backend == SAT_PARALLEL_MASTER;
        if (!master_only &&
            (g_runtime.slave_started == 0u || sat_dual_sh2_available() == 0u)) {
            if (g_runtime.mode == SAT_PARALLEL_AUTO) {
                master_only = true;
            } else {
                mark_failed(slot, SAT_ERR_NOT_CONNECTED);
                if (g_runtime.stats.queued != 0u) --g_runtime.stats.queued;
                ++g_runtime.progress;
                continue;
            }
        }
        queue_logic::Candidate& c = out[count++];
        c.slot = i;
        c.priority = slot.priority > queue_logic::kPriorityMax
            ? queue_logic::kPriorityMax : slot.priority;
        c.order = slot.order;
        c.master_only = master_only;
        c.dep = dependency_state(slot, &c.dep_slot);
    }
    return count;
}

/* Hands the chosen tasks to the Slave in one signal: a lone task goes as the
 * plain execute message, several as a batch the Slave drains in order. */
sat_result_t dispatch_batch(const uint16_t* chosen, uint32_t count) {
    for (uint32_t i = 0u; i < count; ++i) {
        sat_parallel_task_slot_t& slot = g_runtime.slots[chosen[i]];
        slot.state = SAT_PARALLEL_RUNNING;
        if (g_runtime.stats.queued != 0u) --g_runtime.stats.queued;
        publish_range(slot.input_address, slot.input_size);
        publish_range(physical_address(&slot), sizeof(slot));
    }
    sat_result_t sent = SAT_OK;
    uint32_t token = 0u;
    if (count == 1u) {
        sat_parallel_task_slot_t& slot = g_runtime.slots[chosen[0]];
        token = slot.token;
        sent = sat_dual_sh2_send(
            kCommandExecute,
            saturn::hal::dual_sh2::memory::uncached_address(physical_address(&slot)),
            slot.token);
    } else {
        g_batch.count = count;
        for (uint32_t i = 0u; i < count; ++i) {
            sat_parallel_task_slot_t& slot = g_runtime.slots[chosen[i]];
            g_batch.slot_address[i] =
                saturn::hal::dual_sh2::memory::uncached_address(physical_address(&slot));
            g_batch.token[i] = slot.token;
        }
        saturn::hal::sh2::compiler_barrier();
        token = ++g_runtime.batch_sequence;
        if (token == 0u) token = ++g_runtime.batch_sequence;
        sent = sat_dual_sh2_send(
            kCommandBatch,
            saturn::hal::dual_sh2::memory::uncached_address(physical_address(&g_batch)),
            token);
    }
    if (sent == SAT_ERR_BUSY) {
        for (uint32_t i = 0u; i < count; ++i) {
            g_runtime.slots[chosen[i]].state = SAT_PARALLEL_QUEUED;
            ++g_runtime.stats.queued;
        }
        return SAT_OK;
    }
    if (sent != SAT_OK) {
        for (uint32_t i = 0u; i < count; ++i) mark_failed(g_runtime.slots[chosen[i]], sent);
        return sent;
    }
    g_runtime.active = static_cast<uint8_t>(count);
    for (uint32_t i = 0u; i < count; ++i) g_runtime.active_index[i] = chosen[i];
    g_runtime.active_token = token;
    g_runtime.completion_start_tick = saturn::hal::sh2::frt::counter();
    g_runtime.stats.slave_tasks += count;
    ++g_runtime.stats.slave_signals;
    if (count > 1u) g_runtime.stats.batched_tasks += count;
    ++g_runtime.progress;
    return SAT_OK;
}

/* One step of the queue: cancel what can no longer run, give the Slave its
 * next batch if it is idle, then run the best ready Master task in place. */
sat_result_t dispatch_one() {
    cancel_failed_dependents();
    queue_logic::Candidate candidates[kMaxCandidates];
    sat_result_t result = SAT_OK;
    if (g_runtime.active == 0u) {
        const uint32_t n = gather_candidates(candidates, kMaxCandidates);
        uint16_t chosen[kBatchMax] = {};
        const uint32_t count = queue_logic::pick_batch(candidates, n, kBatchMax, chosen);
        if (count != 0u) result = dispatch_batch(chosen, count);
    }
    const uint32_t n = gather_candidates(candidates, kMaxCandidates);
    const int32_t master = queue_logic::pick_master(candidates, n);
    if (master >= 0) {
        const sat_result_t ran = execute_master(g_runtime.slots[candidates[master].slot]);
        if (result == SAT_OK) result = ran;
    }
    return result;
}

}  // namespace

sat_result_t range_process(const void* input, uint32_t input_size, void* output,
                           uint32_t output_capacity, uint32_t* out_size);

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
    for (uint16_t& index : g_runtime.active_index) index = 0u;
    g_runtime.active_token = 0u;
    g_runtime.next_order = 0u;
    g_runtime.batch_sequence = 0u;
    g_runtime.progress = 0u;
    g_runtime.active_error = SAT_OK;
    g_runtime.stats = {};
    g_runtime.initialized = 1u;
    (void)register_task(SAT_PARALLEL_TASK_RANGE, range_process);
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

sat_result_t submit_desc(const sat_parallel_submit_desc_t& desc,
                         sat_parallel_handle_t* out_handle) {
    const sat_parallel_task_type_t type = desc.type;
    const void* input = desc.input;
    const uint32_t input_size = desc.input_size;
    void* output = desc.output;
    const uint32_t output_capacity = desc.output_capacity;
    if (g_runtime.initialized == 0u || out_handle == nullptr) return SAT_ERR_NOT_INITIALIZED;
    if (type == 0u || registration_for(type) == nullptr) return SAT_ERR_UNSUPPORTED;
    if ((input == nullptr && input_size != 0u) ||
        (output == nullptr && output_capacity != 0u)) return SAT_ERR_INVALID_ARG;
    if (desc.priority > queue_logic::kPriorityMax) return SAT_ERR_INVALID_ARG;
    if (desc.depends_on != 0u && slot_for(desc.depends_on) == nullptr) return SAT_ERR_INVALID_ARG;
#if SAT_PARALLEL_TEST_FAULT == SAT_PARALLEL_TEST_FAULT_SUBMIT_REJECT
    if (type == SAT_PARALLEL_TASK_SCENE_GEOMETRY &&
        (g_parallel_test_fault_fired &
         (1u << SAT_PARALLEL_TEST_FAULT_SUBMIT_REJECT)) == 0u) {
        g_parallel_test_fault_fired |=
            (1u << SAT_PARALLEL_TEST_FAULT_SUBMIT_REJECT);
        return SAT_ERR_BUSY;
    }
#endif
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
        slot.force_master = desc.force_master;
        slot.priority = desc.priority;
        slot.depends_on = desc.depends_on;
        slot.order = ++g_runtime.next_order;
        slot.result = SAT_ERR_BUSY;
        slot.reserved3[0] = 0u;
#if SAT_PARALLEL_TEST_FAULT == SAT_PARALLEL_TEST_FAULT_WORKER_ERROR || \
    SAT_PARALLEL_TEST_FAULT == SAT_PARALLEL_TEST_FAULT_TIMEOUT_ABORT || \
    SAT_PARALLEL_TEST_FAULT == SAT_PARALLEL_TEST_FAULT_ABORT_FAILURE || \
    SAT_PARALLEL_TEST_FAULT == SAT_PARALLEL_TEST_FAULT_RELEASE_FAILURE
        if (type == SAT_PARALLEL_TASK_SCENE_GEOMETRY)
            slot.reserved3[0] = SAT_PARALLEL_TEST_FAULT;
#endif
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

sat_result_t submit_ex(const sat_parallel_submit_desc_t* desc,
                       sat_parallel_handle_t* out_handle) {
    if (desc == nullptr) return SAT_ERR_INVALID_ARG;
    return submit_desc(*desc, out_handle);
}

sat_result_t submit_normal(sat_parallel_task_type_t type, const void* input,
                           uint32_t input_size, void* output, uint32_t output_capacity,
                           sat_parallel_handle_t* out_handle, uint8_t force_master) {
    sat_parallel_submit_desc_t desc = {};
    desc.type = type;
    desc.priority = SAT_PARALLEL_PRIORITY_NORMAL;
    desc.force_master = force_master;
    desc.input = input;
    desc.input_size = input_size;
    desc.output = output;
    desc.output_capacity = output_capacity;
    return submit_desc(desc, out_handle);
}

sat_result_t submit(sat_parallel_task_type_t type, const void* input,
                    uint32_t input_size, void* output, uint32_t output_capacity,
                    sat_parallel_handle_t* out_handle) {
    return submit_normal(type, input, input_size, output, output_capacity, out_handle, 0u);
}

sat_result_t submit_master(sat_parallel_task_type_t type, const void* input,
                           uint32_t input_size, void* output,
                           uint32_t output_capacity,
                           sat_parallel_handle_t* out_handle) {
    return submit_normal(type, input, input_size, output, output_capacity, out_handle, 1u);
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
        const uint32_t progress_before = g_runtime.progress;
        SAT_TRY(service());
        if (g_runtime.active_error != SAT_OK) return g_runtime.active_error;
        if (done(handle) != 0u) return result(handle, nullptr);
#if SAT_PARALLEL_TEST_FAULT == SAT_PARALLEL_TEST_FAULT_TIMEOUT_ABORT
        const sat_parallel_task_slot_t* const waiting_slot = slot_for(handle);
        if (waiting_slot != nullptr && g_runtime.active != 0u &&
            waiting_slot->state == SAT_PARALLEL_RUNNING &&
            waiting_slot->type == SAT_PARALLEL_TASK_SCENE_GEOMETRY &&
            waiting_slot->reserved3[0] == SAT_PARALLEL_TEST_FAULT_TIMEOUT_ABORT &&
            (g_parallel_test_fault_fired &
             (1u << SAT_PARALLEL_TEST_FAULT_TIMEOUT_ABORT)) == 0u) {
            /* Deterministically inject the timeout at the real executor wait
             * boundary while the worker callback is pinned in its test hook.
             * The caller then invokes the real abort/stop path. */
            g_parallel_test_fault_fired |=
                (1u << SAT_PARALLEL_TEST_FAULT_TIMEOUT_ABORT);
            return SAT_ERR_TIMEOUT;
        }
#endif
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
        } else if (g_runtime.progress == progress_before) {
            /* Master-only work completes in service(), one task per step, and
             * a task waiting on a dependency needs more steps; a queue that
             * made no progress at all cannot dispatch this task. */
            return SAT_ERR_BUSY;
        }
    }
}

sat_result_t abort_task(sat_parallel_handle_t handle, uint32_t timeout_ticks) {
    uint16_t index = 0u;
    sat_parallel_task_slot_t* slot = slot_for(handle, &index);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    bool member = false;
    for (uint8_t i = 0u; i < g_runtime.active; ++i) {
        if (g_runtime.active_index[i] == index) member = true;
    }
    if (g_runtime.active == 0u || !member || slot->state != SAT_PARALLEL_RUNNING) {
        return SAT_ERR_BUSY;
    }
#if SAT_PARALLEL_TEST_FAULT == SAT_PARALLEL_TEST_FAULT_ABORT_FAILURE
    if (slot->reserved3[0] == SAT_PARALLEL_TEST_FAULT_ABORT_FAILURE &&
        (g_parallel_test_fault_fired &
         (1u << SAT_PARALLEL_TEST_FAULT_ABORT_FAILURE)) == 0u) {
        g_parallel_test_fault_fired |=
            (1u << SAT_PARALLEL_TEST_FAULT_ABORT_FAILURE);
        return SAT_ERR_TIMEOUT;
    }
#endif
    const sat_result_t stopped = sat_dual_sh2_stop(timeout_ticks);
    if (stopped != SAT_OK) return stopped;

    /* stop() only returns after the Slave callback has returned and no longer
     * touches shared Work RAM. It is now safe to invalidate/reclaim storage.
     * Every member of the batch is settled: a task the Slave finished before
     * the stop keeps its result; the aborted one, and any still unfinished,
     * fail with a timeout. */
    for (uint8_t i = 0u; i < g_runtime.active; ++i) {
        sat_parallel_task_slot_t& member_slot = g_runtime.slots[g_runtime.active_index[i]];
        const bool target = &member_slot == slot;
        consume_slot(member_slot);
        const auto member_state = static_cast<sat_parallel_task_state_t>(member_slot.state);
        if (!target && (member_state == SAT_PARALLEL_COMPLETED ||
                        member_state == SAT_PARALLEL_FAILED)) {
            (void)queue_logic::apply_slave_completion(
                member_slot, static_cast<sat_result_t>(member_slot.result), g_runtime.stats);
            continue;
        }
        /* A callback may have observed the stop request and published COMPLETED
         * just before the Slave reached OFFLINE. Abort was initiated while this
         * slot was RUNNING, so its output is not a completed task result: once
         * the worker is confirmed stopped, invalidate that terminal state. */
        member_slot.result = static_cast<int32_t>(SAT_ERR_TIMEOUT);
        member_slot.state = SAT_PARALLEL_FAILED;
        ++g_runtime.stats.failed;
    }
    g_runtime.active = 0u;
    g_runtime.active_token = 0u;
    g_runtime.active_error = SAT_OK;
    g_runtime.slave_started = 0u;
    g_runtime.backend = SAT_PARALLEL_MASTER;
    ++g_runtime.progress;
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
#if SAT_PARALLEL_TEST_FAULT == SAT_PARALLEL_TEST_FAULT_RELEASE_FAILURE
    if (slot->type == SAT_PARALLEL_TASK_SCENE_GEOMETRY &&
        slot->reserved3[0] == SAT_PARALLEL_TEST_FAULT_RELEASE_FAILURE &&
        (g_parallel_test_fault_fired &
         (1u << SAT_PARALLEL_TEST_FAULT_RELEASE_FAILURE)) == 0u) {
        g_parallel_test_fault_fired |=
            (1u << SAT_PARALLEL_TEST_FAULT_RELEASE_FAILURE);
        return SAT_ERR_BUSY;
    }
#endif
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

/* Runs one slot on the Slave: the whole task, its result written back into the
 * shared slot (uncached) before anyone is told. Returns false when the slot is
 * not the task the Master announced, in which case nothing is reported. */
bool run_slot_on_slave(uint32_t slot_argument, uint32_t token, sat_result_t* out_result) {
    const uint32_t physical_slot =
        saturn::hal::dual_sh2::memory::cached_address(slot_argument);
    if (!saturn::hal::sh2::cache::is_supported_work_ram(
            physical_slot, sizeof(sat_parallel_task_slot_t))) return false;
    volatile sat_parallel_task_slot_t* const shared_slot =
        reinterpret_cast<volatile sat_parallel_task_slot_t*>(
            saturn::hal::dual_sh2::memory::uncached_address(physical_slot));
    if (shared_slot->token != token ||
        shared_slot->state != SAT_PARALLEL_RUNNING) return false;
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
#if SAT_PARALLEL_TEST_FAULT == SAT_PARALLEL_TEST_FAULT_WORKER_ERROR
    if (shared_slot->type == SAT_PARALLEL_TASK_SCENE_GEOMETRY &&
        shared_slot->reserved3[0] == SAT_PARALLEL_TEST_FAULT_WORKER_ERROR &&
        (g_parallel_test_fault_fired &
         (1u << SAT_PARALLEL_TEST_FAULT_WORKER_ERROR)) == 0u) {
        result = SAT_ERR_VERIFY_FAILED;
        g_parallel_test_fault_fired |=
            (1u << SAT_PARALLEL_TEST_FAULT_WORKER_ERROR);
    }
#endif
#if SAT_PARALLEL_TEST_FAULT == SAT_PARALLEL_TEST_FAULT_TIMEOUT_ABORT || \
    SAT_PARALLEL_TEST_FAULT == SAT_PARALLEL_TEST_FAULT_ABORT_FAILURE
    if (shared_slot->type == SAT_PARALLEL_TASK_SCENE_GEOMETRY &&
        shared_slot->reserved3[0] == SAT_PARALLEL_TEST_FAULT) {
#if SAT_PARALLEL_TEST_FAULT == SAT_PARALLEL_TEST_FAULT_TIMEOUT_ABORT
        /* The timeout profile's wait hook returns only while this slot is
         * RUNNING. Keep the real Slave callback active until abort sends
         * the shutdown request, eliminating a completion/abort race. */
        uint32_t spin = 0u;
        while (sat_dual_sh2_slave_stop_requested() == 0u) {
            __asm__ volatile("nop");
            if ((++spin & 0xFFu) == 0u &&
                sat_dual_sh2_slave_stop_requested() != 0u) break;
        }
#else
        for (uint32_t spin = 0u; spin < 12000000u; ++spin) {
            __asm__ volatile("nop");
        }
#endif
    }
#endif
    shared_slot->task_ticks = static_cast<uint16_t>(
        saturn::hal::sh2::frt::counter() - task_start);
    shared_slot->output_size = output_size;
    shared_slot->result = static_cast<int32_t>(result);
    shared_slot->state = result == SAT_OK ? SAT_PARALLEL_COMPLETED : SAT_PARALLEL_FAILED;
    saturn::hal::sh2::compiler_barrier();
    *out_result = result;
    return true;
}

/* Tells the Master, waiting while the mailbox is busy. False when the Master
 * stopped this CPU meanwhile. */
bool report_to_master(uint32_t command, uint32_t argument0, uint32_t argument1) {
    while (sat_dual_sh2_slave_send(command, argument0, argument1) == SAT_ERR_BUSY) {
        sat_dual_sh2_slave_heartbeat();
        if (sat_dual_sh2_slave_stop_requested() != 0u) return false;
    }
    (void)sat_dual_sh2_slave_signal_master();
    return true;
}

void slave_entry(void*) {
    for (;;) {
        sat_dual_sh2_slave_heartbeat();
        if (sat_dual_sh2_slave_stop_requested() != 0u) return;
        sat_dual_sh2_message_t message = {};
        if (sat_dual_sh2_slave_receive(&message) != SAT_OK) continue;
        if (message.command == kCommandExecute) {
            sat_result_t result = SAT_OK;
            if (!run_slot_on_slave(message.argument0, message.argument1, &result)) continue;
            if (!report_to_master(kCommandComplete, message.argument1,
                                  static_cast<uint32_t>(result))) return;
        } else if (message.command == kCommandBatch) {
            /* The batch block lives in shared Work RAM; read it uncached. */
            const uint32_t physical_block =
                saturn::hal::dual_sh2::memory::cached_address(message.argument0);
            if (!saturn::hal::sh2::cache::is_supported_work_ram(
                    physical_block, sizeof(BatchBlock))) continue;
            volatile BatchBlock* const block = reinterpret_cast<volatile BatchBlock*>(
                saturn::hal::dual_sh2::memory::uncached_address(physical_block));
            const uint32_t count = block->count < kBatchMax ? block->count : kBatchMax;
            for (uint32_t i = 0u; i < count; ++i) {
                sat_dual_sh2_slave_heartbeat();
                if (sat_dual_sh2_slave_stop_requested() != 0u) return;
                sat_result_t result = SAT_OK;
                (void)run_slot_on_slave(block->slot_address[i], block->token[i], &result);
            }
            if (!report_to_master(kCommandBatchComplete, message.argument1, count)) return;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Data-parallel loop                                                   */
/* ------------------------------------------------------------------ */

namespace {

struct RangeJob {
    sat_parallel_range_fn fn;
    void* context;
    uint32_t begin;
    uint32_t end;
};

/* One job at a time: sat_parallel_for() waits for the Slave half before it
 * returns, so a single shared descriptor is enough. */
static RangeJob g_range_job;
static uint8_t g_range_busy = 0u;
constexpr uint32_t kRangeTimeoutTicks = 60000u;

/* The Slave runs uncached-aliased inputs; when its cache is enabled it may hold
 * stale copies of what the Master just wrote (write-through, so RAM is right),
 * so drop everything first. The purge writes CCR, which the manual wants done
 * from the cache-through area: call it there. */
void purge_slave_cache() {
    namespace cache = saturn::hal::sh2::cache;
    const uint8_t ccr = cache::control_register();
    if ((ccr & 0x01u) == 0u) return;
    using Fn = void (*)(cache::Mode);
    const Fn purge = reinterpret_cast<Fn>(
        reinterpret_cast<uintptr_t>(&cache::purge_all) | saturn::core::startup::kCacheThroughBit);
    purge(static_cast<cache::Mode>(ccr & 0x09u));
}

}  // namespace

sat_result_t range_process(const void* input, uint32_t input_size, void*, uint32_t,
                           uint32_t* out_size) {
    if (input == nullptr || input_size != sizeof(RangeJob)) return SAT_ERR_INVALID_ARG;
    /* The Slave gets the cache-through alias of the descriptor. */
    if ((reinterpret_cast<uintptr_t>(input) & saturn::core::startup::kCacheThroughBit) != 0u) {
        purge_slave_cache();
    }
    const RangeJob* job = static_cast<const RangeJob*>(input);
    job->fn(job->context, job->begin, job->end);
    if (out_size != nullptr) *out_size = 0u;
    return SAT_OK;
}

sat_result_t parallel_for(uint32_t begin, uint32_t end, uint32_t min_grain,
                          sat_parallel_range_fn fn, void* context,
                          void* output, uint32_t output_bytes) {
    if (fn == nullptr || end < begin || (output == nullptr && output_bytes != 0u)) {
        return SAT_ERR_INVALID_ARG;
    }
    const uint32_t count = end - begin;
    if (count == 0u) return SAT_OK;
    const uint32_t grain = min_grain == 0u ? 1u : min_grain;
    const bool split = g_runtime.initialized != 0u && g_runtime.backend == SAT_PARALLEL_SLAVE &&
                       slave_available() != 0u && g_range_busy == 0u && count >= 2u &&
                       count / 2u >= grain;
    if (!split) {
        fn(context, begin, end);
        return SAT_OK;
    }
    const uint32_t middle = begin + count / 2u;
    g_range_busy = 1u;
    g_range_job = {fn, context, middle, end};
    sat_parallel_submit_desc_t desc = {};
    desc.type = SAT_PARALLEL_TASK_RANGE;
    desc.priority = SAT_PARALLEL_PRIORITY_URGENT;
    desc.input = &g_range_job;
    desc.input_size = sizeof(g_range_job);
    sat_parallel_handle_t handle = 0u;
    sat_result_t submitted = submit_desc(desc, &handle);
    if (submitted != SAT_OK) {
        g_range_busy = 0u;
        fn(context, begin, end);
        return SAT_OK;
    }
    fn(context, begin, middle);            /* the Master's half, while the Slave works */
    sat_result_t joined = wait(handle, kRangeTimeoutTicks);
    if (joined == SAT_ERR_TIMEOUT) {
        /* The Slave is stuck: stop it, then redo its half here. */
        if (abort_task(handle, kRangeTimeoutTicks) == SAT_OK) {
            fn(context, middle, end);
            joined = SAT_OK;
        }
    } else if (joined != SAT_OK) {
        /* The Slave half did not run to completion; finish it here so the
         * loop always covers its whole range. */
        fn(context, middle, end);
    }
    if (output != nullptr && output_bytes != 0u) {
        consume_range(physical_address(output), output_bytes);
    }
    (void)release(handle);
    g_range_busy = 0u;
    return joined;
}

#if SAT_PROFILE_METRICS || SAT_PARALLEL_TEST_FAULT != 0
extern "C" uint32_t sat_parallel_test_fault_fired(void) {
    return g_parallel_test_fault_fired;
}
#endif

}  // namespace saturn::core::parallel::executor
