#include "saturn/view_cache.h"

extern "C" sat_result_t sat_view_cache_init(sat_view_cache_t* cache,
    sat_view_cache_item_t* storage, uint16_t* counts,
    uint16_t view_count, uint16_t capacity_per_view) {
    if (!cache || !storage || !counts || !view_count || !capacity_per_view) {
        return SAT_ERR_INVALID_ARG;
    }
    cache->storage = storage;
    cache->counts = counts;
    cache->view_count = view_count;
    cache->capacity_per_view = capacity_per_view;
    cache->current_view = 0u;
    cache->generation = 0u;
    cache->baked_entries = 0u;
    cache->cache_hits = 0u;
    cache->active = 0u;
    for (uint16_t i = 0u; i < view_count; ++i) counts[i] = 0u;
    return SAT_OK;
}

extern "C" sat_result_t sat_view_cache_set_generation(sat_view_cache_t* cache,
    uint32_t generation) {
    if (!cache || !cache->storage || cache->active) return SAT_ERR_INVALID_ARG;
    if (cache->generation == generation) return SAT_OK;
    cache->generation = generation;
    cache->baked_entries = 0u;
    cache->cache_hits = 0u;
    for (uint16_t i = 0u; i < cache->view_count; ++i) cache->counts[i] = 0u;
    return SAT_OK;
}

extern "C" sat_result_t sat_view_cache_begin(sat_view_cache_t* cache, uint16_t view) {
    if (!cache || !cache->storage || view >= cache->view_count) return SAT_ERR_INVALID_ARG;
    cache->current_view = view;
    cache->counts[view] = 0u;
    cache->active = 1u;
    return SAT_OK;
}

extern "C" sat_result_t sat_view_cache_append(sat_view_cache_t* cache,
    const sat_quad2_t* quad, uint32_t depth, uint16_t color, uint16_t tag) {
    if (!cache || !cache->active || !quad) return SAT_ERR_INVALID_ARG;
    uint16_t& count = cache->counts[cache->current_view];
    if (count >= cache->capacity_per_view) return SAT_ERR_CAPACITY;
    sat_view_cache_item_t& item = cache->storage[
        static_cast<uint32_t>(cache->current_view) * cache->capacity_per_view + count];
    item.quad = *quad;
    item.depth = depth;
    item.color = color;
    item.tag = tag;
    ++count;
    ++cache->baked_entries;
    return SAT_OK;
}

extern "C" sat_result_t sat_view_cache_sort(sat_view_cache_t* cache) {
    if (!cache || !cache->active) return SAT_ERR_INVALID_ARG;
    sat_view_cache_item_t* list = cache->storage +
        static_cast<uint32_t>(cache->current_view) * cache->capacity_per_view;
    const uint16_t count = cache->counts[cache->current_view];
    for (uint16_t i = 1u; i < count; ++i) {
        const sat_view_cache_item_t value = list[i];
        int j = static_cast<int>(i) - 1;
        while (j >= 0 && list[j].depth < value.depth) {
            list[j + 1] = list[j];
            --j;
        }
        list[j + 1] = value;
    }
    cache->active = 0u;
    return SAT_OK;
}

extern "C" sat_result_t sat_view_cache_view(sat_view_cache_t* cache, uint16_t view,
    const sat_view_cache_item_t** out_items, uint16_t* out_count) {
    if (!cache || !out_items || !out_count || view >= cache->view_count || cache->active) {
        return SAT_ERR_INVALID_ARG;
    }
    *out_items = cache->storage + static_cast<uint32_t>(view) * cache->capacity_per_view;
    *out_count = cache->counts[view];
    if (cache->counts[view] != 0u) ++cache->cache_hits;
    return SAT_OK;
}

extern "C" sat_result_t sat_view_cache_stats(
    const sat_view_cache_t* cache, sat_view_cache_stats_t* out) {
    if (!cache || !out || !cache->storage || !cache->counts)
        return SAT_ERR_INVALID_ARG;
    out->baked_entries = cache->baked_entries;
    out->cache_hits = cache->cache_hits;
    out->views_ready = 0u;
    for (uint16_t i = 0u; i < cache->view_count; ++i) {
        if (cache->counts[i] != 0u) ++out->views_ready;
    }
    return SAT_OK;
}
