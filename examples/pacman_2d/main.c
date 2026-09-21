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
 *   - pellets are small rectangles, and Pac-Man and the ghosts are 16x16
 *     indexed sprites generated at startup and drawn scaled down;
 *   - the HUD is the built-in 8x8 ASCII font.
 *
 * Controls: D-Pad to move, START to restart.
 */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/grid.h"
#include "saturn/hud.h"
#include "saturn/input.h"
#include "saturn/vdp1.h"
#include "saturn/sprite_anim.h"
#include "saturn/example_util.h"

#include "../common/pacman_game.h"

#define SCREEN_W 320
#define SCREEN_H 224

/* Centre the 224x200 maze on the 320x224 display. */
#define MAZE_X ((SCREEN_W - kPacMazePixelW) / 2)  /* 48 */
#define MAZE_Y ((SCREEN_H - kPacMazePixelH) / 2)  /* 12 */
#define TILE kPacTilePx

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
static sat_hud_t g_hud;

/* Set when the VDP1 command list fills up. The list holds 1024 commands and a
 * full round needs about 350, so this should never trigger -- but editing the
 * maze could push it over, and a silent panic loop would look like a crashed
 * console instead of telling anyone why. */
static int g_draw_overflow;

/* Wall run-length table, rebuilt only when a new stage loads: within a
 * stage the walls never move. */
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
/* Actor sprites                                                       */
/* ------------------------------------------------------------------ */
/* Pac-Man and the ghosts are 16x16 indexed sprites, drawn scaled down to
 * ACTOR_PX. VDP1 texture widths must be a multiple of 8, and 16 is the
 * smallest one with enough room for a recognisable ghost; 16 pixels on an
 * 8-pixel tile grid would swallow the corridor, so the draw is scaled.
 *
 * The pixels are generated at startup rather than stored. Pac-Man is a disc
 * with a wedge removed, which is three lines of arithmetic per pixel and
 * beats hand-editing twelve frames of art; the ghost is one shared outline
 * with its eyes stamped in afterwards. Nothing is kept in RAM but the texture
 * handles -- sat_tex_upload_indexed8 copies into VDP1 VRAM, so one scratch
 * buffer is reused for all thirty of them.
 */
#define SPRITE_DIM 16u
#define ACTOR_PX 12

/* One palette bank holds every colour the actors use. Keeping them in one
 * bank means a texture picks its colour by index, so the ghost outline is
 * uploaded once per colour instead of needing a bank each. */
#define ACTOR_PALETTE 1u
enum {
    PIX_NONE = 0,
    PIX_PAC = 1,
    PIX_EYE = 2,
    PIX_PUPIL = 3,
    PIX_GHOST0 = 4, /* ..7: one per ghost */
    PIX_FRIGHT = 8,
    PIX_FLASH = 9
};

/* Mouth openings, as the tangent of the wedge half-angle in hundredths.
 * Zero is a closed mouth and skips the wedge entirely. */
static const int kMouthTan[] = {0, 27, 70};
#define PAC_FRAMES 3

static uint8_t g_sprite[SPRITE_DIM * SPRITE_DIM];
static sat_vdp1_texture_t g_pac_tex[4][PAC_FRAMES];  /* [direction][mouth frame] */
static sat_vdp1_texture_t g_ghost_tex[PAC_GHOST_COUNT][4]; /* [ghost][direction]  */
static sat_vdp1_texture_t g_fright_tex[2];           /* blue, and the white flash */
static sat_sprite_anim_t g_pac_anim;
static sat_sprite_anim_t g_ghost_anim[PAC_GHOST_COUNT];
static uint16_t g_actor_palette[256];

/* The ghost outline: dome on top, straight sides, notched skirt. */
static const char* const kGhostArt[SPRITE_DIM] = {
    "................",
    ".....######.....",
    "...##########...",
    "..############..",
    ".##############.",
    ".##############.",
    ".##############.",
    ".##############.",
    ".##############.",
    ".##############.",
    ".##############.",
    ".##############.",
    ".##############.",
    ".##############.",
    ".###..####..###.",
    ".##....##....##."
};

static void sprite_fill(uint8_t value) {
    uint16_t i;
    for (i = 0; i < SPRITE_DIM * SPRITE_DIM; ++i) {
        g_sprite[i] = value;
    }
}

static void sprite_box(int x, int y, int w, int h, uint8_t value) {
    int py;
    int px;
    for (py = y; py < y + h; ++py) {
        for (px = x; px < x + w; ++px) {
            if (px >= 0 && px < (int)SPRITE_DIM && py >= 0 && py < (int)SPRITE_DIM) {
                g_sprite[(py * (int)SPRITE_DIM) + px] = value;
            }
        }
    }
}

