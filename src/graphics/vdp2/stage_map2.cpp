#include "saturn/stage_map2.h"

#include <limits.h>

/* Stage map streaming: the logic behind include/saturn/stage_map2.h. Hardware-free: the only
 * thing that leaves this file is a call through the writer given to sat_stage_map2_commit. */

namespace {

constexpr int64_t kOne = SAT_FX16_ONE;
constexpr uint32_t kMaxMetatiles = 16384u;

inline int64_t mul(int64_t a, int64_t b) { return (a * b) >> 16; }
inline int32_t floor_mod(int32_t v, int32_t n) {
    const int32_t r = v % n;
    return r < 0 ? r + n : r;
}
inline int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : (v > hi ? hi : v); }

inline int32_t ring_cols(const sat_stage_map2_ring_t& r) { return static_cast<int32_t>(r.pages_x) << r.page_shift; }
inline int32_t ring_rows(const sat_stage_map2_ring_t& r) { return static_cast<int32_t>(r.pages_y) << r.page_shift; }

bool valid_pages(uint8_t n) { return n == 1 || n == 2 || n == 4; }

bool valid_ring(const sat_stage_map2_ring_t& r) {
    return (r.cell_shift == 3 || r.cell_shift == 4) && (r.page_shift == 5 || r.page_shift == 6) &&
           valid_pages(r.pages_x) && valid_pages(r.pages_y);
}

/* The window a layer needs resident: its size never changes, only where it sits. */
int32_t window_cols(const sat_stage_map2_config_t& c, const sat_stage_map2_ring_t& r) {
    const int32_t cell = 1 << r.cell_shift;
    return ((static_cast<int32_t>(c.viewport_w) + cell - 1) >> r.cell_shift) + 1 + 2 * c.margin_cells;
}
int32_t window_rows(const sat_stage_map2_config_t& c, const sat_stage_map2_ring_t& r) {
    const int32_t cell = 1 << r.cell_shift;
    return ((static_cast<int32_t>(c.viewport_h) + cell - 1) >> r.cell_shift) + 1 + 2 * c.margin_cells;
}

/* One layer cell: map entry -> metatile -> cell word -> the layer's bias and palette. */
uint16_t cell_word(const sat_stage_map2_tileset_t& ts, const sat_stage_map2_layer_desc_t& d, int32_t cx, int32_t cy) {
    const int32_t shift = ts.shift;
    const int32_t span_x = static_cast<int32_t>(d.map_w) << shift;
    const int32_t span_y = static_cast<int32_t>(d.map_h) << shift;
    if (cx < 0 || cx >= span_x || cy < 0 || cy >= span_y) {
        if (d.outside == SAT_STAGE_MAP2_OUTSIDE_CLAMP) {
            cx = clamp_i32(cx, 0, span_x - 1);
            cy = clamp_i32(cy, 0, span_y - 1);
        } else if (d.outside == SAT_STAGE_MAP2_OUTSIDE_WRAP) {
            cx = floor_mod(cx, span_x);
            cy = floor_mod(cy, span_y);
        } else {
            return d.fill_word;
        }
    }
    const uint32_t mask = (1u << shift) - 1u;
    const uint16_t entry = d.map[static_cast<uint32_t>(cy >> shift) * d.map_w + static_cast<uint32_t>(cx >> shift)];
    const uint32_t index = entry & SAT_STAGE_MAP2_INDEX_MASK;
    if (index >= ts.metatile_count) return d.fill_word;
    uint32_t lx = static_cast<uint32_t>(cx) & mask, ly = static_cast<uint32_t>(cy) & mask;
    if (entry & SAT_STAGE_MAP2_FLIP_X) lx = mask - lx;
    if (entry & SAT_STAGE_MAP2_FLIP_Y) ly = mask - ly;
    uint32_t word = ts.cells[(index << (2 * shift)) + (ly << shift) + lx];
    if (entry & SAT_STAGE_MAP2_FLIP_X) word ^= SAT_STAGE_MAP2_CELL_HFLIP;
    if (entry & SAT_STAGE_MAP2_FLIP_Y) word ^= SAT_STAGE_MAP2_CELL_VFLIP;
    if (d.char_bias != 0) word = (word & ~0x3FFu) | ((word + static_cast<uint32_t>(static_cast<int32_t>(d.char_bias))) & 0x3FFu);
    if (d.flags & SAT_STAGE_MAP2_PALETTE_OVERRIDE) word = (word & 0x0FFFu) | (static_cast<uint32_t>(d.palette) << 12);
    return static_cast<uint16_t>(word);
}

