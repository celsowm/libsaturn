#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "saturn/physics2_world.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

static sat_fx16_t F(int px) { return static_cast<sat_fx16_t>(px * SAT_FX16_ONE); }
static sat_box2_t B(int cx, int cy, int hx, int hy) { return {{F(cx), F(cy)}, {F(hx), F(hy)}}; }
static sat_vec2_t V(int x, int y) { return {F(x), F(y)}; }

/* A world with room for 16 colliders, 32 pairs and a 16 x 16 broadphase of 32 px cells. */
struct Fixture {
    static constexpr int kSlots = 16, kPairs = 32, kCells = 16, kEntries = 128;
    sat_collider2_slot_t slots[kSlots];
    sat_physics2_pair_t pairs[kPairs], next_pairs[kPairs];
    uint16_t candidates[kSlots];
    uint16_t heads[kCells * kCells];
    sat_spatial_entry_t entries[kEntries];
    uint16_t stamps[kSlots];
    sat_box2_t items[kSlots];
    sat_spatial_t spatial;
    sat_physics2_world_t world;

    explicit Fixture(uint16_t entry_cap = kEntries, uint16_t pair_cap = kPairs) {
        OK(sat_spatial_init(&spatial, heads, kCells, kCells, 5, entries, entry_cap, stamps, items, kSlots) == SAT_OK);
        sat_physics2_storage_t st = {};
        st.slots = slots;
        st.slot_cap = kSlots;
        st.pairs = pairs;
        st.next_pairs = next_pairs;
        st.pair_cap = pair_cap;
        st.candidates = candidates;
        st.spatial = &spatial;
        OK(sat_physics2_world_init(&world, &st) == SAT_OK);
    }
    sat_collider2_t add(sat_collider2_kind_t kind, const sat_box2_t& box, uint16_t category = 1, uint16_t mask = 0xFFFF,
                        uint8_t flags = 0, uint8_t face = 0) {
        sat_collider2_desc_t d;
        sat_collider2_desc_init(&d, kind, &box);
        d.category = category;
        d.mask = mask;
        d.flags = flags;
        d.one_way_face = face;
        sat_collider2_t id = 0;
        OK(sat_physics2_add(&world, &d, &id) == SAT_OK);
        OK(id != SAT_COLLIDER2_NONE);
        return id;
    }
    std::vector<sat_physics2_event_t> step(sat_physics2_step_result_t* res = nullptr, sat_result_t expect = SAT_OK) {
        sat_physics2_event_t ev[64];
        sat_physics2_step_result_t r = {};
        OK(sat_physics2_step(&world, ev, 64, &r) == expect);
        if (res) *res = r;
        return std::vector<sat_physics2_event_t>(ev, ev + r.events);
    }
};

static bool is_event(const sat_physics2_event_t& e, sat_collider2_t sensor, sat_collider2_t other, int type, int flags = 0) {
    return e.sensor == sensor && e.other == other && e.type == type && e.flags == flags;
}

static void init_and_requirements() {
    Fixture f;
    OK(sat_physics2_requirements(16, 32) ==
       16 * sizeof(sat_collider2_slot_t) + 2 * 32 * sizeof(sat_physics2_pair_t) + 16 * sizeof(uint16_t));

    sat_physics2_world_t w;
    sat_physics2_storage_t st = {};
    OK(sat_physics2_world_init(nullptr, &st) == SAT_ERR_INVALID_ARG);
    OK(sat_physics2_world_init(&w, nullptr) == SAT_ERR_INVALID_ARG);
    OK(sat_physics2_world_init(&w, &st) == SAT_ERR_INVALID_ARG); /* nothing provided */
    st.slots = f.slots;
    st.slot_cap = Fixture::kSlots;
    st.pairs = f.pairs;
    st.next_pairs = f.next_pairs;
    st.pair_cap = 8;
    st.candidates = f.candidates;
    st.spatial = &f.spatial;
    OK(sat_physics2_world_init(&w, &st) == SAT_OK);
    st.slot_cap = Fixture::kSlots + 1; /* the broadphase cannot name that many */
    OK(sat_physics2_world_init(&w, &st) == SAT_ERR_INVALID_ARG);
    st.slot_cap = 0;
    OK(sat_physics2_world_init(&w, &st) == SAT_ERR_INVALID_ARG);
}

