/* test_pacman_game.cpp — host tests for the shared Pac-Man simulation.
 *
 * examples/common/pacman_game.c is the single simulation both the pacman_2d
 * and pacman_3d examples render, so proving it here proves the gameplay for
 * both without an emulator. In particular this covers the class of bug that
 * is invisible in a screenshot but fatal in play: an actor spawned inside a
 * wall still animates, it simply never goes anywhere.
 */

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>

#include "examples/common/pacman_game.h"
#include "saturn/collide2d.h"
#include "saturn/math3d.h"

#define TEST(name) static void name()
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    fprintf(stderr, "FAIL %s:%d: %s (%ld) != %s (%ld)\n", __FILE__, __LINE__, \
            #a, (long)(a), #b, (long)(b)); \
    exit(1); } } while(0)
#define ASSERT_TRUE(cond) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    exit(1); } } while(0)
#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

static const int16_t kOriginX = 48;
static const int16_t kOriginY = 12;

static sat_pad_state_t pad_held(uint16_t bits) {
    sat_pad_state_t p = {};
    p.held = bits;
    return p;
}

static sat_pad_state_t pad_pressed(uint16_t bits) {
    sat_pad_state_t p = {};
    p.held = bits;
    p.pressed = bits;
    return p;
}

static int actor_in_wall(const pac_game_t* g, const sat_grid_actor_t* a) {
    const int col = sat_grid_wrap_col(&g->grid, sat_grid_col_at(&g->grid, (int)a->x));
    const int row = sat_grid_row_at(&g->grid, (int)a->y);
    return pac_game_cell(g, col, row) == '#';
}

TEST(touch_range_is_open_six_pixels) {
    const sat_fx16_t h = sat_fx16_from_int(3);
    const sat_box2_t pac = {{0, 0}, {h, h}};
    sat_box2_t ghost = pac;
    ghost.center.x = sat_fx16_from_int(5);
    ASSERT_TRUE(sat_box2_overlap(&pac, &ghost));
    ghost.center.x = sat_fx16_from_int(6);
    ASSERT_FALSE(sat_box2_overlap(&pac, &ghost));
    ghost.center.x = 0;
    ghost.center.y = sat_fx16_from_int(5);
    ASSERT_TRUE(sat_box2_overlap(&pac, &ghost));
    ghost.center.y = sat_fx16_from_int(6);
    ASSERT_FALSE(sat_box2_overlap(&pac, &ghost));
}

/* ------------------------------------------------------------------ */

TEST(init_sets_up_a_playable_round) {
    pac_game_t g;
    pac_game_init(&g, kOriginX, kOriginY);

    ASSERT_EQ(g.state, PAC_STATE_PLAY);
    ASSERT_EQ(g.score, 0u);
    ASSERT_EQ(g.lives, PAC_START_LIVES);
    ASSERT_EQ(g.pellets, 182);  /* every pellet of the first stage, CLASSIC */
    ASSERT_EQ(g.grid.cols, (int16_t)kPacMazeCols);
    ASSERT_EQ(g.grid.rows, (int16_t)kPacMazeRows);
    ASSERT_EQ(g.grid.origin_x, kOriginX);
    ASSERT_TRUE(g.grid.wrap_cols != 0u);  /* row 14 is a side tunnel */
}

/* The original examples spawned Pac-Man and three of the four ghosts on wall
 * tiles, which is why nothing moved. */
TEST(nobody_spawns_inside_a_wall) {
    pac_game_t g;
    int i;
    pac_game_init(&g, kOriginX, kOriginY);

    ASSERT_FALSE(actor_in_wall(&g, &g.pac));
    for (i = 0; i < PAC_GHOST_COUNT; ++i) {
        ASSERT_FALSE(actor_in_wall(&g, &g.ghosts[i].actor));
    }
}

