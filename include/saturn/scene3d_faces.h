#ifndef SATURN_SCENE3D_FACES_H
#define SATURN_SCENE3D_FACES_H

#include <stdint.h>
#include "saturn/scene3d.h"
#include "saturn/model3d.h"
#include "saturn/collide3d.h"
#include "saturn/parallel.h"
#include "saturn/vdp1_command_batch.h"

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
    /* RGB: the polygon colour. INDEXED_TEXTURED: optional fallback colour
     * (0 = none). A distorted sprite cannot be clipped, so a textured face
     * with a corner behind the near plane is otherwise dropped whole; with a
     * fallback it is drawn as a near-clipped solid polygon of this colour
     * instead (opaque: an RGB polygon has no colour-calc slot). */
    uint16_t rgb555;
    const sat_vdp1_texture_t* texture;
    /* Only used for INDEXED_TILED; descriptor must survive until flush. */
    const sat_indexed_tiled_quad3_t* tiled;
    /* 255 = ordinary opaque sprite; 0..7 = previously configured VDP2 slot.
     * RGB polygon material does not support the indexed-sprite slots. */
    uint8_t color_calc_slot; /* Also accepts SAT_VDP2_SPRITE_ADD for INDEX8. */
    /* Optional per-vertex Gouraud table. The pointer is read only during
     * submission and the four selected entries are copied into the face. */
    const uint16_t* vertex_gouraud; /* RGB or LUT4/RGB-CLUT; INDEX8 is rejected. */
    /* Optional SAT_SPRITE_FLAG_MESH coverage for indexed sprite materials.
     * Zero preserves full coverage; this is screen-door coverage, not alpha. */
    uint16_t sprite_flags;
} sat_scene3d_material_t;

/* Optional immutable projected-command prefixes. Entries are opaque compiler
 * output; a NULL texture marks a material that retains general dispatch.
 * Compile after texture upload and retain table/storage until all queued
 * faces/tasks using it have flushed, completed and released. Rebuild after
 * any material/descriptor mutation; ordinary residency checks still apply. */
typedef sat_vdp1_projected_program_t sat_scene3d_material_program_t;

typedef struct sat_scene3d_material_programs {
    const sat_scene3d_material_t* materials;
    sat_scene3d_material_program_t* entries;
    uint16_t count;
    uint8_t valid;
    uint8_t all_compiled;
} sat_scene3d_material_programs_t;

/* Unsupported RGB/Gouraud/color-calc cases are retained as general entries.
 * Fails atomically for the output table; entry storage may be partially written. */
sat_result_t sat_scene3d_material_programs_init(
    sat_scene3d_material_programs_t* out,
    const sat_scene3d_material_t* materials,uint16_t count,
    sat_scene3d_material_program_t* storage,uint16_t capacity);

/* 8-bit artistic pass in the painter key, leaving 24 bits of depth (faces
 * order by depth out to 4096 world units). Values beyond this fail
 * atomically. */
#define SAT_SCENE3D_PASS_MAX 255u

typedef struct sat_scene3d_profile {
    uint32_t projection_ticks, preparation_ticks, painter_ticks, emission_ticks;
} sat_scene3d_profile_t;
#define SAT_SCENE3D_INSTANCE_PROFILE_AVAILABLE 1
/* Last instance submission / flush FRT costs. UNSUPPORTED when profiling was
 * compiled out; callers must not present zero as a measured cost. */
sat_result_t sat_scene3d_profile_get(sat_scene3d_profile_t* out);

/* Paint order is NOT a field here: it lives in the scene's parallel key
 * array, so ordering a frame moves two-byte indices instead of these
 * multi-word records. program_valid selects an immutable compiled prefix
 * instead of a copied material; only that union member may be read.
 *
 * Every field a merge, flush or emission reads comes first; `world` is last
 * and is written, copied and read ONLY when projected_safe == 0 (the
 * clipping fallback). For a projected-safe face its contents are
 * unspecified. Ordinary batches write these records through uncached Slave memory and
 * copy again on merge. Direct projected spans avoid those general records
 * except when a cold fallback is required. */
