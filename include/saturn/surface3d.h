#ifndef SATURN_SURFACE3D_H
#define SATURN_SURFACE3D_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sat_surface3d_rect {
    int32_t min_x, max_x, min_z, max_z;
} sat_surface3d_rect_t;

/* A bounded planar deck. tilt_z is the signed height difference at +Z from
 * the hinge; holes are world-space apertures. */
typedef struct sat_surface3d_desc {
    int32_t center_x, center_z, base_y;
    int32_t half_x, half_z;
    int32_t tilt_z;
    const sat_surface3d_rect_t* holes;
    uint8_t hole_count;
} sat_surface3d_desc_t;

sat_result_t sat_surface3d_height(const sat_surface3d_desc_t* surface,
    int32_t x, int32_t z, int32_t* out_y);
uint8_t sat_surface3d_split(const sat_surface3d_desc_t* surface,
    sat_surface3d_rect_t out_slices[4]);
uint8_t sat_surface3d_supports_footprint(const sat_surface3d_desc_t* surface,
    int32_t x, int32_t z, int32_t half_x, int32_t half_z);

#ifdef __cplusplus
}
#endif

#endif
