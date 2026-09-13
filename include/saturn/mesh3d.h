#ifndef SATURN_MESH3D_H
#define SATURN_MESH3D_H

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/math3d.h"
#include "saturn/render3d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Quad meshes and primitive solids                                     */
/* ------------------------------------------------------------------ */
/* saturn/render3d.h draws one world quad at a time. That is the right level
 * for a wall or a floor tile, but building a cube or a sphere out of it means
 * every program re-deriving the same corner order -- and getting the winding
 * wrong, which on hardware with no depth buffer shows up as faces that are
 * culled when they should be drawn.
 *
 * A sat_mesh_t is a list of quads sharing a vertex array, plus the one
 * drawing call that turns it into VDP1 commands: cull the faces pointing
 * away, sort the rest back-to-front, shade them, submit them.
 *
 * STORAGE IS CALLER-OWNED. The library never allocates, so a mesh points at
 * arrays the caller supplies, and the sat_mesh_*_counts helpers say exactly
 * how large those arrays must be for a given primitive. This keeps meshes
 * usable from static storage, which is how examples on a console with no
 * heap want to work.
 *
 * WINDING: face corners run A, B, C, D the same way a VDP1 quad does --
 * A top-left, B top-right, C bottom-right, D bottom-left as seen from the
 * front. The outward normal is therefore cross(D - A, B - A), and every
 * primitive here is built so that normal points away from the solid.
 */

typedef struct sat_mesh {
    sat_vec3_t* vertices;
    uint16_t* indices; /* 4 vertex indices per face */
    uint16_t vertex_cap;
    uint16_t vertex_count;
    uint16_t face_cap;
    uint16_t face_count;
} sat_mesh_t;

/* Binds a mesh to caller storage and empties it. `indices` must hold
 * 4 * face_cap entries. */
sat_result_t sat_mesh_init(
    sat_mesh_t* mesh,
    sat_vec3_t* vertices,
    uint16_t vertex_cap,
    uint16_t* indices,
    uint16_t face_cap
);

/* Drops all geometry, keeping the storage binding. Rebuilding a scratch mesh
 * every frame is cheap and is how a program draws many differently-sized
 * boxes from one buffer. */
void sat_mesh_clear(sat_mesh_t* mesh);

sat_result_t sat_mesh_add_vertex(sat_mesh_t* mesh, sat_fx16_t x, sat_fx16_t y, sat_fx16_t z, uint16_t* out_index);
sat_result_t sat_mesh_add_face(sat_mesh_t* mesh, uint16_t a, uint16_t b, uint16_t c, uint16_t d);

/* Appends a standalone quad, adding four unshared vertices. Convenient for
 * mixing loose geometry into a mesh; prefer add_vertex/add_face when corners
 * repeat. */
sat_result_t sat_mesh_add_quad(sat_mesh_t* mesh, const sat_quad3_t* quad);

/* ------------------------------------------------------------------ */
/* Required buffer sizes                                                */
/* ------------------------------------------------------------------ */
/* Call these to size static arrays. They mirror the builders exactly, so a
 * build into buffers sized this way never returns SAT_ERR_CAPACITY. */

void sat_mesh_box_counts(uint16_t* out_vertices, uint16_t* out_faces);
void sat_mesh_plane_counts(uint16_t seg_x, uint16_t seg_z, uint16_t* out_vertices, uint16_t* out_faces);
void sat_mesh_sphere_counts(uint16_t segments, uint16_t rings, uint16_t* out_vertices, uint16_t* out_faces);
void sat_mesh_cylinder_counts(uint16_t segments, int capped, uint16_t* out_vertices, uint16_t* out_faces);

/* ------------------------------------------------------------------ */
/* Primitive builders                                                   */
/* ------------------------------------------------------------------ */
/* Each builder clears the mesh first, so a scratch mesh can be rebuilt in a
 * loop. All return SAT_ERR_CAPACITY, and leave the mesh empty, if the bound
 * storage is too small. */

/* Axis-aligned box centred on `center`, extending +/- half_x/y/z. */
sat_result_t sat_mesh_build_box(
    sat_mesh_t* mesh,
    const sat_vec3_t* center,
    sat_fx16_t half_x,
    sat_fx16_t half_y,
    sat_fx16_t half_z
);

