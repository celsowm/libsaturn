#ifndef SATURN_PHYSICS3_WORLD_H
#define SATURN_PHYSICS3_WORLD_H
#include <stdint.h>
#include "saturn/collide3d.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Fixed-tick, caller-owned sphere/box physics. No heap or graphics dependency. */
typedef enum sat_physics3_kind {
    SAT_PHYSICS3_STATIC_BOX=1,
    SAT_PHYSICS3_KINEMATIC_BOX=2,
    SAT_PHYSICS3_DYNAMIC_SPHERE=3,
    SAT_PHYSICS3_STATIC_PLANE=4,
    SAT_PHYSICS3_STATIC_MESH=5,
    SAT_PHYSICS3_KINEMATIC_MESH=6
} sat_physics3_kind_t;
typedef struct sat_physics3_material {
    sat_fx16_t friction;    /* [0,ONE]; tangent damping on support */
    sat_fx16_t restitution; /* [0,ONE]; normal bounce */
} sat_physics3_material_t;
/* Unit rotation quaternion; (0,0,0,ONE) is the identity. Rolling spin is
 * world-space radians per fixed tick. Renderer can construct a rotation
 * matrix from this quaternion independently of the physics world. */
typedef struct sat_physics3_quat {
    sat_fx16_t x,y,z,w;
} sat_physics3_quat_t;
typedef struct sat_physics3_actor {
    sat_physics3_kind_t kind;
    sat_physics3_material_t material;
    sat_body3_t sphere;
    sat_vec3_t angular_velocity; /* world-space radians per fixed tick */
    sat_physics3_quat_t orientation;
    uint8_t rolling_enabled; /* per-sphere opt-in; legacy damping otherwise */
    /* Sphere/sphere contacts only: relative mass (fx16). 0 means ONE, so
     * zero-initialised actors weigh the same. */
    sat_fx16_t mass;
    sat_aabb3_t box;
    sat_plane3_t plane; /* Infinite, two-sided and static. */
    const sat_mesh_t* mesh; /* Borrowed, immutable WORLD-space quad mesh. */
    sat_mesh3_grid_t* mesh_grid; /* Optional caller-owned acceleration grid. */
    sat_vec3_t mesh_offset; /* Kinematic mesh translation, relative to vertex storage. */
    sat_vec3_t mesh_bounds_min,mesh_bounds_max; /* Cached reference AABB extrema. */
    sat_physics3_quat_t mesh_orientation,mesh_target_orientation;
    sat_physics3_quat_t mesh_tick_start_orientation; /* Internal substep anchor. */
    sat_vec3_t target_center; /* kinematic target at end of NEXT tick */
    sat_vec3_t frame_motion;  /* displacement per tick, zero for static boxes */
} sat_physics3_actor_t;
/* One node per finite collider plus one per internal BVH branch.
 * Six 64-bit world-coordinate limits preserve touching contacts even at
 * Q16.16 extremes. The caller owns a [2 * world.capacity - 1] node pool. */
typedef struct sat_physics3_spatial_node {
    int64_t low_x,low_y,low_z,high_x,high_y,high_z;
    uint32_t left,right; /* Internal child indices; ignored for leaves. */
    uint16_t actor_id;   /* 0xffffu on internal branches. */
} sat_physics3_spatial_node_t;

typedef struct sat_physics3_world {
    sat_physics3_actor_t* actors;
    sat_contact3_t* mesh_contacts; /* Caller-owned shared query scratch. */
    uint16_t mesh_contact_capacity;
    /* Optional caller-owned, capacity-sized index buffer. Stable collider
     * indices are compacted once per tick; no per-ball dynamic-body scan. */
    uint16_t* collider_indices;
    uint16_t collider_index_capacity;
    /* Optional deterministic BVH over finite non-sphere colliders, rebuilt
     * ONCE per tick from immutable/static and swept kinematic world bounds.
     * Caller-owned node and query arrays; a plane stays in collider_indices.
     * When absent the legacy linear and typed-collider paths are unchanged. */
    sat_physics3_spatial_node_t* spatial_nodes;
    uint16_t* spatial_candidates;
    uint32_t spatial_node_capacity;
    uint16_t spatial_candidate_capacity;
    uint16_t spatial_leaf_count;
    uint16_t spatial_fallback_count;
    uint32_t spatial_root;
    uint32_t spatial_queries; /* Per-tick diagnostic counters. */
    uint32_t spatial_candidates_checked;
    /* Optional sphere/sphere contacts: caller-owned sweep order, one entry
     * per actor. NULL keeps spheres passing through each other. */
    uint16_t* sphere_pair_order;
    uint16_t sphere_pair_capacity;
    uint16_t sphere_pair_contacts; /* Per-tick diagnostic counter. */
    sat_vec3_t gravity; /* velocity delta per fixed tick */
    uint16_t count,capacity;
    uint8_t max_substeps; /* 1..64 */
    uint8_t iterations; /* 1..8 */
    uint8_t mesh_face_ccd; /* Opt-in face/edge/vertex mesh sweeps (static + translating). */
    uint8_t kinematic_box_ccd; /* Opt-in moving AABB relative sweep. */
} sat_physics3_world_t;
sat_result_t sat_physics3_world_init(sat_physics3_world_t* world,
    sat_physics3_actor_t* storage,uint16_t capacity,sat_vec3_t gravity,
    uint8_t max_substeps,uint8_t iterations);