/* VRAM word offset of ring cell (rx, ry). */
inline uint32_t ring_word(const sat_stage_map2_ring_t& r, int32_t rx, int32_t ry) {
    const uint32_t mask = (1u << r.page_shift) - 1u;
    const uint32_t page = (static_cast<uint32_t>(ry) >> r.page_shift) * r.pages_x + (static_cast<uint32_t>(rx) >> r.page_shift);
    return r.page_word[page] + ((static_cast<uint32_t>(ry) & mask) << r.page_shift) + (static_cast<uint32_t>(rx) & mask);
}

struct Rect {
    int32_t x0, y0, x1, y1; /* [x0, x1) x [y0, y1) in cells */
    bool empty() const { return x0 >= x1 || y0 >= y1; }
};

Rect window_rect(const sat_stage_map2_layer_state_t& l) { return {l.x0, l.y0, l.x0 + l.cols, l.y0 + l.rows}; }

Rect intersect(const Rect& a, const Rect& b) {
    return {a.x0 > b.x0 ? a.x0 : b.x0, a.y0 > b.y0 ? a.y0 : b.y0, a.x1 < b.x1 ? a.x1 : b.x1, a.y1 < b.y1 ? a.y1 : b.y1};
}

/* Calls run(ring_word_offset, cx, cy, count) for every contiguous stretch of VRAM in the rectangle. */
template <class F>
void for_runs(const sat_stage_map2_ring_t& ring, const Rect& r, F&& run) {
    if (r.empty()) return;
    const int32_t cols = ring_cols(ring), rows = ring_rows(ring);
    const int32_t page = 1 << ring.page_shift;
    for (int32_t cy = r.y0; cy < r.y1; ++cy) {
        const int32_t ry = static_cast<int32_t>(static_cast<uint32_t>(cy) & static_cast<uint32_t>(rows - 1));
        int32_t cx = r.x0;
        while (cx < r.x1) {
            const int32_t rx = static_cast<int32_t>(static_cast<uint32_t>(cx) & static_cast<uint32_t>(cols - 1));
            int32_t n = page - (rx & (page - 1));
            if (n > r.x1 - cx) n = r.x1 - cx;
            run(ring_word(ring, rx, ry), cx, cy, n);
            cx += n;
        }
    }
}

/* What changes between the resident window and the one needed: the needed window minus the overlap,
 * as at most four rectangles. Without an overlap it is the whole needed window. */
int split_new_cells(bool valid, const Rect& have, const Rect& need, Rect out[4]) {
    if (!valid) { out[0] = need; return 1; }
    const Rect o = intersect(have, need);
    if (o.empty()) { out[0] = need; return 1; }
    int n = 0;
    if (need.y0 < o.y0) out[n++] = {need.x0, need.y0, need.x1, o.y0};
    if (need.y1 > o.y1) out[n++] = {need.x0, o.y1, need.x1, need.y1};
    if (need.x0 < o.x0) out[n++] = {need.x0, o.y0, o.x0, o.y1};
    if (need.x1 > o.x1) out[n++] = {o.x1, o.y0, need.x1, o.y1};
    return n;
}

struct Counts {
    uint32_t words = 0, runs = 0;
};

void count_rect(const sat_stage_map2_ring_t& ring, const Rect& r, Counts& c) {
    for_runs(ring, r, [&](uint32_t, int32_t, int32_t, int32_t n) {
        c.words += static_cast<uint32_t>(n);
        ++c.runs;
    });
}

void stage_rect(sat_stage_map2_t& m, uint8_t layer, const Rect& r) {
    const sat_stage_map2_layer_state_t& l = m.layer[layer];
    for_runs(l.desc.ring, r, [&](uint32_t word, int32_t cx, int32_t cy, int32_t n) {
        sat_stage_map2_run_t& run = m.storage.runs[m.pending_runs++];
        run.word_offset = word;
        run.staging_index = m.pending_words;
        run.count = static_cast<uint16_t>(n);
        run.layer = layer;
        for (int32_t i = 0; i < n; ++i) m.storage.words[m.pending_words + static_cast<uint32_t>(i)] = cell_word(m.tileset, l.desc, cx + i, cy);
        m.pending_words += static_cast<uint32_t>(n);
        m.stats.cells_staged += static_cast<uint32_t>(n);
    });
}

