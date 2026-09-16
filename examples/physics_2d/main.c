/* physics_2d - a small arcade-physics playground using only libsaturn APIs. */
#include <stdint.h>
#include "saturn/app.h"
#include "saturn/collide2d.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/fmt.h"
#include "saturn/input.h"
#include "saturn/physics.h"
#include "saturn/spatial.h"
#include "saturn/vdp1.h"
#include "saturn/example_util.h"

#define W 320
#define H 224
#define BALLS 64
#define WALLS 10
#define HUD_PALETTE 1u
#define FX(v) ((sat_fx16_t)((v) << 16))

static sat_body2_t g_balls[BALLS];
static uint8_t g_live[BALLS];
static sat_box2_t g_walls[WALLS];
static uint16_t g_heads[10 * 7];
static sat_spatial_entry_t g_entries[BALLS * 9];
static uint16_t g_stamps[BALLS];
static sat_box2_t g_items[BALLS];
static sat_spatial_pair_t g_pairs[BALLS * 8];
static sat_spatial_t g_spatial;
static sat_ascii_font_t g_font;
static uint16_t g_pair_count;
static uint32_t g_avoided;
static uint16_t g_count;

static void spawn(void);

static void wall(int i, int x, int y, int w, int h) {
    g_walls[i].center = (sat_vec2_t){FX(x + w / 2), FX(y + h / 2)};
    g_walls[i].half = (sat_vec2_t){FX(w / 2), FX(h / 2)};
}
static void reset(void) {
    uint16_t i;
    for (i = 0; i < BALLS; ++i) g_live[i] = 0;
    g_count = 0; g_pair_count = 0; g_avoided = 0;
    wall(0, 12, 38, 296, 6); wall(1, 12, 180, 296, 6);
    wall(2, 12, 38, 6, 148); wall(3, 302, 38, 6, 148);
    wall(4, 70, 80, 80, 6); wall(5, 170, 80, 80, 6);
    wall(6, 70, 140, 80, 6); wall(7, 170, 140, 80, 6);
    wall(8, 156, 38, 8, 35); wall(9, 156, 145, 8, 35);
    spawn();
}
static void spawn(void) {
    uint16_t i;
    for (i = 0; i < BALLS; ++i) if (!g_live[i]) {
        g_live[i] = 1; ++g_count;
        g_balls[i].box.center = (sat_vec2_t){FX(35 + (i * 17) % 245), FX(50 + (i * 11) % 60)};
        g_balls[i].box.half = (sat_vec2_t){FX(4), FX(4)};
        g_balls[i].vel = (sat_vec2_t){FX((int)(i % 5) - 2), FX((int)(i % 3) - 1)};
        g_balls[i].flags = 0; return;
    }
}
static void physics_tick(void) {
    uint16_t i;
    sat_body2_params_t p = {FX(1) / 8, FX(8), FX(255) << 8, 0};
    sat_spatial_clear(&g_spatial);
    for (i = 0; i < BALLS; ++i) if (g_live[i]) {
        sat_body2_step(&g_balls[i], &p);
        sat_body2_move_boxes(&g_balls[i], g_walls, WALLS);
        g_items[i] = g_balls[i].box;
        sat_spatial_insert(&g_spatial, i, &g_items[i]);
    }
    if (sat_spatial_pairs(&g_spatial, g_pairs, BALLS * 8, &g_pair_count) == SAT_ERR_CAPACITY) g_pair_count = BALLS * 8;
    for (i = 0; i < g_pair_count; ++i) sat_body2_separate(&g_balls[g_pairs[i].a], &g_balls[g_pairs[i].b]);
    g_avoided = (uint32_t)g_count * (g_count ? g_count - 1u : 0u) / 2u - g_pair_count;
}
static void draw_text(const char* s, int x, int y) { sat_example_must(sat_ascii_font_draw_text_screen_indexed8(&g_font, s, x, y, 0, HUD_PALETTE, 0)); }
static void draw_number(const char* label, uint32_t n, int x, int y) { char text[32]; sat_example_must(sat_fmt_label_u32(label, n, text, sizeof(text), 0)); draw_text(text, x, y); }
static void draw(void) {
    uint16_t i;
    for (i = 0; i < WALLS; ++i) { const sat_box2_t* b = &g_walls[i]; sat_draw_rect_screen((int16_t)(b->center.x >> 16) - (b->half.x >> 16), (int16_t)(b->center.y >> 16) - (b->half.y >> 16), (uint16_t)(b->half.x >> 15), (uint16_t)(b->half.y >> 15), SAT_RGB555(5, 10, 28)); }
    for (i = 0; i < BALLS; ++i) if (g_live[i]) { const sat_box2_t* b=&g_balls[i].box; sat_draw_rect_screen((int16_t)(b->center.x >> 16)-4,(int16_t)(b->center.y >> 16)-4,8,8,SAT_RGB555(31,(i*3u)%24u,4)); }
    draw_text("PHYSICS 2D  MOVE: D-PAD  SPAWN: B", 8, 4);
    draw_number("BODIES ", g_count, 8, 14); draw_number("PAIRS ", g_pair_count, 96, 14);
    draw_number("BRUTE FORCE SAVED ", g_avoided, 8, 24);
}
int main(void) {
    sat_step_clock_t clock; uint16_t steps;
    sat_example_must(sat_app_init_default());
    sat_example_must(sat_ascii_font_init_8x8_indexed8(&g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, HUD_PALETTE));
    sat_example_must(sat_spatial_init(&g_spatial, g_heads, 10, 7, 5, g_entries, BALLS * 9, g_stamps, g_items, BALLS));
    reset(); sat_step_clock_init(&clock);
    for (;;) { sat_pad_state_t pad={0}; sat_example_must(sat_app_frame_begin(SAT_COLOR_BLACK,SAT_COLOR_BLACK,&pad));
        if (pad.pressed & SAT_PAD_START) reset();
        if (pad.pressed & SAT_PAD_B) spawn();
        if (g_count && (pad.held & (SAT_PAD_LEFT|SAT_PAD_RIGHT))) { sat_fx16_t a=(pad.held&SAT_PAD_LEFT)?-FX(1):FX(1); g_balls[0].vel.x=sat_approach(g_balls[0].vel.x,a,FX(1)/4); }
        steps=sat_step_clock_steps(&clock,4); while(steps--) physics_tick(); draw(); sat_example_must(sat_app_frame_end()); }
}
