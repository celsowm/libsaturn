/* VDP2 hardware horizontal blur (CCCTL BOKEN/BOKN), manual p12_13/p12_14.
 *
 * The blur result is forced into the second image, and only a colour-
 * calculated top image can show it. This scene therefore makes the blurred
 * screen the top image:
 *
 *   NBG0  one-pixel black/white checker, priority 5, opaque everywhere
 *   NBG1  solid blue, priority 3 (the image below)
 *
 * A colour calculation window (W0, the left half) enables NBG0's ratio blend
 * only on the left. The right half keeps the sharp top image in every mode,
 * so blur on/off must leave it byte-identical - the control for the effect.
 *
 * With blur off the left half mixes NBG0 with the blue below; with blur on
 * the second image becomes the blurred NBG0 and the same ratio mixes NBG0
 * with its own 2:1:1 horizontal average. The checker is deliberate: it makes
 * that difference large and lets the test recompute every asserted pixel.
 *
 * Timeline (auto; A cycles by hand):
 *   PLAIN  no colour calculation, no blur
 *   frame  60  CC     NBG0 blends over NBG1 at CCRTA 16 (15:17)
 *   frame 120  BLUR   the same blend, but the second image is blurred NBG0
 *
 * g_blur holds the chapter-12 refusals and the live mutual-exclusion checks.
 * Hi-res is fixed at sat_init, so the examples cannot toggle it mid-run; the
 * pure policy test (tests/host/test_vdp2_compose_logic.cpp) covers it. */
#include <stdint.h>
#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/saturn.h"
#include "saturn/vdp2_compose.h"
#include "saturn/vdp2_layers.h"

#define BLUR_MAGIC SAT_FOURCC('B', 'L', 'R', '1')

typedef struct blur_results {
    uint32_t magic;
    uint32_t cram_mode_ok;              /* CRAM mode 0 before the palettes */
    uint32_t layers_ok;
    uint32_t cc_window_ok;              /* W0 left half confines colour calculation */
    uint32_t refused_bokn3;             /* BOKN 3 is setting-prohibited (p12_14) */
    uint32_t refused_bokn7;             /* BOKN 7 is setting-prohibited (p12_14) */
    uint32_t refused_cram1;             /* BOKEN needs CRAM mode 0 (p12_13) */
    uint32_t refused_line_color;        /* a live insert refuses BOKEN (p12_13) */
    uint32_t blur_accepts_after_line_color; /* only a LIVE insert excludes blur */
    uint32_t blur_accepts_after_extended; /* the two extras clear each other */
    uint32_t extended_accepts_after_blur;
    uint32_t mode_ok;                   /* bit n set when mode n applied cleanly */
    uint32_t mode;                      /* last applied mode */
    uint32_t ratio;                     /* last applied top-image ratio */
    uint32_t frames;
} blur_results_t;

volatile blur_results_t g_blur;

static sat_ascii_font_t font;
static uint16_t g_map[64u * 64u];
static uint16_t g_tiles[2u * 16u];

#define NAMES0 0x00000u
#define NAMES1 0x02000u
#define CHARS0 0x40000u
#define CHARS1 0x48000u

#define MODE_PLAIN 0u
#define MODE_CC    1u
#define MODE_BLUR  2u
#define MODE_SOLO  3u   /* ratio 31: show the blurred second image alone */
#define MODE_COUNT 4u
#define BLUR_RATIO 16u
#define SOLO_RATIO 31u
static const char*const kModeName[MODE_COUNT] = {"PLAIN", "CC", "BLUR", "BLUR 32"};
/* Auto-advance frames: mode n+1 starts at kAutoAt[n]. */
static const uint32_t kAutoAt[MODE_COUNT - 1u] = {60u, 120u, 180u};

/* Tile 0: one-pixel vertical stripes (palette 1/2). Tile 1: solid (palette 1).
 * Both are opaque: the blur only applies to dots that actually show. */
static void build_tiles(void) {
    for (uint32_t k = 0u; k < 2u; ++k) {
        for (uint32_t row = 0u; row < 8u; ++row) {
            uint32_t p[8];
            for (uint32_t x = 0u; x < 8u; ++x) {
                p[x] = (k == 0u && (x & 1u) != 0u) ? 2u : 1u;
            }
            g_tiles[k * 16u + row * 2u] =
                (uint16_t)((p[0] << 12u) | (p[1] << 8u) | (p[2] << 4u) | p[3]);
            g_tiles[k * 16u + row * 2u + 1u] =
                (uint16_t)((p[4] << 12u) | (p[5] << 8u) | (p[6] << 4u) | p[7]);
        }
    }
}

