/* vdp2_rbg0_infinite_plane.c - RBG0 rotation plane with perspective
 *
 * Demonstrates VDP2 RBG0 (Rotation Background 0) for infinite plane effects
 * similar to Panzer Dragoon ground/ceiling rendering.
 *
 * Features:
 * - 256-color bitmap mode (8bpp)
 * - 512x256 bitmap (hardware limitation - 256x256 fits within)
 * - Infinite scrolling via rotation parameters
 * - D-pad controlled movement
 */
#include <stdint.h>

#include "saturn/saturn.h"
#include "saturn/example_util.h"
#include "vdp2_rbg0_infinite_plane/seamless_sea.h"

/* Screen dimensions */
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 224

/* Bitmap size for RBG0 (hardware supports 512x256 or 512x512) */
#define BITMAP_WIDTH  512
#define BITMAP_HEIGHT 256

/* VRAM layout:
 * - Bitmap data at 0x00000 (512x256 bytes = 131072 bytes = 65536 words)
 * - Rotation parameters at 0x10000 (after bitmap)
 */
#define BITMAP_BASE_WORD  0x00000u
#define ROT_PARAM_BASE    0x10000u

/* Scroll position (fixed-point 16.16) */
static sat_fx16_t scroll_x = 0;
static sat_fx16_t scroll_z = 0;

/* Fixed-point shift */
#define FX16_SHIFT 16

/* Upload bitmap data to VRAM
 * For 512x256 bitmap in 256-color mode, we need 131072 bytes
 * Our 256x256 texture is tiled to fill the 512x256 area
 */
static sat_result_t upload_bitmap(const uint8_t* pixels, uint32_t src_width, uint32_t src_height) {
    volatile uint16_t* vram = (volatile uint16_t*)(0x20000000u | 0x05E00000u);
    uint32_t word_offset = BITMAP_BASE_WORD;

    /* Tile the 256x256 source to fill 512x256 bitmap */
    for (uint32_t y = 0; y < BITMAP_HEIGHT; ++y) {
        for (uint32_t x = 0; x < BITMAP_WIDTH; x += 2) {
            /* Source coordinates (wrap around 256x256) */
            uint32_t src_x = x % src_width;
            uint32_t src_y = y % src_height;

            uint16_t word = 0;
            word = (uint16_t)((pixels[src_y * src_width + src_x] << 8) |
                              (src_x + 1 < src_width ? pixels[src_y * src_width + (src_x + 1) % src_width] : 0));
            vram[word_offset++] = word;
        }
    }

    return SAT_OK;
}

/* Setup rotation parameter table with correct values for flat 2D scrolling */
static sat_result_t setup_rotation_params(void) {
    volatile uint16_t* vram = (volatile uint16_t*)(0x20000000u | 0x05E00000u);
    uint32_t base = ROT_PARAM_BASE;

    /* Initialize all parameters to zero */
    for (uint32_t i = 0; i < 48u; ++i) {
        vram[base + i] = 0x0000u;
    }

    /* Set identity rotation matrix and unit scaling using the corrected layout. */
    sat_example_must(sat_vdp2_rbg0_set_rotation_matrix(ROT_PARAM_BASE, 0, 0, 0));
    sat_example_must(sat_vdp2_rbg0_set_scaling(ROT_PARAM_BASE, SAT_FX16_ONE, SAT_FX16_ONE));
    /* ΔXst=0, ΔYst=1: advance Y by 1 per scanline */
    sat_example_must(sat_vdp2_rbg0_set_vertical_increments(ROT_PARAM_BASE, 0, 0, 1, 0));

    return SAT_OK;
}

/* Update scroll position in rotation parameters */
static void update_scroll_params(void) {
    /* Get integer scroll position */
    int32_t x_int = (int32_t)(scroll_x >> FX16_SHIFT);
    int32_t y_int = (int32_t)(scroll_z >> FX16_SHIFT);

    /* Get fractional part (upper 10 bits) */
    int32_t x_frac = (int32_t)((scroll_x >> 6u) & 0x03FFu);
    int32_t y_frac = (int32_t)((scroll_z >> 6u) & 0x03FFu);

    sat_example_must(sat_vdp2_rbg0_set_scroll(
        ROT_PARAM_BASE,
        x_int, x_frac,
        y_int, y_frac
    ));
}

