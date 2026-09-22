#include "saturn/memory.h"
#include "src/core/memory/logic.hpp"

extern "C" sat_result_t sat_arena_init(sat_arena_t* arena, void* memory, size_t capacity) {
    return saturn::core::memory_logic::arena_init(arena, memory, capacity);
}

extern "C" void* sat_arena_alloc(sat_arena_t* arena, size_t size, size_t alignment) {
    return saturn::core::memory_logic::arena_alloc(arena, size, alignment);
}

extern "C" void sat_arena_reset(sat_arena_t* arena) {
    saturn::core::memory_logic::arena_reset(arena);
}

extern "C" size_t sat_arena_used(const sat_arena_t* arena) {
    return arena != nullptr ? arena->offset : 0u;
}

extern "C" size_t sat_arena_high_water(const sat_arena_t* arena) {
    return arena != nullptr ? arena->high_water : 0u;
}

extern "C" sat_result_t sat_pool_init(
    sat_pool_t* pool,
    void* memory,
    size_t memory_size,
    size_t element_size,
    size_t alignment,
    uint16_t capacity,
    sat_pool_slot_t* slots) {
    return saturn::core::memory_logic::pool_init(pool, memory, memory_size, element_size, alignment, capacity, slots);
}

extern "C" sat_result_t sat_pool_acquire(sat_pool_t* pool, sat_pool_handle_t* out_handle, void** out_ptr) {
    return saturn::core::memory_logic::pool_acquire(pool, out_handle, out_ptr);
}

extern "C" sat_result_t sat_pool_release(sat_pool_t* pool, sat_pool_handle_t handle) {
    return saturn::core::memory_logic::pool_release(pool, handle);
}

extern "C" void* sat_pool_get(sat_pool_t* pool, sat_pool_handle_t handle) {
    return saturn::core::memory_logic::pool_get(pool, handle);
}

extern "C" const void* sat_pool_get_const(const sat_pool_t* pool, sat_pool_handle_t handle) {
    return saturn::core::memory_logic::pool_get_const(pool, handle);
}
