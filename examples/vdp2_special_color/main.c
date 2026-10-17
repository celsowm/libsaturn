/* VDP2 special colour: normal ratio blending, add mode and the extended
 * colour calculation cascade (manual chapter 12).
 *
 *   NBG0  yellow vertical bars, priority 5 (the top image)
 *   NBG1  red horizontal stripes, priority 4 (the second image)
 *   NBG2  green fill (solid), priority 3 (the third image, opaque everywhere)
 *
 * A colour calculation window (W0, the left half) confines every effect to
 * x < 160, so each mode shows the effect on the left and the plain stack on
 * the right at the same time.
 *
 * The scene runs in CRAM mode 0 on purpose: table 12.2 only extends a
 * palette-format second image (2:2:0 second:third) in CRAM mode 0; modes
 * 1 and 2 give such dots 4:0:0, i.e. no cascade at all.
 *
 * Timeline (auto; A on the pad cycles modes by hand):
 *   PLAIN    no colour calculation at all
 *   frame  60  RATIO    NBG0 blends over the image below by CCRNA.N0 (15:17)
 *   frame 120  ADD      CCMD: top + second added as they are and saturated
 *   frame 180  EXT      EXCCEN: second = (NBG1 + NBG2) / 2 (table 12.2),
 *                       then NBG0 blends over that pre-mixed pair
 *
 * g_special_color holds what harness/tests/test_vdp2_special_color.py checks,
 * including the chapter-12 mutual-exclusion refusals. */
#include <stdint.h>
#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/saturn.h"
#include "saturn/vdp2_compose.h"
#include "saturn/vdp2_layers.h"

#define SPECIAL_COLOR_MAGIC SAT_FOURCC('S', 'P', 'C', '1')

typedef struct special_color_results {
    uint32_t magic;
    uint32_t cram_mode_ok;               /* CRAM mode 0 before the palettes */
    uint32_t layers_ok;
    uint32_t cc_window_ok;               /* W0 left half confines colour calculation */
    uint32_t refused_ratio_range;        /* ratio 32                                 -> SAT_ERR_INVALID_ARG */
    uint32_t refused_screen_range;       /* the sprite screen is not a scroll screen -> SAT_ERR_INVALID_ARG */
    uint32_t refused_blur_screen;        /* BOKN 3 is setting-prohibited (p12_14)    -> SAT_ERR_UNSUPPORTED */
    uint32_t refused_insert_behind_blur; /* line-colour insert refused while blur is on (p12_13) */
    uint32_t extended_clears_blur;       /* the same insert succeeds once EXCCEN cleared the blur */
    uint32_t mode_ok;                    /* bit n set when mode n applied cleanly */
    uint32_t mode;                       /* last applied mode */
    uint32_t ratio;                      /* last applied top-image ratio */
    uint32_t frames;
} special_color_results_t;

volatile special_color_results_t g_special_color;

static sat_ascii_font_t font;
static uint16_t g_map[64u * 64u];
static uint16_t g_tiles[4u * 16u];

#define NAMES0 0x00000u
#define NAMES1 0x02000u
#define NAMES2 0x20000u
#define CHARS0 0x40000u
#define CHARS1 0x48000u
#define CHARS2 0x60000u

#define MODE_PLAIN 0u
#define MODE_RATIO 1u
#define MODE_ADD   2u
#define MODE_EXT   3u
#define MODE_COUNT 4u
static const uint8_t kModeRatio[MODE_COUNT] = {16u, 16u, 16u, 8u};
static const char*const kModeName[MODE_COUNT] = {"PLAIN", "RATIO", "ADD", "EXT"};
/* Auto-advance frames: mode n+1 starts at kAutoAt[n]. */
static const uint32_t kAutoAt[MODE_COUNT - 1u] = {60u, 120u, 180u};

/* Cell 0 transparent, 1 checker, 2 vertical bar, 3 horizontal stripe - the
 * same three shapes the compose demo uses, so one tile set serves all three
 * palettes. */
static void build_tiles(void) {
    for (uint32_t k = 0u; k < 4u; ++k) {
        for (uint32_t row = 0u; row < 8u; ++row) {
            uint32_t p[8];
            for (uint32_t x = 0u; x < 8u; ++x) {
                uint32_t v = 0u;
                if (k == 1u) v = 1u;                               /* solid: the third image never shows the backdrop */
                else if (k == 2u) v = x < 4u ? 1u : 0u;
                else if (k == 3u) v = row < 4u ? 1u : 0u;
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
                case 0u: tile = (x % 5u) == 0u ? 2u : 0u; break;   /* bars every 5 cells */
                case 1u: tile = (y % 4u) == 0u ? 3u : 0u; break;   /* stripes every 4 cells */
                default: tile = 1u; break;                          /* opaque checker */
            }
            g_map[y * 64u + x] = (uint16_t)((palette << 12u) | tile);
        }
    }
}

