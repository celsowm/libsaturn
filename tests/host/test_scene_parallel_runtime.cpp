#include <cassert>
#include <cstdio>
#include <vector>
#include <utility>
#include "saturn/scene.h"

static std::vector<std::pair<const void*, uint32_t>> synced;
static sat_parallel_process_fn registered_process;
static bool covered(const void* address) {
    for (const auto& range : synced) if (range.first == address) return true;
    return false;
}

static sat_result_t register_status=SAT_OK;
static sat_result_t submit_status=SAT_OK;
static sat_result_t task_result=SAT_ERR_BUSY;
static sat_result_t sync_status=SAT_OK;
static sat_result_t merge_status=SAT_OK;
static uint32_t task_bytes=0u;
static uint32_t submit_count=0u;
static uint32_t master_submit_count=0u;
static sat_parallel_mode_t selected_mode=SAT_PARALLEL_MASTER;
static sat_parallel_handle_t last_handle=0x1234u;

extern "C" sat_result_t sat_parallel_register_task(
    sat_parallel_task_type_t, sat_parallel_process_fn process) {
    registered_process=process;
    return register_status;
}
extern "C" sat_result_t sat_parallel_cache_sync_range(const void* address,uint32_t bytes) {
    synced.emplace_back(address,bytes);
    return sync_status;
}
extern "C" void* sat_parallel_uncached_address(const void* ptr) {
    return const_cast<void*>(ptr);
}
extern "C" sat_parallel_mode_t sat_parallel_mode(void) {
    return selected_mode;
}
extern "C" sat_result_t sat_parallel_submit(
    sat_parallel_task_type_t,const void*,uint32_t,void*,uint32_t,
    sat_parallel_handle_t* handle) {
    ++submit_count;
    if (submit_status==SAT_OK) *handle=last_handle;
    return submit_status;
}
extern "C" sat_result_t sat_parallel_submit_master(
    sat_parallel_task_type_t type,const void* in,uint32_t in_size,
    void* out,uint32_t out_capacity,sat_parallel_handle_t* handle) {
    ++master_submit_count;
    return sat_parallel_submit(type,in,in_size,out,out_capacity,handle);
}
extern "C" sat_result_t sat_parallel_result(
    sat_parallel_handle_t handle,uint32_t* out_size) {
    assert(handle==last_handle && out_size);
    *out_size=task_bytes;
    return task_result;
}
extern "C" sat_result_t sat_parallel_release(sat_parallel_handle_t) {
    return SAT_OK;
}
extern "C" sat_result_t sat_scene3d_prepare_batch_execute(
    sat_scene3d_prepare_batch_t*) { return SAT_OK; }
extern "C" sat_result_t sat_scene3d_faces_merge_prepared(
    sat_scene3d_faces_t* faces,const sat_scene3d_prepare_batch_t* batch) {
    if (merge_status!=SAT_OK) return merge_status;
    faces->count=static_cast<uint16_t>(faces->count+batch->metrics.prepared_faces);
    return SAT_OK;
}

