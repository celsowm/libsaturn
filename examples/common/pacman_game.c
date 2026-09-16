/* Shared Pac-Man simulation. See pacman_game.h for the contract. */

#define PACMAN_MAZE_IMPL
#include "pacman_game.h"
#include "saturn/collide2d.h"
#include "saturn/math3d.h"

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

/* The pen interior, and the corridor tile just outside its door.
 *
 * A ghost inside the pen cannot be steered by the ordinary chase rule. That
 * rule picks whichever legal move lands nearest the target, and the target is
 * Pac-Man, who is almost always BELOW the pen -- so a ghost in the pen walks
 * into its own south wall and stays there. The symptom is subtle enough to
 * miss: the ghosts animate, the game runs, and three of the four simply never
 * appear. (Measured before this fix: ghosts 1, 2 and 3 spent 100% of their
 * released frames inside the pen; only ghost 0 ever escaped, and only because
 * it starts facing the door.)
 *
 * So a penned ghost aims at the tile outside the door instead, until it is
 * out. This is what the arcade game does too. */
#define PAC_PEN_COL_MIN 11
#define PAC_PEN_COL_MAX 16
#define PAC_PEN_ROW_MIN 12
#define PAC_PEN_ROW_MAX 15
#define PAC_DOOR_COL_LEFT 13
#define PAC_DOOR_COL_RIGHT 14
#define PAC_DOOR_EXIT_ROW 11

/* Scatter corners, one per ghost, so they spread out instead of stacking up
 * on the same tile whenever they are not chasing. */
static const uint8_t kScatterCol[PAC_GHOST_COUNT] = {26, 1, 26, 1};
static const uint8_t kScatterRow[PAC_GHOST_COUNT] = {1, 1, 23, 23};

/* Ghosts alternate scatter and chase on this period, and leave the pen
 * staggered so they do not emerge as a single clump. */
#define PAC_MODE_PERIOD 240u
#define PAC_RELEASE_STEP 45u

/* Ghosts skip one step in every PAC_GHOST_SLOW_PERIOD frames.
 *
 * sat_grid_actor_t moves a whole number of pixels per frame, so a ghost
 * cannot simply be given 1.7 pixels; dropping a step is how a slower speed is
 * expressed on an integer grid. Some margin is needed: with four ghosts at
 * exactly Pac-Man's speed, converging on his tile, a corridor has no escape
 * and the game is not winnable. The arcade slows its ghosts for the same
 * reason.
 *
 * One frame in eight puts them at 87.5% of Pac-Man's speed, which is about
 * where the arcade has them on its first level. It is a deliberate choice
 * rather than a tuned one: a bot good enough to measure "playable" against is
 * a bigger piece of work than the game it would be measuring. */
#define PAC_GHOST_SLOW_PERIOD 8u

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

/* Where ghost `index` is heading while chasing.
 *
 * All four aiming at Pac-Man's own tile is what makes them clump: the chase
 * rule is deterministic, so ghosts that share a tile share a decision and
 * then travel as one blob for the rest of the game. Giving each a different
 * target is what separates them, and it is also the whole of Pac-Man's
 * character -- one pursues, one cuts ahead, one flanks, one loses its nerve.
 *
 * Targets are allowed to fall outside the maze. sat_grid_chase_dir only
 * scores squared distance, so an unreachable target simply biases movement in
 * its direction, which is exactly the intent. */
/* True while the ghost is still inside the pen box. */
static int inside_pen(int col, int row) {
    return col >= PAC_PEN_COL_MIN && col <= PAC_PEN_COL_MAX &&
           row >= PAC_PEN_ROW_MIN && row <= PAC_PEN_ROW_MAX;
}

