/* vdp2_panzer_ground.c - Panzer Dragoon style infinite ground plane
 *
 * Uses NBG0 tile mode (same working approach as vdp2_nbg0_image).
 * Generates perspective ground tiles procedurally.
 * NO external assets.
 *
 * D-pad UP/DOWN: scroll forward/back
 * START: exit
 */
#include <stddef.h>
#include <stdint.h>

#include "saturn/saturn.h"
#include "saturn/example_util.h"

#define VDP2_REG(off) (*(volatile uint16_t*)(0x25F80000u + (off)))

enum {
    kTileWidth  = 8,
    kTileHeight = 8,
    kBytesPerTile8bpp = 64,
    kMapPlaneIndex = 0x003B,
    kMapWidthCells = 64,
    kMapHeightCells = 64,
    kMapWordBase   = ((uint32_t)kMapPlaneIndex << 12u),
    kPaletteId     = 1u,
    kPaletteWordOffset = 256u,
    kCellMapOffset = 0x0100u,
};

/* Ground image: 256x256 (32x32 tiles) */
#define IMG_WIDTH  256
#define IMG_HEIGHT 256

static uint16_t g_pattern_names[kMapWidthCells * kMapHeightCells];

static void nbg0_direct_set_scroll(uint16_t x, uint16_t y) {
    VDP2_REG(0x070u) = (uint16_t)(x & 0x07FFu);
    VDP2_REG(0x072u) = 0x0000u;
    VDP2_REG(0x074u) = (uint16_t)(y & 0x07FFu);
    VDP2_REG(0x076u) = 0x0000u;
}

static void nbg0_direct_init(void) {
    VDP2_REG(0x000u) = 0x0000u;
    VDP2_REG(0x00Eu) = 0x1327u;

    VDP2_REG(0x010u) = 0x5555u;
    VDP2_REG(0x012u) = 0xFEEEu;
    VDP2_REG(0x014u) = 0x5555u;
    VDP2_REG(0x016u) = 0xFEEEu;
    VDP2_REG(0x018u) = 0xFFFFu;
    VDP2_REG(0x01Au) = 0xEEEEu;
    VDP2_REG(0x01Cu) = 0x044Fu;
    VDP2_REG(0x01Eu) = 0xEEEEu;

    VDP2_REG(0x020u) = 0x0000u;
    VDP2_REG(0x028u) = 0x3210u;
    VDP2_REG(0x030u) = 0xC00Cu;
    VDP2_REG(0x03Au) = 0x0000u;
    VDP2_REG(0x03Cu) = 0x0000u;
    VDP2_REG(0x040u) = 0x3B3Bu;
    VDP2_REG(0x042u) = 0x3B3Bu;

    VDP2_REG(0x078u) = 0x0001u;
    VDP2_REG(0x07Au) = 0x0000u;
    VDP2_REG(0x07Cu) = 0x0001u;
    VDP2_REG(0x07Eu) = 0x0000u;

    VDP2_REG(0x0F0u) = 0x0606u;
    VDP2_REG(0x0F8u) = 0x0607u;

    VDP2_REG(0x020u) = 0x0101u;
    VDP2_REG(0x000u) = 0x8100u;
}

/* Build earth-tone palette */
static void build_ground_palette(uint16_t palette[256]) {
    for (int i = 0; i < 256; i++) {
        uint16_t r = 0, g = 0, b = 0;
        if (i < 32) {
            r = (uint16_t)(i * 2); g = (uint16_t)(i);
        } else if (i < 128) {
            r = (uint16_t)(40 + ((i - 32) * 20) / 96);
            g = (uint16_t)(20 + ((i - 32) * 15) / 96);
            b = (uint16_t)((i - 32) / 8);
        } else if (i < 224) {
            r = (uint16_t)(60 + ((i - 128) * 10) / 96);
            g = (uint16_t)(35 + ((i - 128) * 10) / 96);
            b = (uint16_t)(10 + ((i - 128) * 5) / 96);
        } else {
            uint16_t v = (uint16_t)(10 + ((i - 224) * 20) / 31);
            r = v; g = v; b = (uint16_t)(v / 2);
        }
        palette[i] = (uint16_t)((b << 10) | (g << 5) | r);
    }
}

