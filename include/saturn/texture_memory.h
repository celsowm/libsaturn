#ifndef SATURN_TEXTURE_MEMORY_H
#define SATURN_TEXTURE_MEMORY_H
/* How many textures and texture regions a program can have registered (optional; most programs never include this).
 *
 * sat_texture_create keeps each texture, and each region cut from one, in a slot. The library supplies the table and
 * the slots it names (src/graphics/2d/textures/texture_memory.cpp): 64 textures, 64 regions and 128 hash buckets,
 * about 5 KiB of Work RAM. A program that registers few textures, or none, can define the table itself in one source
 * file with slots of its own; the linker then leaves the library's table, and its slots, out of the program:
 *
 *     #include "saturn/texture_memory.h"
 *     static sat_texture_slot_t textures[4];
 *     static sat_texture_region_slot_t regions[4];
 *     static uint16_t buckets[8];
 *     SAT_TEXTURE_MEMORY(textures, regions, buckets);
 *
 * The slots are zero-initialized storage the library owns while the program runs. A texture or region past the
 * capacity fails as SAT_ERR_CAPACITY, the same as past the default. The bucket count must be a power of two. */
#include <stdint.h>

#include "saturn/core.h"
#include "saturn/texture.h"
#include "saturn/vdp1.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sat_texture_slot {
    sat_vdp1_texture_t native;
    sat_surface_t source;
    uint16_t generation;
    uint16_t region_count;
    uint8_t palette_bank;
    uint8_t used;
    sat_texture_backing_policy_t policy;
} sat_texture_slot_t;

typedef struct sat_texture_region_slot {
    sat_vdp1_texture_t native;
    sat_rect_t rect;
    uint16_t owner_slot;
    uint16_t owner_generation;
    uint16_t next_hash;
    uint8_t used;
} sat_texture_region_slot_t;

typedef struct sat_texture_memory {
    sat_texture_slot_t* slots;
    sat_texture_region_slot_t* regions;
    uint16_t* buckets;
    uint16_t slot_capacity, region_capacity, bucket_capacity;
} sat_texture_memory_t;

extern const sat_texture_memory_t sat_texture_memory;

#ifdef __cplusplus
}
#endif

#define SAT_TEXTURE_MEMORY(texture_slots, region_slots, bucket_slots) \
    const sat_texture_memory_t sat_texture_memory = { \
        (texture_slots), (region_slots), (bucket_slots), \
        (uint16_t)(sizeof(texture_slots) / sizeof((texture_slots)[0])), \
        (uint16_t)(sizeof(region_slots) / sizeof((region_slots)[0])), \
        (uint16_t)(sizeof(bucket_slots) / sizeof((bucket_slots)[0]))}

#endif
