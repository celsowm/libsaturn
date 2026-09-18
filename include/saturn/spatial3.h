#ifndef SATURN_SPATIAL3_H
#define SATURN_SPATIAL3_H

#include <stdint.h>

#include "saturn/collide3d.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SAT_SPATIAL3_EMPTY ((uint16_t)0xFFFFu)

typedef struct sat_spatial3_entry {
    int32_t cell_x;
    int32_t cell_y;
    int32_t cell_z;
    uint16_t id;
    uint16_t next;
} sat_spatial3_entry_t;

/* Caller-owned hashed uniform-grid broad phase for 3D AABBs.
 *
 * bucket_count must be a power of two. entry_cap counts object/cell
 * memberships, so an object spanning eight cells consumes eight entries.
 * items and stamps must both have item_cap elements. The grid has no world
 * bounds; signed cell coordinates are hashed into the caller's bucket table.
 */
typedef struct sat_spatial3 {
    uint16_t* heads;
    sat_spatial3_entry_t* entries;
    uint16_t* stamps;
    sat_aabb3_t* items;
    uint16_t bucket_count;
    uint16_t entry_cap;
    uint16_t entry_count;
    uint16_t item_cap;
    uint16_t query_stamp;
    uint8_t cell_shift;
} sat_spatial3_t;

sat_result_t sat_spatial3_init(
    sat_spatial3_t* spatial,
    uint8_t cell_shift,
    uint16_t* heads,
    uint16_t bucket_count,
    sat_spatial3_entry_t* entries,
    uint16_t entry_cap,
    uint16_t* stamps,
    sat_aabb3_t* items,
    uint16_t item_cap);

void sat_spatial3_clear(sat_spatial3_t* spatial);

sat_result_t sat_spatial3_insert(
    sat_spatial3_t* spatial,
    uint16_t id,
    const sat_aabb3_t* box);

sat_result_t sat_spatial3_query_aabb(
    sat_spatial3_t* spatial,
    const sat_aabb3_t* box,
    uint16_t* out_ids,
    uint16_t cap,
    uint16_t* out_count);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_SPATIAL3_H */