static void handles_and_capacity() {
    Fixture f;
    const sat_collider2_t a = f.add(SAT_COLLIDER2_STATIC, B(10, 10, 4, 4));
    const sat_collider2_t b = f.add(SAT_COLLIDER2_KINEMATIC, B(50, 10, 4, 4));
    OK(a != b && sat_physics2_is_valid(&f.world, a) && sat_physics2_is_valid(&f.world, b));
    OK(!sat_physics2_is_valid(&f.world, SAT_COLLIDER2_NONE));
    OK(!sat_physics2_is_valid(&f.world, 0x7FFF0001u)); /* a slot that was never used */
    OK(!sat_physics2_is_valid(nullptr, a));

    sat_collider2_desc_t d;
    OK(sat_physics2_get(&f.world, a, &d) == SAT_OK && d.kind == SAT_COLLIDER2_STATIC && d.box.center.x == F(10));
    OK(sat_physics2_get(&f.world, a, nullptr) == SAT_ERR_INVALID_ARG);

    /* removal makes the handle permanently stale, even once its slot is reused */
    OK(sat_physics2_remove(&f.world, a) == SAT_OK);
    OK(!sat_physics2_is_valid(&f.world, a));
    OK(sat_physics2_remove(&f.world, a) == SAT_ERR_NOT_FOUND);
    OK(sat_physics2_get(&f.world, a, &d) == SAT_ERR_NOT_FOUND);
    const sat_collider2_t c = f.add(SAT_COLLIDER2_STATIC, B(20, 20, 4, 4));
    OK(c != a && (c >> 16) == (a >> 16)); /* same slot, new generation */
    OK(!sat_physics2_is_valid(&f.world, a) && sat_physics2_is_valid(&f.world, c));

    /* capacity */
    for (int i = 0; i < Fixture::kSlots - 2; ++i) f.add(SAT_COLLIDER2_STATIC, B(100 + i * 10, 100, 2, 2));
    sat_collider2_desc_t extra;
    const sat_box2_t box = B(0, 0, 1, 1);
    sat_collider2_desc_init(&extra, SAT_COLLIDER2_STATIC, &box);
    sat_collider2_t id = 0;
    OK(sat_physics2_add(&f.world, &extra, &id) == SAT_ERR_CAPACITY);
    OK(sat_physics2_remove(&f.world, b) == SAT_OK);
    OK(sat_physics2_add(&f.world, &extra, &id) == SAT_OK && sat_physics2_is_valid(&f.world, id));

    /* invalid descriptors */
    sat_collider2_desc_t bad = extra;
    bad.kind = static_cast<sat_collider2_kind_t>(0);
    OK(sat_physics2_add(&f.world, &bad, &id) == SAT_ERR_INVALID_ARG);
    bad = extra;
    bad.box.half.x = -1;
    OK(sat_physics2_add(&f.world, &bad, &id) == SAT_ERR_INVALID_ARG);
    bad = extra;
    bad.one_way_face = 4;
    OK(sat_physics2_add(&f.world, &bad, &id) == SAT_ERR_INVALID_ARG);
    OK(sat_physics2_add(&f.world, nullptr, &id) == SAT_ERR_INVALID_ARG);

    /* reset invalidates everything */
    sat_physics2_world_reset(&f.world);
    OK(!sat_physics2_is_valid(&f.world, c) && !sat_physics2_is_valid(&f.world, id));
    OK(f.world.count == 0);
    f.add(SAT_COLLIDER2_STATIC, B(0, 0, 1, 1));
}

static void moving_and_static_setters() {
    Fixture f;
    const sat_collider2_t wall = f.add(SAT_COLLIDER2_STATIC, B(10, 10, 4, 4));
    const sat_collider2_t plat = f.add(SAT_COLLIDER2_KINEMATIC, B(50, 10, 8, 2));
    const sat_collider2_t sens = f.add(SAT_COLLIDER2_SENSOR, B(90, 10, 8, 8));
    OK(sat_physics2_set_center(&f.world, wall, V(11, 10)) == SAT_ERR_INVALID_ARG);
    const sat_box2_t b = B(0, 0, 1, 1);
    OK(sat_physics2_set_box(&f.world, wall, &b) == SAT_ERR_INVALID_ARG);
    OK(sat_physics2_set_center(&f.world, plat, V(53, 12)) == SAT_OK);
    OK(sat_physics2_set_center(&f.world, sens, V(91, 10)) == SAT_OK);
    OK(sat_physics2_set_center(&f.world, 0x00050001u, V(0, 0)) == SAT_ERR_NOT_FOUND);

    sat_vec2_t d;
    OK(sat_physics2_delta(&f.world, plat, &d) == SAT_OK && d.x == F(3) && d.y == F(2));
    OK(sat_physics2_delta(&f.world, wall, &d) == SAT_OK && d.x == 0 && d.y == 0);
    OK(sat_physics2_delta(&f.world, plat, nullptr) == SAT_ERR_INVALID_ARG);
    /* two moves in one tick accumulate from the committed position */
    OK(sat_physics2_set_center(&f.world, plat, V(56, 14)) == SAT_OK);
    OK(sat_physics2_delta(&f.world, plat, &d) == SAT_OK && d.x == F(6) && d.y == F(4));
    f.step();
    OK(sat_physics2_delta(&f.world, plat, &d) == SAT_OK && d.x == 0 && d.y == 0); /* the step committed it */

    const sat_box2_t big = B(56, 14, 8, 4);
    OK(sat_physics2_set_box(&f.world, plat, &big) == SAT_OK);
    sat_collider2_desc_t desc;
    OK(sat_physics2_get(&f.world, plat, &desc) == SAT_OK && desc.box.half.y == F(4));
    const sat_box2_t neg = {{0, 0}, {-1, 0}};
    OK(sat_physics2_set_box(&f.world, plat, &neg) == SAT_ERR_INVALID_ARG);
}

