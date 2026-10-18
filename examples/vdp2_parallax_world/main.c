/* VDP2 parallax world (E10).
 *
 * An original coast at dawn, drawn entirely by the VDP2 while the SH-2 only
 * advances a few table pointers:
 *
 *   back screen   per-line sky gradient (VRAM table, rewritten each VBlank)
 *   NBG3          drifting cloud banks, priority 1, clipped above the horizon
 *                 by window W0 (a rectangle on the NBG3 screen)
 *   NBG2          distant mountains, priority 2
 *   RBG0          the sea: a repeating 64x64-cell cell map drawn through the
 *                 rotation screen, priority 3. A+B parameters scroll it and
 *                 the map's transparent rows above the horizon expose the sky.
 *   NBG1          the shore and tree line, priority 4, moved by a per-line
 *                 horizontal sine (line scroll) so the surf edge ripples
 *   NBG0          foreground leaf silhouettes, priority 6, a second line-scroll
 *                 table with a slower phase
 *
 * Colour calculation runs in add mode (CCCTL CCMD) on RBG0 and NBG1: the RBG0
 * sea adds the sky beneath it and NBG1 adds the line-colour screen, a warm
 * per-line band, so the shore catches the sunrise. The sky, cloud, mountain,
 * sea, shore and leaf
 * maps and tiles are original procedural data; no external assets.
 *
 * Reference comparison (the plan's "CPU/VDP1 savings" slice): at startup the
 * example renders one comparable 320x224 background into an indexed8 buffer,
 * timed, and uploads it once as a VDP1 texture. Press R (or wait for the
 * automatic demonstration around frame 600) to hide the VDP2 layers and show
 * that pre-rendered image with one VDP1 sprite. The HUD and telemetry report
 * the VDP1 commands of both paths and the per-frame CPU cost of the VDP2
 * table updates versus the one-time software build. The reference is a
 * static image, not an animated substitute; it measures the cost of producing
 * a comparable frame in software, not a claim that VDP1 is free.
 *
 * Controls: A toggles the reference image, R toggles the reference (same as A
 * for harness pad scripts), START exits.
 *
 * g_parallax holds what harness/tests/test_vdp2_parallax_world.py checks. */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/saturn.h"
#include "saturn/time.h"
#include "saturn/vdp1.h"
#include "saturn/vdp2.h"
#include "saturn/vdp2_compose.h"
#include "saturn/vdp2_layers.h"

#define PARALLAX_MAGIC SAT_FOURCC('P', 'A', 'R', '1')
#define SCREEN_W 320u
#define SCREEN_H 224u
#define CELLS 64u
#define MAP_WORDS (CELLS * CELLS)
#define HORIZON_ROW 14u          /* 14 * 8 = 112 dots */
#define SHORE_ROW 20u            /* 20 * 8 = 160 dots */
#define REF_AUTO_ON 600u
#define REF_AUTO_OFF 660u

#define NAMES0 0x40000u
#define NAMES1 0x42000u
#define NAMES2 0x44000u
#define NAMES3 0x46000u
#define CHARS  0x48000u
#define BACK_TABLE 0x4C000u
#define LINE_COLOR_TABLE 0x4C200u
#define SCROLL0_TABLE 0x4D000u
#define SCROLL1_TABLE 0x4E000u
#define WINDOW_TABLE 0x4F000u

#define RBG_NAMES 0x00000u
#define RBG_CHARS 0x20000u
#define RBG_TABLE 0x60000u

#define PAL_CLOUD 2u
#define PAL_MOUNTAIN 3u
#define PAL_SEA 4u
#define PAL_SHORE 5u
#define PAL_LEAF 6u
#define PAL_REFERENCE 6u   /* the 256-entry indexed8 bank for the reference image */
#define FONT_BANK 7u
#define LINE_COLOR_BASE 128u

