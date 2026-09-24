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
    g_cached_material_kind=material->kind;
    ++scene->count;
    ++g_pending_cached;
    return SAT_OK;
}
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

extern "C" sat_result_t sat_vdp1_reserve_overlay_commands(uint16_t) {
    return SAT_OK;
}
extern "C" sat_result_t sat_vdp1_command_stats(sat_vdp1_command_stats_t* out) {
    if (!out) return SAT_ERR_INVALID_ARG;
    out->used = static_cast<uint16_t>(
        g_replayed + g_world_commands + 2u);
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

    // Rejected submissions remain visible in the result after flush.
    assert(sat_scene_begin(&scene, &camera, SAT_FX16_ONE,
        320u, 224u, 8u)==SAT_OK);
    sat_quad3_t quad{};
    sat_scene3d_material_t material{};
    g_submit_status=SAT_ERR_CAPACITY;
    assert(sat_scene_submit_quad(&scene,&quad,&material,0u)==SAT_ERR_CAPACITY);
    sat_scene_stats_t failed{};
    assert(sat_scene_stats(&scene,&failed)==SAT_OK);
    assert(failed.result==SAT_ERR_CAPACITY && failed.rejected_faces==1u);
    g_submit_status=SAT_OK;
    assert(sat_scene_flush(&scene)==SAT_ERR_CAPACITY);
    assert(sat_scene_stats(&scene,&failed)==SAT_OK);
    assert(failed.result==SAT_ERR_CAPACITY && failed.flushed_faces==0u);

    // Hardware rejection does not turn queued-but-unemitted faces into
    // successfully dispatched faces, and the next begin resets the error.
    assert(sat_scene_begin(&scene, &camera, SAT_FX16_ONE,
        320u, 224u, 8u)==SAT_OK);
    scene.faces.count=2u;
    g_flush_status=SAT_ERR_CAPACITY;
    g_flush_emissions=1u;
    assert(sat_scene_flush(&scene)==SAT_ERR_CAPACITY);
    assert(sat_scene_stats(&scene,&failed)==SAT_OK);
    assert(failed.result==SAT_ERR_CAPACITY &&
           failed.flushed_faces==0u && failed.world_commands==1u);
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
