/* vdp2_infinite_plan - RBG0 rotation plane (The Polished One) */
#include <stdint.h>
#include "saturn/saturn.h"
#include "vdp2_infinite_plan/seamless_sea.h"

#define VDP2_REG(off) (*(volatile uint16_t*)(0x25F80000u + (off)))
#define VRAM_WORD(w)  (((volatile uint16_t*)0x25E00000u)[w])

int main(void) {
    sat_video_config_t cfg;
    sat_pad_state_t pad;
    int32_t sx = 0, sz = 0;
    uint32_t i;

    cfg.width = 320; cfg.height = 224; cfg.ntsc = 1; cfg.reserved = 0;
    sat_init(&cfg);
    
    sat_vdp2_palette_upload(seamless_sea_asset.palette, 256, 0);

    /* 1. Character Data in Bank A0 (Word 0x00000) */
    sat_vdp2_vram_write_words(0x00000, (const uint16_t*)seamless_sea_asset.pixels, 32768);

    /* 2. Map Data in Bank B0 (Word 0x20000) */
    for(i=0; i<1024; i++) {
        uint16_t tx = (uint16_t)(i % 32);
        uint16_t ty = (uint16_t)(i / 32);
        VRAM_WORD(0x20000 + i) = (uint16_t)(ty * 32 + tx);
    }

    /* 3. Memory Partitioning */
    VDP2_REG(0x000E) = 0x013F; /* ALL (A, B) assigned to RBG0 */
    VDP2_REG(0x0010) = 0x5E5E; VDP2_REG(0x0012) = 0x5E5E; /* A0: Char */
    VDP2_REG(0x0014) = 0xCCCC; VDP2_REG(0x0016) = 0xCCCC; /* A1: Param */
    VDP2_REG(0x0018) = 0x4E4E; VDP2_REG(0x001A) = 0x4E4E; /* B0: Map */

    /* 4. RBG0 Screen Config */
    VDP2_REG(0x002A) = 0x0001; /* CHCTLB: 256 col, 1x1 cell size */
    VDP2_REG(0x0038) = 0x0000; /* PNCNTB: 1-word PN */
    VDP2_REG(0x003A) = 0x0040; /* PLSZ: 1x1 Plane, Repeat mode ON */
    VDP2_REG(0x0050) = 0x0000; /* Map Page 0 in B0 */
    
    VDP2_REG(0x00B8) = 0x0001; /* RPTA0 Word Offset 0x10000 (A1) */
    VDP2_REG(0x00BA) = 0x0000;
    VDP2_REG(0x00B0) = 0x0000; /* RPMD: Mode A */
    VDP2_REG(0x00FC) = 0x0007;

    /* 5. Matrix Initial: Identity */
    VRAM_WORD(0x10000 + 4) = 0x0000; /* dXst */
    VRAM_WORD(0x10000 + 5) = 0x0100; /* dYst */
    VRAM_WORD(0x10000 + 6) = 0x0100; /* dX */
    VRAM_WORD(0x10000 + 7) = 0x0000; /* dY */

    VDP2_REG(0x0020) = 0x0010; /* RBG0 EN */

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