TEST(everyone_spawns_on_a_tile_center) {
    pac_game_t g;
    int i;
    pac_game_init(&g, kOriginX, kOriginY);

    ASSERT_TRUE(sat_grid_at_tile_center(&g.grid, (int)g.pac.x, (int)g.pac.y));
    for (i = 0; i < PAC_GHOST_COUNT; ++i) {
        ASSERT_TRUE(sat_grid_at_tile_center(
            &g.grid,
            (int)g.ghosts[i].actor.x,
            (int)g.ghosts[i].actor.y));
    }
}

TEST(pac_moves_when_the_pad_is_held) {
    pac_game_t g;
    const sat_pad_state_t left = pad_held(SAT_PAD_LEFT);
    int32_t start_x;
    int i;

    pac_game_init(&g, kOriginX, kOriginY);
    start_x = g.pac.x;
    for (i = 0; i < 8; ++i) {
        pac_game_update(&g, &left);
    }
    ASSERT_TRUE(g.pac.x < start_x);
    ASSERT_EQ(g.pac.y, sat_grid_tile_center_y(&g.grid, 20));
}

/* No matter how long the game runs, no actor may ever end a frame inside a
 * wall -- that would mean the collision rule failed somewhere. */
TEST(nobody_ever_enters_a_wall_over_a_long_run) {
    pac_game_t g;
    static const uint16_t kPattern[4] = {
        SAT_PAD_LEFT, SAT_PAD_UP, SAT_PAD_RIGHT, SAT_PAD_DOWN
    };
    int frame;

    pac_game_init(&g, kOriginX, kOriginY);
    for (frame = 0; frame < 4000; ++frame) {
        const sat_pad_state_t p = pad_held(kPattern[(frame / 37) & 3]);
        int i;
        pac_game_update(&g, &p);
        ASSERT_FALSE(actor_in_wall(&g, &g.pac));
        for (i = 0; i < PAC_GHOST_COUNT; ++i) {
            if (pac_game_ghost_penned(&g, i)) {
                continue;
            }
            ASSERT_FALSE(actor_in_wall(&g, &g.ghosts[i].actor));
        }
    }
}

TEST(eating_pellets_scores_and_depletes) {
    pac_game_t g;
    const sat_pad_state_t left = pad_held(SAT_PAD_LEFT);
    int frame;

    pac_game_init(&g, kOriginX, kOriginY);
    /* The spawn tile is itself a pellet and the whole row is pellets, so
     * simply walking left must score. */
    for (frame = 0; frame < 600; ++frame) {
        pac_game_update(&g, &left);
    }
    ASSERT_TRUE(g.score > 0u);
    ASSERT_TRUE(g.pellets < 182);
    /* Score and pellet count must stay consistent: only pellets add score at
     * 10 and power pellets at 50, so the score can never be a non-multiple. */
    ASSERT_EQ(g.score % 10u, 0u);
}

/* Releases are staggered so the four do not emerge as one clump.
 *
 * This deliberately stops before frame 97. Once ghosts could actually get out
 * of the pen (see every_ghost_actually_leaves_the_pen) a Pac-Man left standing
 * on his spawn tile gets caught at frame 97, and a death re-pens everyone with
 * fresh timers -- so running this to frame 200 would be measuring how long the
 * player survives, not the release stagger. */
TEST(ghosts_leave_the_pen_on_a_stagger) {
    pac_game_t g;
    const sat_pad_state_t idle = pad_held(0);
    int step;
    int i;

    pac_game_init(&g, kOriginX, kOriginY);
    for (i = 0; i < PAC_GHOST_COUNT; ++i) {
        ASSERT_EQ(pac_game_ghost_penned(&g, i), i == 0 ? 0 : 1);
    }

    /* One more ghost released per 45-frame step. */
    for (step = 1; step <= 2; ++step) {
        for (i = 0; i < 46; ++i) {
            pac_game_update(&g, &idle);
        }
        for (i = 0; i < PAC_GHOST_COUNT; ++i) {
            ASSERT_EQ(pac_game_ghost_penned(&g, i), (i <= step) ? 0 : 1);
        }
    }
}

