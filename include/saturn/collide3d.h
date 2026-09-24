#ifndef SATURN_COLLIDE3D_H
#define SATURN_COLLIDE3D_H

#include <stdint.h>
#include "saturn/mesh3d.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sat_spatial3;

/* Allocation-free 3D collision helpers. Y is up, matching math3d.h. All
 * squared distances use int64 raw fixed-point products; square roots happen
 * only when a contact or hit point needs a length. */
typedef struct sat_aabb3 { sat_vec3_t center, half; } sat_aabb3_t;
typedef struct sat_sphere { sat_vec3_t center; sat_fx16_t radius; } sat_sphere_t;
typedef struct sat_ray3 { sat_vec3_t origin, dir; sat_fx16_t length; } sat_ray3_t;
typedef struct sat_contact3 { sat_vec3_t normal; sat_fx16_t depth; } sat_contact3_t;
typedef struct sat_hit3 {
    sat_fx16_t t;
    sat_vec3_t point, normal;
    uint16_t face;
} sat_hit3_t;
typedef struct sat_plane3 { sat_vec3_t point, normal; } sat_plane3_t;

/* First collision against a finite quad FACE INTERIOR during a sphere sweep.
 * t is in [0, 1] relative to displacement; center is the sphere center at
 * impact, point is the corresponding point on the face, normal points toward
 * the approaching sphere. This is not a full edge/corner capsule cast. */
typedef struct sat_sphere_mesh_face_hit {
    sat_fx16_t t;
    sat_vec3_t center, point, normal;
    uint16_t face;
} sat_sphere_mesh_face_hit_t;

/* On success, *found is 1 if a face-interior crossing exists, else 0.
 * Initial overlap and edge/corner-only impacts are delegated to discrete
 * contact tests; no side effects on mesh/sphere/displacement. */
sat_result_t sat_sphere_cast_mesh_faces(
    const sat_mesh_t* mesh, const sat_sphere_t* sphere,
    const sat_vec3_t* displacement, sat_sphere_mesh_face_hit_t* out,
    uint8_t* found);

/* Earliest sphere impact against a finite convex quad mesh, including
 * interiors, line-segment edges, and vertices. t is 16.16 fraction of the
 * entire displacement; feature = 0 face, 1 edge, 2 vertex. Mesh is static
 * and world-space. Initial overlaps belong to discrete contact resolution.
 * Uses bounded integer convex-distance minimization, not a floating-point
 * quadratic solver; sub-1/65536-tick grazing contacts may round away. */
typedef struct sat_sphere_mesh_hit {
    sat_fx16_t t;
    sat_vec3_t center, point, normal;
    uint16_t face;
    uint8_t feature;
} sat_sphere_mesh_hit_t;
sat_result_t sat_sphere_cast_mesh(
    const sat_mesh_t* mesh, const sat_sphere_t* sphere,
    const sat_vec3_t* displacement, sat_sphere_mesh_hit_t* out,
    uint8_t* found);

int sat_sphere_overlap(const sat_sphere_t*, const sat_sphere_t*);
int sat_sphere_sphere_overlap(const sat_sphere_t*, const sat_sphere_t*);
int sat_aabb3_overlap(const sat_aabb3_t*, const sat_aabb3_t*);
int sat_sphere_aabb3_overlap(const sat_sphere_t*, const sat_aabb3_t*);
int sat_sphere_plane_overlap(const sat_sphere_t*, const sat_plane3_t*);
int sat_sphere_contact(const sat_sphere_t*, const sat_sphere_t*, sat_contact3_t*);
int sat_sphere_sphere_contact(const sat_sphere_t*, const sat_sphere_t*, sat_contact3_t*);
int sat_aabb3_contact(const sat_aabb3_t*, const sat_aabb3_t*, sat_contact3_t*);
int sat_sphere_aabb3_contact(const sat_sphere_t*, const sat_aabb3_t*, sat_contact3_t*);
int sat_sphere_plane_contact(const sat_sphere_t*, const sat_plane3_t*, sat_contact3_t*);

