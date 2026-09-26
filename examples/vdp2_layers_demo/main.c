/* Four VDP2 scroll screens at once (NBG0-NBG3), each in its own VRAM banks,
 * with the VRAM cycle pattern planned by the library. Every layer scrolls at
 * its own speed and is drawn behind the VDP1 text.
 *
 * Layer  names   characters  look
 *   NBG0  A0      B0          vertical yellow bars, in front
 *   NBG1  A0      B0          horizontal red bars
 *   NBG2  A1      B1          green checkerboard
 *   NBG3  A1      B1          blue grid, at the back
 * The results the harness checks are in g_layers_demo. */
#include <stdint.h>
#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/saturn.h"
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
} layers_demo_results_t;

volatile layers_demo_results_t g_layers_demo;

static sat_ascii_font_t font;
static uint16_t g_map[64u * 64u];
static uint16_t g_tiles[6u * 16u];      /* six 8x8 4-bit cells, 16 words each */

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
            sx[i] += (int32_t)(i + 1u) * 0x8000;
            (void)sat_vdp2_layer_set_scroll((sat_vdp2_layer_t)i, sx[i], sx[i] / 2);
            g_layers_demo.scroll_x[i] = sx[i];
        }
        ++g_layers_demo.frames;
        (void)sat_ascii_font_draw_text_screen_indexed8(&font, "VDP2 NBG0-3", 8, 8, 8, 0u, 0u);
        line("FRAME ", g_layers_demo.frames, 24);
        line("A0L ", g_layers_demo.cycle[0], 40);
        line("B1L ", g_layers_demo.cycle[6], 56);
        (void)sat_end_frame();
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }
    (void)sat_shutdown();
    return 0;
}