static sat_result_t setup_layer(uint32_t layer, uint32_t names, uint32_t chars, uint8_t bank,
                                uint8_t priority, const uint16_t* colours, uint32_t colour_count) {
    sat_vdp2_layer_config_t c;
    uint16_t pal[16];
    const uint32_t palette = layer + 1u;
    for (uint32_t i = 0u; i < 16u; ++i) pal[i] = 0u;
    for (uint32_t i = 0u; i < colour_count; ++i) pal[1u + i] = colours[i];
    SAT_TRY(sat_vdp2_palette_upload(pal, 16u, palette * 16u));
    fill_map(layer, palette);
    SAT_TRY(sat_vdp2_vram_write_words(names / 2u, g_map, 64u * 64u));
    SAT_TRY(sat_vdp2_vram_write_words(chars / 2u, g_tiles, 4u * 16u));
    sat_vdp2_layer_config_default((sat_vdp2_layer_t)layer, &c);
    for (uint32_t p = 0u; p < 4u; ++p) c.plane_address[p] = names;
    c.char_base_address = chars;
    c.char_bank_mask = bank;
    c.priority = priority;
    return sat_vdp2_layer_configure(&c);
}

/* Clears the whole colour-calculation contract we own, then applies one
 * mode: the top-image enable and ratio (CCRNA.N0), the add bit (CCMD), the
 * second-image enable (N1CCEN, the extended cascade's third+second mix) and
 * EXCCEN. Always clearing first keeps every mode independent of the last. */
static sat_result_t apply_mode(uint8_t mode, uint8_t ratio) {
    SAT_TRY(sat_vdp2_extended_color_calc_set(0u));
    SAT_TRY(sat_vdp2_color_calc_set_add(0u));
    SAT_TRY(sat_vdp2_screen_color_calc_set(SAT_VDP2_SCREEN_NBG1, 0u, 0u));
    SAT_TRY(sat_vdp2_screen_color_calc_set(SAT_VDP2_SCREEN_NBG0, 0u, 0u));
    switch (mode) {
        case MODE_RATIO:
            SAT_TRY(sat_vdp2_screen_color_calc_set(SAT_VDP2_SCREEN_NBG0, 1u, ratio));
            break;
        case MODE_ADD:
            SAT_TRY(sat_vdp2_color_calc_set_add(1u));
            SAT_TRY(sat_vdp2_screen_color_calc_set(SAT_VDP2_SCREEN_NBG0, 1u, ratio));
            break;
        case MODE_EXT:
            /* Table 12.2 (CRAM mode 0, no line colour, palette images): with
             * the second image's enable on, second = (NBG1 + NBG2) / 2; the
             * top image then blends over that pre-mixed pair by its ratio.
             * NBG1's own ratio is 31 (show only the image below), so the
             * dots where NBG1 is itself the top image show NBG2: the
             * stripes survive only under the bars on the effect side. */
            SAT_TRY(sat_vdp2_screen_color_calc_set(SAT_VDP2_SCREEN_NBG1, 1u, 31u));
            SAT_TRY(sat_vdp2_screen_color_calc_set(SAT_VDP2_SCREEN_NBG0, 1u, ratio));
            SAT_TRY(sat_vdp2_extended_color_calc_set(1u));
            break;
        default:
            break;
    }
    return SAT_OK;
}

