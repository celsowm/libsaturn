/* test_grid_logic.cpp — host tests for the tile-grid movement module */

#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "saturn/grid.h"
#include "src/core/grid_logic.hpp"

#define TEST(name) static void name()
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    fprintf(stderr, "FAIL %s:%d: %s (%ld) != %s (%ld)\n", __FILE__, __LINE__, \
            #a, (long)(a), #b, (long)(b)); \
    exit(1); } } while(0)
#define ASSERT_TRUE(cond) do { if (!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    exit(1); } } while(0)
#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

using namespace saturn::core::grid;

/* 8 x 5 fixture. Row 2 is a tunnel row open all the way across, so wrapping
 * and floor-division both get exercised. */
static const char* kMap[5] = {
    "########",
    "#......#",
    "........",
    "#.###..#",
    "########",
};

static int map_is_floor(int col, int row, void* user) {
    (void)user;
    if (row < 0 || row >= 5 || col < 0 || col >= 8) {
        return 0;
    }
    return kMap[row][col] != '#' ? 1 : 0;
}

static sat_grid_t make_grid(uint8_t wrap) {
    sat_grid_t g = {};
    g.cols = 8;
    g.rows = 5;
    g.tile_px = 8;
    g.origin_x = 0;
    g.origin_y = 0;
    g.wrap_cols = wrap;
    return g;
}

TEST(dir_helpers) {
    ASSERT_EQ(dir_dx(SAT_DIR_RIGHT), 1);
    ASSERT_EQ(dir_dx(SAT_DIR_LEFT), -1);
    ASSERT_EQ(dir_dx(SAT_DIR_UP), 0);
    ASSERT_EQ(dir_dy(SAT_DIR_DOWN), 1);
    ASSERT_EQ(dir_dy(SAT_DIR_UP), -1);
    ASSERT_EQ(dir_dx(SAT_DIR_NONE), 0);
    ASSERT_EQ(dir_dy(SAT_DIR_NONE), 0);
    ASSERT_EQ(dir_opposite(SAT_DIR_UP), SAT_DIR_DOWN);
    ASSERT_EQ(dir_opposite(SAT_DIR_LEFT), SAT_DIR_RIGHT);
    ASSERT_EQ(dir_opposite(SAT_DIR_NONE), SAT_DIR_NONE);
}

/* The bug this guards: C division truncates toward zero, so px = -1 and px = 0
 * would both land in column 0 and an actor in the left tunnel mouth would
 * probe the wrong tile. */
TEST(col_at_uses_floor_division) {
    const sat_grid_t g = make_grid(1);
    ASSERT_EQ(col_at(&g, 0), 0);
    ASSERT_EQ(col_at(&g, 7), 0);
    ASSERT_EQ(col_at(&g, 8), 1);
    ASSERT_EQ(col_at(&g, -1), -1);
    ASSERT_EQ(col_at(&g, -8), -1);
    ASSERT_EQ(col_at(&g, -9), -2);
}

TEST(col_at_respects_origin) {
    sat_grid_t g = make_grid(0);
    g.origin_x = 48;
    g.origin_y = 12;
    ASSERT_EQ(col_at(&g, 48), 0);
    ASSERT_EQ(col_at(&g, 56), 1);
    ASSERT_EQ(col_at(&g, 47), -1);
    ASSERT_EQ(row_at(&g, 12), 0);
    ASSERT_EQ(row_at(&g, 11), -1);
}

TEST(tile_center_roundtrip) {
    sat_grid_t g = make_grid(0);
    g.origin_x = 48;
    g.origin_y = 12;
    for (int c = 0; c < 8; ++c) {
        ASSERT_EQ(col_at(&g, tile_center_x(&g, c)), c);
    }
    for (int r = 0; r < 5; ++r) {
        ASSERT_EQ(row_at(&g, tile_center_y(&g, r)), r);
    }
    ASSERT_EQ(tile_center_x(&g, 0), 52);
    ASSERT_EQ(tile_center_y(&g, 0), 16);
}

TEST(at_tile_center_detection) {
    const sat_grid_t g = make_grid(1);
    ASSERT_TRUE(at_tile_center(&g, 4, 4));
    ASSERT_TRUE(at_tile_center(&g, 12, 4));
    ASSERT_FALSE(at_tile_center(&g, 5, 4));
    ASSERT_FALSE(at_tile_center(&g, 4, 5));
    /* Negative side of the origin must still detect centres. */
    ASSERT_TRUE(at_tile_center(&g, -4, 4));
}