typedef struct sat_scene3d_face {
    sat_quad2_t projected;
    union {
        sat_scene3d_material_t material;
        const sat_scene3d_material_program_t* program;
    };
    uint8_t projected_safe;
    uint8_t gouraud_valid;
    /* Preprojected cache entry (RGB or indexed), no world fallback. */
    uint8_t cached_projected;
    /* Selects the program union member; projected-safe, opaque, unshaded. */
    uint8_t program_valid;
    uint16_t gouraud[4];
    sat_quad3_t world;
} sat_scene3d_face_t;

/* Optional hot queue for projected, opaque commands. 24 bytes on SH-2;
 * uncommon materials/clipping retain a cold ordinary face. */
typedef enum sat_scene3d_projected_kind {
    SAT_SCENE3D_PROJECTED_RGB = 0,
    SAT_SCENE3D_PROJECTED_PROGRAM = 1,
    SAT_SCENE3D_PROJECTED_COLD = 2
} sat_scene3d_projected_kind_t;
typedef struct sat_scene3d_projected_face {
    sat_quad2_t projected;
    union {
        const sat_scene3d_material_program_t* program;
        const sat_scene3d_face_t* cold;
        uint16_t rgb555;
    };
    uint8_t kind;
    uint8_t cached_projected;
    uint16_t reserved;
} sat_scene3d_projected_face_t;

typedef sat_result_t (*sat_scene3d_owner_validate_fn)(
    void* context,uint16_t slot,uint16_t generation,
    const sat_vdp1_texture_t* expected_native);

/* One generation-checked texture owner for a contiguous run of queued faces
 * (a whole managed view). Validated once per span at flush, so ordinary
 * faces carry no owner data at all. */
typedef struct sat_scene3d_owner_span {
    uint16_t first;
    uint16_t count;
    uint16_t slot;
    uint16_t generation;
    const sat_vdp1_texture_t* native;
} sat_scene3d_owner_span_t;

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
    /* Optional scene-wide ownership verifier, injected by an L3 adapter.
     * Its context must remain live through flush. No dependency on a texture
     * manager is imposed on low-level face submission or bare scene drawing. */
    sat_scene3d_owner_validate_fn validate_owner;
    void* owner_context;
    /* Caller-owned owner spans, bound with the verifier; reset each begin. */
    sat_scene3d_owner_span_t* owner_spans;
    uint16_t owner_span_capacity;
    uint16_t owner_span_count;
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
    /* When bound, entries is cold storage rather than the logical queue. */
    sat_scene3d_projected_face_t* projected_entries;
    uint16_t cold_count;
    /* Cold records available in entries; 0 means capacity. A face that needs
     * a cold record beyond it is skipped and counted in skipped_faces. */
    uint16_t cold_capacity;
    /* Optional logical-to-physical map for disjoint queue writers. */
    uint16_t* projected_indices;
    uint16_t projected_gap;
    struct sat_scene3d_prepare_batch* reserved_batch;
    /* Optional experimental staged native command arena. A hot record with
     * reserved=1 borrows its coordinates from the corresponding command. */
    sat_vdp1_native_command_t* native_commands;
    sat_vdp1_command_batch_t* native_batch;
    /* Per-scene diagnostic accumulators: separate actor scenes have separate
     * owners even when their submissions run on different CPUs. Zero when
     * the SDK was built without profile metrics. */
    sat_scene3d_profile_t instance_profile;
} sat_scene3d_faces_t;

/* Call after begin and before any face, with an already reserved frame batch.
 * Requires LIBSATURN_NATIVE_SCENE_COMMANDS. The scene completes/cancels the
 * batch at flush; outstanding workers retain ownership on timeout. */
sat_result_t sat_scene3d_faces_bind_native_commands(
    sat_scene3d_faces_t* scene,sat_vdp1_command_batch_t* batch);

/* All three buffers are caller-owned and must hold `capacity` entries:
 * `storage` the faces, `keys` and `order` the painter's ordering scratch.
 * The library allocates nothing. */
sat_result_t sat_scene3d_faces_init(
    sat_scene3d_faces_t* scene, sat_scene3d_face_t* storage,
    uint32_t* keys, uint16_t* order, uint16_t capacity);

