#include <cstdio>
#include <cstdlib>
#include <vector>

#include "saturn/character2.h"
#include "tests/host/terrain2_test_world.hpp"

static sat_fx16_t F(int x) { return static_cast<sat_fx16_t>(x * SAT_FX16_ONE); }
static sat_fx16_t FQ(int num, int den) { return static_cast<sat_fx16_t>((static_cast<long long>(num) << 16) / den); }
static int absi(int v) { return v < 0 ? -v : v; }
static int px(sat_fx16_t v) { return v >> 16; }

static sat_character2_config_t default_config() {
    sat_character2_config_t c;
    sat_character2_config_default(&c);
    OK(sat_character2_config_validate(&c) == SAT_OK);
    return c;
}

static const sat_vec2_t kNoGravity = {0, 0};
static const sat_vec2_t kGravityDown = {0, 32768};  /* 0.5 px/step^2 */
static const sat_vec2_t kGravityUp = {0, -32768};

/* One game step: the game adds gravity while airborne, then Character2 resolves the motion. */
static uint32_t tick(sat_character2_t& ch, const sat_character2_config_t& cfg, const World& w,
                     sat_vec2_t gravity = kNoGravity, sat_character2_result_t* out = nullptr) {
    if (!sat_character2_is_supported(&ch)) {
        ch.air_velocity.x += gravity.x;
        ch.air_velocity.y += gravity.y;
    }
    sat_character2_result_t r = {};
    OK(sat_character2_step(&ch, &cfg, &w.map, &r) == SAT_OK);
    if (out) *out = r;
    return r.events;
}

static sat_character2_t spawn(const World& w, const sat_character2_config_t& cfg, int x, int y) {
    sat_character2_t ch;
    sat_character2_init(&ch, F(x), F(y));
    OK(!sat_character2_is_supported(&ch));
    OK(sat_character2_attach(&ch, &cfg, &w.map) == SAT_OK);
    OK(sat_character2_is_supported(&ch));
    return ch;
}

/* A row of floor tiles at ty = 5 (surface y = 40) in a 16 x 8 tile world. */
static void flat_floor(World& w) { w.fill_rect(0, 5, 16, 8, T(P_FULL)); }

static void flat_run() {
    World w(16, 8, 2);
    flat_floor(w);
    w.build();
    const sat_character2_config_t cfg = default_config();

    sat_character2_t ch = spawn(w, cfg, 20, 40);
    OK(ch.support_angle == 0 && ch.position.y == F(40) && ch.support_id == P_FULL);
    OK(ch.support_normal.y == -SAT_FX16_ONE && ch.support_tangent.x == SAT_FX16_ONE);

    ch.ground_speed = F(2);
    for (int i = 0; i < 30; ++i) OK(tick(ch, cfg, w) == 0);
    OK(ch.position.x == F(80) && ch.position.y == F(40));
    OK(sat_character2_is_supported(&ch));

    ch.ground_speed = FQ(-5, 2); /* -2.5 px per step */
    for (int i = 0; i < 8; ++i) OK(tick(ch, cfg, w) == 0);
    OK(ch.position.x == F(60) && ch.position.y == F(40));

    /* a standing character stays put on the surface */
    ch.ground_speed = 0;
    for (int i = 0; i < 5; ++i) OK(tick(ch, cfg, w) == 0);
    OK(ch.position.x == F(60) && ch.position.y == F(40));

    const sat_vec2_t v = sat_character2_world_velocity(&ch);
    OK(v.x == 0 && v.y == 0);
}

/* 45 degree ramp rising to the right, `n` tiles long, starting at tile column tx0 on a floor at ty_floor. */
static void ramp_up(World& w, int tx0, int ty_floor, int n) {
    for (int i = 0; i < n; ++i) {
        w.set(tx0 + i, ty_floor - 1 - i, T(P_RAMP));
        w.fill_rect(tx0 + i, ty_floor - i, tx0 + i + 1, w.ht, T(P_FULL));
    }
    w.fill_rect(tx0 + n, ty_floor - n, w.wt, w.ht, T(P_FULL));
}