void sat_physics3_world_reset(sat_physics3_world_t* world);
sat_result_t sat_physics3_add_box(sat_physics3_world_t* world,
    sat_physics3_kind_t kind,const sat_aabb3_t* box,
    const sat_physics3_material_t* material,uint16_t* out_id);
/* Planar slope collider: infinite and two-sided; finite ramps/holes require
 * the later mesh-collision world integration. */
sat_result_t sat_physics3_add_plane(sat_physics3_world_t* world,
    const sat_plane3_t* plane, const sat_physics3_material_t* material,
    uint16_t* out_id);
/* Set shared scratch before adding a mesh. Capacity must be at least the
 * largest registered mesh face_count, ensuring every contact is retained.
 * Storage must outlive the world; there is no allocation or hidden fallback. */
sat_result_t sat_physics3_set_mesh_contacts(
    sat_physics3_world_t* world, sat_contact3_t* storage, uint16_t capacity);
/* Optional type-based candidate list for worlds with many dynamic spheres.
 * Requires >= world.capacity uint16 entries and lifetime through world_step;
 * NULL/0 detaches. Preserves original actor order and collision semantics.
 * Does not replace the independent mesh-grid spatial broadphase. */
sat_result_t sat_physics3_set_collider_index_scratch(
    sat_physics3_world_t* world, uint16_t* indices, uint16_t capacity);

/* Opt-in BVH over static/kinematic boxes and finite meshes; infinite planes
 * remain a small, source-ordered fallback. Must first bind collider scratch
 * with >=world.capacity slots; nodes need >=2*capacity-1 elements (uint32
 * capacity), candidates need >=capacity IDs. NULL/0 detaches all BVH storage.
 * Any failed bind leaves the previous configuration unchanged. The index is
 * rebuilt before actor mutation on each successful world_step; moving boxes
 * and translating/rotating meshes use conservative FULL-TICK swept bounds,
 * so CCD and iterative contact queries cannot miss a moving collider.
 * Candidate IDs are ordered by original actor ID before narrowphase.
 * No heap allocation, globals, game dependency, or hardware access. */
sat_result_t sat_physics3_set_spatial_broadphase(
    sat_physics3_world_t* world,
    sat_physics3_spatial_node_t* nodes,uint32_t node_capacity,
    uint16_t* candidates,uint16_t candidate_capacity);

/* Opt-in sphere/sphere collision. `order` (>= world.capacity entries,
 * outliving the world) holds the dynamic spheres sorted by their low x
 * extent; each substep re-sorts it (insertion sort, stable by actor ID) and
 * sweeps it, so only pairs overlapping on x reach the narrowphase. Contacts
 * separate the pair along the centre line in inverse proportion to `mass`
 * and apply a restitution impulse (the smaller restitution of the two).
 * Radii must stay below 8192 units while enabled. NULL/0 detaches and
 * restores the exact previous behaviour. */
sat_result_t sat_physics3_set_sphere_pairs(
    sat_physics3_world_t* world, uint16_t* order, uint16_t capacity);
/* Relative mass of a dynamic sphere for sphere/sphere contacts,
 * 0 < mass <= 4096.0. */
sat_result_t sat_physics3_set_mass(
    sat_physics3_world_t* world, uint16_t sphere_id, sat_fx16_t mass);

/* Mesh vertices and indices are already in world coordinates, remain immutable,
 * and must outlive the collider. Finite quad faces retain their edges and gaps.
 * The default path linearly tests mesh faces; add_mesh_grid is opt-in. */
sat_result_t sat_physics3_add_mesh(
    sat_physics3_world_t* world, const sat_mesh_t* mesh,
    const sat_physics3_material_t* material, uint16_t* out_id);

/* Registers an existing, initialized spatial grid for an immutable static
 * world-space mesh. The grid owns its query stamps, so the world borrows it
 * MUTABLY for the lifetime of the collider. Do not rebuild or independently
 * query a registered grid concurrently with world_step(). The world does not
 * build a second grid or allocate; the contact scratch contract is unchanged.
 * Grid query order may differ from the linear mesh face order when multiple
 * contacts are present. */
sat_result_t sat_physics3_add_mesh_grid(
    sat_physics3_world_t* world, sat_mesh3_grid_t* grid,
    const sat_physics3_material_t* material, uint16_t* out_id);