static void overlap_queries() {
    Fixture f;
    const sat_collider2_t a = f.add(SAT_COLLIDER2_STATIC, B(40, 40, 10, 10), 0x1);
    const sat_collider2_t b = f.add(SAT_COLLIDER2_KINEMATIC, B(60, 40, 10, 10), 0x2);
    const sat_collider2_t s = f.add(SAT_COLLIDER2_SENSOR, B(50, 40, 30, 30), 0x4);
    const sat_collider2_t far = f.add(SAT_COLLIDER2_STATIC, B(300, 300, 10, 10), 0x1);

    sat_collider2_t out[8];
    uint16_t n = 0;
    const sat_box2_t q = B(50, 40, 5, 5);
    OK(sat_physics2_query(&f.world, &q, SAT_COLLIDER2_ALL_KINDS, 0xFFFF, out, 8, &n) == SAT_OK);
    OK(n == 3 && out[0] == a && out[1] == b && out[2] == s); /* by handle */
    OK(sat_physics2_query(&f.world, &q, SAT_COLLIDER2_SOLID_KINDS, 0xFFFF, out, 8, &n) == SAT_OK && n == 2);
    OK(sat_physics2_query(&f.world, &q, SAT_COLLIDER2_KIND_BIT(SAT_COLLIDER2_SENSOR), 0xFFFF, out, 8, &n) == SAT_OK && n == 1 && out[0] == s);
    OK(sat_physics2_query(&f.world, &q, SAT_COLLIDER2_ALL_KINDS, 0x2, out, 8, &n) == SAT_OK && n == 1 && out[0] == b);
    OK(sat_physics2_query(&f.world, &q, SAT_COLLIDER2_ALL_KINDS, 0x8, out, 8, &n) == SAT_OK && n == 0);

    /* a box that only touches does not overlap */
    const sat_box2_t touch = B(40, 20, 10, 10); /* bottom edge at y = 30 = a's top */
    OK(sat_physics2_query(&f.world, &touch, SAT_COLLIDER2_KIND_BIT(SAT_COLLIDER2_STATIC), 0xFFFF, out, 8, &n) == SAT_OK && n == 0);

    OK(sat_physics2_query(&f.world, &q, SAT_COLLIDER2_ALL_KINDS, 0xFFFF, out, 2, &n) == SAT_ERR_CAPACITY && n == 2);
    OK(out[0] == a && out[1] == b);
    OK(sat_physics2_query(&f.world, &q, SAT_COLLIDER2_ALL_KINDS, 0xFFFF, nullptr, 0, &n) == SAT_ERR_CAPACITY && n == 0);

    /* a removed collider disappears from queries; a moved one is found at its new place */
    OK(sat_physics2_remove(&f.world, a) == SAT_OK);
    OK(sat_physics2_query(&f.world, &q, SAT_COLLIDER2_SOLID_KINDS, 0xFFFF, out, 8, &n) == SAT_OK && n == 1 && out[0] == b);
    OK(sat_physics2_set_center(&f.world, b, V(300, 300)) == SAT_OK);
    OK(sat_physics2_query(&f.world, &q, SAT_COLLIDER2_SOLID_KINDS, 0xFFFF, out, 8, &n) == SAT_OK && n == 0);
    const sat_box2_t qfar = B(300, 300, 5, 5);
    OK(sat_physics2_query(&f.world, &qfar, SAT_COLLIDER2_SOLID_KINDS, 0xFFFF, out, 8, &n) == SAT_OK && n == 2);
    OK(out[0] == b && out[1] == far); /* handle order: b's slot (1) before far's (3) */

    OK(sat_physics2_query(&f.world, nullptr, 7, 1, out, 8, &n) == SAT_ERR_INVALID_ARG);
    OK(sat_physics2_query(&f.world, &q, 7, 1, out, 8, nullptr) == SAT_ERR_INVALID_ARG);
}

static sat_physics2_support_query_t foot(int x, int y, int half = 5, int reach = 8, int embed = 2) {
    sat_physics2_support_query_t q = {};
    q.origin = V(x, y);
    q.half_width = F(half);
    q.down_quadrant = 1;
    q.reach = static_cast<uint8_t>(reach);
    q.embed = static_cast<uint8_t>(embed);
    q.category_mask = 0xFFFF;
    return q;
}

static sat_result_t support(Fixture& f, const sat_physics2_support_query_t& q, sat_physics2_support_t* s) {
    return sat_physics2_find_support(&f.world, &q, s);
}

