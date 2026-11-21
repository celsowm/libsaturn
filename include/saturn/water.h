#ifndef SATURN_WATER_H
#define SATURN_WATER_H

#include "saturn/geometry3d.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SAT_WATER_SPLIT_PIECES_MAX 4u

typedef struct sat_water_piece {
    sat_quad3_t quad;
    uint8_t submerged;
} sat_water_piece_t;

/* Split a convex planar quad (or triangle with repeated C/D) at y=surface.
 * Pieces preserve winding and share exact edge intersections. Coplanar
 * inputs are dry. Caller supplies SAT_WATER_SPLIT_PIECES_MAX entries;
 * triangles repeat C/D. A failed call leaves the output count untouched. */
sat_result_t sat_water_split_quad(const sat_quad3_t* quad, sat_fx16_t surface,
    sat_water_piece_t out[SAT_WATER_SPLIT_PIECES_MAX], uint8_t* count);

/* A split piece with one RGB555 Gouraud color per corner, in the quad's
 * corner order. Submit it with a material whose vertex_gouraud points at
 * `color` (the quad's four entries are indexed 0..3). */
typedef struct sat_water_shaded_piece {
    sat_water_piece_t piece;
    uint16_t color[4];
} sat_water_shaded_piece_t;

/* sat_water_split_quad that also carries per-corner RGB555 colors. A cut
 * corner blends each 5-bit channel linearly along the edge with the same
 * fixed endpoint order as the position, so reversed shared edges agree. */
sat_result_t sat_water_split_quad_shaded(const sat_quad3_t* quad,
    const uint16_t colors[4], sat_fx16_t surface,
    sat_water_shaded_piece_t out[SAT_WATER_SPLIT_PIECES_MAX], uint8_t* count);

#ifdef __cplusplus
}
#endif
#endif
