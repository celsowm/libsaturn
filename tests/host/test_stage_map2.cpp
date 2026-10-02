#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "saturn/stage_map2.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

static sat_fx16_t PX(double px) { return static_cast<sat_fx16_t>(px * SAT_FX16_ONE); }

/* ----- a fake VDP2 VRAM and the writer that fills it ----- */

struct Vram {
    std::vector<uint16_t> words = std::vector<uint16_t>(0x8000, 0xDEAD);
    int calls = 0;
    int fail_on_call = 0; /* 1-based; 0 = never */
};

static sat_result_t write_vram(void* user, uint32_t offset, const uint16_t* words, uint32_t count) {
    Vram* v = static_cast<Vram*>(user);
    ++v->calls;
    if (v->fail_on_call && v->calls == v->fail_on_call) return SAT_ERR_BUSY;
    OK(offset + count <= v->words.size());
    for (uint32_t i = 0; i < count; ++i) v->words[offset + i] = words[i];
    return SAT_OK;
}

/* ----- content ----- */

static const uint32_t kShift = 2; /* 4 x 4 cell metatiles */
static const uint32_t kMetatiles = 6;
static std::vector<uint16_t> g_cells;

static void build_tileset() {
    g_cells.assign(kMetatiles << (2 * kShift), 0);
    for (uint32_t m = 0; m < kMetatiles; ++m)
        for (uint32_t y = 0; y < 4; ++y)
            for (uint32_t x = 0; x < 4; ++x)
                g_cells[(m << 4) + (y << 2) + x] = static_cast<uint16_t>(((m & 7u) << 12) | (m * 16 + y * 4 + x));
}

static std::vector<uint16_t> make_map(int w, int h, uint32_t seed, bool flips) {
    std::vector<uint16_t> map(static_cast<size_t>(w) * h);
    uint32_t s = seed;
    for (uint16_t& e : map) {
        s = s * 1664525u + 1013904223u;
        e = static_cast<uint16_t>((s >> 16) % kMetatiles);
        if (flips) {
            if ((s >> 8) & 1u) e |= SAT_STAGE_MAP2_FLIP_X;
            if ((s >> 9) & 1u) e |= SAT_STAGE_MAP2_FLIP_Y;
        }
    }
    return map;
}

static sat_stage_map2_ring_t ring_single(uint32_t word, uint8_t cell_shift = 3, uint8_t page_shift = 6) {
    sat_stage_map2_ring_t r = {};
    r.cell_shift = cell_shift;
    r.page_shift = page_shift;
    r.pages_x = r.pages_y = 1;
    r.page_word[0] = word;
    return r;
}

/* 2 x 2 pages, deliberately not in address order. */
static sat_stage_map2_ring_t ring_quad() {
    sat_stage_map2_ring_t r = {};
    r.cell_shift = 3;
    r.page_shift = 6;
    r.pages_x = r.pages_y = 2;
    r.page_word[0] = 0x1000;
    r.page_word[1] = 0x4000;
    r.page_word[2] = 0x2000;
    r.page_word[3] = 0x3000;
    return r;
}

static sat_stage_map2_layer_desc_t layer(const std::vector<uint16_t>& map, int w, int h, const sat_stage_map2_ring_t& ring) {
    sat_stage_map2_layer_desc_t d = {};
    d.map = map.data();
    d.map_w = static_cast<uint16_t>(w);
    d.map_h = static_cast<uint16_t>(h);
    d.fill_word = 0x0777;
    d.outside = SAT_STAGE_MAP2_OUTSIDE_EMPTY;
    d.ratio_x = d.ratio_y = SAT_FX16_ONE;
    d.ring = ring;
    return d;
}

struct World {
    std::vector<uint16_t> map_a, map_b;
    sat_stage_map2_tileset_t tileset = {};
    sat_stage_map2_layer_desc_t layers[2] = {};
    sat_stage_map2_config_t config = {};
    std::vector<uint16_t> words;
    std::vector<sat_stage_map2_run_t> runs;
    sat_stage_map2_storage_t storage = {};
    sat_stage_map2_t map = {};
    Vram vram;

