/* pacman_3d - Pac-Man on a 3D board for Sega Saturn (libsaturn).
 *
 * Same game as examples/pacman_2d, same code driving it: every rule lives in
 * examples/common/pacman_game.c and this file only decides how to look at it.
 * The maze pixels the 2D example treats as screen X and Y are treated here as
 * world X and Z.
 *
 * The camera is a fixed high three-quarter view with the whole maze in frame,
 * the way a board game sits on a table. A chase camera behind Pac-Man was the
 * obvious thing to try and it reads badly here: at this scale the walls are a
 * few tiles away in every direction, so the view is mostly wall, and a game
 * about seeing where the ghosts are becomes a game about not seeing them.
 *
 * Everything in the scene is a solid, flat-shaded polygon submitted to the
 * VDP1, and all of it is built from the library's mesh primitives
 * (saturn/mesh3d.h) rather than from hand-written corner lists:
 *   - wall runs are merged into rectangles and drawn as boxes;
 *   - Pac-Man is a sphere, the ghosts are capped cylinders;
 *   - the board is one subdivided plane;
 *   - pellets are small flat quads.
 * Nothing in the 3D scene is textured. The only asset is the starfield on
 * the VDP2 background behind it.
 *
 * The camera can only be in one of sixteen positions around the board, and
 * that restriction is the whole performance story. From a given position the
 * maze, the board and the pellets always project to the same screen
 * coordinates, so they are projected ONCE -- culled, shaded and depth-sorted
 * there too -- and each frame only replays the corners through
 * sat_draw_quad2_polygon. Projection is four matrix transforms and two
 * 64-bit divides per corner, and this is about 1500 corners; doing it every
 * frame ran the board at roughly a fifth of full rate. Only Pac-Man and the
 * ghosts, which actually move, are projected live.
 *
 * Turning the camera therefore does not re-project anything: all sixteen
 * views are baked at startup, which takes about two seconds and is why there
 * is a progress bar, and turning just selects a different one.
 *
 * Controls: D-Pad to move, L and R to turn the camera, START to restart.
 * No sound.
 */
#include <stdint.h>

#include "saturn/color.h"
#include "saturn/fmt.h"
#include "saturn/font.h"
#include "saturn/grid.h"
#include "saturn/input.h"
#include "saturn/math3d.h"
#include "saturn/mesh3d.h"
#include "saturn/render3d.h"
#include "saturn/vdp1.h"
#include "saturn/vdp2.h"
#include "saturn/video.h"
#include "saturn/example_util.h"

#include "../common/pacman_game.h"
#include "pacman_3d/stars.h"

#define SCREEN_W 320
#define SCREEN_H 224
#define TILE kPacTilePx

#define BOARD_W (kPacMazeCols * TILE) /* 224 */
#define BOARD_D (kPacMazeRows * TILE) /* 200 */

/* Low walls, and the reason is not just that tall ones hide the board.
 *
 * The VDP1 has no depth buffer, so everything is ordered back-to-front per
 * object -- and a tall wall in the row NEARER the camera is drawn after the
 * corridor behind it, painting over anything standing there even though it
 * does not actually occlude it. At this camera angle a wall projects about
 * 1.3 screen pixels per unit of height while a whole tile row is only about
 * 7 pixels deep, so 12-unit walls reached two rows back and sliced pieces off
 * Pac-Man whenever he ran alongside one. Six units keeps a wall inside its
 * own row, which is what makes the per-object ordering sufficient. */
#define WALL_HEIGHT 6

/* Camera. Fixed, so these are the numbers that frame the board: from
 * 260 units up and 190 back, a 36-degree vertical field of view puts all four
 * corners of the maze on screen with the top 28 pixels left clear for the
 * HUD. Changing any one of them needs the other two checked. */
#define CAM_HEIGHT 260
#define CAM_BACK 190
#define CAM_FOV 36

/* Camera positions around the board, selectable with L and R.
 *
 * Sixteen of them, because the whole static scene is baked per angle (see
 * the baked-scene note below) and a baked angle costs about 10KB: sixteen
 * fits in work RAM with room to spare and turns in 22.5-degree steps, which
 * reads as turning rather than as snapping between four fixed views. */
#define CAM_ANGLES 16
#define CAM_STEP_DEGREES (360 / CAM_ANGLES)

#define HUD_PALETTE 0u

/* Colours */
#define COLOR_TABLE SAT_RGB555(0, 1, 3)
#define COLOR_BOARD SAT_RGB555(1, 2, 7)
#define COLOR_WALL SAT_RGB555(6, 11, 31)
#define COLOR_PELLET SAT_RGB555(31, 24, 14)
#define COLOR_POWER SAT_RGB555(31, 31, 31)
#define COLOR_PAC SAT_RGB555(31, 30, 2)
#define COLOR_MOUTH SAT_RGB555(2, 2, 5)

static const uint16_t kGhostColors[PAC_GHOST_COUNT] = {
    SAT_RGB555(31, 0, 0),
    SAT_RGB555(31, 18, 24),
    SAT_RGB555(0, 28, 31),
    SAT_RGB555(31, 20, 4),
};
#define COLOR_FRIGHT_A SAT_RGB555(4, 4, 31)
#define COLOR_FRIGHT_B SAT_RGB555(31, 31, 31)
/* Frames of fright left when the warning flash starts. */
#define PAC_FRIGHT_WARNING 120u

/* Faces turned away from the light keep this fraction of their colour, so a
 * shadowed side stays readable instead of reading as a hole. */
#define AMBIENT (SAT_FX16_ONE / 3)

