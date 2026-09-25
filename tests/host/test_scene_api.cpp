#include <cassert>
#include <cstdio>

#include "saturn/scene.h"

static uint16_t g_replayed;
static sat_result_t g_submit_status=SAT_OK;
static sat_result_t g_flush_status=SAT_OK;
static uint16_t g_world_commands=0u;
static uint16_t g_flush_emissions=0u;
static uint16_t g_pending_cached=0u;
static sat_scene3d_material_kind_t g_cached_material_kind=SAT_SCENE3D_RGB;
static uint16_t g_command_capacity=64u;
static uint16_t g_hud_reserved=8u;
static uint16_t g_flush_calls=0u;
static uint16_t g_checkpoint_calls=0u;
static uint16_t g_rollback_calls=0u;
static sat_result_t g_cache_lookup=SAT_OK;
static uint16_t g_cache_count=0u;
static sat_view_cache_item_t g_cache_items[2]={};
static uint16_t g_cache_queries=0u;
static uint16_t g_projected_calls=0u;
static uint16_t g_projected_fail_at=0u;

extern "C" sat_result_t sat_view_cache_view_camera(
    sat_view_cache_t*,uint16_t,const sat_camera3d_t*,
    sat_fx16_t,uint16_t,uint16_t,
    const sat_view_cache_item_t** out,uint16_t* count) {
    ++g_cache_queries;
    *out=g_cache_lookup==SAT_OK?g_cache_items:nullptr;
    *count=g_cache_lookup==SAT_OK?g_cache_count:0u;
    return g_cache_lookup;
}

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
    sat_scene3d_faces_t* scene, const sat_camera3d_t* camera, sat_fx16_t near_depth,
    uint16_t width, uint16_t height) {
    if (!scene || scene->active) return SAT_ERR_INVALID_ARG;
    scene->active = 1u;
    scene->count = 0u;
    scene->view_proj=camera->view_proj;
    scene->eye=camera->eye;
    scene->near_depth=near_depth;
    scene->width=width;
    scene->height=height;
    /* Mirror the real painter's begin(): frame telemetry cannot leak into
     * a budget-rejected frame that never invokes the mocked flush(). */
    scene->emitted_faces=scene->emitted_cached=scene->skipped_faces=0u;
    g_pending_cached=0u;
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_faces_submit_quad(
    sat_scene3d_faces_t*, const sat_quad3_t*,
    const sat_scene3d_material_t*, uint16_t) { return g_submit_status; }
extern "C" sat_result_t sat_scene3d_faces_submit_projected_rgb(
    sat_scene3d_faces_t* scene,const sat_quad2_t* quad,
    sat_fx16_t camera_depth,uint16_t,uint16_t pass) {
    if (!scene || !scene->active || !quad || camera_depth<0 ||
        pass>SAT_SCENE3D_PASS_MAX) return SAT_ERR_INVALID_ARG;
    if (scene->count>=scene->capacity) return SAT_ERR_CAPACITY;
    ++scene->count;
    ++g_pending_cached;
    return SAT_OK;
}
extern "C" sat_result_t sat_scene3d_faces_submit_projected_material(
    sat_scene3d_faces_t* scene,const sat_quad2_t* quad,
    sat_fx16_t camera_depth,const sat_scene3d_material_t* material,
    uint16_t pass) {
    if (!scene || !scene->active || !quad || !material ||
        camera_depth<0 || pass>SAT_SCENE3D_PASS_MAX ||
        (material->kind!=SAT_SCENE3D_RGB &&
         (!material->texture || !material->texture->valid)))
        return SAT_ERR_INVALID_ARG;
    if (scene->count>=scene->capacity) return SAT_ERR_CAPACITY;
    ++g_projected_calls;
    if(g_projected_fail_at && g_projected_calls==g_projected_fail_at)
        return SAT_ERR_INVALID_ARG;
    g_cached_material_kind=material->kind;
    ++scene->count;
    ++g_pending_cached;
    return SAT_OK;
}
extern "C" sat_result_t sat_scene3d_faces_submit_box(
    sat_scene3d_faces_t*, const sat_indexed_box3_t*, uint8_t,
    uint16_t) { return SAT_OK; }
/* A capture reports three queued faces when it ends. */
extern "C" sat_result_t sat_scene3d_capture_begin(sat_scene3d_faces_t*, uint16_t) {
    return SAT_OK;
}
extern "C" sat_result_t sat_scene3d_capture_end(sat_scene3d_faces_t*, uint16_t* out) {
    if (out) *out=3u;
    return SAT_OK;
}
/* Every split call queues two pieces. */
extern "C" sat_result_t sat_scene3d_faces_submit_quad_split(
    sat_scene3d_faces_t* scene, const sat_quad3_t*,
    const sat_scene3d_material_t*, uint16_t, const sat_plane3_t*, uint8_t) {
    if (scene->count+2u>scene->capacity) return SAT_ERR_CAPACITY;
    scene->count=static_cast<uint16_t>(scene->count+2u);
    return SAT_OK;
}
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
    ++g_flush_calls;
    if (!scene || !scene->active) return SAT_ERR_INVALID_ARG;
    scene->active = 0u;
    scene->emitted_cached = g_flush_status==SAT_OK ? g_pending_cached : 0u;
    scene->emitted_faces = g_flush_status==SAT_OK
        ? static_cast<uint16_t>(scene->count-g_pending_cached) : 0u;
    if (g_flush_status==SAT_OK) {
        g_replayed=static_cast<uint16_t>(g_replayed+g_pending_cached);
    }
    scene->skipped_faces = 0u;
    // Model the observable VDP1 command counter, not one command per face.
    // A failed flush may already have emitted part of its workload.
    g_world_commands=static_cast<uint16_t>(
        g_world_commands+g_flush_emissions);
    return g_flush_status;
}

