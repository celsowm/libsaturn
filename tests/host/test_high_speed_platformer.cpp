#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "game.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

/* Plays the example's stage without a video chip: the same game.c the Saturn build links, driven by
 * scripted pad input. */

static hsp_game_t g_game;
static const bool trace = std::getenv("HSP_TRACE") != nullptr;

static double px(sat_fx16_t v) { return v / 65536.0; }

static void report(const char* label) {
    if (!trace) return;
    const hsp_game_t& g = g_game;
    std::printf("%-10s t=%5u pos=(%8.2f,%8.2f) speed=%5.2f gs=%5.2f sup=%d layer=%d mode=%d roll=%d stand=%u rings=%u deaths=%u\n",
                label, g.stats.ticks, px(g.hero.position.x), px(g.hero.position.y), px(hsp_game_hero_speed(&g)),
                px(g.hero.ground_speed), sat_character2_is_supported(&g.hero), g.hero.layer, g.mode, g.rolling,
                (unsigned)g.stand, g.stats.rings, g.stats.deaths);
}

static void run(unsigned ticks, uint16_t held, unsigned trace_every = 0) {
    static uint16_t previous = 0;
    for (unsigned i = 0; i < ticks; ++i) {
        const uint16_t pressed = (uint16_t)(held & ~previous);
        hsp_game_step(&g_game, held, pressed);
        previous = held;
        if (trace_every && (i % trace_every) == 0) report("run");
    }
}

/* Puts the hero somewhere without playing the stage up to there. `ground` = standing on the terrain. */
static void place(double x, double y, double speed, bool ground) {
    hsp_game_t& g = g_game;
    sat_character2_init(&g.hero, (sat_fx16_t)(x * 65536.0), (sat_fx16_t)(y * 65536.0));
    g.mode = HSP_MODE_RUN;
    g.rolling = 0;
    g.stand = SAT_COLLIDER2_NONE;
    g.rail_cooldown = 0;
    if (!ground) {
        g.hero.air_velocity.x = (sat_fx16_t)(speed * 65536.0);
        g.hero.air_velocity.y = 0;
    } else {
        OK(sat_character2_attach(&g.hero, &g.cfg_stand, &g.terrain) == SAT_OK);
        g.hero.ground_speed = (sat_fx16_t)(speed * 65536.0);
    }
    sat_vec2_t at;
    at.x = g.hero.position.x;
    at.y = g.hero.position.y - (12 << 16);
    (void)sat_follow_camera2d_snap(&g.camera, at);
}

