#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "saturn/entity_stream2.h"
#include "saturn/follow_camera2d.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

static sat_fx16_t PX(double px) { return static_cast<sat_fx16_t>(px * SAT_FX16_ONE); }
static double D(sat_fx16_t v) { return v / 65536.0; }
static sat_box2_t box(double l, double t, double r, double b) {
    return {{PX((l + r) / 2), PX((t + b) / 2)}, {PX((r - l) / 2), PX((b - t) / 2)}};
}

/* ----- an index built the way the offline tool builds it ----- */

struct Index {
    std::vector<sat_entity_desc2_t> descs;
    std::vector<uint16_t> region_start;
    sat_entity_index2_t index = {};

    Index(uint16_t cols, uint16_t rows, uint8_t shift, uint32_t count, uint32_t seed, int32_t ox = -512, int32_t oy = 100) {
        std::vector<sat_entity_desc2_t> raw;
        uint32_t s = seed;
        const uint32_t w = static_cast<uint32_t>(cols) << shift, h = static_cast<uint32_t>(rows) << shift;
        for (uint32_t i = 0; i < count; ++i) {
            s = s * 1664525u + 1013904223u;
            const uint16_t x = static_cast<uint16_t>((s >> 8) % w);
            s = s * 1664525u + 1013904223u;
            const uint16_t y = static_cast<uint16_t>((s >> 8) % h);
            raw.push_back({x, y, static_cast<uint16_t>(i & 7u), static_cast<uint16_t>(i)});
        }
        /* region-major, stable within a region */
        std::stable_sort(raw.begin(), raw.end(), [&](const sat_entity_desc2_t& a, const sat_entity_desc2_t& b) {
            const uint32_t ra = (a.y >> shift) * cols + (a.x >> shift), rb = (b.y >> shift) * cols + (b.x >> shift);
            return ra < rb;
        });
        descs = raw;
        region_start.assign(static_cast<size_t>(cols) * rows + 1, 0);
        for (const sat_entity_desc2_t& d : descs) ++region_start[((d.y >> shift) * cols + (d.x >> shift)) + 1];
        for (size_t i = 1; i < region_start.size(); ++i) region_start[i] = static_cast<uint16_t>(region_start[i] + region_start[i - 1]);
        index.descs = descs.data();
        index.region_start = region_start.data();
        index.origin_x = ox;
        index.origin_y = oy;
        index.desc_count = static_cast<uint16_t>(descs.size());
        index.region_cols = cols;
        index.region_rows = rows;
        index.region_shift = shift;
    }
};

/* ----- a pool the way a game would keep one ----- */

struct Game {
    std::vector<uint32_t> live;       /* indices of live objects, in the order they were made */
    std::vector<uint32_t> made, freed; /* the callback log */
    uint32_t capacity = 1u << 30;
    std::vector<uint8_t> model;       /* 1 when the game believes the descriptor is live */
    std::vector<uint32_t> decline;    /* indices to decline */
    int retire_other_on_free = -1;
    sat_entity_stream2_t* stream = nullptr;
};

static sat_entity_activate_result_t on_activate(void* user, uint32_t index, const sat_entity_desc2_t*) {
    Game* g = static_cast<Game*>(user);
    OK(!g->model[index]); /* never twice for the same crossing */
    if (std::find(g->decline.begin(), g->decline.end(), index) != g->decline.end()) return SAT_ENTITY_DECLINE;
    if (g->live.size() >= g->capacity) return SAT_ENTITY_DEFER;
    g->model[index] = 1;
    g->live.push_back(index);
    g->made.push_back(index);
    return SAT_ENTITY_ACTIVATED;
}

static void on_deactivate(void* user, uint32_t index, const sat_entity_desc2_t*) {
    Game* g = static_cast<Game*>(user);
    OK(g->model[index]); /* only what is live goes away */
    g->model[index] = 0;
    g->live.erase(std::find(g->live.begin(), g->live.end(), index));
    g->freed.push_back(index);
    if (g->retire_other_on_free >= 0 && static_cast<uint32_t>(g->retire_other_on_free) != index) {
        const uint32_t other = static_cast<uint32_t>(g->retire_other_on_free);
        if (g->model[other]) {
            g->model[other] = 0;
            g->live.erase(std::find(g->live.begin(), g->live.end(), other));
        }
        OK(sat_entity_stream2_retire(g->stream, other) == SAT_OK);
    }
}

struct Rig {
    Index idx;
    Game game;
    std::vector<uint32_t> state;
    sat_entity_stream2_t stream = {};

