/* Shared Pac-Man simulation. See pacman_game.h for the contract. */

#define PACMAN_MAZE_IMPL
#include "pacman_game.h"

/* ------------------------------------------------------------------ */
/* Spawn cells                                                         */
/* ------------------------------------------------------------------ */
/* Every one of these is a verified floor tile of kPacMaze. Spawning an actor
 * on a wall leaves it wedged in the scenery, which is not obvious on screen
 * because it still animates -- it simply never goes anywhere.
 *
 *   (12,20) sits on the pellet corridor below the ghost pen, so the player's
 *   first move in any direction immediately does something. The blank lane at
 *   row 17 is also floor, but it holds no pellets and dead-ends at both sides,
 *   which makes the game look broken for the first several seconds.
 *   (12..15, 14) are the four interior cells of the pen itself.
 */
#define PAC_SPAWN_COL 12
#define PAC_SPAWN_ROW 20
#define PAC_PEN_ROW 14
static const uint8_t kGhostSpawnCol[PAC_GHOST_COUNT] = {12, 13, 14, 15};

/* Scatter corners, one per ghost, so they spread out instead of stacking up
 * on the same tile whenever they are not chasing. */
static const uint8_t kScatterCol[PAC_GHOST_COUNT] = {26, 1, 26, 1};
static const uint8_t kScatterRow[PAC_GHOST_COUNT] = {1, 1, 23, 23};

/* Ghosts alternate scatter and chase on this period, and leave the pen
 * staggered so they do not emerge as a single clump. */
#define PAC_MODE_PERIOD 240u
#define PAC_RELEASE_STEP 45u

/* Half-width of the square used for the Pac-Man/ghost overlap test. Actors
 * are 8 pixels apart at adjacent tile centres, so 6 catches a genuine overlap
 * one step before they pass through each other. */
#define PAC_TOUCH_RANGE 6

#define PAC_SCORE_PELLET 10u
#define PAC_SCORE_POWER 50u
#define PAC_SCORE_GHOST 200u

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

/* xorshift32: one multiply-free step, good enough to keep frightened ghosts
 * from moving in lockstep, and deterministic so a replay is reproducible. */
static uint32_t rng_next(pac_game_t* game) {
    uint32_t x = game->rng;
    x ^= x << 13u;
    x ^= x >> 17u;
    x ^= x << 5u;
    game->rng = x;
    return x;
}

int pac_game_is_floor(int col, int row, void* user) {
    const pac_game_t* game = (const pac_game_t*)user;
    if (game == NULL || row < 0 || row >= kPacMazeRows ||
        col < 0 || col >= kPacMazeCols) {
        return 0;
    }
    return game->maze[row][col] != '#' ? 1 : 0;
}

char pac_game_cell(const pac_game_t* game, int col, int row) {
    if (game == NULL || row < 0 || row >= kPacMazeRows ||
        col < 0 || col >= kPacMazeCols) {
        return '#';
    }
    return game->maze[row][col];
}

int pac_game_frightened(const pac_game_t* game) {
    return (game != NULL && game->fright > 0u) ? 1 : 0;
}

int pac_game_ghost_penned(const pac_game_t* game, int index) {
    if (game == NULL || index < 0 || index >= PAC_GHOST_COUNT) {
        return 0;
    }
    return game->ghosts[index].release > 0u ? 1 : 0;
}

static int actor_col(const pac_game_t* game, const sat_grid_actor_t* a) {
    return sat_grid_wrap_col(&game->grid, sat_grid_col_at(&game->grid, (int)a->x));
}

static int actor_row(const pac_game_t* game, const sat_grid_actor_t* a) {
    return sat_grid_row_at(&game->grid, (int)a->y);
}

static void place_actor(const pac_game_t* game, sat_grid_actor_t* a, int col, int row, int dir) {
    a->x = sat_grid_tile_center_x(&game->grid, col);
    a->y = sat_grid_tile_center_y(&game->grid, row);
    a->dir = (int16_t)dir;
    a->want = (int16_t)dir;
    a->reserved = 0;
}

/* ------------------------------------------------------------------ */
/* Spawning                                                            */
/* ------------------------------------------------------------------ */

