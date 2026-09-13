/* pacman_2d - playable top-down Pac-Man for Sega Saturn (libsaturn), no sound.
 *
 * This file is only a renderer. Every rule -- movement, ghost AI, pellets,
 * scoring, lives -- lives in examples/common/pacman_game.c, which the
 * pacman_3d example drives with exactly the same calls. Running the two side
 * by side shows the same game from two points of view.
 *
 * Drawing is pure VDP1 in screen coordinates:
 *   - walls are merged into horizontal runs once at startup and drawn as
 *     filled rectangles, which cuts the command count by roughly 4x versus
 *     one quad per tile;
 *   - pellets, Pac-Man and the ghosts are small rectangles;
 *   - the HUD is the built-in 8x8 ASCII font.
 *
 * Controls: D-Pad to move, START to restart.
 */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/fmt.h"
#include "saturn/font.h"
#include "saturn/grid.h"
#include "saturn/input.h"
#include "saturn/vdp1.h"
#include "saturn/example_util.h"

#include "../common/pacman_game.h"

#define SCREEN_W 320
#define SCREEN_H 224

/* Centre the 224x200 maze on the 320x224 display. */
#define MAZE_X ((SCREEN_W - kPacMazePixelW) / 2)  /* 48 */
#define MAZE_Y ((SCREEN_H - kPacMazePixelH) / 2)  /* 12 */
#define TILE kPacTilePx

#define ACTOR_HALF 3
#define PELLET_HALF 1
#define POWER_HALF 3

/* The font owns one CRAM bank; maze geometry uses RGB-coded polygon colors
 * (bit 15 set, via SAT_RGB555), so the two never collide. */
#define HUD_PALETTE 2u

/* Colours */
#define COLOR_WALL   SAT_RGB555(5, 8, 31)
#define COLOR_PELLET SAT_RGB555(31, 24, 16)
#define COLOR_POWER  SAT_RGB555(31, 31, 31)
#define COLOR_PAC    SAT_RGB555(31, 31, 0)
#define COLOR_MOUTH  SAT_RGB555(0, 0, 0)
#define COLOR_BLACK  SAT_RGB555(0, 0, 0)

static const uint16_t kGhostColors[PAC_GHOST_COUNT] = {
    SAT_RGB555(31, 0, 0),   /* red    */
    SAT_RGB555(31, 18, 24), /* pink   */
    SAT_RGB555(0, 28, 31),  /* cyan   */
    SAT_RGB555(31, 20, 4),  /* orange */
};
#define COLOR_FRIGHT_A SAT_RGB555(4, 4, 31)
#define COLOR_FRIGHT_B SAT_RGB555(31, 31, 31)

static pac_game_t g_game;
static sat_ascii_font_t g_font;

/* Set when the VDP1 command list fills up. The list holds 512 commands and a
 * full round needs about 350, so this should never trigger -- but editing the
 * maze could push it over, and a silent panic loop would look like a crashed
 * console instead of telling anyone why. */
static int g_draw_overflow;

/* Wall run-length table, built once: the maze walls never move. */
static uint8_t g_run_count[kPacMazeRows];
static uint8_t g_run_start[kPacMazeRows][kPacMazeCols];
static uint8_t g_run_len[kPacMazeRows][kPacMazeCols];

/* ------------------------------------------------------------------ */
/* Static geometry                                                     */
/* ------------------------------------------------------------------ */

