/* basic_3d_animation - interactive animated textured 3D model viewer.
 *
 * Shows the compiled animated model generated from
 * examples/basic_3d_animation/assets/ (*.glb) through the generic
 * tools/import_model.py --target saturn pipeline:
 *
 *   GLB skin + animation -> host skinning -> baked pose frames
 *     + baked/dedup face textures -> generated C/H ->
 *     upload once -> per-frame pose decode -> sat_scene_t
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
#include "saturn/orbit_camera3d.h"
#include "saturn/mesh3d.h"
#include "saturn/model3d.h"
#include "saturn/render3d.h"
#include "saturn/scene.h"
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

static sat_ascii_font_t g_font;
static sat_scene3d_instance_t g_instance;

static sat_vec3_t g_mesh_vertices[MODEL_VERTEX_CAP];
static uint16_t g_mesh_indices[MODEL_FACE_CAP * 4u];
static sat_mesh_t g_mesh;
static sat_vdp1_texture_t g_model_textures[MODEL_TEXTURE_CAP];
/* Narrow order path when the model fits the uint8 table (<=255 faces),
 * wide order16 path otherwise; passing both keeps the call valid either
 * way and lets the importer retune counts without code edits. */
static sat_projected_vertex_t g_mesh_screen[MODEL_VERTEX_CAP];
static sat_scene3d_face_t g_scene_faces[MODEL_FACE_CAP];
static uint32_t g_scene_keys[MODEL_FACE_CAP];
static uint16_t g_scene_order[MODEL_FACE_CAP];
static sat_scene_t g_scene;
static sat_scene3d_material_t g_scene_materials[MODEL_FACE_CAP];
static uint16_t g_face_materials[MODEL_FACE_CAP];
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

static sat_orbit_camera3d_t g_orbit;
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

/* Game-specific HUD and animator semantics stay outside camera math. */
static void update_camera_from_inputs(const sat_pad_state_t* pad) {
    sat_example_must(sat_orbit_camera3d_apply_pad(&g_orbit,pad,SAT_PAD_C));
    if((pad->pressed&SAT_PAD_START)!=0u)
        g_show_hud=!g_show_hud;
    if((pad->pressed&SAT_PAD_B)!=0u) {
        note(sat_anim_reset(&g_anim));
        sat_anim_set_paused(&g_anim,0);
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
    if (g_orbit.auto_orbit) {
        draw_text("AUTO", 240, 202);
    }
    if (g_draw_overflow) {
        draw_text("RENDER LIMIT", 4, 32);
    }
}

int main(void) {
    sat_video_config_t video = {SCREEN_W, SCREEN_H, SAT_VIDEO_AUTO, 0u};
    sat_vec3_t mn = {0, 0, 0};
    sat_vec3_t mx = {0, 0, 0};

    SAT_PANIC_IF_ERROR(sat_init(&video));
    SAT_PANIC_IF_ERROR(sat_ascii_font_init_8x8_indexed8(
        &g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, FONT_PALETTE));
    g_ntsc = (int)sat_video_is_ntsc_timing();

    sat_example_must(sat_model_validate(&male_walk_asset));
    sat_example_must(sat_anim_validate(&male_walk_anim_asset));
    sat_example_must(sat_mesh_init(
        &g_mesh, g_mesh_vertices, MODEL_VERTEX_CAP, g_mesh_indices, MODEL_FACE_CAP));
    sat_example_must(sat_model_copy_to_mesh(&male_walk_asset, &g_mesh));
    /* One palette upload + one pixel upload per unique texture, once. */
    sat_example_must(sat_model_upload_textures(
        &male_walk_asset, g_model_textures, MODEL_TEXTURE_CAP));
    sat_example_must(sat_anim_state_init(&g_anim, &male_walk_anim_asset, 0));
    sat_example_must(sat_scene_init(&g_scene, g_scene_faces, g_scene_keys,
        g_scene_order, MODEL_FACE_CAP));
    g_instance.mesh = &g_mesh;
    g_instance.materials = g_scene_materials;
    g_instance.material_count = MODEL_FACE_CAP;
    g_instance.face_materials = g_face_materials;
    g_instance.world = NULL;
    g_instance.pass = 0u;
    g_instance.cull_backfaces = 1u;
    {
        uint16_t f;
        for (f = 0; f < MODEL_FACE_CAP; ++f) {
            g_face_materials[f] = f;
            g_scene_materials[f].kind = SAT_SCENE3D_RGB;
            g_scene_materials[f].color_calc_slot = SAT_INDEXED_SOLID_OPAQUE;
        }
    }

    /* Use UNION bounds across all animation clips, not just the bind pose. */
    anim_bounds(&male_walk_anim_asset, &mn, &mx);
    {
        sat_orbit_camera3d_fit_t fit={0};
        fit.min_extent=SAT_FX16_ONE>>6;
        fit.initial_distance_factor=SAT_FX16_ONE+(SAT_FX16_ONE>>2);
        fit.min_distance_factor=SAT_FX16_ONE/3;
        fit.max_distance_factor=sat_fx16_from_int(16);
        fit.near_plane_factor=SAT_FX16_ONE/8;
        fit.near_plane_floor=1;
        fit.fov_y=sat_fx16_from_int(60);
        fit.aspect=sat_fx16_div(sat_fx16_from_int(SCREEN_W),sat_fx16_from_int(SCREEN_H));
        fit.far_z=sat_fx16_from_int(2000);
        fit.pitch_min_deg=-60;
        fit.pitch_max_deg=60;
        sat_example_must(sat_orbit_camera3d_fit_bounds(&g_orbit,&mn,&mx,&fit));
    }
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
        update_fps();

        if (!g_draw_overflow) {
            uint16_t f;
            sat_camera3d_t camera = {};
            camera.eye = g_orbit.eye;
            camera.target = g_orbit.target;
            camera.up = (sat_vec3_t){0, SAT_FX16_ONE, 0};
            camera.view_proj = g_orbit.view_proj;
            note(sat_scene_begin(&g_scene, &camera, g_orbit.near_z,
                SCREEN_W, SCREEN_H, 64u));
            for (f = 0; f < male_walk_asset.face_count; ++f) {
                g_scene_materials[f].rgb555 = g_gouraud
                    ? g_face_base[f] : (g_has_shades ? g_face_colors[f] : SAT_RGB555(31,31,31));
                g_scene_materials[f].vertex_gouraud = g_gouraud ? g_vertex_gouraud : NULL;
            }
            note(sat_scene_submit_instance(&g_scene, &g_instance,
                SAT_SCENE3D_SLOT_INHERIT, g_mesh_screen, NULL));
            note(sat_scene_flush(&g_scene));
        }
        draw_hud();

        SAT_PANIC_IF_ERROR(sat_end_frame());
    }

    return 0;
}