int sat_raycast_sphere(const sat_sphere_t*, const sat_ray3_t*, sat_hit3_t*);
int sat_raycast_aabb3(const sat_aabb3_t*, const sat_ray3_t*, sat_hit3_t*);
int sat_raycast_quad3(const sat_quad3_t*, const sat_ray3_t*, sat_hit3_t*);
int sat_raycast_mesh(const sat_mesh_t*, const sat_ray3_t*, sat_hit3_t*);
sat_result_t sat_sphere_mesh_contact(const sat_mesh_t*, const sat_sphere_t*,
    sat_contact3_t* out, uint16_t cap, uint16_t* count);

/* Caller-owned hashed 3D grid for static mesh collision. Build it once after
 * the mesh geometry is final. Faces are inserted into every cell touched by
 * their AABB; queries de-duplicate faces with the caller-owned stamp array.
 * bucket_count must be a power of two. entry_cap is the total face/cell
 * membership capacity, not merely face_count. */
#define SAT_MESH3_GRID_EMPTY ((uint16_t)0xFFFFu)
typedef struct sat_mesh3_grid_entry {
    int32_t cell_x;
    int32_t cell_y;
    int32_t cell_z;
    uint16_t face;
    uint16_t next;
} sat_mesh3_grid_entry_t;

typedef struct sat_mesh3_grid {
    const sat_mesh_t* mesh;
    uint16_t* heads;
    sat_mesh3_grid_entry_t* entries;
    uint16_t* stamps;
    uint16_t bucket_count;
    uint16_t entry_cap;
    uint16_t entry_count;
    uint16_t stamp_cap;
    uint16_t query_stamp;
    uint8_t cell_shift;
} sat_mesh3_grid_t;

sat_result_t sat_mesh3_grid_init(
    sat_mesh3_grid_t* grid,
    const sat_mesh_t* mesh,
    uint8_t cell_shift,
    uint16_t* heads,
    uint16_t bucket_count,
    sat_mesh3_grid_entry_t* entries,
    uint16_t entry_cap,
    uint16_t* stamps,
    uint16_t stamp_cap);
sat_result_t sat_sphere_mesh_contact_grid(
    sat_mesh3_grid_t* grid,
    const sat_sphere_t* sphere,
    sat_contact3_t* out,
    uint16_t cap,
    uint16_t* count);

enum {
    SAT_BODY3_GROUNDED = 1u << 0,
    SAT_BODY3_HIT_WALL = 1u << 1
};
typedef struct sat_body3 { sat_sphere_t shape; sat_vec3_t vel; uint16_t flags; } sat_body3_t;
typedef struct sat_body3_params {
    sat_vec3_t gravity;
    sat_fx16_t max_fall, drag, restitution, floor_friction;
} sat_body3_params_t;
void sat_body3_step(sat_body3_t*, const sat_body3_params_t*);
sat_result_t sat_body3_collide_aabbs(sat_body3_t*, const sat_aabb3_t*, uint16_t count);
/* Accelerated AABB path. The spatial grid contains the same boxes and narrows
 * each solver iteration to local candidates; candidate order is unspecified. */
sat_result_t sat_body3_collide_spatial_aabbs(sat_body3_t*, struct sat_spatial3*);
sat_result_t sat_body3_collide_mesh(sat_body3_t*, const sat_mesh_t*);
/* Accelerated static-mesh path. Complexity depends on locally occupied grid
 * cells/candidates rather than scanning every mesh face per substep. */
sat_result_t sat_body3_collide_mesh_grid(sat_body3_t*, sat_mesh3_grid_t*);
int sat_body3_separate(sat_body3_t*, sat_body3_t*);

#ifdef __cplusplus
}
#endif
#endif /* SATURN_COLLIDE3D_H */
