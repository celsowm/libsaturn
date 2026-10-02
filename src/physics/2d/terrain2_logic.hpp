#ifndef SATURN_PHYSICS_TERRAIN2_LOGIC_HPP
#define SATURN_PHYSICS_TERRAIN2_LOGIC_HPP

/* Pure, host-testable Terrain2 queries. include/saturn/terrain2.h wraps these.
 *
 * Everything here works on "spans": for one pixel line of one tile, the solid
 * part is a single run [lo, hi) in display coordinates along the probe axis,
 * anchored to one end of the tile. A probe therefore needs one table read and a
 * couple of comparisons per tile, never a per-pixel loop. */

#include <stdint.h>

#include "saturn/terrain2.h"
#include "src/core/math2d/logic.hpp"

namespace saturn::physics::terrain2 {

namespace m2 = saturn::core::math2d;

enum class Kind : uint8_t { Empty, Profile, Outside };

struct TileRef {
    Kind kind;
    uint16_t word;
    uint16_t id;
    const sat_terrain_profile2_t* profile;
};

struct Span {
    int lo; /* solid pixels are lo <= offset < hi */
    int hi;
};

inline int abs_i(int v) { return v < 0 ? -v : v; }

inline bool valid_map(const sat_terrain_map2_t* map) {
    return map && map->layer_count > 0 && map->layer_count <= SAT_TERRAIN2_MAX_LAYERS &&
           map->metatile_shift <= 5 && map->cols > 0 && map->rows > 0;
}

inline uint8_t effective_angle(uint8_t angle, uint16_t word) {
    if (word & SAT_TERRAIN2_TILE_FLIP_X) angle = static_cast<uint8_t>(0u - angle);
    if (word & SAT_TERRAIN2_TILE_FLIP_Y) angle = static_cast<uint8_t>(128u - angle);
    return angle;
}

inline sat_vec2_t normal_of(uint8_t angle) {
    return {m2::sin8(angle), static_cast<sat_fx16_t>(-m2::cos8(angle))};
}

/* Angle of the face a probe along quadrant `dir` runs into when the profile's own
 * surface does not face the probe (a wall seen from the side, the map edge). */
inline uint8_t face_angle(uint8_t dir) { return static_cast<uint8_t>(((dir + 3u) & 3u) << 6); }

inline TileRef resolve(const sat_terrain_map2_t& m, uint8_t layer, int32_t tx, int32_t ty) {
    const TileRef empty = {Kind::Empty, 0, 0, nullptr};
    const int32_t shift = m.metatile_shift;
    const int32_t wt = static_cast<int32_t>(m.cols) << shift;
    const int32_t ht = static_cast<int32_t>(m.rows) << shift;
    if (tx < 0 || tx >= wt || ty < 0 || ty >= ht) {
        if (m.outside == SAT_TERRAIN2_OUTSIDE_SOLID) return {Kind::Outside, 0, 0xFFFFu, nullptr};
        if (m.outside != SAT_TERRAIN2_OUTSIDE_CLAMP) return empty;
        tx = tx < 0 ? 0 : (tx >= wt ? wt - 1 : tx);
        ty = ty < 0 ? 0 : (ty >= ht ? ht - 1 : ty);
    }
    const int32_t mask = (1 << shift) - 1;
    const uint16_t cell = m.layers[layer][(ty >> shift) * m.cols + (tx >> shift)];
    if (cell >= m.metatile_count) return empty;
    const uint16_t word = m.metatiles[(static_cast<uint32_t>(cell) << (2 * shift)) +
                                      static_cast<uint32_t>(((ty & mask) << shift) + (tx & mask))];
    const uint16_t id = word & SAT_TERRAIN2_TILE_INDEX_MASK;
    if (id >= m.profile_count) return empty;
    return {Kind::Profile, word, id, &m.profiles[id]};
}

/* Which tiles take part in a query. `dir` is the probe direction (any scale);
 * a one-way tile counts only when it opposes that direction. */
struct Filter {
    const sat_terrain_map2_t* map;
    sat_terrain_query2_t query;
    sat_vec2_t dir;
    bool check_one_way;
};

inline TileRef tile_at(const Filter& f, int32_t tx, int32_t ty) {
    TileRef t = resolve(*f.map, f.query.layer, tx, ty);
    if (t.kind != Kind::Profile) return t;
    const sat_terrain_profile2_t& p = *t.profile;
    bool take = (p.category & f.query.category_mask) != 0 && (p.flags & f.query.ignore_flags) == 0;
    if (take && f.check_one_way && (p.flags & SAT_TERRAIN2_ONE_WAY)) {
        take = m2::dot_raw(f.dir, normal_of(effective_angle(p.angle, t.word))) < 0;
    }
    if (!take) t.kind = Kind::Empty;
    return t;
}

inline Span span_of(const TileRef& t, bool columns, int line) {
    if (t.kind == Kind::Outside) return {0, 8};
    if (t.kind != Kind::Profile) return {0, 0};
    const sat_terrain_profile2_t& p = *t.profile;
    const bool fx = (t.word & SAT_TERRAIN2_TILE_FLIP_X) != 0;
    const bool fy = (t.word & SAT_TERRAIN2_TILE_FLIP_Y) != 0;
    int v;
    bool flip_along;
    if (columns) {
        v = p.column[fx ? 7 - line : line];
        flip_along = fy;
    } else {
        v = p.row[fy ? 7 - line : line];
        flip_along = fx;
    }
    if (v == 0) return {0, 0};
    const int len = abs_i(v);
    return ((v > 0) != flip_along) ? Span{8 - len, 8} : Span{0, len};
}

/* One probe line: an axis (0 = X, 1 = Y), the fixed coordinate on the other axis,
 * and the filter. Vertical probes read columns, horizontal probes read rows. */
struct Line {
    Filter filter;
    int axis;
    int32_t cross;
};

inline TileRef line_tile(const Line& l, int32_t pos) {
    const int32_t along = pos >> 3;
    const int32_t across = l.cross >> 3;
    return l.axis == 0 ? tile_at(l.filter, along, across) : tile_at(l.filter, across, along);
}
inline Span line_span(const Line& l, const TileRef& t) {
    return span_of(t, l.axis == 1, static_cast<int>(l.cross & 7));
}

inline bool line_solid(const Line& l, int32_t pos) {
    const Span s = line_span(l, line_tile(l, pos));
    const int off = static_cast<int>(pos & 7);
    return off >= s.lo && off < s.hi;
}

/* Index (0-based, counted from `pos`) of the first solid pixel among `count`
 * pixels stepping by `d`, or -1. */
inline int scan_solid(const Line& l, int32_t pos, int d, int count, TileRef& found) {
    int done = 0;
    while (done < count) {
        const TileRef t = line_tile(l, pos);
        const Span s = line_span(l, t);
        const int off = static_cast<int>(pos & 7);
        const int avail = d > 0 ? 8 - off : off + 1;
        const int use = avail < count - done ? avail : count - done;
        int first = -1;
        if (s.lo < s.hi) {
            if (d > 0) {
                const int f = off > s.lo ? off : s.lo;
                if (f < s.hi && f - off < use) first = f - off;
            } else {
                const int f = off < s.hi - 1 ? off : s.hi - 1;
                if (f >= s.lo && off - f < use) first = off - f;
            }
        }
        if (first >= 0) {
            found = t;
            return done + first;
        }
        pos += d * use;
        done += use;
    }
    return -1;
}

/* The pixel at `pos` is solid. Returns the index of the first free pixel among
 * `count` pixels stepping by `e` (so the solid run is that long), or -1. `last`
 * receives the tile holding the last solid pixel of the run. */
inline int scan_run(const Line& l, int32_t pos, int e, int count, TileRef& last) {
    int done = 0;
    last = line_tile(l, pos);
    while (done < count) {
        const TileRef t = line_tile(l, pos);
        const Span s = line_span(l, t);
        const int off = static_cast<int>(pos & 7);
        const int avail = e > 0 ? 8 - off : off + 1;
        const int use = avail < count - done ? avail : count - done;
        const bool in = off >= s.lo && off < s.hi;
        int free_idx;
        if (!in) free_idx = 0;
        else if (e > 0) free_idx = s.hi < 8 ? s.hi - off : -1;
        else free_idx = s.lo > 0 ? off - (s.lo - 1) : -1;
        if (free_idx >= 0 && free_idx < use) {
            if (free_idx > 0) last = t;
            return done + free_idx;
        }
        last = t;
        pos += e * use;
        done += use;
    }
    return -1;
}

inline void fill_hit(sat_terrain_hit2_t& h, const TileRef& t, sat_vec2_t dir, uint8_t dir_quadrant,
                     bool force_profile_angle, uint8_t layer) {
    uint8_t angle = face_angle(dir_quadrant);
    h.flags = 0;
    h.material = 0;
    h.collider_id = 0xFFFFu;
    if (t.kind == Kind::Profile) {
        const uint8_t ea = effective_angle(t.profile->angle, t.word);
        if (force_profile_angle || m2::dot_raw(dir, normal_of(ea)) < 0) angle = ea;
        h.flags = t.profile->flags;
        h.material = t.profile->material;
        h.collider_id = t.id;
    }
    h.angle = angle;
    h.normal = normal_of(angle);
    h.tangent = {m2::cos8(angle), m2::sin8(angle)};
    h.layer = layer;
}

inline sat_terrain_query2_t query_or_default(const sat_terrain_query2_t* q) {
    return q ? *q : sat_terrain_query2_default();
}

inline bool valid_query(const sat_terrain_map2_t* map, const sat_terrain_query2_t& q) {
    return valid_map(map) && q.layer < map->layer_count && map->layers[q.layer] != nullptr;
}

inline const sat_vec2_t kDirVec[4] = {
    {SAT_FX16_ONE, 0}, {0, SAT_FX16_ONE}, {-SAT_FX16_ONE, 0}, {0, -SAT_FX16_ONE}};

inline sat_result_t sample(const sat_terrain_map2_t* map, int32_t x, int32_t y,
                           const sat_terrain_query2_t* query, sat_terrain_hit2_t* out) {
    const sat_terrain_query2_t q = query_or_default(query);
    if (!valid_query(map, q)) return SAT_ERR_INVALID_ARG;
    const Filter f = {map, q, {0, 0}, false};
    const TileRef t = tile_at(f, x >> 3, y >> 3);
    if (t.kind == Kind::Empty) return SAT_ERR_NOT_FOUND;
    if (t.kind == Kind::Profile) {
        bool any = false;
        for (int i = 0; i < 8; ++i) any = any || t.profile->column[i] != 0;
        if (!any) return SAT_ERR_NOT_FOUND;
    }
    if (out) {
        fill_hit(*out, t, {0, 0}, 1, true, q.layer);
        out->point = {static_cast<sat_fx16_t>((x << 16) + 0x8000), static_cast<sat_fx16_t>((y << 16) + 0x8000)};
        out->distance = 0;
        out->tile_x = x >> 3;
        out->tile_y = y >> 3;
    }
    return SAT_OK;
}

inline int solid_at(const sat_terrain_map2_t* map, int32_t x, int32_t y,
                    const sat_terrain_query2_t* query) {
    const sat_terrain_query2_t q = query_or_default(query);
    if (!valid_query(map, q)) return 0;
    const Line l = {{map, q, {0, 0}, true}, 1, x};
    return line_solid(l, y) ? 1 : 0;
}

inline sat_result_t probe(const sat_terrain_map2_t* map, int32_t x, int32_t y, uint8_t dir,
                          int32_t range, const sat_terrain_query2_t* query,
                          sat_terrain_hit2_t* out) {
    const sat_terrain_query2_t q = query_or_default(query);
    if (!valid_query(map, q) || dir > 3 || range < 1 || range > 255) return SAT_ERR_INVALID_ARG;
    const int axis = dir & 1;
    const int d = dir < 2 ? 1 : -1;
    const int32_t pos = axis == 0 ? x : y;
    const Line l = {{map, q, kDirVec[dir], true}, axis, axis == 0 ? y : x};

    TileRef tile;
    int32_t surface;
    int32_t distance;
    if (!line_solid(l, pos)) {
        const int k = scan_solid(l, pos + d, d, range, tile);
        if (k < 0) return SAT_ERR_NOT_FOUND;
        surface = pos + d * (1 + k);
        distance = k;
    } else {
        const int run = scan_run(l, pos, -d, range + 1, tile);
        if (run < 1) return SAT_ERR_NOT_FOUND;
        surface = pos - d * (run - 1);
        distance = -run;
    }
    if (out) {
        fill_hit(*out, tile, kDirVec[dir], dir, false, q.layer);
        const int32_t edge = d > 0 ? surface : surface + 1;
        const int32_t mid = (l.cross << 16) + 0x8000;
        out->point = axis == 0 ? sat_vec2_t{edge << 16, mid} : sat_vec2_t{mid, edge << 16};
        out->distance = distance << 16;
        out->tile_x = axis == 0 ? surface >> 3 : l.cross >> 3;
        out->tile_y = axis == 0 ? l.cross >> 3 : surface >> 3;
    }
    return SAT_OK;
}

inline sat_result_t cast(const sat_terrain_map2_t* map, sat_vec2_t origin, sat_vec2_t delta,
                         const sat_terrain_query2_t* query, sat_terrain_hit2_t* out) {
    const sat_terrain_query2_t q = query_or_default(query);
    if (!valid_query(map, q)) return SAT_ERR_INVALID_ARG;
    const int64_t ax = delta.x < 0 ? -static_cast<int64_t>(delta.x) : delta.x;
    const int64_t ay = delta.y < 0 ? -static_cast<int64_t>(delta.y) : delta.y;
    const int64_t big = ax > ay ? ax : ay;
    const int64_t steps64 = (big + 0xFFFF) >> 16;
    if (steps64 > SAT_TERRAIN2_CAST_MAX_STEPS) return SAT_ERR_INVALID_ARG;
    const int32_t n = static_cast<int32_t>(steps64);
    const uint8_t quadrant = ax >= ay ? (delta.x >= 0 ? 0 : 2) : (delta.y >= 0 ? 1 : 3);
    const sat_vec2_t step = n ? sat_vec2_t{delta.x / n, delta.y / n} : sat_vec2_t{0, 0};
    const Filter f = {map, q, delta, true};

    sat_vec2_t pos = origin;
    for (int32_t i = 0; i <= n; ++i) {
        const int32_t px = pos.x >> 16;
        const int32_t py = pos.y >> 16;
        const TileRef t = tile_at(f, px >> 3, py >> 3);
        const Span s = span_of(t, true, static_cast<int>(px & 7));
        const int off = static_cast<int>(py & 7);
        if (off >= s.lo && off < s.hi) {
            if (out) {
                fill_hit(*out, t, delta, quadrant, false, q.layer);
                out->point = pos;
                out->distance = n ? static_cast<sat_fx16_t>((static_cast<int64_t>(i) << 16) / n) : 0;
                out->tile_x = px >> 3;
                out->tile_y = py >> 3;
            }
            return SAT_OK;
        }
        pos.x += step.x;
        pos.y += step.y;
    }
    return SAT_ERR_NOT_FOUND;
}

/* --- profile and map construction -------------------------------------- */

inline bool column_solid(int h, int y) { return h > 0 ? y >= 8 - h : (h < 0 && y < -h); }
inline bool row_solid(int w, int x) { return w > 0 ? x >= 8 - w : (w < 0 && x < -w); }

inline sat_result_t profile_from_columns(sat_terrain_profile2_t* out, const int8_t heights[8],
                                         uint8_t angle, uint8_t flags, uint8_t category,
                                         uint16_t material) {
    if (!out || !heights) return SAT_ERR_INVALID_ARG;
    sat_terrain_profile2_t p = {};
    for (int x = 0; x < 8; ++x) {
        if (heights[x] < -8 || heights[x] > 8) return SAT_ERR_INVALID_ARG;
        p.column[x] = heights[x];
    }
    for (int y = 0; y < 8; ++y) {
        int left = 0;
        while (left < 8 && column_solid(heights[left], y)) ++left;
        int right = 0;
        while (right < 8 - left && column_solid(heights[7 - right], y)) ++right;
        int count = 0;
        for (int x = 0; x < 8; ++x) count += column_solid(heights[x], y) ? 1 : 0;
        if (count == 0) p.row[y] = 0;
        else if (right == count) p.row[y] = static_cast<int8_t>(count);
        else if (left == count) p.row[y] = static_cast<int8_t>(-count);
        else return SAT_ERR_INVALID_ARG;
    }
    p.angle = angle;
    p.flags = flags;
    p.category = category;
    p.material = material;
    *out = p;
    return SAT_OK;
}

inline sat_result_t profile_validate(const sat_terrain_profile2_t* p) {
    if (!p) return SAT_ERR_INVALID_ARG;
    for (int i = 0; i < 8; ++i) {
        if (p->column[i] < -8 || p->column[i] > 8 || p->row[i] < -8 || p->row[i] > 8)
            return SAT_ERR_INVALID_ARG;
    }
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            if (column_solid(p->column[x], y) != row_solid(p->row[y], x)) return SAT_ERR_INVALID_ARG;
        }
    }
    return SAT_OK;
}

