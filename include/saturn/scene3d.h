#ifndef SATURN_SCENE3D_H
#define SATURN_SCENE3D_H

#include <stdint.h>

#include "saturn/model3d.h"

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

/* Immediate model draw parameters. All arrays remain caller-owned and are
 * scratch or immutable generated data; the facade does not retain pointers. */
typedef struct sat_scene3d_model_params {
    const sat_vdp1_texture_t* textures;
    uint16_t texture_count;
    uint16_t color;
    sat_fx16_t ambient;
    uint16_t flags;
    const uint16_t* face_colors;
    const uint16_t* vertex_gouraud;
} sat_scene3d_model_params_t;

/* One immediate-mode scene context. The mesh and scratch arrays are supplied
 * by the caller, so one context can draw many models sequentially. */
typedef struct sat_scene3d {
    sat_camera3d_t camera;
    sat_mesh_t mesh;
    uint16_t vertex_cap;
    uint16_t face_cap;
    uint8_t* order;
    uint16_t* order16;
    uint32_t* depth;
    sat_projected_vertex_t* screen;
    uint8_t active;
} sat_scene3d_t;

sat_result_t sat_scene3d_init(
    sat_scene3d_t* scene,
    sat_vec3_t* vertex_storage,
    uint16_t vertex_cap,
    uint16_t* index_storage,
    uint16_t face_cap,
    uint8_t* order,
    uint16_t* order16,
    uint32_t* depth,
    sat_projected_vertex_t* screen
);
sat_result_t sat_scene3d_begin(sat_scene3d_t* scene, const sat_camera3d_t* camera);
sat_result_t sat_scene3d_draw_model(
    sat_scene3d_t* scene,
    const sat_model_asset_t* asset,
    const sat_model_transform3d_t* transform,
    const sat_scene3d_model_params_t* params
);
sat_result_t sat_scene3d_end(sat_scene3d_t* scene);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_SCENE3D_H */
