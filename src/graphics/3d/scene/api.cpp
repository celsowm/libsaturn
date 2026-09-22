#include "saturn/scene3d.h"
#include <stdint.h>

namespace {

sat_result_t compose_transform(const sat_model_transform3d_t& transform, sat_mat4_t* out) {
    sat_mat4_t scale{};
    sat_mat4_t rotate_z{};
    sat_mat4_t rotate_x{};
    sat_mat4_t rotate_y{};
    sat_mat4_t rotation{};
    sat_mat4_t translate{};
    sat_mat4_t tmp{};
    SAT_TRY(sat_mat4_scale(&scale, transform.scale.x, transform.scale.y, transform.scale.z));
    SAT_TRY(sat_mat4_rotate_z(&rotate_z, transform.rotation_deg.z));
    SAT_TRY(sat_mat4_rotate_x(&rotate_x, transform.rotation_deg.x));
    SAT_TRY(sat_mat4_rotate_y(&rotate_y, transform.rotation_deg.y));
    SAT_TRY(sat_mat4_multiply(&tmp, &rotate_y, &rotate_x));
    SAT_TRY(sat_mat4_multiply(&rotation, &tmp, &rotate_z));
    SAT_TRY(sat_mat4_multiply(&tmp, &rotation, &scale));
    SAT_TRY(sat_mat4_translate(
        &translate, transform.position.x, transform.position.y, transform.position.z));
    return sat_mat4_multiply(out, &translate, &tmp);
}

}  // namespace

extern "C" sat_result_t sat_camera3d_init(
    sat_camera3d_t* out,
    const sat_vec3_t* eye,
    const sat_vec3_t* target,
    const sat_vec3_t* up,
    sat_fx16_t fov_y,
    sat_fx16_t aspect,
    sat_fx16_t near_z,
    sat_fx16_t far_z
) {
    if (out == nullptr || eye == nullptr || target == nullptr || up == nullptr ||
        aspect <= 0 || near_z <= 0 || far_z <= near_z) return SAT_ERR_INVALID_ARG;
    out->eye = *eye;
    out->target = *target;
    out->up = *up;
    out->fov_y = fov_y;
    out->aspect = aspect;
    out->near_z = near_z;
    out->far_z = far_z;
    return sat_camera3d_update(out);
}

extern "C" sat_result_t sat_camera3d_update(sat_camera3d_t* camera) {
    if (camera == nullptr || camera->aspect <= 0 || camera->near_z <= 0 ||
        camera->far_z <= camera->near_z) return SAT_ERR_INVALID_ARG;
    sat_mat4_t view{};
    sat_mat4_t projection{};
    SAT_TRY(sat_mat4_look_at(&view, &camera->eye, &camera->target, &camera->up));
    SAT_TRY(sat_mat4_perspective(
        &projection, camera->fov_y, camera->aspect, camera->near_z, camera->far_z));
    return sat_mat4_multiply(&camera->view_proj, &projection, &view);
}

extern "C" void sat_model_transform3d_identity(sat_model_transform3d_t* out) {
    if (out == nullptr) return;
    out->position = {0, 0, 0};
    out->rotation_deg = {0, 0, 0};
    out->scale = {SAT_FX16_ONE, SAT_FX16_ONE, SAT_FX16_ONE};
}

extern "C" sat_result_t sat_model_transform3d_matrix(
    const sat_model_transform3d_t* transform,
    sat_mat4_t* out
) {
    if (transform == nullptr || out == nullptr) return SAT_ERR_INVALID_ARG;
    return compose_transform(*transform, out);
}