static void slope_traversal() {
    World w(24, 8, 2);
    w.fill_rect(0, 5, 8, 8, T(P_FULL));
    ramp_up(w, 8, 5, 2); /* ramp from (64, 40) up to (80, 24), plateau at y = 24 */
    w.build();
    const sat_character2_config_t cfg = default_config();

    sat_character2_t ch = spawn(w, cfg, 30, 40);
    ch.ground_speed = F(2);
    bool saw_ramp = false;
    int last_x = px(ch.position.x);
    for (int i = 0; i < 50; ++i) {
        const uint32_t e = tick(ch, cfg, w);
        OK(e == 0);
        OK(sat_character2_is_supported(&ch));
        if (ch.support_angle == 224) {
            saw_ramp = true;
            /* the higher foot sensor decides, so the feet ride up to half-width above the ideal surface */
            const int ideal = 40 - (px(ch.position.x) - 64);
            OK(px(ch.position.y) <= ideal + 1 && px(ch.position.y) >= ideal - cfg.foot_half_width - 1);
        }
        OK(px(ch.position.x) >= last_x);
        last_x = px(ch.position.x);
    }
    OK(saw_ramp);
    OK(ch.position.y == F(24) && ch.support_angle == 0); /* up on the plateau, flat again */
    OK(ch.position.x > F(110));

    /* and back down the same way */
    ch.ground_speed = F(-2);
    for (int i = 0; i < 55; ++i) {
        OK(tick(ch, cfg, w) == 0);
        OK(sat_character2_is_supported(&ch));
    }
    OK(ch.position.y == F(40) && ch.support_angle == 0 && ch.position.x < F(30));
}

static void edges_gaps_and_ledges() {
    sat_character2_config_t cfg = default_config();
    {
        /* an 8 px gap is narrower than the 10 px between the foot sensors: walked across */
        World w(16, 8, 2);
        flat_floor(w);
        w.fill_rect(8, 5, 9, 8, T(P_EMPTY)); /* x 64..72 */
        w.build();
        sat_character2_t ch = spawn(w, cfg, 40, 40);
        ch.ground_speed = F(2);
        for (int i = 0; i < 40; ++i) OK(tick(ch, cfg, w) == 0);
        OK(ch.position.x == F(120) && ch.position.y == F(40) && sat_character2_is_supported(&ch));
    }
    {
        /* a wide pit: the character detaches carrying its momentum, falls, and lands on the bottom */
        World w(16, 12, 2);
        w.fill_rect(0, 5, 16, 12, T(P_FULL));
        w.fill_rect(6, 5, 14, 9, T(P_EMPTY)); /* pit x 48..112, down to y = 72 */
        w.build();
        sat_character2_t ch = spawn(w, cfg, 20, 40);
        ch.ground_speed = F(3);
        bool detached = false, landed = false;
        sat_vec2_t at_detach = {0, 0};
        for (int i = 0; i < 80 && !landed; ++i) {
            sat_character2_result_t r;
            const uint32_t e = tick(ch, cfg, w, kGravityDown, &r);
            if (e & SAT_CHARACTER2_EVENT_DETACHED) {
                OK(!detached);
                detached = true;
                at_detach = ch.air_velocity;
                OK(r.velocity_before.x == F(3));
                OK(!sat_character2_is_supported(&ch));
                OK(ch.support_id == SAT_CHARACTER2_NO_SUPPORT);
            }
            if (e & SAT_CHARACTER2_EVENT_LANDED) landed = true;
        }
        OK(detached && landed);
        OK(at_detach.x == F(3)); /* momentum kept */
        OK(sat_character2_is_supported(&ch));
        OK(ch.position.y == F(72));
        OK(ch.support_angle == 0);
        OK(absi(ch.ground_speed - F(3)) < 16); /* flat landing keeps the horizontal speed */
    }
}

