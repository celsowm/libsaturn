#ifndef SATURN_SCENE3D_FACES_H
#define SATURN_SCENE3D_FACES_H

#include <stdint.h>
#include "saturn/scene3d.h"
#include "saturn/model3d.h"
#include "saturn/parallel.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A bounded scene-wide face painter. Every submitted face, including faces
 * from DIFFERENT meshes, enters the same ordered list. This does not provide
 * a Z-buffer: crossing polygons may need level-authored subdivision.
 * No allocation or implicit texture uploads occur during a frame. */
typedef enum sat_scene3d_material_kind {
    SAT_SCENE3D_RGB = 0,
    SAT_SCENE3D_INDEXED_SOLID = 1,
    SAT_SCENE3D_INDEXED_TEXTURED = 2,
    SAT_SCENE3D_INDEXED_TILED = 3
} sat_scene3d_material_kind_t;

typedef struct sat_scene3d_material {
    sat_scene3d_material_kind_t kind;
    uint16_t rgb555;
    const sat_vdp1_texture_t* texture;
    /* Only used for INDEXED_TILED; descriptor must survive until flush. */
    const sat_indexed_tiled_quad3_t* tiled;
    /* 255 = ordinary opaque sprite; 0..7 = previously configured VDP2 slot.
     * RGB polygon material does not support the indexed-sprite slots. */
    uint8_t color_calc_slot;
    /* Optional per-vertex Gouraud table. The pointer is read only during
     * submission and the four selected entries are copied into the face. */
    const uint16_t* vertex_gouraud;
} sat_scene3d_material_t;

/* 12-bit artistic pass in the painter key. Values beyond this fail atomically. */
#define SAT_SCENE3D_PASS_MAX 4095u

/* Paint order is NOT a field here: it lives in the scene's parallel key
 * array, so ordering a frame moves two-byte indices instead of these
 * multi-word records. */
typedef struct sat_scene3d_face {
    sat_quad3_t world;
    sat_quad2_t projected;
    sat_scene3d_material_t material;
    uint8_t projected_safe;
    uint8_t gouraud_valid;
    /* Preprojected cache entry (RGB or indexed), no world fallback. */
    uint8_t cached_projected;
    uint16_t gouraud[4];
} sat_scene3d_face_t;

typedef struct sat_scene3d_faces {
    sat_scene3d_face_t* entries;
    uint32_t* keys;
    uint16_t* order;
    uint16_t capacity;
    uint16_t count;
    uint16_t width, height;
    sat_mat4_t view_proj;
    sat_vec3_t eye, forward;
    sat_fx16_t near_depth;
    uint8_t active;
    uint16_t culled_faces;
    uint16_t clipped_faces;
    uint16_t fallback_faces;
    /* Successful face-dispatch calls, not physical VDP1 commands. */
    uint16_t emitted_faces;
    /* Successful preprojected cache-item emissions, not world faces. */
    uint16_t emitted_cached;
    /* Deliberately skipped unsupported faces. */
    uint16_t skipped_faces;
} sat_scene3d_faces_t;

/* All three buffers are caller-owned and must hold `capacity` entries:
 * `storage` the faces, `keys` and `order` the painter's ordering scratch.
 * The library allocates nothing. */
sat_result_t sat_scene3d_faces_init(
    sat_scene3d_faces_t* scene, sat_scene3d_face_t* storage,
    uint32_t* keys, uint16_t* order, uint16_t capacity);

sat_result_t sat_scene3d_faces_begin(
    sat_scene3d_faces_t* scene, const sat_mat4_t* view_proj,
    const sat_vec3_t* eye, const sat_vec3_t* forward,
    sat_fx16_t near_depth, uint16_t width, uint16_t height);

/* Begin using the canonical camera; computes the normalized view forward
 * once. The game no longer needs a second object-painter queue to obtain
 * camera depth and should never calculate painter depth itself. */
sat_result_t sat_scene3d_faces_begin_camera(
    sat_scene3d_faces_t* scene,const sat_camera3d_t* camera,
    sat_fx16_t near_depth,uint16_t width,uint16_t height);

sat_result_t sat_scene3d_faces_depth(
    const sat_scene3d_faces_t* scene,const sat_vec3_t* world,
    sat_fx16_t* out_depth);

