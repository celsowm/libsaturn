#ifndef SATURN_RENDER3D_H
#define SATURN_RENDER3D_H

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/math3d.h"
#include "saturn/vdp1.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* World-space quad rendering                                          */
/* ------------------------------------------------------------------ */
/* The VDP1 has no depth buffer and no clipper: it draws quads, in list order,
 * from four screen-space corners. So a 3D scene on this hardware is built by
 * projecting world quads to screen quads yourself, discarding the ones the
 * camera cannot see, and submitting the rest back-to-front.
 *
 * These helpers cover that whole path, so an application supplies geometry and
 * a view-projection matrix and never touches native VDP1 coordinates.
 *
 * Corner order matches the VDP1 command: A(top-left), B(top-right),
 * C(bottom-right), D(bottom-left). The texture, if any, maps onto it.
 */

typedef struct sat_quad3 {
    sat_vec3_t v[4];
} sat_quad3_t;

/* Projected corners in NATIVE VDP1 coordinates (0,0 = screen centre). */
typedef struct sat_quad2 {
    int16_t x[4];
    int16_t y[4];
} sat_quad2_t;

/* Software near-plane clipping for a convex WORLD quad (including triangles
 * encoded with D=C). eye and forward are world-space 16.16; forward must be
 * a unit camera direction, near_depth > 0. The output array must hold FOUR
 * quads. A fully visible quad is returned unchanged; a crossing quad is
 * clipped and tessellated into <=4 triangles (D=C), which can then be
 * projected through the normal native VDP1 paths without a giant/folded
 * sprite. Fully hidden quad returns count=0.
 *
 * This clips GEOMETRY only, not texture UV coordinates. Use it for solid
 * materials; textures need UV clipping or an explicit skip near the camera.
 * The caller supplies all memory and retains the original input unmodified.
 */
sat_result_t sat_clip_quad_near(
    const sat_quad3_t* quad,
    const sat_vec3_t* eye,
    const sat_vec3_t* forward,
    sat_fx16_t near_depth,
    sat_quad3_t out_triangles[4],
    uint8_t* out_count);

/* Clip a projected solid-color quad against native screen bounds; result
 * is a list of <=6 triangles (D=C) suitable for small uniform indexed
 * VDP1 sprites. This prevents a near-plane-clipped world face with
 * projected +/-2047 corners from issuing huge hardware raster commands.
 * width/height are viewport dimensions, e.g. 320x224. The clipping
 * retains shape/winding but does NOT interpolate texture UVs: never use
 * its triangles with a patterned/photographic sprite.
 * The full-visible fast path preserves all original quad corners. */
sat_result_t sat_clip_quad_screen(
    const sat_quad2_t* quad,
    uint16_t width,uint16_t height,
    sat_quad2_t out_triangles[6],
    uint8_t* out_count);

/* One vertex projected into native VDP1 coordinates. `w` is its clip-space
 * w -- view depth in 16.16 world units. w <= 0 means at or behind the camera
 * plane, and x/y are then meaningless. */
/* Render a uniform-color INDEX8 material with geometrically correct near
 * and screen clipping. This is intentionally NOT patterned-texture UV
 * clipping: all texels in the supplied indexed8 sprite must have one color.
 * The fade selector uses the eight GLOBAL VDP2 sprite color-calc slots; 255
 * selects the ordinary opaque priority. The caller configures that table. */
#define SAT_INDEXED_SOLID_OPAQUE 255u
typedef struct sat_indexed_solid_render3d {
    const sat_mat4_t* view_proj;
    sat_vec3_t eye;
    sat_vec3_t forward; /* normalized camera direction */
    sat_fx16_t near_depth; /* > 0; guard against giant VDP1 sprites */
    uint16_t width, height;
    uint8_t color_calc_slot; /* 0..7 or SAT_INDEXED_SOLID_OPAQUE */
} sat_indexed_solid_render3d_t;

