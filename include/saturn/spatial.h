#ifndef SATURN_SPATIAL_H
#define SATURN_SPATIAL_H

#include <stdint.h>

#include "saturn/collide2d.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SAT_SPATIAL_EMPTY ((uint16_t)0xFFFFu)

typedef struct sat_spatial_entry {
    uint16_t id;
    uint16_t next;
} sat_spatial_entry_t;

typedef struct sat_spatial_pair {
    uint16_t a;
    uint16_t b;
} sat_spatial_pair_t;

/* Uniform-grid broad phase. All arrays are caller-owned. `cell_shift` is the
 * log2 of a cell's world-pixel size (cell size = 1 << cell_shift pixels).
 * The grid clamps out-of-range boxes to its edge cells. A cell about as large
 * as the largest object keeps the usual case close to O(n + candidate pairs);
 * putting every object in one cell is necessarily O(n^2).
 */
typedef struct sat_spatial {
    uint16_t* heads;
    sat_spatial_entry_t* entries;
    uint16_t* stamps;
    sat_box2_t* items;
    uint16_t cols, rows, cell_shift;
    uint16_t entry_cap, entry_count, item_cap;
    uint16_t query_stamp;
} sat_spatial_t;

sat_result_t sat_spatial_init(sat_spatial_t* sp, uint16_t* heads,
    uint16_t cols, uint16_t rows, uint8_t cell_shift,
    sat_spatial_entry_t* entries, uint16_t entry_cap,
    uint16_t* stamps, sat_box2_t* items, uint16_t item_cap);
void sat_spatial_clear(sat_spatial_t* sp);
sat_result_t sat_spatial_insert(sat_spatial_t* sp, uint16_t id, const sat_box2_t* box);
sat_result_t sat_spatial_query(sat_spatial_t* sp, const sat_box2_t* box,
    uint16_t* out_ids, uint16_t cap, uint16_t* out_count);
sat_result_t sat_spatial_pairs(sat_spatial_t* sp, sat_spatial_pair_t* out_pairs,
    uint16_t cap, uint16_t* out_count);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_SPATIAL_H */
