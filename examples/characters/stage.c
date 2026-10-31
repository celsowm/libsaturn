/* VDP2 fighting stage (E11) - backdrop implementation.
 *
 * Four NBG layers behind the existing character demo's 3D fighters:
 *
 *   NBG3  arena stands and a slowly drifting crowd, priority 1
 *   NBG2  hanging banners, priority 3, dimmed/brightened by colour offset A
 *   NBG1  two moving spotlight beams, priority 2, clipped to the upper hall
 *         by window W0
 *   NBG0  ring apron rails and corner lights, priority 4
 *
 * The 3D fighters and the training mat are the normal VDP1 scene painter:
 * every layer here sits at priority 4 or below, and the sprites at 6, so the
 * backdrop cannot cover a fighter. The stage consumes public VDP2 APIs only.
 *
 * This file is compiled only for examples/vdp2_fighting_stage; fighter_arena
 * keeps the legacy NBG0 background and is unchanged. */

#include "stage.h"

#include <stdint.h>

#include "saturn/color.h"
#include "saturn/math3d.h"
#include "saturn/video.h"
#include "saturn/vdp2.h"
#include "saturn/vdp2_color_offset.h"
#include "saturn/vdp2_compose.h"
#include "saturn/vdp2_layers.h"

#define STAGE_MAGIC SAT_FOURCC('S', 'T', 'G', '1')
#define STAGE_CELLS 64u
#define STAGE_MAP_WORDS (STAGE_CELLS * STAGE_CELLS)
#define NAMES0 0x40000u
#define NAMES1 0x42000u
#define NAMES2 0x44000u
#define NAMES3 0x46000u
#define CHARS  0x48000u
#define PAL_CROWD 1u
#define PAL_BANNER 2u
#define PAL_LIGHT 3u
#define PAL_APRON 4u

volatile vdp2_stage_telemetry_t g_vdp2_stage;

static uint16_t g_maps[4][STAGE_MAP_WORDS];
/* Eight character slots per layer (five used), so layer n starts at
 * character n * 8 as the map builder's layer*8+tile assumes. */
static uint16_t g_tiles[4u * 8u * 16u];
static uint32_t g_on = 0u;
static uint32_t g_frames = 0u;

static void set_tile(uint32_t layer, uint32_t tile, const uint8_t rows[8][8]) {
    const uint32_t base = (layer * 8u + tile) * 16u;
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
    for (uint32_t layer = 0u; layer < 4u; ++layer) {
        for (uint32_t y = 0u; y < 8u; ++y)
            for (uint32_t x = 0u; x < 8u; ++x) rows[y][x] = 0u;
        set_tile(layer, 0u, rows);
    }

    /* NBG3 stands: 0 wall, 1 crowd, 2 railing, 3 camera flash. */
    for (uint32_t y = 0u; y < 8u; ++y) {
        for (uint32_t x = 0u; x < 8u; ++x) {
            rows[y][x] = (uint8_t)((y & 1u) ? 1u : 0u);
        }
    }
    set_tile(3u, 0u, rows);
    for (uint32_t y = 0u; y < 8u; ++y) {
        for (uint32_t x = 0u; x < 8u; ++x) {
            uint8_t c = 1u;
            if (((x * 5u + y * 3u) & 7u) == 0u) c = 2u;
            if (y == 0u) c = 3u;
            rows[y][x] = c;
        }
    }
    set_tile(3u, 1u, rows);
    for (uint32_t y = 0u; y < 8u; ++y)
        for (uint32_t x = 0u; x < 8u; ++x) rows[y][x] = (uint8_t)(y < 2u ? 3u : 4u);
    set_tile(3u, 2u, rows);
    for (uint32_t y = 0u; y < 8u; ++y)
        for (uint32_t x = 0u; x < 8u; ++x) rows[y][x] = (uint8_t)(((x + y) & 3u) == 0u ? 2u : 1u);
    set_tile(3u, 3u, rows);

    /* NBG2 banners: 1 red, 2 blue, 3 gold, 4 pole. */
    for (uint32_t tile = 1u; tile <= 3u; ++tile) {
        for (uint32_t y = 0u; y < 8u; ++y) {
            for (uint32_t x = 0u; x < 8u; ++x) {
                uint8_t c = (uint8_t)tile;
                if (y == 7u || x == 0u || x == 7u) c = 4u;
                if ((y == 3u || y == 4u) && x > 1u && x < 6u) c = 5u;
                rows[y][x] = c;
            }
        }
        set_tile(2u, tile, rows);
    }
    for (uint32_t y = 0u; y < 8u; ++y)
        for (uint32_t x = 0u; x < 8u; ++x) rows[y][x] = (uint8_t)(x == 3u ? 4u : 0u);
    set_tile(2u, 4u, rows);

    /* NBG1 spotlights: 1 beam body, 2 core, 3 sparse glow. */
    for (uint32_t tile = 1u; tile <= 3u; ++tile) {
        for (uint32_t y = 0u; y < 8u; ++y) {
            for (uint32_t x = 0u; x < 8u; ++x) {
                const uint8_t width = (uint8_t)(1u + y / 3u);
                const int dx = (int)x - 4;
                uint8_t c = 0u;
                if (dx >= -(int)width && dx <= (int)width) {
                    c = (uint8_t)(tile == 3u ? (y & 1u ? 1u : 0u) : tile);
                }
                rows[y][x] = c;
            }
        }
        set_tile(1u, tile, rows);
    }

    /* NBG0 apron: 1 metal, 2 cable, 3 lamp. */
    for (uint32_t y = 0u; y < 8u; ++y) {
        for (uint32_t x = 0u; x < 8u; ++x) {
            rows[y][x] = (uint8_t)((x + y) & 1u ? 1u : 2u);
        }
    }
    set_tile(0u, 1u, rows);
    for (uint32_t y = 0u; y < 8u; ++y)
        for (uint32_t x = 0u; x < 8u; ++x) rows[y][x] = (uint8_t)(y == 3u || y == 4u ? 2u : 0u);
    set_tile(0u, 2u, rows);
    for (uint32_t y = 0u; y < 8u; ++y)
        for (uint32_t x = 0u; x < 8u; ++x) {
            const int dx = (int)x - 3, dy = (int)y - 3;
            rows[y][x] = (uint8_t)((dx * dx + dy * dy) < 7 ? 3u : 0u);
        }
    set_tile(0u, 3u, rows);
}