/* Pac-Man: every pixel inside the disc, minus the pixels inside the mouth
 * wedge. Coordinates are doubled so the centre lands between pixels without
 * needing fractions, and the wedge test is a dot product against the facing
 * direction with a tangent bound instead of an arctangent. */
static void build_pac(int dir, int frame) {
    const int fx = sat_dir_dx(dir);
    const int fy = sat_dir_dy(dir);
    const int tan100 = kMouthTan[frame];
    int y;
    int x;

    sprite_fill(PIX_NONE);
    for (y = 0; y < (int)SPRITE_DIM; ++y) {
        for (x = 0; x < (int)SPRITE_DIM; ++x) {
            const int dx = (2 * x) - 15;
            const int dy = (2 * y) - 15;
            int dot;
            int cross;
            if ((dx * dx) + (dy * dy) > 232) {
                continue; /* outside the disc */
            }
            if (tan100 > 0) {
                dot = (dx * fx) + (dy * fy);
                cross = (dx * fy) - (dy * fx);
                if (cross < 0) {
                    cross = -cross;
                }
                if (dot > 0 && (cross * 100) <= (dot * tan100)) {
                    continue; /* inside the mouth */
                }
            }
            g_sprite[(y * (int)SPRITE_DIM) + x] = PIX_PAC;
        }
    }
}

static void build_ghost_outline(uint8_t body) {
    int y;
    int x;
    for (y = 0; y < (int)SPRITE_DIM; ++y) {
        for (x = 0; x < (int)SPRITE_DIM; ++x) {
            g_sprite[(y * (int)SPRITE_DIM) + x] =
                (kGhostArt[y][x] == '#') ? body : PIX_NONE;
        }
    }
}

/* Eyes looking where the ghost is going. Three pixels of white with a two
 * pixel pupil inside leaves exactly one pixel of travel per axis, which is
 * enough to read at this size. */
static void build_ghost(uint8_t body, int dir) {
    const int dx = sat_dir_dx(dir);
    const int dy = sat_dir_dy(dir);

    build_ghost_outline(body);
    sprite_box(3, 5, 3, 4, PIX_EYE);
    sprite_box(10, 5, 3, 4, PIX_EYE);
    sprite_box(4 + dx, 6 + dy, 2, 2, PIX_PUPIL);
    sprite_box(11 + dx, 6 + dy, 2, 2, PIX_PUPIL);
}

/* The frightened ghost has a face instead of eyes: two dots and a flat
 * zigzag mouth, the arcade's way of saying this one can be eaten. */
static void build_fright(uint8_t body, uint8_t face) {
    int x;
    build_ghost_outline(body);
    sprite_box(4, 6, 2, 2, face);
    sprite_box(10, 6, 2, 2, face);
    for (x = 3; x < 13; ++x) {
        sprite_box(x, ((x & 1) != 0) ? 10 : 11, 1, 1, face);
    }
}

static void upload_sprite(sat_vdp1_texture_t* out) {
    sat_example_must(sat_tex_upload_indexed8(
        out, g_sprite, SPRITE_DIM, SPRITE_DIM, g_actor_palette, ACTOR_PALETTE));
}

static void build_actor_sprites(void) {
    int i;
    int d;

    for (i = 0; i < 256; ++i) {
        g_actor_palette[i] = 0;
    }
    g_actor_palette[PIX_PAC] = COLOR_PAC;
    g_actor_palette[PIX_EYE] = SAT_RGB555(31, 31, 31);
    g_actor_palette[PIX_PUPIL] = SAT_RGB555(2, 2, 24);
    for (i = 0; i < PAC_GHOST_COUNT; ++i) {
        g_actor_palette[PIX_GHOST0 + i] = kGhostColors[i];
    }
    g_actor_palette[PIX_FRIGHT] = COLOR_FRIGHT_A;
    g_actor_palette[PIX_FLASH] = COLOR_FRIGHT_B;

    for (d = 0; d < 4; ++d) {
        for (i = 0; i < PAC_FRAMES; ++i) {
            build_pac(d, i);
            upload_sprite(&g_pac_tex[d][i]);
        }
        for (i = 0; i < PAC_GHOST_COUNT; ++i) {
            build_ghost((uint8_t)(PIX_GHOST0 + i), d);
            upload_sprite(&g_ghost_tex[i][d]);
        }
    }
    build_fright(PIX_FRIGHT, PIX_EYE);
    upload_sprite(&g_fright_tex[0]);
    build_fright(PIX_FLASH, PIX_FRIGHT);
    upload_sprite(&g_fright_tex[1]);
    sat_example_must(sat_sprite_anim_init(&g_pac_anim, &g_pac_tex[0][0], 4u, PAC_FRAMES));
    for (i = 0; i < PAC_GHOST_COUNT; ++i)
        sat_example_must(sat_sprite_anim_init(&g_ghost_anim[i], &g_ghost_tex[i][0], 4u, 1u));
}

