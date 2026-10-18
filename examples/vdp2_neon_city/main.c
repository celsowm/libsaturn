/* VDP2 neon city (E12).
 *
 * The last showcase composes every VDP2 facility the plan built:
 *
 *   back screen   a fixed night gradient (VRAM line table)
 *   NBG3          distant skyline with lit windows, priority 1
 *   NBG2          mid towers and neon signs, priority 2, colour offset A
 *                 makes the signs flicker
 *   RBG0          a rotating neon ring/sign through the rotation screen,
 *                 priority 3, parameter set A rotated every field
 *   NBG1          the wet street, priority 4, streamed as a 64x64-cell torus
 *                 through two pattern-name rings (sat_stage_map2); the active
 *                 ring is never written while displayed
 *   NBG0          foreground rain streaks and side signs, priority 6, colour
 *                 offset B and a slow line-scroll sway
 *
 * Window W0 is an ellipse around the moving light pool on the street. It is
 * the colour-calculation window: add-mode colour calculation with the line
 * colour screen inserts a cyan/magenta reflection only inside that pool, so
 * the street looks wet without a per-pixel software pass. The rotating sign
 * uses the rotation screen's own parameter table (16.16 rotation about the
 * map centre, map repeats transparently outside the sign).
 *
 * Streaming: two stage_map2 instances own rings at 0x40000 and 0x42000. Each
 * field the next camera view is staged into the inactive ring, committed
 * while the active one is displayed, and the pattern-name base is published
 * for the next VBlank commit. A model/VRAM CRC check runs periodically; the
 * sentinel 0xFFFF canary proves a missing cell would be visible.
 *
 * Controls: B pauses the street stream, R resets the camera, START exits.
 * The sign rotation and layer scrolls run automatically. g_neon_city holds
 * what harness/tests/test_vdp2_neon_city.py checks. */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/dma.h"
#include "saturn/font.h"
#include "saturn/math3d.h"
#include "saturn/saturn.h"
#include "saturn/stage_map2.h"
#include "saturn/time.h"
#include "saturn/vdp1.h"
#include "saturn/vdp2.h"
#include "saturn/vdp2_color_offset.h"
#include "saturn/vdp2_compose.h"
#include "saturn/vdp2_layers.h"

#define NEON_MAGIC SAT_FOURCC('N', 'E', 'O', '1')
#define SCREEN_W 320u
#define SCREEN_H 224u
#define CELLS 64u
#define MAP_WORDS (CELLS * CELLS)
#define HORIZON_ROW 16u
#define RING_A 0x40000u
#define RING_B 0x42000u
#define NAMES3 0x44000u
#define NAMES2 0x46000u
#define NAMES0 0x48000u
#define BACK_TABLE 0x4A000u
#define LINE_COLOR_TABLE 0x4A200u
#define SCROLL0_TABLE 0x4B000u
#define WINDOW_TABLE 0x4C000u
#define CHARS 0x50000u
#define RBG_NAMES 0x00000u
#define RBG_CHARS 0x20000u
#define RBG_TABLE 0x60000u
#define PAL_SKY 1u
#define PAL_BUILD 2u
#define PAL_SIGN 3u
#define PAL_NEON 4u
#define PAL_RAIN 5u
#define PAL_GROUND 6u
#define PAL_RBG 7u
#define FONT_BANK 7u       /* 256-colour bank 7, not the 16-colour palette 7 */
#define LINE_COLOR_BASE 128u
#define STAGE_WORDS 2970u
#define STAGE_RUNS 264u
#define SENTINEL_WORD 0xFFFFu
#define CAMERA_STEP 2u
#define CAMERA_WRAP 16384u
#define LONG_FRAMES 3600u

typedef struct neon_city_results {
    uint32_t magic;
    uint32_t init_ok;
    uint32_t layers_ok;
    uint32_t rotation_ok;
    uint32_t stream_ok;
    uint32_t window_ok;
    uint32_t color_offset_ok;
    uint32_t refused_base;
    uint32_t refused_window;
    uint32_t frames;
    uint32_t switches;
    uint32_t active_ring;
    uint32_t camera_x;
    uint32_t win_x0;
    uint32_t win_y0;
    uint32_t win_cols;
    uint32_t win_rows;
    uint32_t sign_angle;
    uint32_t bloom_x;
    uint32_t staged_words;
    uint32_t committed_words;
    uint32_t max_pending_words;
    uint32_t crc_expected;
    uint32_t crc_vram;
    uint32_t crc_mismatches;
    uint32_t sentinel_cells;
    uint32_t dma_scu;
    uint32_t dma_cpu;
    uint32_t update_us_max;
    uint32_t vdp1_commands;
} neon_city_results_t;

volatile neon_city_results_t g_neon_city;