static void walls_and_steps() {
    sat_character2_config_t cfg = default_config();
    {
        World w(16, 8, 2);
        flat_floor(w);
        w.fill_rect(10, 3, 11, 5, T(P_FULL)); /* wall x 80..88 */
        w.fill_rect(2, 3, 3, 5, T(P_FULL));   /* wall x 16..24 */
        w.build();
        sat_character2_t ch = spawn(w, cfg, 40, 40);
        ch.ground_speed = F(3);
        uint32_t seen = 0;
        for (int i = 0; i < 30; ++i) seen |= tick(ch, cfg, w);
        OK(seen & SAT_CHARACTER2_EVENT_HIT_WALL);
        OK(ch.position.x == F(80 - cfg.wall_radius) && ch.ground_speed == 0);
        OK(sat_character2_is_supported(&ch) && ch.position.y == F(40));
        for (int i = 0; i < 5; ++i) OK(tick(ch, cfg, w) == 0);
        OK(ch.position.x == F(80 - cfg.wall_radius));

        ch.ground_speed = F(-3);
        seen = 0;
        for (int i = 0; i < 40; ++i) seen |= tick(ch, cfg, w);
        OK(seen & SAT_CHARACTER2_EVENT_HIT_WALL);
        OK(ch.position.x == F(24 + cfg.wall_radius) && ch.ground_speed == 0);
    }
    {
        /* an 8 px step is climbed when step_up allows it, and is a wall when it does not */
        World w(16, 8, 2);
        flat_floor(w);
        w.fill_rect(8, 4, 16, 5, T(P_FULL)); /* raised floor, surface y = 32, from x = 64 */
        w.build();
        sat_character2_t ch = spawn(w, cfg, 30, 40);
        ch.ground_speed = F(2);
        for (int i = 0; i < 40; ++i) OK(tick(ch, cfg, w) == 0);
        OK(ch.position.y == F(32) && ch.position.x == F(110));

        cfg.step_up = 4;
        cfg.wall_height = 12; /* above the step */
        sat_character2_t low = spawn(w, cfg, 30, 40);
        low.ground_speed = F(2);
        uint32_t seen = 0;
        for (int i = 0; i < 40; ++i) seen |= tick(low, cfg, w);
        OK(seen & SAT_CHARACTER2_EVENT_HIT_WALL);
        OK(low.position.y == F(40) && low.ground_speed == 0);
        OK(low.position.x < F(64) && low.position.x >= F(64 - 2 * cfg.foot_half_width));
    }
}

/* Interior of a rectangular tunnel with 45 degree corners `corner` tiles long, solid everywhere else. */
static void build_tunnel(World& w, int x0, int y0, int x1, int y1, int corner) {
    w.fill_rect(0, 0, w.wt, w.ht, T(P_FULL));
    w.fill_rect(x0, y0, x1 + 1, y1 + 1, T(P_EMPTY));
    const int L = corner;
    for (int v = 0; v < L; ++v) {
        for (int u = 0; u < L; ++u) {
            if (u < v) continue; /* free interior side of the diagonal */
            const uint16_t br = u == v ? T(P_RAMP) : T(P_FULL);
            const uint16_t bl = u == v ? T(P_RAMP, true, false) : T(P_FULL);
            const uint16_t tr = u == v ? T(P_RAMP, false, true) : T(P_FULL);
            const uint16_t tl = u == v ? T(P_RAMP, true, true) : T(P_FULL);
            w.set(x1 - (L - 1) + u, y1 - v, br);
            w.set(x0 + (L - 1) - u, y1 - v, bl);
            w.set(x1 - (L - 1) + u, y0 + v, tr);
            w.set(x0 + (L - 1) - u, y0 + v, tl);
        }
    }
}