/* ------------------------------------------------------------------ */
/* Baked scene                                                         */
/* ------------------------------------------------------------------ */
/* The VDP1 has no depth buffer: quads overwrite each other in list order, so
 * everything in the scene has to go into ONE back-to-front ordering. Sorting
 * walls and pellets separately looks right until a pellet sits just in front
 * of a wall, at which point the wall paints over it.
 *
 * The static half of that ordering is fixed for the life of the program, so
 * it is computed once into g_baked and then just walked. Pellets stay in the
 * list after they are eaten and are skipped by looking at the maze, which
 * avoids re-sorting anything when the board changes. */
#define MAX_BAKED 448u
#define MAX_WALL_RECTS 96u
/* The board is a flat, evenly lit surface, so subdividing it buys nothing
 * but VDP1 commands -- and on this scene every command counts: 400 of them
 * per frame is already about one frame of SH-2 time. */
#define BOARD_SEGMENTS 1u
#define MAX_BOARD_QUADS 4u

/* Marks a baked quad that is always drawn, as opposed to a pellet that is
 * only drawn while its maze cell still holds one. */
#define BAKED_ALWAYS 0xFFu

typedef struct wall_rect {
    uint8_t col;
    uint8_t row;
    uint8_t cols;
    uint8_t rows;
} wall_rect_t;

typedef struct baked_quad {
    sat_quad2_t quad;
    uint32_t depth; /* squared ground distance from the camera */
    uint16_t color;
    uint8_t cell_col; /* BAKED_ALWAYS, or the pellet cell to test */
    uint8_t cell_row;
} baked_quad_t;

/* Pac-Man and the ghosts move, so they are projected live and merged into the
 * baked order by depth. */
typedef struct actor_draw {
    uint32_t depth;
    int16_t x;
    int16_t z;
    uint16_t color;
    uint8_t is_pac;
    int8_t dir; /* facing, for Pac-Man's mouth */
} actor_draw_t;

/* Chew cycle: closed, half, open, half. */
static const uint8_t kPacChew[4] = {0u, 1u, 2u, 1u};

/* Scratch mesh storage, sized for the largest primitive drawn here, the
 * Pac-Man sphere. sat_mesh_sphere_counts gives the exact numbers at runtime;
 * these have to be compile-time constants, so sat_mesh_build_* is left to
 * report SAT_ERR_CAPACITY if they are ever made too small. */
#define PAC_SPHERE_SEGMENTS 12u
#define PAC_SPHERE_RINGS 3u
#define GHOST_SEGMENTS 6u
#define MESH_VERTEX_CAP 56u
#define MESH_FACE_CAP 40u

static pac_game_t g_game;
static sat_ascii_font_t g_font;

static wall_rect_t g_rects[MAX_WALL_RECTS];
static uint16_t g_rect_count;

/* One baked scene per camera angle. This is the big allocation in the
 * program -- about 170KB -- and it is what makes turning free: the
 * alternative, re-projecting the maze when the angle changes, costs a full
 * unbaked frame every time and would stutter for as long as a button is
 * held. Baking all of them at startup pays that cost once. */
static baked_quad_t g_baked[CAM_ANGLES][MAX_BAKED];
static uint16_t g_baked_count[CAM_ANGLES];

/* The board is under everything and never overlaps itself, so it is drawn
 * first as a block rather than taking part in the depth ordering. */
static sat_quad2_t g_board[CAM_ANGLES][MAX_BOARD_QUADS];
static uint16_t g_board_colors[CAM_ANGLES][MAX_BOARD_QUADS];
static uint16_t g_board_count[CAM_ANGLES];

static sat_vec3_t g_mesh_vertices[MESH_VERTEX_CAP];
static uint16_t g_mesh_indices[MESH_FACE_CAP * 4u];
static sat_mesh_t g_mesh;
static uint8_t g_mesh_order[MESH_FACE_CAP];
static uint32_t g_mesh_depth[MESH_FACE_CAP];

static sat_mat4_t g_view_proj;
static sat_vec3_t g_cam_eye;

/* Which camera angle is live, and the trig for it. The two sines are kept
 * because the ghosts' eye panels have to face the camera, which stops being
 * a constant direction once the camera can turn. */
static uint16_t g_angle;
static uint16_t g_bake_angle;
static sat_fx16_t g_cam_sin;
static sat_fx16_t g_cam_cos;

/* Set when the VDP1 command list filled up, so the overflow is reported on
 * screen rather than appearing as geometry that silently vanishes. */
static int g_draw_overflow;

/* ------------------------------------------------------------------ */
/* Wall rectangles                                                     */
/* ------------------------------------------------------------------ */

static int is_wall(int col, int row) {
    return pac_game_cell(&g_game, col, row) == '#';
}

/* Greedy merge of wall tiles into maximal rectangles: run right along a row,
 * then extend down as far as every column of the run stays wall.
 *
 * One box per rectangle instead of one per tile is the difference between
 * about 60 solids and about 200, and at ~400 polygons per SH-2 frame that is
 * the difference between the board fitting a frame and not. */