/* Box with equal half extents. */
sat_result_t sat_mesh_build_cube(sat_mesh_t* mesh, const sat_vec3_t* center, sat_fx16_t half);

/* Horizontal plane at center.y, spanning +/- half_x and +/- half_z, divided
 * into seg_x by seg_z quads. Its normal points up (+Y); subdivision exists
 * because a single huge quad lit by one flat colour reads as a void, and
 * because projection error grows with quad size. */
sat_result_t sat_mesh_build_plane(
    sat_mesh_t* mesh,
    const sat_vec3_t* center,
    sat_fx16_t half_x,
    sat_fx16_t half_z,
    uint16_t seg_x,
    uint16_t seg_z
);

/* UV sphere: `segments` divisions around Y (minimum 3), `rings` bands from
 * pole to pole (minimum 2). The top and bottom bands have two coincident
 * corners each, so they submit as triangles -- which is what the VDP1 draws
 * for a quad with a repeated corner anyway. */
sat_result_t sat_mesh_build_sphere(
    sat_mesh_t* mesh,
    const sat_vec3_t* center,
    sat_fx16_t radius,
    uint16_t segments,
    uint16_t rings
);

/* Cylinder aligned to Y, `half_height` above and below center. `capped`
 * adds flat end discs as triangle fans (again, degenerate quads). */
sat_result_t sat_mesh_build_cylinder(
    sat_mesh_t* mesh,
    const sat_vec3_t* center,
    sat_fx16_t radius,
    sat_fx16_t half_height,
    uint16_t segments,
    int capped
);

/* ------------------------------------------------------------------ */
/* Face queries                                                         */
/* ------------------------------------------------------------------ */

sat_result_t sat_mesh_face_quad(const sat_mesh_t* mesh, uint16_t face, sat_quad3_t* out);
sat_result_t sat_mesh_face_center(const sat_mesh_t* mesh, uint16_t face, sat_vec3_t* out);

/* Unit outward normal, or the zero vector for a degenerate face. */
sat_result_t sat_mesh_face_normal(const sat_mesh_t* mesh, uint16_t face, sat_vec3_t* out);

/* True when the face is turned towards `eye`. This is the test sat_draw_mesh
 * applies for SAT_MESH_CULL_BACKFACE; it is exposed because a program that
 * batches its own geometry still wants the same answer. */
int sat_mesh_face_visible(const sat_mesh_t* mesh, uint16_t face, const sat_vec3_t* eye);

/* Translates every vertex. Cheaper than rebuilding when only the position of
 * an already-built primitive changes. */
sat_result_t sat_mesh_translate(sat_mesh_t* mesh, sat_fx16_t dx, sat_fx16_t dy, sat_fx16_t dz);

/* ------------------------------------------------------------------ */
/* Drawing                                                              */
/* ------------------------------------------------------------------ */

#define SAT_MESH_CULL_BACKFACE 0x0001u /* drop faces turned away from the eye */
#define SAT_MESH_SORT 0x0002u          /* submit farthest-first (painter's)   */
#define SAT_MESH_SHADE 0x0004u         /* modulate colour by face normal      */

typedef struct sat_mesh_draw {
    const sat_mat4_t* view_proj;
    sat_vec3_t eye; /* camera position in world space: culling and sorting */
    uint16_t color;
    sat_fx16_t ambient; /* 16.16 floor for SAT_MESH_SHADE; 0 = full black */
    uint16_t flags;
    /* Scratch for SAT_MESH_SORT, each at least face_count entries. May be
     * null when SAT_MESH_SORT is not set. Faces are indexed by uint8_t, so
     * sorting is limited to 255 faces; larger meshes must be split, which is
     * usually what you want on a 512-command display list anyway. */
    uint8_t* order;
    uint32_t* depth;
} sat_mesh_draw_t;

/* Submits the mesh. Returns SAT_OK when every surviving face was drawn,
 * SAT_ERR_CAPACITY if the VDP1 command list filled up part-way, and
 * SAT_ERR_INVALID_ARG for a malformed request. Faces the camera cannot see --
 * culled, or straddling the near plane -- are skipped without being errors,
 * so a partly off-screen mesh still returns SAT_OK. */
sat_result_t sat_draw_mesh(const sat_mesh_t* mesh, const sat_mesh_draw_t* params);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_MESH3D_H */
