#include "composition.h"
#include "example_util.h"
#include "saturn/color.h"
#include "saturn/dma.h"
#include "saturn/vdp2_alloc.h"
#include "saturn/vdp2_compose.h"
#include "saturn/vdp2_layers.h"

enum {
    STRIDE = 512, ROWS = 112, WIDTH = 160,
    INDEX_BYTES = 0x20000, RGB_BYTES = 0x40000,
    BACKDROP = 0x7fffe
};

#define VRAM ((volatile uint16_t*)(uintptr_t)0x25e00000u)

static uint8_t rgb_to_index[65536];
static sat_vdp2_palette_range_t palette;

static void configure(uint8_t mode, uint32_t offset, uint8_t color_bank) {
    sat_vdp2_layer_config_t layer;
    sat_vdp2_layer_config_default(SAT_VDP2_NBG0, &layer);
    layer.bitmap = 1;
    layer.bitmap_size = SAT_VDP2_BITMAP_512X256;
    layer.color_mode = mode;
    layer.priority = 7;
    layer.palette = color_bank;
    layer.plane_address[0] = offset;
    sat_example_must(sat_vdp2_layer_configure(&layer));
    sat_example_must(sat_vdp2_layer_set_zoom(SAT_VDP2_NBG0, 0x20000, 0x20000));
    sat_example_must(sat_vdp2_screen_color_calc_set(SAT_VDP2_SCREEN_NBG0, 1, 15));
}

uint32_t probe_composition_pair_rgb_rejected(void) {
    sat_example_must(sat_vdp2_plan_begin());
    sat_example_must(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_BACK_SCREEN, BACKDROP, 2));
    sat_example_must(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_BITMAP, 0, RGB_BYTES));
    const sat_result_t result = sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_BITMAP, RGB_BYTES, RGB_BYTES);
    sat_example_must(sat_vdp2_plan_rollback());
    return result == SAT_ERR_CAPACITY;
}

void probe_composition_synthetic(void) {
    sat_example_must(sat_vdp2_plan_begin());
    sat_example_must(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_BITMAP, 0, RGB_BYTES));
    sat_example_must(sat_vdp2_plan_commit());
    const uint16_t values[] = {0, 0x001f, 0x8000, SAT_RGB555(8, 27, 28)};
    for (unsigned y = 0; y < ROWS; ++y) {
        for (unsigned x = 0; x < WIDTH; ++x) VRAM[y * STRIDE + x] = values[x / 40];
    }
    configure(SAT_VDP2_COLOR_MODE_32768, 0, 0);
    sat_example_must(sat_wait_vblank());
    sat_example_must(sat_vdp2_layers_commit());
    for (unsigned i = 0; i < 12; ++i) sat_example_must(sat_wait_vblank());
    sat_example_must(sat_vdp2_layer_release(SAT_VDP2_NBG0));
    sat_example_must(sat_vdp2_layers_commit());
    sat_example_must(sat_vdp2_plan_release(0));
}

void probe_composition_init(void) {
    sat_example_must(sat_vdp2_plan_begin());
    sat_example_must(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_BITMAP, 0, INDEX_BYTES));
    sat_example_must(sat_vdp2_plan_reserve_at(SAT_VDP2_RESERVE_BITMAP, INDEX_BYTES, INDEX_BYTES));
    sat_example_must(sat_vdp2_plan_palette_reserve(256, &palette));
    sat_example_must(sat_vdp2_plan_commit());
    /* Reserve code 0 for transparency. Color 1 is the covered RGB black. */
    static uint16_t colors[256];
    for (unsigned i = 0; i < 64; ++i) {
        const uint16_t rgb = SAT_RGB555(((i >> 4) & 3u) * 8,
            ((i >> 2) & 3u) * 8, (i & 3u) * 8);
        colors[i + 1] = rgb;
        for (unsigned b = 0; b < 8; ++b) {
            for (unsigned g = 0; g < 8; ++g) {
                for (unsigned r = 0; r < 8; ++r) {
                    rgb_to_index[rgb | (b << 10) | (g << 5) | r] = (uint8_t)(i + 1);
                }
            }
        }
    }
    sat_example_must(sat_vdp2_palette_upload(colors, 256, palette.bank * 256u + palette.code));
    for (unsigned y = 0; y < ROWS; ++y) {
        for (unsigned x = 0; x < WIDTH / 2; ++x) {
            VRAM[y * STRIDE / 2 + x] = 0;
            VRAM[INDEX_BYTES / 2 + y * STRIDE / 2 + x] = 0;
        }
    }
    configure(SAT_VDP2_COLOR_MODE_256, 0, (uint8_t)palette.bank);
}

void probe_composition_release(void) {
    sat_example_must(sat_wait_vblank());
    sat_example_must(sat_vdp2_screen_color_calc_set(SAT_VDP2_SCREEN_NBG0, 0, 0));
    sat_example_must(sat_vdp2_layer_release(SAT_VDP2_NBG0));
    sat_example_must(sat_vdp2_layers_commit());
    sat_example_must(sat_wait_vblank());
    sat_example_must(sat_vdp2_plan_release(0));
    sat_example_must(sat_vdp2_plan_release(INDEX_BYTES));
    sat_example_must(sat_vdp2_plan_palette_release(palette));
}

sat_result_t probe_composition_upload(uint32_t serial, const uint16_t* src, unsigned rows) {
    static _Alignas(4) uint16_t packed[ROWS][WIDTH / 2];
    const unsigned base = (serial & 1u) * INDEX_BYTES / 2;
    sat_dma_transfer_t list[SAT_DMA_MAX_LIST];
    for (unsigned y = 0; y < rows; ++y) {
        for (unsigned x = 0; x < WIDTH; x += 2) {
            const uint8_t left = rgb_to_index[src[y * WIDTH + x]];
            const uint8_t right = rgb_to_index[src[y * WIDTH + x + 1]];
            packed[y][x / 2] = (uint16_t)((left << 8) | right);
        }
    }
    for (unsigned row = 0; row < rows; row += SAT_DMA_MAX_LIST) {
        const unsigned remaining = rows - row;
        const unsigned count = remaining < SAT_DMA_MAX_LIST ? remaining : SAT_DMA_MAX_LIST;
        for (unsigned i = 0; i < count; ++i) {
            const unsigned y = row + i;
            list[i] = (sat_dma_transfer_t){packed[y], (uint16_t*)VRAM + base + y * STRIDE / 2, WIDTH};
        }
        sat_result_t result = sat_dma_start_list(0, list, count);
        if (result != SAT_OK) return result;
        result = sat_dma_wait(0);
        if (result != SAT_OK) return result;
    }
    return rows == ROWS ? SAT_OK : SAT_ERR_IO;
}

void probe_composition_publish(uint32_t serial) {
    sat_example_must(sat_vdp2_layer_set_pattern_name_base(
        SAT_VDP2_NBG0, (serial & 1u) * INDEX_BYTES));
    sat_example_must(sat_vdp2_layers_commit());
}
