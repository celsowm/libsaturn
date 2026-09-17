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

/* Camera defaults derived from the model size at startup (see below); these
 * are multipliers, not per-asset magic distances. */
#define PITCH_MIN_DEG (-60)
#define PITCH_MAX_DEG 60
#define YAW_STEP_DEG 2
#define PITCH_STEP_DEG 2
#define AUTO_YAW_STEP_DEG 1
#define ZOOM_STEP_DIV 40

static sat_ascii_font_t g_font;

static sat_vec3_t g_mesh_vertices[MODEL_VERTEX_CAP];
static uint16_t g_mesh_indices[MODEL_FACE_CAP * 4u];
static sat_mesh_t g_mesh;
static sat_vdp1_texture_t g_model_textures[MODEL_TEXTURE_CAP];
static uint8_t g_mesh_order[MODEL_FACE_CAP];
static uint32_t g_mesh_depth[MODEL_FACE_CAP];
/* Projection cache for sat_draw_mesh's screen-space path. */
static sat_projected_vertex_t g_mesh_screen[MODEL_VERTEX_CAP];

static sat_mat4_t g_view_proj;
static sat_vec3_t g_cam_eye;
static sat_vec3_t g_target;

static int32_t g_yaw_deg;
static int32_t g_pitch_deg;
static sat_fx16_t g_distance;
static sat_fx16_t g_default_dist;
static sat_fx16_t g_min_dist;
static sat_fx16_t g_max_dist;
static int g_auto_orbit;
static int g_show_hud;
static int g_draw_overflow;

static void note(sat_result_t st) {
    if (st != SAT_OK && st != SAT_ERR_UNSUPPORTED) {
        g_draw_overflow = 1;
    }
}

static void reset_camera(void) {
    g_yaw_deg = 0;
    g_pitch_deg = 10;
    g_distance = g_default_dist;
    g_auto_orbit = 0;
}

static void update_camera_from_inputs(const sat_pad_state_t *pad) {
    if ((pad->held & SAT_PAD_LEFT) != 0u) {
        g_yaw_deg -= YAW_STEP_DEG;
    }
    if ((pad->held & SAT_PAD_RIGHT) != 0u) {
        g_yaw_deg += YAW_STEP_DEG;
    }
    if ((pad->held & SAT_PAD_UP) != 0u) {
        g_pitch_deg += PITCH_STEP_DEG;
    }
    if ((pad->held & SAT_PAD_DOWN) != 0u) {
        g_pitch_deg -= PITCH_STEP_DEG;
    }
    if (g_pitch_deg < PITCH_MIN_DEG) {
        g_pitch_deg = PITCH_MIN_DEG;
    }
    if (g_pitch_deg > PITCH_MAX_DEG) {
        g_pitch_deg = PITCH_MAX_DEG;
    }
    if ((pad->held & SAT_PAD_L) != 0u) {
        g_distance += g_distance / ZOOM_STEP_DIV + 1;
    }
    if ((pad->held & SAT_PAD_R) != 0u) {
        g_distance -= g_distance / ZOOM_STEP_DIV + 1;
    }
    if (g_distance < g_min_dist) {
        g_distance = g_min_dist;
    }
    if (g_distance > g_max_dist) {
        g_distance = g_max_dist;
    }
    if ((pad->pressed & SAT_PAD_A) != 0u) {
        g_auto_orbit = !g_auto_orbit;
    }
    if (g_auto_orbit) {
        g_yaw_deg += AUTO_YAW_STEP_DEG;
    }
    if (g_yaw_deg >= 360) {
        g_yaw_deg -= 360;
    }
    if (g_yaw_deg < 0) {
        g_yaw_deg += 360;
    }
    if ((pad->pressed & SAT_PAD_B) != 0u) {
        reset_camera();
    }
    if ((pad->pressed & SAT_PAD_START) != 0u) {
        g_show_hud = !g_show_hud;
    }
}

static void compute_camera(void) {
    sat_fx16_t yaw = sat_fx16_from_int(g_yaw_deg);
    sat_fx16_t pitch = sat_fx16_from_int(g_pitch_deg);
    sat_fx16_t cos_p = sat_cos_deg(pitch);
    sat_fx16_t sin_p = sat_sin_deg(pitch);
    sat_fx16_t sin_y = sat_sin_deg(yaw);
    sat_fx16_t cos_y = sat_cos_deg(yaw);
    sat_fx16_t r = sat_fx16_mul(g_distance, cos_p);

    g_cam_eye.x = g_target.x + sat_fx16_mul(r, sin_y);
    g_cam_eye.y = g_target.y + sat_fx16_mul(g_distance, sin_p);
    g_cam_eye.z = g_target.z + sat_fx16_mul(r, cos_y);

    {
        sat_mat4_t view;
        sat_mat4_t proj;
        sat_vec3_t up = {0, SAT_FX16_ONE, 0};
        sat_example_must(sat_mat4_look_at(&view, &g_cam_eye, &g_target, &up));
        sat_example_must(sat_mat4_perspective(
            &proj,
            sat_fx16_from_int(60),
            sat_fx16_div(sat_fx16_from_int(SCREEN_W), sat_fx16_from_int(SCREEN_H)),
            sat_fx16_from_int(1),
            sat_fx16_from_int(2000)));
        sat_example_must(sat_mat4_multiply(&g_view_proj, &proj, &view));
    }
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
    if (sat_fmt_label_u32("DIST ", (uint32_t)sat_fx16_to_int(g_distance),
                          line, sizeof(line), NULL) == SAT_OK) {
        draw_text(line, 120, 202);
    }
    if (g_auto_orbit) {
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
    sat_fx16_t extent_x;
    sat_fx16_t extent_y;
    sat_fx16_t extent_z;
    sat_fx16_t size;

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

    /* Center the model from its generic bounds; the camera orbits this. */
    sat_example_must(sat_model_compute_bounds(&sonic_model_asset, &mn, &mx));
    sat_example_must(sat_model_compute_center(&sonic_model_asset, &g_target));
    extent_x = mx.x - mn.x;
    extent_y = mx.y - mn.y;
    extent_z = mx.z - mn.z;
    size = extent_x;
    if (extent_y > size) {
        size = extent_y;
    }
    if (extent_z > size) {
        size = extent_z;
    }
    if (size < SAT_FX16_ONE) {
        size = SAT_FX16_ONE;
    }
    /* Frame the model: distance ~3x its largest extent, clamped to keep the
     * camera outside the model and inside stable projection ranges. */
    g_min_dist = size;
    if (g_min_dist < sat_fx16_from_int(10)) {
        g_min_dist = sat_fx16_from_int(10);
    }
    g_max_dist = size * 8;
    g_distance = size * 3;
    if (g_distance < g_min_dist) {
        g_distance = g_min_dist;
    }
    if (g_distance > g_max_dist) {
        g_distance = g_max_dist;
    }
    g_default_dist = g_distance;
    g_yaw_deg = 0;
    g_pitch_deg = 10;
    g_auto_orbit = 0;
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
        compute_camera();

        note(sat_model_bind_draw(
            &sonic_model_asset, &g_mesh,
            g_model_textures, sonic_model_asset.texture_count,
            &g_view_proj, &g_cam_eye,
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