static sat_ascii_font_t g_font;
static uint16_t g_maps[3][MAP_WORDS];   /* NBG3, NBG2, NBG0 */
static uint16_t g_ground_map[MAP_WORDS];
static uint16_t g_rbg_map[MAP_WORDS];
static uint16_t g_cells[MAP_WORDS];     /* ground tileset, one cell per metatile */
static uint16_t g_tiles[4u * 8u * 16u];
static uint16_t g_rbg_tiles[8u * 16u];
static uint16_t g_back[SCREEN_H];
static uint16_t g_line_indices[SCREEN_H];
static uint16_t g_line_palette[32];
static uint16_t g_sentinel[MAP_WORDS];
static uint16_t g_row[64];

static uint16_t g_stage_words[2][STAGE_WORDS];
static sat_stage_map2_run_t g_stage_runs[2][STAGE_RUNS];
static sat_stage_map2_t g_stream[2];
static sat_stage_map2_storage_t g_storage[2];
static sat_stage_map2_config_t g_config[2];
static sat_stage_map2_layer_desc_t g_desc[2];
static sat_stage_map2_tileset_t g_tileset;

static uint32_t g_seed = 0x9E3779B9u;
static uint32_t g_camera_x = 0u;
static uint32_t g_active = 0u;
static uint32_t g_pending = 0u;

static uint32_t rnd(void) {
    g_seed = g_seed * 1664525u + 1013904223u;
    return g_seed >> 8u;
}

static void set_tile(uint32_t group, uint32_t tile, const uint8_t rows[8][8]) {
    const uint32_t base = (group * 8u + tile) * 16u;
    for (uint32_t y = 0u; y < 8u; ++y) {
        for (uint32_t x = 0u; x < 8u; ++x) {
            const uint32_t word = base + y * 2u + x / 4u;
            const uint32_t shift = (3u - (x & 3u)) * 4u;
            if (x % 4u == 0u) g_tiles[word] = 0u;
            g_tiles[word] = (uint16_t)(g_tiles[word] | ((uint32_t)rows[y][x] << shift));
        }
    }
}

static void make_tiles(void) {
    uint8_t rows[8][8];
    for (uint32_t group = 0u; group < 4u; ++group) {
        for (uint32_t y = 0u; y < 8u; ++y)
            for (uint32_t x = 0u; x < 8u; ++x) rows[y][x] = 0u;
        set_tile(group, 0u, rows);
    }

    /* NBG3 skyline (group 0): 1 tower body, 2 dark body, 3 lit window,
     * 4 antenna, 5 roof light. */
    for (uint32_t y = 0u; y < 8u; ++y) {
        for (uint32_t x = 0u; x < 8u; ++x) {
            rows[y][x] = (uint8_t)(((x * 3u + y * 5u) & 7u) == 0u ? 3u : 1u);
        }
    }
    set_tile(0u, 1u, rows);
    for (uint32_t y = 0u; y < 8u; ++y)
        for (uint32_t x = 0u; x < 8u; ++x) rows[y][x] = (uint8_t)((x == 0u || x == 7u) ? 2u : 1u);
    set_tile(0u, 2u, rows);
    for (uint32_t y = 0u; y < 8u; ++y)
        for (uint32_t x = 0u; x < 8u; ++x)
            rows[y][x] = (uint8_t)((x % 3u == 1u && y % 2u == 0u) ? 3u : 1u);
    set_tile(0u, 3u, rows);
    for (uint32_t y = 0u; y < 8u; ++y)
        for (uint32_t x = 0u; x < 8u; ++x) rows[y][x] = (uint8_t)((x >= 3u && x <= 4u) ? 2u : 0u);
    set_tile(0u, 4u, rows);
    for (uint32_t y = 0u; y < 8u; ++y)
        for (uint32_t x = 0u; x < 8u; ++x) rows[y][x] = (uint8_t)((x < 4u && y < 4u) ? 5u : 0u);
    set_tile(0u, 5u, rows);

    /* NBG2 towers and signs (group 1): 1 body, 2 dark body, 3 window,
     * 4 magenta sign, 5 cyan sign, 6 gold sign. */
    for (uint32_t y = 0u; y < 8u; ++y) {
        for (uint32_t x = 0u; x < 8u; ++x) {
            rows[y][x] = (uint8_t)((x == 0u || x == 7u) ? 2u : ((y % 3u == 1u && x % 2u == 0u) ? 3u : 1u));
        }
    }
    set_tile(1u, 1u, rows);
    for (uint32_t y = 0u; y < 8u; ++y)
        for (uint32_t x = 0u; x < 8u; ++x) rows[y][x] = 2u;
    set_tile(1u, 2u, rows);
    for (uint32_t y = 0u; y < 8u; ++y)
        for (uint32_t x = 0u; x < 8u; ++x) rows[y][x] = (uint8_t)(((x + y) & 1u) ? 3u : 1u);
    set_tile(1u, 3u, rows);
    for (uint32_t tile = 4u; tile <= 6u; ++tile) {
        for (uint32_t y = 0u; y < 8u; ++y) {
            for (uint32_t x = 0u; x < 8u; ++x) {
                rows[y][x] = (uint8_t)((y == 0u || y == 7u) ? 0u : tile);
            }
        }
        set_tile(1u, tile, rows);
    }

    /* NBG0 rain/signs (group 2): 1 rain, 2 puddle glow, 3 side sign. */
    for (uint32_t y = 0u; y < 8u; ++y)
        for (uint32_t x = 0u; x < 8u; ++x) rows[y][x] = (uint8_t)(x == 2u || x == 6u ? 1u : 0u);
    set_tile(2u, 1u, rows);
    for (uint32_t y = 0u; y < 8u; ++y)
        for (uint32_t x = 0u; x < 8u; ++x) rows[y][x] = (uint8_t)(((x ^ y) & 1u) ? 2u : 0u);
    set_tile(2u, 2u, rows);
    for (uint32_t y = 0u; y < 8u; ++y)
        for (uint32_t x = 0u; x < 8u; ++x) rows[y][x] = (uint8_t)((x < 3u) ? 3u : 0u);
    set_tile(2u, 3u, rows);

    /* NBG1 ground (group 3): 1 road, 2 wet road, 3 lane, 4 curb,
     * 5 puddle, 6 sidewalk, 7 reflection. */
    for (uint32_t tile = 1u; tile <= 7u; ++tile) {
        for (uint32_t y = 0u; y < 8u; ++y) {
            for (uint32_t x = 0u; x < 8u; ++x) {
                uint8_t c = 1u;
                if (tile == 2u) c = (uint8_t)(((x + y) & 3u) == 0u ? 7u : 1u);
                else if (tile == 3u) c = (uint8_t)(x < 2u ? 3u : 1u);
                else if (tile == 4u) c = (uint8_t)(y < 2u ? 4u : 1u);
                else if (tile == 5u) c = (uint8_t)(((x + y) & 1u) ? 5u : 2u);
                else if (tile == 6u) c = (uint8_t)(((x / 2u + y / 2u) & 1u) ? 6u : 4u);
                else if (tile == 7u) c = (uint8_t)(x == y ? 5u : 2u);
                rows[y][x] = c;
            }
        }
        set_tile(3u, tile, rows);
    }
}