int main(void) {
    static const uint16_t yellow[1] = {SAT_RGB555(31, 28, 3)};
    static const uint16_t red[1] = {SAT_RGB555(31, 6, 6)};
    static const uint16_t green[1] = {SAT_RGB555(6, 28, 8)};
    if (sat_app_init_default() != SAT_OK) return 1;
    if (sat_ascii_font_init_8x8_indexed8(&font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u) != SAT_OK) return 1;
    build_tiles();
    g_special_color.magic = SPECIAL_COLOR_MAGIC;

    /* Table 12.2: only CRAM mode 0 extends a palette-format second image.
     * The mode must be chosen before the palettes are uploaded. */
    g_special_color.cram_mode_ok =
        sat_vdp2_cram_mode_set(0u) == SAT_OK && sat_vdp2_cram_mode() == 0u;

    g_special_color.layers_ok =
        setup_layer(0u, NAMES0, CHARS0, SAT_VDP2_BANK_B0, 5u, yellow, 1u) == SAT_OK &&
        setup_layer(1u, NAMES1, CHARS1, SAT_VDP2_BANK_B0, 4u, red, 1u) == SAT_OK &&
        setup_layer(2u, NAMES2, CHARS2, SAT_VDP2_BANK_B1, 3u, green, 1u) == SAT_OK;

    /* The colour calculation window: W0 as a rectangle over the left half,
     * and colour calculation applies where that window shows the CC screen -
     * so the left half carries the effect and the right half stays plain. */
    {
        sat_vdp2_window_rect_t rect = {0u, 0u, 159u, 223u};
        sat_vdp2_screen_window_t cc = {SAT_VDP2_SCREEN_COLOR_CALC, SAT_VDP2_WINDOW_AREA_INSIDE,
                                       SAT_VDP2_WINDOW_AREA_OFF, 0u};
        g_special_color.cc_window_ok =
            sat_vdp2_window_set_rect(0u, &rect) == SAT_OK &&
            sat_vdp2_screen_window_set(&cc) == SAT_OK;
    }

    /* Chapter-12 refusals, all before any mode runs so the display is
     * untouched by them. */
    g_special_color.refused_ratio_range =
        sat_vdp2_screen_color_calc_set(SAT_VDP2_SCREEN_NBG0, 1u, 32u) == SAT_ERR_INVALID_ARG;
    g_special_color.refused_screen_range =
        sat_vdp2_screen_color_calc_set(SAT_VDP2_SCREEN_SPRITE, 1u, 0u) == SAT_ERR_INVALID_ARG;
    g_special_color.refused_blur_screen =
        sat_vdp2_hardware_blur_set(3u, 1u) == SAT_ERR_UNSUPPORTED;
    /* The blur / line-colour exclusion runs both ways (p12_13): a live blur
     * refuses the insert, and the extended colour calculation clears the
     * blur, after which the same insert succeeds. */
    g_special_color.refused_insert_behind_blur = 0u;
    g_special_color.extended_clears_blur = 0u;
    if (sat_vdp2_hardware_blur_set(0u, 1u) == SAT_OK) {
        g_special_color.refused_insert_behind_blur =
            sat_vdp2_layer_set_line_color_insert(SAT_VDP2_NBG2, 1u) == SAT_ERR_UNSUPPORTED;
        if (sat_vdp2_extended_color_calc_set(1u) == SAT_OK) {
            g_special_color.extended_clears_blur =
                sat_vdp2_layer_set_line_color_insert(SAT_VDP2_NBG2, 1u) == SAT_OK;
            (void)sat_vdp2_layer_set_line_color_insert(SAT_VDP2_NBG2, 0u);
        }
    }

    uint8_t mode = MODE_PLAIN;
    uint8_t ratio = kModeRatio[MODE_PLAIN];
    if (apply_mode(mode, ratio) == SAT_OK) g_special_color.mode_ok = 1u;
    g_special_color.mode = mode;
    g_special_color.ratio = ratio;

    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_wait_vblank() != SAT_OK) break;
        (void)sat_vdp2_layers_commit();
        (void)sat_vdp1_set_erase_transparent();
        if (sat_begin_frame() != SAT_OK) break;
        (void)sat_pad_poll(&pad);
        ++g_special_color.frames;
        uint8_t next = mode;
        if ((pad.pressed & SAT_PAD_A) != 0u) {
            next = (uint8_t)((mode + 1u) % MODE_COUNT);
        } else if (mode + 1u < MODE_COUNT && g_special_color.frames == kAutoAt[mode]) {
            next = (uint8_t)(mode + 1u);
        }
        if (next != mode) {
            mode = next;
            ratio = kModeRatio[mode];
            if (apply_mode(mode, ratio) == SAT_OK) {
                g_special_color.mode_ok = g_special_color.mode_ok | (1u << mode);
            }
            g_special_color.mode = mode;
            g_special_color.ratio = ratio;
        }
        (void)sat_ascii_font_draw_text_screen_indexed8(&font, "VDP2 SPECIAL COLOR", 8, 8, 8, 0u, 0u);
        (void)sat_ascii_font_draw_text_screen_indexed8(&font, kModeName[mode], 8, 24, 8, 0u, 0u);
        (void)sat_ascii_font_draw_label_u32(&font, "RATIO ", ratio, 80, 24, 8, 0u, 0u);
        (void)sat_ascii_font_draw_label_u32(&font, "FRAME ", g_special_color.frames, 8, 40, 8, 0u, 0u);
        (void)sat_end_frame();
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }
    (void)sat_shutdown();
    return 0;
}

