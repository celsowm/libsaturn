#include <cstdio>
#include <cstdlib>

#include "saturn/spatial3.h"
#include "src/physics/spatial/3d_logic.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)
static sat_fx16_t F(int x) { return static_cast<sat_fx16_t>(x * 65536); }

static bool contains(const uint16_t* ids, uint16_t count, uint16_t wanted) {
    for (uint16_t i = 0u; i < count; ++i) if (ids[i] == wanted) return true;
    return false;
}

int main() {
    using namespace saturn::core::spatial3;

    uint16_t heads[16]{};
    sat_spatial3_entry_t entries[96]{};
    uint16_t stamps[8]{};
    sat_aabb3_t items[8]{};
    sat_spatial3_t spatial{};
    OK(init(spatial, 2u, heads, 16u, entries, 96u, stamps, items, 8u) == SAT_OK);

    const sat_aabb3_t wide{{F(1), F(1), F(1)}, {F(3), F(3), F(3)}};
    const sat_aabb3_t near{{F(6), F(1), F(1)}, {F(1), F(1), F(1)}};
    const sat_aabb3_t negative{{F(-10), F(1), F(1)}, {F(1), F(1), F(1)}};
    OK(insert(spatial, 0u, wide) == SAT_OK);
    OK(insert(spatial, 1u, near) == SAT_OK);
    OK(insert(spatial, 2u, negative) == SAT_OK);

    uint16_t ids[8]{};
    uint16_t count = 0u;
    const sat_aabb3_t query_near{{F(3), F(1), F(1)}, {F(5), F(2), F(2)}};
    OK(query(spatial, query_near, ids, 8u, count) == SAT_OK);
    OK(count == 2u);
    OK(contains(ids, count, 0u));
    OK(contains(ids, count, 1u));

    const sat_aabb3_t query_negative{{F(-10), F(1), F(1)}, {F(2), F(2), F(2)}};
    OK(query(spatial, query_negative, ids, 8u, count) == SAT_OK);
    OK(count == 1u && ids[0] == 2u);

    /* wide spans multiple cells but must be reported once. */
    const sat_aabb3_t query_wide{{F(1), F(1), F(1)}, {F(3), F(3), F(3)}};
    OK(query(spatial, query_wide, ids, 8u, count) == SAT_OK);
    OK(contains(ids, count, 0u));
    uint16_t wide_seen = 0u;
    for (uint16_t i = 0u; i < count; ++i) wide_seen += ids[i] == 0u ? 1u : 0u;
    OK(wide_seen == 1u);

    clear(spatial);
    OK(spatial.entry_count == 0u);
    OK(query(spatial, query_near, ids, 8u, count) == SAT_OK && count == 0u);

    sat_spatial3_t tiny{};
    sat_spatial3_entry_t tiny_entry[1]{};
    OK(init(tiny, 2u, heads, 16u, tiny_entry, 1u, stamps, items, 8u) == SAT_OK);
    OK(insert(tiny, 0u, wide) == SAT_ERR_CAPACITY);

    std::puts("spatial3 logic: OK");
    return 0;
}
