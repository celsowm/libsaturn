#include <cassert>
#include <cstdio>
#include "saturn/scene.h"

static sat_result_t register_status=SAT_OK;
static sat_result_t submit_status=SAT_OK;
static sat_result_t task_result=SAT_ERR_BUSY;
static sat_result_t sync_status=SAT_OK;
static sat_result_t merge_status=SAT_OK;
static uint32_t task_bytes=0u;
static uint32_t submit_count=0u;
static sat_parallel_handle_t last_handle=0x1234u;

extern "C" sat_result_t sat_parallel_register_task(
    sat_parallel_task_type_t, sat_parallel_process_fn) { return register_status; }
extern "C" sat_result_t sat_parallel_cache_sync_range(const void*,uint32_t) {
    return sync_status;
}
extern "C" void* sat_parallel_uncached_address(const void* ptr) {
    return const_cast<void*>(ptr);
}
extern "C" sat_parallel_mode_t sat_parallel_mode(void) {
    return SAT_PARALLEL_MASTER;
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

int main() {
    sat_scene_t scene{};
    scene.active=1u;
    scene.faces.active=1u;
    scene.faces.near_depth=SAT_FX16_ONE;
    scene.faces.width=320u;
    scene.faces.height=224u;
    sat_scene3d_face_t storage[2]{};
    uint32_t keys[2]{};
    uint16_t order[2]{};
    sat_scene3d_prepare_batch_t batch{};
    batch.faces=storage;
    batch.keys=keys;
    batch.order=order;
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
    assert(sat_scene_prepare_batch_release(&batch,handle)==SAT_OK);
    assert(batch.pending==0u);
    assert(sat_scene_merge_prepared_batch(&scene,&batch,handle)==SAT_ERR_INVALID_ARG);
    assert(scene.first_error==SAT_ERR_INVALID_ARG);

    std::puts("scene parallel frame errors: OK");
    return 0;
}