extern "C" sat_result_t sat_vdp1_command_checkpoint(
    sat_vdp1_command_checkpoint_t* out) {
    if(!out)return SAT_ERR_INVALID_ARG;
    ++g_checkpoint_calls;
    out->used=static_cast<uint16_t>(g_replayed+g_world_commands+2u);
    out->frame_serial=1u;
    out->overlay_reserved=g_hud_reserved;
    out->gouraud_tables=0u;
    out->overlay_pass=0u;
    out->reserved=0u;
    return SAT_OK;
}
extern "C" sat_result_t sat_vdp1_command_rollback(
    const sat_vdp1_command_checkpoint_t* checkpoint) {
    if(!checkpoint || checkpoint->frame_serial!=1u)
        return SAT_ERR_INVALID_ARG;
    ++g_rollback_calls;
    const uint32_t base=static_cast<uint32_t>(g_replayed)+2u;
    if(checkpoint->used<base)return SAT_ERR_VERIFY_FAILED;
    g_world_commands=static_cast<uint16_t>(checkpoint->used-base);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp1_reserve_overlay_commands(uint16_t) {
    return SAT_OK;
}
extern "C" sat_result_t sat_vdp1_command_stats(sat_vdp1_command_stats_t* out) {
    if (!out) return SAT_ERR_INVALID_ARG;
    out->used = static_cast<uint16_t>(
        g_replayed + g_world_commands + 2u);
    out->capacity = g_command_capacity;
    out->overlay_reserved = g_hud_reserved;
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
    assert(stats.world_commands == 0u);

    // The physical world-command delta counts clipping/subdivision output
    // instead of assuming one hardware command for every accepted face.
    assert(sat_scene_begin(&scene, &camera, SAT_FX16_ONE,
        320u, 224u, 8u)==SAT_OK);
    scene.faces.count=1u;
    g_flush_emissions=3u;
    assert(sat_scene_flush(&scene)==SAT_OK);
    assert(sat_scene_stats(&scene,&stats)==SAT_OK);
    assert(stats.flushed_faces==1u && stats.world_commands==3u);
    assert(stats.commands_used==6u); // replay + 3 scene + 2 pre-existing
    g_flush_emissions=0u;

    // Opt-in queued items do not emit before flush; the cache's own depth
    // is intentionally unrelated to the explicit linear camera_depth.
    assert(sat_scene_begin(&scene, &camera, SAT_FX16_ONE,
        320u,224u,8u)==SAT_OK);
    item.depth=0xFFFFFFFFu;
    assert(sat_scene_queue_view_item(
        &scene,&item,6*SAT_FX16_ONE,0u)==SAT_OK);
    assert(sat_scene_stats(&scene,&stats)==SAT_OK);
    assert(stats.queued_view_items==1u && stats.replayed_items==0u);
    assert(stats.submitted_faces==0u && g_replayed==1u);
    assert(sat_scene_flush(&scene)==SAT_OK);
    assert(sat_scene_stats(&scene,&stats)==SAT_OK);
    assert(stats.queued_view_items==1u && stats.replayed_items==1u);
    assert(stats.flushed_faces==0u && stats.world_commands==1u);
    assert(g_replayed==2u);
    assert(sat_scene_queue_view_item(
        &scene,&item,6*SAT_FX16_ONE,0u)==SAT_ERR_INVALID_ARG);

    // The opt-in material path copies a preprojected textured cache item,
    // shares face capacity, and emits only as part of the scene flush.
    assert(sat_scene_begin(&scene,&camera,SAT_FX16_ONE,
        320u,224u,8u)==SAT_OK);
    sat_vdp1_texture_t tex{};
    tex.valid=1u;tex.srca=71u;
    const sat_scene3d_material_t textured={
        SAT_SCENE3D_INDEXED_TEXTURED,0u,&tex,nullptr,2u,nullptr};
    assert(sat_scene_queue_view_item_material(
        &scene,&item,3*SAT_FX16_ONE,&textured,0u)==SAT_OK);
    assert(g_cached_material_kind==SAT_SCENE3D_INDEXED_TEXTURED);
    assert(sat_scene_stats(&scene,&stats)==SAT_OK);
    assert(stats.queued_view_items==1u && stats.replayed_items==0u);
    assert(g_replayed==2u);
    assert(sat_scene_flush(&scene)==SAT_OK);
    assert(sat_scene_stats(&scene,&stats)==SAT_OK);
    assert(stats.queued_view_items==1u && stats.replayed_items==1u);
    assert(stats.flushed_faces==0u && stats.world_commands==1u);
    assert(g_replayed==3u);

    // The DRY baked path refuses legacy items without a linear W, rather
    // than misinterpreting an arbitrary cache sort key as painter depth.
    assert(sat_scene_begin(&scene,&camera,SAT_FX16_ONE,
        320u,224u,8u)==SAT_OK);
    item.camera_depth_valid=0u;
    assert(sat_scene_queue_baked_view_item_material(
        &scene,&item,&textured,0u)==SAT_ERR_INVALID_ARG);
    assert(sat_scene_stats(&scene,&stats)==SAT_OK);
    assert(stats.queued_view_items==0u && stats.result==SAT_ERR_INVALID_ARG);
    assert(sat_scene_flush(&scene)==SAT_ERR_INVALID_ARG);
    assert(g_replayed==3u);
    assert(sat_scene_begin(&scene,&camera,SAT_FX16_ONE,
        320u,224u,8u)==SAT_OK);
    item.camera_depth=3*SAT_FX16_ONE;
    item.camera_depth_valid=1u;
    assert(sat_scene_queue_baked_view_item_material(
        &scene,&item,&textured,0u)==SAT_OK);
    assert(sat_scene_flush(&scene)==SAT_OK);
    assert(sat_scene_stats(&scene,&stats)==SAT_OK);
    assert(stats.queued_view_items==1u && stats.replayed_items==1u);
    assert(g_replayed==4u);

    // L3 view submission verifies scene-camera coherence and cache identity
    // before queueing, and validates capacity/depth over the ENTIRE view.
    sat_view_cache_t cache{};
    g_cache_items[0].camera_depth=3*SAT_FX16_ONE;
    g_cache_items[1].camera_depth=5*SAT_FX16_ONE;
    g_cache_items[0].camera_depth_valid=1u;
    g_cache_items[1].camera_depth_valid=1u;
    g_cache_count=2u;
    g_cache_lookup=SAT_ERR_NOT_FOUND;
    assert(sat_scene_begin(&scene,&camera,SAT_FX16_ONE,
        320u,224u,8u)==SAT_OK);
    assert(sat_scene_queue_camera_view_material(
        &scene,&cache,0u,&camera,&textured,0u)==SAT_ERR_NOT_FOUND);
    assert(sat_scene_stats(&scene,&stats)==SAT_OK);
    assert(stats.result==SAT_ERR_BUSY && stats.queued_view_items==0u);
    g_cache_lookup=SAT_OK;
    assert(sat_scene_queue_camera_view_material(
        &scene,&cache,0u,&camera,&textured,0u)==SAT_OK);
    assert(sat_scene_stats(&scene,&stats)==SAT_OK);
    assert(stats.queued_view_items==2u && scene.faces.count==2u);
    assert(sat_scene_flush(&scene)==SAT_OK);
    assert(sat_scene_stats(&scene,&stats)==SAT_OK);
    assert(stats.replayed_items==2u);

    // An unexpected late failure on the SECOND material emission must not
    // leave the first cached face behind or modify older queued geometry.
    // This is a queue transaction (no VDP1 commands have been issued yet).
    assert(sat_scene_begin(&scene,&camera,SAT_FX16_ONE,
        320u,224u,8u)==SAT_OK);
    assert(sat_scene_queue_baked_view_item_material(
        &scene,&g_cache_items[0],&textured,0u)==SAT_OK);
    /* Our mock increments the face count but does not populate materials;
     * mark the previous face explicitly to verify it is left untouched. */
    scene.faces.entries[0].material.texture=&tex;
    const uint16_t before_count=scene.faces.count;
    const uint16_t before_queued=scene.queued_view_items;
    const uint16_t fail_on=g_projected_calls+2u;
    g_projected_fail_at=fail_on;
    assert(sat_scene_queue_camera_view_material(
        &scene,&cache,0u,&camera,&textured,0u)==SAT_ERR_INVALID_ARG);
    g_projected_fail_at=0u;
    assert(g_projected_calls==fail_on);
    assert(scene.faces.count==before_count);
    assert(scene.queued_view_items==before_queued);
    assert(scene.faces.entries[0].material.texture==&tex);
    /* The mocked painter tracks accepted test submissions separately; the
     * real painter consumes only the restored scene.faces.count. */
    g_pending_cached=before_count;
    assert(sat_scene_flush(&scene)==SAT_ERR_INVALID_ARG);

    sat_camera3d_t wrong_camera=camera;
    wrong_camera.eye.x+=SAT_FX16_ONE;
    const uint16_t queried=g_cache_queries;
    assert(sat_scene_begin(&scene,&camera,SAT_FX16_ONE,
        320u,224u,8u)==SAT_OK);
    assert(sat_scene_queue_camera_view_material(
        &scene,&cache,0u,&wrong_camera,&textured,0u)==SAT_ERR_INVALID_ARG);
    assert(g_cache_queries==queried && scene.faces.count==0u);
    assert(sat_scene_flush(&scene)==SAT_ERR_INVALID_ARG);

    /* Matrix and eye can remain bitwise identical while target/up/lens
     * fields are changed without calling camera_update(). In that case
     * the scene's depth-forward belongs to its original camera: NEVER
     * submit a view validated against the mutated camera. */
    sat_camera3d_t changed_target=camera;
    changed_target.target.x+=SAT_FX16_ONE;
    assert(sat_scene_begin(&scene,&camera,SAT_FX16_ONE,
        320u,224u,8u)==SAT_OK);
    assert(sat_scene_queue_camera_view_material(
        &scene,&cache,0u,&changed_target,&textured,0u)==SAT_ERR_INVALID_ARG);
    assert(g_cache_queries==queried && scene.faces.count==0u);
    assert(sat_scene_flush(&scene)==SAT_ERR_INVALID_ARG);

    sat_camera3d_t changed_lens=camera;
    changed_lens.aspect+=SAT_FX16_ONE;
    assert(sat_scene_begin(&scene,&camera,SAT_FX16_ONE,
        320u,224u,8u)==SAT_OK);
    assert(sat_scene_queue_camera_view_material(
        &scene,&cache,0u,&changed_lens,&textured,0u)==SAT_ERR_INVALID_ARG);
    assert(g_cache_queries==queried && scene.faces.count==0u);
    assert(sat_scene_flush(&scene)==SAT_ERR_INVALID_ARG);

    assert(sat_scene_begin(&scene,&camera,SAT_FX16_ONE,
        320u,224u,8u)==SAT_OK);
    g_cache_items[1].camera_depth_valid=0u;
    assert(sat_scene_queue_camera_view_material(
        &scene,&cache,0u,&camera,&textured,0u)==SAT_ERR_INVALID_ARG);
    assert(scene.faces.count==0u && scene.queued_view_items==0u);
    assert(sat_scene_flush(&scene)==SAT_ERR_INVALID_ARG);
    g_cache_items[1].camera_depth_valid=1u;

    assert(sat_scene_begin(&scene,&camera,SAT_FX16_ONE,
        320u,224u,8u)==SAT_OK);
    g_cache_count=5u;
    assert(sat_scene_queue_camera_view_material(
        &scene,&cache,0u,&camera,&textured,0u)==SAT_ERR_CAPACITY);
    assert(scene.faces.count==0u && scene.rejected_faces==1u);
    assert(sat_scene_flush(&scene)==SAT_ERR_CAPACITY);
    g_cache_count=0u;

    // Budget admission considers *all* already projected cached AND world
    // faces, leaving an END slot and the HUD reserve. No face may be emitted
    // if even that exact one-command subset does not fit.
    assert(sat_scene_begin(&scene,&camera,SAT_FX16_ONE,
        320u,224u,8u)==SAT_OK);
    const uint16_t flush_calls_before=g_flush_calls;
    const uint16_t replayed_before=g_replayed;
    g_command_capacity=15u; // already used commands + END + HUD exceed quota
    g_hud_reserved=4u;
    scene.faces.entries[0].projected_safe=1u;
    assert(sat_scene_queue_baked_view_item_material(
        &scene,&item,&textured,0u)==SAT_OK);
    scene.faces.entries[1].projected_safe=1u;
    scene.faces.count=2u;
    assert(sat_scene_flush(&scene)==SAT_ERR_CAPACITY);
    assert(g_flush_calls==flush_calls_before && g_replayed==replayed_before);
    assert(!scene.faces.active && scene.faces.count==0u);
    assert(sat_scene_stats(&scene,&stats)==SAT_OK);
    assert(stats.budget_blocked_faces==2u && stats.flushed_faces==0u);
    assert(stats.replayed_items==0u && stats.world_commands==0u);
    assert(stats.result==SAT_ERR_CAPACITY);
    // The next frame resets the terminal admission error and blocked count.
    g_command_capacity=64u;
    g_hud_reserved=8u;
    assert(sat_scene_begin(&scene,&camera,SAT_FX16_ONE,
        320u,224u,8u)==SAT_OK);
    assert(sat_scene_stats(&scene,&stats)==SAT_OK);
    assert(stats.budget_blocked_faces==0u && stats.result==SAT_ERR_BUSY);
    assert(sat_scene_flush(&scene)==SAT_OK);

    // Variable-cost fallback can exhaust the remaining commands even when
    // the projected-face lower-bound fit. Roll back its *entire* staged world
    // batch, but preserve the raw commands that predated the checkpoint.
    assert(sat_scene_begin(&scene,&camera,SAT_FX16_ONE,
        320u,224u,8u)==SAT_OK);
    scene.faces.count=3u;
    for(uint16_t i=0u;i<3u;++i)
        scene.faces.entries[i].projected_safe=0u;
    const uint16_t used_before_failure=static_cast<uint16_t>(
        g_replayed+g_world_commands+2u);
    const uint16_t checkpoints_before=g_checkpoint_calls;
    const uint16_t rollbacks_before=g_rollback_calls;
    g_flush_emissions=2u;
    g_flush_status=SAT_ERR_CAPACITY;
    assert(sat_scene_flush(&scene)==SAT_ERR_CAPACITY);
    assert(g_checkpoint_calls==checkpoints_before+1u);
    assert(g_rollback_calls==rollbacks_before+1u);
    assert(g_replayed+g_world_commands+2u==used_before_failure);
    assert(sat_scene_stats(&scene,&stats)==SAT_OK);
    assert(stats.world_commands==0u && stats.flushed_faces==0u);
    assert(stats.result==SAT_ERR_CAPACITY);
    g_flush_emissions=0u;
    g_flush_status=SAT_OK;

    // Rejected submissions remain visible in the result after flush.
    assert(sat_scene_begin(&scene, &camera, SAT_FX16_ONE,
        320u, 224u, 8u)==SAT_OK);
    sat_quad3_t quad{};
    sat_scene3d_material_t material{};
    // A split face counts every piece it queued as submitted.
    {
        sat_scene_stats_t split{};
        assert(sat_scene_stats(&scene,&split)==SAT_OK);
        const uint32_t before=split.submitted_faces;
        assert(sat_scene_submit_quad_split(&scene,&quad,&material,0u,nullptr,0u)==SAT_OK);
        assert(sat_scene_stats(&scene,&split)==SAT_OK);
        assert(split.submitted_faces==before+2u);
        scene.faces.count=static_cast<uint16_t>(scene.faces.count-2u);
    }
    // Faces queued by captured direct draws count as submitted too.
    {
        sat_scene_stats_t captured{};
        assert(sat_scene_stats(&scene,&captured)==SAT_OK);
        const uint32_t before=captured.submitted_faces;
        assert(sat_scene_capture_begin(&scene,1u)==SAT_OK);
        assert(sat_scene_capture_end(&scene)==SAT_OK);
        assert(sat_scene_stats(&scene,&captured)==SAT_OK);
        assert(captured.submitted_faces==before+3u);
    }
    g_submit_status=SAT_ERR_CAPACITY;
    assert(sat_scene_submit_quad(&scene,&quad,&material,0u)==SAT_ERR_CAPACITY);
    sat_scene_stats_t failed{};
    assert(sat_scene_stats(&scene,&failed)==SAT_OK);
    assert(failed.result==SAT_ERR_CAPACITY && failed.rejected_faces==1u);
    g_submit_status=SAT_OK;
    assert(sat_scene_flush(&scene)==SAT_ERR_CAPACITY);
    assert(sat_scene_stats(&scene,&failed)==SAT_OK);
    assert(failed.result==SAT_ERR_CAPACITY && failed.flushed_faces==0u);

    // Hardware rejection rolls back staged commands rather than counting
    // queued-but-undispatched faces; the next begin resets the error.
    assert(sat_scene_begin(&scene, &camera, SAT_FX16_ONE,
        320u, 224u, 8u)==SAT_OK);
    scene.faces.count=2u;
    g_flush_status=SAT_ERR_CAPACITY;
    g_flush_emissions=1u;
    assert(sat_scene_flush(&scene)==SAT_ERR_CAPACITY);
    assert(sat_scene_stats(&scene,&failed)==SAT_OK);
    assert(failed.result==SAT_ERR_CAPACITY &&
           failed.flushed_faces==0u && failed.world_commands==0u);
    g_flush_status=SAT_OK;
    g_flush_emissions=0u;
    assert(sat_scene_begin(&scene, &camera, SAT_FX16_ONE,
        320u, 224u, 8u)==SAT_OK);
    assert(sat_scene_stats(&scene,&failed)==SAT_OK && failed.result==SAT_ERR_BUSY);
    assert(sat_scene_flush(&scene)==SAT_OK);
    assert(sat_scene_stats(&scene,&failed)==SAT_OK && failed.result==SAT_OK);

    std::puts("scene api: OK");
    return 0;
}
