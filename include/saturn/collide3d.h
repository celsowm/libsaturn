#ifndef SATURN_COLLIDE3D_H
#define SATURN_COLLIDE3D_H

#include <stdint.h>
#include "saturn/math3d.h"
#include "saturn/mesh3d.h"

#ifdef __cplusplus
extern "C" {
#endif

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
sat_result_t sat_body3_collide_mesh(sat_body3_t*, const sat_mesh_t*);
int sat_body3_separate(sat_body3_t*, sat_body3_t*);

#ifdef __cplusplus
}
#endif
#endif /* SATURN_COLLIDE3D_H */