/* Releases every ghost up front rather than waiting out the stagger. Waiting
 * is no longer reliable: ghosts that can reach Pac-Man kill an idle one, and
 * each death re-pens all four with fresh timers, so a fixed number of frames
 * no longer implies a released ghost. */
TEST(ghosts_actually_move_once_released) {
    pac_game_t g;
    const sat_pad_state_t idle = pad_held(0);
    int32_t before[PAC_GHOST_COUNT];
    int i;

    pac_game_init(&g, kOriginX, kOriginY);
    for (i = 0; i < PAC_GHOST_COUNT; ++i) {
        g.ghosts[i].release = 0u;
        ASSERT_FALSE(pac_game_ghost_penned(&g, i));
        before[i] = g.ghosts[i].actor.x + (g.ghosts[i].actor.y * 1000);
    }
    for (i = 0; i < 60; ++i) {
        /* Kept alive so a death does not re-pen them mid-measurement. */
        g.lives = 9;
        g.state = PAC_STATE_PLAY;
        pac_game_update(&g, &idle);
    }
    for (i = 0; i < PAC_GHOST_COUNT; ++i) {
        const int32_t after = g.ghosts[i].actor.x + (g.ghosts[i].actor.y * 1000);
        ASSERT_TRUE(after != before[i]);
    }
}

TEST(start_restarts_a_finished_game) {
    pac_game_t g;
    const sat_pad_state_t start = pad_pressed(SAT_PAD_START);
    const sat_pad_state_t idle = pad_held(0);

    pac_game_init(&g, kOriginX, kOriginY);
    g.state = PAC_STATE_LOSE;
    g.score = 999u;
    g.lives = 0;
    g.pellets = 3;

    pac_game_update(&g, &idle);
    ASSERT_EQ(g.state, PAC_STATE_LOSE);  /* stays finished on its own */

    pac_game_update(&g, &start);
    ASSERT_EQ(g.state, PAC_STATE_PLAY);
    ASSERT_EQ(g.score, 0u);
    ASSERT_EQ(g.lives, PAC_START_LIVES);
    ASSERT_EQ(g.pellets, 182);
    ASSERT_FALSE(actor_in_wall(&g, &g.pac));
}

static void eat_everything(pac_game_t* g) {
    int r;
    int c;
    for (r = 0; r < kPacMazeRows; ++r) {
        for (c = 0; c < kPacMazeCols; ++c) {
            if (g->maze[r][c] == '.' || g->maze[r][c] == 'o') {
                g->maze[r][c] = ' ';
            }
        }
    }
    g->pellets = 0;
}

/* Clearing a stage that is not the last pauses on the cleared maze, then
 * loads the next one with score and lives carried over. */
TEST(clearing_a_stage_moves_on_to_the_next) {
    pac_game_t g;
    const sat_pad_state_t idle = pad_held(0);
    uint32_t i;

    ASSERT_TRUE(pac_stage_count() >= 2u);
    pac_game_init(&g, kOriginX, kOriginY);
    eat_everything(&g);
    g.score = 1234u;
    g.lives = 2;
    pac_game_update(&g, &idle);
    ASSERT_EQ(g.state, PAC_STATE_STAGE_CLEAR);
    ASSERT_TRUE((g.events & PAC_EVENT_STAGE_CLEAR) != 0u);
    ASSERT_EQ(g.stage, 0u);

    for (i = 0; i < PAC_STAGE_CLEAR_FRAMES; ++i) {
        pac_game_update(&g, &idle);
        ASSERT_EQ(g.state, PAC_STATE_STAGE_CLEAR);
    }
    pac_game_update(&g, &idle);
    ASSERT_EQ(g.state, PAC_STATE_PLAY);
    ASSERT_TRUE((g.events & PAC_EVENT_STAGE_START) != 0u);
    ASSERT_EQ(g.stage, 1u);
    ASSERT_EQ(g.score, 1234u);
    ASSERT_EQ(g.lives, 2);
    ASSERT_TRUE(g.pellets > 0);
    ASSERT_FALSE(actor_in_wall(&g, &g.pac));
}

