#ifndef PACMAN_GAME_H
#define PACMAN_GAME_H

/* Shared Pac-Man simulation for the pacman_2d and pacman_3d examples.
 *
 * This is the whole game: maze state, actor movement, ghost AI, pellets,
 * scoring, stage progression and win/lose transitions. The layouts it plays
 * live in pacman_stages.c and are read by pacman_level.c. It draws nothing
 * and touches no hardware beyond reading a pad state, so the two examples
 * are the same game rendered two ways rather than two games that happen to
 * look alike.
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

#include "pacman_level.h"
#include "pacman_maze.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Frames a power pellet keeps the ghosts frightened (~5 s at 60 fps). */
#define PAC_FRIGHT_FRAMES 300u

/* Pixels per frame. 2 divides the 8-pixel tile, so actors land exactly on
 * tile centres every four frames and never drift off the turn grid. */
#define PAC_SPEED 2
#define PAC_GHOST_SPEED 2

#define PAC_START_LIVES 3

/* pac_game_facing's slot for Pac-Man; 0..PAC_GHOST_COUNT-1 are the ghosts,
 * in the same order as pac_game_t::ghosts. */
#define PAC_SLOT_PAC PAC_GHOST_COUNT

/* Frames the cleared maze stays on screen before the next stage loads. */
#define PAC_STAGE_CLEAR_FRAMES 120u

typedef enum pac_state {
    PAC_STATE_PLAY = 0,
    PAC_STATE_STAGE_CLEAR, /* a stage was cleared; the next loads shortly */
    PAC_STATE_WIN,         /* the last stage was cleared */
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
    PAC_EVENT_LOSE        = 1u << 5,
    PAC_EVENT_STAGE_CLEAR = 1u << 6,
    /* A stage's maze was (re)loaded: renderers that cache anything derived
     * from the walls rebuild it. Also raised by a restart. */
    PAC_EVENT_STAGE_START = 1u << 7
} pac_event_t;

typedef struct pac_ghost {
    sat_grid_actor_t actor;
    uint16_t release;  /* frames left before leaving the pen */
    uint8_t index;
    uint8_t reserved;
} pac_ghost_t;

typedef struct pac_game {
    /* Mutable copy of the current stage's layout; pellets are cleared from
     * it as they are eaten, so it doubles as the pellet map. */
    char maze[kPacMazeRows][kPacMazeCols + 1];
    /* Spawns and pen of the current stage. */
    pac_level_t level;

    sat_grid_t grid;
    sat_grid_actor_t pac;
    pac_ghost_t ghosts[PAC_GHOST_COUNT];

    pac_state_t state;
    uint32_t score;
    uint32_t frame;
    uint16_t events;     /* bitwise OR of pac_event_t for the last update */
    uint16_t fright;     /* frames of frightened mode remaining */
    int16_t lives;
    int16_t pellets;     /* pellets left; reaching 0 clears the stage */
    uint16_t stage;      /* index into pacman_stages.h */
    uint16_t stage_timer; /* frames left in PAC_STATE_STAGE_CLEAR */
    /* PAC_LEVEL_OK, or why the current stage failed to load -- in which case
     * the game sits in PAC_STATE_LOSE rather than play a broken maze. */
    pac_level_error_t level_error;
    /* Last non-SAT_DIR_NONE heading of each ghost, then Pac-Man at
     * PAC_SLOT_PAC -- see pac_game_facing. */
    int16_t facing[PAC_GHOST_COUNT + 1];
    uint32_t rng;
} pac_game_t;

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

/* Prepares a game whose tile (0,0) starts at pixel (origin_x, origin_y), and
 * starts a fresh round. Call once. */
void pac_game_init(pac_game_t* game, int16_t origin_x, int16_t origin_y);

/* Back to the first stage with a full maze, score and lives. */
void pac_game_reset(pac_game_t* game);

/* Loads stage `index` and puts everyone on their spawns, keeping score and
 * lives. Out-of-range indices are ignored. */
void pac_game_load_stage(pac_game_t* game, uint16_t index);

/* Advances one frame: applies the pad, moves Pac-Man and the ghosts, eats
 * pellets, resolves collisions and updates state. Clearing a stage pauses
 * for PAC_STAGE_CLEAR_FRAMES and then loads the next one; clearing the last
 * wins. Does nothing but tick the frame counter once the game is won or lost
 * -- except that START restarts. */
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

/* Direction actor `slot` is facing -- pass 0..PAC_GHOST_COUNT-1 for a ghost
 * or PAC_SLOT_PAC for Pac-Man. Unlike the raw sat_grid_actor_t::dir this
 * field comes from, it is never SAT_DIR_NONE once the round has moved at
 * all: an actor stopped square against a wall keeps its last real heading,
 * which is what a sprite direction, a model's facing or a ghost's pupils
 * should track. Both renderers computed this independently before it moved
 * here; an out-of-range slot or a NULL game reads as SAT_DIR_NONE. */
int pac_game_facing(const pac_game_t* game, int slot);

/* One line describing why the round is not being played right now -- a
 * broken stage layout, a win, a loss, or a cleared stage waiting on the
 * next one -- or NULL while play is ongoing and a HUD has nothing to say.
 * Both examples grew their own copy of this exact if/else chain; centralising
 * it is what keeps their wording from drifting apart the next time either
 * one's message changes. The string is static storage, never freed. */
const char* pac_game_status_text(const pac_game_t* game);

/* Non-zero while the status text above should be followed by a "PRESS
 * START" prompt -- everything status_text covers except a stage clearing,
 * which resolves on its own. */
int pac_game_status_needs_start(const pac_game_t* game);

#ifdef __cplusplus
}
#endif

#endif /* PACMAN_GAME_H */
