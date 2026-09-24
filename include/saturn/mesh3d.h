#ifndef SATURN_MESH3D_H
#define SATURN_MESH3D_H

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/geometry3d.h"

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

/* Meshes are borrowed geometry. Drawing contracts are opt-in through
 * saturn/mesh3d_draw.h, which includes the VDP1 renderer. */
/* Face texture selector: face_texture_indices[face] == this value means the
 * face is drawn as an untextured polygon even when a texture table is bound.
 * Source UVs are baked offline (see tools/import_model.py); the runtime only
 * uses this compact per-face selector when the optional mesh renderer is enabled. */
#define SAT_MESH_TEXTURE_NONE 0xFFFFu
 
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

/* An octahedron has six shared vertices and eight triangular facets.
 * Triangles are encoded as A,B,C,C. Faces are interleaved in upper/lower
 * pairs around the equator: side 0 upper, side 0 lower, ..., side 3 lower.
 * The four equator vertices start at +X and proceed towards +Z.
 * A single local-space octahedron can be shared by any number of instances. */
void sat_mesh_octahedron_counts(uint16_t* out_vertices, uint16_t* out_faces);
void sat_mesh_plane_counts(uint16_t seg_x, uint16_t seg_z, uint16_t* out_vertices, uint16_t* out_faces);
void sat_mesh_sphere_counts(uint16_t segments, uint16_t rings, uint16_t* out_vertices, uint16_t* out_faces);
void sat_mesh_sphere_wedge_counts(
    uint16_t segments,
    uint16_t rings,
    uint16_t gap_segments,
    uint16_t* out_vertices,
    uint16_t* out_faces
);
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

/* Builds a pointed diamond with four equator corners at +/-radius in X/Z,
 * and top/bottom tips at +/-half_height on Y. Both extents must be positive.
 * Geometry is centered on 'center'; for reusable instances pass the origin.
 * On invalid input or insufficient storage the existing mesh is unchanged. */
sat_result_t sat_mesh_build_octahedron(
    sat_mesh_t* mesh,
    const sat_vec3_t* center,
    sat_fx16_t radius,
    sat_fx16_t half_height
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

/* Sphere with an angular sector removed and the opening closed by two flat
 * walls -- a pie chart with a slice taken out, or a Pac-Man.
 *
 * `gap_segments` longitude bands are removed starting at `gap_start_segment`,
 * wrapping past longitude 0 if it has to; zero removes nothing. Longitude 0
 * points along +Z and increases towards +X, so the band boundaries sit at
 * multiples of 360/segments degrees -- choose `segments` so the direction the
 * opening should face lands on a boundary, or the mouth comes out lopsided.
 *
 * This is not the same as drawing something dark over a whole sphere: the
 * silhouette really has the notch, and the cut walls shade as the surfaces
 * they are.
 *
 * FACE ORDER: the curved surface comes first, then the two cut walls, 2 *
 * rings faces of them, last. That is a promise, not an accident -- it is what
 * lets a caller paint the inside of the mouth a different colour through
 * sat_mesh_draw_t::face_colors without inspecting any geometry. */
sat_result_t sat_mesh_build_sphere_wedge(
    sat_mesh_t* mesh,
    const sat_vec3_t* center,
    sat_fx16_t radius,
    uint16_t segments,
    uint16_t rings,
    uint16_t gap_start_segment,
    uint16_t gap_segments
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

/* Unit outward normal, or the zero vector for a degenerate face. Normalising
 * costs a square root and three 64-bit divides on this CPU, so prefer the
 * scaled form below unless the length is genuinely needed. */
sat_result_t sat_mesh_face_normal(const sat_mesh_t* mesh, uint16_t face, sat_vec3_t* out);

/* Outward normal with the right direction and an arbitrary length. This is
 * what culling and sat_face_intensity3_scaled want, and it is what
 * sat_vdp1_draw_mesh uses. */
sat_result_t sat_mesh_face_normal_scaled(const sat_mesh_t* mesh, uint16_t face, sat_vec3_t* out);

/* True when the face is turned towards `eye`. This is the test the native mesh
 * applies for SAT_MESH_CULL_BACKFACE; it is exposed because a program that
 * batches its own geometry still wants the same answer. */
int sat_mesh_face_visible(const sat_mesh_t* mesh, uint16_t face, const sat_vec3_t* eye);

/* Smooth unit normals per vertex: the adjacent face normals summed and
 * normalised, so shading blends across every shared edge -- the input
 * sat_gouraud_lambert wants. `normal_cap` must be at least vertex_count;
 * a vertex no face uses gets the zero vector. */
sat_result_t sat_mesh_vertex_normals(const sat_mesh_t* mesh, sat_vec3_t* out_normals, uint16_t normal_cap);

/* Merges vertices within `epsilon` of each other on every axis, remapping
 * face indices to match and shrinking vertex_count.
 *
 * sat_mesh_build_sphere and sat_mesh_build_sphere_wedge give every ring its
 * own copy of the pole, and sat_mesh_build_sphere_wedge its own copy of the
 * seam -- about a third of a sphere's vertices for a typical segment count.
 * Nothing after building reads across that duplication (each ring's faces
 * only reference their own copies), so it costs nothing to leave alone
 * EXCEPT one thing: every vertex of a mesh drawn through
 * sat_scene_submit_instance is projected once per submission, so a mesh
 * resubmitted every frame pays for the duplicates every frame. Weld once
 * after building, not per frame.
 *
 * `remap_scratch` must hold at least vertex_count entries (a value from
 * before the call, since vertex_count changes); pass vertex_cap to always be
 * safe. Vertex order among the survivors is preserved, so a build step run
 * afterward that assumes a specific layout (mouth-wall faces being the last
 * N of a sat_mesh_build_sphere_wedge, for instance) still works -- only the
 * exact indices shift, and those come from the returned mesh, never from a
 * hard-coded count. */
sat_result_t sat_mesh_weld_vertices(
    sat_mesh_t* mesh, sat_fx16_t epsilon, uint16_t* remap_scratch, uint16_t remap_cap);

/* Translates every vertex. Cheaper than rebuilding when only the position of
 * an already-built primitive changes. */
sat_result_t sat_mesh_translate(sat_mesh_t* mesh, sat_fx16_t dx, sat_fx16_t dy, sat_fx16_t dz);

/* Applies an affine matrix to every vertex (w taken as 1).
 *
 * Every builder here works in one canonical pose -- poles on Y for a sphere,
 * axis on Y for a cylinder -- so a primitive that has to point somewhere else
 * is built at the origin and then placed with this. Beware that a matrix with
 * non-uniform scale changes face normals in ways flat shading will show. */
sat_result_t sat_mesh_transform(sat_mesh_t* mesh, const sat_mat4_t* matrix);


#ifdef __cplusplus
}
#endif

#endif /* SATURN_MESH3D_H */
