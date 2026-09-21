#ifndef P3D_CAMERA_H
#define P3D_CAMERA_H

/* The board camera: one of P3D_CAM_ANGLES fixed positions around the maze,
 * turned with L and R. */

#include <stdint.h>

#include "saturn/input.h"
#include "saturn/math3d.h"
#include "saturn/scene3d.h"

typedef struct p3d_camera {
    uint16_t angle;
    /* Trig of the heading, kept because billboards (the ghosts' faces) and
     * the actors' key light have to face the camera, which stops being a
     * constant direction once it can turn. */
    sat_fx16_t sin_az;
    sat_fx16_t cos_az;
    sat_vec3_t eye;
    sat_camera3d_t view;
} p3d_camera_t;

/* Points the camera at the board from `angle`. */
void p3d_camera_set(p3d_camera_t* camera, uint16_t angle);

/* L and R turn one step, on the press. Returns non-zero when it turned. */
int p3d_camera_update(p3d_camera_t* camera, const sat_pad_state_t* pad);

#endif /* P3D_CAMERA_H */