static void make_rbg_tiles(void) {
    /* Solid 8x8 neon tiles for the rotating sign. */
    for (uint32_t tile = 0u; tile < 8u; ++tile) {
        for (uint32_t y = 0u; y < 8u; ++y) {
            for (uint32_t x = 0u; x < 8u; ++x) {
                const uint32_t word = tile * 16u + y * 2u + x / 4u;
                const uint32_t shift = (3u - (x & 3u)) * 4u;
                if (x % 4u == 0u) g_rbg_tiles[word] = 0u;
                const uint32_t c = (tile == 0u) ? 0u : tile;
                g_rbg_tiles[word] = (uint16_t)(g_rbg_tiles[word] | (c << shift));
            }
        }
    }
}

static void put(uint16_t* map, uint32_t x, uint32_t y, uint32_t tile, uint32_t palette) {
    if (x >= CELLS || y >= CELLS) return;
    map[y * CELLS + x] = (uint16_t)((palette << 12u) | (tile & 0x3FFu));
}

static uint16_t ground_cell(uint32_t x, uint32_t y) {
    if (y < HORIZON_ROW) return 0u;
    if (y < HORIZON_ROW + 2u) return 6u;
    if ((x % 16u) < 2u) return 4u;
    if ((x % 16u) == 7u || (x % 16u) == 8u) return 3u;
    if (((x * 7u + y * 3u) % 13u) == 0u) return 5u;
    return ((x + y) & 1u) ? 2u : 1u;
}

static void build_maps(void) {
    for (uint32_t m = 0u; m < 3u; ++m)
        for (uint32_t i = 0u; i < MAP_WORDS; ++i) g_maps[m][i] = 0u;
    for (uint32_t i = 0u; i < MAP_WORDS; ++i) {
        g_cells[i] = (uint16_t)ground_cell(i & 63u, i >> 6u);
        g_ground_map[i] = (uint16_t)i;
    }

    /* NBG3 skyline: a deterministic row of buildings with lit windows. */
    g_seed = 0x51A3C9E7u;
    for (uint32_t x = 0u; x < CELLS; ++x) {
        const uint32_t height = 10u + (rnd() % 14u);
        const uint32_t top = 28u - height;
        for (uint32_t y = top; y < 28u; ++y) {
            const uint32_t tile = (x % 5u == 0u) ? 2u : ((y % 4u == 2u) ? 3u : 1u);
            put(g_maps[0], x, y, tile, PAL_SKY);
        }
        if (height > 12u) put(g_maps[0], x, top - 1u, 4u, PAL_SKY);
        put(g_maps[0], x, 28u, 5u, PAL_SKY);
    }

    /* NBG2 mid towers and neon signs. */
    g_seed = 0x2F6E1B4Du;
    for (uint32_t x = 2u; x < CELLS; x += 7u) {
        const uint32_t height = 9u + (rnd() % 10u);
        const uint32_t top = 28u - height;
        for (uint32_t y = top; y < 28u; ++y) {
            const uint32_t tile = (y - top < 2u) ? 3u : ((x % 3u == 0u) ? 4u + (x % 3u) : 1u);
            put(g_maps[1], x, y, tile, PAL_BUILD);
            put(g_maps[1], x + 1u, y, tile, PAL_BUILD);
        }
        put(g_maps[1], x, top, 4u + (x % 3u), PAL_BUILD);
    }

    /* NBG0 rain streaks and side signs. */
    for (uint32_t i = 0u; i < 40u; ++i) {
        put(g_maps[2], rnd() % CELLS, rnd() % 17u, 1u, PAL_RAIN);
    }
    for (uint32_t y = 4u; y < 12u; ++y) {
        put(g_maps[2], 1u, y, 3u, PAL_RAIN);
        put(g_maps[2], 62u, y, 3u, PAL_RAIN);
    }
}

