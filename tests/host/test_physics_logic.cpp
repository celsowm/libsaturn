#include <cstdio>
#include <cstdlib>

#include "saturn/physics.h"
#include "src/physics/2d/logic.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

static sat_fx16_t F(int x) { return static_cast<sat_fx16_t>(x * SAT_FX16_ONE); }

static uint32_t g_frames;
extern "C" uint32_t sat_frame_count(void) { return g_frames; }

static int solid_tiles(int c, int r, void*) {
    return (r == 3 && c >= 0 && c < 8) ? SAT_TILE_SOLID : SAT_TILE_EMPTY;
}

static int legacy_slope_tiles(int c, int r, void*) {
    return (c == 2 && r == 3) ? SAT_TILE_SLOPE_UP : SAT_TILE_EMPTY;
}

static int shallow_slope_tiles(int c, int r, void*, sat_tile_surface_t* surface) {
    if (c != 2 || r != 3) return SAT_TILE_EMPTY;
    *surface = {0, SAT_FX16_ONE, SAT_FX16_ONE, SAT_FX16_ONE / 2};
    return SAT_TILE_SLOPE;
}

static void clock_and_solid_floor() {
    sat_step_clock_t clock;
    g_frames = 100;
    saturn::core::physics::clock_init(clock);
    g_frames = 105;
    OK(saturn::core::physics::clock_steps(clock, 8) == 5);
    g_frames = 130;
    OK(saturn::core::physics::clock_steps(clock, 4) == 4);

    sat_body2_t body = {{{F(12), F(20)}, {F(3), F(3)}}, {0, F(20)}, 0};
    sat_grid_t grid = {8, 6, 8, 0, 0, 0, 0};
    OK(saturn::core::physics::move_tiles(body, grid, solid_tiles, nullptr) == SAT_OK);
    OK((body.flags & SAT_BODY_GROUNDED) != 0);
    OK(body.box.center.y == F(21));
}

static void slope_surface_geometry() {
    sat_grid_t grid = {8, 6, 8, 0, 0, 0, 0};
    sat_fx16_t top = 0;

    const sat_tile_surface_t legacy =
        saturn::core::physics::slope_preset(SAT_TILE_SLOPE_UP);
    OK(saturn::core::physics::surface_top(grid, 2, 3, legacy, F(20), top));
    OK(top == F(28));

    const sat_tile_surface_t shallow =
        {0, SAT_FX16_ONE, SAT_FX16_ONE, SAT_FX16_ONE / 2};
    OK(saturn::core::physics::surface_top(grid, 2, 3, shallow, F(20), top));
    OK(top == F(30));

    const sat_tile_surface_t steep =
        {0, SAT_FX16_ONE, SAT_FX16_ONE / 2, 0};
    OK(saturn::core::physics::surface_top(grid, 2, 3, steep, F(18), top));
    OK(top == F(28));
    OK(!saturn::core::physics::surface_top(grid, 2, 3, steep, F(22), top));
}

static void legacy_and_custom_slopes_ground() {
    sat_grid_t grid = {8, 6, 8, 0, 0, 0, 0};

    sat_body2_t legacy = {{{F(20), F(20)}, {F(2), F(2)}}, {0, F(8)}, 0};
    OK(saturn::core::physics::move_tiles(legacy, grid, legacy_slope_tiles, nullptr) == SAT_OK);
    OK((legacy.flags & SAT_BODY_GROUNDED) != 0);
    OK(legacy.box.center.y == F(26));

    sat_body2_t shallow = {{{F(20), F(20)}, {F(2), F(2)}}, {0, F(8)}, 0};
    OK(saturn::core::physics::move_tiles_surface(
        shallow, grid, shallow_slope_tiles, nullptr) == SAT_OK);
    OK((shallow.flags & SAT_BODY_GROUNDED) != 0);
    OK(shallow.box.center.y == F(28));
}

static void bodies_separate() {
    sat_body2_t a = {{{0, 0}, {F(4), F(4)}}, {0, 0}, 0};
    sat_body2_t b = {{{F(6), 0}, {F(4), F(4)}}, {0, 0}, 0};
    OK(saturn::core::physics::separate(a, b));
    OK(!sat_box2_overlap(&a.box, &b.box));
}

int main() {
    clock_and_solid_floor();
    slope_surface_geometry();
    legacy_and_custom_slopes_ground();
    bodies_separate();
    std::puts("PASS: test_physics_logic.cpp");
    return 0;
}