typedef struct parallax_results {
    uint32_t magic;
    uint32_t init_ok;
    uint32_t layers_ok;
    uint32_t rotation_ok;
    uint32_t back_ok;
    uint32_t line_color_ok;
    uint32_t window_ok;
    uint32_t refused_scroll;
    uint32_t refused_window;
    uint32_t frames;
    uint32_t reference;
    uint32_t reference_seen;
    uint32_t reference_ready;
    uint32_t bg_commands_vdp2;
    uint32_t bg_commands_reference;
    uint32_t commands_vdp2;
    uint32_t commands_reference;
    uint32_t update_us_last;
    uint32_t update_us_max;
    uint32_t software_fill_us;
    uint32_t software_upload_us;
    uint32_t sea_scroll;
    uint32_t sun_x;
    uint32_t late_swaps;
} parallax_results_t;

volatile parallax_results_t g_parallax;

static sat_ascii_font_t g_font;
static uint16_t g_maps[4][MAP_WORDS];
static uint16_t g_rbg_map[MAP_WORDS];
static uint16_t g_tiles[8u * 6u * 16u];   /* six layers x eight tiles */
static uint16_t g_back[SCREEN_H];
static uint16_t g_line_indices[SCREEN_H];
static uint16_t g_line_palette[32];
static uint8_t g_reference_pixels[SCREEN_W * SCREEN_H];
static uint16_t g_reference_palette[256];
static sat_vdp1_texture_t g_reference_texture;
static uint32_t g_seed = 0x13579BDFu;

static uint32_t rnd(void) {
    g_seed = g_seed * 1664525u + 1013904223u;
    return g_seed >> 8u;
}

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
    for (uint32_t layer = 0u; layer < 6u; ++layer) {
        for (uint32_t y = 0u; y < 8u; ++y)
            for (uint32_t x = 0u; x < 8u; ++x) rows[y][x] = 0u;
        set_tile(layer, 0u, rows); /* transparent for every layer */
    }

    /* Cloud tiles (NBG3 = layer index 3, palette 2). */
    for (uint32_t tile = 1u; tile <= 3u; ++tile) {
        for (uint32_t y = 0u; y < 8u; ++y) {
            for (uint32_t x = 0u; x < 8u; ++x) {
                const int dx = (int)x - 3, dy = (int)y - 3;
                const int d = dx * dx + dy * dy;
                uint8_t c = 0u;
                if (tile == 1u && d < 11) c = 1u;
                if (tile == 2u && d < 7) c = 2u;
                if (tile == 3u && d < 4) c = 1u;
                rows[y][x] = c;
            }
        }
        set_tile(3u, tile, rows);
    }

    /* Mountain tiles (NBG2 = layer 2, palette 3). Tile 1 slope, 2 crest,
     * 3 shadow. The bottom is opaque, the top half tapered. */
    for (uint32_t tile = 1u; tile <= 3u; ++tile) {
        for (uint32_t y = 0u; y < 8u; ++y) {
            for (uint32_t x = 0u; x < 8u; ++x) {
                const uint32_t edge = (tile == 2u) ? 1u : 2u;
                rows[y][x] = (x >= edge + (7u - y) / 3u && x < 8u - edge - (7u - y) / 3u)
                    ? (uint8_t)(tile == 3u ? 2u : 1u) : 0u;
                if (y == 7u) rows[y][x] = (uint8_t)(tile == 3u ? 2u : 1u);
            }
        }
        set_tile(2u, tile, rows);
    }

    /* Sea tiles (RBG0 = layer 4, palette 4). Tile 1 deep water, 2 wave
     * crest, 3 glint, 4 foam. */
    for (uint32_t tile = 1u; tile <= 4u; ++tile) {
        for (uint32_t y = 0u; y < 8u; ++y) {
            for (uint32_t x = 0u; x < 8u; ++x) {
                uint8_t c = 1u;
                if (tile == 2u) c = ((x + y) & 3u) == 0u ? 2u : 1u;
                else if (tile == 3u) c = (x == y) ? 3u : 1u;
                else if (tile == 4u) c = (y < 2u) ? 2u : 1u;
                rows[y][x] = c;
            }
        }
        set_tile(4u, tile, rows);
    }

    /* Shore tiles (NBG1 = layer 1, palette 5): 1 sand, 2 foam, 3 grass,
     * 4 tree canopy, 5 trunk. */
    for (uint32_t tile = 1u; tile <= 5u; ++tile) {
        for (uint32_t y = 0u; y < 8u; ++y) {
            for (uint32_t x = 0u; x < 8u; ++x) {
                uint8_t c = 1u;
                if (tile == 2u) c = (y < 3u) ? 2u : 1u;
                else if (tile == 3u) c = ((x + y * 3u) & 7u) < 5u ? 3u : 1u;
                else if (tile == 4u) c = (((x - 4) * (x - 4) + (y - 3) * (y - 3)) < 12) ? 4u : 0u;
                else if (tile == 5u) c = (x >= 3u && x <= 4u) ? 5u : 0u;
                rows[y][x] = c;
            }
        }
        set_tile(1u, tile, rows);
    }

    /* Leaf tiles (NBG0 = layer 0, palette 6): 1 dark leaf, 2 bright leaf,
     * 3 stem. */
    for (uint32_t tile = 1u; tile <= 3u; ++tile) {
        for (uint32_t y = 0u; y < 8u; ++y) {
            for (uint32_t x = 0u; x < 8u; ++x) {
                uint8_t c = 0u;
                if (tile == 3u) c = (x == 4u || (y == 4u && x > 2u)) ? 1u : 0u;
                else if (((x - 4) * (x - 4) * 2 + (y - 4) * (y - 4)) < (tile == 1u ? 18 : 11)) {
                    c = (uint8_t)tile;
                }
                rows[y][x] = c;
            }
        }
        set_tile(0u, tile, rows);
    }
}

