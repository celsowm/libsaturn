/* pacman_3d - Pac-Man on a 3D board for Sega Saturn (libsaturn).
 *
 * Same game as examples/pacman_2d, same code driving it: every rule lives in
 * examples/common/pacman_game.c and every stage layout in
 * examples/common/pacman_stages.c. This example only decides how to look at
 * it. The maze pixels the 2D example treats as screen X and Y are treated
 * here as world X and Z.
 *
 * The camera is a high three-quarter view with the whole maze in frame, the
 * way a board game sits on a table, and can be turned around the board. A
 * chase camera behind Pac-Man was the obvious thing to try and it reads badly
 * here: at this scale the walls are a few tiles away in every direction, so
 * the view is mostly wall, and a game about seeing where the ghosts are
 * becomes a game about not seeing them.
 *
 * Where things are:
 *   camera.c        the sixteen camera positions and turning between them
 *   board.c         board, walls and pellets, baked once per camera angle
 *   actors.c        reads Pac-Man and the ghosts out of the game
 *   pac_model.c     Pac-Man's mesh, mouth and shading
 *   ghost_model.c   the ghost mesh and its textured face
 *   actor_render.c  what both models share: submission, key light, welding
 *   sky.c           the VDP2 starfield
 *   hud.c           score, lives, banners, the bake progress screen
 *   render_status.c the "RENDER LIMIT" flag every drawing module reports to
 *
 * The frame is SH-2 bound and right at its edge: see board.h for why the
 * static scene is baked, and the notes in ghost_model.c and actor_render.c
 * for what the actors had to give up to fit.
 *
 * Controls: D-Pad to move, L and R to turn the camera, START to restart.
 * No sound.
 */
#include <stdint.h>

#include "saturn/input.h"
#include "saturn/scene.h"
#include "saturn/vdp1.h"
#include "saturn/video.h"
#include "saturn/example_util.h"

#include "../common/pacman_game.h"
#include "actors.h"
#include "board.h"
#include "camera.h"
#include "hud.h"
#include "p3d_config.h"
#include "sky.h"

/* Every actor face that survives culling is queued for the frame's painter
 * sort: about 30 for Pac-Man and 15 per ghost with its face. */
#define SCENE_FACE_CAP 128u
/* VDP1 commands kept back from the world for the HUD drawn after it. */
#define HUD_COMMANDS 64u

#define NO_STAGE 0xFFFFu

static pac_game_t g_game;
static p3d_camera_t g_camera;
static sat_scene3d_face_t g_scene_faces[SCENE_FACE_CAP];
static uint32_t g_scene_keys[SCENE_FACE_CAP];
static uint16_t g_scene_order[SCENE_FACE_CAP];
static sat_scene_t g_scene;

/* The stage whose walls are currently baked. */
static uint16_t g_baked_stage = NO_STAGE;

static void bake_progress(uint16_t done, uint16_t total) {
    p3d_hud_bake_progress(g_game.stage, done, total);
}

/* Runs between frames, never inside one: baking draws its own progress
 * frames. Restarting on the stage already baked costs nothing -- eaten
 * pellets are skipped by looking at the maze, not removed from the bake. */
static void bake_if_stage_changed(void) {
    if (g_game.stage == g_baked_stage) {
        return;
    }
    p3d_board_bake(&g_game, bake_progress);
    g_baked_stage = g_game.stage;
}

/* Driven by hand rather than through sat_app_frame_begin, because that helper
 * sets an OPAQUE VDP1 erase which would paint over the VDP2 backdrop. */
static void frame_begin(sat_pad_state_t* pad) {
    SAT_PANIC_IF_ERROR(sat_wait_vblank());
    SAT_PANIC_IF_ERROR(sat_vdp1_set_erase_transparent());
    SAT_PANIC_IF_ERROR(sat_begin_frame());
    SAT_PANIC_IF_ERROR(sat_pad_poll(pad));
}

int main(void) {
    sat_video_config_t video = {P3D_SCREEN_W, P3D_SCREEN_H, SAT_VIDEO_AUTO, 0u};

    SAT_PANIC_IF_ERROR(sat_init(&video));
    p3d_hud_init();
    p3d_board_init();
    p3d_actors_init();
    sat_example_must(sat_scene_init(&g_scene, g_scene_faces, g_scene_keys,
        g_scene_order, SCENE_FACE_CAP));
    p3d_sky_init();

    pac_game_init(&g_game, 0, 0);
    p3d_camera_set(&g_camera, 0u);
    p3d_actors_relight(&g_camera);
    p3d_sky_set_angle(g_camera.angle);

    while (1) {
        sat_pad_state_t pad = {0};

        bake_if_stage_changed();
        frame_begin(&pad);

        pac_game_update(&g_game, &pad);
        if (g_game.stage != g_baked_stage) {
            /* A new stage's maze under the old stage's baked walls would
             * show for a frame; skip it and bake at the top of the loop. */
            SAT_PANIC_IF_ERROR(sat_end_frame());
            continue;
        }
        if (p3d_camera_update(&g_camera, &pad)) {
            p3d_actors_relight(&g_camera);
            p3d_sky_set_angle(g_camera.angle);
        }

        sat_example_must(sat_scene_begin(&g_scene, &g_camera.view,
            SAT_FX16_ONE, P3D_SCREEN_W, P3D_SCREEN_H, HUD_COMMANDS));
        /* Static scene first, then the actors on top. Merging the actors
         * into the baked depth order looks worse: per-object ordering cannot
         * say "this wall is nearer but does not actually cover you", so a
         * wall one row in front of Pac-Man sliced a band out of him. No wall
         * is taller than an actor's lower half, so drawing them last is the
         * more correct answer for this scene, not a shortcut. */
        p3d_board_draw(&g_scene, &g_game, g_camera.angle);
        p3d_actors_draw(&g_scene, &g_game, &g_camera);
        sat_example_must(sat_scene_flush(&g_scene));
        p3d_hud_draw(&g_game);

        SAT_PANIC_IF_ERROR(sat_end_frame());
    }

    return 0;
}