static void upload_palettes(void) {
    uint16_t palette[16];
    const uint16_t colors[7][8] = {
        /* sky: star, dark blue, window, roof light */
        {0u, SAT_RGB555(18, 22, 38), SAT_RGB555(5, 8, 20), SAT_RGB555(28, 29, 18),
         SAT_RGB555(31, 31, 31), 0u},
        /* buildings: body, dark, window, magenta, cyan, gold */
        {0u, SAT_RGB555(19, 17, 25), SAT_RGB555(9, 9, 15), SAT_RGB555(26, 24, 18),
         SAT_RGB555(31, 5, 24), SAT_RGB555(3, 24, 30)},
        /* signs: magenta, cyan, gold */
        {0u, SAT_RGB555(31, 4, 24), SAT_RGB555(3, 28, 31), SAT_RGB555(31, 25, 3),
         SAT_RGB555(31, 31, 31), 0u},
        /* neon row for RBG0 and line colours: magenta, cyan, white */
        {0u, SAT_RGB555(31, 4, 24), SAT_RGB555(3, 28, 31), SAT_RGB555(31, 31, 31),
         SAT_RGB555(24, 8, 30), SAT_RGB555(8, 20, 31)},
        /* rain: blue white */
        {0u, SAT_RGB555(14, 24, 31), SAT_RGB555(7, 14, 26), SAT_RGB555(26, 12, 30),
         SAT_RGB555(31, 31, 31), 0u},
        /* ground: road, wet, lane, curb, puddle, sidewalk, reflection */
        {0u, SAT_RGB555(8, 9, 14), SAT_RGB555(11, 13, 20), SAT_RGB555(28, 25, 5),
         SAT_RGB555(20, 18, 18), SAT_RGB555(4, 22, 30), SAT_RGB555(14, 14, 18)},
        /* RBG0 sign: magenta, cyan, white, violet, blue */
        {0u, SAT_RGB555(31, 4, 24), SAT_RGB555(3, 28, 31), SAT_RGB555(31, 31, 31),
         SAT_RGB555(24, 8, 30), SAT_RGB555(8, 20, 31)},
    };
    for (uint32_t p = 0u; p < 7u; ++p) {
        for (uint32_t i = 0u; i < 16u; ++i) palette[i] = colors[p][i < 8u ? i : 0u];
        (void)sat_vdp2_palette_upload(palette, 16u, (uint16_t)((p + 1u) * 16u));
    }
    /* The RBG0 palette 7 lands at CRAM 112..127; the line colour block is
     * separate so the two cannot collide. */
    for (uint32_t i = 0u; i < 32u; ++i) {
        g_line_palette[i] = (i < 16u)
            ? SAT_RGB555(3u + i / 2u, 18u + i / 2u, 24u + i / 2u)
            : SAT_RGB555(24u + (i - 16u) / 2u, 4u + (i - 16u) / 2u, 20u + (i - 16u));
    }
    (void)sat_vdp2_palette_upload(g_line_palette, 32u, LINE_COLOR_BASE);
}

static sat_result_t configure_nbg(uint8_t layer, uint32_t names, uint8_t priority) {
    sat_vdp2_layer_config_t c;
    sat_vdp2_layer_config_default((sat_vdp2_layer_t)layer, &c);
    c.color_mode = SAT_VDP2_COLOR_MODE_16;
    c.char_size = SAT_VDP2_CHAR_SIZE_1X1;
    c.pattern_name_words = 1u;
    c.plane_pages_x = 1u;
    c.plane_pages_y = 1u;
    c.plane_address[0] = names;
    c.plane_address[1] = names;
    c.plane_address[2] = names;
    c.plane_address[3] = names;
    c.char_base_address = CHARS;
    c.char_bank_mask = SAT_VDP2_BANK_B0;
    c.priority = priority;
    c.transparent = 1u;
    return sat_vdp2_layer_configure(&c);
}

static sat_result_t configure_rbg(void) {
    sat_vdp2_rotation_config_t r;
    sat_vdp2_rotation_config_default(&r);
    r.color_mode = SAT_VDP2_COLOR_MODE_16;
    r.char_size = SAT_VDP2_CHAR_SIZE_1X1;
    r.pattern_name_words = 1u;
    r.plane_pages_x = 1u;
    r.plane_pages_y = 1u;
    r.priority = 3u;
    r.transparent = 1u;
    r.palette = 0u;
    r.over = SAT_VDP2_ROTATION_OVER_TRANSPARENT;
    r.name_bank_mask = SAT_VDP2_BANK_A0;
    r.char_bank_mask = SAT_VDP2_BANK_A1;
    r.parameter_mode = SAT_VDP2_ROTATION_PARAMETER_A;
    for (uint32_t p = 0u; p < 16u; ++p) r.plane_address[p] = RBG_NAMES;
    r.char_base_address = RBG_CHARS;
    r.table_address = RBG_TABLE;
    return sat_vdp2_rotation_configure(&r);
}

