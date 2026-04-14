/* vdp2_infinite_plan.c - Infinite scrolling background using VDP2 NBG0 */
#include <stdint.h>

#include "saturn/saturn.h"
#include "saturn/example_util.h"
#include "vdp2_infinite_plan/seamless_sea.h"

/* Screen dimensions */
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 224

/* Tile dimensions (VDP2 character size 1x1 = 8x8 pixels) */
#define TILE_WIDTH  8
#define TILE_HEIGHT 8

/* Screen size in tiles */
#define SCREEN_TILES_X  (SCREEN_WIDTH / TILE_WIDTH)   /* 40 tiles */
#define SCREEN_TILES_Y  (SCREEN_HEIGHT / TILE_HEIGHT) /* 28 tiles */

/* Total tiles in screen buffer */
#define SCREEN_TILE_COUNT (SCREEN_TILES_X * SCREEN_TILES_Y)

/* Image dimensions (after resize) */
#define IMAGE_WIDTH  256
#define IMAGE_HEIGHT 256

/* Image size in tiles */
#define IMAGE_TILES_X  (IMAGE_WIDTH / TILE_WIDTH)   /* 32 tiles */
#define IMAGE_TILES_Y  (IMAGE_HEIGHT / TILE_HEIGHT) /* 32 tiles */

/* VRAM layout */
#define CHAR_BASE_WORD_OFFSET   0x0000u  /* Tile patterns at start of VRAM */
#define MAP_PLANE_INDEX         0x0010u  /* Map at offset 0x0010 << 11 = 0x8000 */

/* Tile pattern data: 8x8 pixels, 4bpp (16 colors), 1 word per row = 16 words per tile */
#define TILE_WORDS  (TILE_WIDTH * TILE_HEIGHT / 2)  /* 32 words per 8x8 tile at 4bpp */

/* Tile cache - stores converted tiles */
static uint16_t tile_cache[IMAGE_TILES_X * IMAGE_TILES_Y * TILE_WORDS];

/* Pattern name table for NBG0 map */
static uint16_t pattern_names[SCREEN_TILE_COUNT];

/* Scroll position (fixed-point 16.16) */
static sat_fx16_t scroll_x = 0;
static sat_fx16_t scroll_y = 0;

/* Fixed-point shift amount */
#define FX16_SHIFT 16

/* Convert 8-bit palette index to 4-bit (for 16-color mode) */
static inline uint8_t palette_idx_8to4(uint8_t idx8) {
    /* In 4bpp mode, we use only the lower 4 bits */
    return (uint8_t)(idx8 & 0x0Fu);
}

/* Convert a single 8x8 tile from indexed8 to VDP2 4bpp format */
static void convert_tile(
    const uint8_t* src_pixels,
    uint32_t src_stride,
    uint16_t* dst_words
) {
    for (uint32_t row = 0; row < TILE_HEIGHT; ++row) {
        uint16_t word = 0;
        for (uint32_t col = 0; col < TILE_WIDTH; ++col) {
            uint8_t pal_idx = src_pixels[row * src_stride + col];
            uint8_t pal4 = palette_idx_8to4(pal_idx);
            /* Pack 4-bit palette into word (MSB first) */
            word = (uint16_t)(word | ((uint16_t)pal4 << (12u - (col * 4u))));
        }
        dst_words[row] = word;
    }
}

/* Convert entire image to VDP2 tiles */
static void convert_image_to_tiles(const uint8_t* pixels, uint32_t width, uint32_t height) {
    const uint32_t tiles_x = width / TILE_WIDTH;
    const uint32_t tiles_y = height / TILE_HEIGHT;

    for (uint32_t ty = 0; ty < tiles_y; ++ty) {
        for (uint32_t tx = 0; tx < tiles_x; ++tx) {
            /* Source pixel coordinates */
            const uint32_t src_x = tx * TILE_WIDTH;
            const uint32_t src_y = ty * TILE_HEIGHT;

            /* Destination tile offset */
            const uint32_t tile_idx = (ty * tiles_x + tx);
            uint16_t* dst = &tile_cache[tile_idx * TILE_WORDS];

            convert_tile(
                &pixels[src_y * width + src_x],
                width,
                dst
            );
        }
    }
}

/* Upload all tiles to VRAM */
static sat_result_t upload_tiles(void) {
    const uint32_t total_tiles = IMAGE_TILES_X * IMAGE_TILES_Y;
    const uint32_t total_words = total_tiles * TILE_WORDS;

    SAT_TRY(sat_vdp2_vram_write_words(
        CHAR_BASE_WORD_OFFSET,
        tile_cache,
        total_words
    ));

    return SAT_OK;
}

/* Update pattern name table based on scroll position */
static void update_pattern_names(void) {
    /* Get integer tile coordinates from scroll position */
    int32_t tile_x = (int32_t)(scroll_x >> FX16_SHIFT);
    int32_t tile_y = (int32_t)(scroll_y >> FX16_SHIFT);

    /* Normalize to image tile range (wrapping) */
    tile_x = ((tile_x % IMAGE_TILES_X) + IMAGE_TILES_X) % IMAGE_TILES_X;
    tile_y = ((tile_y % IMAGE_TILES_Y) + IMAGE_TILES_Y) % IMAGE_TILES_Y;

    /* Fill pattern name table with tile indices */
    for (uint32_t y = 0; y < SCREEN_TILES_Y; ++y) {
        for (uint32_t x = 0; x < SCREEN_TILES_X; ++x) {
            /* Calculate which image tile to show (with wrapping) */
            uint32_t img_x = (uint32_t)((tile_x + (int32_t)x) % IMAGE_TILES_X);
            uint32_t img_y = (uint32_t)((tile_y + (int32_t)y) % IMAGE_TILES_Y);

            /* Pattern name = tile index in VRAM */
            uint16_t pattern_name = (uint16_t)(img_y * IMAGE_TILES_X + img_x);

            pattern_names[y * SCREEN_TILES_X + x] = pattern_name;
        }
    }
}

