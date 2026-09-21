#include "board.h"

#include "saturn/grid.h"
#include "saturn/mesh3d.h"
#include "saturn/render3d.h"
#include "saturn/view_cache.h"
#include "saturn/example_util.h"

#include "camera.h"
#include "p3d_config.h"
#include "render_status.h"

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

/* Faces turned away from the light keep this fraction of their colour, so a
 * shadowed side stays readable instead of reading as a hole. */
#define AMBIENT (SAT_FX16_ONE / 3)

/* The VDP1 has no depth buffer: quads overwrite each other in list order, so
 * walls and pellets go into ONE back-to-front ordering. Sorting them
 * separately looks right until a pellet sits just in front of a wall, at
 * which point the wall paints over it.
 *
 * Pellets stay in the baked list after they are eaten and are skipped by
 * looking at the maze, which avoids re-sorting anything as the board
 * changes. */
#define MAX_BAKED 448u
#define MAX_WALL_RECTS 96u
/* The board is a flat, evenly lit surface, so subdividing it buys nothing
 * but VDP1 commands -- and on this scene every command counts: 400 of them
 * per frame is already about one frame of SH-2 time. */
#define BOARD_SEGMENTS 1u
#define MAX_BOARD_QUADS 4u

/* Marks a baked quad that is always drawn, as opposed to a pellet that is
 * only drawn while its maze cell still holds one. */
#define BAKED_ALWAYS 0xFFFFu

/* Quads smaller than this many square pixels are dropped at bake time.
 *
 * A box side face seen nearly edge-on covers no pixels but still costs a
 * VDP1 command, and with the camera above the middle of the board most of
 * the maze's side faces are in exactly that position. Two square pixels is
 * below the smallest pellet, which projects to about five -- and that is the
 * ceiling on this threshold, not a safety margin: at eight square pixels it
 * starts eating the pellets themselves. */
#define MIN_QUAD_AREA2 4

/* Enough for the largest thing built here, one wall box. */
#define BAKE_VERTEX_CAP 8u
#define BAKE_FACE_CAP 6u

typedef struct wall_rect {
    uint8_t col;
    uint8_t row;
    uint8_t cols;
    uint8_t rows;
} wall_rect_t;

static wall_rect_t g_rects[MAX_WALL_RECTS];
static uint16_t g_rect_count;

/* One baked scene per camera angle. This is the big allocation in the
 * program -- about 170KB -- and it is what makes turning free: the
 * alternative, re-projecting the maze when the angle changes, costs a full
 * unbaked frame every time and would stutter for as long as a button is
 * held. */
static sat_view_cache_item_t g_baked[P3D_CAM_ANGLES * MAX_BAKED];
static uint16_t g_baked_counts[P3D_CAM_ANGLES];
static sat_view_cache_t g_view_cache;

/* The board is under everything and never overlaps itself, so it is drawn
 * first as a block rather than taking part in the depth ordering. */
static sat_quad2_t g_board[P3D_CAM_ANGLES][MAX_BOARD_QUADS];
static uint16_t g_board_colors[P3D_CAM_ANGLES][MAX_BOARD_QUADS];
static uint16_t g_board_count[P3D_CAM_ANGLES];

static sat_vec3_t g_mesh_vertices[BAKE_VERTEX_CAP];
static uint16_t g_mesh_indices[BAKE_FACE_CAP * 4u];
static sat_mesh_t g_mesh;

/* The angle being baked, and its camera. */
static const p3d_camera_t* g_bake_camera;

/* ------------------------------------------------------------------ */
/* Wall rectangles                                                     */
/* ------------------------------------------------------------------ */

/* Greedy merge of wall tiles into maximal rectangles: run right along a row,
 * then extend down as far as every column of the run stays wall.
 *
 * One box per rectangle instead of one per tile is the difference between
 * about 60 solids and about 200, and at ~400 polygons per SH-2 frame that is
 * the difference between the board fitting a frame and not. */
