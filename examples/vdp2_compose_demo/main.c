/* VDP2 composition: windows, mosaic and colour calculation per screen.
 *
 *   NBG0  yellow vertical bars, priority 5: shown inside an ellipse (a line
 *         window, W0) AND outside a rectangle (W1), so the ellipse is cut
 *         straight at the rectangle's left edge
 *   NBG1  red horizontal stripes, priority 4: colour calculation, half of the
 *         image below shows through the stripes
 *   NBG2  green checkerboard, priority 3: shown only inside the rectangle W1
 *   NBG3  blue cells, priority 2: 8x8 mosaic, so every 8x8 block of the
 *         screen is one flat colour
 * From frame 150 a colour calculation window limits the stripes' blend to
 * the rectangle: outside it they turn opaque red.
 * g_compose_demo holds what harness/tests/test_vdp2_compose.py checks. */
#include <stdint.h>
#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/saturn.h"
#include "saturn/vdp2_compose.h"
#include "saturn/vdp2_layers.h"

#define COMPOSE_DEMO_MAGIC 0x434D5031u /* "CMP1" */

typedef struct compose_demo_results {
    uint32_t magic;
    uint32_t layers_ok;
    uint32_t window_ok;
    uint32_t screens_ok;
    uint32_t mosaic_ok;
    uint32_t color_calc_ok;
    uint32_t refused_bad_window;      /* window 2 does not exist */
    uint32_t refused_undefined;       /* a screen cannot use a window that was never defined */
    uint32_t refused_mosaic_size;     /* 17 dots */
    uint32_t refused_mosaic_on_vcs;   /* mosaic and vertical cell scroll exclude each other */
    uint32_t refused_vcs_on_mosaic;
    uint32_t cc_window_ok;            /* set at frame 150 */
    uint32_t frames;
} compose_demo_results_t;

volatile compose_demo_results_t g_compose_demo;

static sat_ascii_font_t font;
static uint16_t g_map[64u * 64u];
static uint16_t g_tiles[8u * 16u];

#define NAMES0 0x00000u
#define NAMES1 0x02000u
#define NAMES2 0x20000u
#define NAMES3 0x22000u
#define CHARS0 0x40000u
#define CHARS1 0x48000u
#define CHARS2 0x60000u
#define CHARS3 0x68000u
#define VCS_TABLE 0x0F000u
#define WINDOW_TABLE 0x10000u

#define ELLIPSE_CX 100
#define ELLIPSE_CY 128
#define ELLIPSE_RX 90
#define ELLIPSE_RY 70
#define RECT_X0 176u
#define RECT_Y0 48u
#define RECT_X1 311u
#define RECT_Y1 207u

/* Cell 0 transparent, 1 checker, 2 vertical bar, 3 horizontal stripe,
 * 4..7 four blue variants whose top-left dot differs. */
static void build_tiles(void) {
    for (uint32_t k = 0u; k < 8u; ++k) {
        for (uint32_t row = 0u; row < 8u; ++row) {
            uint32_t p[8];
            for (uint32_t x = 0u; x < 8u; ++x) {
                uint32_t v = 0u;
                if (k == 1u) v = (((x >> 2u) + (row >> 2u)) & 1u) != 0u ? 1u : 0u;
                else if (k == 2u) v = x < 4u ? 1u : 0u;
                else if (k == 3u) v = row < 4u ? 1u : 0u;
                else if (k >= 4u) v = 1u + ((x + 2u * row + (k - 4u)) & 3u);
                p[x] = v;
            }
            g_tiles[k * 16u + row * 2u] = (uint16_t)((p[0] << 12u) | (p[1] << 8u) | (p[2] << 4u) | p[3]);
            g_tiles[k * 16u + row * 2u + 1u] = (uint16_t)((p[4] << 12u) | (p[5] << 8u) | (p[6] << 4u) | p[7]);
        }
    }
}

