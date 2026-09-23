#ifndef SATURN_SCENE_H
#define SATURN_SCENE_H

#include <stdint.h>
#include "saturn/scene3d_faces.h"
#include "saturn/parallel.h"
#include "saturn/transform3d.h"
#include "saturn/view_cache.h"
#include "saturn/vdp1.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Canonical game-facing 3D frame contract.  Geometry, materials and scratch
 * storage are caller-owned; this facade owns only frame state and delegates
 * projection, clipping, ordering and lowering to the existing face painter. */
typedef struct sat_scene_requirements {
    uint16_t face_capacity;
    uint16_t overlay_commands;
    uint32_t face_bytes;
    uint32_t sort_bytes;
} sat_scene_requirements_t;

typedef struct sat_scene_stats {
    uint16_t submitted_faces;
    /* Successful face-dispatch calls, not queued faces or hardware commands. */
    uint16_t flushed_faces;
    uint16_t skipped_faces;
    uint16_t culled_faces;
    uint16_t clipped_faces;
    uint16_t fallback_faces;
    uint16_t replayed_items;
    uint16_t overlay_reserved;
    uint16_t rejected_faces;
    uint16_t commands_used;
    uint16_t commands_capacity;
    sat_result_t result;
} sat_scene_stats_t;

typedef struct sat_scene {
    sat_scene3d_faces_t faces;
    uint16_t overlay_commands;
    uint16_t submitted_faces;
    uint16_t flushed_faces;
    uint16_t skipped_faces;
    uint16_t culled_faces;
    uint16_t clipped_faces;
    uint16_t fallback_faces;
    uint16_t replayed_items;
    uint16_t rejected_faces;
    uint16_t commands_used;
    uint16_t commands_capacity;
    uint8_t active;
    uint8_t flushed;
    sat_result_t first_error; /* First actionable frame error. */
} sat_scene_t;

sat_result_t sat_scene_requirements(uint16_t face_capacity,
                                    uint16_t overlay_commands,
                                    sat_scene_requirements_t* out);
sat_result_t sat_scene_init(sat_scene_t* scene,
                            sat_scene3d_face_t* face_storage,
                            uint32_t* sort_keys, uint16_t* sort_order,
                            uint16_t face_capacity);
sat_result_t sat_scene_begin(sat_scene_t* scene, const sat_camera3d_t* camera,
                             sat_fx16_t near_depth, uint16_t width,
                             uint16_t height, uint16_t overlay_commands);
sat_result_t sat_scene_submit_quad(sat_scene_t* scene, const sat_quad3_t* quad,
                                   const sat_scene3d_material_t* material,
                                   uint16_t pass);
sat_result_t sat_scene_submit_box(sat_scene_t* scene, const sat_indexed_box3_t* box,
                                  uint8_t color_calc_slot, uint16_t pass);
sat_result_t sat_scene_submit_tiled_quad(sat_scene_t* scene, const sat_quad3_t* quad,
                                         const sat_indexed_tiled_quad3_t* regions,
                                         uint8_t color_calc_slot, uint16_t pass);
sat_result_t sat_scene_depth(const sat_scene_t* scene, const sat_vec3_t* world,
                             sat_fx16_t* out_depth);
sat_result_t sat_scene_submit_instance(sat_scene_t* scene,
                                       const sat_scene3d_instance_t* instance,
                                       uint8_t color_calc_slot,
                                       sat_projected_vertex_t* screen_scratch,
                                       sat_vec3_t* world_scratch);

/* Capture the active scene's immutable camera state and submit one coarse
 * batch through the existing parallel executor. The batch remains caller-
 * owned until its handle is complete. In MASTER mode the same function is
 * executed synchronously by the executor; in SLAVE/AUTO it is non-blocking. */
sat_result_t sat_scene_prepare_batch_async(
    sat_scene_t* scene, sat_scene3d_prepare_batch_t* batch,
    sat_parallel_handle_t* out_handle);

/* Merge a completed batch into the canonical scene-wide painter queue. The
 * caller chooses the merge order, so task completion order cannot alter equal-
 * depth tie-breaking. */
sat_result_t sat_scene_merge_prepared_batch(
    sat_scene_t* scene, sat_scene3d_prepare_batch_t* batch,
    sat_parallel_handle_t handle);
sat_result_t sat_scene_prepare_batch_release(
    sat_scene3d_prepare_batch_t* batch, sat_parallel_handle_t handle);
/* Submit the world transform of a hierarchy node to the canonical painter.
 * The prototype instance is never modified; no hierarchy/scratch pointers
 * are retained. Evaluate the transform world after the last mutation first. */
sat_result_t sat_scene_submit_transform_instance(
    sat_scene_t* scene, const sat_transform3d_world_t* hierarchy,
    uint16_t node_id, const sat_scene3d_instance_t* prototype,
    uint8_t color_calc_slot, sat_projected_vertex_t* screen_scratch,
    sat_vec3_t* world_scratch);

/* Replays one already projected, already ordered static-view item. The cache
 * owns bake/projection/sort; the scene still owns the VDP1 world-pass
 * emission, command quota and telemetry. Applications may filter tags for
 * mutable occupancy (for example, eaten pellets) before calling this. */
sat_result_t sat_scene_replay_view_item(
    sat_scene_t* scene, const sat_view_cache_item_t* item);
/* Ends the frame and returns the first recorded error from its submissions
 * and flush. Raw VDP1 operations remain independent. */
sat_result_t sat_scene_flush(sat_scene_t* scene);
sat_result_t sat_scene_stats(const sat_scene_t* scene, sat_scene_stats_t* out);

#ifdef __cplusplus
}
#endif
#endif
