/* vdp2_rbg0_debug.c - Button-driven RBG0 smoke tests
 *
 * Stages:
 *   0. Backdrop only
 *   1. NBG0 solid fill
 *   2. RBG0 solid fill
 *   3. RBG0 checkerboard
 *   4. RBG0 asset upload
 *   5. RBG0 asset upload with auto-scroll
 *
 * Controls:
 *   A      Next stage
 *   B      Previous stage
 *   Start  Exit
 */
#include <stdint.h>
#include "saturn/saturn.h"
#include "saturn/example_util.h"
#include "vdp2_rbg0_debug/seamless_sea.h"

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 224

#define BITMAP_WIDTH  512
#define BITMAP_HEIGHT 256

#define BITMAP_BASE_WORD  0x00000u
#define ROT_PARAM_BASE    0x10000u
#define NBG0_MAP_PLANE_INDEX 0x0010u
#define NBG0_TILE_WORDS 16u
#define AUTO_STAGE_FRAMES 180u

#define FX16_SHIFT 16

typedef enum debug_stage {
    STAGE_BACKDROP = 0,
    STAGE_NBG0 = 1,
    STAGE_SOLID = 2,
    STAGE_CHECKER = 3,
    STAGE_ASSET = 4,
    STAGE_SCROLL = 5,
    STAGE_COUNT
} debug_stage_t;

static sat_fx16_t scroll_x = 0;
static sat_fx16_t scroll_y = 0;
static debug_stage_t g_stage = STAGE_BACKDROP;
static uint16_t g_backdrop_color = SAT_COLOR_BLUE;
static sat_ascii_font_t g_font;
static uint32_t g_frame_counter = 0;

static const char* stage_name(debug_stage_t stage) {
    switch (stage) {
    case STAGE_BACKDROP: return "STAGE 0 BACKDROP";
    case STAGE_NBG0: return "STAGE 1 NBG0";
    case STAGE_SOLID: return "STAGE 2 RBG0 SOLID";
    case STAGE_CHECKER: return "STAGE 3 CHECKER";
    case STAGE_ASSET: return "STAGE 4 ASSET";
    case STAGE_SCROLL: return "STAGE 5 SCROLL";
    default: return "STAGE ?";
    }
}

static void frame_line(char* out, uint32_t frame_counter) {
    static const char prefix[] = "FRAME 000000 A NEXT B PREV";
    for (uint32_t i = 0; i < sizeof(prefix); ++i) {
        out[i] = prefix[i];
    }

    for (int i = 10; i >= 5; --i) {
        out[i] = (char)('0' + (frame_counter % 10u));
        frame_counter /= 10u;
    }
}

static void pad_line(char* out, const sat_pad_state_t* pad) {
    static const char prefix[] = "U0 D0 L0 R0 A0 B0 S0";
    for (uint32_t i = 0; i < sizeof(prefix); ++i) {
        out[i] = prefix[i];
    }

    out[1] = ((pad->held & SAT_PAD_UP) != 0u) ? '1' : '0';
    out[4] = ((pad->held & SAT_PAD_DOWN) != 0u) ? '1' : '0';
    out[7] = ((pad->held & SAT_PAD_LEFT) != 0u) ? '1' : '0';
    out[10] = ((pad->held & SAT_PAD_RIGHT) != 0u) ? '1' : '0';
    out[13] = ((pad->held & SAT_PAD_A) != 0u) ? '1' : '0';
    out[16] = ((pad->held & SAT_PAD_B) != 0u) ? '1' : '0';
    out[19] = ((pad->held & SAT_PAD_START) != 0u) ? '1' : '0';
}

static debug_stage_t next_stage(debug_stage_t stage) {
    return (debug_stage_t)(((uint32_t)stage + 1u) % STAGE_COUNT);
}

static debug_stage_t prev_stage(debug_stage_t stage) {
    return (stage == STAGE_BACKDROP)
        ? (debug_stage_t)(STAGE_COUNT - 1u)
        : (debug_stage_t)((uint32_t)stage - 1u);
}

static sat_result_t upload_bitmap_from_pixels(const uint8_t* pixels, uint32_t src_width, uint32_t src_height) {
    volatile uint16_t* const vram = (volatile uint16_t*)(0x20000000u | 0x05E00000u);
    uint32_t word_offset = BITMAP_BASE_WORD;

    for (uint32_t y = 0; y < BITMAP_HEIGHT; ++y) {
        const uint32_t src_y = y % src_height;
        for (uint32_t x = 0; x < BITMAP_WIDTH; x += 2u) {
            const uint32_t src_x0 = x % src_width;
            const uint32_t src_x1 = (x + 1u) % src_width;
            const uint16_t word = (uint16_t)((pixels[src_y * src_width + src_x0] << 8u) |
                                             pixels[src_y * src_width + src_x1]);
            vram[word_offset++] = word;
        }
    }

    return SAT_OK;
}

