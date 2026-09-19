#ifndef SATURN_SCENE3D_H
#define SATURN_SCENE3D_H

#include <stdint.h>

#include "saturn/model3d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Caller-owned camera state. The derived view-projection matrix is updated by
 * sat_camera3d_update and is consumed by the scene draw facade. */
typedef struct sat_camera3d {
    sat_vec3_t eye;
    sat_vec3_t target;
    sat_vec3_t up;
    sat_fx16_t fov_y;
    sat_fx16_t aspect;
    sat_fx16_t near_z;
    sat_fx16_t far_z;
    sat_mat4_t view_proj;
} sat_camera3d_t;

sat_result_t sat_camera3d_init(
    sat_camera3d_t* out,
    const sat_vec3_t* eye,
    const sat_vec3_t* target,
    const sat_vec3_t* up,
    sat_fx16_t fov_y,
    sat_fx16_t aspect,
    sat_fx16_t near_z,
    sat_fx16_t far_z
);
sat_result_t sat_camera3d_update(sat_camera3d_t* camera);

typedef struct sat_model_transform3d {
    sat_vec3_t position;
    sat_vec3_t rotation_deg;
    sat_vec3_t scale;
} sat_model_transform3d_t;

void sat_model_transform3d_identity(sat_model_transform3d_t* out);
sat_result_t sat_model_transform3d_matrix(
    const sat_model_transform3d_t* transform,
    sat_mat4_t* out
);

/* Immediate model draw parameters. All arrays remain caller-owned and are
 * scratch or immutable generated data; the facade does not retain pointers. */
typedef struct sat_scene3d_model_params {
    const sat_vdp1_texture_t* textures;
    uint16_t texture_count;
    uint16_t color;
    sat_fx16_t ambient;
    uint16_t flags;
    const uint16_t* face_colors;
    const uint16_t* vertex_gouraud;
} sat_scene3d_model_params_t;

/* One immediate-mode scene context. The mesh and scratch arrays are supplied
 * by the caller, so one context can draw many models sequentially. */
typedef struct sat_scene3d {
    sat_camera3d_t camera;
    sat_mesh_t mesh;
    uint16_t vertex_cap;
    uint16_t face_cap;
    uint8_t* order;
    uint16_t* order16;
    uint32_t* depth;
    sat_projected_vertex_t* screen;
    uint8_t active;
} sat_scene3d_t;

sat_result_t sat_scene3d_init(
    sat_scene3d_t* scene,
    sat_vec3_t* vertex_storage,
    uint16_t vertex_cap,
    uint16_t* index_storage,
    uint16_t face_cap,
    uint8_t* order,
    uint16_t* order16,
    uint32_t* depth,
    sat_projected_vertex_t* screen
);
sat_result_t sat_scene3d_begin(sat_scene3d_t* scene, const sat_camera3d_t* camera);
sat_result_t sat_scene3d_draw_model(
    sat_scene3d_t* scene,
    const sat_model_asset_t* asset,
    const sat_model_transform3d_t* transform,
    const sat_scene3d_model_params_t* params
);
sat_result_t sat_scene3d_end(sat_scene3d_t* scene);

/* ------------------------------------------------------------------
 * Scene-wide painter queue (opt-in; immediate drawing stays compatible)
 * ------------------------------------------------------------------
 * VDP1 has no Z buffer. Defer independent drawable objects until flush,
 * then submit them far-to-near in *camera-space* including camera pitch.
 *
 * The caller owns the entries; the library does NOT allocate or copy the
 * meshes/textures referenced by an entry. Camera and per-entry transforms/
 * material descriptors are copied; geometry and context must remain valid
 * until flush completes. A callback is invoked synchronously at flush; it
 * must not call queue begin/submit/flush or modify its queued inputs.
 *
 * Pass 0 is ordinary world geometry, pass 1 foreground actors, etc. Items
 * within one pass are ordered by camera depth. Use the SAME pass for objects
 * that must occlude each other correctly (pig and gems); use another pass
 * only for an explicit artistic dependency, e.g. draw a large supporting
 * deck before a character. A higher pass is always painted on top and is
 * not a substitute for clipping/intersecting geometry or a Z buffer.
 *
 * Depth uses the dot product with target-eye, not squared eye distance or
 * world Z; ties preserve submission order. A 3D object's anchor should be
 * near its visual centre; very large/overlapping objects need subdivision.
 */
