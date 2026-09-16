#ifndef SATURN_COLLIDE2D_H
#define SATURN_COLLIDE2D_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Small allocation-free 2D collision primitives. Coordinates are 16.16
 * world pixels and Y points down. Boxes use a centre and half extents so a
 * moving actor can be expanded for a sweep without rebuilding corners.
 *
 * Edges are strict: two shapes that only touch are not overlapping. This is
 * intentional for platformers (a foot can rest on a floor while grounded)
 * and makes a six-pixel Pac-Man touch mean exactly |dx| < 6. Squares and dot
 * products stay in int64 at the raw 2^32 scale; overlap tests never sqrt. */

typedef struct sat_vec2 { sat_fx16_t x, y; } sat_vec2_t;
typedef struct sat_box2 { sat_vec2_t center, half; } sat_box2_t;
typedef struct sat_circle { sat_vec2_t center; sat_fx16_t radius; } sat_circle_t;
typedef struct sat_contact2 { sat_vec2_t normal; sat_fx16_t depth; } sat_contact2_t;
typedef struct sat_hit2 {
    sat_fx16_t t; /* 0..1 fraction of the supplied segment */
    sat_vec2_t point;
    sat_vec2_t normal;
} sat_hit2_t;

sat_vec2_t sat_vec2_add(sat_vec2_t a, sat_vec2_t b);
sat_vec2_t sat_vec2_sub(sat_vec2_t a, sat_vec2_t b);
sat_vec2_t sat_vec2_scale(sat_vec2_t a, sat_fx16_t scale);
sat_fx16_t sat_approach(sat_fx16_t value, sat_fx16_t target, sat_fx16_t step);
sat_vec2_t sat_reflect2(sat_vec2_t velocity, sat_vec2_t normal, sat_fx16_t restitution);

int sat_box2_overlap(const sat_box2_t* a, const sat_box2_t* b);
int sat_circle_overlap(const sat_circle_t* a, const sat_circle_t* b);
int sat_circle_box_overlap(const sat_circle_t* circle, const sat_box2_t* box);
int sat_point_in_box2(const sat_vec2_t* point, const sat_box2_t* box);
int sat_point_in_circle(const sat_vec2_t* point, const sat_circle_t* circle);

int sat_box2_contact(const sat_box2_t* a, const sat_box2_t* b, sat_contact2_t* out);
int sat_circle_contact(const sat_circle_t* a, const sat_circle_t* b, sat_contact2_t* out);
int sat_circle_box_contact(const sat_circle_t* circle, const sat_box2_t* box, sat_contact2_t* out);

int sat_raycast_box2(const sat_box2_t* box, const sat_vec2_t* origin,
                     const sat_vec2_t* delta, sat_hit2_t* out);
int sat_raycast_circle(const sat_circle_t* circle, const sat_vec2_t* origin,
                       const sat_vec2_t* delta, sat_hit2_t* out);
int sat_sweep_box2(const sat_box2_t* moving, const sat_vec2_t* delta,
                   const sat_box2_t* target, sat_hit2_t* out);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_COLLIDE2D_H */
