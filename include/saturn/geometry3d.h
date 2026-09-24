#ifndef SATURN_GEOMETRY3D_H
#define SATURN_GEOMETRY3D_H

#include "saturn/math3d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* World-space quad with the same A/B/C/D winding used by mesh builders,
 * collision queries and the optional VDP1 renderer. No GPU state or
 * render-runtime initialization is required to construct this value type. */
typedef struct sat_quad3 {
    sat_vec3_t v[4];
} sat_quad3_t;

#ifdef __cplusplus
}
#endif

#endif /* SATURN_GEOMETRY3D_H */