static void build_rbg_sign(void) {
    for (uint32_t i = 0u; i < MAP_WORDS; ++i) g_rbg_map[i] = 0u;
    for (uint32_t y = 0u; y < CELLS; ++y) {
        for (uint32_t x = 0u; x < CELLS; ++x) {
            const int32_t dx = (int32_t)x - 32;
            const int32_t dy = (int32_t)y - 32;
            const int32_t d2 = dx * dx + dy * dy;
            if (d2 > 196 && d2 < 256) {
                put(g_rbg_map, x, y, (uint32_t)((x + y) & 1 ? 1u : 2u), PAL_RBG);
            } else if (d2 <= 36 && ((x + y) & 1)) {
                put(g_rbg_map, x, y, 3u, PAL_RBG);
            } else if (d2 > 64 && d2 < 81) {
                put(g_rbg_map, x, y, 4u, PAL_RBG);
            }
        }
    }
}

static void build_tables(void) {
    for (uint32_t y = 0u; y < SCREEN_H; ++y) {
        g_back[y] = SAT_RGB555(1u + y / 80u, 2u + y / 60u, 8u + y / 18u);
        const uint32_t level = (y > 150u) ? (y - 150u) : (150u - y);
        g_line_indices[y] = (uint16_t)(LINE_COLOR_BASE + (level >= 32u ? 0u : (31u - level)));
    }
}

static void setup_stream(uint32_t index, uint32_t ring) {
    sat_stage_map2_layer_desc_t* d = &g_desc[index];
    *d = (sat_stage_map2_layer_desc_t){0};
    d->map = g_ground_map;
    d->map_w = CELLS;
    d->map_h = CELLS;
    d->fill_word = SENTINEL_WORD;
    d->outside = SAT_STAGE_MAP2_OUTSIDE_WRAP;
    d->flags = SAT_STAGE_MAP2_PALETTE_OVERRIDE;
    d->palette = PAL_GROUND;
    d->vdp2_layer = SAT_VDP2_NBG1;
    d->char_bias = 24;
    d->ratio_x = SAT_FX16_ONE;
    d->ratio_y = SAT_FX16_ONE;
    d->ring.cell_shift = 3u;
    d->ring.page_shift = 6u;
    d->ring.pages_x = 1u;
    d->ring.pages_y = 1u;
    d->ring.page_word[0] = ring / 2u;
    g_config[index].tileset = &g_tileset;
    g_config[index].layers = d;
    g_config[index].layer_count = 1u;
    g_config[index].margin_cells = 2u;
    g_config[index].viewport_w = SCREEN_W;
    g_config[index].viewport_h = SCREEN_H;
    g_storage[index].words = g_stage_words[index];
    g_storage[index].word_capacity = STAGE_WORDS;
    g_storage[index].runs = g_stage_runs[index];
    g_storage[index].run_capacity = STAGE_RUNS;
}

static uint32_t crc_word(uint32_t crc, uint16_t word) {
    crc ^= word;
    for (uint32_t i = 0u; i < 16u; ++i) {
        crc = (crc >> 1u) ^ (0xEDB88320u & (uint32_t)-(int32_t)(crc & 1u));
    }
    return crc;
}

static void update_telemetry(uint8_t compute_crc) {
    const sat_stage_map2_layer_state_t* l = &g_stream[g_active].layer[0];
    g_neon_city.active_ring = g_active;
    g_neon_city.camera_x = g_camera_x;
    g_neon_city.win_x0 = (uint32_t)l->x0 & (CELLS - 1u);
    g_neon_city.win_y0 = (uint32_t)l->y0 & (CELLS - 1u);
    g_neon_city.win_cols = l->cols;
    g_neon_city.win_rows = l->rows;
    if (compute_crc == 0u) return;
    const uint32_t base_word = (g_active == 0u ? RING_A : RING_B) / 2u;
    uint32_t ce = 0xFFFFFFFFu, cv = 0xFFFFFFFFu, sentinel = 0u;
    for (int32_t r = 0; r < (int32_t)l->rows; ++r) {
        const int32_t cy = l->y0 + r;
        const uint32_t ry = (uint32_t)cy & (CELLS - 1u);
        int32_t done = 0;
        while (done < (int32_t)l->cols) {
            const int32_t cx = l->x0 + done;
            const uint32_t rx = (uint32_t)cx & (CELLS - 1u);
            uint32_t n = CELLS - rx;
            if (n > (uint32_t)((int32_t)l->cols - done)) n = (uint32_t)((int32_t)l->cols - done);
            (void)sat_vdp2_vram_read_words(base_word + ry * CELLS + rx, g_row, n);
            for (uint32_t i = 0u; i < n; ++i) {
                const uint16_t expected = sat_stage_map2_cell(&g_stream[g_active], 0, cx + (int32_t)i, cy);
                const uint16_t actual = g_row[i];
                ce = crc_word(ce, expected);
                cv = crc_word(cv, actual);
                if (actual == SENTINEL_WORD) ++sentinel;
            }
            done += (int32_t)n;
        }
    }
    g_neon_city.crc_expected = ce ^ 0xFFFFFFFFu;
    g_neon_city.crc_vram = cv ^ 0xFFFFFFFFu;
    if (g_neon_city.crc_expected != g_neon_city.crc_vram) ++g_neon_city.crc_mismatches;
    g_neon_city.sentinel_cells = sentinel;
}

