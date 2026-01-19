#include "saturn/vdp2.h"
#include "saturn/hardware.h"

void vdp2_init(void) {
    VDP2_TVMD = 0x0000;
    VDP2_RAMCTL = 0x0000;

    VDP2_CYCA0L = 0x4444;
    VDP2_CYCA0U = 0xFFFF;
    VDP2_CYCA1L = 0x4444;
    VDP2_CYCA1U = 0xFFFF;
    VDP2_CYCB0L = 0xFFFF;
    VDP2_CYCB0U = 0xFFFF;
    VDP2_CYCB1L = 0xFFFF;
    VDP2_CYCB1U = 0xFFFF;

    VDP2_CHCTLA = 0x000E; // NBG0 bitmap, 512x256, 16-bit direct color.
    VDP2_BMPNA = 0x0000;
    VDP2_MPOFN = 0x0000;
    VDP2_PRISA = 0x0007;
    VDP2_BGON = 0x0001;

    VDP2_TVMD = 0x8000;
}

void vdp2_set_bg_mode(Vdp2BgMode mode) {
    VDP2_TVMD = (VDP2_TVMD & 0xFFF8) | (u16)mode;
}

void vdp2_enable_bg(Vdp2BgLayer layer) {
    VDP2_BGON |= (1 << (u32)layer);
}

void vdp2_disable_bg(Vdp2BgLayer layer) {
    VDP2_BGON &= ~(1 << (u32)layer);
}

void vdp2_set_bg_config(Vdp2BgLayer layer, const Vdp2BgConfig* config) {
    (void)layer;
    (void)config;
}

void vdp2_set_bg_scroll(Vdp2BgLayer layer, s16 x, s16 y) {
    (void)layer;
    (void)x;
    (void)y;
}

void vdp2_wait_for_vblank(void) {
    while (!(VDP2_TVSTAT & 0x0008));
}