/* Material is copied. Both methods are atomic on invalid input/capacity.
 * pass is an explicit artistic override, NOT an occlusion group: all objects
 * needing physical inter-occlusion must use the same pass.
 * Valid range 0..SAT_SCENE3D_PASS_MAX; larger values fail atomically. */
sat_result_t sat_scene3d_faces_submit_quad(
    sat_scene3d_faces_t* scene, const sat_quad3_t* world,
    const sat_scene3d_material_t* material, uint16_t pass);

/* Preprojected cache entry in the SAME painter queue as world geometry.
 * Any valid RGB/indexed-solid/indexed-textured/indexed-tiled material is
 * accepted. Indexed-tiled uses its preuploaded full texture: there is no
 * geometry/UV clipping of a projected cached sprite at flush.
 * The material descriptor is copied; borrowed VRAM texture/tiled storage
 * must remain resident until flush. Gouraud pointers are not accepted for
 * projected cache entries. camera_depth is positive linear 16.16 view depth,
 * NOT the cache's arbitrary (often squared) application sort key.
 * Caller owns camera/view coherence, native VDP1 coords, and clipping. */
sat_result_t sat_scene3d_faces_submit_projected_material(
    sat_scene3d_faces_t* scene, const sat_quad2_t* projected,
    sat_fx16_t camera_depth, const sat_scene3d_material_t* material,
    uint16_t pass);

/* Convenience RGB-only version of the same projected material path. */
sat_result_t sat_scene3d_faces_submit_projected_rgb(
    sat_scene3d_faces_t* scene, const sat_quad2_t* projected,
    sat_fx16_t camera_depth, uint16_t color, uint16_t pass);

/* Reuses the renderer's box-face generator, not application-authored winding.
 * Each wall/top enters the global painter independently. */
sat_result_t sat_scene3d_faces_submit_box(
    sat_scene3d_faces_t* scene, const sat_indexed_box3_t* box,
    uint8_t color_calc_slot, uint16_t pass);

/* A textured deck inset with pre-uploaded quadrant fallback. */
sat_result_t sat_scene3d_faces_submit_tiled_quad(
    sat_scene3d_faces_t* scene, const sat_quad3_t* world,
    const sat_indexed_tiled_quad3_t* regions,
    uint8_t color_calc_slot, uint16_t pass);

/* Shared mesh instance: world==NULL means an already-prepared world pose
 * (e.g. animated pig); a non-null full world matrix instantiates an immutable
 * LOCAL mesh (e.g. gems). Material bindings and culling belong to the instance,
 * while the scene owns camera projection, face ordering and command emission. */
typedef struct sat_scene3d_instance {
    const sat_mesh_t* mesh;
    const sat_scene3d_material_t* materials;
    uint16_t material_count;
    const uint16_t* face_materials;
    const sat_mat4_t* world; /* NULL: mesh vertices already in world space */
    uint16_t pass;
    uint8_t cull_backfaces;
} sat_scene3d_instance_t;

/* A batch item is an immutable instance descriptor plus caller-owned scratch.
 * Scratch is private to the item and may be written by the selected backend
 * while the batch is queued or running. It must not be reused until wait(). */
typedef struct sat_scene3d_prepare_item {
    const sat_scene3d_instance_t* instance;
    sat_projected_vertex_t* screen_scratch;
    sat_vec3_t* world_scratch;
    uint8_t color_calc_slot;
    uint8_t reserved;
} sat_scene3d_prepare_item_t;

typedef struct sat_scene3d_prepare_metrics {
    uint16_t source_faces;
    uint16_t prepared_faces;
    uint16_t culled_faces;
    uint16_t clipped_faces;
} sat_scene3d_prepare_metrics_t;

/* Independent dispatch policy for one batch; zero-init preserves the
 * measured conservative AUTO behavior without compiling any game-specific
 * flags into the generic scene runtime. RUNTIME delegates to the configured
 * executor mode; MASTER explicitly runs locally in every runtime mode. */
typedef enum sat_scene3d_prepare_dispatch {
    SAT_SCENE3D_PREPARE_DISPATCH_CONSERVATIVE = 0,
    SAT_SCENE3D_PREPARE_DISPATCH_RUNTIME = 1,
    SAT_SCENE3D_PREPARE_DISPATCH_MASTER = 2
} sat_scene3d_prepare_dispatch_t;