static sat_result_t publish_next(void) {
    const uint8_t next = (uint8_t)(g_active ^ 1u);
    const uint32_t next_camera = (g_camera_x + CAMERA_STEP) % CAMERA_WRAP;
    sat_result_t st = sat_stage_map2_set_view(&g_stream[next],
                                              (sat_fx16_t)(next_camera << 16u), 0);
    if (st != SAT_OK) return st;
    uint32_t runs = 0u, words = 0u;
    sat_stage_map2_pending(&g_stream[next], &runs, &words);
    if (words > g_neon_city.max_pending_words) g_neon_city.max_pending_words = words;
    st = sat_stage_map2_commit_vdp2(&g_stream[next]);
    if (st != SAT_OK) return st;
    st = sat_vdp2_layer_set_pattern_name_base(SAT_VDP2_NBG1,
                                              next == 0u ? RING_A : RING_B);
    if (st != SAT_OK) return st;
    st = sat_stage_map2_apply_scroll_vdp2(&g_stream[next]);
    if (st != SAT_OK) return st;
    g_camera_x = next_camera;
    return SAT_OK;
}

static void rotation_params(uint32_t angle_deg, sat_vdp2_rotation_params_t* out) {
    const sat_fx16_t a = sat_sin_deg((sat_fx16_t)(angle_deg << 16u));
    const sat_fx16_t c = sat_cos_deg((sat_fx16_t)(angle_deg << 16u));
    *out = (sat_vdp2_rotation_params_t){0};
    out->xst = 96 << 16;
    out->yst = 144 << 16;
    out->dyst = SAT_FX16_ONE;
    out->dx = SAT_FX16_ONE;
    out->matrix[0] = c;
    out->matrix[1] = -a;
    out->matrix[3] = a;
    out->matrix[4] = c;
    out->cx = 256;
    out->cy = 256;
    out->kx = SAT_FX16_ONE;
    out->ky = SAT_FX16_ONE;
    out->coefficient_step = SAT_FX16_ONE;
}

static uint32_t ticks_us(uint64_t ticks) {
    const uint32_t hz = sat_time_frequency_hz();
    return hz == 0u ? 0u : (uint32_t)((ticks * 1000000u) / hz);
}

static void update_scene(uint32_t frame, uint8_t paused) {
    const uint64_t start = sat_time_ticks();
    const uint32_t angle = (frame * 2u) % 360u;
    sat_vdp2_rotation_params_t pa, pb;
    rotation_params(angle, &pa);
    rotation_params(angle, &pb);
    (void)sat_vdp2_rotation_write_params(RBG_TABLE, &pa, &pb);
    g_neon_city.sign_angle = angle;

    /* Ground streaming: only when not paused. */
    if (!paused) {
        if (g_pending != 0u) {
            g_active ^= 1u;
            g_pending = 0u;
            ++g_neon_city.switches;
        }
        if (publish_next() == SAT_OK) g_pending = 1u;
    }

    /* Street light pool: window W0 is the colour-calculation window. */
    const int32_t bloom = 90 + (int32_t)((frame * 2u) % 140u);
    (void)sat_vdp2_window_set_ellipse(0u, WINDOW_TABLE, bloom, 170, 70, 28);
    g_neon_city.bloom_x = (uint32_t)bloom;

    /* Rain sign sway and colours. */
    const int32_t sway = (int32_t)((frame * 0x1800u) & 0xFFFFu) - 0x8000;
    (void)sat_vdp2_line_scroll_fill_wave(SAT_VDP2_NBG0, SCREEN_H, 3 << 16, 90u,
                                         (int32_t)((frame * 3u) << 16));
    (void)sat_vdp2_layer_set_scroll(SAT_VDP2_NBG0, sway, 0);
    (void)sat_vdp2_layer_set_scroll(SAT_VDP2_NBG3, (int32_t)(frame * 0x4000u), 0);
    (void)sat_vdp2_layer_set_scroll(SAT_VDP2_NBG2, (int32_t)(frame * 0x8000u), 0);

    const sat_vdp2_color_offset_t sign_offset = {
        (int16_t)(-2 + (int16_t)((angle / 30u) % 5u)),
        (int16_t)(-1 + (int16_t)((angle / 45u) % 4u)), 2,
    };
    const sat_vdp2_color_offset_t rain_offset = {0, 0, (int16_t)(-3 + (int16_t)((angle / 20u) % 6u))};
    (void)sat_vdp2_color_offset_set(SAT_VDP2_COLOR_OFFSET_A, &sign_offset);
    (void)sat_vdp2_color_offset_set(SAT_VDP2_COLOR_OFFSET_B, &rain_offset);

    const uint32_t us = ticks_us(sat_time_ticks() - start);
    if (us > g_neon_city.update_us_max) g_neon_city.update_us_max = us;
}

