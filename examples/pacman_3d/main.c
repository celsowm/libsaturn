/* pacman_3d - playable perspective Pac-Man for Sega Saturn (libsaturn).
 *
 * Same game as examples/pacman_2d, same code driving it: every rule lives in
 * examples/common/pacman_game.c and this file only decides how to look at it.
 * The maze pixels the 2D example treats as screen X and Y are treated here as
 * world X and Z.
 *
 * Everything in the scene is a flat-shaded polygon submitted to the VDP1
 * through the library's world-quad helpers (saturn/render3d.h):
 *   - maze walls are merged into runs, backface-culled, depth-sorted and
 *     drawn as solid quads shaded by which way they face;
 *   - pellets are small quads lying on the floor;
 *   - Pac-Man and the ghosts are camera-facing solid quads.
 * There are no textures, so nothing here needs an asset pipeline.
 *
 * The ground is a VDP2 RBG0 coefficient-table plane, which costs no VDP1
 * commands at all -- the 512-command list is reserved for the geometry.
 *
 * Controls: D-Pad to move, START to restart. No sound.
 */
#include <stdint.h>

#include "saturn/color.h"
#include "saturn/fmt.h"
#include "saturn/font.h"
#include "saturn/grid.h"
#include "saturn/input.h"
#include "saturn/math3d.h"
#include "saturn/render3d.h"
#include "saturn/vdp1.h"
#include "saturn/vdp2.h"
#include "saturn/video.h"
#include "saturn/example_util.h"

#include "../common/pacman_game.h"
#include "../vdp2_rbg0_ground/rbg0_math.h"

#define SCREEN_W 320
#define SCREEN_H 224
#define TILE kPacTilePx

#define WALL_HEIGHT 24

/* RBG0 ground plane */
#define RBG0_BITMAP_WIDTH 512u
#define RBG0_BITMAP_HEIGHT 256u
#define RBG0_HORIZON 96u
#define RBG0_FOCAL 96u
#define RBG0_MIN_DEPTH 8u
#define RBG0_GROUND_FORWARD 96u
#define RBG0_BITMAP_BASE_WORD 0x00000u
#define RBG0_ROT_BASE_WORD 0x10000u
#define RBG0_COEF_BASE_WORD 0x12000u
#define RBG0_PALETTE 0u

/* CRAM has 8 palette banks (0..7). The RBG0 ground owns bank 0, so the HUD
 * font gets bank 1. */
#define HUD_PALETTE 1u

/* 146 merged faces is the exact count for kPacMaze; the slack absorbs edits
 * to the maze art without silently dropping walls. Faces are indexed with a
 * uint8_t in the sort table, so this must stay below 256. */
#define MAX_WALL_FACES 192u

/* Pellets beyond this many world units are a pixel or less on screen and only
 * cost VDP1 commands. Walls are not distance-culled -- a missing wall reads as
 * a hole in the maze, while a missing far pellet reads as nothing at all. */
#define PELLET_DRAW_RANGE 120

enum { FACE_HORIZONTAL = 0, FACE_VERTICAL = 1 };

typedef struct wall_face {
    uint8_t axis;
    int8_t side;     /* -1 = faces the low side of `fixed`, +1 = the high side */
    uint8_t fixed;   /* row for horizontal faces, column for vertical ones */
    uint8_t start;
    uint8_t end;
} wall_face_t;

/* Colours */
#define COLOR_SKY     SAT_RGB555(2, 4, 10)
#define COLOR_WALL    SAT_RGB555(6, 11, 31)
#define COLOR_PELLET  SAT_RGB555(31, 24, 14)
#define COLOR_POWER   SAT_RGB555(31, 31, 31)
#define COLOR_PAC     SAT_RGB555(31, 30, 2)
#define COLOR_SHADOW  SAT_RGB555(1, 2, 6)

static const uint16_t kGhostColors[PAC_GHOST_COUNT] = {
    SAT_RGB555(31, 0, 0),
    SAT_RGB555(31, 18, 24),
    SAT_RGB555(0, 28, 31),
    SAT_RGB555(31, 20, 4),
};
#define COLOR_FRIGHT_A SAT_RGB555(4, 4, 31)
#define COLOR_FRIGHT_B SAT_RGB555(31, 31, 31)

