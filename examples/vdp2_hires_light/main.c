/* A red light passing over VDP1 polygons in 640x224 hi-res, done entirely by the VDP2.
 *
 * Hi-res VDP1 pixels are 8-bit palette codes, so the VDP1 cannot shade them (Gouraud needs RGB
 * pixels). The VDP2 can still brighten them after they are drawn:
 *   - the line colour screen holds one CRAM colour per line (a red ramp, brightest at the light's
 *     centre line) and is inserted as the second image under the sprite layer only (LNCLEN SPLCEN),
 *     so the background never receives the light;
 *   - sprite colour calculation in add mode (CCCTL SPCCEN + CCMD) adds that red to the sprite pixel;
 *   - the colour calculation window (an ellipse in W0) limits the light to a spot that moves;
 *   - sprite type C shares bit 7 of the code with the priority-register select, so
 *     SAT_VDP2_HIRES_SPRITE_CC_HIGH_CODES lights codes 128-255 only: a polygon drawn with its colours
 *     copied to 128+ takes the light and one drawn with the plain codes does not (a per-face
 *     "facing the light" flag without losing any shading).
 * In hi-res the VDP2 mixes palette-format images only in CRAM mode 0 (manual table 12.1), so the
 * demo selects it with sat_vdp2_cram_mode_set(0) (1024 colours; it uses banks 1 and 2).
 *
 * All of it goes through the library (sat_vdp2_sprite_set_line_color_insert,
 * sat_vdp2_hires_sprite_color_calc_set, sat_vdp2_color_calc_set_add), which replays it with every
 * sat_vdp2_layers_commit(), so the calls can come in any order.
 *
 * Body: 30 columns of a 15-step warm ramp, codes 128+k, with a stripe at y 84..103 in the plain
 * codes k. The spot holds at (200, 80) until HOLD_FRAME, then sweeps. A toggles the light, B the
 * per-code mask (off: every sprite pixel is lit), C the window (off: the full width of the lines).
 * g_light_demo holds what harness/tests/test_vdp2_hires_light.py checks. */
#include <stdint.h>
#include "saturn/color.h"
#include "saturn/saturn.h"
#include "saturn/vdp2_compose.h"
#include "saturn/vdp2_layers.h"

#define LIGHT_DEMO_MAGIC SAT_FOURCC('L', 'I', 'T', '1')
#define SCREEN_W 640
#define SCREEN_H 224
#define LINES 224u
#define HOLD_FRAME 240u

#define SPRITE_BANK 1u               /* CRAM words 256..511: the sprite palette */
#define RED_BASE 512u                /* CRAM words 512..543: red ramp for the line colours */
#define LIT 0x80u                    /* code bit 7: S1PRIN, the lit priority */

#define LINE_COLOR_TABLE 0x10000u
#define WINDOW_TABLE 0x11000u

#define SPOT_RX 90
#define SPOT_RY 60
#define SPOT_PEAK 28                 /* red added at the centre line, 0..31 */

#define BODY_X 80
#define BODY_Y 30
#define BODY_H 170
#define COLUMN_W 16
#define COLUMNS 30
#define STRIPE_Y 84
#define STRIPE_H 20

typedef struct light_demo_results {
    uint32_t magic;
    uint32_t setup_ok;
    uint32_t frames;
    int32_t spot_x;
    int32_t spot_y;
    uint8_t light_on;
    uint8_t mask_on;
    uint8_t window_on;
} light_demo_results_t;

volatile light_demo_results_t g_light_demo;

static uint16_t g_line_colors[LINES];

static uint16_t ramp_color(uint32_t k) {
    /* k 1..15: dark brown to sand, dark enough that added red shows */
    return SAT_RGB555(5u + k, 3u + (k * 2u) / 3u, 2u + k / 3u);
}

static sat_result_t upload_palettes(void) {
    uint16_t pal[256];
    uint16_t red[32];
    for (uint32_t i = 0u; i < 256u; ++i) pal[i] = 0u;
    for (uint32_t k = 1u; k <= 15u; ++k) {
        pal[k] = ramp_color(k);
        pal[LIT + k] = ramp_color(k);
    }
    for (uint32_t i = 0u; i < 32u; ++i) red[i] = SAT_RGB555(i, i / 4u, 0u);
    SAT_TRY(sat_vdp2_palette_upload(pal, 256u, (uint16_t)(SPRITE_BANK * 256u)));
    return sat_vdp2_palette_upload(red, 32u, (uint16_t)RED_BASE);
}

/* Red per line: SPOT_PEAK at the spot's centre line, fading to 0 at its top and bottom. */
static void build_line_colors(int32_t cy, uint32_t on) {
    for (uint32_t y = 0u; y < LINES; ++y) {
        int32_t d = (int32_t)y - cy;
        int32_t level;
        if (d < 0) d = -d;
        level = on ? SPOT_PEAK - (d * SPOT_PEAK) / SPOT_RY : 0;
        if (level < 0) level = 0;
        g_line_colors[y] = (uint16_t)(RED_BASE + (uint32_t)level);
    }
}

static int32_t spot_x_at(uint32_t frame) {
    /* hold, then a triangle wave between x 140 and 500 (3 dots a frame) */
    const uint32_t t = frame < HOLD_FRAME ? 0u : ((frame - HOLD_FRAME) * 3u) % 720u;
    const int32_t x = t < 360u ? (int32_t)t : (int32_t)(720u - t);
    return frame < HOLD_FRAME ? 200 : 140 + x;
}

