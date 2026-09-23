#include "saturn/scene.h"
#include "src/graphics/3d/scene/frame_result.hpp"

using saturn::core::scene::record_frame_result;

#include "src/hal/dual_sh2/memory.hpp"
#include "src/hal/sh2/frt.hpp"

#ifndef SAT_SKYBRIDGE_VALIDATION
#define SAT_SKYBRIDGE_VALIDATION 0
#endif

#ifndef SAT_SKYBRIDGE_FORCE_GEM_SPLIT
#define SAT_SKYBRIDGE_FORCE_GEM_SPLIT 0
#endif

#ifndef SAT_PARALLEL_RUNTIME_VALIDATION
#define SAT_PARALLEL_RUNTIME_VALIDATION 0
#endif

namespace {
#if SAT_SKYBRIDGE_VALIDATION || SAT_PARALLEL_RUNTIME_VALIDATION
uint32_t g_test_input_publish_ticks;
#endif

void* shared_uncached(void* pointer) {
    return sat_parallel_uncached_address(pointer);
}

sat_result_t sync_range(const void* pointer, uint32_t size) {
    if (pointer == nullptr || size == 0u) return SAT_OK;
    return sat_parallel_cache_sync_range(pointer, size);
}

sat_result_t sync_batch_sources(const sat_scene3d_prepare_batch_t* batch) {
    if (batch == nullptr || batch->items == nullptr) return SAT_OK;
    SAT_TRY(sync_range(batch->items, static_cast<uint32_t>(batch->item_count) *
        sizeof(*batch->items)));
    for (uint16_t i = 0u; i < batch->item_count; ++i) {
        const sat_scene3d_prepare_item_t& item = batch->items[i];
        if (item.instance == nullptr) {
            return SAT_ERR_INVALID_ARG;
        }
        const sat_scene3d_instance_t* const instance = item.instance;
        SAT_TRY(sync_range(instance, sizeof(*instance)));
        const sat_mesh_t* const mesh = instance->mesh;
        if (mesh == nullptr) return SAT_ERR_INVALID_ARG;
        SAT_TRY(sync_range(mesh, sizeof(*mesh)));
        SAT_TRY(sync_range(mesh->vertices, static_cast<uint32_t>(
            mesh->vertex_count) * sizeof(*mesh->vertices)));
        SAT_TRY(sync_range(mesh->indices, static_cast<uint32_t>(
            mesh->face_count) * 4u * sizeof(*mesh->indices)));
        SAT_TRY(sync_range(instance->materials, static_cast<uint32_t>(
            instance->material_count) * sizeof(*instance->materials)));
        SAT_TRY(sync_range(instance->face_materials, static_cast<uint32_t>(
            mesh->face_count) * sizeof(*instance->face_materials)));
        SAT_TRY(sync_range(instance->world, sizeof(sat_mat4_t)));
        SAT_TRY(sync_range(item.screen_scratch, static_cast<uint32_t>(
            mesh->vertex_count) * sizeof(*item.screen_scratch)));
        SAT_TRY(sync_range(item.world_scratch, static_cast<uint32_t>(
            mesh->vertex_count) * sizeof(*item.world_scratch)));
        for (uint16_t m = 0u; m < instance->material_count; ++m) {
            const sat_scene3d_material_t& material = instance->materials[m];
            SAT_TRY(sync_range(material.texture, sizeof(*material.texture)));
            SAT_TRY(sync_range(material.tiled, sizeof(*material.tiled)));
            if (material.tiled != nullptr) {
                SAT_TRY(sync_range(material.tiled->full,
                    sizeof(*material.tiled->full)));
                for (uint8_t tile = 0u; tile < 4u; ++tile) {
                    SAT_TRY(sync_range(material.tiled->tiles[tile],
                        sizeof(*material.tiled->tiles[tile])));
                }
            }
            SAT_TRY(sync_range(material.vertex_gouraud, static_cast<uint32_t>(
                mesh->vertex_count) * sizeof(*material.vertex_gouraud)));
        }
    }
    return SAT_OK;
}

sat_result_t sync_batch_outputs(const sat_scene3d_prepare_batch_t* batch) {
    if (batch == nullptr) return SAT_ERR_INVALID_ARG;
    SAT_TRY(sync_range(batch->keys, static_cast<uint32_t>(batch->capacity) *
        sizeof(*batch->keys)));
    SAT_TRY(sync_range(batch->order, static_cast<uint32_t>(batch->capacity) *
        sizeof(*batch->order)));
    if (batch->items == nullptr) return SAT_OK;
    for (uint16_t i = 0u; i < batch->item_count; ++i) {
        const sat_scene3d_prepare_item_t& item = batch->items[i];
        const sat_mesh_t* const mesh = item.instance != nullptr
            ? item.instance->mesh : nullptr;
        if (mesh == nullptr) continue;
        SAT_TRY(sync_range(item.screen_scratch, static_cast<uint32_t>(
            mesh->vertex_count) * sizeof(*item.screen_scratch)));
        SAT_TRY(sync_range(item.world_scratch, static_cast<uint32_t>(
            mesh->vertex_count) * sizeof(*item.world_scratch)));
    }
    return SAT_OK;
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

    /* The nested graph is caller-owned. Publish it before the Slave reads it,
     * then invalidate the same exact ranges on the worker so cached aliases
     * cannot retain an older descriptor or mesh/material array. */
    SAT_TRY(sync_batch_sources(shared));

    /* The explicit executor output is the only buffer guaranteed to be
     * uncached on the Slave. Use the same alias for the parallel sort keys;
     * source descriptors remain immutable and may be read normally. */
    sat_scene3d_prepare_batch_t worker = *shared;
    worker.faces = static_cast<sat_scene3d_face_t*>(output);
    worker.keys = static_cast<uint32_t*>(shared_uncached(shared->keys));
    worker.order = static_cast<uint16_t*>(shared_uncached(shared->order));
    const sat_result_t result = sat_scene3d_prepare_batch_execute(&worker);
    SAT_TRY(sync_batch_outputs(shared));
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
    if (!scene || !scene->active) return SAT_ERR_INVALID_ARG;
    if (!batch || !out_handle || batch->capacity == 0u ||
        batch->faces == nullptr || batch->keys == nullptr ||
        batch->order == nullptr ||
        (batch->item_count != 0u && batch->items == nullptr))
        return record_frame_result(scene,SAT_ERR_INVALID_ARG);
    // A pending task has not failed: callers can retry after completion.
    if (batch->pending != 0u) return SAT_ERR_BUSY;
    const sat_result_t registered=sat_scene3d_prepare_parallel_register();
    if (registered != SAT_OK) return record_frame_result(scene,registered);
    batch->view_proj = scene->faces.view_proj;
    batch->eye = scene->faces.eye;
    batch->forward = scene->faces.forward;
    batch->near_depth = scene->faces.near_depth;
    batch->width = scene->faces.width;
    batch->height = scene->faces.height;
#if SAT_SKYBRIDGE_VALIDATION || SAT_PARALLEL_RUNTIME_VALIDATION
    const uint16_t publish_start=saturn::hal::sh2::frt::counter();
#endif
    const sat_result_t published=sync_batch_sources(batch);
    if (published != SAT_OK) return record_frame_result(scene,published);
#if SAT_SKYBRIDGE_VALIDATION || SAT_PARALLEL_RUNTIME_VALIDATION
    g_test_input_publish_ticks=static_cast<uint16_t>(
        saturn::hal::sh2::frt::counter()-publish_start);
#endif
    const uint32_t output_capacity =
        static_cast<uint32_t>(batch->capacity) * sizeof(sat_scene3d_face_t);
    sat_result_t submitted;
    if (sat_parallel_mode() == SAT_PARALLEL_AUTO &&
        SAT_SKYBRIDGE_FORCE_GEM_SPLIT == 0) {
        submitted = sat_parallel_submit_master(
            SAT_PARALLEL_TASK_SCENE_GEOMETRY, batch, sizeof(*batch),
            batch->faces, output_capacity, out_handle);
    } else {
        submitted = sat_parallel_submit(
            SAT_PARALLEL_TASK_SCENE_GEOMETRY, batch, sizeof(*batch),
            batch->faces, output_capacity, out_handle);
    }
    if (submitted == SAT_OK) {
        batch->handle = *out_handle;
        batch->pending = 1u;
    }
    return record_frame_result(scene,submitted);
}

extern "C" sat_result_t sat_scene_merge_prepared_batch(
    sat_scene_t* scene, sat_scene3d_prepare_batch_t* batch,
    sat_parallel_handle_t handle) {
    if (!scene || !scene->active) return SAT_ERR_INVALID_ARG;
    if (!batch || batch->pending == 0u || batch->handle != handle)
        return record_frame_result(scene,SAT_ERR_INVALID_ARG);
    uint32_t output_size = 0u;
    const sat_result_t result = sat_parallel_result(handle, &output_size);
    if (result == SAT_ERR_BUSY) return SAT_ERR_BUSY;
    if (result != SAT_OK) return record_frame_result(scene,result);
    if (output_size % sizeof(sat_scene3d_face_t) != 0u ||
        output_size / sizeof(sat_scene3d_face_t) > batch->capacity)
        return record_frame_result(scene,SAT_ERR_VERIFY_FAILED);
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
    return record_frame_result(scene,merged);
}

extern "C" sat_result_t sat_scene_prepare_batch_release(
    sat_scene3d_prepare_batch_t* batch, sat_parallel_handle_t handle) {
    if (batch == nullptr || batch->pending == 0u || batch->handle != handle) {
        return SAT_ERR_INVALID_ARG;
    }
    const sat_result_t released = sat_parallel_release(handle);
    if (released == SAT_OK) {
        batch->pending = 0u;
        batch->handle = 0u;
    }
    return released;
}

#if SAT_SKYBRIDGE_VALIDATION || SAT_PARALLEL_RUNTIME_VALIDATION
extern "C" uint32_t sat_scene3d_test_input_publish_ticks(void) {
    return g_test_input_publish_ticks;
}
#endif
