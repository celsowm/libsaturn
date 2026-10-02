#ifndef SATURN_PATH2_H
#define SATURN_PATH2_H

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/collide2d.h"
#include "saturn/math2d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Deterministic 2D paths: a line, a polyline, a circular arc, a circle, and quadratic
 * and cubic Bezier curves. They carry rails, camera tracks, moving platforms, enemy
 * patrols, pipes and scripted trajectories; nothing here knows what travels on them.
 *
 * A position on a path is a distance: pixels along the path from its start, 16.16, in
 * [0, length]. Sampling gives the point, the unit tangent in the direction of
 * increasing distance, and the unit normal on the left of travel as seen on screen
 * (Y down): for a tangent (tx, ty) the normal is (ty, -tx), the same convention as
 * Terrain2 (a path running along +X has its normal pointing up the screen).
 *
 * Distance is arc length for every kind. Lines, polylines, arcs and circles are exact
 * by construction. A Bezier curve is not parameterised by arc length, so it uses an
 * arc-length table to turn a distance into a curve parameter: sat_path2_build_table
 * fills one at runtime, or an offline tool writes the same data and
 * sat_path2_attach_table takes it. Without a table the curve falls back to a uniform
 * parameter, which still starts and ends exactly on its end points and has the right
 * total length, but moves faster where the control points are far apart.
 *
 * Angles are sat_angle16_t (65536 per turn, 0 along +X, increasing towards +Y, so
 * clockwise on screen). Lengths are 16.16 and limited to 32767 pixels.
 *
 * Paths never allocate and never own memory. A polyline borrows its points and a
 * cumulative-length array; a table is a borrowed array. All of it must outlive the
 * path. A path value can be copied freely. */

typedef enum sat_path2_kind {
    SAT_PATH2_LINE = 1,
    SAT_PATH2_POLYLINE = 2,
    SAT_PATH2_ARC = 3,
    SAT_PATH2_CIRCLE = 4,
    SAT_PATH2_QUADRATIC = 5,
    SAT_PATH2_CUBIC = 6
} sat_path2_kind_t;

#define SAT_PATH2_TABLE_MIN 2u
#define SAT_PATH2_TABLE_MAX 1025u

typedef struct sat_path2 {
    sat_path2_kind_t kind;
    uint8_t closed;                /* the end joins the start: circles and closed polylines */
    uint8_t reserved[1];
    uint16_t count;                /* polyline: points; Bezier: table entries (0 = none) */
    sat_vec2_t p[4];               /* line: ends; Bezier: control points */
    const sat_vec2_t* points;      /* polyline vertices, borrowed */
    const sat_fx16_t* cumulative;  /* polyline: distance of each vertex, borrowed; Bezier: arc-length table */
    sat_vec2_t center;             /* arc, circle */
    sat_fx16_t radius;
    sat_angle16_t start_angle;
    int32_t sweep;                 /* signed angle swept, 65536 = a full turn */
    sat_fx16_t length;
} sat_path2_t;

typedef struct sat_path2_sample {
    sat_vec2_t position;
    sat_vec2_t tangent;  /* unit; zero only on a degenerate Bezier */
    sat_vec2_t normal;   /* unit, left of travel */
    sat_fx16_t distance; /* the (clamped or wrapped) distance that was sampled */
} sat_path2_sample_t;

typedef enum sat_path2_edge {
    SAT_PATH2_CLAMP = 0, /* stop at the ends */
    SAT_PATH2_WRAP = 1   /* continue from the other end (any path; the natural choice for closed ones) */
} sat_path2_edge_t;

#define SAT_PATH2_HIT_START 0x01u /* an advance was stopped at distance 0 */
#define SAT_PATH2_HIT_END 0x02u   /* an advance was stopped at the length */
#define SAT_PATH2_WRAPPED 0x04u   /* an advance went past an end and came out the other */