bool fits(const sat_stage_map2_t& m, const Counts& c) {
    return c.words <= m.storage.word_capacity - m.pending_words && c.runs <= m.storage.run_capacity - m.pending_runs;
}

}  // namespace

/* ----- setup ----- */

extern "C" sat_result_t sat_stage_map2_ring_from_layer(const sat_vdp2_layer_config_t* config, sat_stage_map2_ring_t* out) {
    if (!config || !out || config->bitmap || config->pattern_name_words != 1) return SAT_ERR_INVALID_ARG;
    const bool big = config->char_size == SAT_VDP2_CHAR_SIZE_2X2;
    const uint32_t ppx = config->plane_pages_x, ppy = config->plane_pages_y;
    if ((ppx != 1 && ppx != 2) || (ppy != 1 && ppy != 2)) return SAT_ERR_INVALID_ARG;
    sat_stage_map2_ring_t r = {};
    r.cell_shift = big ? 4 : 3;
    r.page_shift = big ? 5 : 6;
    const uint32_t page_words = 1u << (2 * r.page_shift);
    const bool one_plane = config->plane_address[0] == config->plane_address[1] &&
                           config->plane_address[0] == config->plane_address[2] &&
                           config->plane_address[0] == config->plane_address[3];
    if (one_plane) {
        r.pages_x = static_cast<uint8_t>(ppx);
        r.pages_y = static_cast<uint8_t>(ppy);
        for (uint32_t py = 0; py < ppy; ++py)
            for (uint32_t px = 0; px < ppx; ++px)
                r.page_word[py * ppx + px] = (config->plane_address[0] >> 1) + (py * ppx + px) * page_words;
    } else {
        r.pages_x = static_cast<uint8_t>(2 * ppx);
        r.pages_y = static_cast<uint8_t>(2 * ppy);
        for (uint32_t py = 0; py < r.pages_y; ++py)
            for (uint32_t px = 0; px < r.pages_x; ++px) {
                const uint32_t plane = (py / ppy) * 2 + (px / ppx);
                const uint32_t inside = (py % ppy) * ppx + (px % ppx);
                r.page_word[py * r.pages_x + px] = (config->plane_address[plane] >> 1) + inside * page_words;
            }
    }
    *out = r;
    return SAT_OK;
}

