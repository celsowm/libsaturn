/* basic_3d_animation - interactive animated textured 3D model viewer.
 *
 * Shows the compiled animated model generated from
 * examples/basic_3d_animation/assets/ (*.glb) through the generic
 * tools/import_model.py --target saturn pipeline:
 *
 *   GLB skin + animation -> host skinning -> baked pose frames
 *     + baked/dedup face textures -> generated C/H ->
 *     upload once -> per-frame pose decode -> sat_draw_mesh
 *     textured path -> VDP1 distorted sprites.
 *
 * The viewer itself is generic: the orbit target comes from the baked pose
 * range in the animation encoding (no per-asset magic coordinates), textures are
 * uploaded once at startup, and every frame only advances animation time,
 * decodes one pose into caller-owned vertices, orbits the camera, and
 * submits the mesh with backface culling + painter sorting.
 *
 * Controls (held state for smooth motion, presses for toggles):
 *   LEFT/RIGHT orbit yaw around the model
 *   UP/DOWN    orbit pitch (clamped)
 *   L / R      zoom out / zoom in
 *   A          pause/resume animation
 *   B          reset yaw, pitch, zoom and animation
 *   C          toggle automatic slow orbit
 *   START      toggle the help HUD
 */
#include <stdint.h>

#include "saturn/anim3d.h"
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

#include "basic_3d_animation/male_walk.h"

#define SCREEN_W 320
#define SCREEN_H 224

#define FONT_PALETTE 0u

/* Mesh storage with headroom over the generated asset so importer retuning
 * does not force a recompile of sizes by hand each time. SORT binds through
 * the narrow uint8 path while face_count <= 255 and switches to order16
 * automatically beyond that; both buffers are sized for the cap. */
#define MODEL_VERTEX_CAP 1024u
#define MODEL_FACE_CAP 1600u
#define MODEL_TEXTURE_CAP 32u

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
static sat_texture_t g_model_textures[MODEL_TEXTURE_CAP];
/* Narrow order path when the model fits the uint8 table (<=255 faces),
 * wide order16 path otherwise; passing both keeps the call valid either
 * way and lets the importer retune counts without code edits. */
static uint8_t g_mesh_order[MODEL_FACE_CAP];
static uint16_t g_mesh_order16[MODEL_FACE_CAP];
static uint32_t g_mesh_depth[MODEL_FACE_CAP];
/* Projection cache: every vertex projects once per frame, and culling and
 * sorting then run in screen space. */
static sat_projected_vertex_t g_mesh_screen[MODEL_VERTEX_CAP];
/* Per-face colors for the current frame, looked up from the baked shades. */
static uint16_t g_face_colors[MODEL_FACE_CAP];
static int g_has_shades;
/* Gouraud mode (X toggles): static per-face base colors plus the current
 * frame's per-vertex corrections, both from the baked asset. */
static uint16_t g_face_base[MODEL_FACE_CAP];
static uint16_t g_vertex_gouraud[MODEL_VERTEX_CAP];
static int g_can_gouraud;
static int g_gouraud;
/* Program frames per second of display time, for the HUD. */
static uint32_t g_fps;
static uint32_t g_fps_frames;
static uint32_t g_fps_mark;

static sat_mat4_t g_view_proj;
static sat_vec3_t g_cam_eye;
static sat_vec3_t g_target;

static int32_t g_yaw_deg;
static int32_t g_pitch_deg;
static sat_fx16_t g_distance;
static sat_fx16_t g_default_dist;
static sat_fx16_t g_min_dist;
static sat_fx16_t g_max_dist;
static sat_fx16_t g_near;
static int g_auto_orbit;
static int g_show_hud;
static int g_draw_overflow;
static int g_ntsc;
static uint32_t g_tick;

static sat_anim_state_t g_anim;

static void note(sat_result_t st) {
    if (st != SAT_OK && st != SAT_ERR_UNSUPPORTED) {
        g_draw_overflow = 1;
    }
}

/* Display time since the previous call, in 16.16 seconds. Counting real
 * display frames (sat_frame_count) rather than loop iterations keeps the
 * walk at its authored speed when a frame takes more than one VBlank to
 * build; (n*65536)/rate on the running total keeps NTSC and PAL drift-free
 * instead of accumulating a truncated constant's bias. */
static sat_fx16_t vblank_dt(void) {
    uint32_t rate = (g_ntsc != 0) ? 60u : 50u;
    uint32_t n = sat_frame_count();
    uint64_t now = (uint64_t)n * 65536u / rate;
    uint64_t prev = (uint64_t)g_tick * 65536u / rate;
    g_tick = n;
    return (sat_fx16_t)(now - prev);
}

static sat_fx16_t fx_abs(sat_fx16_t v) {
    return (v < 0) ? -v : v;
}

