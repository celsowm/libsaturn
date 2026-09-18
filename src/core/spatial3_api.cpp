#include "saturn/spatial3.h"
#include "src/core/spatial3_logic.hpp"

extern "C" sat_result_t sat_spatial3_init(
    sat_spatial3_t* spatial,
    uint8_t cell_shift,
    uint16_t* heads,
    uint16_t bucket_count,
    sat_spatial3_entry_t* entries,
    uint16_t entry_cap,
    uint16_t* stamps,
    sat_aabb3_t* items,
    uint16_t item_cap
) {
    if (spatial == nullptr) return SAT_ERR_INVALID_ARG;
    return saturn::core::spatial3::init(
        *spatial, cell_shift, heads, bucket_count, entries, entry_cap,
        stamps, items, item_cap);
}

extern "C" void sat_spatial3_clear(sat_spatial3_t* spatial) {
    if (saturn::core::spatial3::valid(spatial)) saturn::core::spatial3::clear(*spatial);
}

extern "C" sat_result_t sat_spatial3_insert(
    sat_spatial3_t* spatial,
    uint16_t id,
    const sat_aabb3_t* box
) {
    if (spatial == nullptr || box == nullptr) return SAT_ERR_INVALID_ARG;
    return saturn::core::spatial3::insert(*spatial, id, *box);
}

extern "C" sat_result_t sat_spatial3_query_aabb(
    sat_spatial3_t* spatial,
    const sat_aabb3_t* box,
    uint16_t* out_ids,
    uint16_t cap,
    uint16_t* out_count
) {
    if (spatial == nullptr || box == nullptr || out_count == nullptr ||
        (out_ids == nullptr && cap != 0u)) {
        return SAT_ERR_INVALID_ARG;
    }
    uint16_t count = 0u;
    const sat_result_t result =
        saturn::core::spatial3::query(*spatial, *box, out_ids, cap, count);
    *out_count = count;
    return result;
}