int main(void) {
    /* Initialize Saturn with 320x224 resolution */
    sat_video_config_t cfg = {SCREEN_WIDTH, SCREEN_HEIGHT, 1, 0};
    sat_example_must(sat_init(&cfg));
    sat_example_must(sat_vdp1_set_erase_enabled(0));

    /* Upload full 256-color palette */
    sat_example_must(sat_vdp2_palette_upload(
        seamless_sea_asset.palette,
        256,
        0
    ));

    /* Upload bitmap data to VRAM (tile 256x256 to 512x256) */
    sat_example_must(upload_bitmap(
        seamless_sea_asset.pixels,
        seamless_sea_asset.width,
        seamless_sea_asset.height
    ));

    /* Setup rotation parameter table */
    sat_example_must(setup_rotation_params());

    /* Configure RBG0 in 256-color bitmap mode
     * Note: Hardware only supports 512x256 or 512x512 for RBG0 bitmap
     */
    const sat_vdp2_rbg0_config_t rbg0_cfg = {
        SAT_VDP2_RBG0_BITMAP_512x256,  /* 512x256 bitmap (hardware limitation) */
        SAT_VDP2_COLOR_MODE_256,       /* 256 colors (8bpp) */
        BITMAP_BASE_WORD,              /* Bitmap VRAM offset */
        ROT_PARAM_BASE                 /* Rotation params VRAM offset */
    };
    sat_example_must(sat_vdp2_rbg0_init(&rbg0_cfg));

    /* Set rotation parameter mode to A */
    sat_example_must(sat_vdp2_rbg0_set_param_mode(SAT_VDP2_RBG0_PARAM_A));

    /* Set coordinate increments dX=1.0, dY=0 for 1:1 pixel mapping */
    sat_example_must(sat_vdp2_rbg0_set_coordinate_increments(
        ROT_PARAM_BASE,
        1, 0,  /* dX = 1.0 */
        0, 0   /* dY = 0 */
    ));

    /* Set initial scroll position to 0 */
    sat_example_must(sat_vdp2_rbg0_set_scroll(
        ROT_PARAM_BASE,
        0, 0,  /* Xst integer/fraction */
        0, 0   /* Yst integer/fraction */
    ));

    /* Enable RBG0 */
    sat_example_must(sat_vdp2_rbg0_set_enabled(1));

    /* Set backdrop color (black) */
    sat_example_must(sat_vdp2_back_color_set(0x0000));

    /* Main loop */
    uint32_t frame = 0;
    while (1) {
        sat_pad_state_t pad = {0};
        sat_example_must(sat_wait_vblank());
        /* Register writes outside VBlank are silently dropped by the VDP2.
         * Re-apply all RBG0 config registers during VBlank. */
        sat_example_must(sat_vdp2_rbg0_commit());
        sat_example_must(sat_pad_poll(&pad));

        /* Exit on START button */
        if ((pad.pressed & SAT_PAD_START) != 0u) {
            break;
        }

        /* Update scroll based on D-pad input */
        const sat_fx16_t speed = 2 << FX16_SHIFT;  /* 2 pixels per frame */

        if ((pad.held & SAT_PAD_LEFT) != 0u) {
            scroll_x -= speed;
        }
        if ((pad.held & SAT_PAD_RIGHT) != 0u) {
            scroll_x += speed;
        }
        if ((pad.held & SAT_PAD_UP) != 0u) {
            scroll_z -= speed;
        }
        if ((pad.held & SAT_PAD_DOWN) != 0u) {
            scroll_z += speed;
        }

        /* Update rotation parameters with new scroll position */
        update_scroll_params();

        ++frame;
    }

    return 0;
}
