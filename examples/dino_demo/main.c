/* dino_demo - interactive animated T-Rex tech demo.
 *
 * Shows the compiled animated model generated from
 * examples/dino_demo/assets/ (*.glb) through the generic
 * tools/import_model.py --target saturn pipeline:
 *
 *   GLB skin + animation -> host skinning -> baked pose frames
 *     + triangle pairs merged into quads + baked/dedup face textures ->
 *     generated C/H -> upload once -> per-frame pose decode (Slave SH-2)
 *     -> sat_scene_t textured path -> VDP1 distorted sprites.
 *
 * The viewer itself is generic: the orbit target comes from the baked pose
 * range in the animation encoding (no per-asset magic coordinates), textures are
 * uploaded once at startup, and every frame only advances animation time,
 * decodes one pose into caller-owned vertices, orbits the camera, and
 * submits the mesh with backface culling + painter sorting.
 *
 * Both SH-2s share the frame. The face list is split in two halves over the
 * same vertices: the Slave prepares one (projection, culling, painter keys)
 * while the Master prepares the other, and the Master merges both into one
 * painter queue. While the Master then sorts and emits, the Slave decodes
 * the next pose into the second pose buffer. Faces whose baked texture is a
 * single colour (teeth and claws) draw as plain RGB polygons: same look, no
 * texture fetch.
 *
 * Controls (held state for smooth motion, presses for toggles):
 *   L / R      orbit yaw around the model
 *   UP/DOWN    orbit pitch (clamped)
 *   X / Y      zoom in / zoom out
 *   A          pause/resume animation
 *   B          reset yaw, pitch, zoom and animation
 *   C          toggle automatic slow orbit
 *   START      toggle the help HUD
 */
#include <stdint.h>

#include "saturn/anim3d.h"
#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/input.h"
#include "saturn/math3d.h"
#include "saturn/orbit_camera3d.h"
#include "saturn/mesh3d.h"
#include "saturn/parallel.h"
#include "saturn/model3d.h"
#include "saturn/render3d.h"
#include "saturn/scene.h"
#include "saturn/vdp1.h"
#include "saturn/example_util.h"

#include "dino_demo/trex_model.h"

#define SCREEN_W 320
#define SCREEN_H 224
#define MODEL_START_YAW_DEG 90

#define FONT_PALETTE 0u

#define MODEL_VERTEX_CAP TREX_VERTEX_COUNT
#define MODEL_FACE_CAP TREX_FACE_COUNT
#define MODEL_TEXTURE_CAP TREX_TEXTURE_COUNT

/* Frame scratch and upload handles are writable, zero-initialized data.
 * The large face records live in WRAM-L so the baked model tables and
 * program fit in WRAM-H; the arrays every vertex touches every frame stay in
 * the faster WRAM-H. */
#define DINO_WRAM_L __attribute__((section(".wram_l")))

/* Faces [0, MASTER_FACES) are prepared on the Master, the rest on the Slave. */
#define MASTER_FACES (MODEL_FACE_CAP * 3u / 5u)
#define SLAVE_FACES (MODEL_FACE_CAP - MASTER_FACES)

static sat_ascii_font_t g_font DINO_WRAM_L;
static sat_scene3d_instance_t g_instance DINO_WRAM_L;

/* Two pose buffers: the Master draws one while the Slave decodes the next. */
static sat_vec3_t g_pose[2][MODEL_VERTEX_CAP];
static uint16_t g_mesh_indices[MODEL_FACE_CAP * 4u] DINO_WRAM_L;
static sat_mesh_t g_mesh DINO_WRAM_L;
static sat_vdp1_texture_t g_model_textures[MODEL_TEXTURE_CAP] DINO_WRAM_L;
static sat_projected_vertex_t g_mesh_screen[MODEL_VERTEX_CAP];
static sat_scene3d_face_t g_scene_faces[MODEL_FACE_CAP] DINO_WRAM_L;
static uint32_t g_scene_keys[MODEL_FACE_CAP];
static uint16_t g_scene_order[MODEL_FACE_CAP];
/* The two halves: mesh views over the shared index table, each with the
 * window of vertices its faces use. The importer orders faces along the
 * body and vertices by first use, so the windows barely overlap and each
 * CPU projects about half the pose. */
