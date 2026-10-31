/* VDP2 special priority and special colour calculation (manual 11.2/12.2).
 *
 * The scene is a 2-word pattern-name NBG0 band over a solid NBG1, with a
 * VDP1 magenta bar at priority 4. The band's cells alternate:
 *
 *   even cell   PR=0, CC=0   (normal)
 *   odd cell    PR=1, CC=1   (special, only below the HUD rows)
 *
 * NBG0's base priority is 4, so the bar (priority 4, sprites win ties) covers
 * normal cells. A special cell takes priority 5 and rises above the bar. The
 * tile alternates dot codes 1 (white, CRAM MSB clear) and 6 (orange, CRAM MSB
 * set) to exercise the dot modes and the colour-data-MSB mode.
 *
 * Timeline (auto; A cycles by hand):
 *   SCREEN  SFPRMD/SFCCMD 0: priority and CC for the whole screen
 *   CHAR    mode 1: the name's PR/CC bits decide per character
 *   DOT     mode 2: PR/CC=1 and SFCODE A bit 3 (dot codes 6/7) decide per dot
 *   MSB     SFPRMD 0, SFCCMD 3: CRAM colour bit 15 decides CC per dot
 *
 * g_special_priority holds the refusals and the mode telemetry. */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/saturn.h"
#include "saturn/vdp1.h"
#include "saturn/vdp2_compose.h"
#include "saturn/vdp2_layers.h"
#include "saturn/vdp2_special.h"

#define SPECIAL_MAGIC SAT_FOURCC('S', 'P', 'R', '1')

typedef struct special_results {
    uint32_t magic;
    uint32_t layers_ok;
    uint32_t cc_ok;
    uint32_t refused_priority_mode3;
    uint32_t refused_color_mode4;
    uint32_t refused_select2;
    uint32_t refused_layer;
    uint32_t refused_one_word_priority;
    uint32_t refused_one_word_color;
    uint32_t mode_ok;
    uint32_t mode;
    uint32_t ratio;
    uint32_t frames;
} special_results_t;

volatile special_results_t g_special_priority;

static sat_ascii_font_t font;
static uint16_t g_map0[64u * 64u * 2u];   /* 2-word names: 16 KiB */
static uint16_t g_map1[64u * 64u];        /* 1-word backdrop: 8 KiB */
static uint16_t g_tiles[2u * 16u];

#define NAMES0 0x00000u
#define NAMES1 0x04000u
#define CHARS0 0x40000u
#define CHARS1 0x48000u
#define CHAR_NUM0 (CHARS0 / 0x20u)

#define MODE_SCREEN 0u
#define MODE_CHAR   1u
#define MODE_DOT    2u
#define MODE_MSB    3u
#define MODE_COUNT  4u
#define SPECIAL_RATIO 16u
static const char*const kModeName[MODE_COUNT] = {"SCREEN", "CHAR", "DOT", "MSB"};
static const uint8_t kPriorityMode[MODE_COUNT] = {0u, 1u, 2u, 0u};
static const uint8_t kColorMode[MODE_COUNT] = {0u, 1u, 2u, 3u};
static const uint32_t kAutoAt[MODE_COUNT - 1u] = {60u, 120u, 180u};

/* Tile 0: dot codes 1,6 alternating every dot. Tile 1: solid code 1. */
static void build_tiles(void) {
    for (uint32_t row = 0u; row < 8u; ++row) {
        g_tiles[row * 2u] = 0x1616u;
        g_tiles[row * 2u + 1u] = 0x1616u;
        g_tiles[16u + row * 2u] = 0x1111u;
        g_tiles[16u + row * 2u + 1u] = 0x1111u;
    }
}

