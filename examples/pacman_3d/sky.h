#ifndef P3D_SKY_H
#define P3D_SKY_H

/* The starfield behind the board, on VDP2 NBG0. */

#include <stdint.h>

void p3d_sky_init(void);

/* Scrolls the stars with the camera heading. */
void p3d_sky_set_angle(uint16_t angle);

#endif /* P3D_SKY_H */
