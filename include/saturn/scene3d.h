#ifndef SATURN_SCENE3D_H
#define SATURN_SCENE3D_H

#include <stdint.h>

#include "saturn/math3d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Caller-owned camera state. The derived view-projection matrix is updated by
 * sat_camera3d_update and is consumed by the scene draw facade. */
typedef struct sat_camera3d {
    sat_vec3_t eye;
    sat_vec3_t target;
    sat_vec3_t up;
    sat_fx16_t fov_y;
    sat_fx16_t aspect;
    sat_fx16_t near_z;
    sat_fx16_t far_z;
    sat_mat4_t view_proj;
} sat_camera3d_t;

sat_result_t sat_camera3d_init(
    sat_camera3d_t* out,
    const sat_vec3_t* eye,
    const sat_vec3_t* target,
    const sat_vec3_t* up,
    sat_fx16_t fov_y,
    sat_fx16_t aspect,
    sat_fx16_t near_z,
    sat_fx16_t far_z
);
sat_result_t sat_camera3d_update(sat_camera3d_t* camera);

typedef struct sat_model_transform3d {
    sat_vec3_t position;
    sat_vec3_t rotation_deg;
    sat_vec3_t scale;
} sat_model_transform3d_t;

void sat_model_transform3d_identity(sat_model_transform3d_t* out);
sat_result_t sat_model_transform3d_matrix(
    const sat_model_transform3d_t* transform,
    sat_mat4_t* out
);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_SCENE3D_H */
