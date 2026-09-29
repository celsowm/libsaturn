#ifndef PACMAN_STAGES_H
#define PACMAN_STAGES_H

/* The stage list: every layout the Pac-Man examples play, in order.
 *
 * Data only. Turning a layout into something playable -- finding the spawns
 * and the pen, checking it is well formed -- is pacman_level.h's job, and the
 * order the stages are played in is pacman_game.h's. */

#include <stdint.h>

#include "pacman_maze.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pac_stage {
    const char* name;
    /* kPacMazeRows strings of exactly kPacMazeCols cells each; the legend is
     * in pacman_maze.h. */
    const char* rows[kPacMazeRows];
} pac_stage_t;

uint16_t pac_stage_count(void);

/* NULL when `index` is out of range. */
const pac_stage_t* pac_stage_get(uint16_t index);

#ifdef __cplusplus
}
#endif

#endif /* PACMAN_STAGES_H */