static void support_queries() {
    Fixture f;
    const sat_collider2_t floor = f.add(SAT_COLLIDER2_STATIC, B(100, 120, 60, 10)); /* top at y = 110, x 40..160 */
    sat_physics2_support_t s;

    /* distance window [-embed, reach) */
    OK(support(f, foot(100, 110), &s) == SAT_OK);
    OK(s.id == floor && s.distance == 0 && s.point.y == F(110) && s.point.x == F(100));
    OK(s.normal.x == 0 && s.normal.y == -SAT_FX16_ONE && s.delta.x == 0 && s.delta.y == 0);
    OK(support(f, foot(100, 103), &s) == SAT_OK && s.distance == F(7));
    OK(support(f, foot(100, 102), &s) == SAT_ERR_NOT_FOUND); /* exactly `reach` away */
    OK(support(f, foot(100, 112), &s) == SAT_OK && s.distance == F(-2) && s.point.y == F(110));
    OK(support(f, foot(100, 113), &s) == SAT_ERR_NOT_FOUND); /* deeper than `embed` */

    /* across the feet: any overlap counts, a mere touch does not */
    OK(support(f, foot(43, 110), &s) == SAT_OK); /* feet cover x 38..48 */
    OK(support(f, foot(35, 110), &s) == SAT_ERR_NOT_FOUND); /* x 30..40 touches the edge at 40 */
    OK(support(f, foot(163, 110), &s) == SAT_OK);
    OK(support(f, foot(165, 110), &s) == SAT_ERR_NOT_FOUND);

    /* a sub-pixel origin keeps its fraction in the distance */
    sat_physics2_support_query_t q = foot(100, 108);
    q.origin.y += F(1) / 2;
    OK(support(f, q, &s) == SAT_OK && s.distance == F(3) / 2 && s.point.y == F(110));

    /* nearest face wins; equal faces go to the lowest handle */
    const sat_collider2_t low = f.add(SAT_COLLIDER2_STATIC, B(100, 112, 20, 2)); /* top at y = 110 too */
    OK(support(f, foot(100, 108), &s) == SAT_OK && s.id == floor);
    OK(sat_physics2_remove(&f.world, floor) == SAT_OK);
    OK(support(f, foot(100, 108), &s) == SAT_OK && s.id == low);
    const sat_collider2_t step = f.add(SAT_COLLIDER2_STATIC, B(100, 114, 20, 2)); /* a lower surface at y = 112 */
    OK(support(f, foot(100, 108), &s) == SAT_OK && s.id == low);
    OK(sat_physics2_remove(&f.world, low) == SAT_OK);
    OK(support(f, foot(100, 108), &s) == SAT_OK && s.id == step && s.distance == F(4));

    /* sensors never support; category filters */
    f.add(SAT_COLLIDER2_SENSOR, B(100, 109, 20, 1));
    OK(support(f, foot(100, 107), &s) == SAT_OK && s.id == step);
    sat_physics2_support_query_t cq = foot(100, 108);
    cq.category_mask = 0x2;
    OK(support(f, cq, &s) == SAT_ERR_NOT_FOUND);
    OK(sat_physics2_remove(&f.world, step) == SAT_OK);
    OK(support(f, foot(100, 110), &s) == SAT_ERR_NOT_FOUND);

    /* arguments */
    sat_physics2_support_query_t bad = foot(0, 0);
    OK(sat_physics2_find_support(&f.world, &bad, nullptr) == SAT_ERR_INVALID_ARG);
    OK(sat_physics2_find_support(&f.world, nullptr, &s) == SAT_ERR_INVALID_ARG);
    bad.down_quadrant = 4;
    OK(sat_physics2_find_support(&f.world, &bad, &s) == SAT_ERR_INVALID_ARG);
    bad = foot(0, 0);
    bad.half_width = -1;
    OK(sat_physics2_find_support(&f.world, &bad, &s) == SAT_ERR_INVALID_ARG);
}

static void support_in_every_direction() {
    Fixture f;
    /* a room: floor, ceiling, left wall and right wall; each is "down" for some quadrant */
    const sat_collider2_t floor = f.add(SAT_COLLIDER2_STATIC, B(100, 160, 100, 10)); /* top 150 */
    const sat_collider2_t ceil = f.add(SAT_COLLIDER2_STATIC, B(100, 40, 100, 10));   /* bottom 50 */
    const sat_collider2_t left = f.add(SAT_COLLIDER2_STATIC, B(10, 100, 10, 100));   /* right face 20 */
    const sat_collider2_t right = f.add(SAT_COLLIDER2_STATIC, B(190, 100, 10, 100)); /* left face 180 */
    sat_physics2_support_t s;
    sat_physics2_support_query_t q = foot(100, 100, 5, 60, 0);
    q.down_quadrant = 1;
    OK(support(f, q, &s) == SAT_OK && s.id == floor && s.distance == F(50) && s.normal.y == -SAT_FX16_ONE);
    q.down_quadrant = 3;
    OK(support(f, q, &s) == SAT_OK && s.id == ceil && s.distance == F(50) && s.normal.y == SAT_FX16_ONE && s.point.y == F(50));
    q.down_quadrant = 2;
    OK(support(f, q, &s) == SAT_ERR_NOT_FOUND); /* 80 px away, reach is 60 */
    q.reach = 100;
    OK(support(f, q, &s) == SAT_OK && s.id == left && s.distance == F(80) && s.normal.x == SAT_FX16_ONE && s.point.x == F(20));
    q.down_quadrant = 0;
    OK(support(f, q, &s) == SAT_OK && s.id == right && s.distance == F(80) && s.normal.x == -SAT_FX16_ONE && s.point.x == F(180));
}

static void one_way_supports() {
    Fixture f;
    /* a jump-through platform: solid on its top face only */
    const sat_collider2_t plat = f.add(SAT_COLLIDER2_STATIC, B(100, 100, 30, 3), 1, 0xFFFF, SAT_COLLIDER2_ONE_WAY, 3);
    sat_physics2_support_t s;
    OK(support(f, foot(100, 97), &s) == SAT_OK && s.id == plat && s.distance == 0 && s.normal.y == -SAT_FX16_ONE);
    OK(support(f, foot(100, 95), &s) == SAT_OK && s.distance == F(2));
    OK(support(f, foot(100, 99), &s) == SAT_OK && s.distance == F(-2)); /* just sunk into it: still lands */
    OK(support(f, foot(100, 100), &s) == SAT_ERR_NOT_FOUND); /* deeper than embed: passing up through */
    /* probed along another direction its solid face is not the one that faces the probe */
    sat_physics2_support_query_t up = foot(100, 110);
    up.down_quadrant = 3;
    OK(support(f, up, &s) == SAT_ERR_NOT_FOUND); /* from below, going up: not solid that way */
    /* a ceiling-walker (down = up the screen) lands on a one-way whose solid face points down */
    const sat_collider2_t under = f.add(SAT_COLLIDER2_STATIC, B(100, 50, 30, 3), 1, 0xFFFF, SAT_COLLIDER2_ONE_WAY, 1);
    sat_physics2_support_query_t inv = foot(100, 54);
    inv.down_quadrant = 3;
    OK(support(f, inv, &s) == SAT_OK && s.id == under && s.normal.y == SAT_FX16_ONE);
    OK(support(f, foot(100, 47), &s) == SAT_ERR_NOT_FOUND); /* from above onto its non-solid face */
}