    explicit Rig(Index i, double hysteresis = 0, bool with_deactivate = true) : idx(std::move(i)) {
        idx.index.descs = idx.descs.data(); /* the index points into its own vectors */
        idx.index.region_start = idx.region_start.data();
        game.model.assign(idx.descs.size(), 0);
        state.assign(sat_entity_stream2_state_words(idx.index.desc_count), 0xFFFFFFFFu);
        sat_entity_stream2_config_t c = {};
        c.index = &idx.index;
        c.state = state.data();
        c.state_words = static_cast<uint32_t>(state.size());
        c.hysteresis = PX(hysteresis);
        c.activate = on_activate;
        c.deactivate = with_deactivate ? on_deactivate : nullptr;
        c.user = &game;
        OK(sat_entity_stream2_init(&stream, &c) == SAT_OK);
        game.stream = &stream;
    }
    sat_entity_stream2_result_t update(const sat_box2_t& b) {
        sat_entity_stream2_result_t r = {};
        game.made.clear();
        game.freed.clear();
        OK(sat_entity_stream2_update(&stream, &b, &r) == SAT_OK);
        return r;
    }
};

static bool inside_px(const Index& i, const sat_entity_desc2_t& d, const sat_box2_t& b, double grow = 0) {
    const double px = i.index.origin_x + d.x, py = i.index.origin_y + d.y;
    const double cx = b.center.x / 65536.0, cy = b.center.y / 65536.0, hx = b.half.x / 65536.0 + grow, hy = b.half.y / 65536.0 + grow;
    return px >= cx - hx && px < cx + hx && py >= cy - hy && py < cy + hy;
}

static bool ascending(const std::vector<uint32_t>& v) { return std::is_sorted(v.begin(), v.end()) && std::adjacent_find(v.begin(), v.end()) == v.end(); }

/* ----- tests ----- */

static void validation() {
    Index good(8, 4, 6, 200, 1);
    OK(sat_entity_index2_validate(&good.index) == SAT_OK);
    OK(sat_entity_index2_validate(nullptr) == SAT_ERR_INVALID_ARG);

    Index bad = good;
    bad.index.descs = bad.descs.data();
    bad.index.region_start = bad.region_start.data();
    std::swap(bad.descs[0], bad.descs[bad.descs.size() - 1]); /* a descriptor in the wrong region */
    OK(sat_entity_index2_validate(&bad.index) == SAT_ERR_INVALID_ARG);

    Index t(8, 4, 6, 200, 1);
    t.region_start[5] = static_cast<uint16_t>(t.region_start[6] + 1); /* decreasing */
    OK(sat_entity_index2_validate(&t.index) == SAT_ERR_INVALID_ARG);
    Index u(8, 4, 6, 200, 1);
    u.region_start.back() = 199;
    OK(sat_entity_index2_validate(&u.index) == SAT_ERR_INVALID_ARG);
    Index v(8, 4, 6, 200, 1);
    v.region_start[0] = 1;
    OK(sat_entity_index2_validate(&v.index) == SAT_ERR_INVALID_ARG);

    sat_entity_index2_t c = good.index;
    c.region_shift = 2;
    OK(sat_entity_index2_validate(&c) == SAT_ERR_INVALID_ARG);
    c = good.index; c.region_shift = 16;
    OK(sat_entity_index2_validate(&c) == SAT_ERR_INVALID_ARG);
    c = good.index; c.region_cols = 0;
    OK(sat_entity_index2_validate(&c) == SAT_ERR_INVALID_ARG);
    c = good.index; c.descs = nullptr;
    OK(sat_entity_index2_validate(&c) == SAT_ERR_INVALID_ARG);

    OK(sat_entity_index2_bytes(1000, 200) == 8000u + 402u);
    OK(sat_entity_stream2_state_words(0) == 0 && sat_entity_stream2_state_words(1) == 2 && sat_entity_stream2_state_words(33) == 4);

    /* init refuses what it cannot run */
    std::vector<uint32_t> state(sat_entity_stream2_state_words(200));
    sat_entity_stream2_config_t cfg = {};
    cfg.index = &good.index;
    cfg.state = state.data();
    cfg.state_words = static_cast<uint32_t>(state.size());
    cfg.activate = on_activate;
    sat_entity_stream2_t s;
    OK(sat_entity_stream2_init(&s, &cfg) == SAT_OK);
    OK(sat_entity_stream2_init(nullptr, &cfg) == SAT_ERR_INVALID_ARG);
    sat_entity_stream2_config_t x = cfg; x.activate = nullptr;
    OK(sat_entity_stream2_init(&s, &x) == SAT_ERR_INVALID_ARG);
    x = cfg; x.state_words -= 1;
    OK(sat_entity_stream2_init(&s, &x) == SAT_ERR_INVALID_ARG);
    x = cfg; x.hysteresis = -1;
    OK(sat_entity_stream2_init(&s, &x) == SAT_ERR_INVALID_ARG);
    x = cfg; x.index = &bad.index;
    OK(sat_entity_stream2_init(&s, &x) == SAT_ERR_INVALID_ARG);
    x = cfg; x.state = nullptr;
    OK(sat_entity_stream2_init(&s, &x) == SAT_ERR_INVALID_ARG);
    const sat_box2_t inverted = {{0, 0}, {-1, 0}};
    OK(sat_entity_stream2_update(&s, &inverted, nullptr) == SAT_ERR_INVALID_ARG);
    OK(sat_entity_stream2_update(&s, nullptr, nullptr) == SAT_ERR_INVALID_ARG);
}

