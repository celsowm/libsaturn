#include "saturn/scene3d.h"
#include "src/core/scene3d_queue_logic.hpp"
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

/* All scene painter storage belongs to the caller. The immediate-mode
 * sat_scene3d_draw_model implementation above remains unchanged. */
extern "C" sat_result_t sat_scene3d_queue_init(
    sat_scene3d_queue_t* queue,
    sat_scene3d_queue_item_t* storage,
    uint16_t capacity) {
    if (!queue || !storage || capacity == 0u) return SAT_ERR_INVALID_ARG;
    *queue = {};
    queue->items = storage;
    queue->capacity = capacity;
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_queue_begin(
    sat_scene3d_queue_t* queue, const sat_camera3d_t* camera) {
    if (!queue || !queue->items || !queue->capacity || !camera ||
        queue->active || queue->flushing) return SAT_ERR_INVALID_ARG;
    sat_vec3_t delta = {
        static_cast<sat_fx16_t>(camera->target.x-camera->eye.x),
        static_cast<sat_fx16_t>(camera->target.y-camera->eye.y),
        static_cast<sat_fx16_t>(camera->target.z-camera->eye.z)
    };
    if (delta.x==0 && delta.y==0 && delta.z==0) return SAT_ERR_INVALID_ARG;
    queue->camera = *camera;
    sat_vec3_normalize(&queue->forward,&delta);
    queue->count=0u;
    queue->active=1u;
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_queue_depth(
    const sat_scene3d_queue_t* queue, const sat_vec3_t* position,
    sat_fx16_t* out_depth) {
    if (!queue || !queue->active || queue->flushing || !position ||
        !out_depth) return SAT_ERR_INVALID_ARG;
    const int64_t depth=(
        (static_cast<int64_t>(position->x)-queue->camera.eye.x)*queue->forward.x+
        (static_cast<int64_t>(position->y)-queue->camera.eye.y)*queue->forward.y+
        (static_cast<int64_t>(position->z)-queue->camera.eye.z)*queue->forward.z
    ) / SAT_FX16_ONE;
    *out_depth=depth>INT32_MAX?INT32_MAX:
               (depth<INT32_MIN?INT32_MIN:static_cast<sat_fx16_t>(depth));
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_queue_submit_draw(
    sat_scene3d_queue_t* queue, const sat_vec3_t* center,
    uint16_t pass, sat_scene3d_draw_fn draw, void* user) {
    if (!queue || !queue->active || queue->flushing || !center || !draw)
        return SAT_ERR_INVALID_ARG;
    if (queue->count>=queue->capacity) return SAT_ERR_CAPACITY;
    sat_scene3d_queue_item_t& item=queue->items[queue->count];
    item = {};
    item.center=*center;
    item.depth=saturn::core::scene3d_queue::depth_raw(queue->camera,*center);
    item.pass=pass;
    item.submission=queue->count++;
    item.type=1u;
    item.draw=draw;
    item.user=user;
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_queue_submit_model(
    sat_scene3d_queue_t* queue, const sat_vec3_t* center_local,
    uint16_t pass, sat_scene3d_t* model_scene,
    const sat_model_asset_t* model,
    const sat_model_transform3d_t* transform,
    const sat_scene3d_model_params_t* params) {
    if (!queue || !queue->active || queue->flushing || !center_local ||
        !model_scene || !model || !transform || !params || !model_scene->mesh.vertices)
        return SAT_ERR_INVALID_ARG;
    if (queue->count>=queue->capacity) return SAT_ERR_CAPACITY;
    if (model_scene->active) return SAT_ERR_BUSY;
    sat_mat4_t matrix{};
    SAT_TRY(sat_model_transform3d_matrix(transform,&matrix));
    const sat_vec4_t local={center_local->x,center_local->y,center_local->z,SAT_FX16_ONE};
    sat_vec4_t world{};
    SAT_TRY(sat_mat4_transform_vec4(&matrix,&local,&world));
    const sat_vec3_t center={world.x,world.y,world.z};
    sat_scene3d_queue_item_t& item=queue->items[queue->count];
    item = {};
    item.center=center;
    item.depth=saturn::core::scene3d_queue::depth_raw(queue->camera,center);
    item.pass=pass;
    item.submission=queue->count++;
    item.type=2u;
    item.model_scene=model_scene;
    item.model=model;
    item.transform=*transform;
    item.params=*params;
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_queue_flush(sat_scene3d_queue_t* queue) {
    if (!queue || !queue->active || queue->flushing) return SAT_ERR_INVALID_ARG;
    queue->flushing=1u;
    saturn::core::scene3d_queue::sort(queue->items,queue->count);
    sat_result_t result=SAT_OK;
    for (uint16_t i=0u;i<queue->count;++i) {
        const sat_scene3d_queue_item_t& item=queue->items[i];
        if (item.type==1u) {
            result=item.draw(item.user,&queue->camera);
        } else if(item.type==2u) {
            result=sat_scene3d_begin(item.model_scene,&queue->camera);
            if(result==SAT_OK) {
                result=sat_scene3d_draw_model(
                    item.model_scene,item.model,&item.transform,&item.params);
                /* The model scratch scene must not leak its active state,
                 * including when a VDP1 command capacity error occurs. */
                const sat_result_t end_result=sat_scene3d_end(item.model_scene);
                if(result==SAT_OK) result=end_result;
            }
        } else result=SAT_ERR_INVALID_ARG;
        if(result!=SAT_OK) break;
    }
    queue->active=0u;
    queue->flushing=0u;
    queue->count=0u;
    return result;
}
