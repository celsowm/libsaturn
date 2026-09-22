#ifndef SATURN_CORE_GRID_LOGIC_HPP
#define SATURN_CORE_GRID_LOGIC_HPP

/* Pure, host-testable tile-grid movement for libsaturn.
 *
 * No hardware access, so tests/host/test_grid_logic.cpp links this directly.
 * The public C API in include/saturn/grid.h is a thin wrapper over these
 * helpers.
 */

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/grid.h"

namespace saturn::core::grid {

/* Floor division / modulo. C's division truncates toward zero, which folds the
 * two tiles either side of the origin onto the same index and makes an actor
 * in a left-hand tunnel report the wrong tile. */
inline int floor_div(int a, int b) {
    int q = a / b;
    if (((a % b) != 0) && ((a < 0) != (b < 0))) {
        --q;
    }
    return q;
}

inline int floor_mod(int a, int b) {
    int r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) {
        r += b;
    }
    return r;
}

inline int dir_dx(int dir) {
    if (dir == SAT_DIR_RIGHT) {
        return 1;
    }
    if (dir == SAT_DIR_LEFT) {
        return -1;
    }
    return 0;
}

inline int dir_dy(int dir) {
    if (dir == SAT_DIR_DOWN) {
        return 1;
    }
    if (dir == SAT_DIR_UP) {
        return -1;
    }
    return 0;
}

inline int dir_opposite(int dir) {
    if (dir == SAT_DIR_NONE) {
        return SAT_DIR_NONE;
    }
    return (dir + 2) & 3;
}

inline bool grid_valid(const sat_grid_t* g) {
    return g != nullptr && g->cols > 0 && g->rows > 0 && g->tile_px > 0;
}

inline int wrap_col(const sat_grid_t* g, int col) {
    if (!grid_valid(g) || g->wrap_cols == 0u) {
        return col;
    }
    return floor_mod(col, g->cols);
}

inline int col_at(const sat_grid_t* g, int px) {
    if (!grid_valid(g)) {
        return 0;
    }
    return floor_div(px - g->origin_x, g->tile_px);
}

inline int row_at(const sat_grid_t* g, int py) {
    if (!grid_valid(g)) {
        return 0;
    }
    return floor_div(py - g->origin_y, g->tile_px);
}

inline int tile_center_x(const sat_grid_t* g, int col) {
    if (!grid_valid(g)) {
        return 0;
    }
    return g->origin_x + (col * g->tile_px) + (g->tile_px / 2);
}

inline int tile_center_y(const sat_grid_t* g, int row) {
    if (!grid_valid(g)) {
        return 0;
    }
    return g->origin_y + (row * g->tile_px) + (g->tile_px / 2);
}

inline bool at_tile_center(const sat_grid_t* g, int px, int py) {
    if (!grid_valid(g)) {
        return false;
    }
    const int half = g->tile_px / 2;
    return floor_mod(px - g->origin_x - half, g->tile_px) == 0 &&
           floor_mod(py - g->origin_y - half, g->tile_px) == 0;
}

inline bool can_step(
    const sat_grid_t* g,
    int col,
    int row,
    int dir,
    sat_grid_is_floor_fn is_floor,
    void* user
) {
    if (!grid_valid(g) || is_floor == nullptr || dir == SAT_DIR_NONE) {
        return false;
    }
    const int nr = row + dir_dy(dir);
    if (nr < 0 || nr >= g->rows) {
        return false;
    }
    int nc = col + dir_dx(dir);
    if (g->wrap_cols != 0u) {
        nc = wrap_col(g, nc);
    } else if (nc < 0 || nc >= g->cols) {
        return false;
    }
    return is_floor(nc, nr, user) != 0;
}

/* The playfield spans [origin_x, origin_x + cols * tile_px). An actor is
 * teleported once its centre reaches half a tile beyond either edge. The
 * bounds are inclusive because that exact point is one whole tile from the
 * far edge's first tile centre, so the wrap lands on a centre rather than
 * knocking the actor off the turn grid. */