static void loop_traversal() {
    World w(24, 24, 2);
    build_tunnel(w, 4, 4, 19, 19, 3); /* interior x 32..160, y 32..160 */
    w.build();
    sat_character2_config_t cfg = default_config();

    sat_character2_t ch = spawn(w, cfg, 80, 160);
    OK(ch.support_angle == 0);
    ch.ground_speed = FQ(7, 2); /* 3.5 px per step, counter-clockwise on screen */
    int modes_seen = 0;
    int min_y = 1 << 20, max_x = 0, max_y = 0, min_x = 1 << 20;
    for (int i = 0; i < 360; ++i) {
        const uint32_t e = tick(ch, cfg, w);
        if (e != 0 || !sat_character2_is_supported(&ch)) {
            std::fprintf(stderr, "loop step %d: events %u supported %d at (%d, %d) angle %u gs %d\n", i, e,
                         sat_character2_is_supported(&ch), px(ch.position.x), px(ch.position.y),
                         ch.support_angle, ch.ground_speed);
        }
        OK(e == 0);
        OK(sat_character2_is_supported(&ch));
        const int a = ch.support_angle;
        const int m = (a <= 32 || a >= 224) ? 0 : (a <= 95) ? 1 : (a <= 160) ? 2 : 3;
        modes_seen |= 1 << m;
        min_y = px(ch.position.y) < min_y ? px(ch.position.y) : min_y;
        max_y = px(ch.position.y) > max_y ? px(ch.position.y) : max_y;
        min_x = px(ch.position.x) < min_x ? px(ch.position.x) : min_x;
        max_x = px(ch.position.x) > max_x ? px(ch.position.x) : max_x;
    }
    OK(modes_seen == 0xF);
    OK(min_y == 32 && max_y == 160); /* reached the ceiling and the floor */
    OK(min_x == 32 && max_x == 160); /* and both walls */
    /* ground speed is not touched by the controller while the surface keeps it */
    OK(ch.ground_speed == FQ(7, 2));

    /* clockwise as well */
    sat_character2_t cw = spawn(w, cfg, 80, 160);
    cw.ground_speed = FQ(-7, 2);
    for (int i = 0; i < 360; ++i) {
        OK(tick(cw, cfg, w) == 0);
        OK(sat_character2_is_supported(&cw));
    }
}

static void slow_on_a_wall_slips_off() {
    World w(24, 24, 2);
    build_tunnel(w, 4, 4, 19, 19, 3);
    w.build();
    sat_character2_config_t cfg = default_config();
    sat_character2_t ch = spawn(w, cfg, 80, 160);
    ch.ground_speed = F(1);
    bool slipped = false;
    for (int i = 0; i < 400 && !slipped; ++i) {
        const uint32_t e = tick(ch, cfg, w, kGravityDown);
        if (e & SAT_CHARACTER2_EVENT_SLIPPED) {
            slipped = true;
            OK(e & SAT_CHARACTER2_EVENT_DETACHED);
            OK(!sat_character2_is_supported(&ch));
        }
    }
    OK(slipped);
    /* it slipped on a steep part of the track, not on the floor */
    OK(ch.position.x > F(120));
}