sat_result_t sat_draw_indexed_solid_quad3(
    const sat_quad3_t* quad,
    const sat_indexed_solid_render3d_t* params,
    const sat_vdp1_texture_t* uniform_indexed8_texture
);

/* Axis-aligned deck/block instance, described without app-side quad winding.
 * top_center.y is the WALKABLE TOP (the solid extends by 2*half_height
 * below it); half_extents is positive for all three axes.
 * The runtime derives the camera-facing X and Z walls and visible top;
 * the caller supplies three indexed solid material textures. The top is not
 * drawn when the camera is below it: this deliberately does not add a
 * phantom lid when the camera passes underneath a platform.
 *
 * All coordinates and material descriptors are validated before drawing.
 * No mesh vertices, indices, material arrays or scratch allocations are
 * constructed by the game. Uses the SAME safe near/screen solid clipper.
 * For intersecting boxes this is still painter rendering, not Z testing. */
typedef struct sat_indexed_box3 {
    sat_vec3_t top_center;
    sat_vec3_t half_extents;
    const sat_vdp1_texture_t* top_material;
    const sat_vdp1_texture_t* x_material;
    const sat_vdp1_texture_t* z_material;
} sat_indexed_box3_t;

/* Exposes EXACTLY the same camera-facing faces used by the immediate box
 * renderer. out_count is 2 below the deck or 3 above it. This geometry
 * extraction allows a scene-wide painter to sort a box's faces together with
 * actors and gems without reimplementing box winding in the game. */
sat_result_t sat_indexed_box3_faces(
    const sat_indexed_box3_t* box, const sat_vec3_t* eye,
    sat_quad3_t out_faces[3],
    const sat_vdp1_texture_t* out_materials[3],
    uint8_t* out_count);

sat_result_t sat_draw_indexed_box3(
    const sat_indexed_box3_t* box,
    const sat_indexed_solid_render3d_t* params
);

/* VDP1 distorted sprites map the ENTIRE texture to all four quad corners.
 * Arbitrary near/screen clipping cannot retain patterned UVs without an
 * explicit texture-region/materialization pipeline. In the absence of that
 * capability, this helper conservatively draws exactly one unmodified
 * textured quad only when all four corners are in front of the near plane
 * and within screen bounds. Unsafe/fully invisible quads draw nothing,
 * leaving any caller-rendered clipped solid backing face visible.
 *
 * Color-calc follows the same global VDP2 slot rules as indexed solids.
 * This API reports whether it emitted a sprite through optional out_drawn;
 * returning SAT_OK with out_drawn=0 is a valid visibility outcome, not an
 * error. Width/height must describe the actual display viewport. */
sat_result_t sat_draw_indexed_textured_quad3(
    const sat_quad3_t* quad,
    const sat_indexed_solid_render3d_t* params,
    const sat_vdp1_texture_t* texture,
    uint8_t* out_drawn
);

/* PREBAKED 2x2 patterned texture regions: preserve the VDP1's fixed
 * full-texture-to-quad mapping by pairing four subdivision quads with four
 * independently uploaded source regions. No runtime allocation or VRAM writes.
 *
 * The full INDEX8 texture must have an even height and width divisible by 16;
 * each tile must have exactly half those dimensions and the same palette.
 * The four regions must contain the source's true top-left, top-right,
 * bottom-left, bottom-right pixels. Their correspondence is a caller asset
 * invariant: VDP1 cannot verify which source pixels were uploaded.
 * For a fully safe projected quad, draw the original full texture ONCE.
 * Otherwise, draw only subquads that are fully safe in camera and screen
 * space, at most four VDP1 commands; never stretch an unclipped full texture
 * onto a clipped polygon. Skipped tile areas expose the solid backing face.
 * Bilinear world subdivision is exact for an affine/parallelogram surface;
 * arbitrary perspective/UV interpolation and pixel-perfect seams are not
 * promised for non-affine quads. This is a bounded tile fallback, NOT general
 * textured polygon clipping or perspective-correct UV mapping.
 *
 * Prepare/upload regions during asset initialization, not per frame.
 * out_submitted (optional) is 0..4. On a hardware submission failure, prior
 * successful tiles may already have been sent; out_submitted counts only
 * successful commands. The caller should reserve at most four commands. */
