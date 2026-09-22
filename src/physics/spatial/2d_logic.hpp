#ifndef SATURN_CORE_SPATIAL_LOGIC_HPP
#define SATURN_CORE_SPATIAL_LOGIC_HPP

#include <stdint.h>
#include "saturn/spatial.h"
#include "src/physics/2d/collision_logic.hpp"

namespace saturn::core::spatial {

inline bool valid(const sat_spatial_t* s) {
    return s && s->heads && s->entries && s->stamps && s->items &&
           s->cols && s->rows && s->item_cap;
}
inline uint32_t raw_shift(const sat_spatial_t& s) {
    return s.cell_shift >= 16u ? s.cell_shift : 16u + s.cell_shift;
}
inline int cell_x(const sat_spatial_t& s, int64_t raw) {
    int64_t v = raw >> raw_shift(s);
    if (v < 0) v = 0;
    if (v >= s.cols) v = s.cols - 1;
    return static_cast<int>(v);
}
inline int cell_y(const sat_spatial_t& s, int64_t raw) {
    int64_t v = raw >> raw_shift(s);
    if (v < 0) v = 0;
    if (v >= s.rows) v = s.rows - 1;
    return static_cast<int>(v);
}
inline void bounds(const sat_box2_t& b, int64_t& minx, int64_t& miny, int64_t& maxx, int64_t& maxy) {
    minx = static_cast<int64_t>(b.center.x) - b.half.x;
    miny = static_cast<int64_t>(b.center.y) - b.half.y;
    maxx = static_cast<int64_t>(b.center.x) + b.half.x;
    maxy = static_cast<int64_t>(b.center.y) + b.half.y;
}
inline uint32_t index(const sat_spatial_t& s, int x, int y) { return static_cast<uint32_t>(y) * s.cols + x; }

template <class Fn>
inline void for_cells(const sat_spatial_t& s, const sat_box2_t& b, Fn fn) {
    int64_t minx, miny, maxx, maxy;
    bounds(b, minx, miny, maxx, maxy);
    const int x0 = cell_x(s, minx), y0 = cell_y(s, miny);
    const int x1 = cell_x(s, maxx), y1 = cell_y(s, maxy);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) fn(x, y);
}

inline void clear(sat_spatial_t& s) {
    for (uint16_t i = 0; i < s.entry_count; ++i) {
        s.heads[s.entries[i].cell] = SAT_SPATIAL_EMPTY;
    }
    s.entry_count = 0;
}

inline sat_result_t insert(sat_spatial_t& s, uint16_t id, const sat_box2_t& box) {
    if (id >= s.item_cap || box.half.x < 0 || box.half.y < 0) return SAT_ERR_INVALID_ARG;
    uint32_t need = 0;
    for_cells(s, box, [&](int, int) { ++need; });
    if (s.entry_count > s.entry_cap || need > static_cast<uint32_t>(s.entry_cap - s.entry_count)) return SAT_ERR_CAPACITY;
    s.items[id] = box;
    for_cells(s, box, [&](int x, int y) {
        const uint32_t cell = index(s, x, y);
        sat_spatial_entry_t& e = s.entries[s.entry_count];
        e.id = id;
        e.next = s.heads[cell];
        e.cell = cell;
        s.heads[cell] = s.entry_count++;
    });
    return SAT_OK;
}

inline sat_result_t query(sat_spatial_t& s, const sat_box2_t& box, uint16_t* out, uint16_t cap, uint16_t& count) {
    count = 0;
    ++s.query_stamp;
    if (s.query_stamp == 0) {
        for (uint16_t i = 0; i < s.item_cap; ++i) s.stamps[i] = 0;
        s.query_stamp = 1;
    }
    sat_result_t result = SAT_OK;
    for_cells(s, box, [&](int x, int y) {
        for (uint16_t eidx = s.heads[index(s, x, y)]; eidx != SAT_SPATIAL_EMPTY; eidx = s.entries[eidx].next) {
            const uint16_t id = s.entries[eidx].id;
            if (s.stamps[id] == s.query_stamp) continue;
            s.stamps[id] = s.query_stamp;
            if (!saturn::core::collide2d::box_overlap(box, s.items[id])) continue;
            if (count >= cap) { result = SAT_ERR_CAPACITY; continue; }
            out[count++] = id;
        }
    });
    return result;
}

inline sat_result_t pairs(sat_spatial_t& s, sat_spatial_pair_t* out, uint16_t cap, uint16_t& count) {
    count = 0;
    sat_result_t result = SAT_OK;
    for (uint16_t head_entry = 0; head_entry < s.entry_count; ++head_entry) {
        const uint32_t ci = s.entries[head_entry].cell;
        if (s.heads[ci] != head_entry) continue;
        const uint16_t first = head_entry;
        for (uint16_t ea = first; ea != SAT_SPATIAL_EMPTY; ea = s.entries[ea].next) {
            const uint16_t a = s.entries[ea].id;
            for (uint16_t eb = s.entries[ea].next; eb != SAT_SPATIAL_EMPTY; eb = s.entries[eb].next) {
                const uint16_t b = s.entries[eb].id;
                if (a == b) continue;
                const sat_box2_t& ba = s.items[a];
                const sat_box2_t& bb = s.items[b];
                if (!saturn::core::collide2d::box_overlap(ba, bb)) continue;
                int64_t ax, ay, ax1, ay1, bx, by, bx1, by1;
                bounds(ba, ax, ay, ax1, ay1); bounds(bb, bx, by, bx1, by1);
                const int anchor_x = cell_x(s, ax > bx ? ax : bx);
                const int anchor_y = cell_y(s, ay > by ? ay : by);
                if (index(s, anchor_x, anchor_y) != ci) continue;
                const uint16_t lo = a < b ? a : b;
                const uint16_t hi = a < b ? b : a;
                if (count >= cap) { result = SAT_ERR_CAPACITY; continue; }
                out[count++] = {lo, hi};
            }
        }
    }
    return result;
}

} // namespace saturn::core::spatial

#endif