static void activation_basics() {
    Rig r(Index(40, 10, 6, 2500, 3));
    OK(sat_entity_stream2_active_count(&r.stream) == 0); /* init clears whatever the state held */
    const sat_box2_t b = box(100, 150, 420, 374);
    const sat_entity_stream2_result_t res = r.update(b);
    uint32_t expect = 0;
    std::vector<uint32_t> expect_list;
    for (uint32_t i = 0; i < r.idx.descs.size(); ++i)
        if (inside_px(r.idx, r.idx.descs[i], b)) { ++expect; expect_list.push_back(i); }
    OK(expect > 20);
    OK(res.activated == expect && res.deactivated == 0 && !res.deferred);
    OK(r.game.made == expect_list); /* exactly the descriptors in the box, in descriptor order */
    OK(ascending(r.game.made));
    for (uint32_t i : expect_list) OK(sat_entity_stream2_is_active(&r.stream, i));
    OK(sat_entity_stream2_active_count(&r.stream) == expect);
    /* nothing happens twice */
    const sat_entity_stream2_result_t again = r.update(b);
    OK(again.activated == 0 && again.deactivated == 0 && r.game.made.empty() && r.game.freed.empty());
    OK(!sat_entity_stream2_is_active(&r.stream, 99999) && !sat_entity_stream2_is_retired(&r.stream, 99999));
    const sat_entity_stream2_stats_t st = sat_entity_stream2_stats(&r.stream);
    OK(st.updates == 2 && st.activations == expect && st.deactivations == 0 && st.peak_active == expect);
}

static void follows_a_model() {
    /* every update must leave exactly: what is inside the box, plus what was live and is still inside
     * the box grown by the hysteresis */
    for (double hyst : {0.0, 24.0}) {
        Rig r(Index(60, 12, 5, 3000, 5), hyst);
        std::vector<uint8_t> active(r.idx.descs.size(), 0);
        uint32_t s = 4242;
        double cx = 0, cy = 300;
        uint32_t jumps = 0;
        for (int step = 0; step < 600; ++step) {
            s = s * 1664525u + 1013904223u;
            if ((s >> 28) == 0) { cx = static_cast<double>((s >> 8) % 2400) - 300; cy = static_cast<double>((s >> 4) % 480); ++jumps; }
            else { cx += static_cast<double>(static_cast<int>((s >> 8) & 0x3F) - 24); cy += static_cast<double>(static_cast<int>((s >> 16) & 0x1F) - 16); }
            const sat_box2_t b = box(cx - 160, cy - 112, cx + 160, cy + 112);
            r.update(b);
            for (uint32_t i = 0; i < active.size(); ++i) {
                const bool in = inside_px(r.idx, r.idx.descs[i], b);
                const bool keep = active[i] && inside_px(r.idx, r.idx.descs[i], b, hyst);
                active[i] = in || keep;
                OK((sat_entity_stream2_is_active(&r.stream, i) != 0) == (active[i] != 0));
                OK((r.game.model[i] != 0) == (active[i] != 0));
            }
            OK(ascending(r.game.made) && ascending(r.game.freed));
            uint32_t n = 0;
            for (uint8_t a : active) n += a;
            OK(sat_entity_stream2_active_count(&r.stream) == n && r.game.live.size() == n);
        }
        OK(jumps > 10);
        const sat_entity_stream2_stats_t st = sat_entity_stream2_stats(&r.stream);
        OK(st.activations - st.deactivations == sat_entity_stream2_active_count(&r.stream));
    }
}