inline sat_result_t map_requirements(uint16_t profile_count, uint16_t metatile_count,
                                     uint8_t metatile_shift, uint16_t cols, uint16_t rows,
                                     uint8_t layers, uint32_t* out_bytes) {
    if (!out_bytes || metatile_shift > 5 || layers > SAT_TERRAIN2_MAX_LAYERS || profile_count > 1024u)
        return SAT_ERR_INVALID_ARG;
    const uint64_t tiles_per_metatile = 1ull << (2 * metatile_shift);
    const uint64_t bytes = static_cast<uint64_t>(profile_count) * sizeof(sat_terrain_profile2_t) +
                           static_cast<uint64_t>(metatile_count) * tiles_per_metatile * 2u +
                           static_cast<uint64_t>(layers) * cols * rows * 2u;
    if (bytes > 0xFFFFFFFFull) return SAT_ERR_CAPACITY;
    *out_bytes = static_cast<uint32_t>(bytes);
    return SAT_OK;
}

inline sat_result_t map_init(sat_terrain_map2_t* map, const sat_terrain_profile2_t* profiles,
                             uint16_t profile_count, const uint16_t* metatiles,
                             uint16_t metatile_count, uint8_t metatile_shift, uint16_t cols,
                             uint16_t rows) {
    if (!map || metatile_shift > 5 || cols == 0 || rows == 0 || profile_count > 1024u ||
        (profile_count > 0 && !profiles) || (metatile_count > 0 && !metatiles))
        return SAT_ERR_INVALID_ARG;
    *map = {};
    map->profiles = profiles;
    map->metatiles = metatiles;
    map->profile_count = profile_count;
    map->metatile_count = metatile_count;
    map->cols = cols;
    map->rows = rows;
    map->metatile_shift = metatile_shift;
    map->outside = SAT_TERRAIN2_OUTSIDE_EMPTY;
    map->width_px = static_cast<int32_t>(cols) << (metatile_shift + 3);
    map->height_px = static_cast<int32_t>(rows) << (metatile_shift + 3);
    return SAT_OK;
}