/* Upload pattern name table to VDP2 */
static sat_result_t upload_pattern_names(void) {
    const sat_vdp2_map_region_t region = {
        0u, 0u,
        SCREEN_TILES_X,
        SCREEN_TILES_Y
    };

    SAT_TRY(sat_vdp2_nbg0_map_write_region(
        pattern_names,
        &region,
        SCREEN_TILES_X
    ));

    return SAT_OK;
}

int main(void) {
    /* Initialize Saturn with 320x224 resolution */
    sat_video_config_t cfg = {SCREEN_WIDTH, SCREEN_HEIGHT, 1, 0};
    sat_example_must(sat_init(&cfg));

    /* Configure NBG0 for 4bpp (16-color) tile mode */
    const sat_vdp2_nbg0_config_t nbg0_cfg = {
        SAT_VDP2_CHAR_SIZE_1X1,   /* 8x8 pixel tiles */
        SAT_VDP2_COLOR_MODE_16,   /* 16 colors (4bpp) */
        MAP_PLANE_INDEX,          /* Map plane index */
        1u,                       /* Transparent code enabled */
        0u
    };
    sat_example_must(sat_vdp2_nbg0_init(&nbg0_cfg));

    /* Set initial scroll position to 0 */
    const sat_vdp2_scroll_t scroll = {0u, 0u, 0u, 0u};
    sat_example_must(sat_vdp2_nbg0_set_scroll(&scroll));

    /* Upload palette (use first 16 colors from 256-color palette) */
    sat_example_must(sat_vdp2_palette_upload(
        seamless_sea_asset.palette,
        16,  /* 16-color mode */
        0
    ));

    /* Convert image to VDP2 tiles */
    convert_image_to_tiles(
        seamless_sea_asset.pixels,
        seamless_sea_asset.width,
        seamless_sea_asset.height
    );

    /* Upload tiles to VRAM */
    sat_example_must(upload_tiles());

    /* Initialize pattern names */
    update_pattern_names();

    /* Upload pattern names to VDP2 */
    sat_example_must(upload_pattern_names());

    /* Enable NBG0 */
    sat_example_must(sat_vdp2_nbg0_set_enabled(1));

    /* Set backdrop color (black) */
    sat_example_must(sat_vdp2_back_color_set(0x0000));

    /* Movement speed in fixed-point */
    const sat_fx16_t speed = SAT_FX16_ONE / 4;  /* 0.25 pixels per frame */

    /* Main loop */
    uint32_t frame = 0;
    while (1) {
        sat_pad_state_t pad = {0};
        sat_example_must(sat_wait_vblank());
        sat_example_must(sat_pad_poll(&pad));

        /* Exit on START button */
        if ((pad.pressed & SAT_PAD_START) != 0u) {
            break;
        }

        /* Update scroll position based on D-pad input */
        if ((pad.held & SAT_PAD_LEFT) != 0u) {
            scroll_x -= speed;
        }
        if ((pad.held & SAT_PAD_RIGHT) != 0u) {
            scroll_x += speed;
        }
        if ((pad.held & SAT_PAD_UP) != 0u) {
            scroll_y -= speed;
        }
        if ((pad.held & SAT_PAD_DOWN) != 0u) {
            scroll_y += speed;
        }

        /* Wrap scroll position to keep it within image bounds */
        int32_t max_scroll_x = (int32_t)((IMAGE_WIDTH - SCREEN_WIDTH) * SAT_FX16_ONE);
        int32_t max_scroll_y = (int32_t)((IMAGE_HEIGHT - SCREEN_HEIGHT) * SAT_FX16_ONE);

        if (scroll_x < 0) {
            scroll_x = (sat_fx16_t)(scroll_x + (sat_fx16_t)((IMAGE_WIDTH * SAT_FX16_ONE)));
        }
        if (scroll_x > (sat_fx16_t)max_scroll_x) {
            scroll_x = (sat_fx16_t)(scroll_x - (sat_fx16_t)((IMAGE_WIDTH * SAT_FX16_ONE)));
        }
        if (scroll_y < 0) {
            scroll_y = (sat_fx16_t)(scroll_y + (sat_fx16_t)((IMAGE_HEIGHT * SAT_FX16_ONE)));
        }
        if (scroll_y > (sat_fx16_t)max_scroll_y) {
            scroll_y = (sat_fx16_t)(scroll_y - (sat_fx16_t)((IMAGE_HEIGHT * SAT_FX16_ONE)));
        }

        /* Update pattern names and scroll register every frame */
        update_pattern_names();
        sat_example_must(upload_pattern_names());

        /* Set VDP2 scroll registers with sub-pixel precision */
        uint16_t x_int = (uint16_t)((scroll_x >> FX16_SHIFT) & 0x07FFu);
        uint16_t y_int = (uint16_t)((scroll_y >> FX16_SHIFT) & 0x07FFu);
        uint16_t x_frac = (uint16_t)(scroll_x & 0xFFFFu);
        uint16_t y_frac = (uint16_t)(scroll_y & 0xFFFFu);

        const sat_vdp2_scroll_t new_scroll = {x_int, x_frac, y_int, y_frac};
        sat_example_must(sat_vdp2_nbg0_set_scroll(&new_scroll));

        ++frame;
    }

    return 0;
}