typedef struct sat_path2_nearest {
    sat_path2_sample_t sample; /* the closest point on the path */
    sat_fx16_t gap;            /* its distance from the query point */
} sat_path2_nearest_t;

/* All initialisers return SAT_ERR_INVALID_ARG for a missing pointer, a degenerate shape
 * (zero length, no radius, no sweep) or a length above 32767 px. */
sat_result_t sat_path2_init_line(sat_path2_t* path, sat_vec2_t from, sat_vec2_t to);

/* `cumulative` needs sat_path2_polyline_entries(count, closed) entries; it is filled
 * here. A closed polyline gets an extra final segment back to the first point. */
uint32_t sat_path2_polyline_entries(uint16_t point_count, int closed);
sat_result_t sat_path2_init_polyline(sat_path2_t* path, const sat_vec2_t* points, uint16_t count, int closed,
    sat_fx16_t* cumulative, uint32_t cumulative_entries);

/* An arc of `radius` pixels about `center`, from `start_angle` through `sweep` (signed, up to a
 * full turn; the sign gives the direction of travel). */
sat_result_t sat_path2_init_arc(sat_path2_t* path, sat_vec2_t center, sat_fx16_t radius, sat_angle16_t start_angle,
    int32_t sweep);
/* A full circle starting at angle 0, travelling towards +Y (clockwise on screen); closed. */
sat_result_t sat_path2_init_circle(sat_path2_t* path, sat_vec2_t center, sat_fx16_t radius);

sat_result_t sat_path2_init_quadratic(sat_path2_t* path, sat_vec2_t p0, sat_vec2_t p1, sat_vec2_t p2);
sat_result_t sat_path2_init_cubic(sat_path2_t* path, sat_vec2_t p0, sat_vec2_t p1, sat_vec2_t p2, sat_vec2_t p3);

/* Bytes of table storage for `entries` samples. */
uint32_t sat_path2_table_requirements(uint32_t entries);

/* Bezier only. Samples the curve at `entries` evenly spaced parameters (SAT_PATH2_TABLE_MIN ..
 * SAT_PATH2_TABLE_MAX), stores the running length in `storage` and attaches it; it also
 * refines the path's length from the finer sampling. Entry 0 is 0 and the last entry the length. */
sat_result_t sat_path2_build_table(sat_path2_t* path, sat_fx16_t* storage, uint32_t entries);

/* Bezier only. Attaches a table written by an offline tool: non-decreasing, first entry 0, last
 * entry the curve's length (which replaces the length the initialiser estimated). */
sat_result_t sat_path2_attach_table(sat_path2_t* path, const sat_fx16_t* table, uint32_t entries);

sat_fx16_t sat_path2_length(const sat_path2_t* path);
int sat_path2_is_closed(const sat_path2_t* path);

/* The point, tangent and normal at `distance`. Out-of-range distances are clamped on an open
 * path and wrapped on a closed one. SAT_ERR_INVALID_ARG for a path that was never initialised. */
sat_result_t sat_path2_sample(const sat_path2_t* path, sat_fx16_t distance, sat_path2_sample_t* out);

/* Moves `distance` by the signed `delta`. Returns SAT_PATH2_HIT_* / SAT_PATH2_WRAPPED bits in
 * `flags` (may be NULL) and the new distance in `out_distance`. Walking exactly onto an end
 * with CLAMP reports the hit. */
sat_result_t sat_path2_advance(const sat_path2_t* path, sat_fx16_t distance, sat_fx16_t delta,
    sat_path2_edge_t edge, sat_fx16_t* out_distance, uint8_t* flags);

/* The closest point of the path to `point`: exact for lines and polylines, limited by the 16-bit
 * angle step for arcs and circles (about a third of a pixel at 200 px radius), and a coarse
 * search refined to well under a pixel for Bezier curves. When two points of a polyline are
 * equally close the one at the smaller distance wins. */
sat_result_t sat_path2_nearest(const sat_path2_t* path, sat_vec2_t point, sat_path2_nearest_t* out);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_PATH2_H */