/* Faces turned away from the light keep this fraction of their colour, so a
 * back-facing wall stays visible instead of reading as a hole. */
#define WALL_AMBIENT (SAT_FX16_ONE / 3)

static const rbg0_ground_config_t g_rbg0_cfg = {
    RBG0_BITMAP_WIDTH,
    RBG0_BITMAP_HEIGHT,
    SCREEN_W / 2u,
    RBG0_HORIZON,
    RBG0_FOCAL,
    RBG0_MIN_DEPTH,
    RBG0_GROUND_FORWARD,
    RBG0_COEF_BASE_WORD,
};

static pac_game_t g_game;
static sat_ascii_font_t g_font;

static wall_face_t g_faces[MAX_WALL_FACES];
static uint16_t g_face_count;
static uint8_t g_face_order[MAX_WALL_FACES];
static uint32_t g_face_depth[MAX_WALL_FACES];

static sat_mat4_t g_view_proj;
static sat_vec3_t g_cam_eye;
static int g_cam_dir = SAT_DIR_LEFT;  /* last direction Pac-Man actually faced */
static sat_fx16_t g_cam_right_x;
static sat_fx16_t g_cam_right_z;

/* Set when the VDP1 command list filled up, so the overflow is reported on
 * screen rather than appearing as geometry that silently vanishes. */
static int g_draw_overflow;

/* ------------------------------------------------------------------ */
/* RBG0 ground plane                                                   */
/* ------------------------------------------------------------------ */

static void upload_rbg0_palette(void) {
    uint16_t palette[256];
    int i;
    for (i = 0; i < 256; ++i) {
        palette[i] = 0;
    }
    palette[0] = SAT_RGB555(2, 4, 10);
    palette[1] = SAT_RGB555(8, 14, 24);
    palette[2] = SAT_RGB555(16, 24, 31);
    palette[3] = SAT_RGB555(28, 18, 10);
    sat_example_must(sat_vdp2_palette_upload(palette, 256u, RBG0_PALETTE));
}

/* A tile grid, so motion over the ground is readable even where no wall is
 * in view to give a sense of speed. */
static void upload_rbg0_bitmap(void) {
    volatile uint16_t* vram = (volatile uint16_t*)0x25E00000u;
    uint32_t off = RBG0_BITMAP_BASE_WORD;
    uint32_t y;
    uint32_t x;

    for (y = 0; y < RBG0_BITMAP_HEIGHT; ++y) {
        for (x = 0; x < RBG0_BITMAP_WIDTH; x += 2u) {
            uint8_t pix[2];
            uint32_t k;
            for (k = 0; k < 2u; ++k) {
                const uint32_t xx = x + k;
                uint8_t p = (((xx / 32u) + (y / 32u)) & 1u) ? 1u : 0u;
                if ((xx % 32u) == 0u || (y % 32u) == 0u) {
                    p = 2u;
                }
                pix[k] = p;
            }
            vram[off++] = (uint16_t)(((uint16_t)pix[0] << 8u) | pix[1]);
        }
    }
}

static void write_rbg0_coefficients(void) {
    volatile uint16_t* vram = (volatile uint16_t*)0x25E00000u;
    uint32_t y;
    for (y = 0; y < (uint32_t)SCREEN_H; ++y) {
        uint16_t w0;
        uint16_t w1;
        const uint32_t base = RBG0_COEF_BASE_WORD + (y * 2u);
        rbg0_ground_encode_coefficient(&g_rbg0_cfg, y, &w0, &w1);
        vram[base] = w0;
        vram[base + 1u] = w1;
    }
}

static void write_rbg0_params(int cam_x, int cam_z) {
    uint16_t params[48];
    rbg0_ground_build_params(&g_rbg0_cfg, cam_x, cam_z, params);
    sat_example_must(sat_vdp2_vram_write_words(RBG0_ROT_BASE_WORD, params, 48u));
}

static void init_rbg0(void) {
    sat_vdp2_rbg0_mode7_config_t cfg = {
        SAT_VDP2_RBG0_BITMAP_512x256,
        SAT_VDP2_COLOR_MODE_256,
        RBG0_BITMAP_BASE_WORD,
        RBG0_ROT_BASE_WORD,
        COLOR_SKY,
        6u,
        7u
    };
    upload_rbg0_palette();
    upload_rbg0_bitmap();
    write_rbg0_coefficients();
    sat_example_must(sat_vdp2_rbg0_mode7_init(&cfg));
    write_rbg0_params(0, 0);
}