static void draw_actor_sprite(const sat_vdp1_texture_t* tex, int cx, int cy) {
    if (sat_draw_sprite_scaled_screen(
            tex, (int16_t)cx, (int16_t)cy, ACTOR_PX, ACTOR_PX, 0) != SAT_OK) {
        g_draw_overflow = 1;
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
            if (cell == 'o' && ((g_game.frame / 10u) & 1u) != 0u) {
                continue; /* power pellets blink, as they always have */
            }
            draw_centered_box(
                sat_grid_tile_center_x(&g_game.grid, c),
                sat_grid_tile_center_y(&g_game.grid, r),
                (cell == 'o') ? POWER_HALF : PELLET_HALF,
                (cell == 'o') ? COLOR_POWER : COLOR_PELLET);
        }
    }
}

/* Pac-Man chews closed-half-open-half, four frames to the cycle, so the
 * animation reads as a mouth rather than a flicker. */
static void render_pac(void) {
    static const uint8_t kChewFrame[4] = {0, 1, 2, 1};
    const int dir = pac_game_facing(&g_game, PAC_SLOT_PAC);
    const uint8_t frame = kChewFrame[(g_game.frame / 4u) & 3u];
    sat_example_must(sat_sprite_anim_set(&g_pac_anim, (uint8_t)dir, frame));
    draw_actor_sprite(sat_sprite_anim_texture(&g_pac_anim),
        (int)g_game.pac.x, (int)g_game.pac.y);
}

static void render_ghosts(void) {
    int i;

    for (i = 0; i < PAC_GHOST_COUNT; ++i) {
        const sat_grid_actor_t* a = &g_game.ghosts[i].actor;
        const int dir = pac_game_facing(&g_game, i);
        const sat_vdp1_texture_t* tex;

        if (pac_game_ghost_penned(&g_game, i)) {
            continue;
        }
        if (pac_game_frightened(&g_game)) {
            /* Flashing white near the end is how the player sees the timer
             * running out instead of being surprised by it. */
            tex = &g_fright_tex[((g_game.frame / 6u) & 1u) ? 0u : 1u];
        } else {
            sat_example_must(sat_sprite_anim_set(&g_ghost_anim[i], (uint8_t)dir, 0u));
            tex = sat_sprite_anim_texture(&g_ghost_anim[i]);
        }
        draw_actor_sprite(tex, (int)a->x, (int)a->y);
    }
}

/* Thin wrappers around sat_hud_t, whose job is the same one draw_rect above
 * already does for rectangles: turn a full command list into the
 * RENDER LIMIT flag instead of a lost draw call, which sat_hud_t itself has
 * no opinion on. Font, palette and spacing live in g_hud, set up once in
 * main(). */
static void draw_text(const char* text, int x, int y) {
    if (sat_hud_text(&g_hud, text, x, y) != SAT_OK) {
        g_draw_overflow = 1;
    }
}

static void draw_text_centered(const char* text, int y) {
    if (sat_hud_text_centered(&g_hud, text, SCREEN_W / 2, y) != SAT_OK) {
        g_draw_overflow = 1;
    }
}

static void draw_value(const char* label, uint32_t value, int x, int y) {
    if (sat_hud_value(&g_hud, label, value, x, y) != SAT_OK) {
        g_draw_overflow = 1;
    }
}

static void render_hud(void) {
    const char* status;
    int i;

    draw_value("SCORE ", g_game.score, 4, 2);

    draw_text("LIVES", 232, 2);
    for (i = 0; i < g_game.lives; ++i) {
        draw_rect(280 + (i * 10), 3, 6, 6, COLOR_PAC);
    }

    /* Message and PRESS START prompt: shared with pacman_3d so the two
     * examples never say this differently. */
    status = pac_game_status_text(&g_game);
    if (status != NULL) {
        draw_text_centered(status, 104);
        if (pac_game_status_needs_start(&g_game)) {
            draw_text_centered("PRESS START", 116);
        }
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
    SAT_PANIC_IF_ERROR(sat_hud_init(&g_hud, &g_font, HUD_PALETTE, SAT_ASCII_FONT_GLYPH_WIDTH));

    pac_game_init(&g_game, MAZE_X, MAZE_Y);
    build_wall_runs();
    build_actor_sprites();

    while (1) {
        sat_pad_state_t pad = {0};
        SAT_PANIC_IF_ERROR(sat_app_frame_begin(COLOR_BLACK, COLOR_BLACK, &pad));

        pac_game_update(&g_game, &pad);
        if ((g_game.events & PAC_EVENT_STAGE_START) != 0u) {
            build_wall_runs();
        }

        render_maze();
        render_ghosts();
        render_pac();
        render_hud();

        SAT_PANIC_IF_ERROR(sat_app_frame_end());
    }

    return 0;
}