static void build_wall_runs(void) {
    int r;
    for (r = 0; r < kPacMazeRows; ++r) {
        int c = 0;
        g_run_count[r] = 0;
        while (c < kPacMazeCols) {
            int start;
            if (pac_game_cell(&g_game, c, r) != '#') {
                ++c;
                continue;
            }
            start = c;
            while (c < kPacMazeCols && pac_game_cell(&g_game, c, r) == '#') {
                ++c;
            }
            g_run_start[r][g_run_count[r]] = (uint8_t)start;
            g_run_len[r][g_run_count[r]] = (uint8_t)(c - start);
            ++g_run_count[r];
        }
    }
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */

/* Draw calls are never fatal: running out of VDP1 commands drops the rest of
 * the frame and lights the HUD warning rather than hanging. */
static void draw_rect(int x, int y, int w, int h, uint16_t color) {
    if (sat_draw_rect_screen((int16_t)x, (int16_t)y,
                             (uint16_t)w, (uint16_t)h, color) != SAT_OK) {
        g_draw_overflow = 1;
    }
}

static void draw_centered_box(int cx, int cy, int half, uint16_t color) {
    draw_rect(cx - half, cy - half, half * 2, half * 2, color);
}

static void render_maze(void) {
    int r;
    int c;
    int i;

    for (r = 0; r < kPacMazeRows; ++r) {
        for (i = 0; i < g_run_count[r]; ++i) {
            draw_rect(
                MAZE_X + (g_run_start[r][i] * TILE),
                MAZE_Y + (r * TILE),
                g_run_len[r][i] * TILE,
                TILE,
                COLOR_WALL);
        }
    }

    for (r = 0; r < kPacMazeRows; ++r) {
        for (c = 0; c < kPacMazeCols; ++c) {
            const char cell = pac_game_cell(&g_game, c, r);
            if (cell != '.' && cell != 'o') {
                continue;
            }
            draw_centered_box(
                sat_grid_tile_center_x(&g_game.grid, c),
                sat_grid_tile_center_y(&g_game.grid, r),
                (cell == 'o') ? POWER_HALF : PELLET_HALF,
                (cell == 'o') ? COLOR_POWER : COLOR_PELLET);
        }
    }
}

/* Pac-Man is a yellow box with a black notch chewing on the side he faces. */
static void render_pac(void) {
    const int cx = (int)g_game.pac.x;
    const int cy = (int)g_game.pac.y;
    draw_centered_box(cx, cy, ACTOR_HALF, COLOR_PAC);
    if (((g_game.frame / 4u) & 1u) == 0u) {
        return;
    }
    draw_centered_box(
        cx + (sat_dir_dx(g_game.pac.dir) * ACTOR_HALF),
        cy + (sat_dir_dy(g_game.pac.dir) * ACTOR_HALF),
        1,
        COLOR_MOUTH);
}

static void render_ghosts(void) {
    int i;
    for (i = 0; i < PAC_GHOST_COUNT; ++i) {
        uint16_t color;
        if (pac_game_ghost_penned(&g_game, i)) {
            continue;
        }
        color = kGhostColors[i];
        if (pac_game_frightened(&g_game)) {
            /* Flash white near the end so the player can see time running
             * out rather than being surprised by it. */
            color = ((g_game.frame / 6u) & 1u) ? COLOR_FRIGHT_A : COLOR_FRIGHT_B;
        }
        draw_centered_box(
            (int)g_game.ghosts[i].actor.x,
            (int)g_game.ghosts[i].actor.y,
            ACTOR_HALF,
            color);
    }
}

static void draw_text(const char* text, int x, int y) {
    if (sat_ascii_font_draw_text_screen_indexed8(
            &g_font, text, x, y, 0, HUD_PALETTE, 0) != SAT_OK) {
        g_draw_overflow = 1;
    }
}

static void draw_text_centered(const char* text, int y) {
    if (sat_ascii_font_draw_text_screen_centered_indexed8(
            &g_font, text, SCREEN_W / 2, y, 0, HUD_PALETTE, 0) != SAT_OK) {
        g_draw_overflow = 1;
    }
}

static void render_hud(void) {
    char text[24];
    int i;

    sat_example_must(sat_fmt_label_u32("SCORE ", g_game.score, text, sizeof(text), NULL));
    draw_text(text, 4, 2);

    draw_text("LIVES", 232, 2);
    for (i = 0; i < g_game.lives; ++i) {
        draw_rect(280 + (i * 10), 3, 6, 6, COLOR_PAC);
    }

    if (g_game.state == PAC_STATE_WIN) {
        draw_text_centered("YOU WIN", 104);
        draw_text_centered("PRESS START", 116);
    } else if (g_game.state == PAC_STATE_LOSE) {
        draw_text_centered("GAME OVER", 104);
        draw_text_centered("PRESS START", 116);
    } else if (g_game.frame < 240u) {
        draw_text_centered("DPAD MOVE   START RESET", 214);
    }

    if (g_draw_overflow) {
        draw_text("RENDER LIMIT", 4, 14);
    }
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

int main(void) {
    SAT_PANIC_IF_ERROR(sat_app_init_default());
    SAT_PANIC_IF_ERROR(sat_ascii_font_init_8x8_indexed8(
        &g_font, SAT_COLOR_WHITE, 0x0000u, HUD_PALETTE));

    pac_game_init(&g_game, MAZE_X, MAZE_Y);
    build_wall_runs();

    while (1) {
        sat_pad_state_t pad = {0};
        SAT_PANIC_IF_ERROR(sat_app_frame_begin(COLOR_BLACK, COLOR_BLACK, &pad));

        pac_game_update(&g_game, &pad);

        render_maze();
        render_ghosts();
        render_pac();
        render_hud();

        SAT_PANIC_IF_ERROR(sat_app_frame_end());
    }

    return 0;
}
