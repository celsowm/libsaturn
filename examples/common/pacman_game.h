#ifndef PACMAN_GAME_H
#define PACMAN_GAME_H

/* Shared Pac-Man simulation for the pacman_2d and pacman_3d examples.
 *
 * This is the whole game: maze state, actor movement, ghost AI, pellets,
 * scoring and win/lose transitions. It draws nothing and touches no hardware
 * beyond reading a pad state, so the two examples are the same game rendered
 * two ways rather than two games that happen to look alike.
 *
 * Movement is built on the library's sat_grid module, so tile probing, turn
 * timing, tunnel wrapping and pursuit all come from code that is covered by
 * host tests (tests/host/test_grid_logic.cpp) instead of being written twice.
 *
 * Positions are in maze pixels: tile (0,0) spans [origin, origin + 8) on both
 * axes. The 2D example passes a screen origin so the maze sits centred on the
 * display; the 3D example passes (0,0) and treats the same numbers as world X
 * and Z. Nothing else differs.
 */

#include <stddef.h>
#include <stdint.h>

#include "saturn/grid.h"
#include "saturn/input.h"

#include "pacman_maze.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PAC_GHOST_COUNT 4

/* Frames a power pellet keeps the ghosts frightened (~5 s at 60 fps). */
#define PAC_FRIGHT_FRAMES 300u

/* Pixels per frame. 2 divides the 8-pixel tile, so actors land exactly on
 * tile centres every four frames and never drift off the turn grid. */
#define PAC_SPEED 2
#define PAC_GHOST_SPEED 2

#define PAC_START_LIVES 3

typedef enum pac_state {
    PAC_STATE_PLAY = 0,
    PAC_STATE_WIN,
    PAC_STATE_LOSE
} pac_state_t;

/* What the simulation did on the frame just stepped, for renderers that want
 * to react (a flash, a sound, a camera shake). Cleared at the top of every
 * pac_game_update. */
typedef enum pac_event {
    PAC_EVENT_NONE        = 0,
    PAC_EVENT_PELLET      = 1u << 0,
    PAC_EVENT_POWER       = 1u << 1,
    PAC_EVENT_GHOST_EATEN = 1u << 2,
    PAC_EVENT_LIFE_LOST   = 1u << 3,
    PAC_EVENT_WIN         = 1u << 4,
    PAC_EVENT_LOSE        = 1u << 5
} pac_event_t;

typedef struct pac_ghost {
    sat_grid_actor_t actor;
    uint16_t release;  /* frames left before leaving the pen */
    uint8_t index;
    uint8_t reserved;
} pac_ghost_t;

typedef struct pac_game {
    /* Mutable copy of kPacMaze; pellets are cleared from it as they are
     * eaten, so it doubles as the pellet map. */
    char maze[kPacMazeRows][kPacMazeCols + 1];

    sat_grid_t grid;
    sat_grid_actor_t pac;
    pac_ghost_t ghosts[PAC_GHOST_COUNT];

    pac_state_t state;
    uint32_t score;
    uint32_t frame;
    uint16_t events;     /* bitwise OR of pac_event_t for the last update */
    uint16_t fright;     /* frames of frightened mode remaining */
    int16_t lives;
    int16_t pellets;     /* pellets left; reaching 0 wins */
    uint32_t rng;
} pac_game_t;

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

/* Prepares a game whose tile (0,0) starts at pixel (origin_x, origin_y), and
 * starts a fresh round. Call once. */
void pac_game_init(pac_game_t* game, int16_t origin_x, int16_t origin_y);

/* Restores the full maze and resets score and lives. */
void pac_game_reset(pac_game_t* game);

/* Advances one frame: applies the pad, moves Pac-Man and the ghosts, eats
 * pellets, resolves collisions and updates state. Does nothing but tick the
 * frame counter once the game is won or lost -- except that START restarts. */
void pac_game_update(pac_game_t* game, const sat_pad_state_t* pad);

/* ------------------------------------------------------------------ */
/* Queries for renderers                                               */
/* ------------------------------------------------------------------ */

/* Maze character at (col,row): '#', '.', 'o' or ' '. Out-of-range reads
 * return '#'. */
char pac_game_cell(const pac_game_t* game, int col, int row);

/* Non-zero while the ghosts are edible. */
int pac_game_frightened(const pac_game_t* game);

/* Non-zero while ghost `index` is still waiting in the pen (renderers
 * generally skip drawing it). */
int pac_game_ghost_penned(const pac_game_t* game, int index);

/* sat_grid walkability callback. Pass the pac_game_t* as `user`. Exposed so
 * renderers can probe the maze with the same rule the simulation uses. */
int pac_game_is_floor(int col, int row, void* user);

#ifdef __cplusplus
}
#endif

#endif /* PACMAN_GAME_H */