extern "C" sat_result_t sat_stage_map2_init(sat_stage_map2_t* map, const sat_stage_map2_config_t* config,
    const sat_stage_map2_storage_t* storage) {
    if (!map || !config || !storage || !config->tileset || !config->layers) return SAT_ERR_INVALID_ARG;
    const sat_stage_map2_tileset_t& ts = *config->tileset;
    if (!ts.cells || ts.metatile_count == 0 || ts.metatile_count > kMaxMetatiles || ts.shift > 4) return SAT_ERR_INVALID_ARG;
    if (config->layer_count == 0 || config->layer_count > SAT_STAGE_MAP2_MAX_LAYERS) return SAT_ERR_INVALID_ARG;
    if (config->viewport_w == 0 || config->viewport_h == 0) return SAT_ERR_INVALID_ARG;
    if (!storage->words || !storage->runs) return SAT_ERR_INVALID_ARG;
    for (uint8_t i = 0; i < config->layer_count; ++i) {
        const sat_stage_map2_layer_desc_t& d = config->layers[i];
        if (!d.map || d.map_w == 0 || d.map_h == 0 || d.outside > SAT_STAGE_MAP2_OUTSIDE_WRAP || d.palette > 15) return SAT_ERR_INVALID_ARG;
        if (d.ratio_x < 0 || d.ratio_y < 0 || !valid_ring(d.ring)) return SAT_ERR_INVALID_ARG;
        if (window_cols(*config, d.ring) > ring_cols(d.ring) || window_rows(*config, d.ring) > ring_rows(d.ring)) return SAT_ERR_INVALID_ARG;
    }
    *map = {};
    map->tileset = ts;
    map->storage = *storage;
    map->layer_count = config->layer_count;
    map->margin_cells = config->margin_cells;
    map->viewport_w = config->viewport_w;
    map->viewport_h = config->viewport_h;
    for (uint8_t i = 0; i < config->layer_count; ++i) {
        sat_stage_map2_layer_state_t& l = map->layer[i];
        l.desc = config->layers[i];
        l.cols = static_cast<uint16_t>(window_cols(*config, l.desc.ring));
        l.rows = static_cast<uint16_t>(window_rows(*config, l.desc.ring));
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_stage_map2_requirements(const sat_stage_map2_config_t* config, uint32_t* out_words,
    uint32_t* out_runs) {
    if (!config || !config->layers || !out_words || !out_runs || config->layer_count == 0 ||
        config->layer_count > SAT_STAGE_MAP2_MAX_LAYERS) return SAT_ERR_INVALID_ARG;
    uint32_t words = 0, runs = 0;
    for (uint8_t i = 0; i < config->layer_count; ++i) {
        const sat_stage_map2_ring_t& r = config->layers[i].ring;
        if (!valid_ring(r)) return SAT_ERR_INVALID_ARG;
        const uint32_t cols = static_cast<uint32_t>(window_cols(*config, r));
        const uint32_t rows = static_cast<uint32_t>(window_rows(*config, r));
        words += cols * rows;
        runs += rows * ((cols >> r.page_shift) + 4);
    }
    *out_words = words * 2;
    *out_runs = runs * 2;
    return SAT_OK;
}

extern "C" uint32_t sat_stage_map2_requirements_bytes(const sat_stage_map2_config_t* config) {
    uint32_t words = 0, runs = 0;
    if (sat_stage_map2_requirements(config, &words, &runs) != SAT_OK) return 0;
    return words * static_cast<uint32_t>(sizeof(uint16_t)) + runs * static_cast<uint32_t>(sizeof(sat_stage_map2_run_t));
}

/* ----- updates ----- */

extern "C" sat_result_t sat_stage_map2_set_view(sat_stage_map2_t* map, sat_fx16_t x, sat_fx16_t y) {
    if (!map) return SAT_ERR_INVALID_ARG;
    struct Plan {
        int32_t x0, y0;
        int64_t sx, sy;
        Rect pieces[4];
        int count;
    } plan[SAT_STAGE_MAP2_MAX_LAYERS];

    Counts total;
    for (uint8_t i = 0; i < map->layer_count; ++i) {
        const sat_stage_map2_layer_state_t& l = map->layer[i];
        const sat_stage_map2_ring_t& ring = l.desc.ring;
        Plan& p = plan[i];
        p.sx = mul(x, l.desc.ratio_x);
        p.sy = mul(y, l.desc.ratio_y);
        p.x0 = static_cast<int32_t>((p.sx >> 16) >> ring.cell_shift) - map->margin_cells;
        p.y0 = static_cast<int32_t>((p.sy >> 16) >> ring.cell_shift) - map->margin_cells;
        const Rect need = {p.x0, p.y0, p.x0 + l.cols, p.y0 + l.rows};
        p.count = (l.valid && l.x0 == p.x0 && l.y0 == p.y0) ? 0 : split_new_cells(l.valid, window_rect(l), need, p.pieces);
        for (int k = 0; k < p.count; ++k) count_rect(ring, p.pieces[k], total);
    }
    if (!fits(*map, total)) return SAT_ERR_CAPACITY;

    bool moved = false, rebuilt = false;
    for (uint8_t i = 0; i < map->layer_count; ++i) {
        sat_stage_map2_layer_state_t& l = map->layer[i];
        const sat_stage_map2_ring_t& ring = l.desc.ring;
        Plan& p = plan[i];
        if (p.count > 0) {
            moved = true;
            const bool whole = p.count == 1 && p.pieces[0].x0 == p.x0 && p.pieces[0].y0 == p.y0 &&
                               p.pieces[0].x1 == p.x0 + l.cols && p.pieces[0].y1 == p.y0 + l.rows;
            if (!l.valid || whole) rebuilt = true;
            for (int k = 0; k < p.count; ++k) stage_rect(*map, i, p.pieces[k]);
            l.x0 = p.x0;
            l.y0 = p.y0;
            l.valid = 1;
        }
        const int64_t wrap_x = (static_cast<int64_t>(ring_cols(ring)) << ring.cell_shift) << 16;
        const int64_t wrap_y = (static_cast<int64_t>(ring_rows(ring)) << ring.cell_shift) << 16;
        l.scroll_x = static_cast<int32_t>(p.sx & (wrap_x - 1));
        l.scroll_y = static_cast<int32_t>(p.sy & (wrap_y - 1));
    }
    if (moved) ++map->stats.updates;
    if (rebuilt) ++map->stats.rebuilds;
    return SAT_OK;
}

extern "C" sat_result_t sat_stage_map2_scroll(const sat_stage_map2_t* map, uint8_t layer, sat_fx16_t* out_x, sat_fx16_t* out_y) {
    if (!map || layer >= map->layer_count || !out_x || !out_y) return SAT_ERR_INVALID_ARG;
    *out_x = map->layer[layer].scroll_x;
    *out_y = map->layer[layer].scroll_y;
    return SAT_OK;
}

extern "C" uint16_t sat_stage_map2_cell(const sat_stage_map2_t* map, uint8_t layer, int32_t cx, int32_t cy) {
    if (!map || layer >= map->layer_count) return 0;
    return cell_word(map->tileset, map->layer[layer].desc, cx, cy);
}

extern "C" sat_result_t sat_stage_map2_invalidate(sat_stage_map2_t* map, uint8_t layer) {
    if (!map || layer >= map->layer_count) return SAT_ERR_INVALID_ARG;
    map->layer[layer].valid = 0;
    return SAT_OK;
}

extern "C" sat_result_t sat_stage_map2_mark_dirty(sat_stage_map2_t* map, uint8_t layer, int32_t cx, int32_t cy, int32_t w,
    int32_t h) {
    if (!map || layer >= map->layer_count || w < 0 || h < 0) return SAT_ERR_INVALID_ARG;
    const sat_stage_map2_layer_state_t& l = map->layer[layer];
    if (!l.valid) return SAT_OK; /* the next set_view writes everything anyway */
    const Rect r = intersect(window_rect(l), {cx, cy, cx + w, cy + h});
    if (r.empty()) return SAT_OK;
    Counts c;
    count_rect(l.desc.ring, r, c);
    if (!fits(*map, c)) return SAT_ERR_CAPACITY;
    stage_rect(*map, layer, r);
    return SAT_OK;
}

/* ----- commit ----- */

extern "C" void sat_stage_map2_pending(const sat_stage_map2_t* map, uint32_t* out_runs, uint32_t* out_words) {
    if (out_runs) *out_runs = map ? map->pending_runs : 0;
    if (out_words) *out_words = map ? map->pending_words : 0;
}

extern "C" sat_result_t sat_stage_map2_commit(sat_stage_map2_t* map, sat_stage_map2_write_fn write, void* user) {
    if (!map || !write) return SAT_ERR_INVALID_ARG;
    uint32_t done = 0;
    sat_result_t result = SAT_OK;
    while (done < map->pending_runs) {
        const sat_stage_map2_run_t& run = map->storage.runs[done];
        result = write(user, run.word_offset, map->storage.words + run.staging_index, run.count);
        if (result != SAT_OK) break;
        map->stats.cells_committed += run.count;
        ++map->stats.runs_committed;
        ++done;
    }
    if (done == map->pending_runs) {
        map->pending_runs = 0;
        map->pending_words = 0;
        return SAT_OK;
    }
    /* A failed write keeps the rest pending, compacted to the front of the staging. */
    uint32_t kept_words = 0;
    for (uint32_t i = done; i < map->pending_runs; ++i) {
        sat_stage_map2_run_t run = map->storage.runs[i];
        for (uint32_t w = 0; w < run.count; ++w) map->storage.words[kept_words + w] = map->storage.words[run.staging_index + w];
        run.staging_index = kept_words;
        kept_words += run.count;
        map->storage.runs[i - done] = run;
    }
    map->pending_runs -= done;
    map->pending_words = kept_words;
    return result;
}

extern "C" void sat_stage_map2_discard(sat_stage_map2_t* map) {
    if (!map) return;
    map->pending_runs = 0;
    map->pending_words = 0;
    for (uint8_t i = 0; i < map->layer_count; ++i) map->layer[i].valid = 0;
}

extern "C" sat_stage_map2_stats_t sat_stage_map2_stats(const sat_stage_map2_t* map) {
    return map ? map->stats : sat_stage_map2_stats_t{};
}