    explicit World(uint8_t layer_count = 2, uint32_t extra_words = 0, uint32_t extra_runs = 0) {
        build_tileset();
        map_a = make_map(40, 30, 1, true);
        map_b = make_map(25, 20, 2, false);
        tileset.cells = g_cells.data();
        tileset.metatile_count = kMetatiles;
        tileset.shift = kShift;
        layers[0] = layer(map_a, 40, 30, ring_single(0x0000));
        layers[1] = layer(map_b, 25, 20, ring_quad());
        config.tileset = &tileset;
        config.layers = layers;
        config.layer_count = layer_count;
        config.margin_cells = 1;
        config.viewport_w = 320;
        config.viewport_h = 224;
        uint32_t w = 0, r = 0;
        OK(sat_stage_map2_requirements(&config, &w, &r) == SAT_OK);
        words.assign(w + extra_words, 0);
        runs.assign(r + extra_runs, {});
        storage = {words.data(), static_cast<uint32_t>(words.size()), runs.data(), static_cast<uint32_t>(runs.size())};
        OK(sat_stage_map2_init(&map, &config, &storage) == SAT_OK);
    }

    void view(double x, double y) { OK(sat_stage_map2_set_view(&map, PX(x), PX(y)) == SAT_OK); }
    void commit() { OK(sat_stage_map2_commit(&map, write_vram, &vram) == SAT_OK); }
    uint32_t pending_words() const { uint32_t r, w; sat_stage_map2_pending(&map, &r, &w); return w; }
    uint32_t pending_runs() const { uint32_t r, w; sat_stage_map2_pending(&map, &r, &w); return r; }
};

/* The VRAM word of a ring cell, worked out from the manual's page layout rather than from the module. */
static uint32_t vram_word(const sat_stage_map2_ring_t& r, int32_t cx, int32_t cy) {
    const uint32_t page = 1u << r.page_shift;
    const uint32_t rx = static_cast<uint32_t>(cx) % (r.pages_x * page);
    const uint32_t ry = static_cast<uint32_t>(cy) % (r.pages_y * page);
    const uint32_t pi = (ry / page) * r.pages_x + (rx / page);
    return r.page_word[pi] + (ry % page) * page + (rx % page);
}

/* Every cell the layer keeps resident has to hold what a full render would put there. */
static void verify(World& w) {
    for (uint8_t i = 0; i < w.map.layer_count; ++i) {
        const sat_stage_map2_layer_state_t& l = w.map.layer[i];
        if (!l.valid) continue;
        for (int32_t cy = l.y0; cy < l.y0 + l.rows; ++cy)
            for (int32_t cx = l.x0; cx < l.x0 + l.cols; ++cx)
                OK(w.vram.words[vram_word(l.desc.ring, cx, cy)] == sat_stage_map2_cell(&w.map, i, cx, cy));
    }
}

/* ----- tests ----- */

static void initial_fill_and_idle() {
    World w;
    OK(w.map.layer[0].cols == 43 && w.map.layer[0].rows == 31);
    w.view(0, 0);
    OK(w.pending_words() == 2u * 43 * 31);
    w.commit();
    OK(w.pending_words() == 0 && w.pending_runs() == 0);
    verify(w);
    const sat_stage_map2_stats_t s = sat_stage_map2_stats(&w.map);
    OK(s.updates == 1 && s.rebuilds == 1 && s.cells_staged == 2u * 43 * 31 && s.cells_committed == s.cells_staged);

    /* standing still, or moving inside a cell, writes nothing */
    w.view(0, 0);
    OK(w.pending_words() == 0);
    w.view(3, 5);
    OK(w.pending_words() == 0);
    w.view(7.99, 7.5);
    OK(w.pending_words() == 0);
    OK(sat_stage_map2_stats(&w.map).updates == 1);
    sat_fx16_t sx, sy;
    OK(sat_stage_map2_scroll(&w.map, 0, &sx, &sy) == SAT_OK);
    OK(sx == PX(7.99) && sy == PX(7.5)); /* but the scroll follows the sub-cell motion */
}

