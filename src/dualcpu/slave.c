#include "saturn/shared.h"
#include "saturn/dualcpu.h"
#include "saturn/matrix.h"
#include "saturn/vector.h"
#include "saturn/vdp1.h"

extern SharedData shared;

static Vec3 temp_verts[256];
static Vdp1Cmd cmd_buffer[128];
static u32 cmd_index = 0;

static void transform_vertices(const Vertex* verts, u32 count, const Mat4* transform) {
    for (u32 i = 0; i < count; i++) {
        vec3_transform(&verts[i].position, transform, &temp_verts[i]);
    }
}

static void build_polygon_cmd(u32 idx0, u32 idx1, u32 idx2, u16 color) {
    Vdp1Cmd* cmd = &cmd_buffer[cmd_index++];
    
    cmd->ctrl = VDP1_CMD_POLYGON;
    cmd->pmode = 0;
    cmd->color = color;
    cmd->x1 = temp_verts[idx0].x >> 16;
    cmd->y1 = temp_verts[idx0].y >> 16;
    cmd->x2 = temp_verts[idx1].x >> 16;
    cmd->y2 = temp_verts[idx1].y >> 16;
    cmd->x3 = temp_verts[idx2].x >> 16;
    cmd->y3 = temp_verts[idx2].y >> 16;
    cmd->link = 0;
}

__attribute__((section(".slave_code")))
void slave_main() {
    volatile Vdp1Cmd* vdp1_cmd = (volatile Vdp1Cmd*)(VDP1_VRAM + 0x20);
    volatile u32* state = UNCACHED(&shared.state);
    
    Mat4 model_view, projection, mvp;
    
    while (1) {
        while (*state == SHARED_STATE_MASTER_WRITING);
        
        mat4_perspective(FIX16_ONE >> 1, FIX16_ONE * 320 / FIX16_ONE * 224, FIX16_ONE >> 2, FIX16_ONE << 4, &projection);
        mat4_identity(&model_view);
        mat4_rotate_y(&model_view, shared.input.x << 8, &model_view);
        mat4_mul(&projection, &model_view, &mvp);
        
        cmd_index = 0;
        
        const Quad* quads = (const Quad*)0x06010000;
        u32 quad_count = *((u32*)0x06010000 + 4096);
        
        for (u32 i = 0; i < quad_count; i++) {
            transform_vertices(quads[i].vertices, 4, &mvp);
            build_polygon_cmd(0, 1, 2, 0xFFFF);
            build_polygon_cmd(0, 2, 3, 0xFFFF);
        }
        
        vdp1_cmd->link = 0x8000;
        for (u32 i = 0; i < cmd_index; i++) {
            *vdp1_cmd++ = cmd_buffer[i];
        }
        vdp1_cmd[-1].link = 0x8000;
        
        *state = SHARED_STATE_MASTER_WRITING;
    }
}
