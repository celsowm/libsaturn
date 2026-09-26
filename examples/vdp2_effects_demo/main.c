/* VDP2 raster effects: what the VDP2 does per line and per column at no CPU
 * cost while it draws.
 *
 *   NBG0  yellow vertical bars, line scroll: every line is shifted by a sine
 *         (a wobbling curtain); the phase advances every frame
 *   NBG1  red horizontal stripes, vertical cell scroll: every 8-dot column is
 *         shifted up or down by a sine (a rippling banner)
 *   back  screen colour per line: a blue gradient behind both
 * g_effects_demo holds what harness/tests/test_vdp2_effects.py checks. */
#include <stdint.h>
#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/saturn.h"
#include "saturn/math3d.h"
#include "saturn/vdp2_layers.h"

#define EFFECTS_DEMO_MAGIC 0x45464631u /* "EFF1" */
#ifndef EFFECTS_ANIMATE
#define EFFECTS_ANIMATE 0
#endif
#define SCREEN_LINES 224u
#define CELL_COLUMNS 64u

typedef struct effects_demo_results {
    uint32_t magic;
    uint32_t layers_ok;
    uint32_t line_scroll_ok;
    uint32_t vcs_ok;
    uint32_t back_ok;
    uint32_t refused_bad_layer;   /* line scroll on NBG2 refused */
    uint32_t refused_unsupported; /* wave on a layer without horizontal line scroll */
    uint32_t table_bytes;
    uint32_t frames;
} effects_demo_results_t;

volatile effects_demo_results_t g_effects_demo;

static sat_ascii_font_t font;
static uint16_t g_map[64u * 64u];
static uint16_t g_tiles[6u * 16u];
static uint16_t g_back[SCREEN_LINES];

#define NAMES0 0x00000u
#define NAMES1 0x02000u
#define CHARS0 0x40000u
#define CHARS1 0x48000u
#define VCS_TABLE 0x0F000u
#define LINE_TABLE 0x10000u

/* Cell 0 transparent, 4 vertical bar, 5 horizontal stripe (4 bits per dot). */
static void build_tiles(void) {
    for (uint32_t k = 0u; k < 6u; ++k) {
        for (uint32_t row = 0u; row < 8u; ++row) {
            uint32_t p[8];
            for (uint32_t x = 0u; x < 8u; ++x) {
                p[x] = k == 4u ? (x < 4u ? 1u : 0u) : (k == 5u ? (row < 4u ? 1u : 0u) : 0u);
            }
            g_tiles[k * 16u + row * 2u] = (uint16_t)((p[0] << 12u) | (p[1] << 8u) | (p[2] << 4u) | p[3]);
            g_tiles[k * 16u + row * 2u + 1u] = (uint16_t)((p[4] << 12u) | (p[5] << 8u) | (p[6] << 4u) | p[7]);
        }
    }
}

static sat_result_t setup_layer(uint32_t layer, uint32_t names, uint32_t chars, uint32_t palette,
                                uint16_t colour, uint8_t priority, uint32_t tile_for_row_col) {
    sat_vdp2_layer_config_t c;
    uint16_t pal[16];
    for (uint32_t y = 0u; y < 64u; ++y) {
        for (uint32_t x = 0u; x < 64u; ++x) {
            uint32_t tile = 0u;
            if (tile_for_row_col == 4u) tile = (x % 5u) == 0u ? 4u : 0u;        /* bars every 5 cells */
            else tile = (y % 4u) == 0u ? 5u : 0u;                               /* stripes every 4 cells */
            g_map[y * 64u + x] = (uint16_t)((palette << 12u) | tile);
        }
    }
    for (uint32_t i = 0u; i < 16u; ++i) pal[i] = 0u;
    pal[1] = colour;
    SAT_TRY(sat_vdp2_palette_upload(pal, 16u, palette * 16u));
    SAT_TRY(sat_vdp2_vram_write_words(names / 2u, g_map, 64u * 64u));
    SAT_TRY(sat_vdp2_vram_write_words(chars / 2u, g_tiles, 6u * 16u));
    sat_vdp2_layer_config_default((sat_vdp2_layer_t)layer, &c);
    for (uint32_t p = 0u; p < 4u; ++p) c.plane_address[p] = names;
    c.char_base_address = chars;
    c.char_bank_mask = SAT_VDP2_BANK_B0;
    c.priority = priority;
    if (layer == 1u) {
        c.vertical_cell_scroll = 1u;
        c.vertical_cell_scroll_address = VCS_TABLE;
    }
    return sat_vdp2_layer_configure(&c);
}

