/* Every stage layout, in the order they are played.
 *
 * To add a stage, append a layout here; nothing else needs to change. The
 * host test (tests/host/test_pacman_game.cpp) loads every entry through
 * pac_level_load, so a malformed one -- a missing spawn, a pen with a hole in
 * it, a pellet nobody can reach -- fails the build's tests instead of
 * showing up as a ghost that never leaves home.
 *
 * Two budgets that are not checked for you, both set by pacman_3d, which
 * draws each wall rectangle and each pellet as VDP1 polygons and is already
 * right at the edge of a frame:
 *   - about 57 wall rectangles, merged greedily (see build_wall_rects in
 *     examples/pacman_3d/board.c) -- the classic maze's count;
 *   - about 185 pellets.
 * Walls in long straight runs merge into few rectangles; many short
 * separate blocks do not. Past either budget the 3D example drops from 60
 * frames a second to 30.
 *
 * The pen can go anywhere as long as its door is on top: ghosts leave
 * upwards. A row open at both edges is a tunnel -- columns wrap. */

#include "pacman_stages.h"

static const pac_stage_t kStages[] = {
    {
        "CLASSIC",
        {
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
            "     #.## ###--### ##.#     ",
            "######.## #      # ##.######",
            "      .   # GGGG #   .      ",
            "######.## #      # ##.######",
            "     #.## ######## ##.#     ",
            "     #.##          ##.#     ",
            "     #.## ######## ##.#     ",
            "######.## ######## ##.######",
            "#...........P##............#",
            "#.####.#####.##.#####.####.#",
            "#o..##.......##.......##..o#",
            "###.##.##.########.##.##.###",
            "############################",
        },
    },
    {
        "TWIN LANES",
        {
            "############################",
            "#......#.....##.....#......#",
            "#.####.#.###.##.###.#.####.#",
            "#o####.#.###.##.###.#.####o#",
            "#.####.#.###.##.###.#.####.#",
            "#............##............#",
            "#.##.####.########.####.##.#",
            "#.##.####.########.####.##.#",
            "#....#.......##.......#....#",
            "######.#####.##.#####.######",
            "######.#####.##.#####.######",
            "     #.##          ##.#     ",
            "     #.## ###--### ##.#     ",
            "######.## #      # ##.######",
            "      .   # GGGG #   .      ",
            "######.## #      # ##.######",
            "     #.## ######## ##.#     ",
            "     #.##          ##.#     ",
            "     #.## ######## ##.#     ",
            "######.## ######## ##.######",
            "#...........P##............#",
            "#.##.#######.##.#######.##.#",
            "#o...........##...........o#",
            "############################",
            "############################",
        },
    },
    {
        "LOOPS",
        {
            "############################",
            "#o..........####..........o#",
            "#.##.######.####.######.##.#",
            "#.##.######.####.######.##.#",
            "#.##........####........##.#",
            "#....##.##.######.##.##....#",
            "####.##.##.######.##.##.####",
            "####.##.##.######.##.##.####",
            "####.........##.........####",
            "######.#####.##.#####.######",
            "######.#####.##.#####.######",
            "     #.##          ##.#     ",
            "     #.## ###--### ##.#     ",
            "######.## #      # ##.######",
            "      .   # GGGG #   .      ",
            "######.## #      # ##.######",
            "     #.## ######## ##.#     ",
            "     #.##          ##.#     ",
            "     #.## ######## ##.#     ",
            "######.## ######## ##.######",
            "#......#....P##.....#......#",
            "#.####.#.###.##.###.#.####.#",
            "#o...........##...........o#",
            "############################",
            "############################",
        },
    },
};

uint16_t pac_stage_count(void) {
    return (uint16_t)(sizeof(kStages) / sizeof(kStages[0]));
}

const pac_stage_t* pac_stage_get(uint16_t index) {
    return (index < pac_stage_count()) ? &kStages[index] : 0;
}