static void incremental_moves() {
    World w;
    w.view(160, 160);
    w.commit();
    const uint32_t cols = 43, rows = 31;

    w.view(168, 160); /* one cell right: one new column */
    OK(w.pending_words() == 2u * rows);
    w.commit(); verify(w);
    w.view(168, 168); /* one cell down: one new row */
    OK(w.pending_words() == 2u * cols);
    w.commit(); verify(w);
    w.view(160, 160); /* back up-left at once: a column and a row, sharing a corner */
    OK(w.pending_words() == 2u * (rows + cols - 1));
    w.commit(); verify(w);
    w.view(120, 160); /* several cells */
    OK(w.pending_words() == 2u * 5 * rows);
    w.commit(); verify(w);
    w.view(120 - 8 * 20, 160 + 8 * 7);
    OK(w.pending_words() == 2u * (20 * rows + 7 * (cols - 20)));
    w.commit(); verify(w);
    OK(sat_stage_map2_stats(&w.map).rebuilds == 1); /* only the first fill */

    /* a jump that leaves no overlap rebuilds the window, no more than the window */
    w.view(120 - 8 * 20 + 8 * 1000, 216 + 8 * 1000);
    OK(w.pending_words() == 2u * cols * rows);
    w.commit(); verify(w);
    OK(sat_stage_map2_stats(&w.map).rebuilds == 2);
    /* just short of a full window's travel still shares a column */
    w.view(120 - 8 * 20 + 8 * 1000 + 8 * 42, 216 + 8 * 1000);
    OK(w.pending_words() == 2u * 42 * rows);
    w.commit(); verify(w);
    w.view(120 - 8 * 20 + 8 * 1000 + 8 * 42 + 8 * 43, 216 + 8 * 1000);
    OK(w.pending_words() == 2u * cols * rows);
    w.commit(); verify(w);
}

static void random_walk() {
    World w;
    uint32_t s = 12345;
    double x = 0, y = 0;
    uint32_t most = 0;
    for (int i = 0; i < 1500; ++i) {
        s = s * 1664525u + 1013904223u;
        const uint32_t pick = (s >> 24) % 10;
        const double dx = static_cast<double>(static_cast<int>((s >> 8) & 0x3F) - 32);
        const double dy = static_cast<double>(static_cast<int>((s >> 14) & 0x3F) - 32);
        if (pick == 0) { x = static_cast<double>(static_cast<int>(s >> 12) % 800) - 200; y = static_cast<double>(static_cast<int>(s >> 4) % 600) - 200; }
        else { x += dx; y += dy; }
        w.view(x, y);
        if (w.pending_words() > most) most = w.pending_words();
        OK(w.pending_words() <= 4u * 43 * 31); /* two updates between commits */
        if (i % 2 == 1) { w.commit(); verify(w); }
    }
    w.commit();
    verify(w);
    OK(most > 0);
    /* every committed word was staged, and nothing was staged twice over */
    const sat_stage_map2_stats_t st = sat_stage_map2_stats(&w.map);
    OK(st.cells_staged == st.cells_committed);
}

static void parallax_and_scroll() {
    World w;
    w.layers[1].ratio_x = SAT_FX16_ONE / 2;
    w.layers[1].ratio_y = 0;
    OK(sat_stage_map2_init(&w.map, &w.config, &w.storage) == SAT_OK);
    w.view(0, 0);
    w.commit();
    w.view(16, 40); /* layer 0 moves 2 cells right and 5 down, layer 1 one cell right and none down */
    const uint32_t a = 2u * 31 + 5u * (43 - 2);
    const uint32_t b = 1u * 31;
    OK(w.pending_words() == a + b);
    w.commit(); verify(w);
    sat_fx16_t sx, sy;
    OK(sat_stage_map2_scroll(&w.map, 1, &sx, &sy) == SAT_OK);
    OK(sx == PX(8) && sy == 0);
    /* the scroll wraps to the ring (512 px for one page, 1024 for 2 x 2) */
    w.view(-3, 0);
    OK(sat_stage_map2_scroll(&w.map, 0, &sx, &sy) == SAT_OK);
    OK(sx == PX(512 - 3) && sy == 0);
    OK(sat_stage_map2_scroll(&w.map, 1, &sx, &sy) == SAT_OK);
    OK(sx == PX(1024 - 1.5));
    w.view(-3.25, 513);
    OK(sat_stage_map2_scroll(&w.map, 0, &sx, &sy) == SAT_OK);
    OK(sx == PX(512 - 3.25) && sy == PX(1));
    w.commit(); verify(w);
    OK(sat_stage_map2_scroll(&w.map, 2, &sx, &sy) == SAT_ERR_INVALID_ARG);
    OK(sat_stage_map2_scroll(&w.map, 0, nullptr, &sy) == SAT_ERR_INVALID_ARG);
}