/* Bind only between frames. Ordinary entry storage still covers capacity
 * and holds uncommon faces; projected storage must also cover capacity.
 * Retain both buffers through flush. NULL returns to ordinary queue mode. */
sat_result_t sat_scene3d_faces_use_projected_storage(
    sat_scene3d_faces_t* scene,sat_scene3d_projected_face_t* storage,uint16_t capacity);
/* Enables direct batch spans. Bind capacity indices between frames, after
 * projected storage. Retain all queue buffers through flush. */
sat_result_t sat_scene3d_faces_use_projected_indices(
    sat_scene3d_faces_t* scene,uint16_t* indices,uint16_t capacity);
/* Expand a queued record for diagnostics; never read entries[i] directly
 * when projected storage is bound. Unused world/Gouraud fields are unspecified. */
sat_result_t sat_scene3d_faces_get(
    const sat_scene3d_faces_t* scene,uint16_t index,sat_scene3d_face_t* out);

/* Configure the optional owner verifier only while no frame is active.
 * `spans` (caller-owned, `span_capacity` entries) holds one record per
 * owner-checked run of faces per frame. A NULL verifier unbinds both. */
sat_result_t sat_scene3d_faces_bind_owner_validator(
    sat_scene3d_faces_t* scene,
    sat_scene3d_owner_validate_fn validate,void* context,
    sat_scene3d_owner_span_t* spans,uint16_t span_capacity);

/* Marks already-queued faces [first, first + count) as owned by
 * (slot, generation) referencing `native`. The verifier checks the owner
 * once before any VDP1 emission; a stale owner rejects the whole flush.
 * Needs a bound verifier; SAT_ERR_CAPACITY when the span storage is full
 * (check sat_scene3d_faces_owner_span_available() before queueing). */
sat_result_t sat_scene3d_faces_mark_owner(
    sat_scene3d_faces_t* scene,uint16_t first,uint16_t count,
    uint16_t slot,uint16_t generation,const sat_vdp1_texture_t* native);
uint8_t sat_scene3d_faces_owner_span_available(
    const sat_scene3d_faces_t* scene);

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

/* sat_scene3d_faces_submit_quad, cut along up to SAT_SCENE3D_SPLIT_PLANES_MAX
 * planes first; every piece enters the painter as its own face with its own
 * depth. Two uses:
 *  - faces that pass THROUGH each other (a pig's leg through a deck): cut
 *    each along the other's plane and every piece lies on one side, which a
 *    per-face order can draw correctly. No pass or depth key can do that.
 *  - a large face under a small object (a deck under an actor): cut the deck
 *    along the object's bounding planes, so the piece under the object sorts
 *    by where the object is, not by the deck's far-away centre. This is what
 *    authored passes were used for.
 * A piece wholly on one side of a plane is kept whole; a straddling piece is
 * clipped into a quad plus at most one triangle per side. Normals should be
 * unit length (as sat_plane3_t elsewhere). At most SAT_SCENE3D_SPLIT_PIECES_MAX
 * pieces; more returns SAT_ERR_CAPACITY. Solid materials only: a textured or
 * per-vertex Gouraud face would lose its UVs or shading in the cut, so those
 * return SAT_ERR_UNSUPPORTED (bake the split offline with
 * tools/import_model.py --split-intersections instead). Queue-atomic like
 * sat_scene3d_faces_submit_box. */
#define SAT_SCENE3D_SPLIT_PLANES_MAX 4u
#define SAT_SCENE3D_SPLIT_PIECES_MAX 16u
sat_result_t sat_scene3d_faces_submit_quad_split(
    sat_scene3d_faces_t* scene, const sat_quad3_t* world,
    const sat_scene3d_material_t* material, uint16_t pass,
    const sat_plane3_t* planes, uint8_t plane_count);

/* Preprojected cache entry in the SAME painter queue as world geometry.
 * Any valid RGB/indexed-solid/indexed-textured/indexed-tiled material is
 * accepted. Indexed-tiled uses its preuploaded full texture: there is no
 * geometry/UV clipping of a projected cached sprite at flush.
 * The descriptor is copied. Projected tiled materials are reduced at submit
 * to their preuploaded full texture (the tiled descriptor need not survive
 * the call); the chosen texture and its VRAM backing must remain valid
 * through flush. Gouraud pointers are not accepted for
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
/* Compound submission is queue-atomic: later projection failure restores
 * the pre-existing face span and admission statistics without VDP1 emission. */
