#ifndef P3D_BOARD_H
#define P3D_BOARD_H

/* The static half of the scene -- board, walls and pellets -- baked once per
 * camera angle and replayed every frame.
 *
 * The camera can only be in one of P3D_CAM_ANGLES positions, and that
 * restriction is the whole performance story. From a given position the
 * maze always projects to the same screen coordinates, so it is projected
 * ONCE -- culled, shaded and depth-sorted there too -- and each frame only
 * replays the corners. Projection is four matrix transforms and two 64-bit
 * divides per corner, and this is about 1500 corners; doing it every frame
 * ran the board at roughly a fifth of full rate.
 *
 * The walls change with the stage, so a stage change re-bakes all angles
 * (about two seconds, behind a progress bar). */

#include <stdint.h>

#include "saturn/scene.h"

#include "../common/pacman_game.h"

/* Called once per baked angle, before it is baked. */
typedef void (*p3d_bake_progress_fn)(uint16_t done, uint16_t total);

void p3d_board_init(void);

/* Bakes every camera angle for the walls and pellets currently in `game`. */
void p3d_board_bake(const pac_game_t* game, p3d_bake_progress_fn progress);

/* Replays the baked view for `angle` into `scene`, skipping eaten pellets. */
void p3d_board_draw(sat_scene_t* scene, const pac_game_t* game, uint16_t angle);

#endif /* P3D_BOARD_H */
