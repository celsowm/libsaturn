#include "saturn/scene.h"

#include "src/hal/dual_sh2/memory.hpp"

namespace {

void* shared_uncached(void* pointer) {
    const uintptr_t raw = reinterpret_cast<uintptr_t>(pointer);
    if (raw > 0xFFFFFFFFu) return pointer;
    const uint32_t physical =
        saturn::hal::dual_sh2::memory::cached_address(static_cast<uint32_t>(raw));
    const uint32_t uncached = saturn::hal::dual_sh2::memory::uncached_address(
        physical != 0u ? physical : static_cast<uint32_t>(raw));
    return reinterpret_cast<void*>(static_cast<uintptr_t>(
        uncached != 0u ? uncached : static_cast<uint32_t>(raw)));
}

sat_result_t prepare_batch_task(
    const void* input, uint32_t input_size, void* output,
    uint32_t output_capacity, uint32_t* output_size) {
    if (input == nullptr || input_size != sizeof(sat_scene3d_prepare_batch_t) ||
        output == nullptr || output_size == nullptr) return SAT_ERR_INVALID_ARG;
    sat_scene3d_prepare_batch_t* const shared =
        const_cast<sat_scene3d_prepare_batch_t*>(
            static_cast<const sat_scene3d_prepare_batch_t*>(input));
    if (output_capacity < static_cast<uint32_t>(shared->capacity) *
            sizeof(sat_scene3d_face_t)) return SAT_ERR_CAPACITY;

    /* The explicit executor output is the only buffer guaranteed to be
     * uncached on the Slave. Use the same alias for the parallel sort keys;
     * source descriptors remain immutable and may be read normally. */
    sat_scene3d_prepare_batch_t worker = *shared;
    worker.faces = static_cast<sat_scene3d_face_t*>(output);
    worker.keys = static_cast<uint32_t*>(shared_uncached(shared->keys));
    worker.order = static_cast<uint16_t*>(shared_uncached(shared->order));
    const sat_result_t result = sat_scene3d_prepare_batch_execute(&worker);
    if (result == SAT_OK) {
        shared->metrics = worker.metrics;
        *output_size = static_cast<uint32_t>(worker.metrics.prepared_faces) *
            sizeof(sat_scene3d_face_t);
    }
    return result;
}

}  // namespace

extern "C" sat_result_t sat_scene3d_prepare_parallel_register(void) {
    return sat_parallel_register_task(SAT_PARALLEL_TASK_SCENE_GEOMETRY,
                                      &prepare_batch_task);
}

extern "C" sat_result_t sat_scene_prepare_batch_async(
    sat_scene_t* scene, sat_scene3d_prepare_batch_t* batch,
    sat_parallel_handle_t* out_handle) {
    if (!scene || !scene->active || !batch || !out_handle ||
        batch->capacity == 0u || batch->faces == nullptr ||
        batch->keys == nullptr || batch->order == nullptr ||
        (batch->item_count != 0u && batch->items == nullptr))
        return SAT_ERR_INVALID_ARG;
    SAT_TRY(sat_scene3d_prepare_parallel_register());
    batch->view_proj = scene->faces.view_proj;
    batch->eye = scene->faces.eye;
    batch->forward = scene->faces.forward;
    batch->near_depth = scene->faces.near_depth;
    batch->width = scene->faces.width;
    batch->height = scene->faces.height;
    return sat_parallel_submit(
        SAT_PARALLEL_TASK_SCENE_GEOMETRY, batch, sizeof(*batch),
        batch->faces,
        static_cast<uint32_t>(batch->capacity) * sizeof(sat_scene3d_face_t),
        out_handle);
}

extern "C" sat_result_t sat_scene_merge_prepared_batch(
    sat_scene_t* scene, sat_scene3d_prepare_batch_t* batch,
    sat_parallel_handle_t handle) {
    if (!scene || !scene->active || !batch) return SAT_ERR_INVALID_ARG;
    uint32_t output_size = 0u;
    const sat_result_t result = sat_parallel_result(handle, &output_size);
    if (result != SAT_OK) return result;
    if (output_size % sizeof(sat_scene3d_face_t) != 0u ||
        output_size / sizeof(sat_scene3d_face_t) > batch->capacity)
        return SAT_ERR_VERIFY_FAILED;
    batch->metrics.prepared_faces = static_cast<uint16_t>(
        output_size / sizeof(sat_scene3d_face_t));
    const uint16_t before = scene->faces.count;
    const sat_result_t merged = sat_scene3d_faces_merge_prepared(&scene->faces, batch);
    if (merged == SAT_OK) {
        scene->submitted_faces = static_cast<uint16_t>(
            scene->submitted_faces + (scene->faces.count - before));
        scene->culled_faces = scene->faces.culled_faces;
        scene->clipped_faces = scene->faces.clipped_faces;
    } else if (merged == SAT_ERR_CAPACITY) {
        ++scene->rejected_faces;
    }
    return merged;
}