static void put_cell(uint16_t* map, uint32_t x, uint32_t y, uint32_t layer,
                     uint32_t tile, uint32_t palette) {
    if (x >= CELLS || y >= CELLS) return;
    map[y * CELLS + x] = (uint16_t)((palette << 12u) | ((layer * 8u + tile) & 0x3FFu));
}

static void build_maps(void) {
    for (uint32_t i = 0u; i < 4u; ++i) {
        for (uint32_t c = 0u; c < MAP_WORDS; ++c) g_maps[i][c] = 0u;
    }
    for (uint32_t c = 0u; c < MAP_WORDS; ++c) g_rbg_map[c] = 0u;

    /* NBG3 clouds: a few clusters in the upper half. */
    g_seed = 0x13579BDFu;
    for (uint32_t i = 0u; i < 18u; ++i) {
        const uint32_t cx = rnd() % CELLS;
        const uint32_t cy = 1u + rnd() % 13u;
        for (uint32_t dy = 0u; dy < 3u; ++dy) {
            for (uint32_t dx = 0u; dx < 4u; ++dx) {
                put_cell(g_maps[3], (cx + dx) & 63u, (cy + dy) & 63u, 3u,
                         (dx == 0u || dy == 0u) ? 3u : 1u, PAL_CLOUD);
            }
        }
    }

    /* NBG2 mountains: a silhouette from row 10 down to the horizon. */
    for (uint32_t x = 0u; x < CELLS; ++x) {
        const uint32_t h = 6u + ((x * 7u + (x / 5u) * 3u) % 7u);
        const uint32_t top = 28u - h;
        for (uint32_t y = top; y < 28u; ++y) {
            if (y >= CELLS) break;
            const uint32_t tile = (y == top) ? 2u : ((y + x) % 11u == 0u ? 3u : 1u);
            put_cell(g_maps[2], x, y, 2u, tile, PAL_MOUNTAIN);
        }
    }

    /* NBG1 shore: transparent above row 20, sand/foam/grass/trees below.
     * The top edge wiggles so the line scroll reads as surf. */
    for (uint32_t x = 0u; x < CELLS; ++x) {
        const uint32_t edge = SHORE_ROW + ((x * 5u + (x / 6u) * 2u) % 3u);
        for (uint32_t y = edge; y < CELLS; ++y) {
            uint32_t tile = 3u;
            if (y == edge) tile = 2u;
            else if (y == edge + 1u) tile = 1u;
            else if ((x + y) % 7u == 0u) tile = 4u;
            put_cell(g_maps[1], x, y, 1u, tile, PAL_SHORE);
        }
    }
    /* Tree line behind the shore's grass. */
    for (uint32_t x = 0u; x < CELLS; x += 3u) {
        put_cell(g_maps[1], x, SHORE_ROW - 1u, 1u, 5u, PAL_SHORE);
        put_cell(g_maps[1], x, SHORE_ROW - 2u, 1u, 4u, PAL_SHORE);
    }

    /* NBG0 foreground leaves: top corners and the bottom edge. */
    for (uint32_t y = 0u; y < 6u; ++y) {
        for (uint32_t x = 0u; x < 10u; ++x) {
            if ((x + y) % 3u != 0u) put_cell(g_maps[0], x, y, 0u, (x + y) & 1u ? 1u : 2u, PAL_LEAF);
            if ((x + y) % 3u != 0u) put_cell(g_maps[0], 63u - x, y, 0u, (x + y) & 1u ? 2u : 1u, PAL_LEAF);
        }
    }
    for (uint32_t x = 0u; x < CELLS; ++x) {
        for (uint32_t y = 25u; y < CELLS; ++y) {
            if (((x * 3u + y) % 5u) != 0u) put_cell(g_maps[0], x, y, 0u, (x + y) & 1u ? 1u : 2u, PAL_LEAF);
        }
    }

    /* RBG0 sea: transparent above the horizon, wave tiles below. The whole
     * 512x512-dot map repeats outside its plane (screen over = repeat). */
    for (uint32_t y = HORIZON_ROW; y < CELLS; ++y) {
        for (uint32_t x = 0u; x < CELLS; ++x) {
            const uint32_t tile = ((x * 3u + y * 5u) % 13u == 0u) ? 4u
                                : ((x + y) % 5u == 0u ? 2u : ((x * 7u + y) % 17u == 0u ? 3u : 1u));
            put_cell(g_rbg_map, x, y, 4u, tile, PAL_SEA);
        }
    }
}