static void draw_hud(uint32_t commands) {
    (void)sat_draw_rect_screen(0, 0, 210, 56, SAT_RGB555(0, 0, 0));
    (void)sat_ascii_font_draw_text_screen_indexed8(&g_font, "VDP2 NEON CITY", 8, 8, 8, 0u, 0u);
    (void)sat_ascii_font_draw_label_u32(&g_font, "FRAME ", g_neon_city.frames, 8, 24, 8, 0u, 0u);
    (void)sat_ascii_font_draw_label_u32(&g_font, "CMDS ", commands, 104, 24, 8, 0u, 0u);
    (void)sat_ascii_font_draw_label_u32(&g_font, "RING ", g_neon_city.active_ring, 8, 40, 8, 0u, 0u);
    (void)sat_ascii_font_draw_text_screen_indexed8(&g_font, "B PAUSE STREAM R RESET START",
        8, SCREEN_H - 16, 8, 0u, 0u);
}

static sat_result_t setup_scene(void) {
    sat_result_t st = SAT_OK;
    g_neon_city.layers_ok =
        configure_nbg(SAT_VDP2_NBG1, RING_A, 4u) == SAT_OK &&
        configure_nbg(SAT_VDP2_NBG2, NAMES2, 2u) == SAT_OK &&
        configure_nbg(SAT_VDP2_NBG3, NAMES3, 1u) == SAT_OK &&
        configure_nbg(SAT_VDP2_NBG0, NAMES0, 6u) == SAT_OK;
    if (!g_neon_city.layers_ok) st = SAT_ERR_CAPACITY;

    g_neon_city.rotation_ok = configure_rbg() == SAT_OK;
    if (!g_neon_city.rotation_ok) st = SAT_ERR_CAPACITY;

    g_tileset.cells = g_cells;
    g_tileset.metatile_count = MAP_WORDS;
    g_tileset.shift = 0u;
    g_tileset.reserved = 0u;
    setup_stream(0u, RING_A);
    setup_stream(1u, RING_B);
    g_neon_city.stream_ok = 1u;
    for (uint32_t i = 0u; i < 2u; ++i) {
        uint32_t words = 0u, runs = 0u;
        if (sat_stage_map2_requirements(&g_config[i], &words, &runs) != SAT_OK ||
            words > STAGE_WORDS || runs > STAGE_RUNS ||
            sat_stage_map2_init(&g_stream[i], &g_config[i], &g_storage[i]) != SAT_OK) {
            g_neon_city.stream_ok = 0u;
        }
    }
    if (g_neon_city.stream_ok != 0u) {
        for (uint32_t i = 0u; i < 2u; ++i) {
            if (sat_stage_map2_set_view(&g_stream[i], 0, 0) != SAT_OK ||
                sat_stage_map2_commit_vdp2(&g_stream[i]) != SAT_OK) {
                g_neon_city.stream_ok = 0u;
            }
        }
    }
    if (g_neon_city.stream_ok == 0u) st = SAT_ERR_CAPACITY;

    /* Raster tables, line scroll on NBG0, and the light-pool window. */
    g_neon_city.window_ok =
        sat_vdp2_window_set_ellipse(0u, WINDOW_TABLE, 160, 170, 70, 28) == SAT_OK;
    {
        sat_vdp2_screen_window_t sw;
        sat_vdp2_screen_window_default(SAT_VDP2_SCREEN_COLOR_CALC, &sw);
        sw.w0 = SAT_VDP2_WINDOW_AREA_INSIDE;
        g_neon_city.window_ok = g_neon_city.window_ok &&
            sat_vdp2_screen_window_set(&sw) == SAT_OK;
        sat_vdp2_screen_window_t bad;
        sat_vdp2_screen_window_default(SAT_VDP2_SCREEN_NBG3, &bad);
        bad.w1 = SAT_VDP2_WINDOW_AREA_INSIDE;
        g_neon_city.refused_window = sat_vdp2_screen_window_set(&bad) == SAT_ERR_INVALID_ARG;
    }
    if (!g_neon_city.window_ok) st = SAT_ERR_CAPACITY;

    {
        sat_vdp2_line_scroll_config_t ls = {0};
        ls.layer = SAT_VDP2_NBG0;
        ls.horizontal = 1u;
        ls.interval = 3u;
        ls.table_address = SCROLL0_TABLE;
        if (sat_vdp2_layer_line_scroll_enable(&ls) != SAT_OK) st = SAT_ERR_CAPACITY;
    }
    g_neon_city.color_offset_ok =
        sat_vdp2_color_offset_enable(SAT_VDP2_LAYER_NBG2, SAT_VDP2_COLOR_OFFSET_A) == SAT_OK &&
        sat_vdp2_color_offset_enable(SAT_VDP2_LAYER_NBG0, SAT_VDP2_COLOR_OFFSET_B) == SAT_OK;
    if (!g_neon_city.color_offset_ok) st = SAT_ERR_UNSUPPORTED;
    if (sat_vdp2_color_calc_set_add(1u) != SAT_OK ||
        sat_vdp2_screen_color_calc_set(SAT_VDP2_SCREEN_NBG1, 1u, 0u) != SAT_OK ||
        sat_vdp2_layer_set_line_color_insert(SAT_VDP2_NBG1, 1u) != SAT_OK) {
        st = SAT_ERR_UNSUPPORTED;
    }
    (void)sat_vdp2_back_screen_set_lines(BACK_TABLE, g_back, SCREEN_H);
    (void)sat_vdp2_line_color_screen_set(LINE_COLOR_TABLE, g_line_indices, SCREEN_H);

    g_neon_city.refused_base =
        sat_vdp2_layer_set_pattern_name_base(SAT_VDP2_NBG1, 0x01000u) == SAT_ERR_INVALID_ARG;
    (void)sat_vdp2_sprite_set_priority(7u);
    (void)sat_vdp2_layers_commit();
    return st;
}