TEST(clearing_the_last_stage_wins) {
    pac_game_t g;
    const sat_pad_state_t idle = pad_held(0);

    pac_game_init(&g, kOriginX, kOriginY);
    pac_game_load_stage(&g, (uint16_t)(pac_stage_count() - 1u));
    eat_everything(&g);
    pac_game_update(&g, &idle);
    ASSERT_EQ(g.state, PAC_STATE_WIN);
    ASSERT_TRUE((g.events & PAC_EVENT_WIN) != 0u);
}

/* START after reaching a later stage goes back to the first. */
TEST(restart_returns_to_the_first_stage) {
    pac_game_t g;
    const sat_pad_state_t start = pad_pressed(SAT_PAD_START);

    pac_game_init(&g, kOriginX, kOriginY);
    pac_game_load_stage(&g, 1u);
    ASSERT_EQ(g.stage, 1u);
    pac_game_update(&g, &start);
    ASSERT_EQ(g.stage, 0u);
    ASSERT_EQ(g.pellets, 182);
    ASSERT_TRUE((g.events & PAC_EVENT_STAGE_START) != 0u);
}

/* Every stage in pacman_stages.c is loaded and checked here, so a broken
 * layout fails the tests rather than the first person to reach it. */
TEST(every_stage_is_playable) {
    uint16_t s;
    for (s = 0; s < pac_stage_count(); ++s) {
        pac_game_t g;
        int i;
        pac_game_init(&g, kOriginX, kOriginY);
        pac_game_load_stage(&g, s);
        ASSERT_EQ(g.level_error, PAC_LEVEL_OK);
        ASSERT_EQ(g.state, PAC_STATE_PLAY);
        ASSERT_TRUE(g.pellets > 0);
        ASSERT_FALSE(actor_in_wall(&g, &g.pac));
        for (i = 0; i < PAC_GHOST_COUNT; ++i) {
            ASSERT_FALSE(actor_in_wall(&g, &g.ghosts[i].actor));
        }
    }
}

/* Every ghost gets out of the pen on every stage, not just the classic one:
 * the pen and its door are read from the layout now. */
TEST(ghosts_leave_the_pen_on_every_stage) {
    uint16_t s;
    for (s = 0; s < pac_stage_count(); ++s) {
        pac_game_t g;
        const sat_pad_state_t left = pad_held(SAT_PAD_LEFT);
        int left_pen[PAC_GHOST_COUNT] = {0, 0, 0, 0};
        int i;
        uint32_t f;

        pac_game_init(&g, kOriginX, kOriginY);
        pac_game_load_stage(&g, s);
        for (f = 0; f < 600u; ++f) {
            g.lives = 9;
            g.fright = 0u;
            pac_game_update(&g, &left);
            for (i = 0; i < PAC_GHOST_COUNT; ++i) {
                const int col = ((int)g.ghosts[i].actor.x - kOriginX) / kPacTilePx;
                const int row = ((int)g.ghosts[i].actor.y - kOriginY) / kPacTilePx;
                if (!pac_game_ghost_penned(&g, i) && !pac_level_in_pen(&g.level, col, row)) {
                    left_pen[i] = 1;
                }
            }
        }
        for (i = 0; i < PAC_GHOST_COUNT; ++i) {
            ASSERT_TRUE(left_pen[i]);
        }
    }
}

/* The loader rejects what would otherwise be a silently broken stage. */
TEST(broken_layouts_are_rejected) {
    static char maze[kPacMazeRows][kPacMazeCols + 1];
    pac_stage_t stage = *pac_stage_get(0u);
    pac_level_t level;
    char row[kPacMazeCols + 1];
    int i;

    ASSERT_EQ(pac_level_load(&stage, maze, &level), PAC_LEVEL_OK);

    /* No Pac-Man. */
    for (i = 0; i <= kPacMazeCols; ++i) {
        row[i] = stage.rows[20][i];
    }
    row[12] = '.';
    stage.rows[20] = row;
    ASSERT_EQ(pac_level_load(&stage, maze, &level), PAC_LEVEL_PAC_SPAWN);

    /* A hole in the pen's side wall, on the tunnel row, leaks the pen. */
    stage = *pac_stage_get(0u);
    for (i = 0; i <= kPacMazeCols; ++i) {
        row[i] = stage.rows[14][i];
    }
    row[10] = ' ';
    stage.rows[14] = row;
    ASSERT_EQ(pac_level_load(&stage, maze, &level), PAC_LEVEL_PEN_LEAKS);

    /* A short row. */
    stage = *pac_stage_get(0u);
    stage.rows[3] = "#o##";
    ASSERT_EQ(pac_level_load(&stage, maze, &level), PAC_LEVEL_BAD_SHAPE);
}