/* ------------------------------------------------------------------ */
/* Wall face extraction                                                */
/* ------------------------------------------------------------------ */

static void add_face(int axis, int side, int fixed, int start, int end) {
    wall_face_t* face;
    if (g_face_count >= MAX_WALL_FACES || end < start) {
        return;
    }
    face = &g_faces[g_face_count++];
    face->axis = (uint8_t)axis;
    face->side = (int8_t)side;
    face->fixed = (uint8_t)fixed;
    face->start = (uint8_t)start;
    face->end = (uint8_t)end;
}

static int is_wall(int col, int row) {
    return pac_game_cell(&g_game, col, row) == '#';
}

/* Only wall tiles with open floor next to them produce a face, and adjacent
 * such tiles are merged into one quad. Interior wall tiles are never visible,
 * so emitting them would cost commands and draw nothing. */
static void build_wall_faces(void) {
    int fixed;
    int side;

    g_face_count = 0;

    for (fixed = 0; fixed < kPacMazeRows; ++fixed) {
        for (side = -1; side <= 1; side += 2) {
            int c = 0;
            while (c < kPacMazeCols) {
                if (is_wall(c, fixed) && !is_wall(c, fixed + side)) {
                    const int start = c;
                    while (c < kPacMazeCols && is_wall(c, fixed) &&
                           !is_wall(c, fixed + side)) {
                        ++c;
                    }
                    add_face(FACE_HORIZONTAL, side, fixed, start, c - 1);
                } else {
                    ++c;
                }
            }
        }
    }

    for (fixed = 0; fixed < kPacMazeCols; ++fixed) {
        for (side = -1; side <= 1; side += 2) {
            int r = 0;
            while (r < kPacMazeRows) {
                if (is_wall(fixed, r) && !is_wall(fixed + side, r)) {
                    const int start = r;
                    while (r < kPacMazeRows && is_wall(fixed, r) &&
                           !is_wall(fixed + side, r)) {
                        ++r;
                    }
                    add_face(FACE_VERTICAL, side, fixed, start, r - 1);
                } else {
                    ++r;
                }
            }
        }
    }
}

/* World-space plane the face lies on, and the span it covers. */
static int face_plane(const wall_face_t* face) {
    return ((int)face->fixed * TILE) + ((face->side > 0) ? TILE : 0);
}

static void face_center(const wall_face_t* face, int* out_x, int* out_z) {
    const int mid = (((int)face->start + (int)face->end + 1) * TILE) / 2;
    if (face->axis == FACE_HORIZONTAL) {
        *out_x = mid;
        *out_z = face_plane(face);
    } else {
        *out_x = face_plane(face);
        *out_z = mid;
    }
}

/* ------------------------------------------------------------------ */
/* Camera                                                              */
/* ------------------------------------------------------------------ */

