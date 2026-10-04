/* Four VDP2 scroll screens at once (NBG0-NBG3), each in its own VRAM banks,
 * with the VRAM cycle pattern planned by the library. Every layer scrolls at
 * its own speed and is drawn behind the VDP1 text.
 *
 * Layer  names   characters  look
 *   NBG0  A0      B0          vertical yellow bars, in front
 *   NBG1  A0      B0          horizontal red bars
 *   NBG2  A1      B1          green checkerboard
 *   NBG3  A1      B1          blue grid, at the back
 * Two VDP1 squares use per-sprite priority (sprite type 1): the magenta one
 * is in front of every layer, the cyan one behind NBG0 and NBG1. The magenta
 * texture has a white top-left quarter and is drawn flipped both ways, so the
 * white quarter must show bottom-right.
 * The results the harness checks are in g_layers_demo. */
#include <stdint.h>
#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/saturn.h"
#include "saturn/vdp2_color_calc.h"
#include "saturn/vdp2_layers.h"

#define LAYERS_DEMO_MAGIC 0x4C594531u /* "LYE1" */

typedef struct layers_demo_results {
    uint32_t magic;
    uint32_t configure[4];        /* sat_result_t of each layer's configure */
    uint32_t cycle[8];            /* A0L A0U A1L A1U B0L B0U B1L B1U */
    uint32_t refused_too_much;    /* a fifth read-hungry layer was refused with CAPACITY */
    uint32_t refused_busy;        /* the old NBG0 API is refused while layers are managed */
    uint32_t frames;
    int32_t scroll_x[4];
    uint32_t palette_priorities;  /* sat_result_t of sat_vdp2_sprite_set_palette_priorities */
    uint32_t color_calc_busy;     /* sprite colour calc was refused with BUSY meanwhile */
} layers_demo_results_t;

volatile layers_demo_results_t g_layers_demo;

static sat_ascii_font_t font;
static uint16_t g_map[64u * 64u];
static uint16_t g_tiles[6u * 16u];      /* six 8x8 4-bit cells, 16 words each */

/* Per-sprite priority squares: 64x48, solid index 1 (magenta) or 2 (cyan) of bank 6. */
#define SQUARE_W 64u
#define SQUARE_H 48u
#define SQUARE_BANK 6u
static uint8_t g_square_pixels[2][SQUARE_W * SQUARE_H];
static sat_vdp1_texture_t g_square[2];
/* Selector 0 (the text, no override) is drawn at 7, selector 1 at 6 (in front
 * of NBG0 at 4), selector 2 at 2 (behind NBG0 and NBG1). */
static const uint8_t k_sprite_priorities[8] = {7u, 6u, 2u, 2u, 2u, 2u, 2u, 2u};

static sat_result_t setup_squares(void) {
    uint16_t palette[256];
    for (uint32_t i = 0u; i < 256u; ++i) palette[i] = 0u;
    palette[1] = SAT_RGB555(31, 0, 31);
    palette[2] = SAT_RGB555(0, 31, 31);
    palette[3] = SAT_RGB555(31, 31, 31);
    for (uint32_t k = 0u; k < 2u; ++k) {
        for (uint32_t i = 0u; i < SQUARE_W * SQUARE_H; ++i) {
            const uint32_t corner = (i % SQUARE_W) < SQUARE_W / 2u && (i / SQUARE_W) < SQUARE_H / 2u;
            g_square_pixels[k][i] = (uint8_t)(k == 0u && corner ? 3u : k + 1u);
        }
        sat_result_t st = sat_tex_upload_indexed8(&g_square[k], g_square_pixels[k], SQUARE_W, SQUARE_H,
                                                  palette, SQUARE_BANK);
        if (st != SAT_OK) return st;
    }
    return sat_vdp2_sprite_set_palette_priorities(k_sprite_priorities);
}

/* Cell k: 0 transparent, 1 solid colour 1, 2 solid colour 2, 3 checker,
 * 4 vertical stripe, 5 horizontal stripe. Four bits per dot, MSB pixel first. */
static void build_tiles(void) {
    for (uint32_t k = 0u; k < 6u; ++k) {
        for (uint32_t row = 0u; row < 8u; ++row) {
            uint32_t pixels[8];
            for (uint32_t x = 0u; x < 8u; ++x) {
                uint32_t v = 0u;
                switch (k) {
                    case 1u: v = 1u; break;
                    case 2u: v = 2u; break;
                    case 3u: v = (((x >> 2u) + (row >> 2u)) & 1u) != 0u ? 1u : 0u; break;
                    case 4u: v = x < 4u ? 1u : 0u; break;
                    case 5u: v = row < 4u ? 1u : 0u; break;
                    default: v = 0u; break;
                }
                pixels[x] = v;
            }
            g_tiles[k * 16u + row * 2u] =
                (uint16_t)((pixels[0] << 12u) | (pixels[1] << 8u) | (pixels[2] << 4u) | pixels[3]);
            g_tiles[k * 16u + row * 2u + 1u] =
                (uint16_t)((pixels[4] << 12u) | (pixels[5] << 8u) | (pixels[6] << 4u) | pixels[7]);
        }
    }
}