sat_result_t sat_scene3d_faces_submit_box(
    sat_scene3d_faces_t* scene, const sat_indexed_box3_t* box,
    uint8_t color_calc_slot, uint16_t pass);

/* A textured deck inset with pre-uploaded quadrant fallback. */
sat_result_t sat_scene3d_faces_submit_tiled_quad(
    sat_scene3d_faces_t* scene, const sat_quad3_t* world,
    const sat_indexed_tiled_quad3_t* regions,
    uint8_t color_calc_slot, uint16_t pass);

/* A validated immutable topology snapshot. Building one costs O(face_count)
 * once; a matching instance may then skip that same index/material-map walk
 * every frame. Vertex positions, vertex-buffer addresses, mesh descriptors
 * and capacities
 * may change between submissions, provided their validated counts match and
 * the current buffers cover those counts. This supports double-buffered poses
 * and independent instances of the same topology. Indices, face-material map,
 * material table and referenced descriptors must remain immutable for the
 * binding lifetime; rebuild the binding after changing them. Each submission
 * still checks current buffer capacities. Material residency is rechecked
 * before flush. */
typedef struct sat_scene3d_instance_binding {
    const uint16_t* indices;
    uint16_t vertex_count;
    uint16_t face_count;
    const sat_scene3d_material_t* materials;
    uint16_t material_count;
    const uint16_t* face_materials;
    uint8_t has_rgb;
    uint8_t valid;
    /* Optional caller-compiled table for precisely these materials/count.
     * Attach before source-plan initialization; table/storage is immutable. */
    const sat_scene3d_material_programs_t* programs;
} sat_scene3d_instance_binding_t;

sat_result_t sat_scene3d_instance_binding_init(
    sat_scene3d_instance_binding_t* out,
    const sat_mesh_t* mesh,
    const sat_scene3d_material_t* materials,
    uint16_t material_count,
    const uint16_t* face_materials);

/* Shared mesh instance: validates nonempty mesh counts against declared
 * buffer capacities before reading vertices/indices; restores previous
 * queue/cull/clip counts if any face of the compound operation fails.
 * world==NULL means an already-prepared world pose
 * (e.g. animated pig); a non-null full world matrix instantiates an immutable
 * LOCAL mesh (e.g. gems). Material bindings and culling belong to the instance,
 * while the scene owns camera projection, face ordering and command emission.
 * An optional conservative bounds pointer can reject a whole off-frustum
 * instance before vertex transformation/projection and per-face validation.
 * Zero-initialize the whole descriptor before assigning required fields:
 * every optional pointer is NULL-by-default, and future trailing optional
 * fields must not inherit indeterminate stack data. */
#define SAT_SCENE3D_CULL_SCREEN 2u
typedef struct sat_scene3d_instance {
    const sat_mesh_t* mesh;
    const sat_scene3d_material_t* materials;
    uint16_t material_count;
    const uint16_t* face_materials;
    const sat_mat4_t* world; /* NULL: mesh vertices already in world space */
    uint16_t pass;
    /* 0 draws both sides; nonzero culls back faces by projected area and the
     * world normal. SAT_SCENE3D_CULL_SCREEN culls by projected area alone
     * (like a PlayStation NCLIP); the world normal then decides only faces
     * with a corner behind the camera. */
    uint8_t cull_backfaces;
    /* Optional per-face mask: a nonzero byte draws that face from either side. */
    const uint8_t* double_sided_faces;
    /* Optional conservative AABB used for an O(1)-sized frustum rejection
     * before per-face work. When world != NULL the box is in LOCAL mesh
     * space and is transformed by world; when world == NULL it is already in
     * WORLD space, like the mesh vertices. The box is borrowed only for the
     * submission call. A fully rejected bounded instance deliberately skips
     * O(face_count) face-table validation; immutable assets should therefore
     * be validated when they are built/loaded, not every frame. */
    const sat_aabb3_t* bounds;
    /* Optional O(1) proof that immutable index/material topology was already
     * validated. A topology pointer/count mismatch rejects submission rather
     * than silently falling back to a potentially stale token. */
    const sat_scene3d_instance_binding_t* validated_binding;
    /* Nonzero: screen_scratch already holds the mesh's vertices projected
     * with this scene's camera (e.g. sat_skin_project_range), and world must
     * be NULL. Submission then skips projection and never reads the mesh
     * vertices. That needs the compiled fast path (every corner in front of
     * the camera and drawable, culling off or SAT_SCENE3D_CULL_SCREEN);
     * otherwise SAT_ERR_UNSUPPORTED is returned with nothing appended, and
     * the caller submits world vertices instead. sat_scene_submit_instance
     * does not record that refusal as a frame error. */
    uint8_t pre_projected;
} sat_scene3d_instance_t;

