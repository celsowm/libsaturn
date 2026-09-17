#ifndef SATURN_CORE_MEMORY_LOGIC_HPP
#define SATURN_CORE_MEMORY_LOGIC_HPP

#include <stddef.h>
#include <stdint.h>
#include "saturn/memory.h"

namespace saturn::core::memory_logic {

inline bool is_power_of_two(size_t value) {
    return value != 0u && (value & (value - 1u)) == 0u;
}

inline bool align_up(size_t value, size_t alignment, size_t& out) {
    if (!is_power_of_two(alignment)) {
        return false;
    }
    const size_t mask = alignment - 1u;
    if (value > static_cast<size_t>(-1) - mask) {
        return false;
    }
    out = (value + mask) & ~mask;
    return true;
}

inline sat_result_t arena_init(sat_arena_t* arena, void* memory, size_t capacity) {
    if (arena == nullptr || (memory == nullptr && capacity != 0u)) {
        return SAT_ERR_INVALID_ARG;
    }
    arena->memory = static_cast<uint8_t*>(memory);
    arena->capacity = capacity;
    arena->offset = 0u;
    arena->high_water = 0u;
    return SAT_OK;
}

inline void* arena_alloc(sat_arena_t* arena, size_t size, size_t alignment) {
    if (arena == nullptr || arena->memory == nullptr || size == 0u || !is_power_of_two(alignment)) {
        return nullptr;
    }
    const uintptr_t base = reinterpret_cast<uintptr_t>(arena->memory);
    if (base > static_cast<uintptr_t>(-1) - arena->offset) {
        return nullptr;
    }
    const uintptr_t current = base + arena->offset;
    const uintptr_t mask = static_cast<uintptr_t>(alignment - 1u);
    if (current > static_cast<uintptr_t>(-1) - mask) {
        return nullptr;
    }
    const uintptr_t aligned = (current + mask) & ~mask;
    if (aligned < base) {
        return nullptr;
    }
    const size_t aligned_offset = static_cast<size_t>(aligned - base);
    if (aligned_offset > arena->capacity || size > arena->capacity - aligned_offset) {
        return nullptr;
    }
    arena->offset = aligned_offset + size;
    if (arena->offset > arena->high_water) {
        arena->high_water = arena->offset;
    }
    return reinterpret_cast<void*>(aligned);
}

inline void arena_reset(sat_arena_t* arena) {
    if (arena != nullptr) {
        arena->offset = 0u;
    }
}

inline sat_result_t pool_init(
    sat_pool_t* pool,
    void* memory,
    size_t memory_size,
    size_t element_size,
    size_t alignment,
    uint16_t capacity,
    sat_pool_slot_t* slots) {
    if (pool == nullptr || memory == nullptr || slots == nullptr || element_size == 0u || capacity == 0u ||
        !is_power_of_two(alignment)) {
        return SAT_ERR_INVALID_ARG;
    }

    const uintptr_t base = reinterpret_cast<uintptr_t>(memory);
    if ((base & static_cast<uintptr_t>(alignment - 1u)) != 0u) {
        return SAT_ERR_INVALID_ARG;
    }

    size_t stride = 0u;
    if (!align_up(element_size, alignment, stride) || stride == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    if (static_cast<size_t>(capacity) > static_cast<size_t>(-1) / stride) {
        return SAT_ERR_CAPACITY;
    }
    const size_t required = static_cast<size_t>(capacity) * stride;
    if (required > memory_size) {
        return SAT_ERR_CAPACITY;
    }

    pool->memory = static_cast<uint8_t*>(memory);
    pool->slots = slots;
    pool->element_size = element_size;
    pool->element_stride = stride;
    pool->capacity = capacity;
    pool->used = 0u;
    pool->high_water = 0u;
    for (uint16_t i = 0u; i < capacity; ++i) {
        pool->slots[i].generation = 1u;
        pool->slots[i].used = 0u;
        pool->slots[i].reserved = 0u;
    }
    return SAT_OK;
}

inline void* pool_ptr(sat_pool_t* pool, uint16_t index) {
    return pool->memory + static_cast<size_t>(index) * pool->element_stride;
}

inline const void* pool_ptr_const(const sat_pool_t* pool, uint16_t index) {
    return pool->memory + static_cast<size_t>(index) * pool->element_stride;
}

inline sat_result_t pool_acquire(sat_pool_t* pool, sat_pool_handle_t* out_handle, void** out_ptr) {
    if (pool == nullptr || out_handle == nullptr || out_ptr == nullptr || pool->memory == nullptr || pool->slots == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    for (uint16_t i = 0u; i < pool->capacity; ++i) {
        sat_pool_slot_t& slot = pool->slots[i];
        if (slot.used == 0u) {
            slot.used = 1u;
            ++pool->used;
            if (pool->used > pool->high_water) {
                pool->high_water = pool->used;
            }
            out_handle->index = i;
            out_handle->generation = slot.generation;
            *out_ptr = pool_ptr(pool, i);
            return SAT_OK;
        }
    }
    *out_ptr = nullptr;
    return SAT_ERR_CAPACITY;
}

inline bool pool_handle_valid(const sat_pool_t* pool, sat_pool_handle_t handle) {
    return pool != nullptr && pool->slots != nullptr && handle.index < pool->capacity &&
           pool->slots[handle.index].used != 0u && pool->slots[handle.index].generation == handle.generation;
}

inline sat_result_t pool_release(sat_pool_t* pool, sat_pool_handle_t handle) {
    if (!pool_handle_valid(pool, handle)) {
        return SAT_ERR_INVALID_ARG;
    }
    sat_pool_slot_t& slot = pool->slots[handle.index];
    slot.used = 0u;
    --pool->used;
    ++slot.generation;
    if (slot.generation == 0u) {
        slot.generation = 1u;
    }
    return SAT_OK;
}

inline void* pool_get(sat_pool_t* pool, sat_pool_handle_t handle) {
    return pool_handle_valid(pool, handle) ? pool_ptr(pool, handle.index) : nullptr;
}

inline const void* pool_get_const(const sat_pool_t* pool, sat_pool_handle_t handle) {
    return pool_handle_valid(pool, handle) ? pool_ptr_const(pool, handle.index) : nullptr;
}

}  // namespace saturn::core::memory_logic

#endif /* SATURN_CORE_MEMORY_LOGIC_HPP */
