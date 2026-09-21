#ifndef P3D_CONFIG_H
#define P3D_CONFIG_H

/* Numbers more than one pacman_3d module has to agree on: screen, board
 * size, camera framing and the palette. Anything only one module uses lives
 * in that module. */

#include "saturn/color.h"

#include "../common/pacman_maze.h"

#define P3D_SCREEN_W 320
#define P3D_SCREEN_H 224
#define P3D_TILE kPacTilePx

#define P3D_BOARD_W (kPacMazeCols * P3D_TILE) /* 224 */
#define P3D_BOARD_D (kPacMazeRows * P3D_TILE) /* 200 */

/* Camera. These are the numbers that frame the board: from 260 units up and
 * 190 back, a 36-degree vertical field of view puts all four corners of the
 * maze on screen with the top 28 pixels left clear for the HUD. Changing any
 * one of them needs the other two checked -- and P3D_CAM_ELEV_*, below. */
#define P3D_CAM_HEIGHT 260
#define P3D_CAM_BACK 190
#define P3D_CAM_FOV 36

/* The camera's elevation, as the sine and cosine of the angle it looks down
 * at the board. Height and distance are scaled together per angle, so it is
 * the same from every angle: 260 up and 190 back, about 54 degrees. */
#define P3D_CAM_ELEV_SIN 52920 /* 260 / 322, 16.16 */
#define P3D_CAM_ELEV_COS 38670 /* 190 / 322, 16.16 */

/* Camera positions around the board, selectable with L and R. Sixteen,
 * because the whole static scene is baked per angle (board.h) and a baked
 * angle costs about 10KB: sixteen fits in work RAM with room to spare and
 * turns in 22.5-degree steps, which reads as turning rather than as snapping
 * between four fixed views. */
#define P3D_CAM_ANGLES 16u
#define P3D_CAM_STEP_DEGREES (360 / P3D_CAM_ANGLES)

/* Palette. */
#define P3D_COLOR_BOARD SAT_RGB555(1, 2, 7)
#define P3D_COLOR_WALL SAT_RGB555(6, 11, 31)
#define P3D_COLOR_PELLET SAT_RGB555(31, 24, 14)
#define P3D_COLOR_POWER SAT_RGB555(31, 31, 31)
#define P3D_COLOR_PAC SAT_RGB555(31, 30, 2)
#define P3D_COLOR_MOUTH SAT_RGB555(2, 2, 5)
#define P3D_COLOR_FRIGHT_A SAT_RGB555(4, 4, 31)
#define P3D_COLOR_FRIGHT_B SAT_RGB555(31, 31, 31)

#endif /* P3D_CONFIG_H */