int main() {
    OK(hsp_game_init(&g_game) == SAT_OK);
    hsp_game_t& g = g_game;
    report("start");

    /* --- the flat start: the hero accelerates to the walking top speed and collects rings --- */
    run(120, SAT_PAD_RIGHT, trace ? 20 : 0);
    OK(g.stats.rings > 5);
    OK(g.stats.spawned > 0);
    OK(g.stats.footsteps > 4);

    /* --- a moving platform over the first pit carries the hero --- */
    {
        sat_box2_t box;
        hsp_game_platform_box(&g, 0, &box);
        place(px(box.center.x), px(box.center.y) - px(box.half.y) - 20, 0, false);
        const uint32_t before = g.stats.platform_ticks;
        double first_x = -1, last_x = -1;
        run(2, 0);
        for (unsigned i = 0; i < 80; ++i) {
            hsp_game_step(&g, 0, 0);
            if (g.stand != SAT_COLLIDER2_NONE) {
                if (first_x < 0) first_x = px(g.hero.position.x);
                last_x = px(g.hero.position.x);
            }
            if (trace && (i % 10 == 0 || (i > 48 && i < 64))) {
                sat_box2_t pb;
                hsp_game_platform_box(&g, 0, &pb);
                report("plat");
                std::printf("      platform (%.2f, %.2f) vy=%.2f dir=%d\n", px(pb.center.x), px(pb.center.y), px(g.hero.air_velocity.y), g.platform[0].direction);
            }
        }
        OK(g.stats.platform_ticks > before + 20);
        OK(first_x > 0);
        std::printf("platform: stood %u ticks, x %.1f -> %.1f\n", g.stats.platform_ticks - before, first_x, last_x);
        OK(g.stand != SAT_COLLIDER2_NONE); /* still carried at the end of the ride */
        /* jumping off carries the platform's motion and gives up the support */
        hsp_game_step(&g, SAT_PAD_RIGHT | SAT_PAD_A, SAT_PAD_A);
        OK(g.stand == SAT_COLLIDER2_NONE);
        OK(g.hero.air_velocity.y < 0);
    }

    /* --- a one-way plank: jump up through it from below, land on top of it --- */
    place(1620, 320, 0, true);
    {
        const uint32_t landings = g.stats.landings;
        bool passed = false;
        hsp_game_step(&g, SAT_PAD_A, SAT_PAD_A);
        for (unsigned i = 0; i < 60; ++i) {
            hsp_game_step(&g, SAT_PAD_A, 0);
            if (px(g.hero.position.y) < 280.0) passed = true;
            if (passed && sat_character2_is_supported(&g.hero)) break;
        }
        OK(passed);
        OK(sat_character2_is_supported(&g.hero));
        OK(px(g.hero.position.y) < 285.0 && px(g.hero.position.y) > 275.0);
        OK(g.stats.landings > landings);
        std::printf("plank: landed at y=%.1f\n", px(g.hero.position.y));
    }

    /* --- the loop at speed: layer switches at the top, out on the far side, no death --- */
    place(1180, 320, 14.0, true);
    {
        const uint32_t switches = g.stats.layer_switches;
        const uint32_t deaths = g.stats.deaths;
        double min_y = 1e9;
        for (unsigned i = 0; i < 120; ++i) {
            hsp_game_step(&g, SAT_PAD_RIGHT, 0);
            if (px(g.hero.position.y) < min_y) min_y = px(g.hero.position.y);
            if (trace && i % 4 == 0) report("loop");
        }
        std::printf("loop: min_y=%.1f x=%.1f layer=%d switches=%u deaths=%u\n", min_y, px(g.hero.position.x), g.hero.layer,
                    g.stats.layer_switches - switches, g.stats.deaths - deaths);
        OK(g.stats.deaths == deaths);
        OK(g.stats.layer_switches - switches >= 2);
        OK(min_y < 220.0);
        OK(px(g.hero.position.x) > 1400.0);
        OK(g.hero.layer == 0);
    }

    /* --- the rail: grab it in the air, ride the Bezier, drop off at its end --- */
    place(1860, 270, 8.0, false);
    {
        const uint32_t grabs = g.stats.rail_grabs;
        bool rode = false;
        for (unsigned i = 0; i < 240; ++i) {
            hsp_game_step(&g, SAT_PAD_RIGHT, 0);
            if (g.mode == HSP_MODE_RAIL) rode = true;
            if (trace && i % 10 == 0) report("rail");
        }
        OK(g.stats.rail_grabs > grabs);
        OK(rode);
        OK(px(g.hero.position.x) > 2208.0);
        /* the rail dropped the hero onto the ground and the run went on to the finish */
        OK(g.stats.cleared == 1u);
        OK(g.mode == HSP_MODE_CLEAR);
    }

    /* --- the room at the end: the camera is clamped to it, the rings and checkpoints streamed --- */
    {
        sat_fx16_t ox, oy;
        hsp_game_view_origin(&g, &ox, &oy);
        OK(px(ox) >= HSP_ROOM_X - 0.5);
        OK(px(ox) + HSP_VIEW_W <= HSP_STAGE_W + 0.5);
        OK(px(oy) >= 0 && px(oy) + HSP_VIEW_H <= HSP_STAGE_H + 0.5);
        OK(g.stats.spawned > 100u);
        OK(g.stats.despawned > 50u);
        OK(g.stats.pool_full == 0u);
        OK(g.stats.top_speed > (14 << 16));
        OK(g.respawn.x >= 2720L * 65536L); /* the last checkpoint, passed on the way */
        std::printf("stage: rings=%u spawned=%u despawned=%u landings=%u dashes=%u springs=%u top=%.1f\n", g.stats.rings,
                    g.stats.spawned, g.stats.despawned, g.stats.landings, g.stats.dashes, g.stats.springs, px(g.stats.top_speed));
    }
    return 0;
}