static void hysteresis_stops_flicker() {
    /* one descriptor near the box's edge; the edge hovers back and forth by a few pixels */
    std::vector<sat_entity_desc2_t> d = {{300, 40, 0, 0}};
    std::vector<uint16_t> start = {0, 1};
    Index i(1, 1, 9, 0, 1, 0, 0);
    i.descs = d;
    i.region_start = start;
    i.index.descs = i.descs.data();
    i.index.region_start = i.region_start.data();
    i.index.desc_count = 1;
    for (double hyst : {0.0, 16.0}) {
        Rig r(i, hyst);
        r.update(box(0, 0, 301, 200)); /* inside by a pixel */
        OK(sat_entity_stream2_is_active(&r.stream, 0));
        int churn = 0;
        for (int k = 0; k < 20; ++k) {
            const double right = (k & 1) ? 296 : 305; /* the edge crosses the descriptor */
            const sat_entity_stream2_result_t res = r.update(box(0, 0, right, 200));
            churn += static_cast<int>(res.activated + res.deactivated);
        }
        if (hyst == 0.0) OK(churn >= 19);
        else OK(churn == 0);
        /* leaving by more than the margin does release it */
        r.update(box(0, 0, 200, 200));
        OK(!sat_entity_stream2_is_active(&r.stream, 0));
    }
}

static void reactivation() {
    Rig r(Index(20, 4, 6, 400, 8));
    const sat_box2_t near_box = box(-512, 100, -192, 324);
    const sat_box2_t far_box = box(5000, 100, 5320, 324);
    r.update(near_box);
    const std::vector<uint32_t> first = r.game.made;
    OK(!first.empty());
    r.update(far_box);
    OK(r.game.freed == first && sat_entity_stream2_active_count(&r.stream) == 0);
    r.update(near_box);
    OK(r.game.made == first); /* the same descriptors come back, in the same order, as new objects */
    const sat_entity_stream2_stats_t st = sat_entity_stream2_stats(&r.stream);
    OK(st.activations == 2 * first.size() && st.deactivations == first.size());
}

static void jump_across_regions() {
    Rig r(Index(80, 16, 5, 5000, 11));
    r.update(box(-512, 100, -192, 324));
    const uint32_t before = static_cast<uint32_t>(r.game.live.size());
    OK(before > 0);
    const sat_box2_t far_box = box(1800, 300, 2120, 524);
    const sat_entity_stream2_result_t res = r.update(far_box);
    OK(res.deactivated == before && res.activated > 0);
    for (uint32_t i : r.game.live) OK(inside_px(r.idx, r.idx.descs[i], far_box));
    uint32_t expect = 0;
    for (const sat_entity_desc2_t& d : r.idx.descs) expect += inside_px(r.idx, d, far_box);
    OK(res.activated == expect);
    /* a box over the whole grid wakes everything, one far outside wakes nothing */
    r.update(box(-30000, -30000, 30000, 30000));
    OK(sat_entity_stream2_active_count(&r.stream) == r.idx.descs.size());
    r.update(box(-30000, -30000, -29000, -29000));
    OK(sat_entity_stream2_active_count(&r.stream) == 0);
    r.update(box(30000, 30000, 31000, 31000));
    OK(sat_entity_stream2_active_count(&r.stream) == 0 && r.game.made.empty());
}

static void box_edges() {
    /* descriptors at known places against the half-open box [min, max) */
    std::vector<sat_entity_desc2_t> d = {{0, 0, 0, 0}, {10, 0, 0, 1}, {20, 0, 0, 2}, {0, 20, 0, 3}};
    std::vector<uint16_t> start = {0, 4};
    Index i(1, 1, 8, 0, 1, 100, 100);
    i.descs = d;
    i.region_start = start;
    i.index.descs = i.descs.data();
    i.index.region_start = i.region_start.data();
    i.index.desc_count = 4;
    Rig r(i);
    r.update(box(100, 100, 120, 120)); /* x 100..110 in, 120 out; y 100 in, 120 out */
    OK(r.game.made == (std::vector<uint32_t>{0, 1}));
    r.update(box(100.5, 100.5, 120.5, 120.5)); /* the one at the corner (100, 100) is out; (110,100) is out in y */
    OK(sat_entity_stream2_is_active(&r.stream, 0) == 0 && sat_entity_stream2_is_active(&r.stream, 1) == 0);
    Rig z(i);
    z.update(box(100, 100, 100, 100)); /* an empty box holds nothing */
    OK(z.game.made.empty());
    z.update(box(99.5, 99.5, 100.5, 100.5));
    OK(z.game.made == (std::vector<uint32_t>{0}));
}

