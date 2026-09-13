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

#include "examples/common/pacman_game.h"

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

/* ------------------------------------------------------------------ */

TEST(init_sets_up_a_playable_round) {
    pac_game_t g;
    pac_game_init(&g, kOriginX, kOriginY);

    ASSERT_EQ(g.state, PAC_STATE_PLAY);
    ASSERT_EQ(g.score, 0u);
    ASSERT_EQ(g.lives, PAC_START_LIVES);
    ASSERT_EQ(g.pellets, 182);  /* every '.' and 'o' in kPacMaze */
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

TEST(ghosts_leave_the_pen_on_a_stagger) {
    pac_game_t g;
    const sat_pad_state_t idle = pad_held(0);
    int i;

    pac_game_init(&g, kOriginX, kOriginY);
    for (i = 0; i < PAC_GHOST_COUNT; ++i) {
        ASSERT_EQ(pac_game_ghost_penned(&g, i), i == 0 ? 0 : 1);
    }
    for (i = 0; i < 200; ++i) {
        pac_game_update(&g, &idle);
    }
    for (i = 0; i < PAC_GHOST_COUNT; ++i) {
        ASSERT_FALSE(pac_game_ghost_penned(&g, i));
    }
}

TEST(ghosts_actually_move_once_released) {
    pac_game_t g;
    const sat_pad_state_t idle = pad_held(0);
    int32_t before[PAC_GHOST_COUNT];
    int i;

    pac_game_init(&g, kOriginX, kOriginY);
    for (i = 0; i < 200; ++i) {
        pac_game_update(&g, &idle);
    }
    for (i = 0; i < PAC_GHOST_COUNT; ++i) {
        before[i] = g.ghosts[i].actor.x + (g.ghosts[i].actor.y * 1000);
    }
    for (i = 0; i < 60; ++i) {
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

TEST(clearing_every_pellet_wins) {
    pac_game_t g;
    const sat_pad_state_t idle = pad_held(0);
    int r;
    int c;

    pac_game_init(&g, kOriginX, kOriginY);
    for (r = 0; r < kPacMazeRows; ++r) {
        for (c = 0; c < kPacMazeCols; ++c) {
            if (g.maze[r][c] == '.' || g.maze[r][c] == 'o') {
                g.maze[r][c] = ' ';
            }
        }
    }
    g.pellets = 0;
    pac_game_update(&g, &idle);
    ASSERT_EQ(g.state, PAC_STATE_WIN);
    ASSERT_TRUE((g.events & PAC_EVENT_WIN) != 0u);
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

int main() {
    init_sets_up_a_playable_round();
    nobody_spawns_inside_a_wall();
    everyone_spawns_on_a_tile_center();
    pac_moves_when_the_pad_is_held();
    nobody_ever_enters_a_wall_over_a_long_run();
    eating_pellets_scores_and_depletes();
    ghosts_leave_the_pen_on_a_stagger();
    ghosts_actually_move_once_released();
    start_restarts_a_finished_game();
    clearing_every_pellet_wins();
    a_power_pellet_frightens_and_expires();
    origin_only_shifts_positions_not_behaviour();
    null_game_pointer_is_safe();
    out_of_range_cells_read_as_wall();

    printf("PASS: test_pacman_game.cpp (%d tests)\n", 14);
    return 0;
}