static void landing_converts_velocity() {
    sat_character2_config_t cfg = default_config();
    {
        World w(16, 8, 2);
        flat_floor(w);
        w.build();
        sat_character2_t ch;
        sat_character2_init(&ch, F(40), F(10));
        ch.air_velocity = {F(2), F(5)};
        sat_character2_result_t r = {};
        uint32_t e = 0;
        for (int i = 0; i < 20 && !(e & SAT_CHARACTER2_EVENT_LANDED); ++i) e = tick(ch, cfg, w, kNoGravity, &r);
        OK(e & SAT_CHARACTER2_EVENT_LANDED);
        OK(r.velocity_before.x == F(2) && r.velocity_before.y == F(5));
        OK(ch.position.y == F(40) && ch.ground_speed == F(2) && ch.air_velocity.y == 0);
        OK(ch.support_angle == 0 && r.surface_angle == 0);
    }
    {
        /* onto a 45 degree slope rising to the right: speed is the projection on the surface */
        World w(16, 8, 2);
        w.fill_rect(0, 5, 8, 8, T(P_FULL));
        ramp_up(w, 8, 5, 2);
        w.build();
        sat_character2_t ch;
        sat_character2_init(&ch, F(58), F(10));
        ch.air_velocity = {F(2), F(5)};
        uint32_t e = 0;
        for (int i = 0; i < 20 && !(e & SAT_CHARACTER2_EVENT_LANDED); ++i) e = tick(ch, cfg, w);
        OK(e & SAT_CHARACTER2_EVENT_LANDED);
        OK(ch.support_angle == 224);
        /* (2, 5) . (0.7071, -0.7071) = -2.1213 */
        OK(absi(ch.ground_speed - FQ(-21213, 10000)) < 40);
    }
}

static void detach_and_attach_helpers() {
    World w(16, 8, 2);
    flat_floor(w);
    w.build();
    sat_character2_config_t cfg = default_config();
    sat_character2_t ch = spawn(w, cfg, 40, 40);
    ch.ground_speed = F(3);
    sat_character2_detach(&ch); /* jump-style: momentum kept */
    OK(!sat_character2_is_supported(&ch) && ch.air_velocity.x == F(3) && ch.air_velocity.y == 0);
    OK(ch.ground_speed == 0);
    ch.air_velocity.y = F(-6); /* the game's jump */
    sat_vec2_t v = sat_character2_world_velocity(&ch);
    OK(v.x == F(3) && v.y == F(-6));
    for (int i = 0; i < 6; ++i) tick(ch, cfg, w, kNoGravity);
    OK(ch.position.y == F(40 - 36) && ch.position.x == F(58));
    sat_character2_detach(&ch); /* a no-op while airborne */
    OK(ch.air_velocity.y == F(-6));

    /* attach refuses when there is nothing in reach */
    sat_character2_t high;
    sat_character2_init(&high, F(40), F(10));
    OK(sat_character2_attach(&high, &cfg, &w.map) == SAT_ERR_NOT_FOUND);
    OK(!sat_character2_is_supported(&high));
}

static void layer_switch() {
    World w(16, 12, 2, 2);
    w.fill_rect(0, 5, 16, 12, T(P_FULL), 0); /* layer 0 floor at y = 40 */
    w.fill_rect(0, 9, 16, 12, T(P_FULL), 1); /* layer 1 floor at y = 72 */
    w.build();
    sat_character2_config_t cfg = default_config();
    sat_character2_t ch = spawn(w, cfg, 40, 40);
    ch.ground_speed = F(2);
    for (int i = 0; i < 5; ++i) OK(tick(ch, cfg, w) == 0);
    OK(sat_character2_is_supported(&ch));

    ch.layer = 1; /* the floor the character stands on no longer exists */
    uint32_t seen = 0;
    bool landed = false;
    for (int i = 0; i < 60 && !landed; ++i) {
        const uint32_t e = tick(ch, cfg, w, kGravityDown);
        seen |= e;
        landed = (e & SAT_CHARACTER2_EVENT_LANDED) != 0;
    }
    OK(seen & SAT_CHARACTER2_EVENT_DETACHED);
    OK(landed && ch.position.y == F(72));

    /* Back to layer 0 from 32 px inside its floor: only a shallow overlap (step_up) is
     * recovered, so a buried character is not rescued - changing layers is the game's call. */
    ch.layer = 0;
    OK(sat_character2_attach(&ch, &cfg, &w.map) == SAT_ERR_NOT_FOUND);
    ch.position.y = F(44); /* 4 px inside: pops up to the surface */
    OK(sat_character2_attach(&ch, &cfg, &w.map) == SAT_OK && ch.position.y == F(40));
}

