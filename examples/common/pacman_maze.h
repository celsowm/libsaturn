#ifndef PACMAN_MAZE_H
#define PACMAN_MAZE_H

/* Shared maze data + helpers for the pacman_2d and pacman_3d examples.
 *
 * 28 columns x 25 rows. Legend:
 *   '#' wall
 *   '.' pellet
 *   'o' power pellet
 *   ' ' open corridor (no pellet; used for the ghost pen / spawn pockets)
 *
 * The maze is horizontally symmetric. It is intentionally header-only so both
 * examples (and host tests) compile the exact same layout without a separate
 * asset pipeline. Define PACMAN_MAZE_IMPL in exactly one translation unit
 * (the examples do so in main.c) to emit the definition.
 *
 * Cell coordinates are (col, row). 1 tile = kPacTilePx x kPacTilePx pixels.
 */

enum {
    kPacMazeCols = 28,
    kPacMazeRows = 25,
    kPacTilePx = 8,
    kPacMazePixelW = kPacMazeCols * kPacTilePx,  /* 224 */
    kPacMazePixelH = kPacMazeRows * kPacTilePx,  /* 200 */
    kPacTileNone = -1
};

#ifdef PACMAN_MAZE_IMPL
static const char kPacMaze[kPacMazeRows][kPacMazeCols + 1] = {
    "############################",
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#o####.#####.##.#####.####o#",
    "#.####.#####.##.#####.####.#",
    "#..........................#",
    "#.####.##.########.##.####.#",
    "#.####.##.########.##.####.#",
    "#......##....##....##......#",
    "######.#####.##.#####.######",
    "######.#####.##.#####.######",
    "     #.##          ##.#     ",
    "     #.## ###  ### ##.#     ",
    "######.## #      # ##.######",
    "      .   #      #   .      ",
    "######.## #      # ##.######",
    "     #.## ######## ##.#     ",
    "     #.##          ##.#     ",
    "     #.## ######## ##.#     ",
    "######.## ######## ##.######",
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#o..##.......##.......##..o#",
    "###.##.##.########.##.##.###",
    "############################",
};
#endif /* PACMAN_MAZE_IMPL */

/* Returns the maze character at (col, row), or '#' when out of bounds.
 * Rows wrap vertically? No — only columns wrap (tunnel). */
static inline char pac_maze_at(const char (*maze)[kPacMazeCols + 1], int col, int row) {
    if (row < 0 || row >= kPacMazeRows) {
        return '#';
    }
    if (col < 0 || col >= kPacMazeCols) {
        return '#';  /* callers handle the tunnel by wrapping before calling */
    }
    return maze[row][col];
}

/* Wall test. '#' is the only solid cell. */
static inline int pac_maze_is_wall(const char (*maze)[kPacMazeCols + 1], int col, int row) {
    return pac_maze_at(maze, col, row) == '#';
}

/* Walkable test: corridor/pellet/power/open pocket. */
static inline int pac_maze_is_floor(const char (*maze)[kPacMazeCols + 1], int col, int row) {
    return !pac_maze_is_wall(maze, col, row);
}

/* Wrap a column through the side tunnels. Only the two tunnel rows contain
 * out-of-range columns, so a simple modulo is enough. */
static inline int pac_maze_wrap_col(int col) {
    int c = col % kPacMazeCols;
    if (c < 0) {
        c += kPacMazeCols;
    }
    return c;
}

#endif /* PACMAN_MAZE_H */
