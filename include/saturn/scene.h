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
    /* Accepted deferred cached items, which share world face storage. */
    uint16_t queued_view_items;
    /* Queued faces withheld when the exact one-command projected subset
     * alone exceeds the remaining VDP1 world command quota. */
    uint16_t budget_blocked_faces;
    uint16_t overlay_reserved;
    uint16_t rejected_faces;
    uint16_t commands_used;
    uint16_t commands_capacity;
    /* Actual VDP1 command delta produced by the synchronous painter flush,
     * including queued cached items and commands before a partial failure.
     * Excludes previously submitted raw VDP1 and immediate cached replays.
     * Face count differs when clipping/subdivision emits multiple commands. */
    uint16_t world_commands;
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
    uint16_t queued_view_items;
    uint16_t budget_blocked_faces;
    uint16_t rejected_faces;
    uint16_t commands_used;
    uint16_t commands_capacity;
    uint16_t world_commands;
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

/* Capture the active scene's camera and submit a caller-owned batch.
 * The batch dispatch policy is independent of game build flags:
 * CONSERVATIVE (zero-init) keeps geometry on Master in runtime AUTO,
 * RUNTIME delegates to the configured executor, and MASTER always executes
 * locally. SLAVE/AUTO may complete asynchronously. A pending task's BUSY
 * status is transient, while a terminal failure is recorded in scene stats. */
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
/* Opt-in joint painter ordering for a previously projected RGB cache item.
 * camera_depth is the positive LINEAR 16.16 depth of the source geometry
 * in this camera view, not item.depth (which may encode squared XZ distance).
 * Copies the quad into one scene face slot; emitted items count as replayed
 * only after successful flush. Immediate sat_scene_replay_view_item is kept
 * independent for L1 users who want exact call order instead. */
sat_result_t sat_scene_queue_view_item(
    sat_scene_t* scene, const sat_view_cache_item_t* item,
    sat_fx16_t camera_depth, uint16_t pass);

/* Queues a projected cache item with any valid RGB or indexed scene material
 * into the same bounded far-to-near painter as world faces. Its projected
 * corners and material descriptor are copied; tiled cache materials are
 * normalized to their preuploaded full texture at submit. The tiled
 * descriptor may then expire, but texture/VRAM backing remains caller-owned
 * and must stay valid through flush. The cached
 * view must match the current camera: source geometry, clipping and linear
 * camera_depth are explicitly supplied by the caller. For indexed tiled
 * materials, only the full preuploaded texture is drawn (no UV clipping).
 * The item's RGB color is ignored when a material is explicitly supplied. */
sat_result_t sat_scene_queue_view_item_material(
    sat_scene_t* scene, const sat_view_cache_item_t* item,
    sat_fx16_t camera_depth, const sat_scene3d_material_t* material,
    uint16_t pass);

/* DRY path for camera-bound items produced by sat_view_cache_append_world.
 * Uses the baked linear W, eliminating per-frame re-projection, manual
 * painter key arithmetic or one sat_scene_depth call per static face.
 * Requires camera_depth_valid, else fails without consuming a face slot.
 * Obtain the item with view_camera using this scene's active camera to
 * prevent submitting a stale projected view. */
sat_result_t sat_scene_queue_baked_view_item_material(
    sat_scene_t* scene, const sat_view_cache_item_t* item,
    const sat_scene3d_material_t* material, uint16_t pass);

/* L3 camera-coherent queue path for a full baked view sharing ONE material.
 * Rejects any scene/camera mismatch before fetching the view. A cache miss
 * returns NOT_FOUND without poisoning the frame, allowing a rebake/retry.
 * Validates ALL baked depth flags and capacity before enqueuing anything:
 * no partial queue on stale/mixed legacy view data or insufficient storage.
 * The explicit per-item L1/L2 paths above remain independent. */
sat_result_t sat_scene_queue_camera_view_material(
    sat_scene_t* scene, sat_view_cache_t* cache, uint16_t view,
    const sat_camera3d_t* camera,
    const sat_scene3d_material_t* material,uint16_t pass);

/* Before painter emission, checks the VDP1 command budget against all
 * already-safe projected faces (cached + world): each needs exactly one
 * command. If even that subset will not fit with the HUD reservation and
 * END terminator, closes the frame queue with SAT_ERR_CAPACITY and emits
 * NOTHING. fallback clipped faces may need multiple commands and still
 * fail partway through; the public result and actual command delta expose
 * that partial failure. Raw VDP1 operations remain independent. */

sat_result_t sat_scene_flush(sat_scene_t* scene);
sat_result_t sat_scene_stats(const sat_scene_t* scene, sat_scene_stats_t* out);

#ifdef __cplusplus
}
#endif
#endif