/* A character-sized foot ridden by a platform that moves every tick: the game's loop. */
struct Rider {
    sat_vec2_t position;
    sat_collider2_t support;
};

/* Moves the platform, re-tests the support, carries the rider, commits the tick. */
static bool ride(Fixture& f, Rider& r, sat_vec2_t platform_to, sat_collider2_t plat, int reach = 8, int embed = 2) {
    OK(sat_physics2_set_center(&f.world, plat, platform_to) == SAT_OK);
    sat_physics2_support_query_t q = foot(0, 0, 5, reach, embed);
    q.origin = r.position;
    sat_physics2_support_t s;
    bool supported = false;
    if (sat_physics2_find_support(&f.world, &q, &s) == SAT_OK) {
        r.support = s.id;
        OK(sat_physics2_carry(&f.world, s.id, &r.position) == SAT_OK);
        r.position.y = s.point.y; /* snapped onto the face the platform moved to */
        supported = true;
    } else {
        r.support = SAT_COLLIDER2_NONE;
    }
    f.step();
    return supported;
}

static void horizontal_platform_carries() {
    Fixture f;
    const sat_collider2_t plat = f.add(SAT_COLLIDER2_KINEMATIC, B(100, 120, 20, 4)); /* top 116 */
    Rider r = {V(100, 116), SAT_COLLIDER2_NONE};
    for (int i = 1; i <= 20; ++i) {
        OK(ride(f, r, V(100 + 3 * i, 120), plat));
        OK(r.support == plat);
        OK(r.position.x == F(100 + 3 * i) && r.position.y == F(116));
    }
    /* the platform stops: the rider stays where it is on it */
    for (int i = 0; i < 5; ++i) OK(ride(f, r, V(160, 120), plat));
    OK(r.position.x == F(160));
    sat_vec2_t v;
    OK(sat_physics2_launch_velocity(&f.world, plat, &v) == SAT_OK && v.x == 0 && v.y == 0);
}

static void vertical_platform_and_detach() {
    Fixture f;
    sat_collider2_desc_t d;
    const sat_box2_t box = B(100, 100, 20, 4); /* top 96 */
    sat_collider2_desc_init(&d, SAT_COLLIDER2_KINEMATIC, &box);
    d.launch_scale = SAT_FX16_ONE / 2;
    sat_collider2_t plat = 0;
    OK(sat_physics2_add(&f.world, &d, &plat) == SAT_OK);
    Rider r = {V(100, 96), SAT_COLLIDER2_NONE};

    /* moving down 6 px a tick stays inside reach 8: support and carry hold */
    for (int i = 1; i <= 10; ++i) {
        OK(ride(f, r, V(100, 100 + 6 * i), plat));
        OK(r.position.y == F(96 + 6 * i));
    }
    /* moving up 3 px a tick is lifted by the carry: the face is found 1 px inside the feet's embed */
    for (int i = 1; i <= 10; ++i) {
        OK(ride(f, r, V(100, 160 - 3 * i), plat, 8, 4));
        OK(r.position.y == F(156 - 3 * i));
    }
    OK(r.position.y == F(126));

    /* falling away faster than reach: the support is lost and the rider leaves with half the motion */
    OK(sat_physics2_set_center(&f.world, plat, V(106, 130 + 12)) == SAT_OK); /* +6 right, +12 down from (100, 130) */
    sat_physics2_support_t s;
    sat_physics2_support_query_t q = foot(0, 0, 5, 8, 2);
    q.origin = r.position;
    OK(sat_physics2_find_support(&f.world, &q, &s) == SAT_ERR_NOT_FOUND);
    sat_vec2_t launch;
    OK(sat_physics2_launch_velocity(&f.world, plat, &launch) == SAT_OK);
    OK(launch.x == F(3) && launch.y == F(6)); /* delta (6, 12) * 0.5 */
    OK(sat_physics2_launch_velocity(&f.world, plat, nullptr) == SAT_ERR_INVALID_ARG);
}

static void one_way_moving_platform() {
    Fixture f;
    const sat_collider2_t plat = f.add(SAT_COLLIDER2_KINEMATIC, B(100, 140, 20, 3), 1, 0xFFFF, SAT_COLLIDER2_ONE_WAY, 3); /* top 137 */
    Rider r = {V(100, 137), SAT_COLLIDER2_NONE};
    /* carried up by a rising platform */
    for (int i = 1; i <= 10; ++i) {
        OK(ride(f, r, V(100, 140 - 2 * i), plat, 8, 4));
        OK(r.position.y == F(137 - 2 * i));
    }
    OK(r.position.y == F(117));

    /* a character below it is not caught; once above it, it is */
    sat_physics2_support_t s;
    OK(support(f, foot(100, 125, 5, 8, 2), &s) == SAT_ERR_NOT_FOUND); /* 8 px under the top (117) */
    OK(support(f, foot(100, 112, 5, 8, 2), &s) == SAT_OK && s.id == plat && s.distance == F(5));
}

