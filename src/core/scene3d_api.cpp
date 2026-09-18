#include "saturn/scene3d.h"

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

extern "C" sat_result_t sat_scene3d_init(
    sat_scene3d_t* scene,
    sat_vec3_t* vertex_storage,
    uint16_t vertex_cap,
    uint16_t* index_storage,
    uint16_t face_cap,
    uint8_t* order,
    uint16_t* order16,
    uint32_t* depth,
    sat_projected_vertex_t* screen
) {
    if (scene == nullptr || vertex_storage == nullptr || index_storage == nullptr ||
        vertex_cap == 0u || face_cap == 0u) return SAT_ERR_INVALID_ARG;
    *scene = {};
    SAT_TRY(sat_mesh_init(
        &scene->mesh, vertex_storage, vertex_cap, index_storage, face_cap));
    scene->vertex_cap = vertex_cap;
    scene->face_cap = face_cap;
    scene->order = order;
    scene->order16 = order16;
    scene->depth = depth;
    scene->screen = screen;
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_begin(
    sat_scene3d_t* scene,
    const sat_camera3d_t* camera
) {
    if (scene == nullptr || camera == nullptr) return SAT_ERR_INVALID_ARG;
    scene->camera = *camera;
    scene->active = 1u;
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_draw_model(
    sat_scene3d_t* scene,
    const sat_model_asset_t* asset,
    const sat_model_transform3d_t* transform,
    const sat_scene3d_model_params_t* params
) {
    if (scene == nullptr || asset == nullptr || transform == nullptr || params == nullptr ||
        scene->active == 0u) return SAT_ERR_INVALID_ARG;
    SAT_TRY(sat_model_copy_to_mesh(asset, &scene->mesh));
    sat_mat4_t model{};
    SAT_TRY(sat_model_transform3d_matrix(transform, &model));
    SAT_TRY(sat_mesh_transform(&scene->mesh, &model));

    sat_mesh_draw_t draw{};
    SAT_TRY(sat_model_bind_draw_ex(
        asset, &scene->mesh, params->textures, params->texture_count,
        &scene->camera.view_proj, &scene->camera.eye, params->color,
        params->face_colors, params->ambient, params->flags, scene->order,
        scene->order16, scene->depth, &draw));
    draw.screen = scene->screen;
    draw.vertex_gouraud = params->vertex_gouraud;
    return sat_draw_mesh(&scene->mesh, &draw);
}

extern "C" sat_result_t sat_scene3d_end(sat_scene3d_t* scene) {
    if (scene == nullptr || scene->active == 0u) return SAT_ERR_INVALID_ARG;
    scene->active = 0u;
    return SAT_OK;
}
