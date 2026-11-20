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

#ifdef __cplusplus
}
#endif
#endif