static void stale_support_handles() {
    Fixture f;
    const sat_collider2_t plat = f.add(SAT_COLLIDER2_KINEMATIC, B(100, 120, 20, 4));
    Rider r = {V(100, 116), SAT_COLLIDER2_NONE};
    OK(ride(f, r, V(103, 120), plat));
    const sat_collider2_t held = r.support;
    OK(sat_physics2_remove(&f.world, plat) == SAT_OK);

    sat_vec2_t p = V(0, 0), v;
    OK(sat_physics2_carry(&f.world, held, &p) == SAT_ERR_NOT_FOUND && p.x == 0);
    OK(sat_physics2_launch_velocity(&f.world, held, &v) == SAT_ERR_NOT_FOUND);
    sat_physics2_support_t s;
    OK(support(f, foot(103, 116), &s) == SAT_ERR_NOT_FOUND);

    /* a new collider in the old slot is a different collider */
    const sat_collider2_t other = f.add(SAT_COLLIDER2_KINEMATIC, B(100, 120, 20, 4));
    OK(other != held);
    OK(sat_physics2_carry(&f.world, held, &p) == SAT_ERR_NOT_FOUND);
    OK(sat_physics2_carry(&f.world, other, &p) == SAT_OK);
    OK(sat_physics2_carry(&f.world, other, nullptr) == SAT_ERR_INVALID_ARG);
}

static void sensor_enter_stay_leave() {
    Fixture f;
    sat_physics2_set_emit_stay(&f.world, 1);
    const sat_collider2_t sensor = f.add(SAT_COLLIDER2_SENSOR, B(100, 100, 20, 20), 1, 0x2);
    const sat_collider2_t body = f.add(SAT_COLLIDER2_KINEMATIC, B(40, 100, 5, 5), 0x2);
    f.add(SAT_COLLIDER2_KINEMATIC, B(100, 100, 5, 5), 0x4); /* inside, but filtered out by the mask */
    OK(f.step().empty());

    /* approach: touching is not overlapping */
    OK(sat_physics2_set_center(&f.world, body, V(75, 100)) == SAT_OK); /* body right edge at 80 = sensor left edge */
    OK(f.step().empty());
    OK(sat_physics2_set_center(&f.world, body, V(78, 100)) == SAT_OK);
    std::vector<sat_physics2_event_t> ev = f.step();
    OK(ev.size() == 1 && is_event(ev[0], sensor, body, SAT_PHYSICS2_EVENT_ENTER));
    OK(sat_physics2_set_center(&f.world, body, V(90, 100)) == SAT_OK);
    ev = f.step();
    OK(ev.size() == 1 && is_event(ev[0], sensor, body, SAT_PHYSICS2_EVENT_STAY));
    sat_physics2_set_emit_stay(&f.world, 0);
    OK(sat_physics2_set_center(&f.world, body, V(100, 100)) == SAT_OK);
    OK(f.step().empty()); /* stay is off */
    OK(sat_physics2_set_center(&f.world, body, V(130, 100)) == SAT_OK); /* left edge 125: out */
    ev = f.step();
    OK(ev.size() == 1 && is_event(ev[0], sensor, body, SAT_PHYSICS2_EVENT_LEAVE));
    OK(f.step().empty());

    /* a moving sensor sweeps up a standing body */
    OK(sat_physics2_set_center(&f.world, sensor, V(125, 100)) == SAT_OK);
    ev = f.step();
    OK(ev.size() == 1 && is_event(ev[0], sensor, body, SAT_PHYSICS2_EVENT_ENTER));
    OK(f.world.pair_count == 1);
}

static void sensor_removal_while_overlapping() {
    Fixture f;
    const sat_collider2_t sensor = f.add(SAT_COLLIDER2_SENSOR, B(100, 100, 20, 20));
    const sat_collider2_t a = f.add(SAT_COLLIDER2_KINEMATIC, B(100, 100, 5, 5));
    const sat_collider2_t b = f.add(SAT_COLLIDER2_STATIC, B(110, 100, 5, 5));
    std::vector<sat_physics2_event_t> ev = f.step();
    OK(ev.size() == 2 && is_event(ev[0], sensor, a, SAT_PHYSICS2_EVENT_ENTER) && is_event(ev[1], sensor, b, SAT_PHYSICS2_EVENT_ENTER));

    OK(sat_physics2_remove(&f.world, a) == SAT_OK);
    const sat_collider2_t c = f.add(SAT_COLLIDER2_KINEMATIC, B(100, 100, 5, 5)); /* reuses a's slot, overlaps too */
    OK(c != a);
    ev = f.step();
    /* the old pair leaves flagged as removed; the new collider is a fresh ENTER */
    OK(ev.size() == 2);
    OK(is_event(ev[0], sensor, a, SAT_PHYSICS2_EVENT_LEAVE, SAT_PHYSICS2_EVENT_REMOVED) ||
       is_event(ev[1], sensor, a, SAT_PHYSICS2_EVENT_LEAVE, SAT_PHYSICS2_EVENT_REMOVED));
    OK(is_event(ev[0], sensor, c, SAT_PHYSICS2_EVENT_ENTER) || is_event(ev[1], sensor, c, SAT_PHYSICS2_EVENT_ENTER));
    OK(f.world.pair_count == 2);

    /* removing the sensor ends all of its pairs */
    OK(sat_physics2_remove(&f.world, sensor) == SAT_OK);
    ev = f.step();
    OK(ev.size() == 2);
    for (const auto& e : ev) OK(e.sensor == sensor && e.type == SAT_PHYSICS2_EVENT_LEAVE && e.flags == SAT_PHYSICS2_EVENT_REMOVED);
    OK(f.world.pair_count == 0);
    OK(f.step().empty());

    /* removing the sensor and the body in the same tick */
    const sat_collider2_t s2 = f.add(SAT_COLLIDER2_SENSOR, B(50, 50, 10, 10));
    const sat_collider2_t b2 = f.add(SAT_COLLIDER2_STATIC, B(50, 50, 2, 2));
    OK(f.step().size() == 1);
    OK(sat_physics2_remove(&f.world, b2) == SAT_OK && sat_physics2_remove(&f.world, s2) == SAT_OK);
    ev = f.step();
    OK(ev.size() == 1 && ev[0].flags == SAT_PHYSICS2_EVENT_REMOVED && ev[0].type == SAT_PHYSICS2_EVENT_LEAVE);
}