static sat_mesh_t g_half_mesh[2] DINO_WRAM_L;
static sat_scene3d_instance_t g_half[2] DINO_WRAM_L;
static uint16_t g_half_first[2] DINO_WRAM_L;
/* Slave half: its own projection scratch and prepared-face storage, merged
 * into g_scene after the task completes. */
static sat_projected_vertex_t g_slave_screen[MODEL_VERTEX_CAP];
static sat_scene3d_face_t g_slave_faces[SLAVE_FACES] DINO_WRAM_L;
static uint32_t g_slave_keys[SLAVE_FACES];
static uint16_t g_slave_order[SLAVE_FACES];
static sat_scene3d_prepare_item_t g_slave_item DINO_WRAM_L;
static sat_scene3d_prepare_batch_t g_slave_batch DINO_WRAM_L;
static sat_parallel_handle_t g_slave_handle DINO_WRAM_L;
static sat_scene_t g_scene DINO_WRAM_L;
static sat_scene3d_material_t g_scene_materials[MODEL_TEXTURE_CAP] DINO_WRAM_L;
static sat_orbit_camera3d_t g_orbit DINO_WRAM_L;
static int g_show_hud DINO_WRAM_L;
static int g_draw_overflow DINO_WRAM_L;
static int g_ntsc DINO_WRAM_L;
static uint32_t g_tick DINO_WRAM_L;

static sat_anim_state_t g_anim DINO_WRAM_L;
/* The decode job reads its own copy of the state, so advancing g_anim never
 * races the Slave. */
static sat_anim_state_t g_job_state DINO_WRAM_L;
static sat_anim_decode_job_t g_job DINO_WRAM_L;
static sat_parallel_handle_t g_job_handle DINO_WRAM_L;
static int g_job_pending DINO_WRAM_L;
static uint8_t g_pose_read DINO_WRAM_L;

#define PARALLEL_TIMEOUT 60000u

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
    sat_pad_state_t camera_pad = *pad;
    /* Adapt the shared viewer controls for the Saturn pad layout requested
     * here: shoulder buttons orbit, X/Y zoom. Keep D-pad pitch and C toggle. */
    camera_pad.held &= (uint16_t)~(SAT_PAD_LEFT | SAT_PAD_RIGHT |
                                   SAT_PAD_L | SAT_PAD_R);
    if ((pad->held & SAT_PAD_L) != 0u) camera_pad.held |= SAT_PAD_LEFT;
    if ((pad->held & SAT_PAD_R) != 0u) camera_pad.held |= SAT_PAD_RIGHT;
    if ((pad->held & SAT_PAD_X) != 0u) camera_pad.held |= SAT_PAD_R;
    if ((pad->held & SAT_PAD_Y) != 0u) camera_pad.held |= SAT_PAD_L;
    sat_example_must(sat_orbit_camera3d_apply_pad(&g_orbit,&camera_pad,SAT_PAD_C));
    if((pad->pressed&SAT_PAD_START)!=0u)
        g_show_hud=!g_show_hud;
    if((pad->pressed&SAT_PAD_B)!=0u) {
        g_orbit.yaw_deg = MODEL_START_YAW_DEG;
        note(sat_orbit_camera3d_update(&g_orbit));
        note(sat_anim_reset(&g_anim));
        sat_anim_set_paused(&g_anim,0);
    }
}
static void update_animation_from_inputs(const sat_pad_state_t *pad) {
    if ((pad->pressed & SAT_PAD_A) != 0u) {
        sat_anim_set_paused(&g_anim, !sat_anim_is_paused(&g_anim));
    }
    note(sat_anim_advance(&g_anim, &trex_anim_asset, vblank_dt()));
}