static uint16_t name(uint32_t char_number, uint32_t palette) {
    return (uint16_t)((palette << 12u) | (char_number & 0x3FFu));
}

static void fill_map(uint32_t layer) {
    for (uint32_t y = 0u; y < 64u; ++y) {
        for (uint32_t x = 0u; x < 64u; ++x) {
            uint32_t tile = 0u;
            switch (layer) {
                case 0u: tile = (x % 6u) == 0u ? 4u : 0u; break;              /* bars */
                case 1u: tile = (y % 5u) == 0u ? 5u : 0u; break;              /* bars */
                case 2u: tile = ((x + y) & 1u) != 0u ? 3u : 0u; break;        /* checker */
                default: tile = ((x + y) % 8u) == 0u ? 2u : 1u; break;        /* grid */
            }
            g_map[y * 64u + x] = name(tile, layer + 1u);
        }
    }
}

static void upload_palette(uint32_t layer) {
    static const uint16_t colours[4][3] = {
        {SAT_RGB555(31, 28, 3), SAT_RGB555(20, 18, 2), 0},      /* yellow */
        {SAT_RGB555(31, 6, 6), SAT_RGB555(20, 3, 3), 0},        /* red */
        {SAT_RGB555(6, 28, 8), SAT_RGB555(3, 20, 5), 0},        /* green */
        {SAT_RGB555(3, 6, 20), SAT_RGB555(8, 12, 30), 0},       /* blue, grid line brighter */
    };
    uint16_t palette[16];
    for (uint32_t i = 0u; i < 16u; ++i) palette[i] = 0u;
    palette[1] = colours[layer][0];
    palette[2] = colours[layer][1];
    (void)sat_vdp2_palette_upload(palette, 16u, (layer + 1u) * 16u);
}

static sat_result_t setup_layer(uint32_t layer, uint32_t names, uint32_t chars, uint8_t char_bank,
                                uint8_t priority) {
    sat_vdp2_layer_config_t c;
    sat_vdp2_layer_config_default((sat_vdp2_layer_t)layer, &c);
    for (uint32_t p = 0u; p < 4u; ++p) c.plane_address[p] = names;
    c.char_base_address = chars;
    c.char_bank_mask = char_bank;
    c.priority = priority;
    fill_map(layer);
    sat_result_t st = sat_vdp2_vram_write_words(names / 2u, g_map, 64u * 64u);
    if (st != SAT_OK) return st;
    st = sat_vdp2_vram_write_words(chars / 2u, g_tiles, 6u * 16u);
    if (st != SAT_OK) return st;
    upload_palette(layer);
    return sat_vdp2_layer_configure(&c);
}

static void line(const char* label, uint32_t value, int y) {
    (void)sat_ascii_font_draw_label_u32(&font, label, value, 8, y, 8, 0u, 0u);
}