static void ceilings() {
    sat_character2_config_t cfg = default_config();
    cfg.head_height = 16;
    {
        World w(16, 8, 2);
        flat_floor(w);
        w.fill_rect(0, 1, 16, 2, T(P_FULL)); /* ceiling slab y 8..16 */
        w.build();
        sat_character2_t ch = spawn(w, cfg, 40, 40);
        sat_character2_detach(&ch);
        ch.air_velocity = {0, F(-6)};
        uint32_t seen = 0;
        for (int i = 0; i < 6; ++i) seen |= tick(ch, cfg, w, kGravityDown);
        OK(seen & SAT_CHARACTER2_EVENT_HIT_CEILING);
        OK(!sat_character2_is_supported(&ch));
        OK(ch.position.y >= F(32)); /* head stopped under y = 16 */
        bool landed = false;
        for (int i = 0; i < 40 && !landed; ++i) landed = (tick(ch, cfg, w, kGravityDown) & SAT_CHARACTER2_EVENT_LANDED) != 0;
        OK(landed && ch.position.y == F(40));
    }
    {
        /* a slanted ceiling attaches when ceiling_attach allows it; a flat one never does */
        World w(16, 8, 2);
        flat_floor(w);
        w.fill_rect(0, 1, 16, 2, T(P_FULL));
        w.fill_rect(4, 1, 8, 2, T(P_RAMP, false, true)); /* hangs from the top, slanted */
        w.build();
        sat_character2_config_t attach = cfg;
        attach.ceiling_attach = 8;
        for (int flat = 0; flat < 2; ++flat) {
            sat_character2_t ch = spawn(w, cfg, flat ? 100 : 48, 40);
            sat_character2_detach(&ch);
            ch.air_velocity = {F(1), F(-6)};
            bool attached = false;
            for (int i = 0; i < 8 && !attached; ++i) {
                const uint32_t e = tick(ch, attach, w);
                attached = (e & SAT_CHARACTER2_EVENT_LANDED) != 0;
            }
            if (flat) {
                OK(!attached && !sat_character2_is_supported(&ch));
            } else {
                OK(attached && sat_character2_is_supported(&ch));
                OK(ch.support_angle == static_cast<uint8_t>(128 - 224)); /* hangs from the slanted ceiling */
                OK(ch.ground_speed != 0);
            }
        }
    }
}

static void high_speed_does_not_tunnel() {
    sat_character2_config_t cfg = default_config();
    {
        World w(16, 8, 2);
        flat_floor(w);
        w.fill_rect(11, 3, 12, 5, T(P_FULL)); /* one-tile wall x 88..96 */
        w.build();
        sat_character2_t ch = spawn(w, cfg, 20, 40);
        ch.ground_speed = F(30); /* 7.5 tiles per step... ten segments of 3 px */
        uint32_t seen = 0;
        for (int i = 0; i < 6; ++i) seen |= tick(ch, cfg, w);
        OK(seen & SAT_CHARACTER2_EVENT_HIT_WALL);
        OK(ch.position.x == F(88 - cfg.wall_radius) && ch.position.y == F(40));
    }
    {
        World w(16, 8, 2);
        w.fill_rect(0, 5, 16, 6, T(P_FULL)); /* a single 8 px floor, nothing below */
        w.build();
        sat_character2_t ch;
        sat_character2_init(&ch, F(40), F(0));
        ch.air_velocity = {0, F(60)};
        uint32_t e = 0;
        for (int i = 0; i < 4 && !(e & SAT_CHARACTER2_EVENT_LANDED); ++i) e = tick(ch, cfg, w);
        OK(e & SAT_CHARACTER2_EVENT_LANDED);
        OK(ch.position.y == F(40));
    }
}