/* Generate 256x256 ground image with perspective grid */
static void fill_perspective_ground(uint8_t pixels[IMG_WIDTH * IMG_HEIGHT]) {
    for (int y = 0; y < IMG_HEIGHT; y++) {
        for (int x = 0; x < IMG_WIDTH; x++) {
            uint8_t col;
            if (y < 32) {
                /* Sky/horizon zone */
                col = (uint8_t)(y * 2);
            } else {
                int gy = y - 32;
                int gx = x;
                /* Horizontal lines with perspective spacing */
                int line_spacing = 4 + (gy * gy) / 180;
                int h_line = (gy % line_spacing) < 2;
                /* Vertical lines converging to center */
                int cx = IMG_WIDTH / 2;
                int dx = gx - cx;
                int v_spacing = 12 + (gy * 2);
                int v_line = (dx < 0 ? (-dx % v_spacing) : (dx % v_spacing)) < 2;
                /* Base checker ground */
                int checker = ((gx / 16) + (gy / 16)) & 1;
                col = (uint8_t)(checker ? 80 : 50);
                if (h_line) col = 180;
                if (v_line && gy > 16) col = 180;
                if (y < 40) col = (uint8_t)(col + (40 - y) * 3);
            }
            pixels[y * IMG_WIDTH + x] = col;
        }
    }
}

/* Extract 8x8 tile from image (column-major, X-mirrored like JoEngine) */
static void build_tile_stream_8bpp(
    const uint8_t* pixels,
    uint16_t image_width,
    uint16_t tile_x,
    uint16_t tile_y,
    uint8_t out_bytes[kBytesPerTile8bpp]
) {
    uint16_t bi = 0;
    for (uint16_t col = 0; col < kTileWidth; ++col) {
        for (uint16_t row = 0; row < kTileHeight; ++row) {
            const uint32_t src_x = (uint32_t)(image_width - 1u) -
                ((uint32_t)(tile_x * kTileWidth) + col);
            const uint32_t src_y = (uint32_t)(tile_y * kTileHeight) + row;
            out_bytes[bi++] = pixels[(src_y * image_width) + src_x];
        }
    }
}

/* Upload image as tiles + build repeating map (same as vdp2_nbg0_image) */
static sat_result_t upload_ground_as_tiles(const uint8_t pixels[IMG_WIDTH * IMG_HEIGHT]) {
    uint8_t tile_bytes[kBytesPerTile8bpp];
    const uint16_t tiles_x = IMG_WIDTH / kTileWidth;   /* 32 */
    const uint16_t tiles_y = IMG_HEIGHT / kTileHeight;  /* 32 */
    uint32_t word_offset = kCellMapOffset;

    volatile uint16_t* const vram_words = (volatile uint16_t*)(0x20000000u | 0x05E60000u);

    for (uint16_t ty = 0; ty < tiles_y; ++ty) {
        for (uint16_t tx = 0; tx < tiles_x; ++tx) {
            build_tile_stream_8bpp(pixels, IMG_WIDTH, tx, ty, tile_bytes);
            for (uint32_t i = 0; i < kBytesPerTile8bpp; i += 2u) {
                vram_words[word_offset++] = (uint16_t)(
                    ((uint16_t)tile_bytes[i] << 8u) | (uint16_t)tile_bytes[i + 1u]);
            }
        }
    }

    /* Build repeating map: 64x64 cells tiled from 32x32 tiles */
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

    SAT_TRY(sat_vdp2_vram_write_words(
        kMapWordBase, g_pattern_names,
        (uint32_t)(kMapWidthCells * kMapHeightCells)));

    return SAT_OK;
}

int main(void) {
    uint16_t palette[256];
    uint8_t pixels[IMG_WIDTH * IMG_HEIGHT];

    build_ground_palette(palette);
    fill_perspective_ground(pixels);

    sat_video_config_t cfg = {320, 224, 1, 0};
    sat_example_must(sat_init(&cfg));

    nbg0_direct_init();
    sat_example_must(sat_vdp2_palette_upload(palette, 256u, kPaletteWordOffset));
    sat_example_must(upload_ground_as_tiles(pixels));
    sat_example_must(sat_vdp2_back_color_set(0x0000u));
    nbg0_direct_set_scroll(0u, 0u);

    uint16_t scroll_y = 0;
    const uint16_t max_scroll = (IMG_HEIGHT > 224) ? (uint16_t)(IMG_HEIGHT - 224) : 0u;

    while (1) {
        sat_pad_state_t pad = {0};
        sat_example_must(sat_wait_vblank());
        sat_example_must(sat_pad_poll(&pad));
        if ((pad.pressed & SAT_PAD_START) != 0u) break;

        if ((pad.held & SAT_PAD_UP)   && scroll_y < max_scroll) ++scroll_y;
        if ((pad.held & SAT_PAD_DOWN) && scroll_y > 0)          --scroll_y;

        nbg0_direct_set_scroll(0u, scroll_y);
    }
    return 0;
}