static void cells_flips_bias_palette() {
    World w;
    /* map entry 0 of layer 0 with every flip combination, and the metatile's own cells */
    std::vector<uint16_t> map = {0, SAT_STAGE_MAP2_FLIP_X | 0, SAT_STAGE_MAP2_FLIP_Y | 0, SAT_STAGE_MAP2_FLIP_X | SAT_STAGE_MAP2_FLIP_Y | 0,
                                 4, 5, 3, 2};
    w.layers[0] = layer(map, 4, 2, ring_single(0));
    w.layers[0].outside = SAT_STAGE_MAP2_OUTSIDE_EMPTY;
    OK(sat_stage_map2_init(&w.map, &w.config, &w.storage) == SAT_OK);
    auto cell = [&](int cx, int cy) { return sat_stage_map2_cell(&w.map, 0, cx, cy); };
    OK(cell(0, 0) == g_cells[0] && cell(3, 3) == g_cells[15]);
    /* X flip mirrors the metatile's columns and toggles the flip bit of every cell */
    OK(cell(4, 0) == (g_cells[3] ^ SAT_STAGE_MAP2_CELL_HFLIP));
    OK(cell(7, 2) == (g_cells[4 * 2 + 0] ^ SAT_STAGE_MAP2_CELL_HFLIP));
    OK(cell(8, 0) == (g_cells[12] ^ SAT_STAGE_MAP2_CELL_VFLIP));
    OK(cell(12, 0) == (g_cells[15] ^ SAT_STAGE_MAP2_CELL_HFLIP ^ SAT_STAGE_MAP2_CELL_VFLIP));
    OK(cell(0, 4) == g_cells[(4 << 4)]); /* second metatile row */
    OK(cell(2, 6) == g_cells[(4 << 4) + (2 << 2) + 2]);
    /* outside the map: the fill word */
    OK(cell(-1, 0) == 0x0777 && cell(16, 0) == 0x0777 && cell(0, 8) == 0x0777 && cell(0, -9) == 0x0777);

    /* clamp repeats the edge cells; wrap repeats the map */
    w.layers[0].outside = SAT_STAGE_MAP2_OUTSIDE_CLAMP;
    OK(sat_stage_map2_init(&w.map, &w.config, &w.storage) == SAT_OK);
    OK(cell(-5, 0) == cell(0, 0) && cell(99, 0) == cell(15, 0) && cell(3, -4) == cell(3, 0) && cell(3, 40) == cell(3, 7));
    w.layers[0].outside = SAT_STAGE_MAP2_OUTSIDE_WRAP;
    OK(sat_stage_map2_init(&w.map, &w.config, &w.storage) == SAT_OK);
    OK(cell(-1, 0) == cell(15, 0) && cell(16, 3) == cell(0, 3) && cell(5, -1) == cell(5, 7) && cell(5, 8) == cell(5, 0));
    OK(cell(-17, -9) == cell(15, 7));

    /* character bias wraps in ten bits and leaves the other bits alone */
    w.layers[0].outside = SAT_STAGE_MAP2_OUTSIDE_EMPTY;
    w.layers[0].char_bias = 0x100;
    OK(sat_stage_map2_init(&w.map, &w.config, &w.storage) == SAT_OK);
    OK(cell(0, 0) == ((g_cells[0] & ~0x3FFu) | ((g_cells[0] + 0x100u) & 0x3FFu)));
    OK(cell(1, 2) == (g_cells[9] + 0x100u));
    w.layers[0].char_bias = -1;
    OK(sat_stage_map2_init(&w.map, &w.config, &w.storage) == SAT_OK);
    OK((cell(0, 0) & 0x3FFu) == 0x3FFu && (cell(0, 0) & 0xFC00u) == (g_cells[0] & 0xFC00u));
    /* a palette override replaces bits 12-15 */
    w.layers[0].char_bias = 0;
    w.layers[0].flags = SAT_STAGE_MAP2_PALETTE_OVERRIDE;
    w.layers[0].palette = 9;
    OK(sat_stage_map2_init(&w.map, &w.config, &w.storage) == SAT_OK);
    OK(cell(1, 1) == ((g_cells[5] & 0x0FFFu) | 0x9000u));
    /* an index the table does not have shows the fill word */
    std::vector<uint16_t> bad = {static_cast<uint16_t>(kMetatiles + 3)};
    w.layers[0] = layer(bad, 1, 1, ring_single(0));
    OK(sat_stage_map2_init(&w.map, &w.config, &w.storage) == SAT_OK);
    OK(cell(2, 2) == 0x0777);
    OK(sat_stage_map2_cell(&w.map, 7, 0, 0) == 0 && sat_stage_map2_cell(nullptr, 0, 0, 0) == 0);
}

static void outside_while_streaming() {
    for (uint8_t policy = 0; policy < 3; ++policy) {
        World w;
        w.layers[0].outside = policy;
        w.layers[1].outside = static_cast<uint8_t>((policy + 1) % 3);
        OK(sat_stage_map2_init(&w.map, &w.config, &w.storage) == SAT_OK);
        for (double x = -400; x < 1500; x += 37) {
            w.view(x, x * 0.7 - 300);
            w.commit();
            verify(w);
        }
    }
}

