#include "saturn/scene.h"
#include "src/graphics/3d/scene/frame_result.hpp"

using saturn::core::scene::record_frame_result;

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
    scene->first_error=SAT_OK;
    scene->skipped_faces=0u;
    scene->culled_faces = scene->clipped_faces = scene->fallback_faces = 0u;
    scene->replayed_items = scene->queued_view_items = 0u;
    scene->commands_used = scene->commands_capacity = 0u;
    scene->world_commands = 0u;
    scene->flushed = 0;
    const sat_result_t reserve = sat_vdp1_reserve_overlay_commands(overlay_commands);
    if (reserve != SAT_OK) {
        scene->faces.active = 0;
        if (scene->first_error == SAT_OK) scene->first_error = reserve;
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
    return record_frame_result(scene,st);
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
    return record_frame_result(scene,st);
}

extern "C" sat_result_t sat_scene_submit_box(sat_scene_t* scene,
                                               const sat_indexed_box3_t* box,
                                               uint8_t slot, uint16_t pass) {
    if (!scene || !scene->active) return SAT_ERR_INVALID_ARG;
    const uint16_t before = scene->faces.count;
    const sat_result_t st = sat_scene3d_faces_submit_box(&scene->faces, box, slot, pass);
    if (st == SAT_OK) scene->submitted_faces += scene->faces.count - before;
    else if (st == SAT_ERR_CAPACITY) ++scene->rejected_faces;
    return record_frame_result(scene,st);
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
    return record_frame_result(scene,st);
}

extern "C" sat_result_t sat_scene_depth(const sat_scene_t* scene,
                                          const sat_vec3_t* world,
                                          sat_fx16_t* out_depth) {
    if (!scene || !scene->active) return SAT_ERR_INVALID_ARG;
    return sat_scene3d_faces_depth(&scene->faces, world, out_depth);
}

extern "C" sat_result_t sat_scene_flush(sat_scene_t* scene) {
    if (!scene || !scene->active || scene->flushed) return SAT_ERR_INVALID_ARG;
    // Take a command snapshot directly around the synchronous L3 painter.
    // Unlike total used commands, the delta excludes earlier explicit raw
    // VDP1 and immediate cached-view submissions in the same video frame.
    sat_vdp1_command_stats_t before{};
    const sat_result_t before_status = sat_vdp1_command_stats(&before);
    record_frame_result(scene,before_status);
    const sat_result_t st = sat_scene3d_faces_flush(&scene->faces);
    scene->flushed_faces = scene->faces.emitted_faces;
    scene->skipped_faces = scene->faces.skipped_faces;
    scene->replayed_items = static_cast<uint16_t>(
        scene->replayed_items + scene->faces.emitted_cached);
    record_frame_result(scene,st);
    sat_vdp1_command_stats_t after{};
    const sat_result_t after_status = sat_vdp1_command_stats(&after);
    record_frame_result(scene,after_status);
    if (after_status == SAT_OK) {
        scene->commands_used = after.used;
        scene->commands_capacity = after.capacity;
        if (before_status == SAT_OK) {
            if (after.used >= before.used) {
                scene->world_commands = static_cast<uint16_t>(
                    after.used - before.used);
            } else {
                record_frame_result(scene,SAT_ERR_VERIFY_FAILED);
            }
        }
    }
    scene->active = 0;
    scene->flushed = 1;
    return scene->first_error;
}

extern "C" sat_result_t sat_scene_queue_view_item(
    sat_scene_t* scene, const sat_view_cache_item_t* item,
    sat_fx16_t camera_depth, uint16_t pass) {
    if (!scene || !scene->active) return SAT_ERR_INVALID_ARG;
    const sat_result_t st=sat_scene3d_faces_submit_projected_rgb(
        &scene->faces, item ? &item->quad : nullptr,
        camera_depth, item ? item->color : 0u, pass);
    if (st == SAT_OK) ++scene->queued_view_items;
    else if (st == SAT_ERR_CAPACITY) ++scene->rejected_faces;
    return record_frame_result(scene, st);
}

extern "C" sat_result_t sat_scene_queue_view_item_material(
    sat_scene_t* scene, const sat_view_cache_item_t* item,
    sat_fx16_t camera_depth, const sat_scene3d_material_t* material,
    uint16_t pass) {
    if (!scene || !scene->active) return SAT_ERR_INVALID_ARG;
    const sat_result_t st=sat_scene3d_faces_submit_projected_material(
        &scene->faces,item ? &item->quad : nullptr,
        camera_depth,material,pass);
    if (st == SAT_OK) ++scene->queued_view_items;
    else if (st == SAT_ERR_CAPACITY) ++scene->rejected_faces;
    return record_frame_result(scene,st);
}

extern "C" sat_result_t sat_scene_queue_baked_view_item_material(
    sat_scene_t* scene,const sat_view_cache_item_t* item,
    const sat_scene3d_material_t* material,uint16_t pass) {
    if (!scene || !scene->active) return SAT_ERR_INVALID_ARG;
    if (!item || item->camera_depth_valid==0u)
        return record_frame_result(scene,SAT_ERR_INVALID_ARG);
    return sat_scene_queue_view_item_material(
        scene,item,item->camera_depth,material,pass);
}

extern "C" sat_result_t sat_scene_replay_view_item(
    sat_scene_t* scene, const sat_view_cache_item_t* item) {
    if (!scene || !scene->active || !item) return SAT_ERR_INVALID_ARG;
    const sat_result_t st = sat_draw_quad2_polygon(&item->quad, item->color);
    if (st == SAT_OK) ++scene->replayed_items;
    else if (st == SAT_ERR_CAPACITY) ++scene->rejected_faces;
    return record_frame_result(scene,st);
}

extern "C" sat_result_t sat_scene_stats(const sat_scene_t* scene,
                                          sat_scene_stats_t* out) {
    if (!scene || !out) return SAT_ERR_INVALID_ARG;
    out->submitted_faces = scene->submitted_faces;
    out->flushed_faces = scene->flushed_faces;
    out->skipped_faces = scene->skipped_faces;
    out->culled_faces = scene->culled_faces;
    out->clipped_faces = scene->clipped_faces;
    out->fallback_faces = scene->fallback_faces;
    out->replayed_items = scene->replayed_items;
    out->queued_view_items = scene->queued_view_items;
    out->overlay_reserved = scene->overlay_commands;
    out->rejected_faces = scene->rejected_faces;
    out->commands_used = scene->commands_used;
    out->commands_capacity = scene->commands_capacity;
    out->world_commands = scene->world_commands;
    out->result = scene->first_error!=SAT_OK ? scene->first_error :
        (scene->active ? SAT_ERR_BUSY : SAT_OK);
    return SAT_OK;
}