static void sensor_high_speed_crossing() {
    Fixture f;
    const sat_collider2_t sensor = f.add(SAT_COLLIDER2_SENSOR, B(200, 100, 8, 8)); /* y 92..108 */
    const sat_collider2_t bullet = f.add(SAT_COLLIDER2_KINEMATIC, B(100, 100, 2, 2));
    OK(f.step().empty());
    /* 200 px in one tick: it starts before the sensor and ends past it */
    OK(sat_physics2_set_center(&f.world, bullet, V(300, 100)) == SAT_OK);
    std::vector<sat_physics2_event_t> ev = f.step();
    OK(ev.size() == 2);
    OK(is_event(ev[0], sensor, bullet, SAT_PHYSICS2_EVENT_ENTER, SAT_PHYSICS2_EVENT_CROSSING));
    OK(is_event(ev[1], sensor, bullet, SAT_PHYSICS2_EVENT_LEAVE, SAT_PHYSICS2_EVENT_CROSSING));
    OK(f.world.pair_count == 0);

    /* a miss: clear of the sensor's top edge by a pixel, then grazing it exactly */
    OK(sat_physics2_set_center(&f.world, bullet, V(100, 89)) == SAT_OK); /* bottom edge 91 < 92 */
    f.step();
    OK(sat_physics2_set_center(&f.world, bullet, V(300, 89)) == SAT_OK);
    OK(f.step().empty());
    OK(sat_physics2_set_center(&f.world, bullet, V(100, 90)) == SAT_OK); /* bottom edge 92: touches the top edge */
    f.step();
    OK(sat_physics2_set_center(&f.world, bullet, V(300, 90)) == SAT_OK);
    OK(f.step().empty());
    /* one pixel lower it crosses */
    OK(sat_physics2_set_center(&f.world, bullet, V(100, 91)) == SAT_OK);
    f.step();
    OK(sat_physics2_set_center(&f.world, bullet, V(300, 91)) == SAT_OK);
    OK(f.step().size() == 2);

    /* ending inside is a plain ENTER */
    OK(sat_physics2_set_center(&f.world, bullet, V(100, 100)) == SAT_OK);
    f.step();
    OK(sat_physics2_set_center(&f.world, bullet, V(204, 100)) == SAT_OK);
    ev = f.step();
    OK(ev.size() == 1 && is_event(ev[0], sensor, bullet, SAT_PHYSICS2_EVENT_ENTER));
    /* and leaving fast in one go is a plain LEAVE */
    OK(sat_physics2_set_center(&f.world, bullet, V(400, 100)) == SAT_OK);
    ev = f.step();
    OK(ev.size() == 1 && is_event(ev[0], sensor, bullet, SAT_PHYSICS2_EVENT_LEAVE));
}

static void sensor_ordering_is_deterministic() {
    auto run = [](std::vector<sat_physics2_event_t>& log) {
        Fixture f;
        sat_physics2_set_emit_stay(&f.world, 1);
        f.add(SAT_COLLIDER2_SENSOR, B(100, 100, 30, 30));
        f.add(SAT_COLLIDER2_SENSOR, B(110, 100, 30, 30));
        const sat_collider2_t k1 = f.add(SAT_COLLIDER2_KINEMATIC, B(0, 0, 3, 3));
        const sat_collider2_t k2 = f.add(SAT_COLLIDER2_KINEMATIC, B(300, 300, 3, 3));
        f.add(SAT_COLLIDER2_STATIC, B(105, 105, 3, 3));
        for (int i = 0; i < 8; ++i) {
            OK(sat_physics2_set_center(&f.world, k1, V(20 * i, 100)) == SAT_OK);
            OK(sat_physics2_set_center(&f.world, k2, V(300 - 25 * i, 100)) == SAT_OK);
            for (const auto& e : f.step()) log.push_back(e);
        }
    };
    std::vector<sat_physics2_event_t> a, b;
    run(a);
    run(b);
    OK(!a.empty() && a.size() == b.size());
    OK(std::memcmp(a.data(), b.data(), a.size() * sizeof(a[0])) == 0);

    /* within one step events are ordered by (sensor, other) handle */
    Fixture f;
    sat_physics2_set_emit_stay(&f.world, 1);
    const sat_collider2_t s1 = f.add(SAT_COLLIDER2_SENSOR, B(100, 100, 30, 30));
    const sat_collider2_t s2 = f.add(SAT_COLLIDER2_SENSOR, B(110, 100, 30, 30));
    const sat_collider2_t o1 = f.add(SAT_COLLIDER2_STATIC, B(105, 100, 3, 3));
    const sat_collider2_t o2 = f.add(SAT_COLLIDER2_STATIC, B(108, 100, 3, 3));
    std::vector<sat_physics2_event_t> ev = f.step();
    OK(ev.size() == 4);
    OK(is_event(ev[0], s1, o1, SAT_PHYSICS2_EVENT_ENTER) && is_event(ev[1], s1, o2, SAT_PHYSICS2_EVENT_ENTER));
    OK(is_event(ev[2], s2, o1, SAT_PHYSICS2_EVENT_ENTER) && is_event(ev[3], s2, o2, SAT_PHYSICS2_EVENT_ENTER));
    ev = f.step();
    OK(ev.size() == 4);
    for (const auto& e : ev) OK(e.type == SAT_PHYSICS2_EVENT_STAY);
}