static void build_wall_rects(void) {
    uint8_t used[kPacMazeRows][kPacMazeCols];
    int row;
    int col;

    g_rect_count = 0;
    for (row = 0; row < kPacMazeRows; ++row) {
        for (col = 0; col < kPacMazeCols; ++col) {
            used[row][col] = 0u;
        }
    }

    for (row = 0; row < kPacMazeRows; ++row) {
        col = 0;
        while (col < kPacMazeCols) {
            int end;
            int bottom;
            int r;
            int c;

            if (!is_wall(col, row) || used[row][col]) {
                ++col;
                continue;
            }
            end = col;
            while (end < kPacMazeCols && is_wall(end, row) && !used[row][end]) {
                ++end;
            }
            bottom = row + 1;
            while (bottom < kPacMazeRows) {
                int ok = 1;
                for (c = col; c < end; ++c) {
                    if (!is_wall(c, bottom) || used[bottom][c]) {
                        ok = 0;
                        break;
                    }
                }
                if (!ok) {
                    break;
                }
                ++bottom;
            }
            for (r = row; r < bottom; ++r) {
                for (c = col; c < end; ++c) {
                    used[r][c] = 1u;
                }
            }
            if (g_rect_count < MAX_WALL_RECTS) {
                wall_rect_t* rect = &g_rects[g_rect_count++];
                rect->col = (uint8_t)col;
                rect->row = (uint8_t)row;
                rect->cols = (uint8_t)(end - col);
                rect->rows = (uint8_t)(bottom - row);
            }
            col = end;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Camera                                                             */
/* ------------------------------------------------------------------ */

static sat_fx16_t fx_abs(sat_fx16_t v) {
    return (v < 0) ? -v : v;
}

/* How much further back the camera has to sit at this angle for the whole
 * board to stay on screen.
 *
 * The maze is 224 by 200, so it is not square, and a rectangle seen corner-on
 * is wider than the same rectangle seen face-on -- 300 units across the
 * diagonal against 224 across the front. A camera distance that frames the
 * front view crops the corners of the diagonal ones.
 *
 * Holding the distance at the worst case instead would shrink the front view,
 * the one the game is mostly played in, by a quarter for the benefit of the
 * four diagonals. Since every angle is baked separately anyway, each one can
 * simply have the distance that frames it: the board stays about the same
 * size on screen whichever way it is turned, which is the thing the eye
 * actually tracks. The cost is that the camera visibly pulls back and in
 * again while turning, rather than swinging at a fixed radius.
 *
 * Both axes matter, and taking only the width is not enough: turned a
 * quarter of the way round, the maze is NARROWER than it started (200
 * against 224) but DEEPER by the same swap, and it was the depth that ran
 * off the bottom of the screen. */
static sat_fx16_t frame_scale(sat_fx16_t sin_az, sat_fx16_t cos_az) {
    const sat_fx16_t width = sat_fx16_from_int(BOARD_W);
    const sat_fx16_t depth = sat_fx16_from_int(BOARD_D);
    const sat_fx16_t abs_sin = fx_abs(sin_az);
    const sat_fx16_t abs_cos = fx_abs(cos_az);
    /* Bounding box of the rotated board, along the camera's axes. */
    const sat_fx16_t span_x =
        sat_fx16_mul(width, abs_cos) + sat_fx16_mul(depth, abs_sin);
    const sat_fx16_t span_z =
        sat_fx16_mul(width, abs_sin) + sat_fx16_mul(depth, abs_cos);
    const sat_fx16_t need_x = sat_fx16_div(span_x, width);
    const sat_fx16_t need_z = sat_fx16_div(span_z, depth);
    /* Normalised so that angle 0 comes out at exactly 1.0, which keeps the
     * straight-on view identical to the fixed camera this replaced. */
    return (need_x > need_z) ? need_x : need_z;
}

/* Points the camera at the board from `angle`, and leaves g_view_proj and
 * g_cam_eye describing it. Called once per angle while baking, and again
 * whenever the player turns -- never per frame. */
static void set_camera(uint16_t angle) {
    sat_mat4_t view;
    sat_mat4_t projection;
    sat_vec3_t center;
    sat_vec3_t up = {0, SAT_FX16_ONE, 0};
    const sat_fx16_t degrees = sat_fx16_from_int((int)angle * CAM_STEP_DEGREES);
    sat_fx16_t scale;

    g_cam_sin = sat_sin_deg(degrees);
    g_cam_cos = sat_cos_deg(degrees);
    scale = frame_scale(g_cam_sin, g_cam_cos);

    /* Angle 0 is the original fixed camera: straight down the +Z axis,
     * behind the bottom edge of the maze. */
    g_cam_eye.x = sat_fx16_from_int(BOARD_W / 2) +
                  sat_fx16_mul(sat_fx16_mul(sat_fx16_from_int(CAM_BACK), scale), g_cam_sin);
    g_cam_eye.y = sat_fx16_mul(sat_fx16_from_int(CAM_HEIGHT), scale);
    g_cam_eye.z = sat_fx16_from_int(BOARD_D / 2) +
                  sat_fx16_mul(sat_fx16_mul(sat_fx16_from_int(CAM_BACK), scale), g_cam_cos);

    center.x = sat_fx16_from_int(BOARD_W / 2);
    center.y = 0;
    center.z = sat_fx16_from_int(BOARD_D / 2);

    sat_example_must(sat_mat4_look_at(&view, &g_cam_eye, &center, &up));
    sat_example_must(sat_mat4_perspective(
        &projection,
        sat_fx16_from_int(CAM_FOV),
        sat_fx16_div(sat_fx16_from_int(SCREEN_W), sat_fx16_from_int(SCREEN_H)),
        sat_fx16_from_int(1),
        sat_fx16_from_int(1200)));
    sat_example_must(sat_mat4_multiply(&g_view_proj, &projection, &view));
}

/* ------------------------------------------------------------------ */
/* Baking the static scene                                             */
/* ------------------------------------------------------------------ */

static void note(sat_result_t status) {
    /* SAT_ERR_UNSUPPORTED just means the geometry is behind the camera. */
    if (status != SAT_OK && status != SAT_ERR_UNSUPPORTED) {
        g_draw_overflow = 1;
    }
}

/* Twice the area of a projected quad, by the shoelace formula. Sign depends
 * on winding, so callers take the magnitude. */
static int32_t quad_area2(const sat_quad2_t* q) {
    int32_t sum = 0;
    int i;
    for (i = 0; i < 4; ++i) {
        const int j = (i + 1) & 3;
        sum += ((int32_t)q->x[i] * (int32_t)q->y[j]) - ((int32_t)q->x[j] * (int32_t)q->y[i]);
    }
    return (sum < 0) ? -sum : sum;
}

/* Quads smaller than this many square pixels are dropped at bake time.
 *
 * A box side face seen nearly edge-on covers no pixels but still costs a
 * VDP1 command and the work of building it, and with the camera above the
 * middle of the board most of the maze's side faces are in exactly that
 * position. Two square pixels is below the smallest pellet, which projects to
 * about five -- and that is the ceiling on this threshold, not a safety
 * margin: at eight square pixels it starts eating the pellets themselves. */
#define MIN_QUAD_AREA2 4

static uint32_t depth_at(int x, int z) {
    return sat_ground_distance_sq(
        g_cam_eye.x, g_cam_eye.z, sat_fx16_from_int(x), sat_fx16_from_int(z));
}

/* Projects one face of the scratch mesh and files it in the baked list.
 * `x`/`z` are the world position the quad sorts by; `col`/`row` are
 * BAKED_ALWAYS for permanent geometry, or the maze cell a pellet belongs to. */
static void bake_face(uint16_t face, uint16_t color, int x, int z, int col, int row) {
    baked_quad_t* slot;
    sat_quad3_t quad;

    if (g_baked_count[g_bake_angle] >= MAX_BAKED) {
        g_draw_overflow = 1;
        return;
    }
    if (sat_mesh_face_quad(&g_mesh, face, &quad) != SAT_OK) {
        return;
    }
    slot = &g_baked[g_bake_angle][g_baked_count[g_bake_angle]];
    if (sat_project_quad(&g_view_proj, &quad, &slot->quad) != SAT_OK) {
        return; /* behind the camera: nothing to replay later */
    }
    if (quad_area2(&slot->quad) < MIN_QUAD_AREA2) {
        return;
    }
    slot->color = color;
    slot->depth = depth_at(x, z);
    slot->cell_col = (uint8_t)col;
    slot->cell_row = (uint8_t)row;
    ++g_baked_count[g_bake_angle];
}

static uint16_t shade_face(uint16_t face, uint16_t color) {
    sat_vec3_t normal;
    /* The scaled normal is enough: sat_face_intensity3_scaled divides the dot
     * product by the length instead of needing a unit vector. */
    if (sat_mesh_face_normal_scaled(&g_mesh, face, &normal) != SAT_OK) {
        return color;
    }
    return sat_shade_rgb555(color, sat_face_intensity3_scaled(&normal, AMBIENT));
}

/* The board sits under everything and never overlaps itself, so it is drawn
 * first as a block and takes no part in the depth ordering. */
static void bake_board(void) {
    sat_vec3_t center;
    uint16_t i;

    center.x = sat_fx16_from_int(BOARD_W / 2);
    center.y = 0;
    center.z = sat_fx16_from_int(BOARD_D / 2);
    note(sat_mesh_build_plane(
        &g_mesh,
        &center,
        sat_fx16_from_int((BOARD_W / 2) + TILE),
        sat_fx16_from_int((BOARD_D / 2) + TILE),
        BOARD_SEGMENTS,
        BOARD_SEGMENTS));

    g_board_count[g_bake_angle] = 0;
    for (i = 0; i < g_mesh.face_count &&
                g_board_count[g_bake_angle] < MAX_BOARD_QUADS; ++i) {
        sat_quad3_t quad;
        const uint16_t slot = g_board_count[g_bake_angle];
        if (sat_mesh_face_quad(&g_mesh, i, &quad) != SAT_OK) {
            continue;
        }
        if (sat_project_quad(&g_view_proj, &quad, &g_board[g_bake_angle][slot]) != SAT_OK) {
            continue;
        }
        g_board_colors[g_bake_angle][slot] = shade_face(i, COLOR_BOARD);
        ++g_board_count[g_bake_angle];
    }
}

static void bake_walls(void) {
    uint16_t r;

    for (r = 0; r < g_rect_count; ++r) {
        const wall_rect_t* rect = &g_rects[r];
        const int half_x = ((int)rect->cols * TILE) / 2;
        const int half_z = ((int)rect->rows * TILE) / 2;
        sat_vec3_t center;
        uint16_t f;

        center.x = sat_fx16_from_int(((int)rect->col * TILE) + half_x);
        center.y = sat_fx16_from_int(WALL_HEIGHT / 2);
        center.z = sat_fx16_from_int(((int)rect->row * TILE) + half_z);
        note(sat_mesh_build_box(
            &g_mesh,
            &center,
            sat_fx16_from_int(half_x),
            sat_fx16_from_int(WALL_HEIGHT / 2),
            sat_fx16_from_int(half_z)));

        for (f = 0; f < g_mesh.face_count; ++f) {
            sat_vec3_t face_center;
            /* The camera cannot move within an angle, so a face turned away
             * from it is turned away for as long as this baked view is the
             * live one: culling here costs nothing again. Roughly half of
             * every box goes, which is what keeps the whole maze inside one
             * frame of polygon submission. */
            if (!sat_mesh_face_visible(&g_mesh, f, &g_cam_eye)) {
                continue;
            }
            if (sat_mesh_face_center(&g_mesh, f, &face_center) != SAT_OK) {
                continue;
            }
            bake_face(
                f,
                shade_face(f, COLOR_WALL),
                sat_fx16_to_int(face_center.x),
                sat_fx16_to_int(face_center.z),
                BAKED_ALWAYS,
                BAKED_ALWAYS);
        }
    }
}

static void bake_pellets(void) {
    int r;
    int c;

    for (r = 0; r < kPacMazeRows; ++r) {
        for (c = 0; c < kPacMazeCols; ++c) {
            const char cell = pac_game_cell(&g_game, c, r);
            sat_quad3_t quad;
            int x;
            int z;
            if (cell != '.' && cell != 'o') {
                continue;
            }
            x = sat_grid_tile_center_x(&g_game.grid, c);
            z = sat_grid_tile_center_y(&g_game.grid, r);
            sat_quad3_floor(
                &quad,
                sat_fx16_from_int(x),
                /* Lifted a unit off the board so it is not z-fighting the
                 * floor plane at this grazing angle. */
                sat_fx16_from_int(1),
                sat_fx16_from_int(z),
                sat_fx16_from_int((cell == 'o') ? 3 : 1));
            sat_mesh_clear(&g_mesh);
            if (sat_mesh_add_quad(&g_mesh, &quad) != SAT_OK) {
                continue;
            }
            bake_face(0u, (cell == 'o') ? COLOR_POWER : COLOR_PELLET, x, z, c, r);
        }
    }
}

/* Insertion sort, farthest first. It runs once, at startup, on a few hundred
 * entries -- which is why the quadratic form is fine, and why the library's
 * sat_sort_indices_desc (capped at 255 entries by its uint8 index) is not
 * what this uses. */
static void sort_baked(void) {
    baked_quad_t* const list = g_baked[g_bake_angle];
    const uint16_t count = g_baked_count[g_bake_angle];
    uint16_t i;

    for (i = 1u; i < count; ++i) {
        const baked_quad_t value = list[i];
        int j = (int)i - 1;
        while (j >= 0 && list[j].depth < value.depth) {
            list[j + 1] = list[j];
            --j;
        }
        list[j + 1] = value;
    }
}

/* Baking sixteen angles takes about a hundred frames, and a program that
 * shows nothing for the first two seconds looks like one that has hung. The
 * bar costs sixteen extra frames and removes that whole class of false
 * alarm -- worth it for an example, whose readers are precisely the people
 * who cannot tell a slow start from a crash. */
static void draw_text_centered(const char* text, int y);

static void bake_progress(uint16_t done) {
    const int16_t width = (int16_t)((200 * (int)done) / CAM_ANGLES);

    SAT_PANIC_IF_ERROR(sat_wait_vblank());
    SAT_PANIC_IF_ERROR(sat_vdp1_set_erase_transparent());
    SAT_PANIC_IF_ERROR(sat_begin_frame());
    draw_text_centered("BUILDING CAMERA VIEWS", 100);
    SAT_PANIC_IF_ERROR(sat_draw_rect_screen(60, 116, 200, 6, COLOR_BOARD));
    if (width > 0) {
        SAT_PANIC_IF_ERROR(sat_draw_rect_screen(60, 116, width, 6, COLOR_PAC));
    }
    SAT_PANIC_IF_ERROR(sat_end_frame());
}

static void bake_scene(void) {
    for (g_bake_angle = 0u; g_bake_angle < CAM_ANGLES; ++g_bake_angle) {
        bake_progress(g_bake_angle);
        set_camera(g_bake_angle);
        g_baked_count[g_bake_angle] = 0;
        bake_board();
        bake_walls();
        bake_pellets();
        sort_baked();
    }
    g_bake_angle = 0u;
    set_camera(g_angle);
}

/* ------------------------------------------------------------------ */
/* Per-frame drawing                                                   */
/* ------------------------------------------------------------------ */

static void draw_board(void) {
    uint16_t i;
    for (i = 0; i < g_board_count[g_angle]; ++i) {
        note(sat_draw_quad2_polygon(&g_board[g_angle][i], g_board_colors[g_angle][i]));
    }
}

/* ------------------------------------------------------------------ */
/* Actors                                                              */
/* ------------------------------------------------------------------ */
/* Actors are around fourteen pixels across at this camera distance, so the
 * things that make one readable are, in order: colour, a face, and only then
 * silhouette. That ordering is why the ghosts get eyes before they get a
 * rounder body: at this size a face reads and a silhouette barely does.
 *
 * Pac-Man's mouth is the exception, and it is real geometry -- a sector cut
 * out of the sphere, not a dark wedge drawn over one. A wedge laid on top
 * only looks right from directly above, and the camera can now be turned
 * until the sphere is seen edge-on, where the cut shows in the silhouette
 * and a decal would be a flat smear across the face. */

/* The mouth is a sector missing from the sphere, measured in longitude bands.
 *
 * PAC_SPHERE_SEGMENTS is 12 so a band boundary lands exactly on each of the
 * four directions an actor can face: 90 degrees is three bands. An even gap
 * centred on that boundary is therefore symmetric about the way Pac-Man is
 * going, which an odd one would not be -- hence 0, 2 and 4 rather than a
 * smooth count. */
static const uint8_t kMouthGap[] = {0u, 2u, 4u};

#define ACTOR_Y 6      /* centre height of every actor */
#define PAC_RADIUS 7
#define GHOST_RADIUS 5
#define GHOST_HALF_H 6

/* Small: the eye panel sits a unit in front of the body, so perspective makes
 * it slightly larger than its world size, and a ghost is only about fourteen
 * pixels wide. At half-width 2 the two eyes merged into one white band across
 * the whole face. */
#define EYE_Y 9
#define EYE_HALF_W 1
#define EYE_HALF_H 1
#define EYE_SPREAD 2

#define COLOR_EYE SAT_RGB555(31, 31, 31)

static void submit_mesh(uint16_t color, const uint16_t* face_colors) {
    /* Zeroed: the fields this does not set -- textures, face_texture_indices,
     * order16, screen -- must read as absent, not as stack garbage that
     * sat_draw_mesh would dereference. */
    sat_mesh_draw_t draw = {0};
    draw.view_proj = &g_view_proj;
    draw.eye = g_cam_eye;
    draw.color = color;
    draw.face_colors = face_colors;
    draw.ambient = AMBIENT;
    draw.flags = SAT_MESH_CULL_BACKFACE | SAT_MESH_SORT | SAT_MESH_SHADE;
    draw.order = g_mesh_order;
    draw.depth = g_mesh_depth;
    note(sat_draw_mesh(&g_mesh, &draw));
}

/* Paints the inside of the mouth dark.
 *
 * sat_mesh_build_sphere_wedge puts the two cut walls last, so the table is
 * just "body colour everywhere, mouth colour for the tail". Without this the
 * opening is the same yellow as the rest of him and the notch reads as a
 * shading artefact rather than a mouth. */
static const uint16_t* mouth_colors(uint16_t gap) {
    static uint16_t colors[MESH_FACE_CAP];
    const uint16_t walls = (uint16_t)((gap > 0u) ? (2u * PAC_SPHERE_RINGS) : 0u);
    const uint16_t total = g_mesh.face_count;
    uint16_t i;

    if (walls == 0u || total > MESH_FACE_CAP) {
        return NULL;
    }
    for (i = 0; i < total; ++i) {
        colors[i] = (i >= (uint16_t)(total - walls)) ? COLOR_MOUTH : COLOR_PAC;
    }
    return colors;
}

/* Quarter turns from +Z to the way an actor is facing. Longitude 0 in the
 * mesh builders points along +Z, and the maze's +Z is south. */
static int facing_quarter(int dir) {
    const int dx = sat_dir_dx(dir);
    const int dz = sat_dir_dy(dir);
    if (dz > 0) {
        return 0; /* +Z */
    }
    if (dx > 0) {
        return 1; /* +X */
    }
    if (dz < 0) {
        return 2; /* -Z */
    }
    return 3; /* -X */
}

/* Builds Pac-Man: a sphere with a sector genuinely missing.
 *
 * The sector is a lune between two meridians, so the bite is taken out in
 * PLAN view -- which is the right choice for a camera looking down at a
 * board, and is what makes the silhouette read as Pac-Man rather than as a
 * ball. Tipping the mouth over to open up-and-down, the way a character in a
 * third-person game would, was tried and looks worse from up here: the upper
 * jaw hides the opening from any camera above it.
 *
 * The opening is only ever fully visible when he is facing towards or away
 * from the camera. Running along a corridor it shows as a notch in the
 * outline, which is honest -- his cheek really is in the way. */
static void build_pac(const actor_draw_t* actor, uint16_t gap) {
    sat_vec3_t center;
    const int quarter = facing_quarter((int)actor->dir);
    /* Band boundaries sit every 360/12 = 30 degrees and each quarter turn is
     * three of them, so the gap centres exactly on the facing when it starts
     * half a gap earlier. */
    const uint16_t start = (uint16_t)(((quarter * (int)PAC_SPHERE_SEGMENTS / 4)
                                       - ((int)gap / 2)
                                       + (int)PAC_SPHERE_SEGMENTS)
                                      % (int)PAC_SPHERE_SEGMENTS);

    center.x = sat_fx16_from_int(actor->x);
    center.y = sat_fx16_from_int(ACTOR_Y);
    center.z = sat_fx16_from_int(actor->z);
    note(sat_mesh_build_sphere_wedge(
        &g_mesh,
        &center,
        sat_fx16_from_int(PAC_RADIUS),
        PAC_SPHERE_SEGMENTS,
        PAC_SPHERE_RINGS,
        start,
        gap));
}

/* White on a coloured body, dark on the white flash: an eye has to contrast
 * with whatever it is sitting on. */
static uint16_t eye_color(uint16_t body) {
    return (body == COLOR_FRIGHT_B) ? COLOR_MOUTH : COLOR_EYE;
}

/* One eye: a small upright panel on the face of the ghost nearest the camera.
 *
 * This used to be a quad at a fixed -Z offset, because the camera could not
 * turn and "nearest the camera" was therefore always the same direction. Now
 * that it can, the panel has to be placed along the horizontal direction back
 * towards the camera, and spread along the perpendicular of that -- otherwise
 * the eyes slide round the side of the head as the board turns, and at 90
 * degrees they disappear behind it.
 *
 * It stays a flat quad rather than becoming a proper billboard with its own
 * orientation: the camera direction is taken as constant across the board
 * (it is 200-odd units away and the board is 224 across, so the error is a
 * few degrees) and the panel is only four pixels wide. */
static void draw_eye(const actor_draw_t* actor, int offset) {
    /* Towards the camera on the ground plane, and its left perpendicular. */
    const sat_fx16_t toward_x = g_cam_sin;
    const sat_fx16_t toward_z = g_cam_cos;
    const sat_fx16_t right_x = g_cam_cos;
    const sat_fx16_t right_z = -g_cam_sin;
    const sat_fx16_t spread = sat_fx16_from_int(offset);
    const sat_fx16_t lift = sat_fx16_from_int(GHOST_RADIUS + 1);
    const sat_fx16_t half_w = sat_fx16_from_int(EYE_HALF_W);

    /* Centre of this eye: out from the body towards the camera, then along
     * the camera's right by the eye spacing. */
    const sat_fx16_t cx = sat_fx16_from_int(actor->x) +
                          sat_fx16_mul(toward_x, lift) +
                          sat_fx16_mul(right_x, spread);
    const sat_fx16_t cz = sat_fx16_from_int(actor->z) +
                          sat_fx16_mul(toward_z, lift) +
                          sat_fx16_mul(right_z, spread);
    const sat_fx16_t dx = sat_fx16_mul(right_x, half_w);
    const sat_fx16_t dz = sat_fx16_mul(right_z, half_w);
    const sat_fx16_t y0 = sat_fx16_from_int(EYE_Y + EYE_HALF_H);
    const sat_fx16_t y1 = sat_fx16_from_int(EYE_Y - EYE_HALF_H);
    sat_quad3_t quad;

    quad.v[0].x = cx - dx; quad.v[0].y = y0; quad.v[0].z = cz - dz;
    quad.v[1].x = cx + dx; quad.v[1].y = y0; quad.v[1].z = cz + dz;
    quad.v[2].x = cx + dx; quad.v[2].y = y1; quad.v[2].z = cz + dz;
    quad.v[3].x = cx - dx; quad.v[3].y = y1; quad.v[3].z = cz - dz;
    note(sat_draw_world_polygon(&g_view_proj, &quad, eye_color(actor->color)));
}

static void draw_actor(const actor_draw_t* actor) {
    sat_vec3_t center;

    center.x = sat_fx16_from_int(actor->x);
    center.y = sat_fx16_from_int(ACTOR_Y);
    center.z = sat_fx16_from_int(actor->z);

    if (actor->is_pac) {
        const uint16_t gap = kMouthGap[kPacChew[(g_game.frame / 5u) & 3u]];
        build_pac(actor, gap);
        submit_mesh(actor->color, mouth_colors(gap));
        return;
    }

    note(sat_mesh_build_box(
        &g_mesh,
        &center,
        sat_fx16_from_int(GHOST_RADIUS),
        sat_fx16_from_int(GHOST_HALF_H),
        sat_fx16_from_int(GHOST_RADIUS)));
    submit_mesh(actor->color, NULL);
    draw_eye(actor, -EYE_SPREAD);
    draw_eye(actor, EYE_SPREAD);
}

/* Facing goes to SAT_DIR_NONE whenever an actor is stopped against a wall;
 * reusing the last real direction keeps Pac-Man's mouth from snapping to a
 * default the moment he stops. */
static int g_last_dir[PAC_GHOST_COUNT + 1] = {
    SAT_DIR_LEFT, SAT_DIR_LEFT, SAT_DIR_LEFT, SAT_DIR_LEFT, SAT_DIR_LEFT
};

static int actor_facing(const sat_grid_actor_t* a, int slot) {
    if (a->dir != SAT_DIR_NONE) {
        g_last_dir[slot] = (int)a->dir;
    }
    return g_last_dir[slot];
}

/* Collects the movers, farthest first. At most five of them, so a plain
 * insertion sort is the whole algorithm. */
static uint16_t collect_actors(actor_draw_t* out) {
    uint16_t count = 0;
    actor_draw_t pac;
    int i;

    for (i = 0; i < PAC_GHOST_COUNT; ++i) {
        actor_draw_t ghost;
        if (pac_game_ghost_penned(&g_game, i)) {
            continue;
        }
        ghost.x = (int16_t)g_game.ghosts[i].actor.x;
        ghost.z = (int16_t)g_game.ghosts[i].actor.y;
        ghost.is_pac = 0u;
        ghost.dir = (int8_t)actor_facing(&g_game.ghosts[i].actor, i);
        ghost.color = kGhostColors[i];
        if (pac_game_frightened(&g_game)) {
            /* Blue for most of it, flashing white only once the timer is
             * nearly out. Flashing the whole time hides how much is left,
             * and a white ghost with white eyes is a featureless blob. */
            ghost.color =
                (g_game.fright < PAC_FRIGHT_WARNING && ((g_game.frame / 6u) & 1u) == 0u)
                    ? COLOR_FRIGHT_B
                    : COLOR_FRIGHT_A;
        }
        ghost.depth = depth_at(ghost.x, ghost.z);
        out[count++] = ghost;
    }

    pac.x = (int16_t)g_game.pac.x;
    pac.z = (int16_t)g_game.pac.y;
    pac.is_pac = 1u;
    pac.dir = (int8_t)actor_facing(&g_game.pac, PAC_GHOST_COUNT);
    pac.color = COLOR_PAC;
    pac.depth = depth_at(pac.x, pac.z);
    out[count++] = pac;

    for (i = 1; i < (int)count; ++i) {
        const actor_draw_t value = out[i];
        int j = i - 1;
        while (j >= 0 && out[j].depth < value.depth) {
            out[j + 1] = out[j];
            --j;
        }
        out[j + 1] = value;
    }
    return count;
}

static int pellet_still_there(const baked_quad_t* baked) {
    const char cell = pac_game_cell(&g_game, baked->cell_col, baked->cell_row);
    return cell == '.' || cell == 'o';
}

/* Static scene first, then the actors on top.
 *
 * Merging the actors into the static depth order by distance is the obvious
 * thing and it looks worse. Per-object ordering cannot say "this wall is
 * nearer but does not actually cover you", so a wall one row in front of
 * Pac-Man was drawn after him and sliced a band out of him as he ran along a
 * corridor. Since an actor is twelve to fourteen units tall and no wall is
 * more than six, almost all of an actor is above every wall in the maze and
 * genuinely cannot be occluded -- so drawing them last is not a shortcut past
 * the depth problem, it is the more correct answer for this scene. */
static void render_scene(void) {
    actor_draw_t actors[PAC_GHOST_COUNT + 1];
    uint16_t actor_count;
    uint16_t i;

    for (i = 0; i < g_baked_count[g_angle]; ++i) {
        const baked_quad_t* baked = &g_baked[g_angle][i];
        if (baked->cell_col != BAKED_ALWAYS && !pellet_still_there(baked)) {
            continue;
        }
        note(sat_draw_quad2_polygon(&baked->quad, baked->color));
    }

    /* Still farthest-first among themselves, so two actors crossing overlap
     * the right way round. */
    actor_count = collect_actors(actors);
    for (i = 0; i < actor_count; ++i) {
        draw_actor(&actors[i]);
    }
}

/* ------------------------------------------------------------------ */
/* HUD                                                                */
/* ------------------------------------------------------------------ */

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
        draw_text_centered("DPAD MOVE  L R TURN  START RESET", 216);
    }

    if (g_draw_overflow) {
        draw_text("RENDER LIMIT", 4, 14);
    }
}

/* ------------------------------------------------------------------ */
/* Starfield                                                          */
/* ------------------------------------------------------------------ */
/* The board floats in space, so the sky is a VDP2 background rather than
 * more VDP1 geometry: the VDP1 command list is the scarce resource here
 * (around 400 commands is already a whole frame of SH-2 time) and a VDP2
 * layer costs exactly none of it, whatever it shows.
 *
 * NBG0 specifically, of the four normal backgrounds. NBG2 and NBG3 are
 * 16-colour only, and NBG1 has no API in this library yet; NBG0 is also the
 * one already free here, since this example otherwise uses nothing but the
 * VDP2 back-screen colour.
 *
 * The one setting that matters is priority. sat_vdp2_nbg0_init leaves NBG0
 * at 7 -- in FRONT of the sprite layer -- which puts the starfield over the
 * whole maze and looks like the 3D scene failed to draw. It has to be below
 * the sprite priority, and below rather than equal, because ties are broken
 * by a fixed hardware order instead of by setup order. */
#define SKY_PALETTE 1u
#define SKY_PRIORITY 1u
#define SPRITE_PRIORITY 6u

/* Scratch for the pattern-name map; the library allocates nothing itself.
 * 8KB is too much to leave in the fast work RAM for something used once at
 * startup, so it goes to Work RAM Low. */
static uint16_t g_sky_map[SAT_VDP2_NBG0_MAP_CELLS] __attribute__((section(".wram_l")));

static void init_sky(void) {
    sat_vdp2_nbg0_config_t config;
    sat_vdp2_scroll_t scroll = {0u, 0u, 0u, 0u};

    config.char_size = SAT_VDP2_CHAR_SIZE_1X1;
    config.color_mode = SAT_VDP2_COLOR_MODE_256;
    config.map_plane_index = 0x003Bu;
    config.transparent_code_enabled = 0u;
    config.reserved = 0u;

    sat_example_must(sat_vdp2_nbg0_init(&config));
    sat_example_must(sat_vdp2_palette_upload(
        stars_asset.palette, 256u, (uint16_t)(SKY_PALETTE * 256u)));
    sat_example_must(sat_vdp2_nbg0_upload_indexed8(
        stars_asset.pixels,
        stars_asset.width,
        stars_asset.height,
        SKY_PALETTE,
        g_sky_map));

    /* sat_vdp2_nbg0_init re-enables the display on its way out, and VDP2
     * register writes are dropped while the display is active -- so the
     * priorities have to wait for the next VBlank to stick. */
    sat_example_must(sat_wait_vblank());
    sat_example_must(sat_vdp2_sprite_set_priority(SPRITE_PRIORITY));
    sat_example_must(sat_vdp2_nbg0_set_priority(SKY_PRIORITY));
    sat_example_must(sat_vdp2_nbg0_set_scroll(&scroll));
}

/* Turn the sky with the camera.
 *
 * Without this the stars stay nailed to the screen while the board pivots
 * under them, which reads as the BOARD turning rather than the camera going
 * round it -- the sky is the only thing on screen far enough away to say
 * which of the two is happening. A full revolution scrolls the 512-pixel
 * plane exactly once, so the stars come back to where they started. */
static void set_sky_angle(uint16_t angle) {
    sat_vdp2_scroll_t scroll = {0u, 0u, 0u, 0u};
    scroll.x_integer = (uint16_t)(((uint32_t)angle * 512u) / CAM_ANGLES);
    sat_example_must(sat_vdp2_nbg0_set_scroll(&scroll));
}

/* L and R turn the camera one step. On the press rather than while held:
 * a step is 22.5 degrees, and repeating that every frame would spin the
 * board eight times a second. */
static void update_camera(const sat_pad_state_t* pad) {
    uint16_t next = g_angle;

    if ((pad->pressed & SAT_PAD_L) != 0u) {
        next = (uint16_t)((g_angle + CAM_ANGLES - 1u) % CAM_ANGLES);
    } else if ((pad->pressed & SAT_PAD_R) != 0u) {
        next = (uint16_t)((g_angle + 1u) % CAM_ANGLES);
    }
    if (next == g_angle) {
        return;
    }
    g_angle = next;
    /* The baked quads for this angle are already in memory; all that is
     * left is the matrix the live actors are projected through. */
    set_camera(g_angle);
    set_sky_angle(g_angle);
}

/* ------------------------------------------------------------------ */
/* Frame loop                                                         */
/* ------------------------------------------------------------------ */

/* Driven by hand rather than through sat_app_frame_begin, because that helper
 * sets an OPAQUE VDP1 erase which would paint over the VDP2 backdrop. */
static void frame_begin(sat_pad_state_t* pad) {
    SAT_PANIC_IF_ERROR(sat_wait_vblank());
    SAT_PANIC_IF_ERROR(sat_vdp1_set_erase_transparent());
    SAT_PANIC_IF_ERROR(sat_begin_frame());
    SAT_PANIC_IF_ERROR(sat_pad_poll(pad));
}

int main(void) {
    sat_video_config_t video = {SCREEN_W, SCREEN_H, 1u, 0u};

    SAT_PANIC_IF_ERROR(sat_init(&video));
    SAT_PANIC_IF_ERROR(sat_ascii_font_init_8x8_indexed8(
        &g_font, SAT_COLOR_WHITE, 0x0000u, HUD_PALETTE));
    sat_example_must(sat_mesh_init(
        &g_mesh, g_mesh_vertices, MESH_VERTEX_CAP, g_mesh_indices, MESH_FACE_CAP));

    init_sky();

    pac_game_init(&g_game, 0, 0);
    build_wall_rects();
    bake_scene();
    set_sky_angle(g_angle);

    while (1) {
        sat_pad_state_t pad = {0};
        frame_begin(&pad);

        pac_game_update(&g_game, &pad);
        update_camera(&pad);

        draw_board();
        render_scene();
        render_hud();

        SAT_PANIC_IF_ERROR(sat_end_frame());
    }

    return 0;
}