static void multiple_layers_independent() {
    World w(1);
    w.view(0, 0);
    OK(w.pending_words() == 43u * 31);
    w.commit();
    World two;
    two.view(0, 0);
    two.commit();
    /* the second layer lives in its own pages and never disturbs the first */
    for (uint32_t i = 0; i < 0x1000; ++i) OK(w.vram.words[i] == two.vram.words[i]);
    OK(w.vram.words[0x1000] == 0xDEAD);
    OK(two.vram.words[0x1000] != 0xDEAD);
}

static void capacity() {
    const uint32_t full = 2u * 43 * 31;
    World w(2);
    /* storage of exactly one rebuild, with generous runs */
    std::vector<uint16_t> words(full - 1);
    std::vector<sat_stage_map2_run_t> runs(w.runs.size());
    sat_stage_map2_storage_t st = {words.data(), static_cast<uint32_t>(words.size()), runs.data(), static_cast<uint32_t>(runs.size())};
    sat_stage_map2_t m;
    OK(sat_stage_map2_init(&m, &w.config, &st) == SAT_OK);
    OK(sat_stage_map2_set_view(&m, 0, 0) == SAT_ERR_CAPACITY);
    OK(m.pending_words == 0 && m.pending_runs == 0 && !m.layer[0].valid && !m.layer[1].valid);
    OK(sat_stage_map2_stats(&m).updates == 0);

    words.resize(full);
    st.words = words.data();
    st.word_capacity = full;
    OK(sat_stage_map2_init(&m, &w.config, &st) == SAT_OK);
    OK(sat_stage_map2_set_view(&m, 0, 0) == SAT_OK && m.pending_words == full);
    /* a further update does not fit; the pending ones survive and nothing moves */
    const uint32_t before_x = static_cast<uint32_t>(m.layer[0].x0);
    OK(sat_stage_map2_set_view(&m, PX(800), 0) == SAT_ERR_CAPACITY);
    OK(m.pending_words == full && static_cast<uint32_t>(m.layer[0].x0) == before_x);
    OK(sat_stage_map2_commit(&m, write_vram, &w.vram) == SAT_OK);
    OK(sat_stage_map2_set_view(&m, PX(800), 0) == SAT_OK && m.pending_words == full);

    /* too few runs refuses the same way */
    std::vector<sat_stage_map2_run_t> few(3);
    st = {w.words.data(), static_cast<uint32_t>(w.words.size()), few.data(), 3};
    OK(sat_stage_map2_init(&m, &w.config, &st) == SAT_OK);
    OK(sat_stage_map2_set_view(&m, 0, 0) == SAT_ERR_CAPACITY && m.pending_runs == 0);

    /* the advertised requirement is enough to commit every other update without a refusal */
    World r;
    uint32_t seed = 99;
    for (int i = 0; i < 400; ++i) {
        seed = seed * 1664525u + 1013904223u;
        OK(sat_stage_map2_set_view(&r.map, PX(static_cast<int>(seed >> 20) % 1000), PX(static_cast<int>(seed >> 8) % 700)) == SAT_OK);
        if (i & 1) r.commit();
    }
    r.commit();
    verify(r);
}