/* Waits for a Slave task and reports whether it completed. A task still
 * running after the timeout is stopped before its buffers are reused. */
static sat_result_t finish_task(sat_parallel_handle_t handle) {
    sat_result_t st = sat_parallel_wait(handle, PARALLEL_TIMEOUT);
    if (st == SAT_OK) {
        st = sat_parallel_result(handle, NULL);
    }
    if (sat_parallel_state(handle) == SAT_PARALLEL_RUNNING) {
        (void)sat_parallel_abort(handle, PARALLEL_TIMEOUT);
        st = SAT_ERR_TIMEOUT;
    }
    return st;
}

/* Pose for this frame: the Slave decoded it while the previous frame was
 * being emitted. Falls back to a Master decode whenever the Slave path is
 * unavailable, so the demo never stalls. */
static void collect_pose(void) {
    sat_result_t st = SAT_ERR_UNSUPPORTED;

    if (g_job_pending) {
        st = finish_task(g_job_handle);
        (void)sat_parallel_release(g_job_handle);
        g_job_pending = 0;
    }
    if (st == SAT_OK) {
        g_pose_read = (uint8_t)(g_pose_read ^ 1u);
    } else {
        note(sat_anim_decode(&trex_anim_asset, &g_anim,
            g_pose[g_pose_read], MODEL_VERTEX_CAP));
    }
    g_mesh.vertices = g_pose[g_pose_read];
    g_half_mesh[0].vertices = &g_pose[g_pose_read][g_half_first[0]];
    g_half_mesh[1].vertices = &g_pose[g_pose_read][g_half_first[1]];
}

/* Queue the next frame's pose on the Slave, into the buffer not drawn. */
static void request_pose(void) {
    g_job_state = g_anim;
    g_job.asset = &trex_anim_asset;
    g_job.state = &g_job_state;
    g_job.output = g_pose[g_pose_read ^ 1u];
    g_job.vertex_cap = MODEL_VERTEX_CAP;
    g_job_pending = sat_anim_decode_async(&g_job, &g_job_handle) == SAT_OK;
}

/* Narrows a half's mesh view to the vertices its faces use, rebasing its
 * slice of the index table onto that window. */
static uint16_t bind_vertex_window(sat_mesh_t *half) {
    uint16_t lo = 0xFFFFu;
    uint16_t hi = 0u;
    uint32_t i;

    for (i = 0u; i < (uint32_t)half->face_count * 4u; ++i) {
        if (half->indices[i] < lo) lo = half->indices[i];
        if (half->indices[i] > hi) hi = half->indices[i];
    }
    for (i = 0u; i < (uint32_t)half->face_count * 4u; ++i) {
        half->indices[i] = (uint16_t)(half->indices[i] - lo);
    }
    half->vertex_count = (uint16_t)(hi - lo + 1u);
    half->vertex_cap = half->vertex_count;
    return lo;
}

/* Prepare both face halves, the Slave's concurrently with the Master's.
 * Whatever the Slave cannot do, the Master does, so a frame is never short
 * of faces. */
static void submit_model(void) {
    int slave_pending =
        sat_scene_prepare_batch_async(&g_scene, &g_slave_batch, &g_slave_handle) == SAT_OK;
    int slave_done = 0;

    note(sat_scene_submit_instance(&g_scene, &g_half[0],
        SAT_SCENE3D_SLOT_INHERIT, g_mesh_screen, NULL));
    if (slave_pending) {
        if (finish_task(g_slave_handle) == SAT_OK) {
            note(sat_scene_merge_prepared_batch(&g_scene, &g_slave_batch, g_slave_handle));
            slave_done = 1;
        }
        (void)sat_scene_prepare_batch_release(&g_slave_batch, g_slave_handle);
    }
    if (!slave_done) {
        note(sat_scene_submit_instance(&g_scene, &g_half[1],
            SAT_SCENE3D_SLOT_INHERIT, g_slave_screen, NULL));
    }
}