TEST(wrap_col_only_when_enabled) {
    const sat_grid_t wrapping = make_grid(1);
    const sat_grid_t plain = make_grid(0);
    ASSERT_EQ(wrap_col(&wrapping, -1), 7);
    ASSERT_EQ(wrap_col(&wrapping, 8), 0);
    ASSERT_EQ(wrap_col(&wrapping, 3), 3);
    ASSERT_EQ(wrap_col(&plain, -1), -1);
    ASSERT_EQ(wrap_col(&plain, 8), 8);
}

TEST(can_step_blocks_walls_and_edges) {
    const sat_grid_t g = make_grid(1);
    /* (1,1) is floor; up is row 0, all wall. */
    ASSERT_FALSE(can_step(&g, 1, 1, SAT_DIR_UP, map_is_floor, nullptr));
    ASSERT_TRUE(can_step(&g, 1, 1, SAT_DIR_RIGHT, map_is_floor, nullptr));
    ASSERT_TRUE(can_step(&g, 1, 1, SAT_DIR_DOWN, map_is_floor, nullptr));
    /* Off the top of the map. */
    ASSERT_FALSE(can_step(&g, 1, 0, SAT_DIR_UP, map_is_floor, nullptr));
    ASSERT_FALSE(can_step(&g, 1, 1, SAT_DIR_NONE, map_is_floor, nullptr));
    /* Row 3 is walled at columns 0 and 2..4. */
    ASSERT_FALSE(can_step(&g, 1, 3, SAT_DIR_RIGHT, map_is_floor, nullptr));
    ASSERT_FALSE(can_step(&g, 1, 3, SAT_DIR_LEFT, map_is_floor, nullptr));
}

TEST(can_step_wraps_through_tunnel) {
    const sat_grid_t wrapping = make_grid(1);
    const sat_grid_t plain = make_grid(0);
    /* Row 2 is open at both ends, so leaving column 0 leftwards re-enters at 7. */
    ASSERT_TRUE(can_step(&wrapping, 0, 2, SAT_DIR_LEFT, map_is_floor, nullptr));
    ASSERT_TRUE(can_step(&wrapping, 7, 2, SAT_DIR_RIGHT, map_is_floor, nullptr));
    /* Without wrapping the same step runs off the edge. */
    ASSERT_FALSE(can_step(&plain, 0, 2, SAT_DIR_LEFT, map_is_floor, nullptr));
}

TEST(actor_step_moves_and_stops_at_wall) {
    const sat_grid_t g = make_grid(1);
    sat_grid_actor_t a = {};
    a.x = tile_center_x(&g, 1);
    a.y = tile_center_y(&g, 1);
    a.dir = SAT_DIR_RIGHT;
    a.want = SAT_DIR_RIGHT;
    a.speed = 2;

    /* Four steps of 2px cross exactly one 8px tile. */
    for (int i = 0; i < 4; ++i) {
        ASSERT_TRUE(actor_step(&g, &a, map_is_floor, nullptr));
    }
    ASSERT_EQ(a.x, tile_center_x(&g, 2));
    ASSERT_EQ(a.y, tile_center_y(&g, 1));

    /* Walk right until the wall at column 7 stops it. */
    for (int i = 0; i < 200; ++i) {
        actor_step(&g, &a, map_is_floor, nullptr);
    }
    ASSERT_EQ(a.dir, SAT_DIR_NONE);
    ASSERT_EQ(a.x, tile_center_x(&g, 6));
}

TEST(actor_step_turns_only_at_tile_center) {
    const sat_grid_t g = make_grid(1);
    sat_grid_actor_t a = {};
    a.x = tile_center_x(&g, 1);
    a.y = tile_center_y(&g, 1);
    a.dir = SAT_DIR_RIGHT;
    a.want = SAT_DIR_RIGHT;
    a.speed = 2;

    actor_step(&g, &a, map_is_floor, nullptr);  /* now 2px off-centre */
    a.want = SAT_DIR_DOWN;
    actor_step(&g, &a, map_is_floor, nullptr);
    ASSERT_EQ(a.dir, SAT_DIR_RIGHT);            /* too early to turn */

    actor_step(&g, &a, map_is_floor, nullptr);
    actor_step(&g, &a, map_is_floor, nullptr);  /* back on a centre */
    ASSERT_TRUE(at_tile_center(&g, (int)a.x, (int)a.y));
    actor_step(&g, &a, map_is_floor, nullptr);
    ASSERT_EQ(a.dir, SAT_DIR_DOWN);
}