static void pool_full() {
    Rig r(Index(20, 6, 6, 900, 21));
    r.game.capacity = 5;
    const sat_box2_t b = box(-512, 100, 300, 484);
    sat_entity_stream2_result_t res = r.update(b);
    OK(res.deferred && res.activated == 5 && r.game.live.size() == 5);
    std::vector<uint32_t> want;
    for (uint32_t i = 0; i < r.idx.descs.size(); ++i) if (inside_px(r.idx, r.idx.descs[i], b)) want.push_back(i);
    OK(want.size() > 10);
    OK(r.game.made == std::vector<uint32_t>(want.begin(), want.begin() + 5)); /* the first five in order */
    OK(sat_entity_stream2_stats(&r.stream).deferrals == 1);
    /* still full: nothing new, still deferred */
    res = r.update(b);
    OK(res.deferred && res.activated == 0);
    /* the game frees two (it destroyed them); the next two in order take their place */
    OK(sat_entity_stream2_release(&r.stream, want[1]) == SAT_OK && sat_entity_stream2_release(&r.stream, want[3]) == SAT_OK);
    r.game.model[want[1]] = r.game.model[want[3]] = 0;
    r.game.live.erase(std::find(r.game.live.begin(), r.game.live.end(), want[1]));
    r.game.live.erase(std::find(r.game.live.begin(), r.game.live.end(), want[3]));
    res = r.update(b);
    /* the freed ones are inside the box and lower than the rest, so they come back first */
    OK(res.activated == 2 && r.game.made == (std::vector<uint32_t>{want[1], want[3]}));
}

static void retire_and_decline() {
    Rig r(Index(20, 4, 6, 400, 33));
    const sat_box2_t b = box(-512, 100, 300, 360);
    r.update(b);
    OK(r.game.made.size() > 4);
    const uint32_t a = r.game.made[1], c = r.game.made[3];
    /* retiring a live descriptor releases it without a callback and keeps it out */
    OK(sat_entity_stream2_retire(&r.stream, a) == SAT_OK);
    r.game.model[a] = 0;
    r.game.live.erase(std::find(r.game.live.begin(), r.game.live.end(), a));
    OK(sat_entity_stream2_is_retired(&r.stream, a) && !sat_entity_stream2_is_active(&r.stream, a));
    r.update(b);
    OK(r.game.made.empty() && r.game.freed.empty());
    r.update(box(5000, 5000, 5100, 5100));
    r.update(b);
    OK(std::find(r.game.made.begin(), r.game.made.end(), a) == r.game.made.end()); /* it never comes back */
    OK(std::find(r.game.made.begin(), r.game.made.end(), c) != r.game.made.end());
    OK(sat_entity_stream2_unretire(&r.stream, a) == SAT_OK && !sat_entity_stream2_is_retired(&r.stream, a));
    r.update(b);
    OK(r.game.made == (std::vector<uint32_t>{a}));
    OK(sat_entity_stream2_retire(&r.stream, 99999) == SAT_ERR_INVALID_ARG);
    OK(sat_entity_stream2_release(&r.stream, 99999) == SAT_ERR_INVALID_ARG);
    OK(sat_entity_stream2_unretire(&r.stream, 99999) == SAT_ERR_INVALID_ARG);

    /* a callback can decline for good */
    Rig d(Index(20, 4, 6, 400, 33));
    d.update(b);
    const uint32_t victim = d.game.made[2];
    Rig e(Index(20, 4, 6, 400, 33));
    e.game.decline = {victim};
    e.update(b);
    OK(sat_entity_stream2_is_retired(&e.stream, victim) && !sat_entity_stream2_is_active(&e.stream, victim));
    OK(e.game.made.size() + 1 == d.game.made.size());
    OK(sat_entity_stream2_stats(&e.stream).declines == 1);
    e.game.decline.clear();
    e.update(box(5000, 5000, 5100, 5100));
    e.update(b);
    OK(!sat_entity_stream2_is_active(&e.stream, victim));

    /* a stage restart: everything inactive, nothing retired, no callbacks */
    sat_entity_stream2_reset(&e.stream);
    OK(sat_entity_stream2_active_count(&e.stream) == 0 && !sat_entity_stream2_is_retired(&e.stream, victim));
    e.game.model.assign(e.game.model.size(), 0);
    e.game.live.clear();
    e.update(b);
    OK(e.game.made == d.game.made);
}