/* The RGB color of a baked texture that is a single color, or 0 when it has
 * detail worth a texture fetch. Code 0 is the transparent texel code, so it
 * never counts as a color. */
static uint16_t solid_texture_color(const sat_model_texture_asset_t *t) {
    const int lut4 = (t->flags & SAT_MODEL_TEXTURE_LUT4) != 0u;
    const uint32_t bytes = lut4 ? t->pixel_count / 2u : t->pixel_count;
    const uint8_t first = t->pixels[0];
    uint32_t i;

    for (i = 1u; i < bytes; ++i) {
        if (t->pixels[i] != first) {
            return 0u;
        }
    }
    if (lut4) {
        const uint8_t code = (uint8_t)(first & 0x0Fu);
        if ((first >> 4) != code || code == 0u) {
            return 0u;
        }
        return trex_asset.luts_rgb555[(uint32_t)t->palette_slot * 16u + code];
    }
    return first != 0u ? trex_asset.palettes_rgb555[first] : 0u;
}

static void draw_text(const char *text, int x, int y) {
    sat_result_t st = sat_ascii_font_draw_text_screen_indexed8(
        &g_font, text, x, y, 8, 0, 0);
    (void)st;
}

static void draw_hud(void) {
    if (!g_show_hud) {
        return;
    }
    draw_text("T-REX WALK  L/R ROT  X/Y ZOOM", 4, 4);
    draw_text("A PAUSE B RESET C AUTO START HUD", 4, 14);
    draw_text("made using celsowm/libsaturn", 48, 210);
    if (g_draw_overflow) {
        draw_text("RENDER LIMIT", 4, 24);
    }
}