static sat_result_t upload_solid_bitmap(uint8_t palette_index) {
    volatile uint16_t* const vram = (volatile uint16_t*)(0x20000000u | 0x05E00000u);
    const uint16_t word = (uint16_t)(((uint16_t)palette_index << 8u) | palette_index);

    for (uint32_t i = 0; i < ((BITMAP_WIDTH * BITMAP_HEIGHT) / 2u); ++i) {
        vram[BITMAP_BASE_WORD + i] = word;
    }

    return SAT_OK;
}

static sat_result_t upload_checker_bitmap(void) {
    volatile uint16_t* const vram = (volatile uint16_t*)(0x20000000u | 0x05E00000u);
    uint32_t word_offset = BITMAP_BASE_WORD;

    for (uint32_t y = 0; y < BITMAP_HEIGHT; ++y) {
        for (uint32_t x = 0; x < BITMAP_WIDTH; x += 2u) {
            const uint8_t p0 = (((x >> 5u) ^ (y >> 5u)) & 1u) ? 2u : 1u;
            const uint8_t p1 = ((((x + 1u) >> 5u) ^ (y >> 5u)) & 1u) ? 2u : 1u;
            vram[word_offset++] = (uint16_t)(((uint16_t)p0 << 8u) | p1);
        }
    }

    return SAT_OK;
}

static sat_result_t setup_nbg0_solid(void) {
    static const uint16_t tile_words[NBG0_TILE_WORDS] = {
        0x1111u, 0x1111u, 0x1111u, 0x1111u,
        0x1111u, 0x1111u, 0x1111u, 0x1111u,
        0x1111u, 0x1111u, 0x1111u, 0x1111u,
        0x1111u, 0x1111u, 0x1111u, 0x1111u
    };
    static const uint16_t palette[16] = {
        SAT_COLOR_BLACK, SAT_COLOR_GREEN
    };
    const sat_vdp2_nbg0_config_t nbg0_cfg = {
        SAT_VDP2_CHAR_SIZE_1X1,
        SAT_VDP2_COLOR_MODE_16,
        NBG0_MAP_PLANE_INDEX,
        0u,
        0u
    };
    const sat_vdp2_scroll_t scroll = {0u, 0u, 0u, 0u};

    SAT_TRY(sat_vdp2_palette_upload(palette, 16, 0));
    SAT_TRY(sat_vdp2_vram_write_words(BITMAP_BASE_WORD, tile_words, NBG0_TILE_WORDS));
    SAT_TRY(sat_vdp2_nbg0_init(&nbg0_cfg));
    SAT_TRY(sat_vdp2_nbg0_set_scroll(&scroll));
    SAT_TRY(sat_vdp2_nbg0_map_fill(0u));
    SAT_TRY(sat_vdp2_nbg0_set_enabled(1));

    return SAT_OK;
}

static sat_result_t setup_rotation_params(void) {
    volatile uint16_t* const vram = (volatile uint16_t*)(0x20000000u | 0x05E00000u);

    for (uint32_t i = 0; i < 48u; ++i) {
        vram[ROT_PARAM_BASE + i] = 0x0000u;
    }

    SAT_TRY(sat_vdp2_rbg0_set_rotation_matrix(ROT_PARAM_BASE, 0, 0, 0));
    SAT_TRY(sat_vdp2_rbg0_set_scaling(ROT_PARAM_BASE, SAT_FX16_ONE, SAT_FX16_ONE));
    SAT_TRY(sat_vdp2_rbg0_set_coordinate_increments(ROT_PARAM_BASE, 1, 0, 1, 0));
    SAT_TRY(sat_vdp2_rbg0_set_scroll(ROT_PARAM_BASE, 0, 0, 0, 0));

    return SAT_OK;
}

static sat_result_t configure_rbg0(void) {
    const sat_vdp2_rbg0_config_t rbg0_cfg = {
        SAT_VDP2_RBG0_BITMAP_512x256,
        SAT_VDP2_COLOR_MODE_256,
        BITMAP_BASE_WORD,
        ROT_PARAM_BASE
    };

    SAT_TRY(sat_vdp2_rbg0_init(&rbg0_cfg));
    SAT_TRY(sat_vdp2_rbg0_set_param_mode(SAT_VDP2_RBG0_PARAM_A));
    SAT_TRY(setup_rotation_params());

    return SAT_OK;
}