static void fill_map(uint32_t layer, uint32_t palette) {
    for (uint32_t y = 0u; y < 64u; ++y) {
        for (uint32_t x = 0u; x < 64u; ++x) {
            uint32_t tile = 0u;
            switch (layer) {
                case 0u: tile = (x % 5u) == 0u ? 2u : 0u; break;
                case 1u: tile = (y % 4u) == 0u ? 3u : 0u; break;
                case 2u: tile = 1u; break;
                default: tile = 4u + ((x * 3u + y * 5u) & 3u); break;
            }
            g_map[y * 64u + x] = (uint16_t)((palette << 12u) | tile);
        }
    }
}

static sat_result_t setup_layer(uint32_t layer, uint32_t names, uint32_t chars, uint8_t bank,
                                uint8_t priority, const uint16_t* colours, uint32_t colour_count,
                                uint8_t vcs) {
    sat_vdp2_layer_config_t c;
    uint16_t pal[16];
    const uint32_t palette = layer + 1u;
    for (uint32_t i = 0u; i < 16u; ++i) pal[i] = 0u;
    for (uint32_t i = 0u; i < colour_count; ++i) pal[1u + i] = colours[i];
    SAT_TRY(sat_vdp2_palette_upload(pal, 16u, palette * 16u));
    fill_map(layer, palette);
    SAT_TRY(sat_vdp2_vram_write_words(names / 2u, g_map, 64u * 64u));
    SAT_TRY(sat_vdp2_vram_write_words(chars / 2u, g_tiles, 8u * 16u));
    sat_vdp2_layer_config_default((sat_vdp2_layer_t)layer, &c);
    for (uint32_t p = 0u; p < 4u; ++p) c.plane_address[p] = names;
    c.char_base_address = chars;
    c.char_bank_mask = bank;
    c.priority = priority;
    if (vcs) {
        c.vertical_cell_scroll = 1u;
        c.vertical_cell_scroll_address = VCS_TABLE;
    }
    return sat_vdp2_layer_configure(&c);
}

static void line(const char* label, uint32_t value, int y) {
    (void)sat_ascii_font_draw_label_u32(&font, label, value, 8, y, 8, 0u, 0u);
}