static void inverted_gravity_mirrors_the_world() {
    World w(16, 8, 2);
    w.fill_rect(0, 1, 16, 2, T(P_FULL)); /* ceiling slab, lower face y = 16 */
    w.build();
    sat_character2_config_t cfg = default_config();
    cfg.gravity_quadrant = 3; /* "down" is up the screen */
    sat_character2_t ch;
    sat_character2_init(&ch, F(60), F(60));
    uint32_t e = 0;
    for (int i = 0; i < 60 && !(e & SAT_CHARACTER2_EVENT_LANDED); ++i) e = tick(ch, cfg, w, kGravityUp);
    OK(e & SAT_CHARACTER2_EVENT_LANDED);
    OK(ch.position.y == F(16) && ch.support_angle == 128);
    OK(ch.support_normal.y == SAT_FX16_ONE);
    /* it is level for this gravity: running keeps it attached, travelling towards -X at positive speed */
    ch.ground_speed = F(2);
    for (int i = 0; i < 20; ++i) OK(tick(ch, cfg, w, kGravityUp) == 0);
    OK(ch.position.x == F(60 - 40) && ch.position.y == F(16) && sat_character2_is_supported(&ch));
}

static void arguments_are_validated() {
    World w(16, 8, 2);
    flat_floor(w);
    w.build();
    sat_character2_config_t cfg = default_config();
    sat_character2_t ch = spawn(w, cfg, 40, 40);
    sat_character2_result_t r;
    OK(sat_character2_step(nullptr, &cfg, &w.map, &r) == SAT_ERR_INVALID_ARG);
    OK(sat_character2_step(&ch, nullptr, &w.map, &r) == SAT_ERR_INVALID_ARG);
    OK(sat_character2_step(&ch, &cfg, nullptr, &r) == SAT_ERR_INVALID_ARG);
    OK(sat_character2_step(&ch, &cfg, &w.map, nullptr) == SAT_OK);
    ch.layer = 3;
    OK(sat_character2_step(&ch, &cfg, &w.map, &r) == SAT_ERR_INVALID_ARG);
    ch.layer = 0;

    sat_character2_config_t bad = cfg;
    bad.segment_px = 0;
    OK(sat_character2_config_validate(&bad) == SAT_ERR_INVALID_ARG);
    bad = cfg;
    bad.max_segments = 33;
    OK(sat_character2_config_validate(&bad) == SAT_ERR_INVALID_ARG);
    bad = cfg;
    bad.gravity_quadrant = 4;
    OK(sat_character2_config_validate(&bad) == SAT_ERR_INVALID_ARG);
    bad = cfg;
    bad.step_up = 65;
    OK(sat_character2_config_validate(&bad) == SAT_ERR_INVALID_ARG);
    OK(sat_character2_step(&ch, &bad, &w.map, &r) == SAT_ERR_INVALID_ARG);
    OK(sat_character2_config_validate(nullptr) == SAT_ERR_INVALID_ARG);
}

static void stepping_is_deterministic() {
    World w(24, 24, 2);
    build_tunnel(w, 4, 4, 19, 19, 3);
    w.build();
    sat_character2_config_t cfg = default_config();
    sat_character2_t a = spawn(w, cfg, 80, 160), b = spawn(w, cfg, 80, 160);
    a.ground_speed = b.ground_speed = FQ(7, 2);
    for (int i = 0; i < 200; ++i) {
        tick(a, cfg, w);
        tick(b, cfg, w);
        OK(a.position.x == b.position.x && a.position.y == b.position.y && a.support_angle == b.support_angle);
    }
}

int main() {
    flat_run();
    slope_traversal();
    edges_gaps_and_ledges();
    walls_and_steps();
    loop_traversal();
    slow_on_a_wall_slips_off();
    landing_converts_velocity();
    detach_and_attach_helpers();
    layer_switch();
    ceilings();
    high_speed_does_not_tunnel();
    inverted_gravity_mirrors_the_world();
    arguments_are_validated();
    stepping_is_deterministic();
    std::puts("PASS: test_character2.cpp");
    return 0;
}