TEST(actor_step_wraps_through_tunnel) {
    const sat_grid_t g = make_grid(1);
    sat_grid_actor_t a = {};
    a.x = tile_center_x(&g, 0);
    a.y = tile_center_y(&g, 2);
    a.dir = SAT_DIR_LEFT;
    a.want = SAT_DIR_LEFT;
    a.speed = 2;

    for (int i = 0; i < 4; ++i) {
        ASSERT_TRUE(actor_step(&g, &a, map_is_floor, nullptr));
    }
    /* One tile past column 0 leftwards lands on column 7's centre. */
    ASSERT_EQ(a.x, tile_center_x(&g, 7));
    ASSERT_EQ(a.dir, SAT_DIR_LEFT);
}

TEST(chase_dir_moves_toward_target) {
    const sat_grid_t g = make_grid(1);
    /* From (1,1), chasing (6,1) should go right. */
    ASSERT_EQ(chase_dir(&g, 1, 1, SAT_DIR_NONE, 6, 1, map_is_floor, nullptr),
              SAT_DIR_RIGHT);
    /* From (1,1), chasing (1,3) should go down. */
    ASSERT_EQ(chase_dir(&g, 1, 1, SAT_DIR_NONE, 1, 3, map_is_floor, nullptr),
              SAT_DIR_DOWN);
}

TEST(chase_dir_never_reverses_unless_boxed_in) {
    const sat_grid_t g = make_grid(1);
    /* Travelling right into (6,1): the target is behind, but reversing is
     * forbidden while another option exists (down into (6,2)). */
    const int d = chase_dir(&g, 6, 1, SAT_DIR_RIGHT, 1, 1, map_is_floor, nullptr);
    ASSERT_TRUE(d != SAT_DIR_LEFT);
    ASSERT_TRUE(d != SAT_DIR_NONE);

    /* (1,3) is a dead end reached from above: walls left, right and below, so
     * the only way out is the reversal the chase rule normally forbids. */
    ASSERT_EQ(chase_dir(&g, 1, 3, SAT_DIR_DOWN, 0, 0, map_is_floor, nullptr),
              SAT_DIR_UP);
}

TEST(wander_dir_returns_walkable_dirs) {
    const sat_grid_t g = make_grid(1);
    for (uint32_t seed = 0; seed < 16u; ++seed) {
        const int d = wander_dir(&g, 1, 1, SAT_DIR_NONE, seed, map_is_floor, nullptr);
        ASSERT_TRUE(d != SAT_DIR_NONE);
        ASSERT_TRUE(can_step(&g, 1, 1, d, map_is_floor, nullptr));
    }
}

TEST(null_and_degenerate_inputs_are_safe) {
    ASSERT_EQ(col_at(nullptr, 10), 0);
    ASSERT_EQ(wrap_col(nullptr, 10), 10);
    ASSERT_FALSE(at_tile_center(nullptr, 0, 0));
    ASSERT_FALSE(can_step(nullptr, 0, 0, SAT_DIR_UP, map_is_floor, nullptr));

    const sat_grid_t g = make_grid(1);
    ASSERT_FALSE(can_step(&g, 1, 1, SAT_DIR_UP, nullptr, nullptr));
    ASSERT_FALSE(actor_step(&g, nullptr, map_is_floor, nullptr));

    sat_grid_t zero = {};
    ASSERT_EQ(col_at(&zero, 10), 0);
    ASSERT_EQ(tile_center_x(&zero, 3), 0);
}

int main() {
    dir_helpers();
    col_at_uses_floor_division();
    col_at_respects_origin();
    tile_center_roundtrip();
    at_tile_center_detection();
    wrap_col_only_when_enabled();
    can_step_blocks_walls_and_edges();
    can_step_wraps_through_tunnel();
    actor_step_moves_and_stops_at_wall();
    actor_step_turns_only_at_tile_center();
    actor_step_wraps_through_tunnel();
    chase_dir_moves_toward_target();
    chase_dir_never_reverses_unless_boxed_in();
    wander_dir_returns_walkable_dirs();
    null_and_degenerate_inputs_are_safe();

    printf("PASS: test_grid_logic.cpp (%d tests)\n", 15);
    return 0;
}
