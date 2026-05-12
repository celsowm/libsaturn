/* vdp2_nbg0_ramp.c - Procedural gradient using NBG0 with direct register writes
 *
 * Copies the working approach from vdp2_nbg0_image (direct VDP2_REG writes).
 * NO external assets. Everything generated in-code.
 */
#include <stddef.h>
#include <stdint.h>

#include "saturn/saturn.h"
#include "saturn/example_util.h"

#define VDP2_REG(off) (*(volatile uint16_t*)(0x25F80000u + (off)))

/* NBG0 map configuration (matching working vdp2_nbg0_image) */
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
};

/* Our "image" is 64x64 pixels (8x8 tiles) with a color ramp */
#define IMG_WIDTH  64
#define IMG_HEIGHT 64

static uint16_t g_pattern_names[kMapWidthCells * kMapHeightCells];

/* Direct register NBG0 init - copies working vdp2_nbg0_image approach */
static void nbg0_direct_init(void) {
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
    VDP2_REG(0x0F8u) = 0x0607u;  /* PRINA */

    VDP2_REG(0x020u) = 0x0101u;  /* BGON: enable NBG0 */
    VDP2_REG(0x000u) = 0x8100u;  /* TVMD on, 320x224 NTSC */
}

static void nbg0_direct_set_scroll(uint16_t x, uint16_t y) {
    VDP2_REG(0x070u) = (uint16_t)(x & 0x07FFu);
    VDP2_REG(0x072u) = 0x0000u;
    VDP2_REG(0x074u) = (uint16_t)(y & 0x07FFu);
    VDP2_REG(0x076u) = 0x0000u;
}

/* Build a 256-color RGB ramp palette */
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

/* Generate a 64x64 gradient image */
static void build_gradient_image(uint8_t pixels[IMG_WIDTH * IMG_HEIGHT]) {
    for (int y = 0; y < IMG_HEIGHT; y++) {
        for (int x = 0; x < IMG_WIDTH; x++) {
            /* Horizontal ramp with vertical phase shift */
            pixels[y * IMG_WIDTH + x] = (uint8_t)((x + y * 4) & 0xFF);
        }
    }
}

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

/* Convert 64x64 image into 8x8 tiles and upload to VRAM */
static sat_result_t upload_image_as_tiles(const uint8_t pixels[IMG_WIDTH * IMG_HEIGHT]) {
    uint8_t tile_bytes[kBytesPerTile8bpp];
    const uint16_t tiles_x = IMG_WIDTH / kTileWidth;
    const uint16_t tiles_y = IMG_HEIGHT / kTileHeight;
    uint32_t word_offset = 0x1000u;

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

    SAT_TRY(sat_vdp2_vram_write_words(
        kMapWordBase, g_pattern_names,
        (uint32_t)(kMapWidthCells * kMapHeightCells)));

    return SAT_OK;
}

int main(void) {
    uint16_t palette[256];
    uint8_t pixels[IMG_WIDTH * IMG_HEIGHT];

    build_palette(palette);
    build_gradient_image(pixels);

    sat_video_config_t cfg = {320, 224, 1, 0};
    sat_example_must(sat_init(&cfg));

    /* Direct register init (the one that works) */
    nbg0_direct_init();

    /* Upload palette */
    sat_example_must(sat_vdp2_palette_upload(palette, 256u, kPaletteWordOffset));

    /* Upload image tiles */
    sat_example_must(upload_image_as_tiles(pixels));

    sat_example_must(sat_vdp2_back_color_set(0x0000u));
    nbg0_direct_set_scroll(0u, 0u);

    uint16_t scroll_x = 0;
    uint16_t scroll_y = 0;
    const uint16_t max_scroll = 256;

    while (1) {
        sat_pad_state_t pad = {0};
        sat_example_must(sat_wait_vblank());
        sat_example_must(sat_pad_poll(&pad));

        if ((pad.pressed & SAT_PAD_START) != 0u) break;

        if ((pad.held & SAT_PAD_LEFT)  && scroll_x > 0) --scroll_x;
        if ((pad.held & SAT_PAD_RIGHT) && scroll_x < max_scroll) ++scroll_x;
        if ((pad.held & SAT_PAD_UP)    && scroll_y > 0) --scroll_y;
        if ((pad.held & SAT_PAD_DOWN)  && scroll_y < max_scroll) ++scroll_y;

        nbg0_direct_set_scroll(scroll_x, scroll_y);
    }

    return 0;
}
