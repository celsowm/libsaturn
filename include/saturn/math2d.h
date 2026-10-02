#ifndef SATURN_MATH2D_H
#define SATURN_MATH2D_H

#include <stdint.h>

#include "saturn/collide2d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 2D fixed-point helpers shared by terrain, character, path and camera code.
 * No allocation, no floating point, no hardware access.
 *
 * Angles. `sat_angle_t` is a binary angle: 256 steps per turn, 0 along +X and
 * 64 along +Y. Y points down on screen, so increasing angles turn clockwise as
 * seen on screen. It wraps naturally in uint8_t arithmetic. `sat_angle16_t`
 * is the same convention with 65536 steps per turn, for smooth curves.
 *
 * Fixed point. Values are sat_fx16_t (16.16). Unit vectors have components in
 * [-SAT_FX16_ONE, SAT_FX16_ONE]. Products use 64-bit intermediates, so the
 * helpers below are exact for any 16.16 operands whose result fits in 32 bits.
 */
typedef uint8_t sat_angle_t;
typedef uint16_t sat_angle16_t;

#define SAT_ANGLE_QUARTER ((sat_angle_t)64)
#define SAT_ANGLE_HALF ((sat_angle_t)128)

/* sin/cos in 16.16: exact table value for the 8-bit angle; the 16-bit forms
 * interpolate linearly between table entries (error below 1e-4). */
sat_fx16_t sat_sin8(sat_angle_t angle);
sat_fx16_t sat_cos8(sat_angle_t angle);
sat_fx16_t sat_sin16(sat_angle16_t angle);
sat_fx16_t sat_cos16(sat_angle16_t angle);

/* Unit vector (cos a, sin a). */
sat_vec2_t sat_vec2_from_angle(sat_angle_t angle);

/* Angle of the vector (x, y) with the convention above. (0, 0) gives 0. */
sat_angle_t sat_atan2_8(sat_fx16_t y, sat_fx16_t x);
sat_angle16_t sat_atan2_16(sat_fx16_t y, sat_fx16_t x);

/* Smallest signed difference to - from, in [-128, 127]. */
int8_t sat_angle_diff(sat_angle_t from, sat_angle_t to);

/* Quadrant of an angle after rounding to the nearest axis: 0 = +X, 1 = +Y,
 * 2 = -X, 3 = -Y. Angles within 32 steps of an axis map to that axis. */
uint8_t sat_angle_quadrant(sat_angle_t angle);

sat_fx16_t sat_vec2_dot(sat_vec2_t a, sat_vec2_t b);
/* Euclidean length, exact floor of the root of the raw sum of squares. */
sat_fx16_t sat_vec2_length(sat_vec2_t v);
/* Unit vector in the direction of v; the zero vector stays zero. */
sat_vec2_t sat_vec2_normalize(sat_vec2_t v);
/* Rotate by +90 degrees in the Y-down convention: (x, y) -> (-y, x). */
sat_vec2_t sat_vec2_perp(sat_vec2_t v);
/* Component of v along the unit vector `unit`, and the remainder. */
sat_vec2_t sat_vec2_project(sat_vec2_t v, sat_vec2_t unit);
sat_vec2_t sat_vec2_reject(sat_vec2_t v, sat_vec2_t unit);

/* a + (b - a) * t, t in 16.16 (not clamped). */
sat_fx16_t sat_lerp_fx16(sat_fx16_t a, sat_fx16_t b, sat_fx16_t t);
sat_vec2_t sat_vec2_lerp(sat_vec2_t a, sat_vec2_t b, sat_fx16_t t);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_MATH2D_H */
