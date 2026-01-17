#ifndef SATURN_VDP1_H
#define SATURN_VDP1_H

#include "saturn/types.h"
#include "saturn/shared.h"

typedef enum {
    VDP1_CMD_NORMAL_SPRITE = 0,
    VDP1_CMD_SCALED_SPRITE,
    VDP1_CMD_DISTORTED_SPRITE,
    VDP1_CMD_POLYGON,
    VDP1_CMD_POLYLINE,
    VDP1_CMD_LINE,
    VDP1_CMD_USER_CLIPPING,
    VDP1_CMD_SYSTEM_CLIPPING,
    VDP1_CMD_CHANGE_PRIORITY,
    VDP1_CMD_LOCAL_COORD
} Vdp1CommandType;

typedef enum {
    VDP1_MODE_4BPP = 0,
    VDP1_MODE_8BPP = 1,
    VDP1_MODE_16BPP = 2,
    VDP1_MODE_RGB = 3
} Vdp1ColorMode;

void vdp1_init(void);
void vdp1_wait_for_vblank(void);
void vdp1_start_frame(void);
void vdp1_end_frame(void);
void vdp1_clear_screen(u16 color);

Vdp1Cmd* vdp1_allocate_cmd(void);
void vdp1_submit_cmd(Vdp1Cmd* cmd);
void vdp1_flush_cmd_list(void);

void vdp1_draw_quad(const Vdp1Cmd* cmd);
void vdp1_draw_polygon(const Vdp1Cmd* cmd);
void vdp1_draw_sprite(const Vdp1Cmd* cmd);

void vdp1_set_clipping(s16 x1, s16 y1, s16 x2, s16 y2);

#endif
