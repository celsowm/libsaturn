#ifndef SATURN_SKIN3D_H
#define SATURN_SKIN3D_H

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/collide3d.h"
#include "saturn/math3d.h"
#include "saturn/mesh3d.h"
#include "saturn/skeleton3d.h"
#include "saturn/parallel.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* CPU skinning with joint-local (multi-anchor) vertex positions       */
/* ------------------------------------------------------------------ */
/* Every influence stores its OWN joint-local position, so a vertex is
 *
 *     P = w0 * (M0 * P0) + w1 * (M1 * P1) + w2 * (M2 * P2)
 *
 * where Mi is the joint's model-space matrix (sat_skeleton_joint_world) and
 * P0..P2 may all differ. A conventional single-bind-position mesh converts to
 * this by storing Pi = inverse_bind_i * P_bind at export time; the runtime then
 * needs no inverse-bind matrices at all and the same code serves historical
 * multi-anchor models.
 *
 * Output vertices are the mesh's UNIQUE vertices (indices and materials stay
 * immutable and shared), so a vertex shared by several triangles is
 * transformed once. The asset orders output vertices as
 *
 *     [0, rigid_count)                      single-influence records
 *     [rigid_count, rigid_count+blend_count) seam records (2 or 3 influences)
 *
 * which makes the common case a compact 8-byte record with no type tag.
 *
 * Binary layout (all little-endian-agnostic C structs, naturally aligned to 2):
 *   sat_skin_rigid_t  8 bytes : int16 pos[3] (joint-local), uint8 joint, pad
 *   sat_skin_blend_t 26 bytes : uint8 joint[3], uint8 count (1..3),
 *                               uint16 weight[2] (Q15), int16 pos[3][3]
 *   Weights are Q15 and always sum to exactly 32768: count 2 stores w0 and uses
 *   w1 = 32768 - w0; count 3 stores w0, w1 and uses w2 = 32768 - w0 - w1.
 *   Positions are int16 with `pos_shift` fractional bits (value = q / 2^shift
 *   world units, shift 0..15).
 *   Optional normals: int16[3] per output vertex, Q14 (16384 = 1.0), in the
 *   joint space of the vertex's first influence.
 *
 * Arithmetic is integer: 16.16 matrices times int16 positions accumulate in
 * 64 bits, weights are applied once, and a vertex whose influences carry the
 * same matrix reproduces the single-influence result exactly. Exact half-weight
 * pairs (w0 == 16384) use an add-and-shift with no weight multiplies. */

typedef struct sat_skin_rigid {
    int16_t pos[3];
    uint8_t joint;
    uint8_t reserved;
} sat_skin_rigid_t;

typedef struct sat_skin_blend {
    uint8_t joint[3];
    uint8_t count;
    uint16_t weight[2];
    int16_t pos[3][3];
} sat_skin_blend_t;

typedef struct sat_skin_asset {
    const sat_skin_rigid_t* rigid;
    const sat_skin_blend_t* blend;
    const int16_t* normals; /* optional, 3 per output vertex */
    uint16_t rigid_count;
    uint16_t blend_count;
    uint16_t joint_count;   /* skeleton joints the asset was built against */
    uint8_t pos_shift;
    uint8_t max_joint;      /* highest joint index referenced; checked by validate */
} sat_skin_asset_t;

/* Total output vertices (rigid_count + blend_count). */
uint32_t sat_skin_vertex_count(const sat_skin_asset_t* skin);

/* Walks every record: joint indices < joint_count, counts 1..3, weights that
 * fit, pos_shift <= 15, max_joint truthful. Run once at load. Deform trusts a
 * validated asset and only checks max_joint against the matrices given. */
sat_result_t sat_skin_validate(const sat_skin_asset_t* skin);

/* Copies every joint's model-space matrix into one contiguous array (the form
 * deform wants). Reusable across all skins of the same instance. */
sat_result_t sat_skeleton_world_matrices(
    const sat_skeleton_instance_t* instance, sat_mat4_t* out, uint16_t capacity);

/* Deforms output vertices [first, first + count) into out[v] (absolute index;
 * out_capacity must cover the whole vertex array). Nothing is written when an
 * argument is rejected. This is the unit a parallel task takes: disjoint ranges
 * write disjoint outputs. */
sat_result_t sat_skin_deform_range(
    const sat_skin_asset_t* skin, const sat_mat4_t* joint_world, uint16_t joint_world_count,
    uint32_t first, uint32_t count, sat_vec3_t* out, uint32_t out_capacity);

/* An integral placement applied as each deformed vertex is stored. */
typedef struct sat_skin_placement {
    int32_t scale;
    int32_t offset[3];
    int32_t limit; /* every deformed coordinate must lie in [-limit, limit] */
} sat_skin_placement_t;

/* sat_skin_deform_range, storing scale * v + offset[axis] for each deformed
 * coordinate v. The placement is rejected unless |scale| * limit + |offset|
 * fits int32. When any v lies outside [-limit, limit] the range is still
 * written and SAT_ERR_INVALID_ARG is returned: discard that output. */
sat_result_t sat_skin_deform_range_placed(
    const sat_skin_asset_t* skin, const sat_mat4_t* joint_world, uint16_t joint_world_count,
    uint32_t first, uint32_t count, sat_vec3_t* out, uint32_t out_capacity,
    const sat_skin_placement_t* placement);

sat_result_t sat_skin_deform(
    const sat_skin_asset_t* skin, const sat_mat4_t* joint_world, uint16_t joint_world_count,
    sat_vec3_t* out, uint32_t out_capacity);

/* Rotates the optional normals by the same joints (upper 3x3 only, output
 * 16.16 unit vectors up to quantisation). SAT_ERR_UNSUPPORTED if the asset has
 * no normals. */
