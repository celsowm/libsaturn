#ifndef SATURN_MATH3D_H
#define SATURN_MATH3D_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* 3D vector / matrix types (16.16 fixed point)                         */
/* ------------------------------------------------------------------ */
typedef struct sat_vec3 {
    sat_fx16_t x;
    sat_fx16_t y;
    sat_fx16_t z;
} sat_vec3_t;

typedef struct sat_vec4 {
    sat_fx16_t x;
    sat_fx16_t y;
    sat_fx16_t z;
    sat_fx16_t w;
} sat_vec4_t;

/* Row-major 4x4 matrix: element (row, col) at m[row * 4 + col]. */
typedef struct sat_mat4 {
    sat_fx16_t m[16];
} sat_mat4_t;

/* ------------------------------------------------------------------ */
/* Scalar helpers                                                      */
/* ------------------------------------------------------------------ */
sat_fx16_t sat_fx16_from_int(int32_t v);
int32_t sat_fx16_to_int(sat_fx16_t v);
sat_fx16_t sat_fx16_mul(sat_fx16_t a, sat_fx16_t b);
sat_fx16_t sat_fx16_div(sat_fx16_t a, sat_fx16_t b);
sat_fx16_t sat_fx16_sqrt(sat_fx16_t v);

/* Angles in degrees. */
sat_fx16_t sat_sin_deg(sat_fx16_t degrees);
sat_fx16_t sat_cos_deg(sat_fx16_t degrees);
sat_fx16_t sat_tan_deg(sat_fx16_t degrees);

/* ------------------------------------------------------------------ */
/* Vector helpers                                                      */
/* ------------------------------------------------------------------ */
/* All operate on 16.16 components, so a world coordinate of a few hundred
 * units is well inside range. Products of two large vectors are not: a cross
 * product of components near 32768 overflows, which in practice means keeping
 * scene coordinates in the hundreds rather than the tens of thousands. */

void sat_vec3_set(sat_vec3_t* out, sat_fx16_t x, sat_fx16_t y, sat_fx16_t z);
void sat_vec3_add(sat_vec3_t* out, const sat_vec3_t* a, const sat_vec3_t* b);
void sat_vec3_sub(sat_vec3_t* out, const sat_vec3_t* a, const sat_vec3_t* b);
void sat_vec3_scale(sat_vec3_t* out, const sat_vec3_t* a, sat_fx16_t s);
sat_fx16_t sat_vec3_dot(const sat_vec3_t* a, const sat_vec3_t* b);

/* Right-handed cross product: out = a x b. */
void sat_vec3_cross(sat_vec3_t* out, const sat_vec3_t* a, const sat_vec3_t* b);

sat_fx16_t sat_vec3_length(const sat_vec3_t* v);

/* Scales `v` to unit length. A zero-length vector is left at zero rather than
 * producing a division by zero, so callers can normalise unconditionally. */
void sat_vec3_normalize(sat_vec3_t* out, const sat_vec3_t* v);

/* ------------------------------------------------------------------ */
/* Matrix construction / math                                          */
/* ------------------------------------------------------------------ */
sat_result_t sat_mat4_identity(sat_mat4_t* out);
sat_result_t sat_mat4_multiply(sat_mat4_t* out, const sat_mat4_t* a, const sat_mat4_t* b);
sat_result_t sat_mat4_translate(sat_mat4_t* out, sat_fx16_t tx, sat_fx16_t ty, sat_fx16_t tz);
sat_result_t sat_mat4_scale(sat_mat4_t* out, sat_fx16_t sx, sat_fx16_t sy, sat_fx16_t sz);
sat_result_t sat_mat4_rotate_x(sat_mat4_t* out, sat_fx16_t degrees);
sat_result_t sat_mat4_rotate_y(sat_mat4_t* out, sat_fx16_t degrees);
sat_result_t sat_mat4_rotate_z(sat_mat4_t* out, sat_fx16_t degrees);

/* Right-handed view matrix (camera looks down -Z). */
sat_result_t sat_mat4_look_at(
    sat_mat4_t* out,
    const sat_vec3_t* eye,
    const sat_vec3_t* center,
    const sat_vec3_t* up
);

/* OpenGL-style perspective projection. fov_y in degrees, aspect = w/h. */
sat_result_t sat_mat4_perspective(
    sat_mat4_t* out,
    sat_fx16_t fov_y,
    sat_fx16_t aspect,
    sat_fx16_t near_z,
    sat_fx16_t far_z
);

sat_result_t sat_mat4_transform_vec4(
    const sat_mat4_t* matrix,
    const sat_vec4_t* v,
    sat_vec4_t* out
);

/* Projects a world point through a pre-computed view-projection matrix to
 * screen pixels, (0,0) top-left. Returns SAT_ERR_UNSUPPORTED when the point is
 * behind the camera (w <= 0); callers must skip drawing then. */
sat_result_t sat_project_to_screen(
    const sat_mat4_t* view_proj,
    sat_fx16_t x,
    sat_fx16_t y,
    sat_fx16_t z,
    int16_t screen_w,
    int16_t screen_h,
    int16_t* out_x,
    int16_t* out_y
);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_MATH3D_H */
