#include <cstdio>

#include "saturn/view_cache.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); return 1; } } while (0)

int main() {
    sat_view_cache_item_t storage[6] = {};
    uint16_t counts[2] = {};
    sat_view_cache_t cache{};
    sat_quad2_t quad{};
    const sat_view_cache_item_t* items = nullptr;
    uint16_t count = 0u;

    OK(sat_view_cache_init(&cache, storage, counts, 2u, 3u) == SAT_OK);
    OK(sat_view_cache_set_generation(&cache, 1u) == SAT_OK);
    OK(sat_view_cache_begin(&cache, 0u) == SAT_OK);
    OK(sat_view_cache_append(&cache, &quad, 2u, 20u, 2u) == SAT_OK);
    OK(sat_view_cache_append(&cache, &quad, 5u, 50u, 5u) == SAT_OK);
    OK(sat_view_cache_append(&cache, &quad, 5u, 51u, 6u) == SAT_OK);
    OK(sat_view_cache_append(&cache, &quad, 1u, 10u, 1u) == SAT_ERR_CAPACITY);
    OK(sat_view_cache_sort(&cache) == SAT_OK);
    OK(sat_view_cache_view(&cache, 0u, &items, &count) == SAT_OK);
    OK(count == 3u && items[0].color == 50u && items[1].color == 51u && items[2].color == 20u);
    {
        sat_view_cache_stats_t stats{};
        OK(sat_view_cache_stats(&cache, &stats) == SAT_OK);
        OK(stats.baked_entries == 3u && stats.cache_hits == 1u && stats.views_ready == 1u);
    }
    OK(sat_view_cache_begin(&cache, 1u) == SAT_OK);
    OK(sat_view_cache_append(&cache, &quad, 9u, 90u, 9u) == SAT_OK);
    OK(sat_view_cache_sort(&cache) == SAT_OK);
    OK(sat_view_cache_view(&cache, 1u, &items, &count) == SAT_OK && count == 1u && items[0].tag == 9u);
    {
        sat_view_cache_stats_t stats{};
        OK(sat_view_cache_stats(&cache, &stats) == SAT_OK);
        OK(stats.baked_entries == 4u && stats.cache_hits == 2u && stats.views_ready == 2u);
    }
    OK(sat_view_cache_set_generation(&cache, 2u) == SAT_OK);
    OK(sat_view_cache_view(&cache, 0u, &items, &count) == SAT_OK && count == 0u);
    {
        sat_view_cache_stats_t stats{};
        OK(sat_view_cache_stats(&cache, &stats) == SAT_OK);
        OK(stats.baked_entries == 0u && stats.cache_hits == 0u && stats.views_ready == 0u);
    }
    std::puts("view cache: OK");
    return 0;
}