static void dirty_regions() {
    World w;
    w.view(80, 64);
    w.commit();
    const sat_stage_map2_layer_state_t& l = w.map.layer[0];
    /* edit one map entry, restage its four cells */
    w.map_a[(l.y0 / 4 + 2) * 40 + (l.x0 / 4 + 3)] = 5;
    OK(sat_stage_map2_mark_dirty(&w.map, 0, (l.x0 / 4 + 3) * 4, (l.y0 / 4 + 2) * 4, 4, 4) == SAT_OK);
    OK(w.pending_words() == 16);
    w.commit(); verify(w);
    /* only the part inside the resident window is staged */
    OK(sat_stage_map2_mark_dirty(&w.map, 0, l.x0 - 3, l.y0 - 3, 5, 5) == SAT_OK);
    OK(w.pending_words() == 4);
    w.commit();
    OK(sat_stage_map2_mark_dirty(&w.map, 0, l.x0 + 1000, l.y0, 4, 4) == SAT_OK && w.pending_words() == 0);
    OK(sat_stage_map2_mark_dirty(&w.map, 0, l.x0, l.y0, 0, 4) == SAT_OK && w.pending_words() == 0);
    OK(sat_stage_map2_mark_dirty(&w.map, 0, 0, 0, -1, 4) == SAT_ERR_INVALID_ARG);
    OK(sat_stage_map2_mark_dirty(&w.map, 5, 0, 0, 4, 4) == SAT_ERR_INVALID_ARG);

    /* an edit that wraps the ring is split across runs but still lands */
    World v;
    v.view(8 * 61, 0); /* the resident columns straddle the ring's right edge */
    v.commit();
    v.map_a[3] = 4;
    v.map_a[4] = 4;
    OK(sat_stage_map2_mark_dirty(&v.map, 0, 4 * 15, 0, 8, 4) == SAT_OK);
    OK(v.pending_words() == 32);
    v.commit(); verify(v);

    /* a window nothing has filled yet has nothing to restage */
    World u;
    OK(sat_stage_map2_mark_dirty(&u.map, 0, 0, 0, 100, 100) == SAT_OK && u.pending_words() == 0);

    /* capacity is checked before anything is staged */
    World t(1);
    t.view(0, 0);
    t.commit();
    sat_stage_map2_storage_t st = {t.words.data(), 10, t.runs.data(), static_cast<uint32_t>(t.runs.size())};
    t.map.storage = st;
    OK(sat_stage_map2_mark_dirty(&t.map, 0, 0, 0, 8, 8) == SAT_ERR_CAPACITY && t.pending_words() == 0);
}

static void invalidate_and_discard() {
    World w;
    w.view(40, 40);
    w.commit();
    w.view(40, 40);
    OK(w.pending_words() == 0);
    OK(sat_stage_map2_invalidate(&w.map, 0) == SAT_OK);
    w.view(40, 40);
    OK(w.pending_words() == 43u * 31);
    OK(sat_stage_map2_invalidate(&w.map, 9) == SAT_ERR_INVALID_ARG);
    w.commit(); verify(w);

    /* discarding pending writes forgets what the layers thought was resident */
    w.view(400, 40);
    OK(w.pending_words() > 0);
    sat_stage_map2_discard(&w.map);
    OK(w.pending_words() == 0 && w.pending_runs() == 0);
    w.view(400, 40);
    OK(w.pending_words() == 2u * 43 * 31);
    w.commit(); verify(w);
}

static void commit_failure() {
    World w;
    w.view(0, 0);
    const uint32_t runs = w.pending_runs();
    OK(runs > 6);
    w.vram.fail_on_call = 3;
    OK(sat_stage_map2_commit(&w.map, write_vram, &w.vram) == SAT_ERR_BUSY);
    OK(w.pending_runs() == runs - 2); /* two runs went out, the failed one and the rest wait */
    const uint32_t left = w.pending_words();
    OK(left < 2u * 43 * 31);
    w.vram.fail_on_call = 0;
    OK(sat_stage_map2_commit(&w.map, write_vram, &w.vram) == SAT_OK);
    OK(w.pending_runs() == 0 && w.pending_words() == 0);
    verify(w);
    const sat_stage_map2_stats_t s = sat_stage_map2_stats(&w.map);
    OK(s.cells_committed == 2u * 43 * 31 && s.runs_committed == runs);
    OK(sat_stage_map2_commit(&w.map, nullptr, nullptr) == SAT_ERR_INVALID_ARG);
    OK(sat_stage_map2_commit(nullptr, write_vram, &w.vram) == SAT_ERR_INVALID_ARG);
}