static void fill_map(uint32_t layer, uint32_t palette, uint32_t tile) {
    for (uint32_t y = 0u; y < 64u; ++y) {
        for (uint32_t x = 0u; x < 64u; ++x) {
            g_map[y * 64u + x] = (uint16_t)((palette << 12u) | tile);
        }
    }
}

static sat_result_t setup_layer(uint32_t layer, uint32_t names, uint32_t chars, uint8_t bank,
                                uint8_t priority, uint32_t tile,
                                const uint16_t* colours, uint32_t colour_count) {
    sat_vdp2_layer_config_t c;
    uint16_t pal[16];
    const uint32_t palette = layer + 1u;
    for (uint32_t i = 0u; i < 16u; ++i) pal[i] = 0u;
    for (uint32_t i = 0u; i < colour_count; ++i) pal[1u + i] = colours[i];
    SAT_TRY(sat_vdp2_palette_upload(pal, 16u, palette * 16u));
    fill_map(layer, palette, tile);
    SAT_TRY(sat_vdp2_vram_write_words(names / 2u, g_map, 64u * 64u));
    SAT_TRY(sat_vdp2_vram_write_words(chars / 2u, g_tiles, 2u * 16u));
    sat_vdp2_layer_config_default((sat_vdp2_layer_t)layer, &c);
    for (uint32_t p = 0u; p < 4u; ++p) c.plane_address[p] = names;
    c.char_base_address = chars;
    c.char_bank_mask = bank;
    c.priority = priority;
    return sat_vdp2_layer_configure(&c);
}

/* Clears everything the scene owns, then applies one mode. Clearing the
 * extended extra matters here: the init probe leaves it on. */
static sat_result_t apply_mode(uint8_t mode) {
    SAT_TRY(sat_vdp2_extended_color_calc_set(0u));
    SAT_TRY(sat_vdp2_hardware_blur_set(2u, 0u));
    SAT_TRY(sat_vdp2_screen_color_calc_set(SAT_VDP2_SCREEN_NBG0, 0u, 0u));
    if (mode == MODE_CC || mode == MODE_BLUR) {
        SAT_TRY(sat_vdp2_screen_color_calc_set(SAT_VDP2_SCREEN_NBG0, 1u, BLUR_RATIO));
    } else if (mode == MODE_SOLO) {
        SAT_TRY(sat_vdp2_screen_color_calc_set(SAT_VDP2_SCREEN_NBG0, 1u, SOLO_RATIO));
    }
    if (mode == MODE_BLUR || mode == MODE_SOLO) {
        SAT_TRY(sat_vdp2_hardware_blur_set(2u, 1u));
    }
    return SAT_OK;
}