/* Caller-owned batch storage. The worker writes only faces, keys, order
 * scratch, and metrics; it never writes the scene or VDP1 state. The camera
 * fields are captured by sat_scene3d_prepare_batch_async(). */
typedef struct sat_scene3d_prepare_batch {
    const sat_scene3d_prepare_item_t* items;
    uint16_t item_count;
    uint16_t capacity;
    sat_scene3d_face_t* faces;
    uint32_t* keys;
    uint16_t* order;
    sat_scene3d_prepare_metrics_t metrics;
    sat_mat4_t view_proj;
    sat_vec3_t eye;
    sat_vec3_t forward;
    sat_fx16_t near_depth;
    uint16_t width;
    uint16_t height;
    sat_scene3d_prepare_dispatch_t dispatch;
    sat_parallel_handle_t handle;
    uint8_t pending;
    uint8_t reserved2[3];
} sat_scene3d_prepare_batch_t;

sat_result_t sat_scene3d_prepare_batch_init(
    sat_scene3d_prepare_batch_t* batch,
    const sat_scene3d_prepare_item_t* items, uint16_t item_count,
    sat_scene3d_face_t* faces, uint32_t* keys, uint16_t* order,
    uint16_t capacity);

/* Builds a bounded object-range view of an existing batch.  The item
 * descriptors are copied in source order into caller-owned storage; camera
 * state and the preparation contract are copied as well.  This is the
 * reusable partition primitive used when one portion is dispatched to the
 * Slave and another is prepared concurrently by the Master. The dispatch
 * policy is inherited from the source, but each slice may override it. */
sat_result_t sat_scene3d_prepare_batch_slice(
    const sat_scene3d_prepare_batch_t* source,
    uint16_t first_item, uint16_t item_count,
    sat_scene3d_prepare_item_t* slice_items,
    sat_scene3d_face_t* faces, uint32_t* keys, uint16_t* order,
    uint16_t capacity, sat_scene3d_prepare_batch_t* out_slice);

/* Pure preparation entry point shared by the Master path and the Slave task.
 * It performs no hardware access and does not sort or emit commands. */
sat_result_t sat_scene3d_prepare_batch_execute(
    sat_scene3d_prepare_batch_t* batch);

/* Registers the geometry task type with the existing single Slave owner. */
sat_result_t sat_scene3d_prepare_parallel_register(void);

/* Merges prepared faces into the caller-owned canonical queue. The merge is
 * intentionally separate from preparation: callers can submit/prepare other
 * work on the Master while a batch is running, then merge batches in a stable
 * source order before the one global scene flush. */
sat_result_t sat_scene3d_faces_merge_prepared(
    sat_scene3d_faces_t* scene,
    const sat_scene3d_prepare_batch_t* batch);

/* Every face of the instance takes this slot, exactly as submit_box and
 * submit_tiled_quad apply one slot to the faces they generate. A whole object
 * is what distance fade acts on, so the slot belongs to the submission and not
 * to the shared material table, which would otherwise need one copy per slot.
 * Distinct from SAT_INDEXED_SOLID_OPAQUE (255), itself a valid slot meaning
 * "ordinary opaque sprite". */
#define SAT_SCENE3D_SLOT_INHERIT 254u

/* Projects each world vertex once; copies accepted faces into the queue and
 * retains no pointers to the descriptor, matrix or temporary vertex buffers.
 * screen_scratch holds vertex_count projected points. If world != NULL,
 * world_scratch must hold vertex_count transformed points.
 * color_calc_slot is 0..7, SAT_INDEXED_SOLID_OPAQUE, or
 * SAT_SCENE3D_SLOT_INHERIT to keep each material's own slot. */
sat_result_t sat_scene3d_faces_submit_instance(
    sat_scene3d_faces_t* scene, const sat_scene3d_instance_t* instance,
    uint8_t color_calc_slot,
    sat_projected_vertex_t* screen_scratch, sat_vec3_t* world_scratch);

/* Emits far-to-near; closes the frame even if hardware submission fails.
 * emitted_faces counts successful face-dispatch calls; clipped faces may
 * generate multiple hardware commands. Unsupported skips are counted
 * separately. Unexpected hardware errors propagate to the caller.
 * Caller manages sat_begin_frame/sat_end_frame and HUD command reservation. */
sat_result_t sat_scene3d_faces_flush(sat_scene3d_faces_t* scene);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_SCENE3D_FACES_H */
