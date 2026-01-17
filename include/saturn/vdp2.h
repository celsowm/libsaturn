#ifndef SATURN_VDP2_H
#define SATURN_VDP2_H

#include "saturn/types.h"

typedef enum {
    BG_MODE_0 = 0,
    BG_MODE_1,
    BG_MODE_2,
    BG_MODE_3,
    BG_MODE_4,
    BG_MODE_5
} Vdp2BgMode;

typedef enum {
    BG_NBG0 = 0,
    BG_NBG1,
    BG_NBG2,
    BG_NBG3,
    BG_RBG0,
    BG_RBG1
} Vdp2BgLayer;

typedef struct {
    u16 map_base;
    u16 char_base;
    u16 palette_base;
} Vdp2BgConfig;

void vdp2_init(void);
void vdp2_set_bg_mode(Vdp2BgMode mode);
void vdp2_enable_bg(Vdp2BgLayer layer);
void vdp2_disable_bg(Vdp2BgLayer layer);
void vdp2_set_bg_config(Vdp2BgLayer layer, const Vdp2BgConfig* config);
void vdp2_set_bg_scroll(Vdp2BgLayer layer, s16 x, s16 y);
void vdp2_wait_for_vblank(void);

#endif