inline void wrap_actor(const sat_grid_t* g, sat_grid_actor_t* a) {
    if (!grid_valid(g) || a == nullptr || g->wrap_cols == 0u) {
        return;
    }
    const int span = g->cols * g->tile_px;
    const int half = g->tile_px / 2;
    const int lo = g->origin_x - half;
    const int hi = g->origin_x + span + half;
    if (a->x <= lo) {
        a->x += span;
    } else if (a->x >= hi) {
        a->x -= span;
    }
}

inline bool actor_step(
    const sat_grid_t* g,
    sat_grid_actor_t* a,
    sat_grid_is_floor_fn is_floor,
    void* user
) {
    if (!grid_valid(g) || a == nullptr || is_floor == nullptr) {
        return false;
    }
    if (at_tile_center(g, static_cast<int>(a->x), static_cast<int>(a->y))) {
        const int col = wrap_col(g, col_at(g, static_cast<int>(a->x)));
        const int row = row_at(g, static_cast<int>(a->y));
        if (a->want != SAT_DIR_NONE && can_step(g, col, row, a->want, is_floor, user)) {
            a->dir = a->want;
        }
        if (!can_step(g, col, row, a->dir, is_floor, user)) {
            a->dir = SAT_DIR_NONE;
            return false;
        }
    }
    if (a->dir == SAT_DIR_NONE || a->speed == 0) {
        return false;
    }
    a->x += dir_dx(a->dir) * a->speed;
    a->y += dir_dy(a->dir) * a->speed;
    wrap_actor(g, a);
    return true;
}

/* Collects the walkable non-reversing neighbours of (col,row) into out_dirs,
 * returning how many there were. */
inline int gather_dirs(
    const sat_grid_t* g,
    int col,
    int row,
    int cur_dir,
    sat_grid_is_floor_fn is_floor,
    void* user,
    int out_dirs[4]
) {
    const int reverse = dir_opposite(cur_dir);
    int count = 0;
    for (int d = 0; d < 4; ++d) {
        if (d == reverse) {
            continue;
        }
        if (can_step(g, col, row, d, is_floor, user)) {
            out_dirs[count++] = d;
        }
    }
    return count;
}

/* A dead end offers only the reversal, so callers must be able to turn back
 * rather than freeze. */
inline int any_dir(
    const sat_grid_t* g,
    int col,
    int row,
    sat_grid_is_floor_fn is_floor,
    void* user
) {
    for (int d = 0; d < 4; ++d) {
        if (can_step(g, col, row, d, is_floor, user)) {
            return d;
        }
    }
    return SAT_DIR_NONE;
}

inline int chase_dir(
    const sat_grid_t* g,
    int col,
    int row,
    int cur_dir,
    int target_col,
    int target_row,
    sat_grid_is_floor_fn is_floor,
    void* user
) {
    if (!grid_valid(g) || is_floor == nullptr) {
        return SAT_DIR_NONE;
    }
    int dirs[4];
    const int count = gather_dirs(g, col, row, cur_dir, is_floor, user, dirs);
    if (count == 0) {
        return any_dir(g, col, row, is_floor, user);
    }

    int best = SAT_DIR_NONE;
    int32_t best_score = 0;
    for (int i = 0; i < count; ++i) {
        const int d = dirs[i];
        const int nc = wrap_col(g, col + dir_dx(d));
        const int nr = row + dir_dy(d);
        const int32_t dx = static_cast<int32_t>(nc - target_col);
        const int32_t dy = static_cast<int32_t>(nr - target_row);
        const int32_t score = (dx * dx) + (dy * dy);
        if (best == SAT_DIR_NONE || score < best_score) {
            best = d;
            best_score = score;
        }
    }
    return best;
}

inline int wander_dir(
    const sat_grid_t* g,
    int col,
    int row,
    int cur_dir,
    uint32_t rnd,
    sat_grid_is_floor_fn is_floor,
    void* user
) {
    if (!grid_valid(g) || is_floor == nullptr) {
        return SAT_DIR_NONE;
    }
    int dirs[4];
    const int count = gather_dirs(g, col, row, cur_dir, is_floor, user, dirs);
    if (count == 0) {
        return any_dir(g, col, row, is_floor, user);
    }
    return dirs[rnd % static_cast<uint32_t>(count)];
}

}  // namespace saturn::core::grid

#endif /* SATURN_CORE_GRID_LOGIC_HPP */
