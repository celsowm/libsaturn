#ifndef P3D_PAC_MODEL_H
#define P3D_PAC_MODEL_H

/* Pac-Man: a Gouraud-shaded sphere with a real wedge cut out for a mouth. */

#include <stdint.h>

#include "saturn/math3d.h"
#include "saturn/scene.h"

void p3d_pac_init(void);

/* Re-shades for a new key light (world space). Call when the camera turns. */
void p3d_pac_relight(const sat_vec3_t* light);

/* `frame` drives the chewing. */
void p3d_pac_draw(sat_scene_t* scene, int x, int z, int dir, uint32_t frame);

#endif /* P3D_PAC_MODEL_H */