TEST(facing_starts_at_the_spawn_heading) {
    pac_game_t g;
    int i;

    pac_game_init(&g, kOriginX, kOriginY);
    ASSERT_EQ(pac_game_facing(&g, PAC_SLOT_PAC), SAT_DIR_LEFT);
    for (i = 0; i < PAC_GHOST_COUNT; ++i) {
        ASSERT_EQ(pac_game_facing(&g, i), (i & 1) ? SAT_DIR_LEFT : SAT_DIR_RIGHT);
    }
}

/* The bug this pins: sat_grid_actor_t::dir goes to SAT_DIR_NONE the instant
 * an actor stops square against a wall, and a renderer reading that raw
 * field directly snaps its sprite/model to a default facing for that one
 * frame. pac_game_facing must keep reporting the last real heading instead. */
TEST(facing_holds_through_a_stop_against_a_wall) {
    pac_game_t g;
    const sat_pad_state_t idle = pad_held(0);

    pac_game_init(&g, kOriginX, kOriginY);
    /* Column 0 of row 1 is maze boundary wall; Pac-Man at (1,1) heading LEFT
     * has nowhere to go. Seed facing with a direction the spawn default
     * (LEFT) does not share, so a value that survives the update proves the
     * hold rather than coincidentally matching the untouched default. */
    g.pac.x = sat_grid_tile_center_x(&g.grid, 1);
    g.pac.y = sat_grid_tile_center_y(&g.grid, 1);
    g.pac.dir = SAT_DIR_LEFT;
    g.pac.want = SAT_DIR_LEFT;
    g.facing[PAC_SLOT_PAC] = SAT_DIR_UP;
    ASSERT_EQ(pac_game_cell(&g, 0, 1), '#');

    pac_game_update(&g, &idle);

    ASSERT_EQ(g.pac.dir, SAT_DIR_NONE);                       /* actor stopped */
    ASSERT_EQ(pac_game_facing(&g, PAC_SLOT_PAC), SAT_DIR_UP); /* facing held */
}

TEST(facing_rejects_bad_slots) {
    pac_game_t g;
    pac_game_init(&g, kOriginX, kOriginY);
    ASSERT_EQ(pac_game_facing(NULL, PAC_SLOT_PAC), SAT_DIR_NONE);
    ASSERT_EQ(pac_game_facing(&g, -1), SAT_DIR_NONE);
    ASSERT_EQ(pac_game_facing(&g, PAC_GHOST_COUNT + 1), SAT_DIR_NONE);
}

TEST(status_text_matches_state) {
    pac_game_t g;
    pac_game_init(&g, kOriginX, kOriginY);

    ASSERT_TRUE(pac_game_status_text(&g) == NULL);
    ASSERT_FALSE(pac_game_status_needs_start(&g));

    g.state = PAC_STATE_STAGE_CLEAR;
    ASSERT_EQ(strcmp(pac_game_status_text(&g), "STAGE CLEAR"), 0);
    ASSERT_FALSE(pac_game_status_needs_start(&g)); /* resolves on its own */

    g.state = PAC_STATE_WIN;
    ASSERT_EQ(strcmp(pac_game_status_text(&g), "YOU WIN"), 0);
    ASSERT_TRUE(pac_game_status_needs_start(&g));

    g.state = PAC_STATE_LOSE;
    ASSERT_EQ(strcmp(pac_game_status_text(&g), "GAME OVER"), 0);
    ASSERT_TRUE(pac_game_status_needs_start(&g));

    /* A broken stage layout overrides whatever state carried it there, and
     * still asks for START -- restarting returns to stage 0. */
    g.state = PAC_STATE_PLAY;
    g.level_error = PAC_LEVEL_PAC_SPAWN;
    ASSERT_EQ(strcmp(pac_game_status_text(&g), "BROKEN STAGE LAYOUT"), 0);
    ASSERT_TRUE(pac_game_status_needs_start(&g));

    ASSERT_TRUE(pac_game_status_text(NULL) == NULL);
    ASSERT_FALSE(pac_game_status_needs_start(NULL));
}

