#ifndef SATURN_CORE_SPATIAL3_LOGIC_HPP
#define SATURN_CORE_SPATIAL3_LOGIC_HPP

#include <stdint.h>

#include "saturn/spatial3.h"
#include "src/physics/3d/collision_logic.hpp"
#include "src/physics/spatial/hash.hpp"

namespace saturn::core::spatial3 {

inline bool valid(const sat_spatial3_t* spatial) {
    return spatial != nullptr && spatial->heads != nullptr &&
           spatial->entries != nullptr && spatial->stamps != nullptr &&
           spatial->items != nullptr && spatial->entry_cap != 0u &&
           spatial->item_cap != 0u &&
           saturn::core::spatial3_hash::power_of_two(spatial->bucket_count) &&
           spatial->cell_shift <= 15u;
}

inline sat_result_t init(
    sat_spatial3_t& spatial,
    uint8_t cell_shift,
    uint16_t* heads,
    uint16_t bucket_count,
    sat_spatial3_entry_t* entries,
    uint16_t entry_cap,
    uint16_t* stamps,
    sat_aabb3_t* items,
    uint16_t item_cap
) {
    if (heads == nullptr || entries == nullptr || stamps == nullptr || items == nullptr ||
        entry_cap == 0u || item_cap == 0u || cell_shift > 15u ||
        !saturn::core::spatial3_hash::power_of_two(bucket_count)) {
        return SAT_ERR_INVALID_ARG;
    }
    spatial.heads = heads;
    spatial.entries = entries;
    spatial.stamps = stamps;
    spatial.items = items;
    spatial.bucket_count = bucket_count;
    spatial.entry_cap = entry_cap;
    spatial.entry_count = 0u;
    spatial.item_cap = item_cap;
    spatial.query_stamp = 0u;
    spatial.cell_shift = cell_shift;
    for (uint16_t i = 0u; i < bucket_count; ++i) heads[i] = SAT_SPATIAL3_EMPTY;
    for (uint16_t i = 0u; i < item_cap; ++i) stamps[i] = 0u;
    return SAT_OK;
}

inline void clear(sat_spatial3_t& spatial) {
    for (uint16_t i = 0u; i < spatial.entry_count; ++i) {
        const sat_spatial3_entry_t& entry = spatial.entries[i];
        spatial.heads[saturn::core::spatial3_hash::bucket(
            spatial.bucket_count, entry.cell_x, entry.cell_y, entry.cell_z)] = SAT_SPATIAL3_EMPTY;
    }
    spatial.entry_count = 0u;
}

inline sat_result_t insert(sat_spatial3_t& spatial, uint16_t id, const sat_aabb3_t& box) {
    if (!valid(&spatial) || id >= spatial.item_cap ||
        box.half.x < 0 || box.half.y < 0 || box.half.z < 0) {
        return SAT_ERR_INVALID_ARG;
    }
    int32_t x0, y0, z0, x1, y1, z1;
    saturn::core::spatial3_hash::aabb_cells(
        box, spatial.cell_shift, x0, y0, z0, x1, y1, z1);
    const uint64_t need =
        saturn::core::spatial3_hash::cell_count(x0, y0, z0, x1, y1, z1);
    if (spatial.entry_count > spatial.entry_cap ||
        need > static_cast<uint64_t>(spatial.entry_cap - spatial.entry_count)) {
        return SAT_ERR_CAPACITY;
    }
    spatial.items[id] = box;
    for (int32_t z = z0; z <= z1; ++z) {
        for (int32_t y = y0; y <= y1; ++y) {
            for (int32_t x = x0; x <= x1; ++x) {
                const uint16_t bucket = saturn::core::spatial3_hash::bucket(
                    spatial.bucket_count, x, y, z);
                sat_spatial3_entry_t& entry = spatial.entries[spatial.entry_count];
                entry.cell_x = x;
                entry.cell_y = y;
                entry.cell_z = z;
                entry.id = id;
                entry.next = spatial.heads[bucket];
                spatial.heads[bucket] = spatial.entry_count++;
            }
        }
    }
    return SAT_OK;
}

inline void next_query_stamp(sat_spatial3_t& spatial) {
    ++spatial.query_stamp;
    if (spatial.query_stamp == 0u) {
        for (uint16_t i = 0u; i < spatial.item_cap; ++i) spatial.stamps[i] = 0u;
        spatial.query_stamp = 1u;
    }
}

template <class Fn>
inline sat_result_t query_each(sat_spatial3_t& spatial, const sat_aabb3_t& box, Fn fn) {
    if (!valid(&spatial) || box.half.x < 0 || box.half.y < 0 || box.half.z < 0) {
        return SAT_ERR_INVALID_ARG;
    }
    next_query_stamp(spatial);
    int32_t x0, y0, z0, x1, y1, z1;
    saturn::core::spatial3_hash::aabb_cells(
        box, spatial.cell_shift, x0, y0, z0, x1, y1, z1);
    for (int32_t z = z0; z <= z1; ++z) {
        for (int32_t y = y0; y <= y1; ++y) {
            for (int32_t x = x0; x <= x1; ++x) {
                const uint16_t bucket = saturn::core::spatial3_hash::bucket(
                    spatial.bucket_count, x, y, z);
                for (uint16_t ei = spatial.heads[bucket];
                     ei != SAT_SPATIAL3_EMPTY;
                     ei = spatial.entries[ei].next) {
                    const sat_spatial3_entry_t& entry = spatial.entries[ei];
                    if (entry.cell_x != x || entry.cell_y != y || entry.cell_z != z) continue;
                    const uint16_t id = entry.id;
                    if (id >= spatial.item_cap || spatial.stamps[id] == spatial.query_stamp) continue;
                    spatial.stamps[id] = spatial.query_stamp;
                    if (!saturn::core::collide3d::aabb_overlap(box, spatial.items[id])) continue;
                    fn(id);
                }
            }
        }
    }
    return SAT_OK;
}

inline sat_result_t query(
    sat_spatial3_t& spatial,
    const sat_aabb3_t& box,
    uint16_t* out,
    uint16_t cap,
    uint16_t& count
) {
    count = 0u;
    sat_result_t result = SAT_OK;
    const sat_result_t query_result = query_each(spatial, box, [&](uint16_t id) {
        if (count >= cap) {
            result = SAT_ERR_CAPACITY;
            return;
        }
        out[count++] = id;
    });
    return query_result == SAT_OK ? result : query_result;
}

}  // namespace saturn::core::spatial3

#endif /* SATURN_CORE_SPATIAL3_LOGIC_HPP */
