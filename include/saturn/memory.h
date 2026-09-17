#ifndef SATURN_MEMORY_H
#define SATURN_MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sat_arena {
    uint8_t* memory;
    size_t capacity;
    size_t offset;
    size_t high_water;
} sat_arena_t;

sat_result_t sat_arena_init(sat_arena_t* arena, void* memory, size_t capacity);
void* sat_arena_alloc(sat_arena_t* arena, size_t size, size_t alignment);
void sat_arena_reset(sat_arena_t* arena);
size_t sat_arena_used(const sat_arena_t* arena);
size_t sat_arena_high_water(const sat_arena_t* arena);

typedef struct sat_pool_handle {
    uint16_t index;
    uint16_t generation;
} sat_pool_handle_t;

typedef struct sat_pool_slot {
    uint16_t generation;
    uint8_t used;
    uint8_t reserved;
} sat_pool_slot_t;

typedef struct sat_pool {
    uint8_t* memory;
    sat_pool_slot_t* slots;
    size_t element_size;
    size_t element_stride;
    uint16_t capacity;
    uint16_t used;
    uint16_t high_water;
} sat_pool_t;

sat_result_t sat_pool_init(
    sat_pool_t* pool,
    void* memory,
    size_t memory_size,
    size_t element_size,
    size_t alignment,
    uint16_t capacity,
    sat_pool_slot_t* slots);
sat_result_t sat_pool_acquire(sat_pool_t* pool, sat_pool_handle_t* out_handle, void** out_ptr);
sat_result_t sat_pool_release(sat_pool_t* pool, sat_pool_handle_t handle);
void* sat_pool_get(sat_pool_t* pool, sat_pool_handle_t handle);
const void* sat_pool_get_const(const sat_pool_t* pool, sat_pool_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_MEMORY_H */