TEST(a_power_pellet_frightens_and_expires) {
    pac_game_t g;
    const sat_pad_state_t idle = pad_held(0);
    uint32_t i;

    pac_game_init(&g, kOriginX, kOriginY);
    ASSERT_FALSE(pac_game_frightened(&g));

    /* Drop Pac-Man onto the power pellet at (1,3) and step once. */
    g.pac.x = sat_grid_tile_center_x(&g.grid, 1);
    g.pac.y = sat_grid_tile_center_y(&g.grid, 3);
    g.pac.dir = SAT_DIR_NONE;
    g.pac.want = SAT_DIR_NONE;
    ASSERT_EQ(pac_game_cell(&g, 1, 3), 'o');

    pac_game_update(&g, &idle);
    ASSERT_TRUE(pac_game_frightened(&g));
    ASSERT_TRUE((g.events & PAC_EVENT_POWER) != 0u);
    ASSERT_EQ(g.score, 50u);
    ASSERT_EQ(pac_game_cell(&g, 1, 3), ' ');

    for (i = 0; i < PAC_FRIGHT_FRAMES + 4u; ++i) {
        pac_game_update(&g, &idle);
    }
    ASSERT_FALSE(pac_game_frightened(&g));
}

/* Two renderers reading the same simulation must see identical state, or the
 * 2D and 3D examples would drift apart. The only thing that may differ is the
 * pixel origin, so the same inputs must produce the same tile sequence. */
TEST(origin_only_shifts_positions_not_behaviour) {
    pac_game_t a;
    pac_game_t b;
    const sat_pad_state_t left = pad_held(SAT_PAD_LEFT);
    int frame;

    pac_game_init(&a, kOriginX, kOriginY);  /* 2D: centred on screen */
    pac_game_init(&b, 0, 0);                /* 3D: world origin */

    for (frame = 0; frame < 500; ++frame) {
        pac_game_update(&a, &left);
        pac_game_update(&b, &left);

        ASSERT_EQ(a.pac.x - kOriginX, b.pac.x);
        ASSERT_EQ(a.pac.y - kOriginY, b.pac.y);
        ASSERT_EQ(a.score, b.score);
        ASSERT_EQ(a.pellets, b.pellets);
        ASSERT_EQ(a.lives, b.lives);
        ASSERT_EQ(a.state, b.state);
    }
}

TEST(null_game_pointer_is_safe) {
    const sat_pad_state_t idle = pad_held(0);
    pac_game_init(NULL, 0, 0);
    pac_game_reset(NULL);
    pac_game_update(NULL, &idle);
    ASSERT_EQ(pac_game_cell(NULL, 0, 0), '#');
    ASSERT_EQ(pac_game_frightened(NULL), 0);
    ASSERT_EQ(pac_game_ghost_penned(NULL, 0), 0);
    ASSERT_EQ(pac_game_is_floor(0, 0, NULL), 0);
}

TEST(out_of_range_cells_read_as_wall) {
    pac_game_t g;
    pac_game_init(&g, kOriginX, kOriginY);
    ASSERT_EQ(pac_game_cell(&g, -1, 0), '#');
    ASSERT_EQ(pac_game_cell(&g, 0, -1), '#');
    ASSERT_EQ(pac_game_cell(&g, kPacMazeCols, 0), '#');
    ASSERT_EQ(pac_game_cell(&g, 0, kPacMazeRows), '#');
    ASSERT_EQ(pac_game_ghost_penned(&g, PAC_GHOST_COUNT), 0);
    ASSERT_EQ(pac_game_ghost_penned(&g, -1), 0);
}