static sat_result_t setup_layers(void) {
    sat_vdp2_layer_config_t c;

    /* NBG0: 2-word names carry the palette and the PR/CC bits themselves.
     * The special cells are only below the HUD rows (cell row 7 onward) so
     * the HUD's priority-4 sprites stay visible above the normal cells. */
    for (uint32_t y = 0u; y < 64u; ++y) {
        for (uint32_t x = 0u; x < 64u; ++x) {
            const uint8_t special = ((x & 1u) != 0u && y >= 7u) ? 1u : 0u;
            const uint32_t name = sat_vdp2_pattern_name_2word(
                (uint16_t)CHAR_NUM0, 1u, 0u, 0u, special, special);
            const uint32_t i = (y * 64u + x) * 2u;
            g_map0[i] = (uint16_t)(name >> 16u);
            g_map0[i + 1u] = (uint16_t)name;
        }
    }
    for (uint32_t y = 0u; y < 64u; ++y) {
        for (uint32_t x = 0u; x < 64u; ++x) {
            g_map1[y * 64u + x] = (uint16_t)((2u << 12u) | 0u);
        }
    }

    SAT_TRY(sat_vdp2_vram_write_words(NAMES0 / 2u, g_map0, 64u * 64u * 2u));
    SAT_TRY(sat_vdp2_vram_write_words(NAMES1 / 2u, g_map1, 64u * 64u));
    SAT_TRY(sat_vdp2_vram_write_words(CHARS0 / 2u, g_tiles, 16u));
    SAT_TRY(sat_vdp2_vram_write_words(CHARS1 / 2u, g_tiles + 16u, 16u));

    sat_vdp2_layer_config_default(SAT_VDP2_NBG0, &c);
    c.color_mode = SAT_VDP2_COLOR_MODE_16;
    c.char_size = SAT_VDP2_CHAR_SIZE_1X1;
    c.pattern_name_words = 2u;
    c.plane_pages_x = 1u;
    c.plane_pages_y = 1u;
    c.priority = 4u;
    c.transparent = 1u;
    c.char_bank_mask = SAT_VDP2_BANK_B0;
    c.char_base_address = 0u;   /* the whole character number is in the name */
    for (uint32_t p = 0u; p < 4u; ++p) c.plane_address[p] = NAMES0;
    SAT_TRY(sat_vdp2_layer_configure(&c));

    sat_vdp2_layer_config_default(SAT_VDP2_NBG1, &c);
    c.color_mode = SAT_VDP2_COLOR_MODE_16;
    c.char_size = SAT_VDP2_CHAR_SIZE_1X1;
    c.pattern_name_words = 1u;
    c.plane_pages_x = 1u;
    c.plane_pages_y = 1u;
    c.priority = 2u;
    c.transparent = 1u;
    c.char_bank_mask = SAT_VDP2_BANK_B0;
    c.char_base_address = CHARS1;
    for (uint32_t p = 0u; p < 4u; ++p) c.plane_address[p] = NAMES1;
    return sat_vdp2_layer_configure(&c);
}

static sat_result_t apply_mode(uint8_t mode) {
    SAT_TRY(sat_vdp2_special_priority_mode_set(SAT_VDP2_NBG0, kPriorityMode[mode]));
    SAT_TRY(sat_vdp2_special_color_calc_mode_set(SAT_VDP2_NBG0, kColorMode[mode]));
    SAT_TRY(sat_vdp2_screen_color_calc_set(SAT_VDP2_SCREEN_NBG0, 1u, SPECIAL_RATIO));
    return SAT_OK;
}

