#include "saturn/scene.h"

extern "C" sat_result_t sat_scene_requirements(uint16_t face_capacity,
                                                 uint16_t overlay_commands,
                                                 sat_scene_requirements_t* out) {
    if (!out || !face_capacity) return SAT_ERR_INVALID_ARG;
    out->face_capacity = face_capacity;
    out->overlay_commands = overlay_commands;
    out->face_bytes = static_cast<uint32_t>(face_capacity) * sizeof(sat_scene3d_face_t);
    out->sort_bytes = static_cast<uint32_t>(face_capacity) *
        (sizeof(uint32_t) + sizeof(uint16_t));
    return SAT_OK;
}

extern "C" sat_result_t sat_scene_init(sat_scene_t* scene,
                                         sat_scene3d_face_t* face_storage,
                                         uint32_t* sort_keys, uint16_t* sort_order,
                                         uint16_t face_capacity) {
    if (!scene) return SAT_ERR_INVALID_ARG;
    const sat_result_t st = sat_scene3d_faces_init(
        &scene->faces, face_storage, sort_keys, sort_order, face_capacity);
    if (st != SAT_OK) return st;
    const sat_scene3d_faces_t faces = scene->faces;
    *scene = {};
    scene->faces = faces;
    return SAT_OK;
}

extern "C" sat_result_t sat_scene_begin(sat_scene_t* scene,
                                          const sat_camera3d_t* camera,
                                          sat_fx16_t near_depth, uint16_t width,
                                          uint16_t height, uint16_t overlay_commands) {
    if (!scene || scene->active || !camera) return SAT_ERR_INVALID_ARG;
    const sat_result_t st = sat_scene3d_faces_begin_camera(
        &scene->faces, camera, near_depth, width, height);
    if (st != SAT_OK) return st;
    scene->overlay_commands = overlay_commands;
    scene->submitted_faces = scene->flushed_faces = scene->rejected_faces = 0;
    scene->culled_faces = scene->clipped_faces = scene->fallback_faces = 0u;
    scene->replayed_items = 0u;
    scene->commands_used = scene->commands_capacity = 0u;
    scene->flushed = 0;
    const sat_result_t reserve = sat_vdp1_reserve_overlay_commands(overlay_commands);
    if (reserve != SAT_OK) {
        scene->faces.active = 0;
        return reserve;
    }
    scene->active = 1;
    return SAT_OK;
}

extern "C" sat_result_t sat_scene_submit_quad(sat_scene_t* scene,
                                                const sat_quad3_t* quad,
                                                const sat_scene3d_material_t* material,
                                                uint16_t pass) {
    if (!scene || !scene->active) return SAT_ERR_INVALID_ARG;
    const sat_result_t st = sat_scene3d_faces_submit_quad(
        &scene->faces, quad, material, pass);
    if (st == SAT_OK) ++scene->submitted_faces;
    else if (st == SAT_ERR_CAPACITY) ++scene->rejected_faces;
    return st;
}

extern "C" sat_result_t sat_scene_submit_instance(sat_scene_t* scene,
                                                    const sat_scene3d_instance_t* instance,
                                                    uint8_t slot,
                                                    sat_projected_vertex_t* screen,
                                                    sat_vec3_t* world) {
    if (!scene || !scene->active) return SAT_ERR_INVALID_ARG;
    const uint16_t before = scene->faces.count;
    const sat_result_t st = sat_scene3d_faces_submit_instance(
        &scene->faces, instance, slot, screen, world);
    if (st == SAT_OK) scene->submitted_faces += scene->faces.count - before;
    else if (st == SAT_ERR_CAPACITY) ++scene->rejected_faces;
    return st;
}

extern "C" sat_result_t sat_scene_submit_box(sat_scene_t* scene,
                                               const sat_indexed_box3_t* box,
                                               uint8_t slot, uint16_t pass) {
    if (!scene || !scene->active) return SAT_ERR_INVALID_ARG;
    const uint16_t before = scene->faces.count;
    const sat_result_t st = sat_scene3d_faces_submit_box(&scene->faces, box, slot, pass);
    if (st == SAT_OK) scene->submitted_faces += scene->faces.count - before;
    else if (st == SAT_ERR_CAPACITY) ++scene->rejected_faces;
    return st;
}

extern "C" sat_result_t sat_scene_submit_tiled_quad(
    sat_scene_t* scene, const sat_quad3_t* quad,
    const sat_indexed_tiled_quad3_t* regions, uint8_t slot, uint16_t pass) {
    if (!scene || !scene->active) return SAT_ERR_INVALID_ARG;
    const uint16_t before = scene->faces.count;
    const sat_result_t st = sat_scene3d_faces_submit_tiled_quad(
        &scene->faces, quad, regions, slot, pass);
    if (st == SAT_OK) scene->submitted_faces += scene->faces.count - before;
    else if (st == SAT_ERR_CAPACITY) ++scene->rejected_faces;
    return st;
}

extern "C" sat_result_t sat_scene_depth(const sat_scene_t* scene,
                                          const sat_vec3_t* world,
                                          sat_fx16_t* out_depth) {
    if (!scene || !scene->active) return SAT_ERR_INVALID_ARG;
    return sat_scene3d_faces_depth(&scene->faces, world, out_depth);
}

extern "C" sat_result_t sat_scene_flush(sat_scene_t* scene) {
    if (!scene || !scene->active || scene->flushed) return SAT_ERR_INVALID_ARG;
    scene->flushed_faces = scene->faces.count;
    sat_result_t st = sat_scene3d_faces_flush(&scene->faces);
    sat_vdp1_command_stats_t commands{};
    const sat_result_t command_status = sat_vdp1_command_stats(&commands);
    if (command_status == SAT_OK) {
        scene->commands_used = commands.used;
        scene->commands_capacity = commands.capacity;
    } else if (st == SAT_OK) {
        st = command_status;
    }
    scene->active = 0;
    scene->flushed = 1;
    return st;
}

extern "C" sat_result_t sat_scene_replay_view_item(
    sat_scene_t* scene, const sat_view_cache_item_t* item) {
    if (!scene || !scene->active || !item) return SAT_ERR_INVALID_ARG;
    const sat_result_t st = sat_draw_quad2_polygon(&item->quad, item->color);
    if (st == SAT_OK) ++scene->replayed_items;
    else if (st == SAT_ERR_CAPACITY) ++scene->rejected_faces;
    return st;
}

extern "C" sat_result_t sat_scene_stats(const sat_scene_t* scene,
                                          sat_scene_stats_t* out) {
    if (!scene || !out) return SAT_ERR_INVALID_ARG;
    out->submitted_faces = scene->submitted_faces;
    out->flushed_faces = scene->flushed_faces;
    out->culled_faces = scene->culled_faces;
    out->clipped_faces = scene->clipped_faces;
    out->fallback_faces = scene->fallback_faces;
    out->replayed_items = scene->replayed_items;
    out->overlay_reserved = scene->overlay_commands;
    out->rejected_faces = scene->rejected_faces;
    out->commands_used = scene->commands_used;
    out->commands_capacity = scene->commands_capacity;
    out->result = scene->active ? SAT_ERR_BUSY : SAT_OK;
    return SAT_OK;
}
