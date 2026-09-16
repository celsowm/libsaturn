#ifndef INFINITE_EXPLORER_LOGIC_H
#define INFINITE_EXPLORER_LOGIC_H

#include <stdint.h>

#define EXPLORER_CHUNK_SIZE 256
#define EXPLORER_HORIZON 96
#define EXPLORER_FOCAL 96
#define EXPLORER_CAMERA_HEIGHT 34

typedef struct explorer_pos {
    int32_t chunk_x, chunk_z;
    int32_t local_x, local_z; /* 16.16 within the current chunk */
} explorer_pos_t;

typedef struct explorer_projection {
    int16_t x, ground_y;
    int16_t size;
    int32_t depth;
    uint8_t visible;
} explorer_projection_t;

static inline uint32_t explorer_hash(uint32_t seed, int32_t x, int32_t z) {
    uint32_t h = seed ^ ((uint32_t)x * 0x9E3779B9u) ^ ((uint32_t)z * 0x85EBCA6Bu);
    h ^= h >> 16; h *= 0x7FEB352Du; h ^= h >> 15; h *= 0x846CA68Bu; h ^= h >> 16;
    return h;
}

static inline uint8_t explorer_biome(uint32_t seed, int32_t chunk_x, int32_t chunk_z) {
    int32_t region_x = chunk_x >= 0 ? chunk_x / 3 : (chunk_x - 2) / 3;
    int32_t region_z = chunk_z >= 0 ? chunk_z / 3 : (chunk_z - 2) / 3;
    return (uint8_t)(explorer_hash(seed ^ 0xB10B10u, region_x, region_z) % 3u);
}

static inline void explorer_rebase(explorer_pos_t* p) {
    const int32_t span = EXPLORER_CHUNK_SIZE << 16;
    while (p->local_x >= span) { p->local_x -= span; ++p->chunk_x; }
    while (p->local_x < 0)     { p->local_x += span; --p->chunk_x; }
    while (p->local_z >= span) { p->local_z -= span; ++p->chunk_z; }
    while (p->local_z < 0)     { p->local_z += span; --p->chunk_z; }
}

static inline int32_t explorer_relative_fx(int32_t object_chunk, int32_t object_local,
                                            int32_t player_chunk, int32_t player_local) {
    return ((object_chunk - player_chunk) * (EXPLORER_CHUNK_SIZE << 16)) +
           object_local - player_local;
}

static inline explorer_projection_t explorer_project(int32_t dx, int32_t dz,
                                                       int32_t sin_h, int32_t cos_h) {
    explorer_projection_t p = {0, 0, 0, 0, 0};
    int32_t side = (int32_t)(((int64_t)dx * cos_h - (int64_t)dz * sin_h) >> 16);
    int32_t depth = (int32_t)(((int64_t)dx * sin_h + (int64_t)dz * cos_h) >> 16);
    int32_t di = depth >> 16;
    if (di < 10 || di > 1150) return p;
    int32_t sx = 160 + (int32_t)(((int64_t)(side >> 8) * EXPLORER_FOCAL) / (depth >> 8));
    int32_t gy = EXPLORER_HORIZON + (EXPLORER_CAMERA_HEIGHT * EXPLORER_FOCAL) / di;
    if (sx < -40 || sx > 360 || gy < EXPLORER_HORIZON || gy > 230) return p;
    p.x = (int16_t)sx;
    p.ground_y = (int16_t)gy;
    p.size = (int16_t)(768 / di + 2);
    if (p.size > 38) p.size = 38;
    p.depth = depth;
    p.visible = 1;
    return p;
}

static inline void explorer_write_fx_pair(uint16_t out[48], uint32_t at, int32_t value) {
    out[at] = (uint16_t)((uint32_t)value >> 16);
    out[at + 1u] = (uint16_t)value;
}

/* Builds the same Mode-7 table as vdp2_rbg0_ground, with a yaw matrix.
 * sin_h/cos_h are 16.16, so the ground turns around the camera instead of
 * merely sliding sideways. */
static inline void explorer_build_ground_params(int32_t cam_x, int32_t cam_z,
                                                 int32_t sin_h, int32_t cos_h,
                                                 uint32_t coef_base_word,
                                                 uint16_t out[48]) {
    int i;
    for (i = 0; i < 48; ++i) out[i] = 0;
    out[2] = EXPLORER_HORIZON + EXPLORER_FOCAL;
    out[10] = 1; /* horizontal screen step */
    explorer_write_fx_pair(out, 14, cos_h);  /* A */
    explorer_write_fx_pair(out, 16, sin_h);  /* B */
    explorer_write_fx_pair(out, 20, -sin_h); /* D */
    explorer_write_fx_pair(out, 22, cos_h);  /* E */
    out[26] = 160;
    out[27] = EXPLORER_HORIZON;
    out[30] = 160;
    out[31] = EXPLORER_HORIZON;
    out[34] = (uint16_t)(((cam_x >> 16) & 511) - 160);
    out[36] = (uint16_t)(((cam_z >> 16) & 255) - EXPLORER_HORIZON);
    out[38] = 1; out[40] = 1;
    out[42] = (uint16_t)((coef_base_word * 2u) / 4u);
    out[44] = 1;
}

#endif
