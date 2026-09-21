#include <cassert>
#include <cstdio>

#include "saturn/scene.h"

static uint16_t g_replayed;

extern "C" sat_result_t sat_scene3d_faces_init(
    sat_scene3d_faces_t* scene, sat_scene3d_face_t* storage,
    uint32_t* keys, uint16_t* order, uint16_t capacity) {
    if (!scene || !storage || !keys || !order || !capacity)
        return SAT_ERR_INVALID_ARG;
    *scene = {};
    scene->entries = storage;
    scene->keys = keys;
    scene->order = order;
    scene->capacity = capacity;
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_faces_begin_camera(
    sat_scene3d_faces_t* scene, const sat_camera3d_t*, sat_fx16_t,
    uint16_t, uint16_t) {
    if (!scene || scene->active) return SAT_ERR_INVALID_ARG;
    scene->active = 1u;
    scene->count = 0u;
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_faces_submit_quad(
    sat_scene3d_faces_t*, const sat_quad3_t*,
    const sat_scene3d_material_t*, uint16_t) { return SAT_OK; }
extern "C" sat_result_t sat_scene3d_faces_submit_box(
    sat_scene3d_faces_t*, const sat_indexed_box3_t*, uint8_t,
    uint16_t) { return SAT_OK; }
extern "C" sat_result_t sat_scene3d_faces_submit_tiled_quad(
    sat_scene3d_faces_t*, const sat_quad3_t*,
    const sat_indexed_tiled_quad3_t*, uint8_t,
    uint16_t) { return SAT_OK; }
extern "C" sat_result_t sat_scene3d_faces_submit_instance(
    sat_scene3d_faces_t*, const sat_scene3d_instance_t*, uint8_t,
    sat_projected_vertex_t*, sat_vec3_t*) { return SAT_OK; }
extern "C" sat_result_t sat_scene3d_faces_depth(
    const sat_scene3d_faces_t*, const sat_vec3_t*, sat_fx16_t*) {
    return SAT_OK;
}
extern "C" sat_result_t sat_scene3d_faces_flush(sat_scene3d_faces_t* scene) {
    if (!scene || !scene->active) return SAT_ERR_INVALID_ARG;
    scene->active = 0u;
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp1_reserve_overlay_commands(uint16_t) {
    return SAT_OK;
}
extern "C" sat_result_t sat_vdp1_command_stats(sat_vdp1_command_stats_t* out) {
    if (!out) return SAT_ERR_INVALID_ARG;
    out->used = static_cast<uint16_t>(g_replayed + 2u);
    out->capacity = 64u;
    out->overlay_reserved = 8u;
    out->overlay_pass = 0u;
    out->reserved = 0u;
    return SAT_OK;
}
extern "C" sat_result_t sat_draw_quad2_polygon(
    const sat_quad2_t*, uint16_t) {
    ++g_replayed;
    return SAT_OK;
}

int main() {
    sat_scene_t scene = {};
    sat_scene3d_face_t faces[4] = {};
    uint32_t keys[4] = {};
    uint16_t order[4] = {};
    sat_camera3d_t camera = {};
    camera.eye.z = sat_fx16_from_int(10);
    camera.target.z = 0;
    assert(sat_scene_init(&scene, faces, keys, order, 4u) == SAT_OK);
    assert(sat_scene_begin(&scene, &camera, SAT_FX16_ONE,
        320u, 224u, 8u) == SAT_OK);

    sat_view_cache_item_t item = {};
    item.color = 0x801Fu;
    assert(sat_scene_replay_view_item(&scene, &item) == SAT_OK);
    sat_scene_stats_t stats = {};
    assert(sat_scene_stats(&scene, &stats) == SAT_OK);
    assert(stats.replayed_items == 1u);
    assert(stats.result == SAT_ERR_BUSY);
    assert(sat_scene_flush(&scene) == SAT_OK);
    assert(sat_scene_stats(&scene, &stats) == SAT_OK);
    assert(stats.replayed_items == 1u);
    assert(stats.commands_used == 3u && stats.commands_capacity == 64u);
    assert(g_replayed == 1u);

    std::puts("scene api: OK");
    return 0;
}