static void line(const char* label, uint32_t value, int y) {
    (void)sat_ascii_font_draw_label_u32(&font, label, value, 8, y, 8, 0u, 0u);
}

int main(void) {
    int32_t phase = 0;
    if (sat_app_init_default() != SAT_OK) return 1;
    if (sat_ascii_font_init_8x8_indexed8(&font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u) != SAT_OK) return 1;
    build_tiles();
    g_effects_demo.magic = EFFECTS_DEMO_MAGIC;

    g_effects_demo.layers_ok =
        setup_layer(0u, NAMES0, CHARS0, 1u, SAT_RGB555(31, 28, 3), 4u, 4u) == SAT_OK &&
        setup_layer(1u, NAMES1, CHARS1, 2u, SAT_RGB555(31, 6, 6), 3u, 5u) == SAT_OK;

    /* NBG0: a table entry per line, horizontal scroll only. */
    {
        sat_vdp2_line_scroll_config_t ls = {0};
        ls.layer = SAT_VDP2_NBG0;
        ls.horizontal = 1u;
        ls.interval = 0u;
        ls.table_address = LINE_TABLE;
        g_effects_demo.table_bytes = sat_vdp2_line_scroll_table_bytes(&ls, SCREEN_LINES);
        g_effects_demo.line_scroll_ok = sat_vdp2_layer_line_scroll_enable(&ls) == SAT_OK &&
            sat_vdp2_line_scroll_fill_wave(SAT_VDP2_NBG0, SCREEN_LINES, 12 << 16, 56u, 0) == SAT_OK;
        /* Line scroll belongs to NBG0/NBG1 only; a wave needs horizontal entries. */
        ls.layer = SAT_VDP2_NBG2;
        g_effects_demo.refused_bad_layer = sat_vdp2_layer_line_scroll_enable(&ls) == SAT_ERR_INVALID_ARG;
        g_effects_demo.refused_unsupported =
            sat_vdp2_line_scroll_fill_wave(SAT_VDP2_NBG1, SCREEN_LINES, 12 << 16, 56u, 0) == SAT_ERR_UNSUPPORTED;
    }

    /* NBG1: a vertical scroll per 8-dot column. */
    {
        int32_t values[CELL_COLUMNS];
        for (uint32_t i = 0u; i < CELL_COLUMNS; ++i) {
            values[i] = (int32_t)(((int64_t)sat_sin_deg((sat_fx16_t)((int32_t)(i * 36u) << 16)) * (6 << 16)) >> 16);
        }
        g_effects_demo.vcs_ok =
            sat_vdp2_vertical_cell_scroll_write(SAT_VDP2_NBG1, 0u, values, CELL_COLUMNS) == SAT_OK;
    }

    /* Back screen: dark to light blue down the screen. */
    for (uint32_t y = 0u; y < SCREEN_LINES; ++y) {
        g_back[y] = SAT_RGB555(2, 4 + (y * 20u) / SCREEN_LINES, 10 + (y * 20u) / SCREEN_LINES);
    }
    g_effects_demo.back_ok = sat_vdp2_back_screen_set_lines(0x20000u, g_back, SCREEN_LINES) == SAT_OK;

    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_wait_vblank() != SAT_OK) break;
        (void)sat_vdp2_layers_commit();
        (void)sat_vdp1_set_erase_transparent();
        if (sat_begin_frame() != SAT_OK) break;
        (void)sat_pad_poll(&pad);
        phase += 6 << 16;
        if (EFFECTS_ANIMATE) (void)sat_vdp2_line_scroll_fill_wave(SAT_VDP2_NBG0, SCREEN_LINES, 12 << 16, 56u, phase);
        ++g_effects_demo.frames;
        (void)sat_ascii_font_draw_text_screen_indexed8(&font, "VDP2 EFFECTS", 8, 8, 8, 0u, 0u);
        line("FRAME ", g_effects_demo.frames, 24);
        (void)sat_end_frame();
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }
    (void)sat_shutdown();
    return 0;
}