int main(void) {
    int32_t sx[4] = {0, 0, 0, 0};
    if (sat_app_init_default() != SAT_OK) return 1;
    if (sat_ascii_font_init_8x8_indexed8(&font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 7u) != SAT_OK) {
        return 1;
    }
    build_tiles();
    g_layers_demo.magic = LAYERS_DEMO_MAGIC;
    g_layers_demo.configure[0] = (uint32_t)setup_layer(0u, 0x00000u, 0x40000u, SAT_VDP2_BANK_B0, 4u);
    g_layers_demo.configure[1] = (uint32_t)setup_layer(1u, 0x02000u, 0x48000u, SAT_VDP2_BANK_B0, 3u);
    g_layers_demo.configure[2] = (uint32_t)setup_layer(2u, 0x20000u, 0x60000u, SAT_VDP2_BANK_B1, 2u);
    g_layers_demo.configure[3] = (uint32_t)setup_layer(3u, 0x22000u, 0x68000u, SAT_VDP2_BANK_B1, 1u);

    /* A layer of 16.77 M colours needs eight reads per cycle: one more layer
     * of that kind cannot fit next to the four above. Nothing may change. */
    {
        sat_vdp2_layer_config_t big;
        uint16_t before[8], after[8];
        uint32_t same = 1u;
        (void)sat_vdp2_layer_cycle_patterns(before);
        sat_vdp2_layer_config_default(SAT_VDP2_NBG0, &big);
        big.color_mode = SAT_VDP2_COLOR_MODE_16770000;
        big.char_bank_mask = SAT_VDP2_BANK_B0;
        for (uint32_t p = 0u; p < 4u; ++p) big.plane_address[p] = 0x04000u;
        g_layers_demo.refused_too_much = sat_vdp2_layer_configure(&big) == SAT_ERR_CAPACITY;
        (void)sat_vdp2_layer_cycle_patterns(after);
        for (uint32_t i = 0u; i < 8u; ++i) same = same && before[i] == after[i];
        g_layers_demo.refused_too_much = g_layers_demo.refused_too_much && same;
    }
    {
        sat_vdp2_nbg0_config_t old = {SAT_VDP2_CHAR_SIZE_1X1, SAT_VDP2_COLOR_MODE_256, 0x3Bu, 1u, 0u};
        g_layers_demo.refused_busy = sat_vdp2_nbg0_init(&old) == SAT_ERR_BUSY;
    }
    g_layers_demo.palette_priorities = (uint32_t)setup_squares();
    {
        sat_vdp2_sprite_color_calc_config_t cc = {1u, 7u, {0}};
        g_layers_demo.color_calc_busy = sat_vdp2_sprite_color_calc_configure(&cc) == SAT_ERR_BUSY;
    }
    {
        uint16_t regs[8];
        (void)sat_vdp2_layer_cycle_patterns(regs);
        for (uint32_t i = 0u; i < 8u; ++i) g_layers_demo.cycle[i] = regs[i];
    }

    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_wait_vblank() != SAT_OK) break;
        (void)sat_vdp2_layers_commit();
        (void)sat_vdp1_set_erase_transparent();
        if (sat_begin_frame() != SAT_OK) break;
        (void)sat_pad_poll(&pad);
        for (uint32_t i = 0u; i < 4u; ++i) {
            if (i == 2u) continue; /* NBG2 stays at (0, 0), see below */
            sx[i] += (int32_t)(i + 1u) * 0x8000;
            (void)sat_vdp2_layer_set_scroll((sat_vdp2_layer_t)i, sx[i], sx[i] / 2);
            g_layers_demo.scroll_x[i] = sx[i];
        }
        ++g_layers_demo.frames;
        (void)sat_ascii_font_draw_text_screen_indexed8(&font, "VDP2 NBG0-3", 8, 8, 8, 0u, 0u);
        line("FRAME ", g_layers_demo.frames, 24);
        line("A0L ", g_layers_demo.cycle[0], 40);
        line("B1L ", g_layers_demo.cycle[6], 56);
        {
            /* Top-left (128, 96) in native coordinates (the origin is the screen centre). */
            sat_sprite_cmd_t front = {(128 - 160) * SAT_FX16_ONE, (96 - 112) * SAT_FX16_ONE, SQUARE_W, SQUARE_H,
                                      &g_square[0], (1u << 5u) | SQUARE_BANK,
                                      SAT_SPRITE_FLAG_FLIP_H | SAT_SPRITE_FLAG_FLIP_V};
            (void)sat_draw_sprite(&front);
        }
        /* Centred on (240, 120). */
        (void)sat_draw_sprite_screen(&g_square[1], 240, 120, SQUARE_W, SQUARE_H, (2u << 5u) | SQUARE_BANK);
        (void)sat_end_frame();
        /* Change NBG2's scroll in the middle of the picture and set it back before the picture ends:
         * a scroll write there applies from that scanline (a write to SCYN2 even restarts the
         * layer's line counter), so the library must hold it until VBlank, where only the final
         * (0, 0) is written. The harness checks that the checkerboard is drawn at (0, 0) on every
         * line, including the band 101-140 that the transient (4, 0) would have shifted. */
        {
            /* Reading EXTEN latches VCNT (EXLTEN = 0); outside double-density interlace it holds the
             * line number itself (manual table 2.4). */
            volatile uint16_t* exten = (volatile uint16_t*)0x25F80002u;
            volatile uint16_t* vcnt = (volatile uint16_t*)0x25F8000Au;
            uint32_t spin = 0u;
            while (((void)*exten, *vcnt & 0x1FFu) >= 101u && ++spin < 1000000u) { /* VBlank: let the picture start */
            }
            while (((void)*exten, *vcnt & 0x1FFu) < 101u && ++spin < 2000000u) {
            }
            (void)sat_vdp2_layer_set_scroll(SAT_VDP2_NBG2, 4 << 16, 0);
            while (((void)*exten, *vcnt & 0x1FFu) < 140u && ++spin < 3000000u) {
            }
            (void)sat_vdp2_layer_set_scroll(SAT_VDP2_NBG2, 0, 0);
        }
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }
    (void)sat_shutdown();
    return 0;
}