static void chase_target(const pac_game_t* game, int index, int* out_col, int* out_row) {
    const int pac_col = actor_col(game, &game->pac);
    const int pac_row = actor_row(game, &game->pac);
    const int dx = sat_dir_dx((int)game->pac.dir);
    const int dy = sat_dir_dy((int)game->pac.dir);

    switch (index) {
    case 1:
        /* Four tiles in front of Pac-Man: arrives where he is going. */
        *out_col = pac_col + (dx * 4);
        *out_row = pac_row + (dy * 4);
        break;
    case 2: {
        /* The point two ahead of Pac-Man, reflected through ghost 0. That
         * keeps it roughly opposite its partner, so the two of them close
         * from both sides instead of following each other. */
        const int ahead_col = pac_col + (dx * 2);
        const int ahead_row = pac_row + (dy * 2);
        const int lead_col = actor_col(game, &game->ghosts[0].actor);
        const int lead_row = actor_row(game, &game->ghosts[0].actor);
        *out_col = ahead_col + (ahead_col - lead_col);
        *out_row = ahead_row + (ahead_row - lead_row);
        break;
    }
    case 3: {
        /* Chases from a distance and breaks off when it gets close, which
         * leaves one corner of the maze survivable. */
        const int gap_col = pac_col - actor_col(game, &game->ghosts[3].actor);
        const int gap_row = pac_row - actor_row(game, &game->ghosts[3].actor);
        if ((gap_col * gap_col) + (gap_row * gap_row) > (8 * 8)) {
            *out_col = pac_col;
            *out_row = pac_row;
        } else {
            *out_col = (int)kScatterCol[3];
            *out_row = (int)kScatterRow[3];
        }
        break;
    }
    default:
        *out_col = pac_col;
        *out_row = pac_row;
        break;
    }
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
    /* Staggered by ghost, not global: dropping the same frame for all four
     * keeps them in lockstep, which puts them back to making identical moves
     * from identical tiles -- the clumping this is meant to avoid. */
    if (((game->frame + (uint32_t)index) % PAC_GHOST_SLOW_PERIOD) == 0u) {
        return;
    }
    if (!sat_grid_at_tile_center(&game->grid, (int)a->x, (int)a->y)) {
        a->want = a->dir;
        sat_grid_actor_step(&game->grid, a, pac_game_is_floor, game);
        return;
    }

    col = actor_col(game, a);
    row = actor_row(game, a);

    if (inside_pen(col, row)) {
        /* Getting out comes before scattering, chasing or fleeing: a ghost
         * that wanders inside the pen is a ghost that is not in the game. */
        const int door_col =
            (col <= PAC_DOOR_COL_LEFT) ? PAC_DOOR_COL_LEFT : PAC_DOOR_COL_RIGHT;
        a->dir = (int16_t)sat_grid_chase_dir(
            &game->grid, col, row, a->dir, door_col, PAC_DOOR_EXIT_ROW,
            pac_game_is_floor, game);
        a->want = a->dir;
        sat_grid_actor_step(&game->grid, a, pac_game_is_floor, game);
        return;
    }

    if (game->fright > 0u) {
        /* Frightened ghosts wander, which is what makes a power pellet worth
         * eating: their paths stop being predictable. */
        a->dir = (int16_t)sat_grid_wander_dir(
            &game->grid, col, row, a->dir, rng_next(game), pac_game_is_floor, game);
    } else {
        int target_col;
        int target_row;
        /* The round OPENS on scatter, not chase. Four ghosts beelining at a
         * Pac-Man who has not moved yet is how a round ends before it starts;
         * the arcade sends them to their corners first for the same reason.
         * The same applies after a death, since the frame counter is what
         * drives the alternation and a death does not reset it. */
        if (((game->frame / PAC_MODE_PERIOD) & 1u) != 0u) {
            chase_target(game, index, &target_col, &target_row);
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
    const sat_fx16_t half = sat_fx16_from_int(PAC_TOUCH_RANGE / 2);
    const sat_box2_t pac = {{sat_fx16_from_int(game->pac.x), sat_fx16_from_int(game->pac.y)}, {half, half}};
    const sat_box2_t actor = {{sat_fx16_from_int(a->x), sat_fx16_from_int(a->y)}, {half, half}};
    return sat_box2_overlap(&pac, &actor);
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
