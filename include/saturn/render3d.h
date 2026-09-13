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

/* Submits an ALREADY projected quad -- the output of sat_project_quad -- as a
 * flat-shaded polygon.
 *
 * This exists for scenes whose camera does not move. Projection is the
 * expensive half of drawing on this hardware (four matrix transforms and two
 * 64-bit divides per corner), and a fixed camera projects the same static
 * geometry to the same screen coordinates every frame. Projecting it once at
 * startup and replaying the corners turns a per-frame cost into a startup
 * cost; see examples/pacman_3d, where it is the difference between the board
 * running at full rate and at a fifth of it. */
sat_result_t sat_draw_quad2_polygon(const sat_quad2_t* quad, uint16_t color);

/* Projects and submits a textured (distorted-sprite) quad. Same rejection
 * rule as sat_draw_world_polygon. */
sat_result_t sat_draw_world_sprite(
    const sat_mat4_t* view_proj,
    const sat_quad3_t* quad,
    const sat_texture_t* texture,
    uint16_t palette_override,
    uint16_t flags
);

/* ------------------------------------------------------------------ */
/* Painter's algorithm and flat shading                                */
/* ------------------------------------------------------------------ */

/* Orders `indices` so the largest key comes first, i.e. farthest-first when
 * the keys are squared distances from the camera. Insertion sort: stable, no
 * scratch memory, and near-linear on the frame-to-frame coherent orders a
 * moving camera produces. `count` must not exceed 255. */
void sat_sort_indices_desc(uint8_t* indices, const uint32_t* keys, uint16_t count);

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