/* Compiled publication plan for immutable topology/material resources.
 * Initialize on Master after binding validation and resource loading. Storage
 * is caller-owned Work RAM and must outlive all tasks using it. Treat the plan,
 * range storage, binding topology, material table and referenced texture/tiled
 * descriptors as immutable until every such task has completed AND released.
 * Rebuild the binding and plan after changing any of those resources, even
 * when pointer identities are unchanged. Never copy a plan to republish it.
 * This is a cache publication contract, not proof of live VRAM ownership:
 * normal residency/owner checks still apply at flush.
 * Vertex Gouraud values may change between tasks; their addresses stay fixed.
 * Vertices, mesh descriptors, bounds, transforms, face masks and scratch remain
 * per-task data. Receiver invalidation is retained on every backend, so a
 * shutdown/restart or Master fallback needs no cached acknowledgment reset.
 * NULL/empty ranges consume no capacity. On failure out is invalid; range
 * storage may have been written. Initialization must not overlap any user of
 * out or its range storage. Zero-init prepare items for the optional pointer. */
typedef struct sat_scene3d_source_range {
    const void* address;
    uint32_t bytes;
} sat_scene3d_source_range_t;

typedef struct sat_scene3d_source_plan {
    sat_scene3d_instance_binding_t topology;
    sat_scene3d_source_range_t* ranges;
    uint16_t static_count;
    uint16_t range_count;
} sat_scene3d_source_plan_t;

sat_result_t sat_scene3d_source_plan_init(
    sat_scene3d_source_plan_t* out, const sat_scene3d_instance_t* instance,
    sat_scene3d_source_range_t* ranges, uint16_t capacity);

/* A batch item is an immutable instance descriptor plus caller-owned scratch.
 * Scratch is private to the item and may be written by the selected backend
 * while the batch is queued or running. It must not be reused until wait(). */
typedef struct sat_scene3d_prepare_item {
    const sat_scene3d_instance_t* instance;
    sat_projected_vertex_t* screen_scratch;
    sat_vec3_t* world_scratch;
    uint8_t color_calc_slot;
    uint8_t reserved;
    const sat_scene3d_source_plan_t* source_plan; /* NULL: conservative graph walk */
} sat_scene3d_prepare_item_t;

