/* Darkening limited to a window: a black colour-calculation layer shown only outside two moving beams.
 *
 * The VDP2 colour offset (the library's fade, GBA BLDCNT brighten/darken) is per screen: it cannot be
 * limited to an area, so "darken everything except these spotlights" has no direct register. The
 * answer used here, and by the sa2-saturn port for Sonic Advance's emerald shine:
 *   NBG1  seven flat colour bands, priority 3: the picture to dim
 *   NBG3  a full-screen black cell, priority 7, colour calculation on: mixed with the image below it,
 *         the result is that image times (ratio + 1) / 32. Window W0 and W1 are two line windows (one
 *         span per screen line, rewritten every frame as the beams swing); NBG3 is shown OUTSIDE both
 *         (logic AND), so only the area outside the beams is dimmed.
 * The beams are wedges from the top edge whose slope sweeps from frame 0 to frame 50 and then holds.
 * The dimming starts at half brightness and drops to a quarter at frame 90, so the harness can check
 * the ratio arithmetic at two values. The text is VDP1 sprites: they sit below the overlay and are
 * dimmed with the rest.
 * g_dim_demo holds what harness/tests/test_vdp2_window_dim.py checks, with the final span tables. */
#include <stdint.h>
#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/saturn.h"
#include "saturn/vdp2_compose.h"
#include "saturn/vdp2_layers.h"

#define DIM_DEMO_MAGIC SAT_FOURCC('D', 'I', 'M', '1')
#define LINES 224u
#define HOLD_FRAME 50u             /* the beams stop here */
#define RATIO_SWITCH_FRAME 90u
#define BELOW_A 16u                /* 32nds of the picture that shows through: 1/2 */
#define BELOW_B 8u                 /* then 1/4 */

typedef struct dim_demo_results {
    uint32_t magic;
    uint32_t layers_ok;
    uint32_t window_ok;
    uint32_t screens_ok;
    uint32_t color_calc_ok;
    uint32_t ratio_a;         /* the hardware ratio register values used */
    uint32_t ratio_b;
    uint32_t frames;
    int32_t centre100[3];     /* W0's middle dot on line 100 at frames 10, 40 and 50 */
    uint16_t spans[2][LINES][2];
} dim_demo_results_t;

volatile dim_demo_results_t g_dim_demo;

static sat_ascii_font_t font;
static uint16_t g_names[64u * 64u];
static uint16_t g_cells[2u * 16u];
static sat_vdp2_window_span_t g_spans[2][LINES];

#define NAMES_BANDS 0x02000u
#define CHARS_BANDS 0x48000u
#define NAMES_DIM 0x22000u
#define CHARS_DIM 0x68000u
#define WINDOW_TABLE0 0x10000u
#define WINDOW_TABLE1 0x11000u

/* Cell 0 transparent, cell 1 solid dot 1 (4bpp: two dots a nibble, so 0x1111 words). */
static void build_cells(void) {
    for (uint32_t i = 0u; i < 16u; ++i) {
        g_cells[i] = 0u;
        g_cells[16u + i] = 0x1111u;
    }
}

/* A flat colour band every 4 tile rows: palette 1..7 picks the colour. */
static void fill_names(uint32_t bands) {
    for (uint32_t y = 0u; y < 64u; ++y) {
        for (uint32_t x = 0u; x < 64u; ++x) {
            const uint32_t palette = bands ? 1u + ((y / 4u) % 7u) : 8u;
            g_names[y * 64u + x] = (uint16_t)((palette << 12u) | 1u);
        }
    }
}

static sat_result_t setup_layer(uint32_t layer, uint32_t names, uint32_t chars, uint8_t bank,
                                uint8_t priority, uint32_t bands) {
    sat_vdp2_layer_config_t c;
    fill_names(bands);
    SAT_TRY(sat_vdp2_vram_write_words(names / 2u, g_names, 64u * 64u));
    SAT_TRY(sat_vdp2_vram_write_words(chars / 2u, g_cells, 2u * 16u));
    sat_vdp2_layer_config_default((sat_vdp2_layer_t)layer, &c);
    for (uint32_t p = 0u; p < 4u; ++p) c.plane_address[p] = names;
    c.char_base_address = chars;
    c.char_bank_mask = bank;
    c.priority = priority;
    return sat_vdp2_layer_configure(&c);
}

static void upload_palettes(void) {
    static const uint16_t bands[7] = {
        SAT_RGB555(31, 6, 6), SAT_RGB555(31, 20, 4), SAT_RGB555(30, 30, 6), SAT_RGB555(8, 28, 8),
        SAT_RGB555(6, 26, 28), SAT_RGB555(10, 12, 31), SAT_RGB555(26, 8, 28)};
    uint16_t pal[16];
    for (uint32_t b = 0u; b < 7u; ++b) {
        for (uint32_t i = 0u; i < 16u; ++i) pal[i] = 0u;
        pal[1] = bands[b];
        (void)sat_vdp2_palette_upload(pal, 16u, (1u + b) * 16u);
    }
    for (uint32_t i = 0u; i < 16u; ++i) pal[i] = 0u;
    pal[1] = SAT_RGB555(0, 0, 0);
    (void)sat_vdp2_palette_upload(pal, 16u, 8u * 16u);
}

