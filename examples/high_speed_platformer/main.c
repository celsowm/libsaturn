/* High-speed platformer: a stage built only from the generic 2D runtime modules (Terrain2,
 * Character2, Physics2, Path2, Follow Camera2D, stage_map2, entity_stream2, sprite_clip). Run
 * right, jump with A/B/C, hold DOWN to roll. See README.md. */
#include <stdint.h>

#include "saturn/color.h"
#include "saturn/core.h"
#include "saturn/input.h"
#include "saturn/physics.h"
#include "saturn/saturn.h"
#include "saturn/app.h"
#include "saturn/vdp1.h"
#include "saturn/vdp2.h"

#include "game.h"
#include "view.h"

/* The example uses the public headers only, so it carries its own check macro. */
#define HSP_MUST(expr) SAT_PANIC_IF_ERROR(expr)

#define MAX_STEPS_PER_FRAME 3u

/* What the harness reads out of work RAM after a scripted run (tools/hsp_probe_check.py). */
#define HSP_TELEMETRY_MAGIC 0x48535031u /* "HSP1" */
typedef struct hsp_telemetry {
    uint32_t magic;
    uint32_t frames;
    uint32_t ticks;
    int32_t x, y;          /* the hero, 16.16 pixels */
    uint32_t rings;
    uint32_t deaths;
    uint32_t layer_switches;
    uint32_t platform_ticks;
    uint32_t rail_grabs;
    uint32_t landings;
    uint32_t spawned;
    uint32_t despawned;
    int32_t top_speed;
    uint32_t cleared;
} hsp_telemetry_t;

volatile hsp_telemetry_t g_hsp_telemetry;

static hsp_game_t g_game;
static hsp_view_t g_view;

static void publish_telemetry(void) {
    const hsp_stats_t* st = &g_game.stats;
    g_hsp_telemetry.magic = HSP_TELEMETRY_MAGIC;
    ++g_hsp_telemetry.frames;
    g_hsp_telemetry.ticks = st->ticks;
    g_hsp_telemetry.x = g_game.hero.position.x;
    g_hsp_telemetry.y = g_game.hero.position.y;
    g_hsp_telemetry.rings = st->rings;
    g_hsp_telemetry.deaths = st->deaths;
    g_hsp_telemetry.layer_switches = st->layer_switches;
    g_hsp_telemetry.platform_ticks = st->platform_ticks;
    g_hsp_telemetry.rail_grabs = st->rail_grabs;
    g_hsp_telemetry.landings = st->landings;
    g_hsp_telemetry.spawned = st->spawned;
    g_hsp_telemetry.despawned = st->despawned;
    g_hsp_telemetry.top_speed = st->top_speed;
    g_hsp_telemetry.cleared = st->cleared;
}

int main(void) {
    sat_step_clock_t clock;
    HSP_MUST(sat_app_init_default());
    HSP_MUST(hsp_game_init(&g_game));
    HSP_MUST(hsp_view_init(&g_view));
    HSP_MUST(hsp_view_follow(&g_view, &g_game));
    sat_step_clock_init(&clock);

    for (;;) {
        sat_pad_state_t pad = {0};
        uint16_t steps;
        uint16_t pressed;
        HSP_MUST(sat_wait_vblank());
        HSP_MUST(hsp_view_present(&g_view));
        HSP_MUST(sat_vdp2_back_color_set(SAT_RGB555(12, 21, 31)));
        HSP_MUST(sat_vdp1_set_erase_transparent());
        HSP_MUST(sat_begin_frame());
        HSP_MUST(sat_pad_poll(&pad));

        pressed = pad.pressed;
        steps = sat_step_clock_steps(&clock, MAX_STEPS_PER_FRAME);
        while (steps--) {
            hsp_game_step(&g_game, pad.held, pressed);
            pressed = 0u; /* a press is one tick, however many ticks this frame owes */
        }
        HSP_MUST(hsp_view_follow(&g_view, &g_game));
        HSP_MUST(hsp_view_draw(&g_view, &g_game));
        publish_telemetry();
        HSP_MUST(sat_end_frame());
    }
}
