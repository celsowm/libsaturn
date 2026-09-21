#ifndef PACMAN_LEVEL_H
#define PACMAN_LEVEL_H

/* Turns a stage layout into a playable level.
 *
 * A layout marks its spawns and its pen door with letters (pacman_maze.h);
 * this reads them out, works out which tiles make up the pen, and checks the
 * whole thing is playable. Everything the simulation used to hard-code for
 * the one classic maze -- where Pac-Man starts, where each ghost starts, the
 * pen box a ghost has to find its way out of, the tile outside the door --
 * comes from here instead, which is what lets a new stage be nothing but a
 * new layout. */

#include <stdint.h>

#include "pacman_maze.h"
#include "pacman_stages.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum pac_level_error {
    PAC_LEVEL_OK = 0,
    PAC_LEVEL_BAD_SHAPE,      /* a row is missing or not kPacMazeCols wide */
    PAC_LEVEL_BAD_CELL,       /* a character outside the legend */
    PAC_LEVEL_PAC_SPAWN,      /* not exactly one 'P' */
    PAC_LEVEL_GHOST_SPAWNS,   /* not exactly PAC_GHOST_COUNT 'G's */
    PAC_LEVEL_DOOR,           /* no door, a door not in one row, or no floor above it */
    PAC_LEVEL_PEN_LEAKS,      /* Pac-Man's tile is reachable from the pen without the door */
    PAC_LEVEL_UNREACHABLE     /* a pellet, ghost or door Pac-Man cannot get to */
} pac_level_error_t;

typedef struct pac_level {
    int8_t pac_col;
    int8_t pac_row;
    int8_t ghost_col[PAC_GHOST_COUNT];
    int8_t ghost_row[PAC_GHOST_COUNT];
    /* The door is a run of cells in one row, and ghosts leave through the
     * row above it. */
    int8_t door_col_min;
    int8_t door_col_max;
    int8_t door_row;
    int8_t reserved;
    /* Bit c of pen_rows[r] is set when (c, r) is pen floor or door: the
     * tiles where a ghost steers for the door instead of hunting. */
    uint32_t pen_rows[kPacMazeRows];
    int16_t pellets;
    uint16_t reserved2;
} pac_level_t;

/* Reads `stage` into `maze` (markers replaced by the floor under them) and
 * `out`. On any error other than PAC_LEVEL_OK both are left in an
 * unspecified state and must not be played. */
pac_level_error_t pac_level_load(const pac_stage_t* stage,
                                 char maze[kPacMazeRows][kPacMazeCols + 1],
                                 pac_level_t* out);

/* Non-zero while (col, row) is inside the pen or on its door. */
int pac_level_in_pen(const pac_level_t* level, int col, int row);

#ifdef __cplusplus
}
#endif

#endif /* PACMAN_LEVEL_H */
