#include "saturn/task.h"

/* Gameplay task scheduler: the logic behind include/saturn/task.h.
 *
 * `order` holds slot numbers: [0, sorted) are the live tasks in run order, [sorted, sorted +
 * pending) were created during the current run and wait in creation order. Nothing moves while
 * a run walks the sorted part; destroyed tasks are only marked, and one flush at the end of the
 * run drops them, sorts the queued ones in with a stable insertion, and frees the slots. */

namespace {

enum : uint8_t { kFree = 0, kActive = 1, kPending = 2, kDead = 3 };

inline uint16_t next_generation(uint16_t g) { return static_cast<uint16_t>(g + 1u == 0x10000u ? 1u : g + 1u); }

inline bool valid(const sat_task_scheduler_t* s, sat_task_handle_t h) {
    if (h.generation == 0u || h.index >= s->capacity) return false;
    const sat_task_slot_t& slot = s->slots[h.index];
    return slot.generation == h.generation && (slot.state == kActive || slot.state == kPending);
}

void flush(sat_task_scheduler_t* s) {
    const uint16_t total = static_cast<uint16_t>(s->sorted + s->pending);
    uint16_t kept = 0u, kept_sorted = 0u;
    for (uint16_t i = 0u; i < total; ++i) {
        const uint16_t id = s->order[i];
        if (s->slots[id].state == kDead) {
            s->slots[id].state = kFree;
            continue;
        }
        s->order[kept++] = id;
        if (i < s->sorted) ++kept_sorted;
    }
    for (uint16_t k = kept_sorted; k < kept; ++k) {
        const uint16_t id = s->order[k];
        uint16_t j = k;
        s->slots[id].state = kActive;
        while (j > 0u && s->slots[s->order[j - 1u]].priority > s->slots[id].priority) {
            s->order[j] = s->order[j - 1u];
            --j;
        }
        s->order[j] = id;
    }
    s->sorted = kept;
    s->pending = 0u;
}

/* Marks the task dead, retires its handle and calls its destroy function. The slot is freed by flush. */
void kill(sat_task_scheduler_t* s, uint16_t index) {
    sat_task_slot_t& slot = s->slots[index];
    sat_task_destroy_fn destroy = slot.destroy;
    void* data = slot.data;
    slot.state = kDead;
    slot.generation = next_generation(slot.generation);
    --s->live;
    ++s->stats.destroyed;
    if (destroy) destroy(data);
}

}  // namespace

uint32_t sat_task_scheduler_slot_bytes(uint32_t capacity) { return capacity * static_cast<uint32_t>(sizeof(sat_task_slot_t)); }

sat_result_t sat_task_scheduler_init(sat_task_scheduler_t* s, sat_task_slot_t* slots, uint16_t* order, uint16_t capacity) {
    if (!s || !slots || !order || capacity == 0u) return SAT_ERR_INVALID_ARG;
    s->slots = slots;
    s->order = order;
    s->capacity = capacity;
    s->sorted = 0u;
    s->pending = 0u;
    s->live = 0u;
    s->busy = 0u;
    s->clearing = 0u;
    s->reserved[0] = s->reserved[1] = 0u;
    s->stats = sat_task_stats_t{};
    for (uint16_t i = 0u; i < capacity; ++i) {
        slots[i] = sat_task_slot_t{};
        slots[i].generation = 1u;
        slots[i].state = kFree;
    }
    return SAT_OK;
}

sat_result_t sat_task_create(sat_task_scheduler_t* s, const sat_task_desc_t* desc, sat_task_handle_t* out) {
    if (!s || !desc || !desc->update) return SAT_ERR_INVALID_ARG;
    if (s->clearing) return SAT_ERR_BUSY;
    uint16_t index = 0u;
    while (index < s->capacity && s->slots[index].state != kFree) ++index;
    if (index == s->capacity) return SAT_ERR_CAPACITY;

    sat_task_slot_t& slot = s->slots[index];
    slot.update = desc->update;
    slot.destroy = desc->destroy;
    slot.data = desc->data;
    slot.priority = desc->priority;
    slot.state = kPending;
    s->order[s->sorted + s->pending] = index;
    ++s->pending;
    ++s->live;
    ++s->stats.created;
    if (s->live > s->stats.peak_live) s->stats.peak_live = s->live;
    if (out) {
        out->index = index;
        out->generation = slot.generation;
    }
    if (!s->busy) flush(s);
    return SAT_OK;
}

sat_result_t sat_task_destroy(sat_task_scheduler_t* s, sat_task_handle_t h) {
    if (!s) return SAT_ERR_INVALID_ARG;
    if (!valid(s, h)) return SAT_ERR_NOT_FOUND;
    if (s->busy) {
        kill(s, h.index);
        return SAT_OK;
    }
    s->busy = 1u; /* a destroy function that destroys a peer must not flush under us */
    kill(s, h.index);
    s->busy = 0u;
    flush(s);
    return SAT_OK;
}

sat_result_t sat_task_set_update(sat_task_scheduler_t* s, sat_task_handle_t h, sat_task_update_fn update) {
    if (!s || !update) return SAT_ERR_INVALID_ARG;
    if (!valid(s, h)) return SAT_ERR_NOT_FOUND;
    s->slots[h.index].update = update;
    return SAT_OK;
}

int sat_task_is_alive(const sat_task_scheduler_t* s, sat_task_handle_t h) { return s && valid(s, h) ? 1 : 0; }

sat_result_t sat_task_data(const sat_task_scheduler_t* s, sat_task_handle_t h, void** out) {
    if (!s || !out) return SAT_ERR_INVALID_ARG;
    if (!valid(s, h)) return SAT_ERR_NOT_FOUND;
    *out = s->slots[h.index].data;
    return SAT_OK;
}

sat_result_t sat_task_scheduler_run(sat_task_scheduler_t* s) {
    if (!s) return SAT_ERR_INVALID_ARG;
    if (s->busy) return SAT_ERR_BUSY;
    s->busy = 1u;
    ++s->stats.runs;
    const uint16_t count = s->sorted;
    for (uint16_t i = 0u; i < count; ++i) {
        const uint16_t index = s->order[i];
        const sat_task_slot_t& slot = s->slots[index];
        if (slot.state != kActive) continue; /* destroyed by an earlier task this step */
        sat_task_handle_t self;
        self.index = index;
        self.generation = slot.generation;
        ++s->stats.tasks_run;
        slot.update(s, self, slot.data);
    }
    s->busy = 0u;
    flush(s);
    return SAT_OK;
}

sat_result_t sat_task_scheduler_clear(sat_task_scheduler_t* s) {
    if (!s) return SAT_ERR_INVALID_ARG;
    if (s->busy) return SAT_ERR_BUSY;
    s->busy = 1u;
    s->clearing = 1u;
    const uint16_t total = static_cast<uint16_t>(s->sorted + s->pending);
    for (uint16_t i = 0u; i < total; ++i) {
        const uint16_t index = s->order[i];
        if (s->slots[index].state == kActive || s->slots[index].state == kPending) kill(s, index);
    }
    s->clearing = 0u;
    s->busy = 0u;
    flush(s);
    return SAT_OK;
}

uint32_t sat_task_scheduler_count(const sat_task_scheduler_t* s) { return s ? s->live : 0u; }

sat_task_stats_t sat_task_scheduler_stats(const sat_task_scheduler_t* s) { return s ? s->stats : sat_task_stats_t{}; }