typedef struct sat_indexed_tiled_quad3 {
    const sat_vdp1_texture_t* full;
    const sat_vdp1_texture_t* tiles[4]; /* TL, TR, BL, BR */
} sat_indexed_tiled_quad3_t;

/* Offline/loading-time region preparation for the tiled renderer, no heap.
 * Packs four contiguous quadrant pixel buffers and uploads 4 native INDEX8
 * textures once. 'pixels' has source_pitch bytes per row; width must be
 * divisible by 16, height even (<=254), and each quadrant fits VDP1's
 * 8-pixel width alignment. 'scratch' requires (width/2)*(height/2) bytes,
 * reused for each tile. 'tiles' holds four returned descriptors ordered
 * TL,TR,BL,BR; source pixels/pitch/format and palette must match the already
 * uploaded full texture used with sat_draw_indexed_tiled_quad3.
 * Input/capacity failures do not upload anything; a hardware failure after
 * some uploads may leave partially allocated VRAM (startup should abort).
 * No runtime crop, no SRC-address trick, and no hidden VRAM allocation on
 * each draw. */
sat_result_t sat_upload_indexed8_quadrants(
    const uint8_t* pixels,
    uint16_t width, uint16_t height, uint16_t source_pitch,
    uint16_t palette_bank,
    sat_vdp1_texture_t tiles[4],
    uint8_t* scratch, uint32_t scratch_capacity
);

sat_result_t sat_draw_indexed_tiled_quad3(
    const sat_quad3_t* quad,
    const sat_indexed_solid_render3d_t* params,
    const sat_indexed_tiled_quad3_t* regions,
    uint8_t* out_submitted
);

typedef struct sat_projected_vertex {
    int16_t x;
    int16_t y;
    sat_fx16_t w;
} sat_projected_vertex_t;

/* ------------------------------------------------------------------ */
/* Quad construction                                                   */
/* ------------------------------------------------------------------ */

/* Upright wall panel rising from y = 0 to y = height along the ground segment
 * (x0,z0) -> (x1,z1). Corners come out A/B on top, C/D on the floor, so the
 * quad faces whichever side the segment runs left-to-right from. */
void sat_quad3_wall(
    sat_quad3_t* out,
    sat_fx16_t x0,
    sat_fx16_t z0,
    sat_fx16_t x1,
    sat_fx16_t z1,
    sat_fx16_t height
);

/* Flat, axis-aligned patch on the horizontal plane y, spanning +/- half in
 * both x and z around (cx, cz). Useful for floors, pellets and shadows. */
void sat_quad3_floor(
    sat_quad3_t* out,
    sat_fx16_t cx,
    sat_fx16_t y,
    sat_fx16_t cz,
    sat_fx16_t half
);

/* Camera-facing panel standing on the ground at (cx, cz), `height` tall and
 * 2 * half_w wide, spread along the camera's right vector (right_x, right_z).
 * Pass the right vector the view matrix was built with so the panel stays
 * square-on as the camera turns. */
void sat_quad3_billboard(
    sat_quad3_t* out,
    sat_fx16_t cx,
    sat_fx16_t cz,
    sat_fx16_t right_x,
    sat_fx16_t right_z,
    sat_fx16_t half_w,
    sat_fx16_t height
);

/* ------------------------------------------------------------------ */
/* Projection and drawing                                              */
/* ------------------------------------------------------------------ */

/* Projects all four corners through view_proj into native VDP1 coordinates.
 * Returns SAT_ERR_UNSUPPORTED when any corner is at or behind the camera
 * plane -- the VDP1 cannot clip, so such a quad must be dropped whole.
 * Corners that project far off-screen are clamped to the coordinate range the
 * VDP1 command fields can hold. */
