#include <stddef.h>
#include <stdint.h>

#include "saturn/example_util.h"
#include "saturn/saturn.h"
#include "vdp2_nbg0_image/bg.h"

#define VDP2_REG(off) (*(volatile uint16_t*)(0x25F80000u + (off)))

enum {
    kTileWidth = 8,
    kTileHeight = 8,
    kBytesPerTile8bpp = 64,
    /* JoEngine-style NBG0 map base: 0x3B3B in MPABN0/MPCDN0.
     * This matches the working benchmark layout and keeps the map in B1.
     */
    kMapPlaneIndex = 0x003B,
    kMapWidthCells = 64,
    kMapHeightCells = 64,
    kMapWordBase = ((uint32_t)kMapPlaneIndex << 12u),
    kPaletteId = 1u,
    kPaletteWordOffset = 256u,
    kCellMapOffset = 0x0100u
};

static uint16_t g_pattern_names[kMapWidthCells * kMapHeightCells];

static void nbg0_direct_set_scroll(const sat_vdp2_scroll_t* scroll) {
    VDP2_REG(0x070u) = (uint16_t)(scroll->x_integer & 0x07FFu);
    VDP2_REG(0x072u) = (uint16_t)(scroll->x_fraction << 8u);
    VDP2_REG(0x074u) = (uint16_t)(scroll->y_integer & 0x07FFu);
    VDP2_REG(0x076u) = (uint16_t)(scroll->y_fraction << 8u);
}

static void nbg0_direct_init(void) {
    /* Mirror the working JoEngine sequence as closely as possible and
     * keep the whole setup local to the demo so we can isolate HAL issues.
     */
    VDP2_REG(0x000u) = 0x0000u;  /* TVMD off */
    VDP2_REG(0x00Eu) = 0x1327u;  /* RAMCTL */

    VDP2_REG(0x010u) = 0x5555u;
    VDP2_REG(0x012u) = 0xFEEEu;
    VDP2_REG(0x014u) = 0x5555u;
    VDP2_REG(0x016u) = 0xFEEEu;
    VDP2_REG(0x018u) = 0xFFFFu;
    VDP2_REG(0x01Au) = 0xEEEEu;
    VDP2_REG(0x01Cu) = 0x044Fu;
    VDP2_REG(0x01Eu) = 0xEEEEu;

    VDP2_REG(0x020u) = 0x0000u;  /* BGON */
    VDP2_REG(0x028u) = 0x3210u;  /* CHCTLA: NBG0 256 colors, 1x1 */
    VDP2_REG(0x030u) = 0xC00Cu;  /* PNCN0: 1 word, CN_12BIT, cell base in B1 */
    VDP2_REG(0x03Au) = 0x0000u;  /* PLSZ: 1x1 plane */
    VDP2_REG(0x03Cu) = 0x0000u;  /* MPOFN */
    VDP2_REG(0x040u) = 0x3B3Bu;  /* MPABN0 */
    VDP2_REG(0x042u) = 0x3B3Bu;  /* MPCDN0 */

    VDP2_REG(0x078u) = 0x0001u;
    VDP2_REG(0x07Au) = 0x0000u;
    VDP2_REG(0x07Cu) = 0x0001u;
    VDP2_REG(0x07Eu) = 0x0000u;

    VDP2_REG(0x0F0u) = 0x0606u;  /* PRISA */
    VDP2_REG(0x0F8u) = 0x0607u;  /* PRINA as in JoEngine */

    VDP2_REG(0x020u) = 0x0101u;  /* BGON: enable NBG0, disable transparent code */
    VDP2_REG(0x000u) = 0x8100u;  /* TVMD on, 320x224 NTSC */
}

static void build_tile_stream_8bpp(
    const uint8_t* pixels,
    uint16_t image_width,
    uint16_t tile_x,
    uint16_t tile_y,
    uint8_t out_bytes[kBytesPerTile8bpp]
) {
    uint16_t byte_index = 0u;

    /* Match JoEngine's jo_img_to_vdp2_cells() exactly for vertical_flip=false:
     * column-major inside the cell, with the source X mirrored.
     */
    for (uint16_t col = 0; col < kTileWidth; ++col) {
        for (uint16_t row = 0; row < kTileHeight; ++row) {
            const uint32_t src_x = (uint32_t)(image_width - 1u) -
                ((uint32_t)(tile_x * kTileWidth) + col);
            const uint32_t src_y = (uint32_t)(tile_y * kTileHeight) + row;
            out_bytes[byte_index++] = pixels[(src_y * image_width) + src_x];
        }
    }
}

