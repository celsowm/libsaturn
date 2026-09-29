/* Stage layout -> playable level. See pacman_level.h. */

#include "pacman_level.h"

#define ROW_BIT(col) (1ul << (uint32_t)(col))

/* A set of tiles, one bit per column. 28 columns fit a uint32_t. */
typedef struct tile_set {
    uint32_t rows[kPacMazeRows];
} tile_set_t;

static int tile_in(const tile_set_t* set, int col, int row) {
    return (set->rows[row] & ROW_BIT(col)) != 0u;
}

static void tile_add(tile_set_t* set, int col, int row) {
    set->rows[row] |= ROW_BIT(col);
}

static void tile_clear(tile_set_t* set) {
    int r;
    for (r = 0; r < kPacMazeRows; ++r) {
        set->rows[r] = 0u;
    }
}

/* Every tile reachable from the tiles already in `set`, walking through
 * cells where `open(cell)` holds, with columns wrapping through the side
 * tunnels the way the simulation's grid does. A plain stack walk: the maze
 * is 700 tiles, so the stack never needs more than that. */
static void flood(const char maze[kPacMazeRows][kPacMazeCols + 1],
                  int (*open)(char), tile_set_t* set) {
    static uint16_t stack[kPacMazeRows * kPacMazeCols];
    uint16_t top = 0;
    int r;
    int c;

    for (r = 0; r < kPacMazeRows; ++r) {
        for (c = 0; c < kPacMazeCols; ++c) {
            if (tile_in(set, c, r)) {
                stack[top++] = (uint16_t)((r * kPacMazeCols) + c);
            }
        }
    }
    while (top > 0u) {
        static const int kStep[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        const uint16_t cell = stack[--top];
        const int col = (int)(cell % kPacMazeCols);
        const int row = (int)(cell / kPacMazeCols);
        int d;

        for (d = 0; d < 4; ++d) {
            const int nr = row + kStep[d][1];
            int nc = col + kStep[d][0];
            if (nr < 0 || nr >= kPacMazeRows) {
                continue;
            }
            nc = (nc + kPacMazeCols) % kPacMazeCols;
            if (tile_in(set, nc, nr) || !open(maze[nr][nc])) {
                continue;
            }
            tile_add(set, nc, nr);
            stack[top++] = (uint16_t)((nr * kPacMazeCols) + nc);
        }
    }
}

static int open_floor(char cell) {
    return cell != PAC_CELL_WALL;
}

static int open_pen(char cell) {
    return cell != PAC_CELL_WALL && cell != PAC_CELL_DOOR;
}

/* Copies the layout and checks every character. Markers stay in `maze` for
 * the passes below and are replaced at the end. */
static pac_level_error_t copy_layout(const pac_stage_t* stage,
                                     char maze[kPacMazeRows][kPacMazeCols + 1]) {
    int r;
    for (r = 0; r < kPacMazeRows; ++r) {
        const char* src = stage->rows[r];
        int c;
        if (src == 0) {
            return PAC_LEVEL_BAD_SHAPE;
        }
        for (c = 0; c < kPacMazeCols; ++c) {
            const char cell = src[c];
            if (cell == '\0') {
                return PAC_LEVEL_BAD_SHAPE;
            }
            if (cell != PAC_CELL_WALL && cell != PAC_CELL_PELLET &&
                cell != PAC_CELL_POWER && cell != PAC_CELL_EMPTY &&
                cell != PAC_CELL_PAC && cell != PAC_CELL_GHOST &&
                cell != PAC_CELL_DOOR) {
                return PAC_LEVEL_BAD_CELL;
            }
            maze[r][c] = cell;
        }
        if (src[kPacMazeCols] != '\0') {
            return PAC_LEVEL_BAD_SHAPE;
        }
        maze[r][kPacMazeCols] = '\0';
    }
    return PAC_LEVEL_OK;
}

/* Finds P, the Gs and the door, in reading order. */
static pac_level_error_t find_markers(const char maze[kPacMazeRows][kPacMazeCols + 1],
                                      pac_level_t* out) {
    int pacs = 0;
    int ghosts = 0;
    int doors = 0;
    int r;
    int c;

    for (r = 0; r < kPacMazeRows; ++r) {
        for (c = 0; c < kPacMazeCols; ++c) {
            const char cell = maze[r][c];
            if (cell == PAC_CELL_PAC) {
                out->pac_col = (int8_t)c;
                out->pac_row = (int8_t)r;
                ++pacs;
            } else if (cell == PAC_CELL_GHOST) {
                if (ghosts < PAC_GHOST_COUNT) {
                    out->ghost_col[ghosts] = (int8_t)c;
                    out->ghost_row[ghosts] = (int8_t)r;
                }
                ++ghosts;
            } else if (cell == PAC_CELL_DOOR) {
                if (doors == 0) {
                    out->door_row = (int8_t)r;
                    out->door_col_min = (int8_t)c;
                } else if (r != out->door_row || c != out->door_col_max + 1) {
                    return PAC_LEVEL_DOOR; /* one unbroken run, in one row */
                }
                out->door_col_max = (int8_t)c;
                ++doors;
            }
        }
    }
    if (pacs != 1) {
        return PAC_LEVEL_PAC_SPAWN;
    }
    if (ghosts != PAC_GHOST_COUNT) {
        return PAC_LEVEL_GHOST_SPAWNS;
    }
    if (doors == 0 || out->door_row == 0) {
        return PAC_LEVEL_DOOR;
    }
    for (c = out->door_col_min; c <= out->door_col_max; ++c) {
        if (maze[out->door_row - 1][c] == PAC_CELL_WALL) {
            return PAC_LEVEL_DOOR; /* ghosts leave upwards */
        }
    }
    return PAC_LEVEL_OK;
}

/* The pen is everything the ghosts' spawn tiles can reach without passing
 * the door. If that includes Pac-Man's tile, the pen has a hole in it. */
static pac_level_error_t find_pen(const char maze[kPacMazeRows][kPacMazeCols + 1],
                                  pac_level_t* out) {
    tile_set_t pen;
    int g;
    int c;
    int r;

    tile_clear(&pen);
    for (g = 0; g < PAC_GHOST_COUNT; ++g) {
        tile_add(&pen, out->ghost_col[g], out->ghost_row[g]);
    }
    flood(maze, open_pen, &pen);
    if (tile_in(&pen, out->pac_col, out->pac_row)) {
        return PAC_LEVEL_PEN_LEAKS;
    }
    for (c = out->door_col_min; c <= out->door_col_max; ++c) {
        tile_add(&pen, c, out->door_row);
    }
    for (r = 0; r < kPacMazeRows; ++r) {
        out->pen_rows[r] = pen.rows[r];
    }
    return PAC_LEVEL_OK;
}

/* Everything that matters has to be reachable from where Pac-Man starts:
 * a pellet he cannot reach makes the stage unwinnable. */
static pac_level_error_t check_reachable(const char maze[kPacMazeRows][kPacMazeCols + 1],
                                         const pac_level_t* level) {
    tile_set_t reach;
    int r;
    int c;

    tile_clear(&reach);
    tile_add(&reach, level->pac_col, level->pac_row);
    flood(maze, open_floor, &reach);
    for (r = 0; r < kPacMazeRows; ++r) {
        for (c = 0; c < kPacMazeCols; ++c) {
            const char cell = maze[r][c];
            if (cell != PAC_CELL_WALL && cell != PAC_CELL_EMPTY &&
                !tile_in(&reach, c, r)) {
                return PAC_LEVEL_UNREACHABLE;
            }
        }
    }
    return PAC_LEVEL_OK;
}

/* Replaces the markers with the floor they stand on, and counts pellets. */
static void settle_markers(char maze[kPacMazeRows][kPacMazeCols + 1], pac_level_t* out) {
    int r;
    int c;
    out->pellets = 0;
    for (r = 0; r < kPacMazeRows; ++r) {
        for (c = 0; c < kPacMazeCols; ++c) {
            char* cell = &maze[r][c];
            if (*cell == PAC_CELL_PAC) {
                *cell = PAC_CELL_PELLET;
            } else if (*cell == PAC_CELL_GHOST || *cell == PAC_CELL_DOOR) {
                *cell = PAC_CELL_EMPTY;
            }
            if (*cell == PAC_CELL_PELLET || *cell == PAC_CELL_POWER) {
                ++out->pellets;
            }
        }
    }
}

pac_level_error_t pac_level_load(const pac_stage_t* stage,
                                 char maze[kPacMazeRows][kPacMazeCols + 1],
                                 pac_level_t* out) {
    pac_level_error_t err;

    if (stage == 0 || maze == 0 || out == 0) {
        return PAC_LEVEL_BAD_SHAPE;
    }
    out->reserved = 0;
    out->reserved2 = 0u;
    if ((err = copy_layout(stage, maze)) != PAC_LEVEL_OK ||
        (err = find_markers(maze, out)) != PAC_LEVEL_OK ||
        (err = find_pen(maze, out)) != PAC_LEVEL_OK ||
        (err = check_reachable(maze, out)) != PAC_LEVEL_OK) {
        return err;
    }
    settle_markers(maze, out);
    return PAC_LEVEL_OK;
}

int pac_level_in_pen(const pac_level_t* level, int col, int row) {
    if (level == 0 || row < 0 || row >= kPacMazeRows ||
        col < 0 || col >= kPacMazeCols) {
        return 0;
    }
    return (level->pen_rows[row] & ROW_BIT(col)) != 0u;
}