/* The bug this pins: the ordinary chase rule steers towards whichever legal
 * move lands nearest Pac-Man, and Pac-Man is almost always below the pen --
 * so a ghost inside the pen walks into its own south wall and stays there
 * forever. Before the fix, ghosts 1, 2 and 3 spent 100% of their released
 * frames in the pen and never appeared in the game at all. */
TEST(every_ghost_actually_leaves_the_pen) {
    pac_game_t g;
    const sat_pad_state_t left = pad_held(SAT_PAD_LEFT);
    long released[PAC_GHOST_COUNT] = {0, 0, 0, 0};
    long in_pen[PAC_GHOST_COUNT] = {0, 0, 0, 0};
    int i;
    uint32_t f;

    pac_game_init(&g, kOriginX, kOriginY);
    for (f = 0; f < 3000u; ++f) {
        pac_game_update(&g, &left);
        for (i = 0; i < PAC_GHOST_COUNT; ++i) {
            int col;
            int row;
            if (pac_game_ghost_penned(&g, i)) {
                continue;
            }
            ++released[i];
            col = ((int)g.ghosts[i].actor.x - kOriginX) / kPacTilePx;
            row = ((int)g.ghosts[i].actor.y - kOriginY) / kPacTilePx;
            if (col >= 11 && col <= 16 && row >= 12 && row <= 15) {
                ++in_pen[i];
            }
        }
    }

    for (i = 0; i < PAC_GHOST_COUNT; ++i) {
        ASSERT_TRUE(released[i] > 1000);
        /* Leaving, and re-entering after being eaten, is a small fraction of
         * a run. Being trapped is 100%. */
        ASSERT_TRUE(in_pen[i] * 5 < released[i]);
    }
}

/* Four ghosts aiming at the same tile make the same decision at every
 * junction, so any two that meet travel as one for the rest of the game.
 * Starting them all on one tile is the scenario that shows it: with a single
 * shared target they stay stacked about 11% of the run, and with a target
 * each about 5%. */
TEST(ghosts_that_meet_separate_again) {
    pac_game_t g;
    const int kCol = 13;
    const int kRow = 11;
    long stacked = 0;
    long pairs = 0;
    int i;
    int j;
    uint32_t f;

    pac_game_init(&g, kOriginX, kOriginY);
    for (i = 0; i < PAC_GHOST_COUNT; ++i) {
        g.ghosts[i].release = 0u;
        g.ghosts[i].actor.x = (int16_t)(kOriginX + (kCol * kPacTilePx) + (kPacTilePx / 2));
        g.ghosts[i].actor.y = (int16_t)(kOriginY + (kRow * kPacTilePx) + (kPacTilePx / 2));
        g.ghosts[i].actor.dir = SAT_DIR_LEFT;
        g.ghosts[i].actor.want = SAT_DIR_LEFT;
    }

    for (f = 0; f < 1200u; ++f) {
        const sat_pad_state_t pad = pad_held(((f / 70u) & 1u) ? SAT_PAD_UP : SAT_PAD_LEFT);
        /* Held inside the CHASE window -- frame 0 is scatter, and scatter
         * already gives each ghost its own corner -- while still advancing,
         * because the frame counter also drives the per-ghost slowdown
         * stagger: freezing it at a constant would stop one ghost outright.
         * 240 is PAC_MODE_PERIOD, which pacman_game.c keeps private. */
        g.frame = 240u + (f % 240u);
        g.fright = 0u;
        g.lives = 9;
        g.state = PAC_STATE_PLAY;
        pac_game_update(&g, &pad);

        for (i = 0; i < PAC_GHOST_COUNT; ++i) {
            for (j = i + 1; j < PAC_GHOST_COUNT; ++j) {
                const int32_t dx = g.ghosts[i].actor.x - g.ghosts[j].actor.x;
                const int32_t dy = g.ghosts[i].actor.y - g.ghosts[j].actor.y;
                ++pairs;
                if ((dx * dx) + (dy * dy) < (kPacTilePx * kPacTilePx)) {
                    ++stacked;
                }
            }
        }
    }
    ASSERT_TRUE(pairs > 0);
    ASSERT_TRUE((stacked * 100) / pairs < 8);
}