static sat_result_t apply_stage(debug_stage_t stage) {
    static const uint16_t solid_palette[256] = {
        0x0000u, 0x7C00u, 0x03E0u, 0x7FFFu
    };

    scroll_x = 0;
    scroll_y = 0;
    SAT_TRY(sat_vdp2_nbg0_set_enabled(0));
    SAT_TRY(sat_vdp2_rbg0_set_enabled(0));

    switch (stage) {
    case STAGE_BACKDROP:
        g_backdrop_color = SAT_COLOR_BLUE;
        SAT_TRY(sat_vdp2_back_color_set(g_backdrop_color));
        break;
    case STAGE_NBG0:
        SAT_TRY(setup_nbg0_solid());
        g_backdrop_color = SAT_COLOR_BLACK;
        SAT_TRY(sat_vdp2_back_color_set(g_backdrop_color));
        break;
    case STAGE_SOLID:
        SAT_TRY(sat_vdp2_palette_upload(solid_palette, 4, 0));
        SAT_TRY(upload_solid_bitmap(1u));
        SAT_TRY(configure_rbg0());
        SAT_TRY(sat_vdp2_rbg0_set_enabled(1));
        g_backdrop_color = SAT_COLOR_BLACK;
        SAT_TRY(sat_vdp2_back_color_set(g_backdrop_color));
        break;
    case STAGE_CHECKER:
        SAT_TRY(sat_vdp2_palette_upload(solid_palette, 4, 0));
        SAT_TRY(upload_checker_bitmap());
        SAT_TRY(configure_rbg0());
        SAT_TRY(sat_vdp2_rbg0_set_enabled(1));
        g_backdrop_color = SAT_COLOR_BLACK;
        SAT_TRY(sat_vdp2_back_color_set(g_backdrop_color));
        break;
    case STAGE_ASSET:
    case STAGE_SCROLL:
        SAT_TRY(sat_vdp2_palette_upload(seamless_sea_asset.palette, 256, 0));
        SAT_TRY(upload_bitmap_from_pixels(
            seamless_sea_asset.pixels,
            seamless_sea_asset.width,
            seamless_sea_asset.height));
        SAT_TRY(configure_rbg0());
        SAT_TRY(sat_vdp2_rbg0_set_enabled(1));
        g_backdrop_color = SAT_COLOR_BLACK;
        SAT_TRY(sat_vdp2_back_color_set(g_backdrop_color));
        break;
    default:
        return SAT_ERR_INVALID_ARG;
    }

    g_stage = stage;
    return SAT_OK;
}

static void update_scroll(void) {
    if (g_stage != STAGE_SCROLL) {
        return;
    }

    scroll_x += (sat_fx16_t)(SAT_FX16_ONE * 2);
    scroll_y += (sat_fx16_t)(SAT_FX16_ONE / 2);

    sat_example_must(sat_vdp2_rbg0_set_scroll(
        ROT_PARAM_BASE,
        (int32_t)(scroll_x >> FX16_SHIFT),
        (int32_t)(scroll_x & 0xFFFF),
        (int32_t)(scroll_y >> FX16_SHIFT),
        (int32_t)(scroll_y & 0xFFFF)));
}

int main(void) {
    sat_video_config_t cfg = {SCREEN_WIDTH, SCREEN_HEIGHT, 1, 0};
    sat_example_must(sat_init(&cfg));
    sat_example_must(sat_ascii_font_init_8x8_indexed8(&g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 1));
    sat_example_must(apply_stage(STAGE_BACKDROP));

    while (1) {
        sat_pad_state_t pad = {0};
        char line1[] = "FRAME 000000 A NEXT B PREV";
        char line2[] = "U0 D0 L0 R0 A0 B0 S0";
        sat_example_must(sat_app_frame_begin(g_backdrop_color, SAT_COLOR_BLACK, &pad));

        if ((pad.pressed & SAT_PAD_START) != 0u) {
            sat_example_must(sat_end_frame());
            break;
        }

        if ((pad.pressed & (SAT_PAD_A | SAT_PAD_RIGHT | SAT_PAD_DOWN)) != 0u) {
            sat_example_must(apply_stage(next_stage(g_stage)));
        }

        if ((pad.pressed & (SAT_PAD_B | SAT_PAD_LEFT | SAT_PAD_UP)) != 0u) {
            sat_example_must(apply_stage(prev_stage(g_stage)));
        }

        if ((g_frame_counter != 0u) && ((g_frame_counter % AUTO_STAGE_FRAMES) == 0u)) {
            sat_example_must(apply_stage(next_stage(g_stage)));
        }

        update_scroll();
        frame_line(line1, g_frame_counter);
        pad_line(line2, &pad);
        sat_example_must(sat_ascii_font_draw_text_indexed8(&g_font, stage_name(g_stage), -152, -96, 8, 0, 0));
        sat_example_must(sat_ascii_font_draw_text_indexed8(&g_font, line1, -152, -84, 8, 0, 0));
        sat_example_must(sat_ascii_font_draw_text_indexed8(&g_font, line2, -152, -72, 8, 0, 0));
        sat_example_must(sat_end_frame());
        ++g_frame_counter;
    }

    return 0;
}