int main(void) {
    uint16_t palette0[16];
    uint16_t palette1[16];
    if (sat_app_init_default() != SAT_OK) return 1;
    if (sat_ascii_font_init_8x8_indexed8(&font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u) != SAT_OK) return 1;

    build_tiles();
    g_special_priority.magic = SPECIAL_MAGIC;

    for (uint32_t i = 0u; i < 16u; ++i) {
        palette0[i] = 0u;
        palette1[i] = 0u;
    }
    /* Dot code 1: white with the CRAM MSB clear (mode 3 must not colour it).
     * Dot code 6: orange with the MSB set (mode 3 colours it). */
    palette0[1] = SAT_RGB555_RAW(31, 31, 31);
    palette0[6] = SAT_RGB555(31, 16, 0);
    palette1[1] = SAT_RGB555(0, 0, 16);
    if (sat_vdp2_palette_upload(palette0, 16u, 16u) != SAT_OK) return 1;
    if (sat_vdp2_palette_upload(palette1, 16u, 32u) != SAT_OK) return 1;

    g_special_priority.layers_ok = setup_layers() == SAT_OK;

    /* SFCODE A bit 3 covers dot colour codes 6 and 7 (manual 10.3). */
    g_special_priority.cc_ok =
        sat_vdp2_special_function_code_set(0x08u, 0x00u) == SAT_OK &&
        sat_vdp2_special_function_select(SAT_VDP2_NBG0, 0u) == SAT_OK &&
        sat_vdp2_screen_color_calc_set(SAT_VDP2_SCREEN_NBG0, 1u, SPECIAL_RATIO) == SAT_OK &&
        sat_vdp2_sprite_set_priority(4u) == SAT_OK;

    g_special_priority.refused_priority_mode3 =
        sat_vdp2_special_priority_mode_set(SAT_VDP2_NBG0, 3u) == SAT_ERR_INVALID_ARG;
    g_special_priority.refused_color_mode4 =
        sat_vdp2_special_color_calc_mode_set(SAT_VDP2_NBG0, 4u) == SAT_ERR_INVALID_ARG;
    g_special_priority.refused_select2 =
        sat_vdp2_special_function_select(SAT_VDP2_NBG0, 2u) == SAT_ERR_INVALID_ARG;
    g_special_priority.refused_layer =
        sat_vdp2_special_priority_mode_set((sat_vdp2_layer_t)4, 0u) == SAT_ERR_INVALID_ARG;
    /* NBG1 is a 1-word backdrop: the library does not expose the PNCN
     * auxiliary PR/CC bits, so per-character/dot modes are refused there. */
    g_special_priority.refused_one_word_priority =
        sat_vdp2_special_priority_mode_set(SAT_VDP2_NBG1, 1u) == SAT_ERR_UNSUPPORTED;
    g_special_priority.refused_one_word_color =
        sat_vdp2_special_color_calc_mode_set(SAT_VDP2_NBG1, 2u) == SAT_ERR_UNSUPPORTED;

    uint8_t mode = MODE_SCREEN;
    if (apply_mode(mode) == SAT_OK) g_special_priority.mode_ok = 1u;
    g_special_priority.mode = mode;
    g_special_priority.ratio = SPECIAL_RATIO;

    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_wait_vblank() != SAT_OK) break;
        (void)sat_vdp2_layers_commit();
        (void)sat_vdp1_set_erase_transparent();
        if (sat_begin_frame() != SAT_OK) break;
        (void)sat_pad_poll(&pad);
        ++g_special_priority.frames;

        uint8_t next = mode;
        if ((pad.pressed & SAT_PAD_A) != 0u) {
            next = (uint8_t)((mode + 1u) % MODE_COUNT);
        } else if (mode + 1u < MODE_COUNT && g_special_priority.frames == kAutoAt[mode]) {
            next = (uint8_t)(mode + 1u);
        }
        if (next != mode) {
            mode = next;
            if (apply_mode(mode) == SAT_OK) {
                g_special_priority.mode_ok = g_special_priority.mode_ok | (1u << mode);
            }
            g_special_priority.mode = mode;
        }

        /* A magenta bar at priority 4: normal NBG0 cells (priority 4) lose
         * to it (sprites win ties), special cells (priority 5) win. VDP1
         * local coordinates are relative to the screen centre (160,112). */
        const sat_polygon_cmd_t bar = {
            .x = {-160, 159, 159, -160},
            .y = {-32, -32, 8, 8},
            .color = SAT_RGB555(31, 0, 31),
            .flags = 0u
        };
        (void)sat_vdp1_draw_polygon(&bar);

        (void)sat_ascii_font_draw_text_screen_indexed8(&font, "VDP2 SPECIAL PRIORITY", 8, 8, 8, 0u, 0u);
        (void)sat_ascii_font_draw_text_screen_indexed8(&font, kModeName[mode], 8, 24, 8, 0u, 0u);
        (void)sat_ascii_font_draw_label_u32(&font, "RATIO ", SPECIAL_RATIO, 80, 24, 8, 0u, 0u);
        (void)sat_ascii_font_draw_label_u32(&font, "FRAME ", g_special_priority.frames, 8, 40, 8, 0u, 0u);
        (void)sat_end_frame();
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }
    (void)sat_shutdown();
    return 0;
}
