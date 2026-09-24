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
    scene->budget_blocked_faces=0u;
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
    /* The normal safe projected path emits one command per face. If that
     * exact subset already exceeds the remaining world budget, avoid
     * issuing a misleading partial frame, preserving the HUD reservation.
     * Clipped/fallback geometry has variable emission cost; this is an
     * admission lower-bound gate, not a guarantee that every flush fits. */
    uint32_t guaranteed_commands=0u;
    for (uint16_t i=0u;i<scene->faces.count;++i)
        if (scene->faces.entries[i].projected_safe!=0u)
            ++guaranteed_commands;
    sat_result_t st=SAT_OK;
    bool insufficient=false;
    if (before_status==SAT_OK) {
        const uint32_t reserved=before.overlay_pass!=0u
            ? 0u : before.overlay_reserved;
        const uint32_t occupied=static_cast<uint32_t>(before.used)+
            reserved+1u; /* hardware END slot */
        insufficient=occupied>before.capacity ||
            guaranteed_commands>static_cast<uint32_t>(before.capacity)-occupied;
    }
    if (insufficient || before_status!=SAT_OK) {
        if(insufficient)scene->budget_blocked_faces=scene->faces.count;
        scene->faces.active=0u;
        scene->faces.count=0u;
        st=insufficient?SAT_ERR_CAPACITY:before_status;
    } else {
        /* The actual number of clipped/fallback commands may exceed the
         * lower-bound admission estimate. Stage a frame-local checkpoint:
         * on a draw failure discard the ENTIRE scene's newly staged VDP1
         * commands and Gouraud tables, retaining prior raw draws and HUD
         * capacity. No hardware VRAM writes are reverted by this operation. */
        sat_vdp1_command_checkpoint_t checkpoint{};
        const sat_result_t marked=sat_vdp1_command_checkpoint(&checkpoint);
        if(marked!=SAT_OK) {
            scene->faces.active=0u;
            scene->faces.count=0u;
            st=marked;
        } else {
            st=sat_scene3d_faces_flush(&scene->faces);
            if(st!=SAT_OK) {
                const sat_result_t rolled=sat_vdp1_command_rollback(
                    &checkpoint);
                if(rolled==SAT_OK) {
                    scene->faces.emitted_faces=0u;
                    scene->faces.emitted_cached=0u;
                } else record_frame_result(scene,rolled);
            }
        }
    }
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

extern "C" sat_result_t sat_scene_queue_camera_view_material(
    sat_scene_t* scene,sat_view_cache_t* cache,uint16_t view,
    const sat_camera3d_t* camera,const sat_scene3d_material_t* material,
    uint16_t pass) {
    if (!scene || !scene->active) return SAT_ERR_INVALID_ARG;
    if (!camera || !cache || !material || pass>SAT_SCENE3D_PASS_MAX)
        return record_frame_result(scene,SAT_ERR_INVALID_ARG);
    const sat_scene3d_faces_t& faces=scene->faces;
    if (faces.eye.x!=camera->eye.x ||
        faces.eye.y!=camera->eye.y ||
        faces.eye.z!=camera->eye.z)
        return record_frame_result(scene,SAT_ERR_INVALID_ARG);
    for (uint8_t i=0u;i<16u;++i)
        if (faces.view_proj.m[i]!=camera->view_proj.m[i])
            return record_frame_result(scene,SAT_ERR_INVALID_ARG);

    const sat_view_cache_item_t* items=nullptr;
    uint16_t count=0u;
    const sat_result_t found=sat_view_cache_view_camera(
        cache,view,camera,faces.near_depth,faces.width,faces.height,
        &items,&count);
    /* NOT_FOUND is an ordinary cache miss: clients can rebake in-frame. */
    if (found==SAT_ERR_NOT_FOUND) return found;
    if (found!=SAT_OK) return record_frame_result(scene,found);
    if ((count!=0u && !items) ||
        count>static_cast<uint16_t>(faces.capacity-faces.count)) {
        ++scene->rejected_faces;
        return record_frame_result(scene,SAT_ERR_CAPACITY);
    }
    for (uint16_t i=0u;i<count;++i)
        if (!items[i].camera_depth_valid || items[i].camera_depth<0)
            return record_frame_result(scene,SAT_ERR_INVALID_ARG);
    for (uint16_t i=0u;i<count;++i) {
        const sat_result_t st=sat_scene_queue_baked_view_item_material(
            scene,&items[i],material,pass);
        if (st!=SAT_OK) return st;
    }
    return SAT_OK;
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
    out->budget_blocked_faces = scene->budget_blocked_faces;
    out->overlay_reserved = scene->overlay_commands;
    out->rejected_faces = scene->rejected_faces;
    out->commands_used = scene->commands_used;
    out->commands_capacity = scene->commands_capacity;
    out->world_commands = scene->world_commands;
    out->result = scene->first_error!=SAT_OK ? scene->first_error :
        (scene->active ? SAT_ERR_BUSY : SAT_OK);
    return SAT_OK;
}
