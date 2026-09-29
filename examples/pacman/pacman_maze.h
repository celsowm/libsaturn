#ifndef PACMAN_MAZE_H
#define PACMAN_MAZE_H

/* Maze dimensions and cell vocabulary shared by the Pac-Man examples.
 *
 * Every stage is 28 columns x 25 rows. The size is fixed rather than per
 * stage because both renderers are laid out around it: the 2D example
 * centres a 224x200 maze on a 320x224 screen, and the 3D one frames a board
 * of that size with its camera.
 *
 * Layout legend, as written in pacman_stages.c:
 *   '#' wall
 *   '.' pellet
 *   'o' power pellet
 *   ' ' open floor with nothing on it (the pen, the tunnel, the void
 *       outside the maze)
 *   'P' where Pac-Man starts; the tile holds a pellet, so his first step
 *       already eats
 *   'G' where a ghost starts, four of them, all inside the pen; read left to
 *       right, top to bottom, they are ghosts 0 to 3
 *   '-' the pen door: open floor that the pen's outline stops at
 *
 * Once a stage is loaded (pacman_level.h) only '#', '.', 'o' and ' ' remain
 * in the maze; the markers become the level's spawn and pen data.
 *
 * Cell coordinates are (col, row). 1 tile = kPacTilePx x kPacTilePx pixels.
 */

enum {
    kPacMazeCols = 28,
    kPacMazeRows = 25,
    kPacTilePx = 8,
    kPacMazePixelW = kPacMazeCols * kPacTilePx,  /* 224 */
    kPacMazePixelH = kPacMazeRows * kPacTilePx,  /* 200 */
    kPacTileNone = -1
};

#define PAC_GHOST_COUNT 4

#define PAC_CELL_WALL '#'
#define PAC_CELL_PELLET '.'
#define PAC_CELL_POWER 'o'
#define PAC_CELL_EMPTY ' '
#define PAC_CELL_PAC 'P'
#define PAC_CELL_GHOST 'G'
#define PAC_CELL_DOOR '-'

#endif /* PACMAN_MAZE_H */
