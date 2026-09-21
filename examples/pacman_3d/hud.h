#ifndef P3D_HUD_H
#define P3D_HUD_H

/* Score, lives, stage banners and the bake progress screen. */

#include <stdint.h>

#include "../common/pacman_game.h"

void p3d_hud_init(void);

/* Draws over the finished world pass. */
void p3d_hud_draw(const pac_game_t* game);

/* A whole frame of its own: "BUILDING STAGE n" and a bar at done/total.
 * Baking takes a couple of seconds, and a program that shows nothing for
 * that long looks like one that has hung. */
void p3d_hud_bake_progress(uint16_t stage, uint16_t done, uint16_t total);

#endif /* P3D_HUD_H */
