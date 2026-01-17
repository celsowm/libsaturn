#include "saturn/vdp1.h"
#include "saturn/hardware.h"

static volatile Vdp1Cmd* cmd_list = (volatile Vdp1Cmd*)VDP1_VRAM;

void vdp1_init(void) {
    VDP1_FBCR = 0;
    VDP1_PTMR = 0;
    VDP1_EWDR = 0;
    VDP1_EWLR = 0;
    VDP1_EWRR = 0;
    VDP1_ENDR = 0;
}

void vdp1_wait_for_vblank(void) {
    while (!(VDP1_PTMR & 0x02));
}

void vdp1_start_frame(void) {
    VDP1_PTMR = 0;
}

void vdp1_end_frame(void) {
    VDP1_ENDR = 0x8000;
}

void vdp1_clear_screen(u16 color) {
    volatile u16* fb = (volatile u16*)0x25E00000;
    for (int i = 0; i < 320 * 224; i++) {
        fb[i] = color;
    }
}

Vdp1Cmd* vdp1_allocate_cmd(void) {
    return (Vdp1Cmd*)(cmd_list + 1);
}

void vdp1_submit_cmd(Vdp1Cmd* cmd) {
    *cmd_list = *cmd;
    cmd_list = cmd;
}

void vdp1_flush_cmd_list(void) {
    VDP1_FBCR = 0x02;
}

void vdp1_draw_quad(const Vdp1Cmd* cmd) {
    *cmd_list = *cmd;
    cmd->link = (u32)(cmd_list + 1);
    cmd_list++;
}

void vdp1_draw_polygon(const Vdp1Cmd* cmd) {
    Vdp1Cmd poly = *cmd;
    poly.ctrl = VDP1_CMD_POLYGON;
    *cmd_list = poly;
    cmd_list++;
}

void vdp1_draw_sprite(const Vdp1Cmd* cmd) {
    Vdp1Cmd sprite = *cmd;
    sprite.ctrl = VDP1_CMD_NORMAL_SPRITE;
    *cmd_list = sprite;
    cmd_list++;
}

void vdp1_set_clipping(s16 x1, s16 y1, s16 x2, s16 y2) {
    Vdp1Cmd clip;
    clip.ctrl = VDP1_CMD_SYSTEM_CLIPPING;
    clip.x1 = x1;
    clip.y1 = y1;
    clip.x2 = x2;
    clip.y2 = y2;
    vdp1_draw_quad(&clip);
}