int main(void) {
    static const uint16_t yellow[1] = {SAT_RGB555(31, 28, 3)};
    static const uint16_t red[1] = {SAT_RGB555(31, 6, 6)};
    static const uint16_t green[1] = {SAT_RGB555(6, 28, 8)};
    static const uint16_t blues[4] = {SAT_RGB555(3, 6, 20), SAT_RGB555(8, 12, 30),
                                      SAT_RGB555(4, 16, 26), SAT_RGB555(12, 8, 22)};
    if (sat_app_init_default() != SAT_OK) return 1;
    if (sat_ascii_font_init_8x8_indexed8(&font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u) != SAT_OK) return 1;
    build_tiles();
    g_compose_demo.magic = COMPOSE_DEMO_MAGIC;

    /* Mosaic and vertical cell scroll exclude each other, in both orders. */
    (void)setup_layer(1u, NAMES1, CHARS1, SAT_VDP2_BANK_B0, 4u, red, 1u, 1u);
    g_compose_demo.refused_mosaic_on_vcs = sat_vdp2_screen_set_mosaic(SAT_VDP2_SCREEN_NBG1, 1u) == SAT_ERR_UNSUPPORTED;
    (void)setup_layer(1u, NAMES1, CHARS1, SAT_VDP2_BANK_B0, 4u, red, 1u, 0u);
    g_compose_demo.refused_vcs_on_mosaic = 0u;
    if (sat_vdp2_screen_set_mosaic(SAT_VDP2_SCREEN_NBG1, 1u) == SAT_OK) {
        g_compose_demo.refused_vcs_on_mosaic =
            setup_layer(1u, NAMES1, CHARS1, SAT_VDP2_BANK_B0, 4u, red, 1u, 1u) == SAT_ERR_UNSUPPORTED;
    }
    (void)sat_vdp2_screen_set_mosaic(SAT_VDP2_SCREEN_NBG1, 0u);
    (void)setup_layer(1u, NAMES1, CHARS1, SAT_VDP2_BANK_B0, 4u, red, 1u, 0u);

    g_compose_demo.layers_ok =
        setup_layer(0u, NAMES0, CHARS0, SAT_VDP2_BANK_B0, 5u, yellow, 1u, 0u) == SAT_OK &&
        setup_layer(1u, NAMES1, CHARS1, SAT_VDP2_BANK_B0, 4u, red, 1u, 0u) == SAT_OK &&
        setup_layer(2u, NAMES2, CHARS2, SAT_VDP2_BANK_B1, 3u, green, 1u, 0u) == SAT_OK &&
        setup_layer(3u, NAMES3, CHARS3, SAT_VDP2_BANK_B1, 2u, blues, 4u, 0u) == SAT_OK;

    /* W0: an ellipse from a line table; W1: a rectangle. */
    {
        sat_vdp2_window_rect_t rect = {RECT_X0, RECT_Y0, RECT_X1, RECT_Y1};
        sat_vdp2_screen_window_t undefined = {SAT_VDP2_SCREEN_NBG2, SAT_VDP2_WINDOW_AREA_INSIDE,
                                              SAT_VDP2_WINDOW_AREA_OFF, 0u};
        g_compose_demo.refused_bad_window = sat_vdp2_window_set_rect(2u, &rect) == SAT_ERR_INVALID_ARG;
        g_compose_demo.refused_undefined = sat_vdp2_screen_window_set(&undefined) == SAT_ERR_INVALID_ARG;
        g_compose_demo.window_ok =
            sat_vdp2_window_set_ellipse(0u, WINDOW_TABLE, ELLIPSE_CX, ELLIPSE_CY, ELLIPSE_RX, ELLIPSE_RY) == SAT_OK &&
            sat_vdp2_window_set_rect(1u, &rect) == SAT_OK;
    }
    {
        sat_vdp2_screen_window_t nbg0 = {SAT_VDP2_SCREEN_NBG0, SAT_VDP2_WINDOW_AREA_INSIDE,
                                         SAT_VDP2_WINDOW_AREA_OUTSIDE, 1u};
        sat_vdp2_screen_window_t nbg2 = {SAT_VDP2_SCREEN_NBG2, SAT_VDP2_WINDOW_AREA_OFF,
                                         SAT_VDP2_WINDOW_AREA_INSIDE, 0u};
        g_compose_demo.screens_ok = sat_vdp2_screen_window_set(&nbg0) == SAT_OK &&
                                    sat_vdp2_screen_window_set(&nbg2) == SAT_OK;
    }
    g_compose_demo.refused_mosaic_size = sat_vdp2_mosaic_set_size(17u, 4u) == SAT_ERR_INVALID_ARG;
    g_compose_demo.mosaic_ok = sat_vdp2_mosaic_set_size(8u, 8u) == SAT_OK &&
                               sat_vdp2_screen_set_mosaic(SAT_VDP2_SCREEN_NBG3, 1u) == SAT_OK;
    g_compose_demo.color_calc_ok =
        sat_vdp2_screen_color_calc_set(SAT_VDP2_SCREEN_NBG1, 1u, sat_vdp2_color_calc_ratio_for_below(17u)) == SAT_OK;

    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_wait_vblank() != SAT_OK) break;
        (void)sat_vdp2_layers_commit();
        (void)sat_vdp1_set_erase_transparent();
        if (sat_begin_frame() != SAT_OK) break;
        (void)sat_pad_poll(&pad);
        ++g_compose_demo.frames;
        if (g_compose_demo.frames == 150u) {
            sat_vdp2_screen_window_t cc = {SAT_VDP2_SCREEN_COLOR_CALC, SAT_VDP2_WINDOW_AREA_OFF,
                                           SAT_VDP2_WINDOW_AREA_INSIDE, 0u};
            g_compose_demo.cc_window_ok = sat_vdp2_screen_window_set(&cc) == SAT_OK;
        }
        (void)sat_ascii_font_draw_text_screen_indexed8(&font, "VDP2 COMPOSE", 8, 8, 8, 0u, 0u);
        line("FRAME ", g_compose_demo.frames, 24);
        (void)sat_end_frame();
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }
    (void)sat_shutdown();
    return 0;
}
