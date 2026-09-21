#ifndef P3D_ACTOR_RENDER_H
#define P3D_ACTOR_RENDER_H

/* What Pac-Man's and the ghosts' models share: submitting a mesh at a board
 * position, the key light they are shaded by, and the mesh fix-ups. */

#include <stdint.h>

#include "saturn/mesh3d.h"
#include "saturn/scene.h"

#include "camera.h"

/* Scratch sizes, for the largest actor mesh, Pac-Man's sphere.
 * sat_mesh_sphere_counts gives the exact numbers at runtime; these have to
 * be compile-time constants, so sat_mesh_build_* is left to report
 * SAT_ERR_CAPACITY if they are ever made too small. */
#define P3D_ACTOR_VERTEX_CAP 96u
#define P3D_ACTOR_FACE_CAP 96u

/* An opaque RGB polygon material, Gouraud-shaded by `gouraud` (one word per
 * mesh vertex) when that is not NULL. */
void p3d_actor_material(sat_scene3d_material_t* material, uint16_t color,
                        const uint16_t* gouraud);

/* Submits `mesh` standing at (x, y, z), turned `quarter` quarter-turns from
 * +Z towards +X. */
void p3d_actor_submit(sat_scene_t* scene, const sat_mesh_t* mesh,
                      const sat_scene3d_material_t* materials, uint16_t material_count,
                      const uint16_t* face_materials, int x, int y, int z, int quarter);

/* Quarter turns from +Z to the way `dir` faces. The maze's +Z is south. */
int p3d_facing_quarter(int dir);

/* The actors' key light for this camera, in world space, unit length. */
void p3d_actor_key_light(const p3d_camera_t* camera, sat_vec3_t* out);

/* One Gouraud word per vertex under `light`. */
void p3d_actor_gouraud(const sat_vec3_t* normals, uint16_t count,
                       const sat_vec3_t* light, uint16_t* out);

/* Merges vertices that sit in the same place (see actor_render.c). */
void p3d_mesh_weld(sat_mesh_t* mesh);

#endif /* P3D_ACTOR_RENDER_H */