/* Moving finite quad geometry with a translation-only pose. Mesh vertices
 * remain immutable in their authored reference coordinate system. The optional
 * grid indexes THAT untransformed mesh; both are borrowed and never rebuilt.
 * initial_offset and next target are translations of the entire mesh,
 * not a vertex edit or world-space mesh copy. Contact scratch still must fit
 * the mesh's full face_count. No rotation / deformation in this API. */
sat_result_t sat_physics3_add_kinematic_mesh(
    sat_physics3_world_t* world, const sat_mesh_t* mesh,
    sat_mesh3_grid_t* optional_grid, const sat_vec3_t* initial_offset,
    const sat_physics3_material_t* material, uint16_t* out_id);
/* Optional rotation of a kinematic quad mesh about its local origin.
 * The input is a unit quaternion (x,y,z,w), where identity is (0,0,0,ONE).
 * Shortest-path normalized-linear interpolation across bounded substeps;
 * one tick may rotate at most about 45 degrees. Vertex data and reference
 * grid remain immutable; collision queries transform the sphere instead.
 * This first rotational slice is DISCRETE, not swept rotational CCD. */
sat_result_t sat_physics3_set_kinematic_mesh_orientation_target(
    sat_physics3_world_t* world, uint16_t mesh_actor_id,
    const sat_physics3_quat_t* target);
/* Move an existing kinematic mesh to target_offset at the NEXT tick. */
sat_result_t sat_physics3_set_kinematic_mesh_target(
    sat_physics3_world_t* world, uint16_t mesh_actor_id,
    const sat_vec3_t* target_offset);

sat_result_t sat_physics3_add_sphere(sat_physics3_world_t* world,
    const sat_sphere_t* sphere,const sat_vec3_t* velocity,
    const sat_physics3_material_t* material,uint16_t* out_id);
sat_result_t sat_physics3_set_kinematic_target(sat_physics3_world_t* world,
    uint16_t id,const sat_vec3_t* center);
/* Solid homogeneous sphere, I=2/5*m*r^2. On upward contacts friction
 * is an impulse that couples linear surface slip and angular velocity.
 * No-slip rolling is approached at friction=ONE; friction=0 preserves spin.
 * This changes contact behavior only for the opted-in sphere. */
sat_result_t sat_physics3_set_rolling(
    sat_physics3_world_t* world, uint16_t sphere_id, int enabled);
/* Angular velocity is in world-space radians per fixed tick.
 * Components must be inside [-8,+8] radians/tick; the simulation
 * saturates generated spin at this fixed-point range. */
sat_result_t sat_physics3_set_angular_velocity(
    sat_physics3_world_t* world, uint16_t sphere_id,
    const sat_vec3_t* angular_velocity);
sat_result_t sat_physics3_set_velocity(sat_physics3_world_t* world,
    uint16_t id,const sat_vec3_t* velocity);
/* Builds an unscaled row-major model matrix (world translation + quaternion
 * orientation) for a dynamic sphere. Pass the result as instance.world with
 * a LOCAL-space sphere mesh to sat_scene_submit_instance. Caller owns matrix.
 * Does not mutate scene hierarchy, physics, or the sphere mesh. */
sat_result_t sat_physics3_sphere_model_matrix(
    const sat_physics3_world_t* world, uint16_t sphere_id, sat_mat4_t* out);
sat_result_t sat_physics3_get_actor(const sat_physics3_world_t* world,
    uint16_t id,sat_physics3_actor_t* out);
/* Preflights velocity/step capacity before changing ANY actor; uses existing
 * sat_sphere_aabb3_contact / sat_sphere_plane_contact for iterative collision.
 * Sphere/sphere contacts are discrete and opt-in (sat_physics3_set_sphere_pairs);
 * there is no swept sphere/sphere CCD. */
/* Enables finite-mesh sweeps (face interior + finite edges + vertices). With no box/plane colliders,
 * allows larger velocities than the discrete substep budget by capping the
 * substeps; with translating mesh targets this uses relative motion,
 * but does not cover rotating meshes or sphere/sphere interactions,
 * or numerical grazing cases below the fixed-point time resolution. */
sat_result_t sat_physics3_set_mesh_face_ccd(
    sat_physics3_world_t* world, int enabled);
/* Continuous relative sweeps against linearly translated kinematic AABBs.
 * Opt-in: performs a six-face finite box cast in relative coordinates, then
 * resolves the hit using the platform's velocity. With only swept colliders
 * present, enables capped substeps instead of rejecting the tick. Static
 * AABB/plane contacts remain discrete and keep the original step budget.
 * Does not support rotating boxes or sphere/sphere CCD;
 * translating finite meshes have their own mesh sweep option. */
sat_result_t sat_physics3_set_kinematic_box_ccd(
    sat_physics3_world_t* world, int enabled);
sat_result_t sat_physics3_world_step(sat_physics3_world_t* world);
#ifdef __cplusplus
}
#endif
#endif
