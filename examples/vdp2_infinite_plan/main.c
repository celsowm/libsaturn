/* vdp2_infinite_plan - RBG0 rotation plane (Scale Fix) */
#include <stdint.h>
#include "saturn/saturn.h"
#include "vdp2_infinite_plan/seamless_sea.h"

#define VDP2_REG(off) (*(volatile uint16_t*)(0x25F80000u + (off)))
#define VRAM_WORD(word_off) (((volatile uint16_t*)0x25E00000u)[word_off])

int main(void) {
    sat_video_config_t cfg;
    sat_pad_state_t pad;
    int32_t sx = 0, sz = 0;
    uint32_t i;

    cfg.width = 320; cfg.height = 224; cfg.ntsc = 1; cfg.reserved = 0;
    sat_init(&cfg);
    
    sat_vdp2_palette_upload(seamless_sea_asset.palette, 256, 0);
    sat_vdp2_vram_write_words(0, (const uint16_t*)seamless_sea_asset.pixels, 32768);

    for(i=0; i<4096; i++) {
        uint16_t tx = (uint16_t)((i % 64) % 32);
        uint16_t ty = (uint16_t)((i / 64) % 32);
        VRAM_WORD(0x20000 + i) = (ty * 32 + tx);
    }

    VDP2_REG(0x000E) = 0x1127; /* RAMCTL */
    VDP2_REG(0x0010) = 0x5E5E; VDP2_REG(0x0012) = 0x5E5E;
    VDP2_REG(0x0014) = 0xCCCC; VDP2_REG(0x0016) = 0xCCCC;
    VDP2_REG(0x0018) = 0x4E4E; VDP2_REG(0x001A) = 0x4E4E;
    VDP2_REG(0x002A) = 0x1000;
    VDP2_REG(0x0038) = 0x8000;
    VDP2_REG(0x003A) = 0x000C;
    VDP2_REG(0x0050) = 0x2120; VDP2_REG(0x0052) = 0x2322;
    VDP2_REG(0x0054) = 0x2524; VDP2_REG(0x0056) = 0x2726;
    VDP2_REG(0x0058) = 0x2928; VDP2_REG(0x005A) = 0x2B2A;
    VDP2_REG(0x005C) = 0x2D2C; VDP2_REG(0x005E) = 0x2F2E;
    VDP2_REG(0x00B8) = 0x0001; 
    VDP2_REG(0x00BA) = 0x0000;
    VDP2_REG(0x00B0) = 0x0000; 
    VDP2_REG(0x00FC) = 0x0007; /* Set MAX priority for RBG0 */

    /* Identity Matrix (1.0 = 0x0100 for 8.8 format) */
    VRAM_WORD(0x10000 + 4) = 0;      /* dXst */
    VRAM_WORD(0x10000 + 5) = 0x0100; /* dYst */
    VRAM_WORD(0x10000 + 6) = 0x0100; /* dX */
    VRAM_WORD(0x10000 + 7) = 0;      /* dY */

    VDP2_REG(0x0020) = 0x0010;

    while(1) {
        sat_app_frame_begin(0, 0, &pad);
        sat_pad_poll(&pad);
        if (pad.pressed & SAT_PAD_START) break;
        if (pad.held & SAT_PAD_LEFT)  sx -= 65536;
        if (pad.held & SAT_PAD_RIGHT) sx += 65536;
        if (pad.held & SAT_PAD_UP)    sz -= 65536;
        if (pad.held & SAT_PAD_DOWN)  sz += 65536;

        VRAM_WORD(0x10000 + 0) = (uint16_t)((sx >> 16) & 0x1FFF);
        VRAM_WORD(0x10000 + 1) = (uint16_t)(sx & 0xFFFF);
        VRAM_WORD(0x10000 + 2) = (uint16_t)((sz >> 16) & 0x1FFF);
        VRAM_WORD(0x10000 + 3) = (uint16_t)(sz & 0xFFFF);

        sat_end_frame();
    }
    return 0;
}
