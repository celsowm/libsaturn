#ifndef SATURN_SHARED_H
#define SATURN_SHARED_H

#include <stdint.h>
#include "saturn/types.h"

#define UNCACHED(ptr) ((void*)((uint32_t)(ptr) | 0x20000000))
#define VDP1_VRAM 0x25C00000
#define VDP2_VRAM 0x25E00000

typedef struct {
    volatile u32 state;
    struct { 
        s16 x, y; 
        u16 buttons;
    } input;
    u32 _padding[4];
} __attribute__((aligned(16))) SharedData;

typedef struct {
    u16 ctrl, link, pmode, color, char_addr, size;
    s16  x1, y1, x2, y2, x3, y3, x4, y4;
    u16 grp, reserved;
} __attribute__((packed)) Vdp1Cmd;

typedef struct {
    fix16_t x, y, z;
} Vec3;

typedef struct {
    fix16_t x, y;
} Vec2;

typedef struct {
    fix16_t m[4][4];
} Mat4;

typedef struct {
    Vec3 position;
    Vec2 uv;
} Vertex;

typedef struct {
    Vertex vertices[4];
    u16 texture_id;
} Quad;

#define SHARED_STATE_MASTER_WRITING 0
#define SHARED_STATE_SLAVE_WORKING  1

#endif
