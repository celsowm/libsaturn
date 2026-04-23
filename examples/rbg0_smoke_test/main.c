/* rbg0_smoke_test - Minimal RBG0 bitmap test */
#include <stdint.h>
#include "saturn/saturn.h"

#define BITMAP_WIDTH  512
#define BITMAP_HEIGHT 256
#define BITMAP_BASE_WORD  0x00000u
#define ROT_PARAM_BASE    0x10000u

int main(void) {
    sat_video_config_t cfg = {320, 224, 1, 0};
    volatile uint16_t* vram = (volatile uint16_t*)(0x20000000u | 0x05E00000u);
    uint32_t i;

    sat_init(&cfg);

    /* Upload solid blue bitmap (palette index 1 = blue) */
    for (i = 0; i < (BITMAP_WIDTH * BITMAP_HEIGHT) / 2; i++) {
        vram[BITMAP_BASE_WORD + i] = 0x0101;  /* Two blue pixels per word */
    }

    /* Upload palette: 0 = black, 1 = blue */
    sat_vdp2_palette_upload((const uint16_t[]){0x0000, 0x7C00}, 2, 0);

    /* Zero rotation parameter table */
    for (i = 0; i < 48; i++) {
        vram[ROT_PARAM_BASE + i] = 0x0000;
    }

    /* Setup identity rotation */
    sat_vdp2_rbg0_set_rotation_matrix(ROT_PARAM_BASE, 0, 0, 0);
    sat_vdp2_rbg0_set_scaling(ROT_PARAM_BASE, 0x10000, 0x10000);  /* 1.0 in fx16 */
    
    /* dXst=0, dYst=1: advance Y by 1 per scanline */
    sat_vdp2_rbg0_set_vertical_increments(ROT_PARAM_BASE, 0, 0, 1, 0);
    
    /* dX=1, dY=0: advance X by 1 per pixel */
    sat_vdp2_rbg0_set_coordinate_increments(ROT_PARAM_BASE, 1, 0, 0, 0);
    
    /* Scroll = 0 */
    sat_vdp2_rbg0_set_scroll(ROT_PARAM_BASE, 0, 0, 0, 0);

    /* Configure RBG0 */
    const sat_vdp2_rbg0_config_t rbg0_cfg = {
        SAT_VDP2_RBG0_BITMAP_512x256,
        SAT_VDP2_COLOR_MODE_256,
        BITMAP_BASE_WORD,
        ROT_PARAM_BASE
    };
    sat_vdp2_rbg0_init(&rbg0_cfg);
    sat_vdp2_rbg0_set_param_mode(SAT_VDP2_RBG0_PARAM_A);
    sat_vdp2_rbg0_set_enabled(1);

    /* Backdrop = green (so we know if RBG0 is transparent) */
    sat_vdp2_back_color_set(0x03E0);

    while (1) {
        sat_wait_vblank();
        /* Register writes outside VBlank are silently dropped by the VDP2.
         * Re-apply all RBG0 config registers during VBlank to ensure
         * they take effect. */
        sat_vdp2_rbg0_commit();
    }

    return 0;
}