typedef struct sat_scene3d_prepare_metrics {
    uint16_t source_faces;
    uint16_t prepared_faces;
    uint16_t culled_faces;
    uint16_t clipped_faces;
    /* Faces dropped because the cold store was full. */
    uint16_t skipped_faces;
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

/* Caller-owned batch storage. The worker writes only faces, keys and
 * metrics; it never sorts or writes the scene/VDP1 state. The camera
 * fields are captured by sat_scene_prepare_batch_async(). */
typedef struct sat_scene3d_prepare_batch {
    const sat_scene3d_prepare_item_t* items;
    uint16_t item_count;
    uint16_t capacity;
    sat_scene3d_face_t* faces;
    uint32_t* keys;
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
    /* Internal direct-span ownership; initialize with the APIs below. */
    sat_scene3d_projected_face_t* projected_faces;
    sat_scene3d_faces_t* direct_owner;
    uint16_t direct_first;
    /* Direct-span batches: cold records requested (0: capacity). When the
     * scene's cold store is bounded, reserve lowers it to what is left,
     * possibly 0; the batch then drops and counts its extra cold faces. */
    uint16_t cold_capacity;
    sat_vdp1_native_command_t* native_commands;
    sat_scene3d_profile_t instance_profile;
} sat_scene3d_prepare_batch_t;

sat_result_t sat_scene3d_prepare_batch_init(
    sat_scene3d_prepare_batch_t* batch,
    const sat_scene3d_prepare_item_t* items, uint16_t item_count,
    sat_scene3d_face_t* faces, uint32_t* keys, uint16_t capacity);

/* Reserve one disjoint projected/cold span for a batch before dispatch. The
 * Master may append concurrently. Merge publishes indices/keys in logical
 * source order without copying face records. One outstanding reservation per
 * scene; flush stays BUSY until merge or cancellation. Capacity is the worst
 * case, including culled faces. Unused reserved slots last until next begin.
 * Do not copy/reinitialize the scene or batch while a reservation is held.
 * Retain the canonical queue through flush; batch keys may be reused after
 * merge and task release. A cancelled task must finish and release first. */
sat_result_t sat_scene3d_faces_reserve_batch(
    sat_scene3d_faces_t* scene,sat_scene3d_prepare_batch_t* batch);
sat_result_t sat_scene3d_faces_cancel_batch(
    sat_scene3d_faces_t* scene,sat_scene3d_prepare_batch_t* batch);

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
    sat_scene3d_face_t* faces, uint32_t* keys, uint16_t capacity,
    sat_scene3d_prepare_batch_t* out_slice);

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
 * SAT_SCENE3D_SLOT_INHERIT to keep each material's own slot. A matching
 * validated_binding removes the separate O(face_count) topology preflight;
 * the visible-face walk itself remains O(face_count). */
sat_result_t sat_scene3d_faces_submit_instance(
    sat_scene3d_faces_t* scene, const sat_scene3d_instance_t* instance,
    uint8_t color_calc_slot,
    sat_projected_vertex_t* screen_scratch, sat_vec3_t* world_scratch);

/* Revalidates all borrowed material descriptors before emitting ANY VDP1
 * commands. If a texture became invalid after submission, the entire queued
 * painter is rejected atomically with SAT_ERR_INVALID_ARG and zero emissions.
 * This cannot guard against overwriting still-valid VRAM or dangling pointers:
 * raw callers must retain texture/tiled descriptors AND their VRAM ownership
 * until the hardware finishes consuming commands. Opt-in generation-checked
 * owners detect release/reuse at flush, but do not lock VRAM after emission.
 * Hardware command errors may still be partial and irreversible.
 * Emits far-to-near; closes the frame even if hardware submission fails.
 * emitted_faces counts successful face-dispatch calls; clipped faces may
 * generate multiple hardware commands. Unsupported skips are counted
 * separately. Unexpected hardware errors propagate to the caller.
 * Caller manages sat_begin_frame/sat_end_frame and HUD command reservation. */
sat_result_t sat_scene3d_faces_flush(sat_scene3d_faces_t* scene);

/* Opt-in capture of direct draws into the painter. While a capture is open
 * the render3d.h world-quad draws -- sat_draw_polygon_quad3, its _gouraud
 * form, sat_draw_indexed_solid_quad3, sat_draw_indexed_textured_quad3 and
 * sat_draw_indexed_tiled_quad3 (and so sat_draw_indexed_box3 and
 * sat_draw_indexed_solid_mesh3, which draw through them) -- queue each quad
 * into this scene at `pass` instead of drawing it at once, so it sorts with
 * the scene's faces rather than landing on top of or under all of them.
 * The draw's own camera parameters are NOT used: the scene's camera projects
 * the quad, so they must describe the same view. Its colour-calc slot is
 * kept. The draw returns the queue result (SAT_ERR_CAPACITY when full); the
 * textured/tiled out counts read 1 for a queued quad.
 * One capture at a time, on the Master; a flush of the capturing scene
 * returns SAT_ERR_BUSY until sat_scene3d_capture_end. out_captured
 * (optional) receives the faces queued during the capture. */
sat_result_t sat_scene3d_capture_begin(sat_scene3d_faces_t* scene, uint16_t pass);
sat_result_t sat_scene3d_capture_end(sat_scene3d_faces_t* scene, uint16_t* out_captured);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_SCENE3D_FACES_H */
