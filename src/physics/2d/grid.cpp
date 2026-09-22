#include "saturn/grid.h"

#include "src/physics/2d/grid_logic.hpp"

using namespace saturn::core::grid;

extern "C" int sat_dir_dx(int dir) {
    return dir_dx(dir);
}

extern "C" int sat_dir_dy(int dir) {
    return dir_dy(dir);
}

extern "C" int sat_dir_opposite(int dir) {
    return dir_opposite(dir);
}

extern "C" int sat_grid_wrap_col(const sat_grid_t* grid, int col) {
    return wrap_col(grid, col);
}

extern "C" int sat_grid_col_at(const sat_grid_t* grid, int px) {
    return col_at(grid, px);
}

extern "C" int sat_grid_row_at(const sat_grid_t* grid, int py) {
    return row_at(grid, py);
}

extern "C" int sat_grid_tile_center_x(const sat_grid_t* grid, int col) {
    return tile_center_x(grid, col);
}

extern "C" int sat_grid_tile_center_y(const sat_grid_t* grid, int row) {
    return tile_center_y(grid, row);
}

extern "C" int sat_grid_at_tile_center(const sat_grid_t* grid, int px, int py) {
    return at_tile_center(grid, px, py) ? 1 : 0;
}

extern "C" int sat_grid_can_step(
    const sat_grid_t* grid,
    int col,
    int row,
    int dir,
    sat_grid_is_floor_fn is_floor,
    void* user
) {
    return can_step(grid, col, row, dir, is_floor, user) ? 1 : 0;
}

extern "C" void sat_grid_wrap_actor(const sat_grid_t* grid, sat_grid_actor_t* actor) {
    wrap_actor(grid, actor);
}

extern "C" int sat_grid_actor_step(
    const sat_grid_t* grid,
    sat_grid_actor_t* actor,
    sat_grid_is_floor_fn is_floor,
    void* user
) {
    return actor_step(grid, actor, is_floor, user) ? 1 : 0;
}

extern "C" int sat_grid_chase_dir(
    const sat_grid_t* grid,
    int col,
    int row,
    int cur_dir,
    int target_col,
    int target_row,
    sat_grid_is_floor_fn is_floor,
    void* user
) {
    return chase_dir(grid, col, row, cur_dir, target_col, target_row, is_floor, user);
}

extern "C" int sat_grid_wander_dir(
    const sat_grid_t* grid,
    int col,
    int row,
    int cur_dir,
    uint32_t rnd,
    sat_grid_is_floor_fn is_floor,
    void* user
) {
    return wander_dir(grid, col, row, cur_dir, rnd, is_floor, user);
}