static void callbacks_may_edit_state() {
    /* a deactivate callback that retires another live descriptor: the scan must not report it as freed */
    Rig r(Index(20, 4, 6, 400, 77));
    r.update(box(-512, 100, 300, 360));
    OK(r.game.live.size() > 6);
    const uint32_t other = r.game.live[r.game.live.size() - 2];
    r.game.retire_other_on_free = static_cast<int>(other);
    r.update(box(5000, 5000, 5100, 5100));
    OK(sat_entity_stream2_active_count(&r.stream) == 0 && r.game.live.empty());
    OK(sat_entity_stream2_is_retired(&r.stream, other));
    OK(std::find(r.game.freed.begin(), r.game.freed.end(), other) == r.game.freed.end() || r.game.freed.front() == other);
}

static void without_a_deactivate_callback() {
    Rig r(Index(20, 4, 6, 400, 5), 0, false);
    r.update(box(-512, 100, 300, 360));
    const uint32_t n = sat_entity_stream2_active_count(&r.stream);
    OK(n > 0);
    r.update(box(5000, 5000, 5100, 5100));
    OK(sat_entity_stream2_active_count(&r.stream) == 0); /* still tracked, just not announced */
}

static void empty_and_offset_grids() {
    Index none(4, 4, 6, 0, 1);
    OK(sat_entity_index2_validate(&none.index) == SAT_OK);
    Rig r(std::move(none));
    r.update(box(-1000, -1000, 1000, 1000));
    OK(r.game.made.empty());

    /* a grid far from the origin and a box straddling its corner */
    Rig s(Index(10, 10, 6, 800, 9, -20000, 15000));
    s.update(box(-20200, 14800, -19800, 15200));
    for (uint32_t i : s.game.made) OK(inside_px(s.idx, s.idx.descs[i], box(-20200, 14800, -19800, 15200)));
    uint32_t expect = 0;
    for (const sat_entity_desc2_t& d : s.idx.descs) expect += inside_px(s.idx, d, box(-20200, 14800, -19800, 15200));
    OK(expect > 0 && s.game.made.size() == expect);
}

/* The prediction margin comes from the follow camera's ACTIVATION range: a fast camera wakes what is
 * ahead of it sooner than a still one with the same margin. */
static void prediction_margin() {
    sat_follow_camera2d_config_t cc;
    sat_follow_camera2d_config_default(&cc);
    cc.activation_margin = PX(16);
    sat_follow_camera2d_config_t cp = cc;
    cp.predict_steps = 20;

    Index idx(100, 4, 6, 1500, 13, 0, 0);
    uint32_t woken[2] = {0, 0};
    for (int mode = 0; mode < 2; ++mode) {
        Rig r(idx);
        sat_follow_camera2d_t cam;
        sat_vec2_t centre = {PX(160), PX(112)};
        OK(sat_follow_camera2d_init(&cam, mode ? &cp : &cc, centre) == SAT_OK);
        for (int step = 0; step < 40; ++step) {
            sat_camera2d_t out;
            OK(sat_follow_camera2d_step(&cam, {PX(160 + step * 6), PX(112)}, nullptr, &out) == SAT_OK);
            sat_box2_t act;
            OK(sat_follow_camera2d_range(&cam, SAT_CAMERA_RANGE_ACTIVATION, &act) == SAT_OK);
            r.update(act);
        }
        woken[mode] = sat_entity_stream2_stats(&r.stream).activations;
        /* what is ahead of a camera heading right is alive early, what is behind it is not extended */
        const double view_right = D(cam.centre.x) + 160;
        bool ahead = false;
        for (uint32_t i : r.game.live) ahead = ahead || r.idx.descs[i].x > view_right + 20;
        OK(ahead == (mode == 1));
    }
    OK(woken[1] > woken[0]);
}

int main() {
    validation();
    activation_basics();
    follows_a_model();
    hysteresis_stops_flicker();
    reactivation();
    jump_across_regions();
    box_edges();
    pool_full();
    retire_and_decline();
    callbacks_may_edit_state();
    without_a_deactivate_callback();
    empty_and_offset_grids();
    prediction_margin();
    std::puts("entity_stream2: ok");
    return 0;
}