int main(void) {
    sat_video_config_t video = {SCREEN_W, SCREEN_H, 1u, 0u};
    sat_vec3_t mn = {0, 0, 0};
    sat_vec3_t mx = {0, 0, 0};

    SAT_PANIC_IF_ERROR(sat_init(&video));
    SAT_PANIC_IF_ERROR(sat_ascii_font_init_8x8_indexed8(
        &g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, FONT_PALETTE));
    g_ntsc = (int)video.ntsc;

    sat_example_must(sat_model_validate(&trex_asset));
    sat_example_must(sat_anim_validate(&trex_anim_asset));
    sat_example_must(sat_mesh_init(
        &g_mesh, g_pose[0], MODEL_VERTEX_CAP, g_mesh_indices, MODEL_FACE_CAP));
    sat_example_must(sat_model_copy_to_mesh(&trex_asset, &g_mesh));
    /* One palette upload + one pixel upload per unique texture, once. */
    sat_example_must(sat_model_upload_textures(
        &trex_asset, g_model_textures, MODEL_TEXTURE_CAP));
    sat_example_must(sat_anim_state_init(&g_anim, &trex_anim_asset, 0));
    sat_example_must(sat_scene_init(&g_scene, g_scene_faces, g_scene_keys,
        g_scene_order, MODEL_FACE_CAP));
    g_instance.mesh = &g_mesh;
    g_instance.materials = g_scene_materials;
    g_instance.material_count = MODEL_TEXTURE_CAP;
    g_instance.face_materials = trex_asset.face_texture_indices;
    g_instance.world = NULL;
    g_instance.pass = 0u;
    g_instance.cull_backfaces = 1u;
    g_half_mesh[0] = g_mesh;
    g_half_mesh[0].face_count = MASTER_FACES;
    g_half_mesh[1] = g_mesh;
    g_half_mesh[1].indices = &g_mesh_indices[MASTER_FACES * 4u];
    g_half_mesh[1].face_count = SLAVE_FACES;
    g_half_mesh[1].face_cap = SLAVE_FACES;
    g_half_first[0] = bind_vertex_window(&g_half_mesh[0]);
    g_half_first[1] = bind_vertex_window(&g_half_mesh[1]);
    g_half[0] = g_instance;
    g_half[0].mesh = &g_half_mesh[0];
    g_half[1] = g_instance;
    g_half[1].mesh = &g_half_mesh[1];
    g_half[1].face_materials = &trex_asset.face_texture_indices[MASTER_FACES];
    g_slave_item.instance = &g_half[1];
    g_slave_item.screen_scratch = g_slave_screen;
    g_slave_item.color_calc_slot = SAT_SCENE3D_SLOT_INHERIT;
    sat_example_must(sat_scene3d_prepare_batch_init(&g_slave_batch, &g_slave_item, 1u,
        g_slave_faces, g_slave_keys, g_slave_order, SLAVE_FACES));
    g_slave_batch.dispatch = SAT_SCENE3D_PREPARE_DISPATCH_RUNTIME;
    {
        uint16_t f;
        for (f = 0; f < MODEL_TEXTURE_CAP; ++f) {
            const uint16_t solid = solid_texture_color(&trex_asset.textures[f]);
            g_scene_materials[f].color_calc_slot = SAT_INDEXED_SOLID_OPAQUE;
            if (solid != 0u) {
                g_scene_materials[f].kind = SAT_SCENE3D_RGB;
                g_scene_materials[f].rgb555 = solid;
            } else {
                g_scene_materials[f].kind = SAT_SCENE3D_INDEXED_TEXTURED;
                g_scene_materials[f].texture = &g_model_textures[f];
            }
        }
    }

    /* Use UNION bounds across all animation clips, not just the bind pose. */
    anim_bounds(&trex_anim_asset, &mn, &mx);
    {
        sat_orbit_camera3d_fit_t fit={0};
        fit.min_extent=SAT_FX16_ONE>>6;
        fit.initial_distance_factor=(SAT_FX16_ONE*3)/4;
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
        g_orbit.yaw_deg = MODEL_START_YAW_DEG;
        sat_example_must(sat_orbit_camera3d_update(&g_orbit));
    }
    g_show_hud = 1;
    g_draw_overflow = 0;
    /* The Slave decodes poses; without it every decode runs on the Master. */
    if (sat_anim_parallel_register() == SAT_OK) {
        sat_parallel_config_t config = {SAT_PARALLEL_AUTO, 0, 0u, 0u, PARALLEL_TIMEOUT};
        (void)sat_parallel_init(&config);
    }
    g_pose_read = 0u;
    g_job_pending = 0;
    g_tick = sat_frame_count();
    while (1) {
        sat_pad_state_t pad = {0};

        SAT_PANIC_IF_ERROR(sat_wait_vblank());
        SAT_PANIC_IF_ERROR(sat_vdp2_back_color_set(SAT_COLOR_BLACK));
        SAT_PANIC_IF_ERROR(sat_set_clear_color(SAT_COLOR_BLACK));
        SAT_PANIC_IF_ERROR(sat_begin_frame());
        SAT_PANIC_IF_ERROR(sat_pad_poll(&pad));

        update_camera_from_inputs(&pad);
        update_animation_from_inputs(&pad);
        /* Draw the pose the Slave decoded during the previous frame. */
        collect_pose();
        if (!g_draw_overflow) {
            sat_camera3d_t camera = {};
            camera.eye = g_orbit.eye;
            camera.target = g_orbit.target;
            camera.up = (sat_vec3_t){0, SAT_FX16_ONE, 0};
            camera.view_proj = g_orbit.view_proj;
            note(sat_scene_begin(&g_scene, &camera, g_orbit.near_z,
                SCREEN_W, SCREEN_H, 64u));
            submit_model();
            request_pose();
            note(sat_scene_flush(&g_scene));
        } else {
            request_pose();
        }
        draw_hud();

        SAT_PANIC_IF_ERROR(sat_end_frame());
    }

    return 0;
}