inline sat_result_t map_add_layer(sat_terrain_map2_t* map, const uint16_t* cells) {
    if (!map || !cells || map->cols == 0) return SAT_ERR_INVALID_ARG;
    if (map->layer_count >= SAT_TERRAIN2_MAX_LAYERS) return SAT_ERR_CAPACITY;
    map->layers[map->layer_count++] = cells;
    return SAT_OK;
}

inline sat_result_t map_set_outside(sat_terrain_map2_t* map, sat_terrain_outside2_t policy) {
    if (!map || policy < SAT_TERRAIN2_OUTSIDE_EMPTY || policy > SAT_TERRAIN2_OUTSIDE_CLAMP)
        return SAT_ERR_INVALID_ARG;
    map->outside = static_cast<uint8_t>(policy);
    return SAT_OK;
}

inline sat_result_t map_validate(const sat_terrain_map2_t* map) {
    if (!valid_map(map)) return SAT_ERR_INVALID_ARG;
    for (uint32_t i = 0; i < map->profile_count; ++i) {
        if (profile_validate(&map->profiles[i]) != SAT_OK) return SAT_ERR_INVALID_ARG;
    }
    const uint32_t per_metatile = 1u << (2 * map->metatile_shift);
    for (uint32_t m = 0; m < map->metatile_count; ++m) {
        for (uint32_t i = 0; i < per_metatile; ++i) {
            if ((map->metatiles[m * per_metatile + i] & SAT_TERRAIN2_TILE_INDEX_MASK) >= map->profile_count)
                return SAT_ERR_INVALID_ARG;
        }
    }
    const uint32_t cells = static_cast<uint32_t>(map->cols) * map->rows;
    for (uint32_t layer = 0; layer < map->layer_count; ++layer) {
        if (!map->layers[layer]) return SAT_ERR_INVALID_ARG;
        for (uint32_t i = 0; i < cells; ++i) {
            if (map->layers[layer][i] >= map->metatile_count) return SAT_ERR_INVALID_ARG;
        }
    }
    return SAT_OK;
}

} // namespace saturn::physics::terrain2

#endif