static void source_publication_contract() {
    sat_vec3_t vertices[4]{};
    uint16_t indices[4]{0u,1u,2u,3u}, face_materials[1]{0u}, gouraud[4]{};
    uint8_t masks[1]{};
    sat_mesh_t mesh{};
    mesh.vertices=vertices; mesh.indices=indices;
    mesh.vertex_count=mesh.vertex_cap=4u;
    mesh.face_count=mesh.face_cap=1u;
    sat_vdp1_texture_t textures[2]{};
    sat_indexed_tiled_quad3_t tiled{};
    tiled.full=&textures[0]; tiled.tiles[0]=&textures[1];
    sat_scene3d_material_t materials[2]{};
    materials[0].texture=&textures[0];
    materials[1].texture=&textures[1];
    materials[1].tiled=&tiled;
    materials[0].vertex_gouraud=materials[1].vertex_gouraud=gouraud;
    sat_scene3d_instance_binding_t binding{};
    binding.valid=1u; binding.indices=indices; binding.vertex_count=4u;
    binding.face_count=1u; binding.materials=materials; binding.material_count=2u;
    binding.face_materials=face_materials;
    sat_scene3d_instance_t instance{};
    instance.mesh=&mesh; instance.materials=materials; instance.material_count=2u;
    instance.face_materials=face_materials; instance.validated_binding=&binding;
    instance.double_sided_faces=masks;
    sat_scene3d_source_plan_t plan{};
    sat_scene3d_source_range_t ranges[16]{};
    assert(sat_scene3d_source_plan_init(&plan,&instance,ranges,1u)==SAT_ERR_CAPACITY);
    assert(!plan.topology.valid);
    sync_status=SAT_ERR_IO;
    assert(sat_scene3d_source_plan_init(&plan,&instance,ranges,16u)==SAT_ERR_IO);
    assert(!plan.topology.valid);
    sync_status=SAT_OK;
    synced.clear();
    assert(sat_scene3d_source_plan_init(&plan,&instance,ranges,16u)==SAT_OK);
    assert(covered(indices) && covered(materials) && covered(face_materials));
    assert(covered(&tiled) && covered(textures) && covered(&textures[1]));
    assert(plan.range_count==plan.static_count+1u); // Shared dynamic Gouraud once.
    assert(ranges[plan.static_count].address==gouraud);
    sat_scene3d_prepare_item_t item{};
    item.instance=&instance; item.source_plan=&plan;
    sat_scene3d_face_t faces[1]{}; uint32_t keys[1]{};
    sat_scene3d_prepare_batch_t batch{};
    batch.items=&item; batch.item_count=1u;
    batch.faces=faces; batch.keys=keys; batch.capacity=1u;
    sat_scene_t scene{}; scene.active=1u;
    sat_parallel_handle_t handle{};
    synced.clear();
    assert(sat_scene_prepare_batch_async(&scene,&batch,&handle)==SAT_OK);
    assert(!covered(indices) && !covered(materials) && !covered(&tiled));
    assert(covered(vertices) && covered(masks) && covered(gouraud));
    // Every receiver invalidates static sources again. Calling the captured
    // adapter twice also models local fallback and a restarted worker: neither
    // can poison an acknowledgment belonging to a different CPU/session.
    for (unsigned receiver=0; receiver<2; ++receiver) {
        synced.clear();
        uint32_t output_size=99u;
        assert(registered_process(&batch,sizeof(batch),faces,sizeof(faces),&output_size)==SAT_OK);
        assert(covered(indices) && covered(materials) && covered(&tiled));
        assert(covered(vertices) && covered(gouraud) && covered(masks));
    }
    assert(sat_scene_prepare_batch_release(&scene,&batch,handle)==SAT_OK);
    sat_vec3_t next_vertices[4]{};
    mesh.vertices=next_vertices; gouraud[0]=0x1234u;
    synced.clear();
    assert(sat_scene_prepare_batch_async(&scene,&batch,&handle)==SAT_OK);
    assert(covered(next_vertices) && covered(gouraud) && !covered(materials));
    assert(sat_scene_prepare_batch_release(&scene,&batch,handle)==SAT_OK);
    uint16_t next_indices[4]{0u,1u,2u,3u}; mesh.indices=next_indices;
    assert(sat_scene_prepare_batch_async(&scene,&batch,&handle)==SAT_ERR_INVALID_ARG);
    // Republish after topology/descriptor edits, including edits at the same
    // material address. Nested pointers are compiled afresh.
    binding.indices=next_indices;
    materials[1].tiled=nullptr;
    assert(sat_scene3d_source_plan_init(&plan,&instance,ranges,16u)==SAT_OK);
    scene.first_error=SAT_OK;
    assert(sat_scene_prepare_batch_async(&scene,&batch,&handle)==SAT_OK);
    synced.clear();
    uint32_t output_size{};
    assert(registered_process(&batch,sizeof(batch),faces,sizeof(faces),&output_size)==SAT_OK);
    assert(covered(next_indices) && !covered(&tiled));
    assert(sat_scene_prepare_batch_release(&scene,&batch,handle)==SAT_OK);
    // A NULL plan keeps conservative publication for existing callers.
    item.source_plan=nullptr;
    synced.clear();
    assert(sat_scene_prepare_batch_async(&scene,&batch,&handle)==SAT_OK);
    assert(covered(next_indices) && covered(materials) && covered(gouraud));
    assert(sat_scene_prepare_batch_release(&scene,&batch,handle)==SAT_OK);
    submit_count=master_submit_count=0u;
}

