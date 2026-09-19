/* basic_3d_texture - interactive textured 3D model viewer for Sega Saturn.
 *
 * Shows the compiled model generated from examples/basic_3d_texture/assets/
 * (OBJ + MTL + PNG) through the generic tools/import_model.py pipeline:
 *
 *   OBJ + MTL + PNG -> generated C/H model -> upload once -> sat_draw_mesh
 *   textured path -> VDP1 distorted sprites.
 *
 * The viewer itself is generic: the model center comes from
 * sat_model_compute_center (no per-asset magic coordinates), textures are
 * uploaded once at startup, and every frame only polls the pad, orbits the
 * camera, and submits the mesh with backface culling + painter sorting.
 *
 * Controls (held state for smooth motion, presses for toggles):
 *   LEFT/RIGHT orbit yaw around the model
 *   UP/DOWN    orbit pitch (clamped)
 *   L / R      zoom out / zoom in
 *   A          toggle automatic slow orbit
 *   B          reset yaw, pitch and zoom
 *   START      toggle the help HUD
 */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/fmt.h"
#include "saturn/font.h"
#include "saturn/input.h"
#include "saturn/math3d.h"
#include "saturn/orbit_camera3d.h"
#include "saturn/mesh3d.h"
#include "saturn/model3d.h"
#include "saturn/render3d.h"
#include "saturn/vdp1.h"
#include "saturn/example_util.h"

#include "basic_3d_texture/sonic_model.h"

#define SCREEN_W 320
#define SCREEN_H 224

#define FONT_PALETTE 0u

/* Mesh storage: sonic_model has 285 vertices / 185 faces; keep headroom for
 * importer tuning without recompiling sizes by hand each time. SORT needs
 * order/depth entries per face and faces indexed by uint8 (max 255). */
#define MODEL_VERTEX_CAP 320u
#define MODEL_FACE_CAP 200u
#define MODEL_TEXTURE_CAP 64u

static sat_ascii_font_t g_font;

static sat_vec3_t g_mesh_vertices[MODEL_VERTEX_CAP];
static uint16_t g_mesh_indices[MODEL_FACE_CAP * 4u];
static sat_mesh_t g_mesh;
static sat_vdp1_texture_t g_model_textures[MODEL_TEXTURE_CAP];
static uint8_t g_mesh_order[MODEL_FACE_CAP];
static uint32_t g_mesh_depth[MODEL_FACE_CAP];
/* Projection cache for sat_draw_mesh's screen-space path. */
static sat_projected_vertex_t g_mesh_screen[MODEL_VERTEX_CAP];

static sat_orbit_camera3d_t g_orbit;
static int g_show_hud;
static int g_draw_overflow;

static void note(sat_result_t st) {
    if (st != SAT_OK && st != SAT_ERR_UNSUPPORTED) {
        g_draw_overflow = 1;
    }
}

/* Game-specific HUD and animator semantics stay outside camera math. */
static void update_camera_from_inputs(const sat_pad_state_t* pad) {
    sat_example_must(sat_orbit_camera3d_apply_pad(&g_orbit,pad,SAT_PAD_A));
    if((pad->pressed&SAT_PAD_START)!=0u)
        g_show_hud=!g_show_hud;
}
static void draw_text(const char *text, int x, int y) {
    sat_result_t st = sat_ascii_font_draw_text_screen_indexed8(
        &g_font, text, x, y, 8, 0, 0);
    (void)st;
}

static void draw_hud(void) {
    char line[40];

    if (!g_show_hud) {
        return;
    }
    draw_text("LEFT/RIGHT ORBIT  UP/DN PITCH", 4, 2);
    draw_text("L/R ZOOM  A AUTO  B RESET", 4, 12);
    if (sat_fmt_label_u32("TEX ", (uint32_t)sonic_model_asset.texture_count,
                          line, sizeof(line), NULL) == SAT_OK) {
        draw_text(line, 4, 202);
    }
    if (sat_fmt_label_u32("DIST ", (uint32_t)sat_fx16_to_int(g_orbit.distance),
                          line, sizeof(line), NULL) == SAT_OK) {
        draw_text(line, 120, 202);
    }
    if (g_orbit.auto_orbit) {
        draw_text("AUTO", 240, 202);
    }
    if (g_draw_overflow) {
        draw_text("RENDER LIMIT", 4, 22);
    }
}

int main(void) {
    sat_video_config_t video = {SCREEN_W, SCREEN_H, 1u, 0u};
    sat_vec3_t mn;
    sat_vec3_t mx;

    SAT_PANIC_IF_ERROR(sat_init(&video));
    SAT_PANIC_IF_ERROR(sat_ascii_font_init_8x8_indexed8(
        &g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, FONT_PALETTE));

    sat_example_must(sat_model_validate(&sonic_model_asset));
    sat_example_must(sat_mesh_init(
        &g_mesh, g_mesh_vertices, MODEL_VERTEX_CAP, g_mesh_indices, MODEL_FACE_CAP));
    sat_example_must(sat_model_copy_to_mesh(&sonic_model_asset, &g_mesh));
    /* One palette upload + one pixel upload per unique texture, once. */
    sat_example_must(sat_model_upload_textures(
        &sonic_model_asset, g_model_textures, MODEL_TEXTURE_CAP));

    /* Static-model bounds use a wider camera and absolute zoom-in floor. */
    sat_example_must(sat_model_compute_bounds(&sonic_model_asset, &mn, &mx));
    {
        sat_orbit_camera3d_fit_t fit={0};
        fit.min_extent=SAT_FX16_ONE;
        fit.min_distance_floor=sat_fx16_from_int(10);
        fit.initial_distance_factor=sat_fx16_from_int(3);
        fit.min_distance_factor=SAT_FX16_ONE;
        fit.max_distance_factor=sat_fx16_from_int(8);
        fit.near_plane_floor=sat_fx16_from_int(1);
        fit.fov_y=sat_fx16_from_int(60);
        fit.aspect=sat_fx16_div(sat_fx16_from_int(SCREEN_W),sat_fx16_from_int(SCREEN_H));
        fit.far_z=sat_fx16_from_int(2000);
        fit.pitch_min_deg=-60;
        fit.pitch_max_deg=60;
        sat_example_must(sat_orbit_camera3d_fit_bounds(&g_orbit,&mn,&mx,&fit));
    }
    g_show_hud = 1;
    g_draw_overflow = 0;

    while (1) {
        sat_pad_state_t pad = {0};
        sat_mesh_draw_t draw;

        SAT_PANIC_IF_ERROR(sat_wait_vblank());
        SAT_PANIC_IF_ERROR(sat_vdp2_back_color_set(SAT_COLOR_BLACK));
        SAT_PANIC_IF_ERROR(sat_set_clear_color(SAT_COLOR_BLACK));
        SAT_PANIC_IF_ERROR(sat_begin_frame());
        SAT_PANIC_IF_ERROR(sat_pad_poll(&pad));

        update_camera_from_inputs(&pad);

        note(sat_model_bind_draw(
            &sonic_model_asset, &g_mesh,
            g_model_textures, sonic_model_asset.texture_count,
            &g_orbit.view_proj, &g_orbit.eye,
            SAT_RGB555(31, 31, 31), NULL, 0,
            SAT_MESH_CULL_BACKFACE | SAT_MESH_SORT,
            g_mesh_order, g_mesh_depth, &draw));
        draw.screen = g_mesh_screen;
        if (!g_draw_overflow) {
            note(sat_draw_mesh(&g_mesh, &draw));
        }
        draw_hud();

        SAT_PANIC_IF_ERROR(sat_end_frame());
    }

    return 0;
}
