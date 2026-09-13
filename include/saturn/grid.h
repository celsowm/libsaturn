#ifndef SATURN_GRID_H
#define SATURN_GRID_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Tile-grid movement                                                  */
/* ------------------------------------------------------------------ */
/* Grid-locked actors travel tile-to-tile along corridor centres and only
 * change direction when exactly centred on a tile. That reduces collision
 * handling to a single "is the next tile walkable?" test, which is what makes
 * maze games deterministic and cheap on the SH-2.
 *
 * The grid itself owns no map data: walkability is supplied by a caller
 * callback, so the same code drives a 2D top-down view and a 3D first-person
 * view over identical state.
 */

#define SAT_DIR_NONE  (-1)
#define SAT_DIR_UP    0
#define SAT_DIR_RIGHT 1
#define SAT_DIR_DOWN  2
#define SAT_DIR_LEFT  3

/* Unit step for a direction, in tiles. Returns 0 for SAT_DIR_NONE. */
int sat_dir_dx(int dir);
int sat_dir_dy(int dir);

/* The 180-degree reversal of dir, or SAT_DIR_NONE for SAT_DIR_NONE. */
int sat_dir_opposite(int dir);

typedef struct sat_grid {
    int16_t cols;
    int16_t rows;
    int16_t tile_px;    /* square tile edge, in pixels */
    int16_t origin_x;   /* pixel X of tile (0,0)'s left edge */
    int16_t origin_y;   /* pixel Y of tile (0,0)'s top edge */
    uint8_t wrap_cols;  /* non-zero: columns wrap (side tunnel) */
    uint8_t reserved;
} sat_grid_t;

/* Walkability probe. Return non-zero when (col,row) may be entered. `col` is
 * already wrapped when the grid wraps; `row` may be out of range and the
 * callback is expected to reject it. */
typedef int (*sat_grid_is_floor_fn)(int col, int row, void* user);

typedef struct sat_grid_actor {
    int32_t x;      /* pixel centre X */
    int32_t y;      /* pixel centre Y */
    int16_t dir;    /* direction currently travelled */
    int16_t want;   /* direction requested; taken at the next tile centre */
    int16_t speed;  /* pixels per step; must divide tile_px */
    int16_t reserved;
} sat_grid_actor_t;

/* ------------------------------------------------------------------ */
/* Coordinate helpers                                                  */
/* ------------------------------------------------------------------ */

/* Brings col into [0, cols) when the grid wraps; otherwise returns col
 * unchanged (out-of-range values stay out of range so probes reject them). */
int sat_grid_wrap_col(const sat_grid_t* grid, int col);

/* Tile containing a pixel coordinate. Uses floor division, so pixels left of
 * or above the origin map to negative tiles instead of folding onto zero. */
int sat_grid_col_at(const sat_grid_t* grid, int px);
int sat_grid_row_at(const sat_grid_t* grid, int py);

/* Pixel centre of a tile. */
int sat_grid_tile_center_x(const sat_grid_t* grid, int col);
int sat_grid_tile_center_y(const sat_grid_t* grid, int row);

/* Non-zero when (px,py) is exactly on a tile centre, i.e. a turn is legal. */
int sat_grid_at_tile_center(const sat_grid_t* grid, int px, int py);

/* Non-zero when the tile adjacent to (col,row) along dir is walkable. */
int sat_grid_can_step(
    const sat_grid_t* grid,
    int col,
    int row,
    int dir,
    sat_grid_is_floor_fn is_floor,
    void* user
);

/* Wraps an actor that walked off a tunnel edge back to the far side. */
void sat_grid_wrap_actor(const sat_grid_t* grid, sat_grid_actor_t* actor);

/* ------------------------------------------------------------------ */
/* Stepping and steering                                               */
/* ------------------------------------------------------------------ */

/* Advances the actor one frame: at a tile centre it adopts `want` when that
 * direction is walkable and stops (dir = SAT_DIR_NONE) when the current
 * direction is not; then it moves `speed` pixels and wraps.
 * Returns non-zero when the actor actually moved. */
int sat_grid_actor_step(
    const sat_grid_t* grid,
    sat_grid_actor_t* actor,
    sat_grid_is_floor_fn is_floor,
    void* user
);

/* Greedy pursuit: of the walkable neighbours of (col,row), excluding a
 * reversal of cur_dir, returns the one whose tile is closest (squared tile
 * distance) to (target_col,target_row). Falls back to any walkable direction
 * -- reversal included -- when boxed in, and SAT_DIR_NONE when fully walled. */
int sat_grid_chase_dir(
    const sat_grid_t* grid,
    int col,
    int row,
    int cur_dir,
    int target_col,
    int target_row,
    sat_grid_is_floor_fn is_floor,
    void* user
);

/* As sat_grid_chase_dir, but picks uniformly among the non-reversing walkable
 * neighbours using `rnd` (any 32-bit value; only the low bits are used). */
int sat_grid_wander_dir(
    const sat_grid_t* grid,
    int col,
    int row,
    int cur_dir,
    uint32_t rnd,
    sat_grid_is_floor_fn is_floor,
    void* user
);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_GRID_H */