static void two_by_two_characters() {
    std::vector<uint16_t> map = make_map(30, 20, 7, true);
    build_tileset();
    sat_stage_map2_tileset_t ts = {g_cells.data(), kMetatiles, kShift, 0};
    sat_stage_map2_layer_desc_t d = layer(map, 30, 20, ring_single(0x2000, 4, 5));
    sat_stage_map2_config_t cfg = {};
    cfg.tileset = &ts;
    cfg.layers = &d;
    cfg.layer_count = 1;
    cfg.margin_cells = 1;
    cfg.viewport_w = 320;
    cfg.viewport_h = 224;
    uint32_t wn = 0, rn = 0;
    OK(sat_stage_map2_requirements(&cfg, &wn, &rn) == SAT_OK);
    std::vector<uint16_t> words(wn);
    std::vector<sat_stage_map2_run_t> runs(rn);
    sat_stage_map2_storage_t st = {words.data(), wn, runs.data(), rn};
    sat_stage_map2_t m;
    OK(sat_stage_map2_init(&m, &cfg, &st) == SAT_OK);
    OK(m.layer[0].cols == 23 && m.layer[0].rows == 17); /* 16 px cells */
    Vram vram;
    for (int i = 0; i < 300; ++i) {
        OK(sat_stage_map2_set_view(&m, PX(i * 13 - 100), PX(i * 7 - 60)) == SAT_OK);
        OK(sat_stage_map2_commit(&m, write_vram, &vram) == SAT_OK);
        const sat_stage_map2_layer_state_t& l = m.layer[0];
        for (int32_t cy = l.y0; cy < l.y0 + l.rows; ++cy)
            for (int32_t cx = l.x0; cx < l.x0 + l.cols; ++cx)
                OK(vram.words[vram_word(l.desc.ring, cx, cy)] == sat_stage_map2_cell(&m, 0, cx, cy));
    }
    /* the scroll wraps at 32 cells of 16 px */
    sat_fx16_t sx, sy;
    OK(sat_stage_map2_set_view(&m, PX(-1), PX(1)) == SAT_OK);
    OK(sat_stage_map2_scroll(&m, 0, &sx, &sy) == SAT_OK && sx == PX(511) && sy == PX(1));
}

static void validation() {
    World w;
    sat_stage_map2_t m;
    sat_stage_map2_config_t c = w.config;
    OK(sat_stage_map2_init(nullptr, &c, &w.storage) == SAT_ERR_INVALID_ARG);
    OK(sat_stage_map2_init(&m, nullptr, &w.storage) == SAT_ERR_INVALID_ARG);
    OK(sat_stage_map2_init(&m, &c, nullptr) == SAT_ERR_INVALID_ARG);
    c = w.config; c.layer_count = 0;
    OK(sat_stage_map2_init(&m, &c, &w.storage) == SAT_ERR_INVALID_ARG);
    c = w.config; c.layer_count = 5;
    OK(sat_stage_map2_init(&m, &c, &w.storage) == SAT_ERR_INVALID_ARG);
    c = w.config; c.viewport_w = 0;
    OK(sat_stage_map2_init(&m, &c, &w.storage) == SAT_ERR_INVALID_ARG);
    c = w.config; c.viewport_w = 600; /* more cells than the 64-cell ring holds */
    OK(sat_stage_map2_init(&m, &c, &w.storage) == SAT_ERR_INVALID_ARG);
    c = w.config; c.margin_cells = 12;
    OK(sat_stage_map2_init(&m, &c, &w.storage) == SAT_ERR_INVALID_ARG);
    sat_stage_map2_tileset_t ts = w.tileset;
    c = w.config; c.tileset = &ts;
    ts.shift = 5;
    OK(sat_stage_map2_init(&m, &c, &w.storage) == SAT_ERR_INVALID_ARG);
    ts = w.tileset; ts.metatile_count = 0;
    OK(sat_stage_map2_init(&m, &c, &w.storage) == SAT_ERR_INVALID_ARG);
    ts = w.tileset; ts.cells = nullptr;
    OK(sat_stage_map2_init(&m, &c, &w.storage) == SAT_ERR_INVALID_ARG);
    sat_stage_map2_layer_desc_t layers[2] = {w.layers[0], w.layers[1]};
    c = w.config; c.layers = layers;
    layers[0].ratio_x = -1;
    OK(sat_stage_map2_init(&m, &c, &w.storage) == SAT_ERR_INVALID_ARG);
    layers[0] = w.layers[0]; layers[0].outside = 3;
    OK(sat_stage_map2_init(&m, &c, &w.storage) == SAT_ERR_INVALID_ARG);
    layers[0] = w.layers[0]; layers[0].map_w = 0;
    OK(sat_stage_map2_init(&m, &c, &w.storage) == SAT_ERR_INVALID_ARG);
    layers[0] = w.layers[0]; layers[0].map = nullptr;
    OK(sat_stage_map2_init(&m, &c, &w.storage) == SAT_ERR_INVALID_ARG);
    layers[0] = w.layers[0]; layers[0].palette = 16;
    OK(sat_stage_map2_init(&m, &c, &w.storage) == SAT_ERR_INVALID_ARG);
    layers[0] = w.layers[0]; layers[0].ring.pages_x = 3;
    OK(sat_stage_map2_init(&m, &c, &w.storage) == SAT_ERR_INVALID_ARG);
    layers[0] = w.layers[0]; layers[0].ring.page_shift = 7;
    OK(sat_stage_map2_init(&m, &c, &w.storage) == SAT_ERR_INVALID_ARG);
    sat_stage_map2_storage_t st = w.storage;
    st.runs = nullptr;
    OK(sat_stage_map2_init(&m, &w.config, &st) == SAT_ERR_INVALID_ARG);
    OK(sat_stage_map2_set_view(nullptr, 0, 0) == SAT_ERR_INVALID_ARG);
    uint32_t a, b;
    OK(sat_stage_map2_requirements(nullptr, &a, &b) == SAT_ERR_INVALID_ARG);
    OK(sat_stage_map2_requirements(&w.config, nullptr, &b) == SAT_ERR_INVALID_ARG);
    OK(sat_stage_map2_requirements(&w.config, &a, &b) == SAT_OK);
    OK(sat_stage_map2_requirements_bytes(&w.config) == a * 2u + b * sizeof(sat_stage_map2_run_t));
    OK(sat_stage_map2_requirements_bytes(nullptr) == 0);
}

