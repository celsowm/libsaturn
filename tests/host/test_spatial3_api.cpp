#include <cstdio>
#include <cstdlib>

#include "saturn/collide3d.h"
#include "saturn/spatial3.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)
static sat_fx16_t F(int x) { return static_cast<sat_fx16_t>(x * 65536); }

int main() {
    uint16_t heads[16]{};
    sat_spatial3_entry_t entries[64]{};
    uint16_t stamps[2]{};
    sat_aabb3_t items[2]{};
    sat_spatial3_t spatial{};

    OK(sat_spatial3_init(&spatial, 2u, heads, 16u, entries, 64u, stamps, items, 2u) == SAT_OK);

    const sat_aabb3_t boxes[2] = {
        {{0, 0, 0}, {F(1), F(1), F(1)}},
        {{F(100), 0, 0}, {F(1), F(1), F(1)}}
    };
    OK(sat_spatial3_insert(&spatial, 0u, &boxes[0]) == SAT_OK);
    OK(sat_spatial3_insert(&spatial, 1u, &boxes[1]) == SAT_OK);

    uint16_t ids[2]{};
    uint16_t count = 0u;
    const sat_aabb3_t near_query{{0, 0, 0}, {F(4), F(4), F(4)}};
    OK(sat_spatial3_query_aabb(&spatial, &near_query, ids, 2u, &count) == SAT_OK);
    OK(count == 1u && ids[0] == 0u);

    sat_body3_t linear{{{F(-4), 0, 0}, F(1)}, {F(4), 0, 0}, 0u};
    sat_body3_t accelerated = linear;
    OK(sat_body3_collide_aabbs(&linear, boxes, 2u) == SAT_OK);
    OK(sat_body3_collide_spatial_aabbs(&accelerated, &spatial) == SAT_OK);
    OK(accelerated.shape.center.x == linear.shape.center.x);
    OK(accelerated.shape.center.y == linear.shape.center.y);
    OK(accelerated.shape.center.z == linear.shape.center.z);
    OK(accelerated.vel.x == linear.vel.x);
    OK(accelerated.vel.y == linear.vel.y);
    OK(accelerated.vel.z == linear.vel.z);
    OK(accelerated.flags == linear.flags);

    sat_spatial3_clear(&spatial);
    OK(sat_spatial3_query_aabb(&spatial, &near_query, ids, 2u, &count) == SAT_OK);
    OK(count == 0u);

    std::puts("spatial3 public API: OK");
    return 0;
}