sat_result_t sat_project_quad(
    const sat_mat4_t* view_proj,
    const sat_quad3_t* quad,
    sat_quad2_t* out
);

/* Projects and submits a flat-shaded polygon. Returns SAT_ERR_UNSUPPORTED
 * (and draws nothing) when the quad is not fully in front of the camera. */
sat_result_t sat_draw_world_polygon(
    const sat_mat4_t* view_proj,
    const sat_quad3_t* quad,
    uint16_t color
);

/* Select explicit native VDP1 RGB polygon effects without duplicating
 * projection. Flags: SAT_SPRITE_FLAG_MESH, HALF_TRANSPARENT and
 * HALF_LUMINANCE (declared in vdp1.h). Both half modes require RGB-coded
 * color (bit 15 set). Callers submit translucent quads back-to-front:
 * VDP1 has no depth buffer and only blends against its own RGB framebuffer,
 * not against NBG0/RBG0. */
sat_result_t sat_draw_world_polygon_effects(
    const sat_mat4_t* view_proj,
    const sat_quad3_t* quad,
    uint16_t color,
    uint16_t flags
);

/* Submits an ALREADY projected quad -- the output of sat_project_quad -- as a
 * flat-shaded polygon.
 *
 * This exists for scenes whose camera does not move. Projection is the
 * expensive half of drawing on this hardware (four matrix transforms and two
 * 64-bit divides per corner), and a fixed camera projects the same static
 * geometry to the same screen coordinates every frame. Projecting it once at
 * startup and replaying the corners turns a per-frame cost into a startup
 * cost; see examples/pacman_3d, where it is the difference between the board
 * running at full rate and at a fifth of it.
 *
 * A camera that moves between a SMALL FIXED SET of positions still qualifies:
 * bake one list per position and select between them. pacman_3d does exactly
 * that for its sixteen view angles, at a cost of about 10KB per angle. */
sat_result_t sat_draw_quad2_polygon(const sat_quad2_t* quad, uint16_t color);
sat_result_t sat_draw_quad2_polygon_effects(
    const sat_quad2_t* quad, uint16_t color, uint16_t flags);

/* Textured counterpart of sat_draw_quad2_polygon: submits an already
 * projected quad as a distorted sprite. */
sat_result_t sat_draw_quad2_sprite(
    const sat_quad2_t* quad,
    const sat_vdp1_texture_t* texture,
    uint16_t palette_override,
    uint16_t flags
);

/* Gouraud-shaded counterparts of sat_draw_quad2_polygon and
 * sat_draw_world_polygon (table format in saturn/vdp1.h): gouraud[0..3]
 * correct corners A..D, and `color` must be RGB-coded. */
sat_result_t sat_draw_quad2_polygon_gouraud(
    const sat_quad2_t* quad,
    uint16_t color,
    const uint16_t gouraud[4]
);
sat_result_t sat_draw_quad2_polygon_gouraud_effects(
    const sat_quad2_t* quad, uint16_t color,
    const uint16_t gouraud[4], uint16_t flags);
sat_result_t sat_draw_world_polygon_gouraud(
    const sat_mat4_t* view_proj,
    const sat_quad3_t* quad,
    uint16_t color,
    const uint16_t gouraud[4]
);
sat_result_t sat_draw_world_polygon_gouraud_effects(
    const sat_mat4_t* view_proj, const sat_quad3_t* quad,
    uint16_t color, const uint16_t gouraud[4], uint16_t flags);

/* White-Gouraud table entry for a light intensity (16.16): SAT_FX16_ONE
 * keeps the part color, 0 subtracts 16 levels, about 1.94 adds 15. Pick the
 * part color as the fully lit one and feed intensities in [0, 1]. */
uint16_t sat_gouraud_from_intensity(sat_fx16_t intensity);

