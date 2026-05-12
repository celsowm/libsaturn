/* vdp2_nbg0_ramp_api.c - Procedural gradient using NBG0 via libsaturn API
 *
 * Same visual output as vdp2_nbg0_ramp (direct registers) but uses
 * the libsaturn API (sat_vdp2_nbg0_init, sat_vdp2_vram_write_words, etc.)
 * to verify the API is now working correctly.
 */
#include <stddef.h>
#include <stdint.h>

#include "saturn/saturn.h"
#include "saturn/example_util.h"

enum {
    kMapPlaneIndex = 0x003B,
    kMapWidthCells = 64,
    kMapHeightCells = 64,
    kMapWordBase   = ((uint32_t)kMapPlaneIndex << 12u),
    kPaletteId     = 1u,
    kPaletteWordOffset = 256u,
    kCellMapOffset = 0x0100u,
    kTileWidth     = 8,
    kTileHeight    = 8,
    kBytesPerTile8bpp = 64,
    kImageWidth    = 64,
    kImageHeight   = 64,
};

static uint16_t g_pattern_names[kMapWidthCells * kMapHeightCells];

/* JoEngine-style tile streaming: column-major with X mirrored */
static void build_tile_stream_8bpp(
    const uint8_t* pixels,
    uint16_t image_width,
    uint16_t tile_x,
    uint16_t tile_y,
    uint8_t out_bytes[kBytesPerTile8bpp]
) {
    uint16_t byte_index = 0u;
    for (uint16_t col = 0; col < kTileWidth; ++col) {
        for (uint16_t row = 0; row < kTileHeight; ++row) {
            const uint32_t src_x = (uint32_t)(image_width - 1u) -
                ((uint32_t)(tile_x * kTileWidth) + col);
            const uint32_t src_y = (uint32_t)(tile_y * kTileHeight) + row;
            out_bytes[byte_index++] = pixels[(src_y * image_width) + src_x];
        }
    }
}

/* Generate 256-color RGB ramp palette */
static void build_palette(uint16_t palette[256]) {
    for (int i = 0; i < 256; i++) {
        uint16_t r, g, b;
        if (i < 64) {
            r = (uint16_t)((i * 31) / 63); g = 0; b = 0;
        } else if (i < 128) {
            r = 0; g = (uint16_t)(((i - 64) * 31) / 63); b = 0;
        } else if (i < 192) {
            r = 0; g = 0; b = (uint16_t)(((i - 128) * 31) / 63);
        } else {
            uint16_t v = (uint16_t)(((i - 192) * 31) / 63);
            r = v; g = v; b = v;
        }
        palette[i] = (uint16_t)((b << 10) | (g << 5) | r);
    }
}

/* Generate 64x64 gradient image */
static void build_gradient_image(uint8_t pixels[kImageWidth * kImageHeight]) {
    for (int y = 0; y < kImageHeight; y++) {
        for (int x = 0; x < kImageWidth; x++) {
            pixels[y * kImageWidth + x] = (uint8_t)((x + y * 4) & 0xFF);
        }
    }
}

/* Upload image as tiles + build map, then upload map via API */
static sat_result_t upload_and_map(const uint8_t pixels[kImageWidth * kImageHeight]) {
    uint8_t tile_bytes[kBytesPerTile8bpp];
    const uint16_t tiles_x = kImageWidth / kTileWidth;
    const uint16_t tiles_y = kImageHeight / kTileHeight;
    uint32_t word_offset = 0x1000u;

    volatile uint16_t* const vram_words = (volatile uint16_t*)(0x20000000u | 0x05E60000u);

    for (uint16_t ty = 0; ty < tiles_y; ++ty) {
        for (uint16_t tx = 0; tx < tiles_x; ++tx) {
            build_tile_stream_8bpp(pixels, kImageWidth, tx, ty, tile_bytes);
            for (uint32_t i = 0; i < kBytesPerTile8bpp; i += 2u) {
                vram_words[word_offset++] = (uint16_t)(
                    ((uint16_t)tile_bytes[i] << 8u) | (uint16_t)tile_bytes[i + 1u]);
            }
        }
    }

    /* Build pattern names */
    uint16_t x2 = 0u, y2 = 0u;
    for (uint32_t i = 0; i < (uint32_t)(kMapWidthCells * kMapHeightCells); ++i) {
        if (i != 0u && ((i % kMapWidthCells) == 0u)) { ++x2; }
        if (x2 >= tiles_x) { x2 = 0u; }
        g_pattern_names[i] = (uint16_t)(
            (kPaletteId * 4096u) + kCellMapOffset +
            (2u * (uint16_t)(y2 + (x2 * tiles_y))));
        ++y2;
        if (y2 >= tiles_y) { y2 = 0u; }
    }

    /* Upload map via libsaturn API */
    SAT_TRY(sat_vdp2_vram_write_words(
        kMapWordBase, g_pattern_names,
        (uint32_t)(kMapWidthCells * kMapHeightCells)));

    return SAT_OK;
}

