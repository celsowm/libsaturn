#ifndef SATURN_VIEW_CACHE_H
#define SATURN_VIEW_CACHE_H

#include <stdint.h>

#include "saturn/math3d.h"
#include "saturn/render3d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Immutable, caller-owned finite camera-view cache. Entries are copied into
 * the cache during bake; runtime replay never projects or sorts them. A
 * generation change invalidates every view before a new bake. The application
 * may use tag for occupancy metadata. */
typedef struct sat_view_cache_item {
    sat_quad2_t quad;
    uint32_t depth;
    uint16_t color;
    uint16_t tag;
} sat_view_cache_item_t;

typedef struct sat_view_cache_stats {
    uint32_t baked_entries;
    uint32_t cache_hits;
    uint16_t views_ready;
} sat_view_cache_stats_t;

typedef struct sat_view_cache {
    sat_view_cache_item_t* storage;
    uint16_t* counts;
    uint16_t view_count;
    uint16_t capacity_per_view;
    uint16_t current_view;
    uint32_t generation;
    uint32_t baked_entries;
    uint32_t cache_hits;
    uint8_t active;
} sat_view_cache_t;

sat_result_t sat_view_cache_init(sat_view_cache_t* cache,
    sat_view_cache_item_t* storage, uint16_t* counts,
    uint16_t view_count, uint16_t capacity_per_view);
sat_result_t sat_view_cache_set_generation(sat_view_cache_t* cache, uint32_t generation);
sat_result_t sat_view_cache_begin(sat_view_cache_t* cache, uint16_t view);
sat_result_t sat_view_cache_append(sat_view_cache_t* cache,
    const sat_quad2_t* quad, uint32_t depth, uint16_t color, uint16_t tag);
sat_result_t sat_view_cache_sort(sat_view_cache_t* cache);
sat_result_t sat_view_cache_view(sat_view_cache_t* cache, uint16_t view,
    const sat_view_cache_item_t** out_items, uint16_t* out_count);
sat_result_t sat_view_cache_stats(const sat_view_cache_t* cache,
    sat_view_cache_stats_t* out);

#ifdef __cplusplus
}
#endif

#endif