static sat_result_t upload_image_as_tiles(const sat_indexed8_asset_t* asset) {
    uint8_t tile_bytes[kBytesPerTile8bpp];
    const uint16_t tiles_x = (uint16_t)(asset->width / kTileWidth);
    const uint16_t tiles_y = (uint16_t)(asset->height / kTileHeight);
    const uint32_t tile_count = (uint32_t)tiles_x * (uint32_t)tiles_y;
    uint32_t word_offset = 0x1000u;

    if ((asset->width % kTileWidth) != 0u || (asset->height % kTileHeight) != 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    if (tiles_x > kMapWidthCells || tiles_y > kMapHeightCells) {
        return SAT_ERR_CAPACITY;
    }
    if ((tile_count * kBytesPerTile8bpp) > 0x20000u) {
        return SAT_ERR_CAPACITY;
    }

    /* JoEngine stores both the map and the cell bank in VRAM B1.
     * The map starts at the bank base and the cells begin immediately after it.
     */
    volatile uint16_t* const vram_words = (volatile uint16_t*)(0x20000000u | 0x05E60000u);

    for (uint16_t ty = 0; ty < tiles_y; ++ty) {
        for (uint16_t tx = 0; tx < tiles_x; ++tx) {
            build_tile_stream_8bpp(asset->pixels, asset->width, tx, ty, tile_bytes);
            for (uint32_t i = 0; i < kBytesPerTile8bpp; i += 2u) {
                vram_words[word_offset++] = (uint16_t)(
                    ((uint16_t)tile_bytes[i] << 8u) |
                    (uint16_t)tile_bytes[i + 1u]
                );
            }
        }
    }

    /* Match JoEngine's __jo_create_map() layout so the plane repeats cleanly. */
    uint16_t x2 = 0u;
    uint16_t y2 = 0u;
    for (uint32_t i = 0; i < (uint32_t)(kMapWidthCells * kMapHeightCells); ++i) {
        if (i != 0u && ((i % kMapWidthCells) == 0u)) {
            ++x2;
        }
        if (x2 >= tiles_x) {
            x2 = 0u;
        }
        g_pattern_names[i] = (uint16_t)(
            (kPaletteId * 4096u) +
            kCellMapOffset +
            (2u * (uint16_t)(y2 + (x2 * tiles_y)))
        );
        ++y2;
        if (y2 >= tiles_y) {
            y2 = 0u;
        }
    }

    SAT_TRY(sat_vdp2_vram_write_words(
        kMapWordBase,
        g_pattern_names,
        (uint32_t)(kMapWidthCells * kMapHeightCells)
    ));

    return SAT_OK;
}

int main(void) {
    const sat_video_config_t video_cfg = {320, 224, 1, 0};
    const uint16_t max_scroll_x = (bg_asset.width > video_cfg.width)
        ? (uint16_t)(bg_asset.width - video_cfg.width)
        : 0u;
    const uint16_t max_scroll_y = (bg_asset.height > video_cfg.height)
        ? (uint16_t)(bg_asset.height - video_cfg.height)
        : 0u;
    sat_vdp2_scroll_t scroll = {0u, 0u, 0u, 0u};

    sat_example_must(sat_init(&video_cfg));
    nbg0_direct_init();
    sat_example_must(sat_vdp2_palette_upload(bg_asset.palette, 256u, kPaletteWordOffset));
    sat_example_must(upload_image_as_tiles(&bg_asset));
    sat_example_must(sat_vdp2_back_color_set(0x0000u));
    nbg0_direct_set_scroll(&scroll);

    while (1) {
        sat_pad_state_t pad = {0};

        sat_example_must(sat_wait_vblank());
        sat_example_must(sat_pad_poll(&pad));

        if ((pad.pressed & SAT_PAD_START) != 0u) {
            break;
        }

        if ((pad.held & SAT_PAD_LEFT) != 0u && scroll.x_integer > 0u) {
            --scroll.x_integer;
        }
        if ((pad.held & SAT_PAD_RIGHT) != 0u &&
            scroll.x_integer < max_scroll_x) {
            ++scroll.x_integer;
        }
        if ((pad.held & SAT_PAD_UP) != 0u && scroll.y_integer > 0u) {
            --scroll.y_integer;
        }
        if ((pad.held & SAT_PAD_DOWN) != 0u &&
            scroll.y_integer < max_scroll_y) {
            ++scroll.y_integer;
        }

        nbg0_direct_set_scroll(&scroll);
    }

    return 0;
}
