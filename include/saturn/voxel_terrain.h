#ifndef SATURN_VOXEL_TERRAIN_H
#define SATURN_VOXEL_TERRAIN_H

#include <stdint.h>
#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Height-field renderer (Voxel Space), NOT arbitrary 3D volumetric voxels.
 * Heights and colors are indexed by [z * pitch + x]. Colors are INDEX8
 * palette indices; the caller owns the palette and the presentation backend.
 * height_scale multiplies each height sample in integer world units, 1..16.
 * No hidden allocation and no hardware/VDP access in the renderer. */
typedef enum sat_voxel_edge_mode {
    SAT_VOXEL_EDGE_CLAMP = 0,
    SAT_VOXEL_EDGE_WRAP = 1
} sat_voxel_edge_mode_t;

typedef struct sat_voxel_terrain {
    const uint8_t* heights;
    const uint8_t* colors;
    uint16_t width;
    uint16_t height;
    uint16_t pitch;
    uint8_t height_scale;
    uint8_t edge_mode;
} sat_voxel_terrain_t;

/* Camera x,y,z in 16.16 world units; angle in 16.16 degrees: 0 looks +Z,
 * 90 looks +X. half_fov is the lateral/forward ray ratio (0..65536),
 * not an angle; SAT_FX16_ONE = approximately 90-degree horizontal FOV.
 * pitch_pixels shifts the horizon downward when positive. projection_scale
 * is the number of screen pixels per world unit at a distance of one.
 * The first baseline traverses every distance step, 1..view_distance. */
typedef struct sat_voxel_camera {
    sat_fx16_t x;
    sat_fx16_t y;
    sat_fx16_t z;
    sat_fx16_t angle;
    sat_fx16_t half_fov;
    int16_t pitch_pixels;
    uint16_t projection_scale;
    uint16_t view_distance;
} sat_voxel_camera_t;

/* Entire target is overwritten, including sky. pitch may exceed width.
 * sky_index is an opaque INDEX8 palette index chosen by the caller.
 * For VDP1 sprites, avoid palette index 0 unless SPD/OPAQUE is enabled. */
typedef struct sat_voxel_target {
    uint8_t* pixels;
    uint16_t width;
    uint16_t height;
    uint16_t pitch;
    uint8_t sky_index;
} sat_voxel_target_t;

/* Returns required scratch bytes for scale[distance+1] and ceiling[width].
 * Scratch must be 4-byte aligned, and must not overlap any map/target.
 * Returns zero for unsupported dimensions (width 1..512, distance 1..512). */
uint32_t sat_voxel_terrain_scratch_bytes(uint16_t width, uint16_t view_distance);

/* Pure CPU rendering; does not initialize LibSaturn or access Saturn MMIO.
 * Returns INVALID_ARG for invalid buffers/config, CAPACITY for short scratch.
 * camera y is restricted to 0..4096 world units for bounded projection math.
 * Caller must provide valid readable/writable storage for stated dimensions. */
sat_result_t sat_voxel_terrain_render(
    const sat_voxel_terrain_t* terrain,
    const sat_voxel_camera_t* camera,
    const sat_voxel_target_t* target,
    void* scratch,
    uint32_t scratch_bytes
);

/* Returns height in 16.16 units for the terrain's CLAMP/WRAP edge policy.
 * No hardware access; useful for collision and camera-ground clearance. */
sat_result_t sat_voxel_terrain_height_at(
    const sat_voxel_terrain_t* terrain,
    sat_fx16_t world_x,
    sat_fx16_t world_z,
    sat_fx16_t* out_height
);

#ifdef __cplusplus
}
#endif
#endif /* SATURN_VOXEL_TERRAIN_H */
