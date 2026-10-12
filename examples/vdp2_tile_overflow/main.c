/* More than 512 distinct 8bpp tiles on one scroll screen, with two more 256-colour layers around it.
 *
 * A VDP2 layer's 1-word pattern names carry a 10-bit character number, and an 8bpp 8x8 cell takes two
 * character numbers (64 bytes, characters count in 32-byte units): a layer's 32 KiB character window
 * holds 512 tiles. A GBA 256-colour map can name tiles up to 1023; the SA1 title screen uses 525, and
 * the numbers past 511 used to wrap onto tiles 0-12.
 *
 * The cure used here, and by the sa2-saturn port (sa2_video_vdp2.c, OV_FIRST): the next NBG up reads
 * the window that follows the first one, so both layers see one run of character data.
 *   NBG0  names A0 0x00000  characters B0 0x40000: tiles 0-510, cell 511 zeroed (blank)
 *   NBG1  names A0 0x02000  characters B0 0x48000: tiles 511-1021 (cell 0 = tile 511), cell 511 blank
 *   NBG2  names A1 0x20000  characters B1 0x60000: a checkerboard behind both
 * A map cell shows in the layer that owns its tile and is the blank character in the other. All three
 * layers are 256-colour, which needs one name read plus two character reads each per cycle: the
 * results the harness checks (the library's verdicts and the cycle pattern) are in g_overflow_demo, and
 * the picture is a smooth colour ramp over the 960 tiles, so a wrapped tile is a visible jump.
 * All three layers are scrolled by (-3, -4): the picture starts 3 dots right and 4 lines down, the same
 * offset the sa2-saturn port gives a BG whose GBA reference point is negative, and the harness checks
 * that NBG0/NBG1 (which keep fractional scroll bits) and NBG2 (integer only) put the same line first.
 * See docs/sega_saturn_hardware/hard/vdp2/hon/p03_31.md (access counts) and p03_34.md. */
#include <stdint.h>
#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/saturn.h"
#include "saturn/vdp2_layers.h"

#define OVERFLOW_DEMO_MAGIC 0x4F564631u /* "OVF1" */
#define MAP_W 40u
#define MAP_H 24u
#define TILES (MAP_W * MAP_H)           /* 960, all distinct */
#define OV_FIRST 511u
#define BLANK_CHAR 1022u                /* the last 8bpp cell of a window */

typedef struct overflow_demo_results {
    uint32_t magic;
    uint32_t configure[3];   /* sat_result_t of each layer's configure */
    uint32_t enable[3];      /* sat_result_t of showing each layer */
    uint32_t cycle[8];       /* A0L A0U A1L A1U B0L B0U B1L B1U */
    uint32_t tiles;          /* distinct tiles on the map */
    uint32_t tiles_past_511; /* of them, shown by NBG1 */
    uint32_t frames;
} overflow_demo_results_t;

volatile overflow_demo_results_t g_overflow_demo;

static sat_ascii_font_t font;
static uint16_t g_names[2][64u * 64u];  /* NBG0, NBG1 */
static uint16_t g_check_names[64u * 64u];
static uint16_t g_cell[32u];            /* one 8bpp cell, two dots a word */
static uint16_t g_check_cells[2u * 32u];

/* The colour index of tile t: 1..254, a ramp over all the tiles. */
static uint32_t tile_index(uint32_t t) {
    return 1u + (t * 253u) / (TILES - 1u);
}

static void upload_palette(void) {
    uint16_t palette[256];
    for (uint32_t i = 0u; i < 256u; ++i) {
        palette[i] = SAT_RGB555((i >> 3u) & 31u, 31u - ((i >> 3u) & 31u), 16u);
    }
    palette[1] = SAT_RGB555(0, 0, 0);   /* the first tile is black, so a wrap onto it is obvious */
    palette[254] = SAT_RGB555(31, 31, 31);
    palette[255] = SAT_RGB555(6, 6, 10);
    (void)sat_vdp2_palette_upload(palette, 256u, 0u);
}

static sat_result_t write_cell(uint32_t chars, uint32_t cell, uint32_t index) {
    const uint16_t word = (uint16_t)((index << 8u) | index);
    for (uint32_t i = 0u; i < 32u; ++i) g_cell[i] = word;
    return sat_vdp2_vram_write_words((chars + cell * 64u) / 2u, g_cell, 32u);
}

static sat_result_t build_tiles(void) {
    sat_result_t st;
    for (uint32_t t = 0u; t < TILES; ++t) {
        const uint32_t layer = t < OV_FIRST ? 0u : 1u;
        const uint32_t chars = layer == 0u ? 0x40000u : 0x48000u;
        st = write_cell(chars, layer == 0u ? t : t - OV_FIRST, tile_index(t));
        if (st != SAT_OK) return st;
    }
    /* each window's blank cell: dot 0 is transparent */
    st = write_cell(0x40000u, 511u, 0u);
    if (st != SAT_OK) return st;
    return write_cell(0x48000u, 511u, 0u);
}