static void update_camera(void) {
    sat_mat4_t view;
    sat_mat4_t projection;
    sat_vec3_t center;
    sat_vec3_t up = {0, SAT_FX16_ONE, 0};
    int fx;
    int fz;

    /* Pac-Man's direction goes to SAT_DIR_NONE whenever he is stopped against
     * a wall. Reusing the last real facing keeps the view from collapsing:
     * a zero forward vector makes look_at degenerate. */
    if (g_game.pac.dir != SAT_DIR_NONE) {
        g_cam_dir = g_game.pac.dir;
    }
    fx = sat_dir_dx(g_cam_dir);
    fz = sat_dir_dy(g_cam_dir);

    /* Right-hand perpendicular of the forward vector, used to spread the
     * billboards square-on to the camera. */
    g_cam_right_x = sat_fx16_from_int(fz);
    g_cam_right_z = sat_fx16_from_int(-fx);

    g_cam_eye.x = sat_fx16_from_int((int)g_game.pac.x - (fx * 48));
    g_cam_eye.y = sat_fx16_from_int(34);
    g_cam_eye.z = sat_fx16_from_int((int)g_game.pac.y - (fz * 48));
    center.x = sat_fx16_from_int((int)g_game.pac.x + (fx * 22));
    center.y = sat_fx16_from_int(5);
    center.z = sat_fx16_from_int((int)g_game.pac.y + (fz * 22));

    sat_example_must(sat_mat4_look_at(&view, &g_cam_eye, &center, &up));
    sat_example_must(sat_mat4_perspective(
        &projection,
        sat_fx16_from_int(65),
        sat_fx16_div(sat_fx16_from_int(SCREEN_W), sat_fx16_from_int(SCREEN_H)),
        sat_fx16_from_int(1),
        sat_fx16_from_int(500)));
    sat_example_must(sat_mat4_multiply(&g_view_proj, &projection, &view));
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */

static void submit_polygon(const sat_quad3_t* quad, uint16_t color) {
    const sat_result_t st = sat_draw_world_polygon(&g_view_proj, quad, color);
    /* SAT_ERR_UNSUPPORTED just means the quad is behind the camera. */
    if (st != SAT_OK && st != SAT_ERR_UNSUPPORTED) {
        g_draw_overflow = 1;
    }
}

static void draw_wall_face(const wall_face_t* face) {
    sat_quad3_t quad;
    const int plane = face_plane(face);
    const int a = (int)face->start * TILE;
    const int b = ((int)face->end + 1) * TILE;
    sat_fx16_t nx = 0;
    sat_fx16_t nz = 0;
    sat_fx16_t intensity;

    if (face->axis == FACE_HORIZONTAL) {
        /* Backface cull: the face is only visible from the side its open
         * floor is on. Skipping the other half of the maze's faces is what
         * keeps the command list in budget. */
        if ((sat_fx16_to_int(g_cam_eye.z) - plane) * face->side <= 0) {
            return;
        }
        nz = sat_fx16_from_int(face->side);
        sat_quad3_wall(
            &quad,
            sat_fx16_from_int(a), sat_fx16_from_int(plane),
            sat_fx16_from_int(b), sat_fx16_from_int(plane),
            sat_fx16_from_int(WALL_HEIGHT));
    } else {
        if ((sat_fx16_to_int(g_cam_eye.x) - plane) * face->side <= 0) {
            return;
        }
        nx = sat_fx16_from_int(face->side);
        sat_quad3_wall(
            &quad,
            sat_fx16_from_int(plane), sat_fx16_from_int(a),
            sat_fx16_from_int(plane), sat_fx16_from_int(b),
            sat_fx16_from_int(WALL_HEIGHT));
    }

    intensity = sat_face_intensity(nx, nz, WALL_AMBIENT);
    submit_polygon(&quad, sat_shade_rgb555(COLOR_WALL, intensity));
}

static void render_maze(void) {
    uint16_t i;

    for (i = 0; i < g_face_count; ++i) {
        int cx;
        int cz;
        face_center(&g_faces[i], &cx, &cz);
        g_face_order[i] = (uint8_t)i;
        g_face_depth[i] = sat_ground_distance_sq(
            g_cam_eye.x, g_cam_eye.z,
            sat_fx16_from_int(cx), sat_fx16_from_int(cz));
    }
    /* The VDP1 has no depth buffer: quads simply overwrite each other in list
     * order, so the list has to be built farthest-first. */
    sat_sort_indices_desc(g_face_order, g_face_depth, g_face_count);

    for (i = 0; i < g_face_count; ++i) {
        draw_wall_face(&g_faces[g_face_order[i]]);
    }
}

static void render_pellets(void) {
    const int cam_x = sat_fx16_to_int(g_cam_eye.x);
    const int cam_z = sat_fx16_to_int(g_cam_eye.z);
    int r;
    int c;

    for (r = 0; r < kPacMazeRows; ++r) {
        for (c = 0; c < kPacMazeCols; ++c) {
            const char cell = pac_game_cell(&g_game, c, r);
            sat_quad3_t quad;
            int x;
            int z;
            int dx;
            int dz;
            if (cell != '.' && cell != 'o') {
                continue;
            }
            x = sat_grid_tile_center_x(&g_game.grid, c);
            z = sat_grid_tile_center_y(&g_game.grid, r);
            dx = x - cam_x;
            dz = z - cam_z;
            if (dx > PELLET_DRAW_RANGE || dx < -PELLET_DRAW_RANGE ||
                dz > PELLET_DRAW_RANGE || dz < -PELLET_DRAW_RANGE) {
                continue;
            }
            /* Lifted a unit off the floor so it is not z-fighting the RBG0
             * plane at grazing angles. */
            sat_quad3_floor(
                &quad,
                sat_fx16_from_int(x),
                sat_fx16_from_int(1),
                sat_fx16_from_int(z),
                sat_fx16_from_int((cell == 'o') ? 3 : 1));
            submit_polygon(&quad, (cell == 'o') ? COLOR_POWER : COLOR_PELLET);
        }
    }
}

static void draw_actor(const sat_grid_actor_t* actor, uint16_t color) {
    sat_quad3_t quad;
    const sat_fx16_t wx = sat_fx16_from_int((int)actor->x);
    const sat_fx16_t wz = sat_fx16_from_int((int)actor->y);

    /* Floor shadow first: it anchors the billboard to the ground, which is
     * what stops a flat quad from looking like it is floating. */
    sat_quad3_floor(&quad, wx, sat_fx16_from_int(1), wz, sat_fx16_from_int(6));
    submit_polygon(&quad, COLOR_SHADOW);

    sat_quad3_billboard(
        &quad, wx, wz, g_cam_right_x, g_cam_right_z,
        sat_fx16_from_int(6), sat_fx16_from_int(18));
    submit_polygon(&quad, color);
}

static void render_actors(void) {
    int i;
    for (i = 0; i < PAC_GHOST_COUNT; ++i) {
        uint16_t color;
        if (pac_game_ghost_penned(&g_game, i)) {
            continue;
        }
        color = kGhostColors[i];
        if (pac_game_frightened(&g_game)) {
            color = ((g_game.frame / 6u) & 1u) ? COLOR_FRIGHT_A : COLOR_FRIGHT_B;
        }
        draw_actor(&g_game.ghosts[i].actor, color);
    }
    draw_actor(&g_game.pac, COLOR_PAC);
}

static void draw_text(const char* text, int x, int y) {
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(
        &g_font, text, x, y, 0, HUD_PALETTE, 0));
}

