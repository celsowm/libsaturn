#ifndef SATURN_VDP2_FLOOR_H
#define SATURN_VDP2_FLOOR_H

#include <stdint.h>
#include "saturn/core.h"
#include "saturn/math3d.h"
#include "saturn/vdp2_layers.h"

#ifdef __cplusplus
extern "C" {
#endif

/* An RBG0 map laid on a horizontal world plane and seen through a 3D camera
 * (look-at times perspective, GL convention: y up, the camera looks down -z).
 * The camera may pitch and yaw but not roll: every screen line is then a
 * floor line at one depth, which a per-line coefficient scales. */
typedef struct sat_vdp2_floor {
    uint16_t width, height;      /* screen size in dots */
    int32_t plane_y;             /* world height of the floor (16.16) */
    int32_t density;             /* map dots per world unit (16.16) */
    int32_t origin_x, origin_y;  /* map dot of world x = z = 0 (16.16) */
    int32_t period;              /* map repeat in dots (16.16), 0 none: keeps mx/my small */
    /* Optional far depth per group of `far_step` lines (16.16 along the view
     * axis, 0 for none), for a floor that ends at walls drawn on another
     * layer. NULL: far_z alone. */
    const sat_fx16_t* far_lines;
    uint16_t far_step;
} sat_vdp2_floor_t;

/* Rotation parameters and `floor->height` coefficients (as
 * sat_vdp2_rotation_coefficient makes them) that draw world point (x, z) of
 * the plane at map dot origin + density * (x, z). Lines above the horizon or
 * nearer than near_z or farther than far_z (or their far_lines entry) are
 * transparent. The caller sets
 * params->coefficient_address. SAT_ERR_UNSUPPORTED for a rolled camera or
 * one on or below the plane. */
sat_result_t sat_vdp2_floor_from_view(const sat_vdp2_floor_t* floor, const sat_mat4_t* view_proj,
                                      const sat_vec3_t* eye, sat_fx16_t near_z, sat_fx16_t far_z,
                                      sat_vdp2_rotation_params_t* params, uint32_t* coefficients);

#ifdef __cplusplus
}
#endif

#endif