int main(void) {
    static const uint16_t checker[2] = {SAT_RGB555(31, 31, 31), SAT_RGB555(1, 1, 1)};
    static const uint16_t blue[1] = {SAT_RGB555(2, 2, 31)};
    if (sat_app_init_default() != SAT_OK) return 1;
    if (sat_ascii_font_init_8x8_indexed8(&font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u) != SAT_OK) return 1;
    build_tiles();
    g_blur.magic = BLUR_MAGIC;

    /* BOKEN needs CRAM mode 0; choose it before the palettes. */
    g_blur.cram_mode_ok =
        sat_vdp2_cram_mode_set(0u) == SAT_OK && sat_vdp2_cram_mode() == 0u;

    g_blur.layers_ok =
        setup_layer(0u, NAMES0, CHARS0, SAT_VDP2_BANK_B0, 5u, 0u, checker, 2u) == SAT_OK &&
        setup_layer(1u, NAMES1, CHARS1, SAT_VDP2_BANK_B0, 3u, 1u, blue, 1u) == SAT_OK;

    {
        sat_vdp2_window_rect_t rect = {0u, 0u, 159u, 223u};
        sat_vdp2_screen_window_t cc = {SAT_VDP2_SCREEN_COLOR_CALC, SAT_VDP2_WINDOW_AREA_INSIDE,
                                       SAT_VDP2_WINDOW_AREA_OFF, 0u};
        g_blur.cc_window_ok =
            sat_vdp2_window_set_rect(0u, &rect) == SAT_OK &&
            sat_vdp2_screen_window_set(&cc) == SAT_OK;
    }

    /* Chapter-12 refusals, before any mode runs. */
    g_blur.refused_bokn3 = sat_vdp2_hardware_blur_set(3u, 1u) == SAT_ERR_UNSUPPORTED;
    g_blur.refused_bokn7 = sat_vdp2_hardware_blur_set(7u, 1u) == SAT_ERR_UNSUPPORTED;
    g_blur.refused_cram1 = 0u;
    if (sat_vdp2_cram_mode_set(1u) == SAT_OK) {
        g_blur.refused_cram1 = sat_vdp2_hardware_blur_set(2u, 1u) == SAT_ERR_UNSUPPORTED;
        (void)sat_vdp2_cram_mode_set(0u);
    }
    g_blur.refused_line_color = 0u;
    g_blur.blur_accepts_after_line_color = 0u;
    if (sat_vdp2_layer_set_line_color_insert(SAT_VDP2_NBG1, 1u) == SAT_OK) {
        g_blur.refused_line_color = sat_vdp2_hardware_blur_set(2u, 1u) == SAT_ERR_UNSUPPORTED;
        (void)sat_vdp2_layer_set_line_color_insert(SAT_VDP2_NBG1, 0u);
        /* Clearing the insert must make blur legal again: the manual
         * excludes a live insertion, not a historical one. */
        g_blur.blur_accepts_after_line_color =
            sat_vdp2_hardware_blur_set(2u, 1u) == SAT_OK;
        (void)sat_vdp2_hardware_blur_set(2u, 0u);
    }

    /* p12_14: BOKEN and EXCCEN are mutually exclusive; turning one on clears
     * the other, and both directions must succeed from a live configuration. */
    g_blur.blur_accepts_after_extended = 0u;
    g_blur.extended_accepts_after_blur = 0u;
    if (sat_vdp2_extended_color_calc_set(1u) == SAT_OK) {
        g_blur.blur_accepts_after_extended =
            sat_vdp2_hardware_blur_set(2u, 1u) == SAT_OK;
        g_blur.extended_accepts_after_blur =
            sat_vdp2_extended_color_calc_set(1u) == SAT_OK;
        (void)sat_vdp2_extended_color_calc_set(0u);
    }

    uint8_t mode = MODE_PLAIN;
    if (apply_mode(mode) == SAT_OK) g_blur.mode_ok = 1u;
    g_blur.mode = mode;
    g_blur.ratio = BLUR_RATIO;

    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_wait_vblank() != SAT_OK) break;
        (void)sat_vdp2_layers_commit();
        (void)sat_vdp1_set_erase_transparent();
        if (sat_begin_frame() != SAT_OK) break;
        (void)sat_pad_poll(&pad);
        ++g_blur.frames;
        uint8_t next = mode;
        if ((pad.pressed & SAT_PAD_A) != 0u) {
            next = (uint8_t)((mode + 1u) % MODE_COUNT);
        } else if (mode + 1u < MODE_COUNT && g_blur.frames == kAutoAt[mode]) {
            next = (uint8_t)(mode + 1u);
        }
        if (next != mode) {
            mode = next;
            if (apply_mode(mode) == SAT_OK) {
                g_blur.mode_ok = g_blur.mode_ok | (1u << mode);
            }
            g_blur.mode = mode;
            g_blur.ratio = mode == MODE_SOLO ? SOLO_RATIO : BLUR_RATIO;
        }
        (void)sat_ascii_font_draw_text_screen_indexed8(&font, "VDP2 HARDWARE BLUR", 8, 8, 8, 0u, 0u);
        (void)sat_ascii_font_draw_text_screen_indexed8(&font, kModeName[mode], 8, 24, 8, 0u, 0u);
        (void)sat_ascii_font_draw_label_u32(&font, "RATIO ", g_blur.ratio, 80, 24, 8, 0u, 0u);
        (void)sat_ascii_font_draw_label_u32(&font, "FRAME ", g_blur.frames, 8, 40, 8, 0u, 0u);
        (void)sat_end_frame();
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }
    (void)sat_shutdown();
    return 0;
}