int main(void) {
    sat_video_config_t video = {SCREEN_W, SCREEN_H, SAT_VIDEO_AUTO, 0u};
    if (sat_init(&video) != SAT_OK) return 1;
    g_neon_city.magic = NEON_MAGIC;
    if (sat_ascii_font_init_8x8_indexed8(&g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, FONT_BANK) != SAT_OK) {
        return 1;
    }
    make_tiles();
    make_rbg_tiles();
    build_maps();
    build_rbg_sign();
    build_tables();
    upload_palettes();

    (void)sat_vdp2_vram_write_words(NAMES3 / 2u, g_maps[0], MAP_WORDS);
    (void)sat_vdp2_vram_write_words(NAMES2 / 2u, g_maps[1], MAP_WORDS);
    (void)sat_vdp2_vram_write_words(NAMES0 / 2u, g_maps[2], MAP_WORDS);
    (void)sat_vdp2_vram_write_words(RBG_NAMES / 2u, g_rbg_map, MAP_WORDS);
    (void)sat_vdp2_vram_write_words(CHARS / 2u, g_tiles, 4u * 8u * 16u);
    (void)sat_vdp2_vram_write_words(RBG_CHARS / 2u, g_rbg_tiles, 8u * 16u);
    for (uint32_t i = 0u; i < MAP_WORDS; ++i) g_sentinel[i] = SENTINEL_WORD;
    (void)sat_vdp2_vram_write_words(RING_A / 2u, g_sentinel, MAP_WORDS);
    (void)sat_vdp2_vram_write_words(RING_B / 2u, g_sentinel, MAP_WORDS);

    g_neon_city.init_ok = (setup_scene() == SAT_OK && g_neon_city.layers_ok != 0u &&
                           g_neon_city.rotation_ok != 0u && g_neon_city.stream_ok != 0u &&
                           g_neon_city.window_ok != 0u && g_neon_city.color_offset_ok != 0u &&
                           g_neon_city.refused_base != 0u &&
                           g_neon_city.refused_window != 0u) ? 1u : 0u;
    g_neon_city.active_ring = 0u;

    uint8_t paused = 0u;
    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_wait_vblank() != SAT_OK) break;
        ++g_neon_city.frames;
        update_scene(g_neon_city.frames, paused);
        if ((g_neon_city.frames % 60u) == 0u) update_telemetry(1u);
        else update_telemetry(0u);
        (void)sat_vdp2_layers_commit();
        (void)sat_vdp1_set_erase_transparent();
        if (sat_begin_frame() != SAT_OK) break;
        (void)sat_pad_poll(&pad);
        if ((pad.pressed & SAT_PAD_B) != 0u) paused ^= 1u;
        if ((pad.pressed & SAT_PAD_R) != 0u) { g_camera_x = 0u; paused = 0u; }
        sat_vdp1_command_stats_t commands = {0};
        (void)sat_vdp1_command_stats(&commands);
        g_neon_city.vdp1_commands = commands.used;
        {
            sat_dma_stats_t ds = {0};
            sat_dma_get_stats(&ds);
            g_neon_city.dma_scu = ds.scu_transfers;
            g_neon_city.dma_cpu = ds.cpu_copies;
        }
        g_neon_city.staged_words = sat_stage_map2_stats(&g_stream[0]).cells_staged +
                                   sat_stage_map2_stats(&g_stream[1]).cells_staged;
        g_neon_city.committed_words = sat_stage_map2_stats(&g_stream[0]).cells_committed +
                                      sat_stage_map2_stats(&g_stream[1]).cells_committed;
        draw_hud(commands.used);
        (void)sat_end_frame();
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }
    (void)sat_shutdown();
    return 0;
}
