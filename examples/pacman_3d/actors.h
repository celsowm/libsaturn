#ifndef P3D_ACTORS_H
#define P3D_ACTORS_H

/* Pac-Man and the ghosts: reads where they are from the game and hands them
 * to their models. They move, so unlike the board they are projected live
 * every frame, and their faces go into the scene's painter queue. */

#include "saturn/scene.h"

#include "../common/pacman_game.h"
#include "camera.h"

/* Needs the VDP1 up: the ghosts upload their face textures. */
void p3d_actors_init(void);

/* Re-shades both models for the camera's key light. Call when it turns. */
void p3d_actors_relight(const p3d_camera_t* camera);

void p3d_actors_draw(sat_scene_t* scene, const pac_game_t* game,
                     const p3d_camera_t* camera);

#endif /* P3D_ACTORS_H */