int main(void) {
    uint16_t palette[256];
    uint8_t pixels[kImageWidth * kImageHeight];

    build_palette(palette);
    build_gradient_image(pixels);

    sat_video_config_t cfg = {320, 224, 1, 0};
    sat_example_must(sat_init(&cfg));

    /* Init NBG0 via API FIRST (before VRAM uploads) */
    const sat_vdp2_nbg0_config_t nbg0_cfg = {
        SAT_VDP2_CHAR_SIZE_1X1,
        SAT_VDP2_COLOR_MODE_256,
        kMapPlaneIndex,
        0u,
        0u
    };
    sat_example_must(sat_vdp2_nbg0_init(&nbg0_cfg));

    /* Then upload palette and tiles */
    sat_example_must(sat_vdp2_palette_upload(palette, 256u, kPaletteWordOffset));

    /* Upload tiles directly to VRAM (B1 bank) */
    {
        volatile uint16_t* const vram = (volatile uint16_t*)(0x20000000u | 0x05E60000u);
        uint32_t off = 0x1000u;
        uint8_t tile_bytes[kBytesPerTile8bpp];
        const uint16_t tiles_x = kImageWidth / kTileWidth;
        const uint16_t tiles_y = kImageHeight / kTileHeight;
        for (uint16_t ty = 0; ty < tiles_y; ++ty) {
            for (uint16_t tx = 0; tx < tiles_x; ++tx) {
                build_tile_stream_8bpp(pixels, kImageWidth, tx, ty, tile_bytes);
                for (uint32_t i = 0; i < kBytesPerTile8bpp; i += 2u) {
                    vram[off++] = (uint16_t)(
                        ((uint16_t)tile_bytes[i] << 8u) | (uint16_t)tile_bytes[i + 1u]);
                }
            }
        }
    }

    /* Upload map via API */
    sat_example_must(upload_and_map(pixels));

    sat_example_must(sat_vdp2_back_color_set(0x0000u));

    uint16_t scroll_x = 0, scroll_y = 0;
    const uint16_t max_scroll = 256;
    const sat_vdp2_scroll_t zero_scroll = {0, 0, 0, 0};
    sat_example_must(sat_vdp2_nbg0_set_scroll(&zero_scroll));

    while (1) {
        sat_pad_state_t pad = {0};
        sat_example_must(sat_wait_vblank());
        sat_example_must(sat_pad_poll(&pad));
        if ((pad.pressed & SAT_PAD_START) != 0u) break;

        if ((pad.held & SAT_PAD_LEFT)  && scroll_x > 0) --scroll_x;
        if ((pad.held & SAT_PAD_RIGHT) && scroll_x < max_scroll) ++scroll_x;
        if ((pad.held & SAT_PAD_UP)    && scroll_y > 0) --scroll_y;
        if ((pad.held & SAT_PAD_DOWN)  && scroll_y < max_scroll) ++scroll_y;

        const sat_vdp2_scroll_t sc = {scroll_x, 0, scroll_y, 0};
        sat_example_must(sat_vdp2_nbg0_set_scroll(&sc));
    }

    return 0;
}