static void upload_palettes(void) {
    uint16_t palette[16];
    /* One 16-colour block per screen: 2 clouds, 3 mountains, 4 sea,
     * 5 shore, 6 leaves. */
    static const uint16_t colors[5][6] = {
        {0u, SAT_RGB555(22, 24, 28), SAT_RGB555(29, 30, 31), SAT_RGB555(16, 18, 24), 0u, 0u},
        {0u, SAT_RGB555(9, 11, 18), SAT_RGB555(21, 23, 28), SAT_RGB555(5, 6, 11), 0u, 0u},
        {0u, SAT_RGB555(2, 6, 18), SAT_RGB555(6, 16, 28), SAT_RGB555(16, 27, 30), SAT_RGB555(28, 26, 18), 0u},
        {0u, SAT_RGB555(26, 22, 8), SAT_RGB555(31, 31, 31), SAT_RGB555(5, 20, 7), SAT_RGB555(3, 12, 5), SAT_RGB555(15, 9, 3)},
        {0u, SAT_RGB555(2, 8, 2), SAT_RGB555(8, 20, 5), SAT_RGB555(4, 12, 4), 0u, 0u},
    };
    for (uint32_t p = 0u; p < 5u; ++p) {
        for (uint32_t i = 0u; i < 16u; ++i) palette[i] = colors[p][i < 6u ? i : 0u];
        (void)sat_vdp2_palette_upload(palette, 16u, (uint16_t)((p + 2u) * 16u));
    }
    for (uint32_t i = 0u; i < 32u; ++i) {
        g_line_palette[i] = SAT_RGB555(6u + i / 2u, 3u + i / 4u, 1u + i / 8u);
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

static sat_result_t configure_sea(void) {
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
    r.over = SAT_VDP2_ROTATION_OVER_REPEAT;
    r.name_bank_mask = SAT_VDP2_BANK_A0;
    r.char_bank_mask = SAT_VDP2_BANK_A1;
    r.parameter_mode = SAT_VDP2_ROTATION_PARAMETER_A;
    for (uint32_t p = 0u; p < 16u; ++p) r.plane_address[p] = RBG_NAMES;
    r.char_base_address = RBG_CHARS;
    r.table_address = RBG_TABLE;
    return sat_vdp2_rotation_configure(&r);
}

static void build_back_and_tables(void) {
    for (uint32_t y = 0u; y < SCREEN_H; ++y) {
        uint16_t c;
        if (y < 112u) {
            const uint32_t t = (y * 31u) / 112u;
            c = SAT_RGB555(2u + t / 5u, 4u + t / 3u, 10u + t / 2u);
        } else {
            c = SAT_RGB555(3, 7, 16);
        }
        g_back[y] = c;
        const int32_t d = (int32_t)y - 108;
        const uint32_t level = (uint32_t)(d < 0 ? -d : d);
        g_line_indices[y] = (uint16_t)(LINE_COLOR_BASE + (level >= 32u ? 0u : (31u - level)));
    }
}

static uint16_t reference_color(uint32_t index) {
    if (index < 32u) {
        /* 1..31: dawn sky, dark blue to warm horizon. */
        const uint32_t t = index;
        return SAT_RGB555(1u + t / 3u, 3u + t / 2u, 9u + (31u - t) / 3u);
    }
    if (index < 36u) return SAT_RGB555(22u + (index - 32u) * 2u, 24u + (index - 32u) * 2u,
                                       26u + (index - 32u));
    if (index < 48u) return SAT_RGB555(28u + (index - 36u) / 2u, 20u + (index - 36u) / 2u,
                                       3u + (index - 36u) / 4u);
    if (index < 64u) return SAT_RGB555(8u + (index - 48u) / 2u, 10u + (index - 48u) / 2u,
                                       16u + (index - 48u) / 3u);
    if (index < 96u) {
        const uint32_t t = index - 64u;
        return SAT_RGB555(2u + t / 6u, 6u + t / 3u, 18u + t / 2u);
    }
    if (index < 128u) {
        const uint32_t t = index - 96u;
        return SAT_RGB555(20u + t / 4u, 18u + t / 5u, 5u + t / 10u);
    }
    {
        const uint32_t t = index - 128u;
        return SAT_RGB555(2u + t / 8u, 8u + t / 3u, 2u + t / 10u);
    }
}

static void build_reference_palette(void) {
    for (uint32_t i = 0u; i < 256u; ++i) g_reference_palette[i] = reference_color(i);
    g_reference_palette[0] = SAT_RGB555(2, 4, 12);
}

static void reference_frame(uint32_t phase, int32_t sun_x) {
    /* One comparable frame in software: sky gradient, cloud bands, a sun
     * disc, mountain silhouettes, a wave-banded sea, shore and leaf
     * silhouettes. It is only used by the reference path and by the startup
     * timing, not as an animated substitute for the VDP2 scene. */
    for (uint32_t y = 0u; y < SCREEN_H; ++y) {
        for (uint32_t x = 0u; x < SCREEN_W; ++x) {
            uint8_t c;
            if (y < 112u) {
                c = (uint8_t)(1u + (y * 30u) / 112u);
                if (((x + phase / 4u) / 48u + y / 16u) % 5u == 0u && y < 60u) {
                    c = (uint8_t)(32u + (x / 8u) % 3u);
                }
                const int32_t dx = (int32_t)x - sun_x;
                const int32_t dy = (int32_t)y - 80;
                if (dx * dx + dy * dy < 900) c = (uint8_t)(42u + (dx + 30) / 10u);
            } else if (y < 160u) {
                c = (uint8_t)(64u + ((x + phase) / 16u + y / 3u) % 3u);
                const int32_t dx = (int32_t)x - sun_x;
                if (dx > -20 && dx < 20 && y < 124u) c = (uint8_t)(80u + (dx + 20) / 5u);
                if (y > 128u && ((x + phase) / 24u + y / 4u) % 7u == 0u) c = 72u;
            } else if (y < 176u) {
                c = (uint8_t)(96u + ((x + phase / 2u) / 8u) % 16u);
            } else {
                c = (uint8_t)(112u + ((x + phase / 2u) / 8u + (y - 176u) / 4u) % 12u);
                if (y > 200u && ((x / 8u + y / 8u) & 1u) == 0u) c = (uint8_t)(128u + (x / 8u) % 16u);
            }
            g_reference_pixels[y * SCREEN_W + x] = c;
        }
    }
}

static uint32_t ticks_us(uint64_t ticks) {
    const uint32_t hz = sat_time_frequency_hz();
    if (hz == 0u) return 0u;
    return (uint32_t)((ticks * 1000000u) / hz);
}

static void build_reference(void) {
    uint64_t start = sat_time_ticks();
    build_reference_palette();
    reference_frame(0u, 220);
    g_parallax.software_fill_us = ticks_us(sat_time_ticks() - start);
    start = sat_time_ticks();
    if (sat_tex_upload_indexed8(&g_reference_texture, g_reference_pixels, SCREEN_W, SCREEN_H,
                                g_reference_palette, PAL_REFERENCE) == SAT_OK) {
        g_parallax.reference_ready = 1u;
    }
    g_parallax.software_upload_us = ticks_us(sat_time_ticks() - start);
}

static sat_result_t setup_scene(void) {
    sat_result_t st = SAT_OK;
    g_parallax.layers_ok =
        configure_nbg(SAT_VDP2_NBG0, NAMES0, 6u) == SAT_OK &&
        configure_nbg(SAT_VDP2_NBG1, NAMES1, 4u) == SAT_OK &&
        configure_nbg(SAT_VDP2_NBG2, NAMES2, 2u) == SAT_OK &&
        configure_nbg(SAT_VDP2_NBG3, NAMES3, 1u) == SAT_OK;
    if (!g_parallax.layers_ok) st = SAT_ERR_CAPACITY;

    g_parallax.rotation_ok = configure_sea() == SAT_OK;
    if (!g_parallax.rotation_ok) st = SAT_ERR_CAPACITY;

    /* Line scroll on NBG0 and NBG1. */
    {
        sat_vdp2_line_scroll_config_t ls = {0};
        ls.layer = SAT_VDP2_NBG0;
        ls.horizontal = 1u;
        /* One entry per 8 lines keeps the per-frame table write small while
         * the wave still moves; the manual supports this interval. */
        ls.interval = 3u;
        ls.table_address = SCROLL0_TABLE;
        sat_result_t s0 = sat_vdp2_layer_line_scroll_enable(&ls);
        ls.layer = SAT_VDP2_NBG1;
        ls.table_address = SCROLL1_TABLE;
        sat_result_t s1 = sat_vdp2_layer_line_scroll_enable(&ls);
        if (s0 != SAT_OK || s1 != SAT_OK) st = SAT_ERR_CAPACITY;
        ls.layer = SAT_VDP2_NBG2;
        g_parallax.refused_scroll = sat_vdp2_layer_line_scroll_enable(&ls) == SAT_ERR_INVALID_ARG;
    }

    g_parallax.back_ok =
        sat_vdp2_back_screen_set_lines(BACK_TABLE, g_back, SCREEN_H) == SAT_OK;
    g_parallax.line_color_ok =
        sat_vdp2_line_color_screen_set(LINE_COLOR_TABLE, g_line_indices, SCREEN_H) == SAT_OK &&
        sat_vdp2_layer_set_line_color_insert(SAT_VDP2_NBG1, 1u) == SAT_OK;
    if (!g_parallax.back_ok || !g_parallax.line_color_ok) st = SAT_ERR_CAPACITY;

    /* W0 clips NBG3 above the horizon. */
    {
        const sat_vdp2_window_rect_t sky = {0u, 0u, SCREEN_W - 1u, 127u};
        sat_vdp2_screen_window_t sw;
        sat_vdp2_screen_window_default(SAT_VDP2_SCREEN_NBG3, &sw);
        sw.w0 = SAT_VDP2_WINDOW_AREA_INSIDE;
        g_parallax.window_ok =
            sat_vdp2_window_set_rect(0u, &sky) == SAT_OK &&
            sat_vdp2_screen_window_set(&sw) == SAT_OK;
        if (!g_parallax.window_ok) st = SAT_ERR_CAPACITY;
        /* A screen window that has no defined window is refused. */
        sat_vdp2_screen_window_t bad;
        sat_vdp2_screen_window_default(SAT_VDP2_SCREEN_NBG2, &bad);
        bad.w1 = SAT_VDP2_WINDOW_AREA_INSIDE;
        g_parallax.refused_window = sat_vdp2_screen_window_set(&bad) == SAT_ERR_INVALID_ARG;
    }

    /* Add-mode colour calculation: the sea picks up the sky below, the shore
     * picks up the warm line-colour screen. */
    if (sat_vdp2_color_calc_set_add(1u) != SAT_OK ||
        sat_vdp2_screen_color_calc_set(SAT_VDP2_SCREEN_RBG0, 1u, 0u) != SAT_OK ||
        sat_vdp2_screen_color_calc_set(SAT_VDP2_SCREEN_NBG1, 1u, 0u) != SAT_OK) {
        st = SAT_ERR_UNSUPPORTED;
    }
    return st;
}

static void hide_vdp2_background(uint8_t hidden) {
    for (uint8_t layer = 0u; layer < 4u; ++layer) {
        (void)sat_vdp2_layer_set_enabled((sat_vdp2_layer_t)layer, hidden ? 0u : 1u);
    }
    (void)sat_vdp2_rotation_set_enabled(hidden ? 0u : 1u);
}

static void upload_scene(void) {
    for (uint32_t i = 0u; i < 4u; ++i) {
        (void)sat_vdp2_vram_write_words((NAMES0 + i * 0x2000u) / 2u, g_maps[i], MAP_WORDS);
    }
    (void)sat_vdp2_vram_write_words(RBG_NAMES / 2u, g_rbg_map, MAP_WORDS);
    (void)sat_vdp2_vram_write_words(CHARS / 2u, g_tiles, 8u * 6u * 16u);
    (void)sat_vdp2_vram_write_words(RBG_CHARS / 2u, &g_tiles[4u * 8u * 16u], 8u * 16u);
}

static void rotation_params(uint32_t scroll, sat_vdp2_rotation_params_t* out) {
    *out = (sat_vdp2_rotation_params_t){0};
    out->xst = (int32_t)(scroll << 16u);
    out->yst = 0;
    out->zst = 0;
    out->dxst = 0;
    out->dyst = SAT_FX16_ONE;
    out->dx = SAT_FX16_ONE;
    out->dy = 0;
    out->matrix[0] = SAT_FX16_ONE;
    out->matrix[1] = 0;
    out->matrix[2] = 0;
    out->matrix[3] = 0;
    out->matrix[4] = SAT_FX16_ONE;
    out->matrix[5] = 0;
    out->kx = SAT_FX16_ONE;
    out->ky = SAT_FX16_ONE;
    out->coefficient_address = 0u;
    out->coefficient_step = SAT_FX16_ONE;
}

static void update_scene(uint32_t frame) {
    const uint64_t start = sat_time_ticks();
    const int32_t phase = (int32_t)((frame * 3u) & 0xFFu);
    const int32_t sun_x = 90 + (int32_t)((frame * 2u) % 140u);
    const uint32_t sea_scroll = (frame * 2u) & 511u;
    sat_vdp2_rotation_params_t pa, pb;
    rotation_params(sea_scroll, &pa);
    rotation_params(sea_scroll, &pb);
    (void)sat_vdp2_rotation_write_params(RBG_TABLE, &pa, &pb);
    (void)sat_vdp2_line_scroll_fill_wave(SAT_VDP2_NBG1, SCREEN_H, 5 << 16, 44u, phase << 16);
    (void)sat_vdp2_line_scroll_fill_wave(SAT_VDP2_NBG0, SCREEN_H, 2 << 16, 70u, (phase / 2) << 16);
    /* Scroll arguments are 16.16; the parallax speeds are pixels per field. */
    (void)sat_vdp2_layer_set_scroll(SAT_VDP2_NBG3, (int32_t)(frame * 0x8000u), 0);
    (void)sat_vdp2_layer_set_scroll(SAT_VDP2_NBG2, (int32_t)(frame * 0x4000u), 0);
    (void)sat_vdp2_layer_set_scroll(SAT_VDP2_NBG1, (int32_t)(frame << 16u), 0);
    (void)sat_vdp2_layer_set_scroll(SAT_VDP2_NBG0, (int32_t)((frame * 2u) << 16u), 0);
    /* The back gradient and line-colour tables never change after setup;
     * rewriting them every frame would be pure overhead. */
    g_parallax.sea_scroll = sea_scroll;
    g_parallax.sun_x = (uint32_t)sun_x;
    const uint32_t us = ticks_us(sat_time_ticks() - start);
    g_parallax.update_us_last = us;
    if (us > g_parallax.update_us_max) g_parallax.update_us_max = us;
}

static void draw_reference_background(void) {
    if (g_parallax.reference_ready == 0u) return;
    (void)sat_draw_sprite_screen(&g_reference_texture, SCREEN_W / 2, SCREEN_H / 2,
                                 SCREEN_W, SCREEN_H, PAL_REFERENCE);
}

static void draw_hud(uint32_t commands) {
    (void)sat_draw_rect_screen(0, 0, 190, 56, SAT_RGB555(0, 0, 0));
    (void)sat_ascii_font_draw_text_screen_indexed8(&g_font, "VDP2 PARALLAX COAST", 8, 8, 8, 0u, 0u);
    (void)sat_ascii_font_draw_label_u32(&g_font, "FRAME ", g_parallax.frames, 8, 24, 8, 0u, 0u);
    (void)sat_ascii_font_draw_label_u32(&g_font, "CMDS ", commands, 104, 24, 8, 0u, 0u);
    (void)sat_ascii_font_draw_text_screen_indexed8(
        &g_font, g_parallax.reference ? "VDP1 REF" : "VDP2 LIVE", 8, 40, 8, 0u, 0u);
    (void)sat_ascii_font_draw_label_u32(&g_font, "BG ", g_parallax.bg_commands_vdp2, 104, 40, 8, 0u, 0u);
    (void)sat_ascii_font_draw_text_screen_indexed8(&g_font, "A/R REF  START",
        8, SCREEN_H - 16, 8, 0u, 0u);
}

int main(void) {
    sat_video_config_t video = {SCREEN_W, SCREEN_H, SAT_VIDEO_AUTO, 0u};
    if (sat_init(&video) != SAT_OK) return 1;
    g_parallax.magic = PARALLAX_MAGIC;
    if (sat_ascii_font_init_8x8_indexed8(&g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, FONT_BANK) != SAT_OK) {
        return 1;
    }
    make_tiles();
    build_maps();
    upload_palettes();
    build_back_and_tables();
    build_reference();
    upload_scene();
    g_parallax.init_ok =
        (setup_scene() == SAT_OK && g_parallax.layers_ok != 0u &&
         g_parallax.rotation_ok != 0u && g_parallax.refused_scroll != 0u &&
         g_parallax.refused_window != 0u && g_parallax.reference_ready != 0u) ? 1u : 0u;
    g_parallax.bg_commands_vdp2 = 0u;
    g_parallax.bg_commands_reference = 1u;
    (void)sat_vdp2_sprite_set_priority(7u);

    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_wait_vblank() != SAT_OK) break;
        ++g_parallax.frames;
        if (g_parallax.frames == REF_AUTO_ON || g_parallax.frames == REF_AUTO_OFF) {
            g_parallax.reference ^= 1u;
            if (g_parallax.reference) g_parallax.reference_seen = 1u;
            hide_vdp2_background(g_parallax.reference);
        }
        update_scene(g_parallax.frames);
        (void)sat_vdp2_layers_commit();
        (void)sat_vdp1_set_erase_transparent();
        if (sat_begin_frame() != SAT_OK) break;
        (void)sat_pad_poll(&pad);
        if ((pad.pressed & (SAT_PAD_A | SAT_PAD_R)) != 0u) {
            g_parallax.reference ^= 1u;
            if (g_parallax.reference) g_parallax.reference_seen = 1u;
            hide_vdp2_background(g_parallax.reference);
        }
        if (g_parallax.reference) draw_reference_background();
        sat_vdp1_command_stats_t commands = {0};
        (void)sat_vdp1_command_stats(&commands);
        if (g_parallax.reference) g_parallax.commands_reference = commands.used;
        else g_parallax.commands_vdp2 = commands.used;
        draw_hud(commands.used);
        (void)sat_end_frame();
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }
    (void)sat_shutdown();
    return 0;
}