sat_result_t sat_skin_deform_normals(
    const sat_skin_asset_t* skin, const sat_mat4_t* joint_world, uint16_t joint_world_count,
    sat_vec3_t* out, uint32_t out_capacity);

/* ------------------------------------------------------------------ */
/* Scene integration                                                   */
/* ------------------------------------------------------------------ */
/* Skinned characters reuse the canonical scene path: the deformed vertices
 * are written into a caller-owned sat_mesh_t whose indices stay immutable, and
 * the mesh goes to sat_scene_submit_instance like any other (materials, face
 * materials, backface culling and the single global painter queue all apply,
 * so two characters sort against each other face by face, never as whole
 * objects). The vertices are in model space; put the character's placement in
 * sat_scene3d_instance_t::world and pass the AABB below as ::bounds so an
 * off-screen character costs O(1). */

/* Every index of the mesh refers to a deformed vertex (index < vertex count)
 * and the mesh has room for them. Load-time check; O(4 * faces). */
sat_result_t sat_skin_validate_mesh(const sat_skin_asset_t* skin, const sat_mesh_t* mesh);

/* Deforms into mesh->vertices and sets mesh->vertex_count. Indices and the face
 * count are not touched. Nothing is written when the mesh cannot hold the
 * vertices (SAT_ERR_CAPACITY) or an argument is rejected. bounds_out (optional)
 * receives the AABB of the deformed vertices grown by `margin` on every side. */
sat_result_t sat_skin_update_mesh(
    const sat_skin_asset_t* skin, const sat_mat4_t* joint_world, uint16_t joint_world_count,
    sat_mesh_t* mesh, sat_fx16_t margin, sat_aabb3_t* bounds_out);

/* Exact AABB of `count` vertices grown by `margin` (>= 0). */
sat_result_t sat_skin_bounds(
    const sat_vec3_t* vertices, uint32_t count, sat_fx16_t margin, sat_aabb3_t* out);

/* ------------------------------------------------------------------ */
/* Character presentation: submeshes, LOD, lighting                    */
/* ------------------------------------------------------------------ */
/* Faces are stored grouped by submesh (and LOD level), so visibility and LOD
 * are face ranges over ONE shared vertex array and index list: a view is just
 * a sat_mesh_t whose indices pointer is offset, which the scene already
 * accepts. Offset face_materials by the same first face. Nothing is copied. */

typedef struct sat_face_range {
    uint16_t first_face;
    uint16_t face_count;
} sat_face_range_t;

/* View of faces [first, first+count) of `mesh`: same vertices, indices offset.
 * Rejects a range outside the mesh (view untouched). */
sat_result_t sat_mesh_face_view(
    const sat_mesh_t* mesh, sat_face_range_t range, sat_mesh_t* out_view);

/* LOD levels ordered from most to least detailed. Picks the first level whose
 * face_count fits `face_budget`. If even the last does not, returns
 * SAT_ERR_CAPACITY (and *out_level = last level) so the caller can decide
 * explicitly (draw anyway, skip the frame, shrink another character) instead
 * of faces vanishing silently. */
sat_result_t sat_lod_select(
    const sat_face_range_t* levels, uint16_t level_count, uint16_t face_budget,
    uint16_t* out_level);

/* Flat directional lighting for a (deformed) mesh. light_dir points TOWARD the
 * light and need not be normalised; ambient is 16.16 in [0, 1]. Writes one
 * ramp index per face into out_face_materials, ready for
 * sat_scene3d_instance_t::face_materials over a material table that is a dark
 * to bright ramp (ramp_count >= 2 entries). Face normal = cross(D - A, B - A)
 * (outward, see mesh3d.h). Degenerate faces get the ambient index. Intensity =
 * ambient + (1 - ambient) * max(0, n . l). */
sat_result_t sat_mesh_face_ramp(
    const sat_mesh_t* mesh, const sat_vec3_t* light_dir, sat_fx16_t ambient,
    uint16_t ramp_count, uint16_t* out_face_materials, uint16_t capacity);

/* Bounded asynchronous skin task; skin must have passed sat_skin_validate.
 * Descriptor, nested sources and output must
 * remain alive until the handle is terminal and released. Register once after
 * sat_parallel_init. Output ownership follows the existing executor; a failed
 * wait is not permission to overwrite a still-running worker's span. */
#define SAT_PARALLEL_TASK_SKIN_DEFORM 4u
typedef struct sat_skin_job {
    const sat_skin_asset_t* skin;
    const sat_mat4_t* joint_world;
    sat_vec3_t* output;
    uint32_t output_capacity;
    uint32_t first, count;
    uint16_t joint_world_count;
    uint16_t reserved;
} sat_skin_job_t;
sat_result_t sat_skin_parallel_register(void);
sat_result_t sat_skin_job_submit(const sat_skin_job_t* job,
    sat_parallel_mode_t policy, sat_parallel_handle_t* handle);

typedef struct sat_skin_requirements {
    uint32_t skeleton_bytes, skin_bytes, vertex_bytes, joint_matrix_bytes;
    uint32_t pose_bytes, node_bytes, hierarchy_scratch_bytes, command_upper_bound;
} sat_skin_requirements_t;
sat_result_t sat_skin_requirements(const sat_skeleton_asset_t* skeleton,
    const sat_skin_asset_t* skin, uint16_t faces, sat_skin_requirements_t* out);

/* Joint attachment in application world space; local may be NULL (identity). */
sat_result_t sat_skeleton_attachment(const sat_skeleton_instance_t* instance,
    uint16_t joint, const sat_mat4_t* character_world, const sat_mat4_t* local,
    sat_mat4_t* out);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_SKIN3D_H */