static sat_result_t build_names(void) {
    for (uint32_t y = 0u; y < 64u; ++y) {
        for (uint32_t x = 0u; x < 64u; ++x) {
            uint16_t a = (uint16_t)BLANK_CHAR, b = (uint16_t)BLANK_CHAR;
            if (x < MAP_W && y < MAP_H) {
                const uint32_t t = y * MAP_W + x;
                if (t < OV_FIRST) a = (uint16_t)(t << 1u);
                else b = (uint16_t)((t - OV_FIRST) << 1u);
            }
            g_names[0][y * 64u + x] = a;
            g_names[1][y * 64u + x] = b;
            g_check_names[y * 64u + x] = (uint16_t)(((x + y) & 1u) != 0u ? 2u : 0u);
        }
    }
    sat_result_t st = sat_vdp2_vram_write_words(0x00000u / 2u, g_names[0], 64u * 64u);
    if (st != SAT_OK) return st;
    st = sat_vdp2_vram_write_words(0x02000u / 2u, g_names[1], 64u * 64u);
    if (st != SAT_OK) return st;
    /* NBG2: character 0 is blank, 2 is a solid 8bpp cell of index 255 */
    for (uint32_t i = 0u; i < 32u; ++i) {
        g_check_cells[i] = 0u;
        g_check_cells[32u + i] = 0xFFFFu;
    }
    st = sat_vdp2_vram_write_words(0x60000u / 2u, g_check_cells, 64u);
    if (st != SAT_OK) return st;
    return sat_vdp2_vram_write_words(0x20000u / 2u, g_check_names, 64u * 64u);
}

static sat_result_t setup_layer(uint32_t layer, uint32_t names, uint32_t chars, uint8_t char_bank,
                                uint8_t priority) {
    sat_vdp2_layer_config_t c;
    sat_vdp2_layer_config_default((sat_vdp2_layer_t)layer, &c);
    c.color_mode = SAT_VDP2_COLOR_MODE_256;
    for (uint32_t p = 0u; p < 4u; ++p) c.plane_address[p] = names;
    c.char_base_address = chars;
    c.char_bank_mask = char_bank;
    c.priority = priority;
    c.transparent = 1u;
    return sat_vdp2_layer_configure(&c);
}

int main(void) {
    if (sat_app_init_default() != SAT_OK) return 1;
    if (sat_ascii_font_init_8x8_indexed8(&font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u) != SAT_OK) return 1;
    g_overflow_demo.magic = OVERFLOW_DEMO_MAGIC;
    g_overflow_demo.tiles = TILES;
    g_overflow_demo.tiles_past_511 = TILES - OV_FIRST;
    upload_palette();
    if (build_tiles() != SAT_OK || build_names() != SAT_OK) return 1;
    g_overflow_demo.configure[0] = (uint32_t)setup_layer(0u, 0x00000u, 0x40000u, SAT_VDP2_BANK_B0, 4u);
    g_overflow_demo.configure[1] = (uint32_t)setup_layer(1u, 0x02000u, 0x48000u, SAT_VDP2_BANK_B0, 4u);
    g_overflow_demo.configure[2] = (uint32_t)setup_layer(2u, 0x20000u, 0x60000u, SAT_VDP2_BANK_B1, 1u);
    for (uint32_t l = 0u; l < 3u; ++l) {
        g_overflow_demo.enable[l] = (uint32_t)sat_vdp2_layer_set_enabled((sat_vdp2_layer_t)l, 1u);
    }
    for (uint32_t l = 0u; l < 3u; ++l) {
        (void)sat_vdp2_layer_set_scroll((sat_vdp2_layer_t)l, -3 * SAT_FX16_ONE, -4 * SAT_FX16_ONE);
    }
    {
        uint16_t regs[8];
        (void)sat_vdp2_layer_cycle_patterns(regs);
        for (uint32_t i = 0u; i < 8u; ++i) g_overflow_demo.cycle[i] = regs[i];
    }
    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_wait_vblank() != SAT_OK) break;
        (void)sat_vdp2_layers_commit();
        (void)sat_vdp1_set_erase_transparent();
        if (sat_begin_frame() != SAT_OK) break;
        (void)sat_pad_poll(&pad);
        ++g_overflow_demo.frames;
        (void)sat_ascii_font_draw_text_screen_indexed8(&font, "VDP2 8BPP TILES 0-959", 8, 200, 8, 0u, 0u);
        (void)sat_ascii_font_draw_label_u32(&font, "FRAME ", g_overflow_demo.frames, 8, 212, 8, 0u, 0u);
        (void)sat_end_frame();
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }
    (void)sat_shutdown();
    return 0;
}