static void spawn_pac(pac_game_t* game) {
    place_actor(game, &game->pac, PAC_SPAWN_COL, PAC_SPAWN_ROW, SAT_DIR_LEFT);
    game->pac.speed = PAC_SPEED;
}

static void spawn_ghost(pac_game_t* game, int index) {
    pac_ghost_t* ghost = &game->ghosts[index];
    place_actor(
        game,
        &ghost->actor,
        (int)kGhostSpawnCol[index],
        PAC_PEN_ROW,
        (index & 1) ? SAT_DIR_LEFT : SAT_DIR_RIGHT);
    ghost->actor.speed = PAC_GHOST_SPEED;
    ghost->index = (uint8_t)index;
    ghost->reserved = 0;
    ghost->release = (uint16_t)(index * PAC_RELEASE_STEP);
}

static void spawn_all(pac_game_t* game) {
    int i;
    spawn_pac(game);
    for (i = 0; i < PAC_GHOST_COUNT; ++i) {
        spawn_ghost(game, i);
    }
    game->fright = 0u;
}

static void load_maze(pac_game_t* game) {
    int r;
    int c;
    game->pellets = 0;
    for (r = 0; r < kPacMazeRows; ++r) {
        for (c = 0; c < kPacMazeCols; ++c) {
            const char cell = kPacMaze[r][c];
            game->maze[r][c] = cell;
            if (cell == '.' || cell == 'o') {
                ++game->pellets;
            }
        }
        game->maze[r][kPacMazeCols] = '\0';
    }
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

void pac_game_init(pac_game_t* game, int16_t origin_x, int16_t origin_y) {
    if (game == NULL) {
        return;
    }
    game->grid.cols = (int16_t)kPacMazeCols;
    game->grid.rows = (int16_t)kPacMazeRows;
    game->grid.tile_px = (int16_t)kPacTilePx;
    game->grid.origin_x = origin_x;
    game->grid.origin_y = origin_y;
    /* Row 14 runs off both sides of the maze into the side tunnel. */
    game->grid.wrap_cols = 1u;
    game->grid.reserved = 0u;

    game->rng = 0x1234567u;
    game->frame = 0u;
    pac_game_reset(game);
}

void pac_game_reset(pac_game_t* game) {
    if (game == NULL) {
        return;
    }
    load_maze(game);
    spawn_all(game);
    game->state = PAC_STATE_PLAY;
    game->score = 0u;
    game->lives = PAC_START_LIVES;
    game->events = 0u;
}

/* Loses a life without clearing the maze: pellets already eaten stay eaten. */
static void lose_life(pac_game_t* game) {
    --game->lives;
    game->events |= PAC_EVENT_LIFE_LOST;
    if (game->lives <= 0) {
        game->state = PAC_STATE_LOSE;
        game->events |= PAC_EVENT_LOSE;
        return;
    }
    spawn_all(game);
}

/* ------------------------------------------------------------------ */
/* Per-frame simulation                                                */
/* ------------------------------------------------------------------ */

static void apply_input(pac_game_t* game, const sat_pad_state_t* pad) {
    if (pad == NULL) {
        return;
    }
    /* `want` is stored rather than applied: sat_grid_actor_step takes it at
     * the next tile centre, which is what lets a player pre-turn into a
     * corner slightly early and still make it. */
    if (pad->held & SAT_PAD_UP) {
        game->pac.want = SAT_DIR_UP;
    } else if (pad->held & SAT_PAD_DOWN) {
        game->pac.want = SAT_DIR_DOWN;
    } else if (pad->held & SAT_PAD_LEFT) {
        game->pac.want = SAT_DIR_LEFT;
    } else if (pad->held & SAT_PAD_RIGHT) {
        game->pac.want = SAT_DIR_RIGHT;
    }
}

static void eat_here(pac_game_t* game) {
    const int col = actor_col(game, &game->pac);
    const int row = actor_row(game, &game->pac);
    char cell;
    if (row < 0 || row >= kPacMazeRows || col < 0 || col >= kPacMazeCols) {
        return;
    }
    cell = game->maze[row][col];
    if (cell == '.') {
        game->maze[row][col] = ' ';
        game->score += PAC_SCORE_PELLET;
        --game->pellets;
        game->events |= PAC_EVENT_PELLET;
    } else if (cell == 'o') {
        game->maze[row][col] = ' ';
        game->score += PAC_SCORE_POWER;
        --game->pellets;
        game->fright = PAC_FRIGHT_FRAMES;
        game->events |= PAC_EVENT_POWER;
    }
}

static void step_pac(pac_game_t* game) {
    /* Eat before moving: at this instant Pac-Man is exactly on a tile centre
     * whenever he is on one at all, so the pellet under him is unambiguous. */
    if (sat_grid_at_tile_center(&game->grid, (int)game->pac.x, (int)game->pac.y)) {
        eat_here(game);
    }
    sat_grid_actor_step(&game->grid, &game->pac, pac_game_is_floor, game);
}

static void step_ghost(pac_game_t* game, int index) {
    pac_ghost_t* ghost = &game->ghosts[index];
    sat_grid_actor_t* a = &ghost->actor;
    int col;
    int row;

    if (ghost->release > 0u) {
        --ghost->release;
        return;
    }
    if (!sat_grid_at_tile_center(&game->grid, (int)a->x, (int)a->y)) {
        a->want = a->dir;
        sat_grid_actor_step(&game->grid, a, pac_game_is_floor, game);
        return;
    }

    col = actor_col(game, a);
    row = actor_row(game, a);

    if (game->fright > 0u) {
        /* Frightened ghosts wander, which is what makes a power pellet worth
         * eating: their paths stop being predictable. */
        a->dir = (int16_t)sat_grid_wander_dir(
            &game->grid, col, row, a->dir, rng_next(game), pac_game_is_floor, game);
    } else {
        int target_col;
        int target_row;
        if (((game->frame / PAC_MODE_PERIOD) & 1u) == 0u) {
            target_col = actor_col(game, &game->pac);
            target_row = actor_row(game, &game->pac);
        } else {
            target_col = (int)kScatterCol[index];
            target_row = (int)kScatterRow[index];
        }
        a->dir = (int16_t)sat_grid_chase_dir(
            &game->grid, col, row, a->dir, target_col, target_row,
            pac_game_is_floor, game);
    }

    /* The direction was just chosen for this tile, so it is already legal;
     * matching `want` keeps the step from second-guessing it. */
    a->want = a->dir;
    sat_grid_actor_step(&game->grid, a, pac_game_is_floor, game);
}

static int touching_pac(const pac_game_t* game, const sat_grid_actor_t* a) {
    const int32_t dx = a->x - game->pac.x;
    const int32_t dy = a->y - game->pac.y;
    return dx > -PAC_TOUCH_RANGE && dx < PAC_TOUCH_RANGE &&
           dy > -PAC_TOUCH_RANGE && dy < PAC_TOUCH_RANGE;
}

static void resolve_collisions(pac_game_t* game) {
    int i;
    for (i = 0; i < PAC_GHOST_COUNT; ++i) {
        pac_ghost_t* ghost = &game->ghosts[i];
        if (ghost->release > 0u || !touching_pac(game, &ghost->actor)) {
            continue;
        }
        if (game->fright > 0u) {
            game->score += PAC_SCORE_GHOST;
            game->events |= PAC_EVENT_GHOST_EATEN;
            /* Back to the pen, and back on the release timer, so eating one
             * buys real breathing room. */
            spawn_ghost(game, i);
        } else {
            lose_life(game);
            return;  /* everything moved; re-checking the rest is meaningless */
        }
    }
}

void pac_game_update(pac_game_t* game, const sat_pad_state_t* pad) {
    int i;
    if (game == NULL) {
        return;
    }
    game->events = 0u;

    if (pad != NULL && (pad->pressed & SAT_PAD_START)) {
        pac_game_reset(game);
        ++game->frame;
        return;
    }
    if (game->state != PAC_STATE_PLAY) {
        ++game->frame;
        return;
    }

    apply_input(game, pad);

    if (game->fright > 0u) {
        --game->fright;
    }

    step_pac(game);
    for (i = 0; i < PAC_GHOST_COUNT; ++i) {
        step_ghost(game, i);
    }
    resolve_collisions(game);

    if (game->state == PAC_STATE_PLAY && game->pellets <= 0) {
        game->state = PAC_STATE_WIN;
        game->events |= PAC_EVENT_WIN;
    }

    ++game->frame;
}