static void sensor_capacities() {
    {
        Fixture f(Fixture::kEntries, 2); /* room for two pairs */
        f.add(SAT_COLLIDER2_SENSOR, B(100, 100, 30, 30));
        for (int i = 0; i < 4; ++i) f.add(SAT_COLLIDER2_STATIC, B(95 + 4 * i, 100, 1, 1));
        sat_physics2_step_result_t r;
        std::vector<sat_physics2_event_t> ev = f.step(&r, SAT_ERR_CAPACITY);
        OK(r.pairs_dropped == 2 && ev.size() == 2 && f.world.pair_count == 2);
    }
    {
        Fixture f;
        f.add(SAT_COLLIDER2_SENSOR, B(100, 100, 30, 30));
        for (int i = 0; i < 4; ++i) f.add(SAT_COLLIDER2_STATIC, B(95 + 4 * i, 100, 1, 1));
        sat_physics2_event_t ev[3];
        sat_physics2_step_result_t r;
        OK(sat_physics2_step(&f.world, ev, 3, &r) == SAT_ERR_CAPACITY);
        OK(r.events == 3 && r.events_dropped == 1 && f.world.pair_count == 4); /* the pairs are tracked anyway */
        OK(f.step().empty());                                                    /* ...so no repeat ENTERs */
        OK(sat_physics2_step(&f.world, nullptr, 0, nullptr) == SAT_OK);
        OK(sat_physics2_step(&f.world, nullptr, 3, nullptr) == SAT_ERR_INVALID_ARG);
        OK(sat_physics2_step(nullptr, ev, 3, &r) == SAT_ERR_INVALID_ARG);
    }
    {
        /* a broadphase too small for the colliders is reported, not silently wrong */
        Fixture f(2);
        for (int i = 0; i < 4; ++i) f.add(SAT_COLLIDER2_STATIC, B(20 + 40 * i, 20, 5, 5));
        sat_physics2_step_result_t r;
        f.step(&r, SAT_ERR_CAPACITY);
        sat_collider2_t out[4];
        uint16_t n;
        const sat_box2_t q = B(100, 20, 100, 20);
        OK(sat_physics2_query(&f.world, &q, SAT_COLLIDER2_ALL_KINDS, 0xFFFF, out, 4, &n) == SAT_ERR_CAPACITY);
        sat_physics2_support_t s;
        OK(support(f, foot(20, 20), &s) == SAT_ERR_CAPACITY);
    }
}

static void many_colliders_through_the_broadphase() {
    /* a field of platforms and one sensor: results match a brute-force count */
    Fixture f;
    const sat_collider2_t sensor = f.add(SAT_COLLIDER2_SENSOR, B(256, 256, 64, 64));
    int expected = 0;
    for (int i = 0; i < Fixture::kSlots - 1; ++i) {
        const int x = 30 + (i * 53) % 440, y = 30 + (i * 91) % 440;
        f.add(SAT_COLLIDER2_STATIC, B(x, y, 12, 12));
        const int dx = x > 256 ? x - 256 : 256 - x, dy = y > 256 ? y - 256 : 256 - y;
        if (dx < 64 + 12 && dy < 64 + 12) ++expected;
    }
    std::vector<sat_physics2_event_t> ev = f.step();
    int enters = 0;
    for (const auto& e : ev)
        if (e.sensor == sensor && e.type == SAT_PHYSICS2_EVENT_ENTER) ++enters;
    OK(enters == expected && expected > 0);
}

int main() {
    init_and_requirements();
    handles_and_capacity();
    moving_and_static_setters();
    overlap_queries();
    support_queries();
    support_in_every_direction();
    one_way_supports();
    horizontal_platform_carries();
    vertical_platform_and_detach();
    one_way_moving_platform();
    stale_support_handles();
    sensor_enter_stay_leave();
    sensor_removal_while_overlapping();
    sensor_high_speed_crossing();
    sensor_ordering_is_deterministic();
    sensor_capacities();
    many_colliders_through_the_broadphase();
    std::puts("PASS: test_physics2_world.cpp");
    return 0;
}