static void put(uint16_t* map, uint32_t x, uint32_t y, uint32_t layer,
                uint32_t tile, uint32_t palette) {
    if (x >= STAGE_CELLS || y >= STAGE_CELLS) return;
    map[y * STAGE_CELLS + x] =
        (uint16_t)((palette << 12u) | ((layer * 8u + tile) & 0x3FFu));
}

static void build_maps(void) {
    for (uint32_t m = 0u; m < 4u; ++m)
        for (uint32_t i = 0u; i < STAGE_MAP_WORDS; ++i) g_maps[m][i] = 0u;

    /* NBG3: wall, railing, crowd tiers, occasional camera flash. */
    for (uint32_t y = 0u; y < STAGE_CELLS; ++y) {
        for (uint32_t x = 0u; x < STAGE_CELLS; ++x) {
            uint32_t tile = 0u;
            if (y == 8u) tile = 2u;
            else if (y > 8u && y < 28u) tile = 1u;
            put(g_maps[3], x, y, 3u, tile, PAL_CROWD);
        }
    }
    for (uint32_t i = 0u; i < 20u; ++i) {
        const uint32_t x = (i * 13u + 5u) & 63u;
        const uint32_t y = 10u + (i * 7u) % 17u;
        put(g_maps[3], x, y, 3u, 3u, PAL_CROWD);
    }

    /* NBG2: banners hanging from the upper hall. */
    for (uint32_t i = 0u; i < 8u; ++i) {
        const uint32_t x = 2u + i * 8u;
        for (uint32_t y = 0u; y < 10u; ++y) {
            put(g_maps[2], x, y, 2u, 1u + (i % 3u), PAL_BANNER);
        }
        put(g_maps[2], x, 10u, 2u, 4u, PAL_BANNER);
    }

    /* NBG1: two spotlight columns. Scroll moves both. */
    for (uint32_t y = 0u; y < 14u; ++y) {
        put(g_maps[1], 8u, y, 1u, y < 2u ? 2u : 1u, PAL_LIGHT);
        put(g_maps[1], 9u, y, 1u, 3u, PAL_LIGHT);
        put(g_maps[1], 42u, y, 1u, y < 2u ? 2u : 1u, PAL_LIGHT);
        put(g_maps[1], 43u, y, 1u, 3u, PAL_LIGHT);
    }

    /* NBG0: the ring apron across the bottom, with corner posts. */
    for (uint32_t x = 0u; x < STAGE_CELLS; ++x) {
        for (uint32_t y = 25u; y < 28u; ++y) {
            const uint32_t tile = (y == 25u) ? 3u : 1u;
            put(g_maps[0], x, y, 0u, tile, PAL_APRON);
        }
    }
    for (uint32_t y = 19u; y < 28u; ++y) {
        put(g_maps[0], 0u, y, 0u, 2u, PAL_APRON);
        put(g_maps[0], 63u, y, 0u, 2u, PAL_APRON);
    }
}