static void draw_body(void) {
    for (int32_t c = 0; c < COLUMNS; ++c) {
        const uint16_t k = (uint16_t)(1 + c % 15);
        const int16_t x = (int16_t)(BODY_X + c * COLUMN_W);
        (void)sat_draw_rect_screen(x, BODY_Y, COLUMN_W, STRIPE_Y - BODY_Y, (uint16_t)(LIT + k));
        (void)sat_draw_rect_screen(x, STRIPE_Y, COLUMN_W, STRIPE_H, k);
        (void)sat_draw_rect_screen(x, STRIPE_Y + STRIPE_H, COLUMN_W,
                                   (uint16_t)(BODY_Y + BODY_H - STRIPE_Y - STRIPE_H), (uint16_t)(LIT + k));
    }
}

/* Light on: the line colour screen under the sprites, added to the lit codes (or to every
 * sprite dot without the mask). Recorded by the library and replayed every commit. */
static sat_result_t apply_light(void) {
    const sat_vdp2_hires_sprite_cc_t mode =
        !g_light_demo.light_on ? SAT_VDP2_HIRES_SPRITE_CC_OFF
        : g_light_demo.mask_on ? SAT_VDP2_HIRES_SPRITE_CC_HIGH_CODES
                               : SAT_VDP2_HIRES_SPRITE_CC_ALL;
    SAT_TRY(sat_vdp2_sprite_set_line_color_insert((uint8_t)g_light_demo.light_on));
    return sat_vdp2_hires_sprite_color_calc_set(mode, 0u);
}

static sat_result_t place_spot(int32_t cx, int32_t cy) {
    if (g_light_demo.window_on) {
        return sat_vdp2_window_set_ellipse(0u, WINDOW_TABLE, cx, cy, SPOT_RX, SPOT_RY);
    }
    {
        /* no window: the light covers the whole of every line it reaches */
        const sat_vdp2_window_rect_t all = {0u, 0u, SCREEN_W - 1u, SCREEN_H - 1u};
        return sat_vdp2_window_set_rect(0u, &all);
    }
}

int main(void) {
    sat_video_config_t video = {SCREEN_W, SCREEN_H, SAT_VIDEO_AUTO, 0u};
    sat_result_t ok = SAT_OK;
    if (sat_init(&video) != SAT_OK) return 1;
    g_light_demo.magic = LIGHT_DEMO_MAGIC;
    g_light_demo.light_on = 1u;
    g_light_demo.mask_on = 1u;
    g_light_demo.window_on = 1u;

    /* hi-res colour calculation with a palette-format image needs CRAM mode 0; set it before the
     * palettes go in */
    if (sat_vdp2_cram_mode_set(0u) != SAT_OK) ok = SAT_ERR_INVALID_ARG;
    if (upload_palettes() != SAT_OK) ok = SAT_ERR_INVALID_ARG;
    if (sat_vdp2_sprite_palette_bank_set(SPRITE_BANK) != SAT_OK) ok = SAT_ERR_INVALID_ARG;
    if (sat_vdp2_back_color_set(SAT_RGB555(1, 2, 6)) != SAT_OK) ok = SAT_ERR_INVALID_ARG;
    build_line_colors(80, 1u);
    if (sat_vdp2_line_color_screen_set(LINE_COLOR_TABLE, g_line_colors, LINES) != SAT_OK) {
        ok = SAT_ERR_INVALID_ARG;
    }
    if (place_spot(200, 80) != SAT_OK) ok = SAT_ERR_INVALID_ARG;
    {
        /* colour calculation applies inside W0 only */
        sat_vdp2_screen_window_t cc;
        sat_vdp2_screen_window_default(SAT_VDP2_SCREEN_COLOR_CALC, &cc);
        cc.w0 = SAT_VDP2_WINDOW_AREA_INSIDE;
        if (sat_vdp2_screen_window_set(&cc) != SAT_OK) ok = SAT_ERR_INVALID_ARG;
    }
    if (sat_vdp2_color_calc_set_add(1u) != SAT_OK) ok = SAT_ERR_INVALID_ARG;
    if (apply_light() != SAT_OK) ok = SAT_ERR_INVALID_ARG;
    g_light_demo.setup_ok = ok == SAT_OK;

    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_wait_vblank() != SAT_OK) break;
        (void)sat_vdp2_layers_commit();
        {
            const int32_t cx = spot_x_at(g_light_demo.frames);
            const int32_t cy = 80;
            build_line_colors(cy, g_light_demo.light_on);
            (void)sat_vdp2_line_color_screen_set(LINE_COLOR_TABLE, g_line_colors, LINES);
            (void)place_spot(cx, cy);
            g_light_demo.spot_x = cx;
            g_light_demo.spot_y = cy;
        }
        (void)sat_vdp1_set_erase_transparent();
        if (sat_begin_frame() != SAT_OK) break;
        (void)sat_pad_poll(&pad);
        ++g_light_demo.frames;
        sat_pad_toggle(&pad, SAT_PAD_A, &g_light_demo.light_on);
        sat_pad_toggle(&pad, SAT_PAD_B, &g_light_demo.mask_on);
        sat_pad_toggle(&pad, SAT_PAD_C, &g_light_demo.window_on);
        if ((pad.pressed & (SAT_PAD_A | SAT_PAD_B)) != 0u) (void)apply_light();
        draw_body();
        (void)sat_end_frame();
    }
    (void)sat_shutdown();
    return 0;
}
