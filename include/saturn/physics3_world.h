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
    SAT_PHYSICS3_STATIC_MESH=5
} sat_physics3_kind_t;
typedef struct sat_physics3_material {
    sat_fx16_t friction;    /* [0,ONE]; tangent damping on support */
    sat_fx16_t restitution; /* [0,ONE]; normal bounce */
} sat_physics3_material_t;
typedef struct sat_physics3_actor {
    sat_physics3_kind_t kind;
    sat_physics3_material_t material;
    sat_body3_t sphere;
    sat_aabb3_t box;
    sat_plane3_t plane; /* Infinite, two-sided and static. */
    const sat_mesh_t* mesh; /* Borrowed, immutable WORLD-space quad mesh. */
    sat_mesh3_grid_t* mesh_grid; /* Optional caller-owned acceleration grid. */
    sat_vec3_t target_center; /* kinematic target at end of NEXT tick */
    sat_vec3_t frame_motion;  /* displacement per tick, zero for static boxes */
} sat_physics3_actor_t;
typedef struct sat_physics3_world {
    sat_physics3_actor_t* actors;
    sat_contact3_t* mesh_contacts; /* Caller-owned shared query scratch. */
    uint16_t mesh_contact_capacity;
    sat_vec3_t gravity; /* velocity delta per fixed tick */
    uint16_t count,capacity;
    uint8_t max_substeps; /* 1..64 */
    uint8_t iterations; /* 1..8 */
    uint8_t mesh_face_ccd; /* Opt-in conservative FACE-INTERIOR crossing guard. */
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

sat_result_t sat_physics3_add_sphere(sat_physics3_world_t* world,
    const sat_sphere_t* sphere,const sat_vec3_t* velocity,
    const sat_physics3_material_t* material,uint16_t* out_id);
sat_result_t sat_physics3_set_kinematic_target(sat_physics3_world_t* world,
    uint16_t id,const sat_vec3_t* center);
sat_result_t sat_physics3_set_velocity(sat_physics3_world_t* world,
    uint16_t id,const sat_vec3_t* velocity);
sat_result_t sat_physics3_get_actor(const sat_physics3_world_t* world,
    uint16_t id,sat_physics3_actor_t* out);
/* Preflights velocity/step capacity before changing ANY actor; uses existing
 * sat_sphere_aabb3_contact / sat_sphere_plane_contact for iterative collision.
 * No sphere/sphere collision or exact swept CCD in this slice. */
/* Enables finite-mesh sweeps (face interior + finite edges + vertices). With no box/plane colliders,
 * allows larger velocities than the discrete substep budget by capping the
 * substeps; this does NOT protect moving objects, sphere/sphere interactions,
 * or numerical grazing cases below the fixed-point time resolution. */
sat_result_t sat_physics3_set_mesh_face_ccd(
    sat_physics3_world_t* world, int enabled);
sat_result_t sat_physics3_world_step(sat_physics3_world_t* world);
#ifdef __cplusplus
}
#endif
#endif