/* Ghosts have to be slower than Pac-Man or the game is not winnable: four
 * pursuers at the player's exact speed leave a corridor with no way out.
 * Positions are whole pixels, so the slowdown is a dropped step rather than a
 * fractional speed, and this pins both that it happens and that it stays
 * small. */
TEST(ghosts_move_slower_than_pac_man) {
    pac_game_t g;
    const sat_pad_state_t idle = pad_held(0);
    int moved[PAC_GHOST_COUNT] = {0, 0, 0, 0};
    int live[PAC_GHOST_COUNT] = {0, 0, 0, 0};
    int32_t prev_x[PAC_GHOST_COUNT];
    int32_t prev_y[PAC_GHOST_COUNT];
    int i;
    int f;

    pac_game_init(&g, kOriginX, kOriginY);
    for (i = 0; i < PAC_GHOST_COUNT; ++i) {
        g.ghosts[i].release = 0u;
        prev_x[i] = g.ghosts[i].actor.x;
        prev_y[i] = g.ghosts[i].actor.y;
    }
    for (f = 0; f < 600; ++f) {
        g.lives = 9;
        g.state = PAC_STATE_PLAY;
        g.fright = 0u;
        pac_game_update(&g, &idle);
        for (i = 0; i < PAC_GHOST_COUNT; ++i) {
            /* Catching Pac-Man re-pens everyone, and a penned ghost does not
             * move at all -- which would be counted as slowness that is not
             * the slowness under test. */
            if (!pac_game_ghost_penned(&g, i)) {
                ++live[i];
                if (g.ghosts[i].actor.x != prev_x[i] || g.ghosts[i].actor.y != prev_y[i]) {
                    ++moved[i];
                }
            }
            prev_x[i] = g.ghosts[i].actor.x;
            prev_y[i] = g.ghosts[i].actor.y;
        }
    }
    for (i = 0; i < PAC_GHOST_COUNT; ++i) {
        ASSERT_TRUE(live[i] > 200);
        ASSERT_TRUE(moved[i] < live[i]);              /* slower than Pac-Man */
        ASSERT_TRUE(moved[i] * 4 > live[i] * 3);      /* but only a little   */
    }
}

int main() {
    touch_range_is_open_six_pixels();
    init_sets_up_a_playable_round();
    nobody_spawns_inside_a_wall();
    everyone_spawns_on_a_tile_center();
    pac_moves_when_the_pad_is_held();
    nobody_ever_enters_a_wall_over_a_long_run();
    eating_pellets_scores_and_depletes();
    ghosts_leave_the_pen_on_a_stagger();
    ghosts_actually_move_once_released();
    start_restarts_a_finished_game();
    clearing_a_stage_moves_on_to_the_next();
    clearing_the_last_stage_wins();
    restart_returns_to_the_first_stage();
    every_stage_is_playable();
    ghosts_leave_the_pen_on_every_stage();
    broken_layouts_are_rejected();
    facing_starts_at_the_spawn_heading();
    facing_holds_through_a_stop_against_a_wall();
    facing_rejects_bad_slots();
    status_text_matches_state();
    a_power_pellet_frightens_and_expires();
    origin_only_shifts_positions_not_behaviour();
    null_game_pointer_is_safe();
    out_of_range_cells_read_as_wall();
    every_ghost_actually_leaves_the_pen();
    ghosts_that_meet_separate_again();
    ghosts_move_slower_than_pac_man();

    printf("PASS: test_pacman_game.cpp (%d tests)\n", 27);
    return 0;
}