typedef sat_result_t (*sat_scene3d_draw_fn)(
    void* user, const sat_camera3d_t* camera);

typedef struct sat_scene3d_queue_item {
    sat_vec3_t center;       /* world-space painter anchor */
    int64_t depth;           /* computed at submission by the queue */
    uint16_t pass;           /* smaller pass paints earlier */
    uint16_t submission;     /* stable equal-depth ordering */
    uint8_t type;            /* private discriminator; do not set manually */
    uint8_t reserved[3];
    sat_scene3d_draw_fn draw;
    void* user;
    /* Used only by submit_model: all small descriptors copied, while
     * model/texture/material arrays remain caller-owned until flush. */
    sat_scene3d_t* model_scene;
    const sat_model_asset_t* model;
    sat_model_transform3d_t transform;
    sat_scene3d_model_params_t params;
} sat_scene3d_queue_item_t;

typedef struct sat_scene3d_queue {
    sat_scene3d_queue_item_t* items;
    uint16_t capacity;
    uint16_t count;
    uint8_t active;
    uint8_t flushing;
    sat_camera3d_t camera;
    sat_vec3_t forward; /* unit 16.16, derived once at queue_begin */
} sat_scene3d_queue_t;

sat_result_t sat_scene3d_queue_init(
    sat_scene3d_queue_t* queue,
    sat_scene3d_queue_item_t* storage,
    uint16_t capacity);
sat_result_t sat_scene3d_queue_begin(
    sat_scene3d_queue_t* queue, const sat_camera3d_t* camera);

/* Reusable normalized camera-space depth for per-game LOD/fade/culling.
 * An actor should NOT use this helper to sort other actors: queue owns
 * that order. Requires an active queue; negative means behind the eye. */
sat_result_t sat_scene3d_queue_depth(
    const sat_scene3d_queue_t* queue,
    const sat_vec3_t* position,
    sat_fx16_t* out_depth);

/* Synchronous callback receives its own opaque user context and the frame
 * camera. It can invoke existing sat_draw_world_* / VDP1 APIs; it does NOT
 * need to know about queue internals or compute any depth.
 * Returns SAT_ERR_CAPACITY without modifying the queue if full. */
sat_result_t sat_scene3d_queue_submit_draw(
    sat_scene3d_queue_t* queue,
    const sat_vec3_t* center,
    uint16_t pass,
    sat_scene3d_draw_fn draw,
    void* user);

/* The model variant uses the existing immediate-model renderer internally.
 * center_local is the model's visual centre in LOCAL coordinates; transformed
 * by transform on submission so sorting works for moving/rotating models.
 * model_scene is caller-owned reusable per-model scratch (large enough for
 * this asset), and need not have begun a frame: flush temporarily begins it.
 * Cannot share it with an actively drawing immediate-mode scene. */
sat_result_t sat_scene3d_queue_submit_model(
    sat_scene3d_queue_t* queue,
    const sat_vec3_t* center_local,
    uint16_t pass,
    sat_scene3d_t* model_scene,
    const sat_model_asset_t* model,
    const sat_model_transform3d_t* transform,
    const sat_scene3d_model_params_t* params);

/* Flush sorts the caller-owned entries in-place, issues draw callbacks
 * exactly once in painter order, then closes the frame. No VDP1 frame
 * begin/end is performed here: the game owns that hardware frame boundary.
 * On first drawing failure, stops and closes the queue; a new begin resets
 * it, preventing accidental duplicate VDP1 command submission. */
sat_result_t sat_scene3d_queue_flush(sat_scene3d_queue_t* queue);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_SCENE3D_H */