static void build_wall_rects(const pac_game_t* game) {
    static uint8_t used[kPacMazeRows][kPacMazeCols];
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

            if (pac_game_cell(game, col, row) != PAC_CELL_WALL || used[row][col]) {
                ++col;
                continue;
            }
            end = col;
            while (end < kPacMazeCols && pac_game_cell(game, end, row) == PAC_CELL_WALL &&
                   !used[row][end]) {
                ++end;
            }
            bottom = row + 1;
            while (bottom < kPacMazeRows) {
                int ok = 1;
                for (c = col; c < end; ++c) {
                    if (pac_game_cell(game, c, bottom) != PAC_CELL_WALL || used[bottom][c]) {
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
            } else {
                p3d_note_overflow();
            }
            col = end;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Baking one angle                                                    */
/* ------------------------------------------------------------------ */

/* Twice the area of a projected quad, magnitude only: winding does not
 * matter for a size threshold, only for the backface test bake_walls
 * already did with sat_mesh_face_visible. */
static int32_t quad_area2(const sat_quad2_t* q) {
    const int32_t signed_area = sat_quad2_area2(q);
    return (signed_area < 0) ? -signed_area : signed_area;
}

/* Squared ground distance from the camera: the baked painter's sort key. */
static uint32_t depth_at(int x, int z) {
    return sat_ground_distance_sq(g_bake_camera->eye.x, g_bake_camera->eye.z,
        sat_fx16_from_int(x), sat_fx16_from_int(z));
}

/* Projects one face of the scratch mesh and files it in the baked list.
 * `x`/`z` are the world position the quad sorts by; `tag` is BAKED_ALWAYS
 * for permanent geometry, or the maze cell a pellet belongs to. */
static void bake_face(uint16_t face, uint16_t color, int x, int z, uint16_t tag) {
    sat_quad3_t quad;
    sat_quad2_t projected;

    if (sat_mesh_face_quad(&g_mesh, face, &quad) != SAT_OK) {
        return;
    }
    if (sat_project_quad(&g_bake_camera->view.view_proj, &quad, &projected) != SAT_OK) {
        return; /* behind the camera: nothing to replay later */
    }
    if (quad_area2(&projected) < MIN_QUAD_AREA2) {
        return;
    }
    if (sat_view_cache_append(&g_view_cache, &projected, depth_at(x, z), color, tag)
        != SAT_OK) {
        p3d_note_overflow();
    }
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

static void bake_board(uint16_t angle) {
    sat_vec3_t center;
    uint16_t i;

    center.x = sat_fx16_from_int(P3D_BOARD_W / 2);
    center.y = 0;
    center.z = sat_fx16_from_int(P3D_BOARD_D / 2);
    p3d_note(sat_mesh_build_plane(
        &g_mesh,
        &center,
        sat_fx16_from_int((P3D_BOARD_W / 2) + P3D_TILE),
        sat_fx16_from_int((P3D_BOARD_D / 2) + P3D_TILE),
        BOARD_SEGMENTS,
        BOARD_SEGMENTS));

    g_board_count[angle] = 0;
    for (i = 0; i < g_mesh.face_count && g_board_count[angle] < MAX_BOARD_QUADS; ++i) {
        sat_quad3_t quad;
        const uint16_t slot = g_board_count[angle];
        if (sat_mesh_face_quad(&g_mesh, i, &quad) != SAT_OK) {
            continue;
        }
        if (sat_project_quad(&g_bake_camera->view.view_proj, &quad,
                &g_board[angle][slot]) != SAT_OK) {
            continue;
        }
        g_board_colors[angle][slot] = shade_face(i, P3D_COLOR_BOARD);
        ++g_board_count[angle];
    }
}

static void bake_walls(void) {
    uint16_t r;

    for (r = 0; r < g_rect_count; ++r) {
        const wall_rect_t* rect = &g_rects[r];
        const int half_x = ((int)rect->cols * P3D_TILE) / 2;
        const int half_z = ((int)rect->rows * P3D_TILE) / 2;
        sat_vec3_t center;
        uint16_t f;

        center.x = sat_fx16_from_int(((int)rect->col * P3D_TILE) + half_x);
        center.y = sat_fx16_from_int(WALL_HEIGHT / 2);
        center.z = sat_fx16_from_int(((int)rect->row * P3D_TILE) + half_z);
        p3d_note(sat_mesh_build_box(
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
            if (!sat_mesh_face_visible(&g_mesh, f, &g_bake_camera->eye)) {
                continue;
            }
            if (sat_mesh_face_center(&g_mesh, f, &face_center) != SAT_OK) {
                continue;
            }
            bake_face(f, shade_face(f, P3D_COLOR_WALL),
                sat_fx16_to_int(face_center.x), sat_fx16_to_int(face_center.z),
                BAKED_ALWAYS);
        }
    }
}

static void bake_pellets(const pac_game_t* game) {
    int r;
    int c;

    for (r = 0; r < kPacMazeRows; ++r) {
        for (c = 0; c < kPacMazeCols; ++c) {
            const char cell = pac_game_cell(game, c, r);
            const int power = (cell == PAC_CELL_POWER);
            sat_quad3_t quad;
            int x;
            int z;
            if (cell != PAC_CELL_PELLET && !power) {
                continue;
            }
            x = sat_grid_tile_center_x(&game->grid, c);
            z = sat_grid_tile_center_y(&game->grid, r);
            sat_quad3_floor(
                &quad,
                sat_fx16_from_int(x),
                /* Lifted a unit off the board so it is not z-fighting the
                 * floor plane at this grazing angle. */
                sat_fx16_from_int(1),
                sat_fx16_from_int(z),
                sat_fx16_from_int(power ? 3 : 1));
            sat_mesh_clear(&g_mesh);
            if (sat_mesh_add_quad(&g_mesh, &quad) != SAT_OK) {
                continue;
            }
            bake_face(0u, power ? P3D_COLOR_POWER : P3D_COLOR_PELLET, x, z,
                (uint16_t)(((c & 0xFF) << 8) | (r & 0xFF)));
        }
    }
}

/* ------------------------------------------------------------------ */
/* Public                                                              */
/* ------------------------------------------------------------------ */

void p3d_board_init(void) {
    sat_example_must(sat_mesh_init(
        &g_mesh, g_mesh_vertices, BAKE_VERTEX_CAP, g_mesh_indices, BAKE_FACE_CAP));
    sat_example_must(sat_view_cache_init(&g_view_cache, g_baked,
        g_baked_counts, P3D_CAM_ANGLES, MAX_BAKED));
}

void p3d_board_bake(const pac_game_t* game, p3d_bake_progress_fn progress) {
    p3d_camera_t camera;
    uint16_t angle;

    /* A new generation empties every view, so an angle the loop below
     * failed to reach can never replay the previous stage's walls. */
    static uint32_t generation;

    p3d_note(sat_view_cache_set_generation(&g_view_cache, ++generation));
    build_wall_rects(game);
    g_bake_camera = &camera;
    for (angle = 0u; angle < P3D_CAM_ANGLES; ++angle) {
        if (progress != 0) {
            progress(angle, P3D_CAM_ANGLES);
        }
        p3d_camera_set(&camera, angle);
        sat_example_must(sat_view_cache_begin(&g_view_cache, angle));
        bake_board(angle);
        bake_walls();
        bake_pellets(game);
        if (sat_view_cache_sort(&g_view_cache) != SAT_OK) {
            p3d_note_overflow();
        }
    }
    g_bake_camera = 0;
}

static int pellet_still_there(const pac_game_t* game, uint16_t tag) {
    const char cell = pac_game_cell(game, (int)(tag >> 8), (int)(tag & 0xFFu));
    return cell == PAC_CELL_PELLET || cell == PAC_CELL_POWER;
}

void p3d_board_draw(sat_scene_t* scene, const pac_game_t* game, uint16_t angle) {
    const sat_view_cache_item_t* baked = 0;
    uint16_t baked_count = 0u;
    uint16_t i;

    for (i = 0; i < g_board_count[angle]; ++i) {
        sat_view_cache_item_t item = {};
        item.quad = g_board[angle][i];
        item.color = g_board_colors[angle][i];
        p3d_note(sat_scene_replay_view_item(scene, &item));
    }

    p3d_note(sat_view_cache_view(&g_view_cache, angle, &baked, &baked_count));
    for (i = 0; i < baked_count; ++i) {
        if (baked[i].tag != BAKED_ALWAYS && !pellet_still_there(game, baked[i].tag)) {
            continue;
        }
        p3d_note(sat_scene_replay_view_item(scene, &baked[i]));
    }
}