/* A beam from the top edge at x = apex: its centre line leans by slope/256 dots a line and it widens
 * by 40/256 a side. */
static void build_beam(uint32_t window, int32_t apex, int32_t slope) {
    for (uint32_t y = 0u; y < LINES; ++y) {
        const int32_t centre = apex + (((int32_t)y * slope) >> 8);
        const int32_t half = (int32_t)((y * 40u) >> 8) + 3;
        int32_t x0 = centre - half, x1 = centre + half;
        if (x1 < 0 || x0 > 319) {
            g_spans[window][y].x0 = 1u;   /* an empty line: everything on it is outside */
            g_spans[window][y].x1 = 0u;
            continue;
        }
        if (x0 < 0) x0 = 0;
        if (x1 > 319) x1 = 319;
        g_spans[window][y].x0 = (uint16_t)x0;
        g_spans[window][y].x1 = (uint16_t)x1;
    }
}

static sat_result_t write_beams(void) {
    SAT_TRY(sat_vdp2_window_line_write(0u, 0u, g_spans[0], LINES));
    return sat_vdp2_window_line_write(1u, 0u, g_spans[1], LINES);
}

int main(void) {
    if (sat_app_init_default() != SAT_OK) return 1;
    if (sat_ascii_font_init_8x8_indexed8(&font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u) != SAT_OK) return 1;
    build_cells();
    upload_palettes();
    g_dim_demo.magic = DIM_DEMO_MAGIC;
    g_dim_demo.ratio_a = sat_vdp2_color_calc_ratio_for_below(BELOW_A);
    g_dim_demo.ratio_b = sat_vdp2_color_calc_ratio_for_below(BELOW_B);

    g_dim_demo.layers_ok =
        setup_layer(1u, NAMES_BANDS, CHARS_BANDS, SAT_VDP2_BANK_B0, 3u, 1u) == SAT_OK &&
        setup_layer(3u, NAMES_DIM, CHARS_DIM, SAT_VDP2_BANK_B1, 7u, 0u) == SAT_OK;

    build_beam(0u, 80, -256);
    build_beam(1u, 240, 256);
    g_dim_demo.window_ok =
        sat_vdp2_window_set_line_table(0u, WINDOW_TABLE0, 0u, LINES - 1u) == SAT_OK &&
        sat_vdp2_window_set_line_table(1u, WINDOW_TABLE1, 0u, LINES - 1u) == SAT_OK &&
        write_beams() == SAT_OK;

    /* The dim layer exists only outside W0 AND outside W1. */
    {
        sat_vdp2_screen_window_t dim;
        sat_vdp2_screen_window_default(SAT_VDP2_SCREEN_NBG3, &dim);
        dim.w0 = SAT_VDP2_WINDOW_AREA_OUTSIDE;
        dim.w1 = SAT_VDP2_WINDOW_AREA_OUTSIDE;
        dim.logic_and = 1u;
        g_dim_demo.screens_ok = sat_vdp2_screen_window_set(&dim) == SAT_OK;
    }
    g_dim_demo.color_calc_ok =
        sat_vdp2_screen_color_calc_set(SAT_VDP2_SCREEN_NBG3, 1u, (uint8_t)g_dim_demo.ratio_a) == SAT_OK;

    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_wait_vblank() != SAT_OK) break;
        (void)sat_vdp2_layers_commit();
        (void)sat_vdp1_set_erase_transparent();
        if (sat_begin_frame() != SAT_OK) break;
        (void)sat_pad_poll(&pad);
        ++g_dim_demo.frames;
        {
            const uint32_t f = g_dim_demo.frames;
            const int32_t s = -256 + 6 * (int32_t)(f < HOLD_FRAME ? f : HOLD_FRAME);
            build_beam(0u, 80, s);
            build_beam(1u, 240, -s);
            (void)write_beams();
            if (f == 10u || f == 40u || f == HOLD_FRAME) {
                const uint32_t k = f == 10u ? 0u : (f == 40u ? 1u : 2u);
                g_dim_demo.centre100[k] = ((int32_t)g_spans[0][100].x0 + (int32_t)g_spans[0][100].x1) / 2;
            }
            if (f == RATIO_SWITCH_FRAME) {
                (void)sat_vdp2_screen_color_calc_set(SAT_VDP2_SCREEN_NBG3, 1u, (uint8_t)g_dim_demo.ratio_b);
            }
        }
        for (uint32_t w = 0u; w < 2u; ++w) {
            for (uint32_t y = 0u; y < LINES; ++y) {
                g_dim_demo.spans[w][y][0] = g_spans[w][y].x0;
                g_dim_demo.spans[w][y][1] = g_spans[w][y].x1;
            }
        }
        (void)sat_ascii_font_draw_text_screen_indexed8(&font, "VDP2 WINDOW DIM", 8, 204, 8, 0u, 0u);
        (void)sat_ascii_font_draw_label_u32(&font, "FRAME ", g_dim_demo.frames, 8, 214, 8, 0u, 0u);
        (void)sat_end_frame();
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }
    (void)sat_shutdown();
    return 0;
}