int main() {
    source_publication_contract();
    sat_scene_t scene{};
    scene.active=1u;
    scene.faces.active=1u;
    scene.faces.near_depth=SAT_FX16_ONE;
    scene.faces.width=320u;
    scene.faces.height=224u;
    sat_scene3d_face_t storage[2]{};
    uint32_t keys[2]{};
    sat_scene3d_prepare_batch_t batch{};
    batch.faces=storage;
    batch.keys=keys;
    batch.capacity=2u;
    sat_parallel_handle_t handle=0u;

    // Rejected initialization belongs to the scene frame.
    register_status=SAT_ERR_UNSUPPORTED;
    assert(sat_scene_prepare_batch_async(&scene,&batch,&handle)==SAT_ERR_UNSUPPORTED);
    assert(scene.first_error==SAT_ERR_UNSUPPORTED);
    register_status=SAT_OK;
    scene.first_error=SAT_OK;

    submit_status=SAT_ERR_CAPACITY;
    assert(sat_scene_prepare_batch_async(&scene,&batch,&handle)==SAT_ERR_CAPACITY);
    assert(scene.first_error==SAT_ERR_CAPACITY && batch.pending==0u);
    submit_status=SAT_OK;
    scene.first_error=SAT_OK;
    assert(sat_scene_prepare_batch_async(&scene,&batch,&handle)==SAT_OK);
    assert(batch.pending==1u && handle==last_handle);
    const uint32_t after_submit=submit_count;
    assert(sat_scene_prepare_batch_async(&scene,&batch,&handle)==SAT_ERR_BUSY);
    assert(submit_count==after_submit && scene.first_error==SAT_OK);

    // An unfinished task must not poison a valid frame.
    assert(sat_scene_merge_prepared_batch(&scene,&batch,handle)==SAT_ERR_BUSY);
    assert(scene.first_error==SAT_OK);
    task_result=SAT_ERR_IO;
    assert(sat_scene_merge_prepared_batch(&scene,&batch,handle)==SAT_ERR_IO);
    assert(scene.first_error==SAT_ERR_IO);
    scene.first_error=SAT_OK;
    task_result=SAT_OK;
    task_bytes=1u; // Not a whole prepared face.
    assert(sat_scene_merge_prepared_batch(&scene,&batch,handle)==SAT_ERR_VERIFY_FAILED);
    assert(scene.first_error==SAT_ERR_VERIFY_FAILED);
    scene.first_error=SAT_OK;

    task_bytes=sizeof(sat_scene3d_face_t);
    merge_status=SAT_ERR_CAPACITY;
    assert(sat_scene_merge_prepared_batch(&scene,&batch,handle)==SAT_ERR_CAPACITY);
    assert(scene.rejected_faces==1u && scene.first_error==SAT_ERR_CAPACITY);
    merge_status=SAT_OK;
    scene.first_error=SAT_OK;
    assert(sat_scene_merge_prepared_batch(&scene,&batch,handle)==SAT_OK);
    assert(scene.faces.count==1u && scene.submitted_faces==1u);
    assert(scene.first_error==SAT_OK);
    assert(sat_scene_prepare_batch_release(&scene,&batch,handle)==SAT_OK);
    assert(batch.pending==0u);
    // A failed release lands in the frame like any other error; NULL scene
    // (teardown) only returns it.
    assert(sat_scene_prepare_batch_release(&scene,&batch,handle)==SAT_ERR_INVALID_ARG);
    assert(scene.first_error==SAT_ERR_INVALID_ARG);
    scene.first_error=SAT_OK;
    assert(sat_scene_prepare_batch_release(nullptr,&batch,handle)==SAT_ERR_INVALID_ARG);
    assert(scene.first_error==SAT_OK);
    assert(sat_scene_merge_prepared_batch(&scene,&batch,handle)==SAT_ERR_INVALID_ARG);
    assert(scene.first_error==SAT_ERR_INVALID_ARG);

    // The default remains conservative in AUTO, with per-batch overrides.
    scene.first_error=SAT_OK;
    selected_mode=SAT_PARALLEL_AUTO;
    const uint32_t first_master=master_submit_count;
    const uint32_t first_submit=submit_count;
    batch.dispatch=SAT_SCENE3D_PREPARE_DISPATCH_CONSERVATIVE;
    assert(sat_scene_prepare_batch_async(&scene,&batch,&handle)==SAT_OK);
    assert(master_submit_count==first_master+1u &&
           submit_count==first_submit+1u);
    assert(sat_scene_prepare_batch_release(&scene,&batch,handle)==SAT_OK);
    batch.dispatch=SAT_SCENE3D_PREPARE_DISPATCH_RUNTIME;
    assert(sat_scene_prepare_batch_async(&scene,&batch,&handle)==SAT_OK);
    assert(master_submit_count==first_master+1u &&
           submit_count==first_submit+2u);
    assert(sat_scene_prepare_batch_release(&scene,&batch,handle)==SAT_OK);
    batch.dispatch=SAT_SCENE3D_PREPARE_DISPATCH_MASTER;
    assert(sat_scene_prepare_batch_async(&scene,&batch,&handle)==SAT_OK);
    assert(master_submit_count==first_master+2u &&
           submit_count==first_submit+3u);
    assert(sat_scene_prepare_batch_release(&scene,&batch,handle)==SAT_OK);
    batch.dispatch=static_cast<sat_scene3d_prepare_dispatch_t>(99);
    assert(sat_scene_prepare_batch_async(&scene,&batch,&handle)==SAT_ERR_INVALID_ARG);
    assert(scene.first_error==SAT_ERR_INVALID_ARG &&
           submit_count==first_submit+3u && batch.pending==0u);

    std::puts("scene parallel frame errors: OK");
    return 0;
}