static sat_vdp2_layer_config_t cell_layer(sat_vdp2_layer_t which) {
    sat_vdp2_layer_config_t c = {};
    c.layer = which;
    c.pattern_name_words = 1;
    c.plane_pages_x = c.plane_pages_y = 1;
    return c;
}

static void ring_from_layer_config() {
    sat_vdp2_layer_config_t c = cell_layer(SAT_VDP2_NBG1);
    /* one page repeated in all four planes */
    for (int i = 0; i < 4; ++i) c.plane_address[i] = 0x10000;
    sat_stage_map2_ring_t r;
    OK(sat_stage_map2_ring_from_layer(&c, &r) == SAT_OK);
    OK(r.cell_shift == 3 && r.page_shift == 6 && r.pages_x == 1 && r.pages_y == 1 && r.page_word[0] == 0x8000);
    /* four distinct planes: the whole 2 x 2 map */
    c.plane_address[0] = 0x00000; c.plane_address[1] = 0x04000; c.plane_address[2] = 0x08000; c.plane_address[3] = 0x0C000;
    OK(sat_stage_map2_ring_from_layer(&c, &r) == SAT_OK);
    OK(r.pages_x == 2 && r.pages_y == 2);
    OK(r.page_word[0] == 0x0000 && r.page_word[1] == 0x2000 && r.page_word[2] == 0x4000 && r.page_word[3] == 0x6000);
    /* planes of 2 x 1 pages: page order inside a plane is row-major, planes come in A B / C D order */
    c.plane_pages_x = 2; c.plane_pages_y = 1;
    c.plane_address[0] = 0x00000; c.plane_address[1] = 0x08000; c.plane_address[2] = 0x10000; c.plane_address[3] = 0x18000;
    OK(sat_stage_map2_ring_from_layer(&c, &r) == SAT_OK);
    OK(r.pages_x == 4 && r.pages_y == 2);
    OK(r.page_word[0] == 0x0000 && r.page_word[1] == 0x1000 && r.page_word[2] == 0x4000 && r.page_word[3] == 0x5000);
    OK(r.page_word[4] == 0x8000 && r.page_word[5] == 0x9000 && r.page_word[6] == 0xC000 && r.page_word[7] == 0xD000);
    /* 2 x 2 characters use 32-cell pages of 16-pixel cells */
    c = cell_layer(SAT_VDP2_NBG0);
    c.char_size = SAT_VDP2_CHAR_SIZE_2X2;
    for (int i = 0; i < 4; ++i) c.plane_address[i] = 0x4800;
    OK(sat_stage_map2_ring_from_layer(&c, &r) == SAT_OK);
    OK(r.cell_shift == 4 && r.page_shift == 5 && r.page_word[0] == 0x2400);
    /* refusals */
    c.bitmap = 1;
    OK(sat_stage_map2_ring_from_layer(&c, &r) == SAT_ERR_INVALID_ARG);
    c.bitmap = 0; c.pattern_name_words = 2;
    OK(sat_stage_map2_ring_from_layer(&c, &r) == SAT_ERR_INVALID_ARG);
    OK(sat_stage_map2_ring_from_layer(nullptr, &r) == SAT_ERR_INVALID_ARG);
}

int main() {
    initial_fill_and_idle();
    incremental_moves();
    random_walk();
    parallax_and_scroll();
    cells_flips_bias_palette();
    outside_while_streaming();
    multiple_layers_independent();
    capacity();
    dirty_regions();
    invalidate_and_discard();
    commit_failure();
    two_by_two_characters();
    validation();
    ring_from_layer_config();
    std::puts("stage_map2: ok");
    return 0;
}