/* Bounds over every frame of every clip, straight from the pose encoding
 * (each decoded axis lies within bias +/- scale). The bind pose is no
 * substitute: a rig's rest frame can differ from its animated frame -- this
 * asset's bind pose lies along -Z while every walk frame stands along +Y --
 * and framing the bind bounds aims the camera beside the model. */
static void anim_bounds(const sat_animated_model_asset_t *asset,
                        sat_vec3_t *mn, sat_vec3_t *mx) {
    uint16_t c;

    for (c = 0; c < asset->animation_count; ++c) {
        const sat_anim_position_encoding_t *e = &asset->animations[c].encoding;
        sat_vec3_t lo;
        sat_vec3_t hi;

        lo.x = e->bias_x - fx_abs(e->scale_x);
        lo.y = e->bias_y - fx_abs(e->scale_y);
        lo.z = e->bias_z - fx_abs(e->scale_z);
        hi.x = e->bias_x + fx_abs(e->scale_x);
        hi.y = e->bias_y + fx_abs(e->scale_y);
        hi.z = e->bias_z + fx_abs(e->scale_z);
        if (c == 0 || lo.x < mn->x) { mn->x = lo.x; }
        if (c == 0 || lo.y < mn->y) { mn->y = lo.y; }
        if (c == 0 || lo.z < mn->z) { mn->z = lo.z; }
        if (c == 0 || hi.x > mx->x) { mx->x = hi.x; }
        if (c == 0 || hi.y > mx->y) { mx->y = hi.y; }
        if (c == 0 || hi.z > mx->z) { mx->z = hi.z; }
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
    if ((pad->pressed & SAT_PAD_C) != 0u) {
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
        note(sat_anim_reset(&g_anim));
        sat_anim_set_paused(&g_anim, 0);
    }
    if ((pad->pressed & SAT_PAD_START) != 0u) {
        g_show_hud = !g_show_hud;
    }
}

/* Counts program frames against display time; refreshes once a second. */
static void update_fps(void) {
    uint32_t rate = (g_ntsc != 0) ? 60u : 50u;
    uint32_t now = sat_frame_count();
    uint32_t span = now - g_fps_mark;

    ++g_fps_frames;
    if (span >= rate) {
        g_fps = (g_fps_frames * rate + span / 2u) / span;
        g_fps_frames = 0;
        g_fps_mark = now;
    }
}

static void update_animation_from_inputs(const sat_pad_state_t *pad) {
    if ((pad->pressed & SAT_PAD_A) != 0u) {
        sat_anim_set_paused(&g_anim, !sat_anim_is_paused(&g_anim));
    }
    if ((pad->pressed & SAT_PAD_X) != 0u && g_can_gouraud) {
        g_gouraud = !g_gouraud;
    }
    note(sat_anim_advance(&g_anim, &male_walk_anim_asset, vblank_dt()));
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
            g_near,
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
    draw_text("L/R ZOOM  A PAUSE  B RESET", 4, 12);
    draw_text("C ORBIT  X SHADE  START HUD", 4, 22);
    if (g_can_gouraud) {
        draw_text(g_gouraud ? "GOURAUD" : "FLAT", 200, 192);
    }
    if (sat_anim_is_paused(&g_anim)) {
        draw_text("ANIM PAUSE", 4, 192);
    } else {
        draw_text("ANIM RUN", 4, 192);
    }
    if (sat_fmt_label_u32("FR ", (uint32_t)(g_anim.frame + 1u),
                          line, sizeof(line), NULL) == SAT_OK) {
        draw_text(line, 110, 192);
    }
    if (sat_fmt_label_u32("TRIS ", (uint32_t)male_walk_asset.face_count,
                          line, sizeof(line), NULL) == SAT_OK) {
        draw_text(line, 4, 202);
    }
    if (sat_fmt_label_u32("FPS ", g_fps, line, sizeof(line), NULL) == SAT_OK) {
        draw_text(line, 120, 202);
    }
    if (g_auto_orbit) {
        draw_text("AUTO", 240, 202);
    }
    if (g_draw_overflow) {
        draw_text("RENDER LIMIT", 4, 32);
    }
}

int main(void) {
    sat_video_config_t video = {SCREEN_W, SCREEN_H, 1u, 0u};
    sat_vec3_t mn = {0, 0, 0};
    sat_vec3_t mx = {0, 0, 0};
    sat_fx16_t extent_x;
    sat_fx16_t extent_y;
    sat_fx16_t extent_z;
    sat_fx16_t size;

    SAT_PANIC_IF_ERROR(sat_init(&video));
    SAT_PANIC_IF_ERROR(sat_ascii_font_init_8x8_indexed8(
        &g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, FONT_PALETTE));
    g_ntsc = (int)video.ntsc;

    sat_example_must(sat_model_validate(&male_walk_asset));
    sat_example_must(sat_anim_validate(&male_walk_anim_asset));
    sat_example_must(sat_mesh_init(
        &g_mesh, g_mesh_vertices, MODEL_VERTEX_CAP, g_mesh_indices, MODEL_FACE_CAP));
    sat_example_must(sat_model_copy_to_mesh(&male_walk_asset, &g_mesh));
    /* One palette upload + one pixel upload per unique texture, once. */
    sat_example_must(sat_model_upload_textures(
        &male_walk_asset, g_model_textures, MODEL_TEXTURE_CAP));
    sat_example_must(sat_anim_state_init(&g_anim, &male_walk_anim_asset, 0));

    /* The camera orbits the center of the animated pose range. */
    anim_bounds(&male_walk_anim_asset, &mn, &mx);
    g_target.x = mn.x + (mx.x - mn.x) / 2;
    g_target.y = mn.y + (mx.y - mn.y) / 2;
    g_target.z = mn.z + (mx.z - mn.z) / 2;
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
    /* No unit-scale floor: assets come in any world scale (this one spans
     * ~0.5 units), so framing stays proportional to the measured size.
     * Only clamp degenerate bounds away from zero. */
    if (size < (SAT_FX16_ONE >> 6)) {
        size = SAT_FX16_ONE >> 6;
    }
    /* Frame the model: with a 60 degree FOV, 1.25x its largest extent fills
     * about 70% of the screen height -- close enough that the figure gets
     * the pixels, with room for the stride and the HUD. Zoom 1/3x to 16x. */
    g_min_dist = size / 3;
    g_max_dist = size * 16;
    g_distance = size + size / 4;
    if (g_distance < g_min_dist) {
        g_distance = g_min_dist;
    }
    if (g_distance > g_max_dist) {
        g_distance = g_max_dist;
    }
    g_default_dist = g_distance;
    /* Near plane scales with the model too, otherwise a sub-unit asset at
     * minimum zoom sits inside the near clip and disappears. */
    g_near = size / 8;
    if (g_near == 0) {
        g_near = 1;
    }
    g_yaw_deg = 0;
    g_pitch_deg = 10;
    g_auto_orbit = 0;
    g_show_hud = 1;
    g_draw_overflow = 0;
    g_tick = sat_frame_count();
    g_fps_mark = g_tick;
    g_fps_frames = 0;
    g_fps = 0;
    /* Solid-color assets carry per-frame baked shades; textured ones don't. */
    g_has_shades = sat_anim_face_colors(
        &male_walk_anim_asset, &g_anim, g_face_colors, MODEL_FACE_CAP) == SAT_OK;
    g_can_gouraud =
        sat_model_face_base_colors(&male_walk_asset, g_face_base, MODEL_FACE_CAP) == SAT_OK &&
        sat_anim_vertex_gouraud(
            &male_walk_anim_asset, &g_anim, g_vertex_gouraud, MODEL_VERTEX_CAP) == SAT_OK;
    g_gouraud = 0;

    while (1) {
        sat_pad_state_t pad = {0};
        sat_mesh_draw_t draw;

        SAT_PANIC_IF_ERROR(sat_wait_vblank());
        SAT_PANIC_IF_ERROR(sat_vdp2_back_color_set(SAT_COLOR_BLACK));
        SAT_PANIC_IF_ERROR(sat_set_clear_color(SAT_COLOR_BLACK));
        SAT_PANIC_IF_ERROR(sat_begin_frame());
        SAT_PANIC_IF_ERROR(sat_pad_poll(&pad));

        update_camera_from_inputs(&pad);
        update_animation_from_inputs(&pad);
        /* Decode the current pose into the caller-owned mesh vertices:
         * model-local positions, no texture state touched. */
        note(sat_anim_decode(
            &male_walk_anim_asset, &g_anim, g_mesh.vertices, g_mesh.vertex_cap));
        if (g_gouraud) {
            note(sat_anim_vertex_gouraud(
                &male_walk_anim_asset, &g_anim, g_vertex_gouraud, MODEL_VERTEX_CAP));
        } else if (g_has_shades) {
            note(sat_anim_face_colors(
                &male_walk_anim_asset, &g_anim, g_face_colors, MODEL_FACE_CAP));
        }
        compute_camera();
        update_fps();

        note(sat_model_bind_draw_ex(
            &male_walk_asset, &g_mesh,
            g_model_textures, male_walk_asset.texture_count,
            &g_view_proj, &g_cam_eye,
            SAT_RGB555(31, 31, 31),
            g_gouraud ? g_face_base : (g_has_shades ? g_face_colors : NULL), 0,
            SAT_MESH_CULL_BACKFACE | SAT_MESH_SORT,
            g_mesh_order, g_mesh_order16, g_mesh_depth, &draw));
        draw.screen = g_mesh_screen;
        draw.vertex_gouraud = g_gouraud ? g_vertex_gouraud : NULL;
        if (!g_draw_overflow) {
            note(sat_draw_mesh(&g_mesh, &draw));
        }
        draw_hud();

        SAT_PANIC_IF_ERROR(sat_end_frame());
    }

    return 0;
}