/* Per-vertex white-Gouraud entries under one directional light:
 * intensity = ambient + (1 - ambient) * max(0, normal . light), so with the
 * part color as the fully lit color, lit corners keep it and corners turned
 * away darken towards `ambient`. `normals` and `light` must be unit length,
 * `light` pointing towards the light -- sat_mesh_vertex_normals gives the
 * normals for any mesh. Pass the result as sat_mesh_draw_t::vertex_gouraud. */
sat_result_t sat_gouraud_lambert(
    const sat_vec3_t* normals,
    uint16_t count,
    const sat_vec3_t* light,
    sat_fx16_t ambient,
    uint16_t* out_gouraud
);

/* Projects `count` world points into native VDP1 coordinates in one call,
 * clamped exactly as sat_project_quad clamps corners. A point at or behind
 * the camera plane comes back with w <= 0 rather than as an error. This is
 * the step sat_draw_mesh's projection cache runs once per draw. */
sat_result_t sat_project_vertices(
    const sat_mat4_t* view_proj,
    const sat_vec3_t* points,
    uint16_t count,
    sat_projected_vertex_t* out
);

/* Projects and submits a textured (distorted-sprite) quad. Same rejection
 * rule as sat_draw_world_polygon. */
sat_result_t sat_draw_world_sprite(
    const sat_mat4_t* view_proj,
    const sat_quad3_t* quad,
    const sat_vdp1_texture_t* texture,
    uint16_t palette_override,
    uint16_t flags
);

/* ------------------------------------------------------------------ */
/* Painter's algorithm and flat shading                                */
/* ------------------------------------------------------------------ */

/* Orders `indices` so the largest key comes first, i.e. farthest-first when
 * the keys are squared distances from the camera. Equal keys come out in
 * ascending index order, which for an ascending input list is exactly the
 * stable order. Heapsort: O(n log n) worst case, no scratch memory.
 * `count` must not exceed 255. */
void sat_sort_indices_desc(uint8_t* indices, const uint32_t* keys, uint16_t count);

/* Wide form for meshes past the 255-face limit; `count` may reach 65535. */
void sat_sort_indices16_desc(uint16_t* indices, const uint32_t* keys, uint32_t count);

/* Squared distance between two points on the ground plane, in world units.
 * Saturates rather than overflowing on far-apart points. */
uint32_t sat_ground_distance_sq(sat_fx16_t ax, sat_fx16_t az, sat_fx16_t bx, sat_fx16_t bz);

/* Scales an RGB555 colour by `intensity` (16.16; SAT_FX16_ONE = unchanged,
 * values above it brighten and saturate). Bit 15 is preserved so the result
 * stays a VDP1 RGB-coded colour. This is the whole lighting model available
 * for flat-shaded polygons on this hardware. */
uint16_t sat_shade_rgb555(uint16_t rgb555, sat_fx16_t intensity);

/* Intensity for a face whose outward normal is the ground-plane unit vector
 * (nx, nz), lit by a fixed directional light. Returns 16.16 in
 * [floor, SAT_FX16_ONE], so faces pointing away stay readable instead of
 * going black. Used to give axis-aligned maze walls distinguishable sides. */
sat_fx16_t sat_face_intensity(sat_fx16_t nx, sat_fx16_t nz, sat_fx16_t floor_intensity);

/* Intensity for a face with an arbitrary unit normal, lit by the same light
 * lifted out of the ground plane. This is what sat_draw_mesh applies for
 * SAT_MESH_SHADE, and what a solid built from saturn/mesh3d.h wants: the 2D
 * form above assumes a vertical wall and gives every horizontal surface the
 * same value. */
sat_fx16_t sat_face_intensity3(const sat_vec3_t* normal, sat_fx16_t floor_intensity);

/* Same, for a normal that has direction but no particular length -- the form
 * sat_mesh_face_normal_scaled returns. Dividing the dot product by the length
 * once is cheaper than normalising the vector first, which is why
 * sat_draw_mesh uses this one. */
sat_fx16_t sat_face_intensity3_scaled(const sat_vec3_t* normal, sat_fx16_t floor_intensity);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_RENDER3D_H */