static sat_result_t configure_layer(uint8_t layer, uint32_t names, uint8_t priority) {
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

static void upload_palettes(void) {
    uint16_t palette[16];
    const uint16_t colors[4][6] = {
        /* crowd: wall, crowd dark, crowd light, rail, flash */
        {0u, SAT_RGB555(5, 9, 14), SAT_RGB555(13, 18, 25), SAT_RGB555(24, 30, 38),
         SAT_RGB555(9, 6, 4), SAT_RGB555(31, 31, 26)},
        /* banners: red, blue, gold, pole, emblem */
        {0u, SAT_RGB555(24, 5, 6), SAT_RGB555(5, 8, 24), SAT_RGB555(26, 21, 4),
         SAT_RGB555(16, 16, 18), SAT_RGB555(31, 30, 24)},
        /* lights: warm white, core, glow */
        {0u, SAT_RGB555(24, 20, 10), SAT_RGB555(31, 31, 28), SAT_RGB555(31, 25, 12),
        SAT_RGB555(0, 0, 0), SAT_RGB555(0, 0, 0)},
        /* apron: metal, cable, lamp */
        {0u, SAT_RGB555(7, 9, 12), SAT_RGB555(14, 12, 9), SAT_RGB555(27, 22, 6),
        SAT_RGB555(0, 0, 0), SAT_RGB555(0, 0, 0)},
    };
    for (uint32_t p = 0u; p < 4u; ++p) {
        for (uint32_t i = 0u; i < 16u; ++i) palette[i] = colors[p][i < 6u ? i : 0u];
        (void)sat_vdp2_palette_upload(palette, 16u, (uint16_t)((p + 1u) * 16u));
    }
}

sat_result_t vdp2_stage_init(void) {
    g_vdp2_stage.magic = STAGE_MAGIC;
    g_on = 1u;
    g_frames = 0u;
    make_tiles();
    build_maps();
    upload_palettes();
    for (uint32_t i = 0u; i < 4u; ++i) {
        (void)sat_vdp2_vram_write_words((NAMES0 + i * 0x2000u) / 2u, g_maps[i], STAGE_MAP_WORDS);
    }
    (void)sat_vdp2_vram_write_words(CHARS / 2u, g_tiles, 4u * 8u * 16u);

    sat_result_t st = SAT_OK;
    g_vdp2_stage.layers_ok =
        configure_layer(SAT_VDP2_NBG0, NAMES0, 4u) == SAT_OK &&
        configure_layer(SAT_VDP2_NBG1, NAMES1, 2u) == SAT_OK &&
        configure_layer(SAT_VDP2_NBG2, NAMES2, 3u) == SAT_OK &&
        configure_layer(SAT_VDP2_NBG3, NAMES3, 1u) == SAT_OK;
    if (!g_vdp2_stage.layers_ok) st = SAT_ERR_CAPACITY;

    {
        const sat_vdp2_window_rect_t upper = {0u, 0u, 319u, 127u};
        sat_vdp2_screen_window_t sw;
        sat_vdp2_screen_window_default(SAT_VDP2_SCREEN_NBG1, &sw);
        sw.w0 = SAT_VDP2_WINDOW_AREA_INSIDE;
        g_vdp2_stage.window_ok =
            sat_vdp2_window_set_rect(0u, &upper) == SAT_OK &&
            sat_vdp2_screen_window_set(&sw) == SAT_OK;
        if (!g_vdp2_stage.window_ok) st = SAT_ERR_CAPACITY;
        sat_vdp2_screen_window_t bad;
        sat_vdp2_screen_window_default(SAT_VDP2_SCREEN_NBG2, &bad);
        bad.w1 = SAT_VDP2_WINDOW_AREA_INSIDE;
        g_vdp2_stage.refused_window =
            sat_vdp2_screen_window_set(&bad) == SAT_ERR_INVALID_ARG;
    }
    {
        sat_vdp2_line_scroll_config_t ls = {0};
        ls.layer = SAT_VDP2_NBG2;
        ls.horizontal = 1u;
        ls.table_address = 0x4F000u;
        g_vdp2_stage.refused_scroll =
            sat_vdp2_layer_line_scroll_enable(&ls) == SAT_ERR_INVALID_ARG;
    }

    const sat_vdp2_color_offset_t dim = {0, -4, -2};
    g_vdp2_stage.color_offset_ok =
        sat_vdp2_color_offset_set(SAT_VDP2_COLOR_OFFSET_A, &dim) == SAT_OK &&
        sat_vdp2_color_offset_enable(SAT_VDP2_LAYER_NBG3 | SAT_VDP2_LAYER_NBG2,
                                     SAT_VDP2_COLOR_OFFSET_A) == SAT_OK;
    if (!g_vdp2_stage.color_offset_ok) st = SAT_ERR_UNSUPPORTED;

    (void)sat_vdp2_back_color_set(SAT_RGB555(2, 4, 10));
    (void)sat_vdp2_sprite_set_priority(6u);
    (void)sat_vdp2_layers_commit();
    g_vdp2_stage.stage_on = 1u;
    g_vdp2_stage.init_ok =
        (st == SAT_OK && g_vdp2_stage.layers_ok != 0u &&
         g_vdp2_stage.window_ok != 0u && g_vdp2_stage.color_offset_ok != 0u &&
         g_vdp2_stage.refused_window != 0u && g_vdp2_stage.refused_scroll != 0u) ? 1u : 0u;
    return st;
}

static void apply_enabled(void) {
    for (uint8_t layer = 0u; layer < 4u; ++layer) {
        (void)sat_vdp2_layer_set_enabled((sat_vdp2_layer_t)layer, g_on ? 1u : 0u);
    }
}

void vdp2_stage_toggle(void) {
    g_on ^= 1u;
    apply_enabled();
    ++g_vdp2_stage.toggles;
    g_vdp2_stage.stage_on = g_on;
    (void)sat_vdp2_layers_commit();
}

uint32_t vdp2_stage_on(void) {
    return g_on;
}

void vdp2_stage_frame(void) {
    ++g_frames;
    ++g_vdp2_stage.frames;
    if (g_frames == 70u || g_frames == 120u) {
        g_on ^= 1u;
        apply_enabled();
        ++g_vdp2_stage.toggles;
        ++g_vdp2_stage.auto_toggles;
        g_vdp2_stage.stage_on = g_on;
    }
    if (g_on) ++g_vdp2_stage.stage_visible_frames;

    /* Crowd drift, banner sway and two moving beams, all in 16.16 dots. */
    const sat_fx16_t sway = sat_sin_deg((sat_fx16_t)((g_frames * 2u) << 16u));
    const int32_t sway_dots = (int32_t)(((int64_t)sway * (10 << 16)) >> 16);
    (void)sat_vdp2_layer_set_scroll(SAT_VDP2_NBG3, (int32_t)(g_frames * 0x8000u), 0);
    (void)sat_vdp2_layer_set_scroll(SAT_VDP2_NBG2, sway_dots, 0);
    (void)sat_vdp2_layer_set_scroll(SAT_VDP2_NBG1, sway_dots * 3, 0);
    (void)sat_vdp2_layer_set_scroll(SAT_VDP2_NBG0, 0, 0);

    /* Colour offset A: the whole stand dims and brightens with the sway. */
    const sat_vdp2_color_offset_t glow = {
        (int16_t)(-6 + (sway >> 11)), (int16_t)(-3 + (sway >> 12)), (int16_t)(-1),
    };
    (void)sat_vdp2_color_offset_set(SAT_VDP2_COLOR_OFFSET_A, &glow);
    (void)sat_vdp2_layers_commit();
}

void vdp2_stage_note_frame(uint32_t commands, uint32_t geometry_us) {
    g_vdp2_stage.last_commands = commands;
    g_vdp2_stage.last_geometry_us = geometry_us;
    if (g_on) {
        g_vdp2_stage.commands_on = commands;
        g_vdp2_stage.geometry_us_on = geometry_us;
    } else {
        g_vdp2_stage.commands_off = commands;
        g_vdp2_stage.geometry_us_off = geometry_us;
    }
}