static void draw_text_centered(const char* text, int y) {
    sat_example_must(sat_ascii_font_draw_text_screen_centered_indexed8(
        &g_font, text, SCREEN_W / 2, y, 0, HUD_PALETTE, 0));
}

static void render_hud(void) {
    char text[24];
    int i;

    sat_example_must(sat_fmt_label_u32("SCORE ", g_game.score, text, sizeof(text), NULL));
    draw_text(text, 4, 2);

    draw_text("LIVES", 232, 2);
    for (i = 0; i < g_game.lives; ++i) {
        sat_example_must(sat_draw_rect_screen(
            (int16_t)(280 + (i * 10)), 3, 6, 6, COLOR_PAC));
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
/* Frame loop                                                          */
/* ------------------------------------------------------------------ */

/* Driven by hand rather than through sat_app_frame_begin, because that helper
 * sets an OPAQUE VDP1 erase which would paint over the VDP2 ground plane. */
static void frame_begin(sat_pad_state_t* pad) {
    SAT_PANIC_IF_ERROR(sat_wait_vblank());
    SAT_PANIC_IF_ERROR(sat_vdp2_back_color_set(COLOR_SKY));
    SAT_PANIC_IF_ERROR(sat_vdp1_set_erase_transparent());
    SAT_PANIC_IF_ERROR(sat_begin_frame());
    SAT_PANIC_IF_ERROR(sat_pad_poll(pad));
}

int main(void) {
    sat_video_config_t video = {SCREEN_W, SCREEN_H, 1u, 0u};

    SAT_PANIC_IF_ERROR(sat_init(&video));
    init_rbg0();
    SAT_PANIC_IF_ERROR(sat_ascii_font_init_8x8_indexed8(
        &g_font, SAT_COLOR_WHITE, 0x0000u, HUD_PALETTE));

    pac_game_init(&g_game, 0, 0);
    build_wall_faces();

    while (1) {
        sat_pad_state_t pad = {0};
        frame_begin(&pad);

        pac_game_update(&g_game, &pad);
        update_camera();

        write_rbg0_params((int)g_game.pac.x, (int)g_game.pac.y);
        SAT_PANIC_IF_ERROR(sat_vdp2_rbg0_commit());

        render_maze();
        render_pellets();
        render_actors();
        render_hud();

        SAT_PANIC_IF_ERROR(sat_end_frame());
    }

    return 0;
}
