#include "saturn/vdp2.h"
#include "saturn/hardware.h"

void vdp2_init(void) {
    VDP2_TVMD = 0x8000;
    VDP2_TVMD = 0x8100;
}

void vdp2_set_bg_mode(Vdp2BgMode mode) {
    VDP2_TVMD = (VDP2_TVMD & 0xFFF8) | (u16)mode;
}

void vdp2_enable_bg(Vdp2BgLayer layer) {
    VDP2_EXTEN |= (1 << (u32)layer);
}

void vdp2_disable_bg(Vdp2BgLayer layer) {
    VDP2_EXTEN &= ~(1 << (u32)layer);
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
