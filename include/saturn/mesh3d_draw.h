#ifndef SATURN_MESH3D_DRAW_H
#define SATURN_MESH3D_DRAW_H

/* Optional VDP1-specific mesh submission. Geometry and collision clients
 * include saturn/mesh3d.h without importing render3d.h or vdp1.h. */
#include "saturn/mesh3d.h"
#include "saturn/render3d.h"

#ifdef __cplusplus
extern "C" {
#endif

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
    /* Optional per-face colours, face_count entries, overriding `color`.
     * Null uses `color` throughout. This is how a solid gets more than one
     * material without being split into several meshes -- the inside of a
     * sat_mesh_build_sphere_wedge mouth, for instance, which is the same
     * yellow as the outside until something says otherwise. */
    const uint16_t* face_colors;
    /* Optional per-face textures, face_count entries. Null selects the legacy
     * polygon path for every face. A face set to SAT_MESH_TEXTURE_NONE is
     * drawn as a polygon; any other value indexes `textures` and is drawn as
     * a VDP1 distorted sprite. An out-of-range index returns
     * SAT_ERR_INVALID_ARG without reading past the arrays.
     *
     * SAT_MESH_SHADE applies only to polygon faces. Textured faces are drawn
     * with the texture's own colours because the VDP1 distorted-sprite path
     * has no per-face RGB modulation matching the polygon path. */
    const sat_vdp1_texture_t* textures;
    uint16_t texture_count;
    const uint16_t* face_texture_indices;
    sat_fx16_t ambient; /* 16.16 floor for SAT_MESH_SHADE; 0 = full black */
    uint16_t flags;
    /* Scratch for SAT_MESH_SORT, each at least face_count entries. May be
     * null when SAT_MESH_SORT is not set. Meshes of up to 255 faces sort
     * through the legacy `order` (uint8_t) table; larger meshes -- an
     * animated character near the VDP1 command budget, for example -- sort
     * through `order16` instead, which the native mesh path selects automatically
     * by face_count. `depth` serves both paths. Splitting a character just
     * to preserve the old uint8_t limit is not required. */
    uint8_t* order;
    uint32_t* depth;
    uint16_t* order16;
    /* Optional projection cache, vertex_count entries. With it, the draw
     * projects every vertex once, then works in screen space: back faces
     * are culled by the signed area of their projected corners (exact under
     * perspective, no 64-bit face normals), and SAT_MESH_SORT orders by
     * view depth -- the corners' clip w -- instead of distance from `eye`.
     * Faces with a corner at or behind the camera plane are skipped, as on
     * the per-face path. Null keeps the per-face world-space path. Pure
     * scratch: nothing in it survives from one draw to the next.
     * Zero-initialise the struct so this reads as absent when unused. */
    sat_projected_vertex_t* screen;
    /* Optional Gouraud shading: one table entry per vertex (format in
     * saturn/vdp1.h). Each untextured face is drawn Gouraud-shaded with the
     * entries of its four corner vertices, on top of its flat color --
     * sat_gouraud_lambert over sat_mesh_vertex_normals builds this for any
     * mesh. Textured faces ignore it: their color-bank texels are not
     * RGB-coded, which the VDP1 requires. Null draws flat faces. */
    const uint16_t* vertex_gouraud;
} sat_mesh_draw_t;

/* Submits the mesh. Returns SAT_OK when every surviving face was drawn,
 * SAT_ERR_CAPACITY if the VDP1 command list filled up part-way, and
 * SAT_ERR_INVALID_ARG for a malformed request. Faces the camera cannot see --
 * culled, or straddling the near plane -- are skipped without being errors,
 * so a partly off-screen mesh still returns SAT_OK. */
/* Explicit low-level VDP1 mesh path for focused renderer tests/probes. Normal
 * games submit through sat_scene_t; this function is not a scene owner. */
sat_result_t sat_vdp1_draw_mesh(const sat_mesh_t* mesh, const sat_mesh_draw_t* params);

/* Local-space flat indexed model instance. Each face references one uniform
 * INDEX8 texture through face_materials, and the base mesh is never mutated.
 * The library validates the full face/material table, sorts by camera depth,
 * translates each face and uses the same near/screen-safe indexed quad path.
 * order/depth are caller-owned arrays of mesh->face_count entries. This
 * painter is not an exact Z-buffer for intersecting scene geometry. */
typedef struct sat_indexed_solid_mesh3d_draw {
    sat_indexed_solid_render3d_t render;
    sat_vec3_t position;
    const sat_vdp1_texture_t* textures;
    uint16_t texture_count;
    const uint16_t* face_materials;
    uint16_t* order;
    uint32_t* depth;
} sat_indexed_solid_mesh3d_draw_t;

sat_result_t sat_draw_indexed_solid_mesh3(
    const sat_mesh_t* mesh, const sat_indexed_solid_mesh3d_draw_t* params
);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_MESH3D_DRAW_H */
