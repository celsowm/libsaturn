/* skeletal_animation_3d - Phase-8A MVP with a decimated rigged humanoid.
 * See assets/ROBOT_POLICE_ATTRIBUTION.md for the source and CC BY 4.0
 * license. The importer reduces the source to a mesh that fits stock
 * Saturn RAM; the runtime evaluates the skeleton and skins it each frame.
 */

#include "saturn/saturn.h"
#include "saturn/font.h"
#include "saturn/loader.h"
#include "saturn/mesh3d.h"
#include "saturn/model3d.h"
#include "saturn/orbit_camera3d.h"
#include "saturn/render2d.h"
#include "saturn/scene.h"
#include "saturn/skeleton3d.h"
#include "saturn/skin3d.h"
#include "saturn/video.h"
#include "saturn/time.h"
#include "saturn/parallel.h"

#include "examples/common/example_util.h"

/* Keep the hot deform/project/painter buffers on the 32-bit WRAM-H bus.
 * The 30 fps clip leaves room for them plus the linker's Master stack guard.
 * Library/CD staging buffers continue to use the separate WRAM-L bank. */
#define SAT_SCN

/* The asset is committed under assets/ (CC BY 4.0). The Makefile runs
 * tools/import_model.py --target skeletal --max-triangles 1800 on the
 * source GLB, dropping the 28,073-vertex / 20,053-face original down
 * to 933 shared deformation vertices / 1,799 faces. The exact counts
 * come from the generated header so the example stays in sync
 * with the importer. */
#include "robot_police_skeletal.h"

#define ASSET_VERTEX_COUNT ROBOT_POLICE_SKELETAL_VERTEX_COUNT
#define ASSET_FACE_COUNT ROBOT_POLICE_SKELETAL_FACE_COUNT

/* Forward declarations for the imported asset symbols. */
extern const sat_skeleton_asset_t robot_police_skeletal_skeleton;
extern const sat_skin_asset_t robot_police_skeletal_skin;
extern const uint16_t robot_police_skeletal_indices[ROBOT_POLICE_SKELETAL_FACE_COUNT * 4u];
extern const sat_skeleton_clip_t robot_police_skeletal_clip__3903460652032_TempMotion;

#define VISIBLE_FACE_COUNT ASSET_FACE_COUNT
#define MESH_VERTEX_CAP ASSET_VERTEX_COUNT
#define SCENE_FACE_CAP (VISIBLE_FACE_COUNT + 32u)
#define SCENE_ORDER_CAP SCENE_FACE_CAP
#define FONT_PALETTE 0
#define SCREEN_W 320u
#define SCREEN_H 224u

static sat_skeleton_instance_t g_skeleton;
static sat_transform3d_node_t g_skeleton_nodes[128];
static uint16_t g_skeleton_scratch[128];
static sat_skel_clip_player_t g_player;
static sat_skeleton_pose_joint_t g_pose[128];
static sat_mat4_t g_joint_world[128];

static sat_vec3_t g_mesh_vertices[MESH_VERTEX_CAP] SAT_SCN;
static uint16_t g_mesh_indices[VISIBLE_FACE_COUNT * 4u] SAT_SCN;
static sat_mesh_t g_mesh SAT_SCN;
static sat_projected_vertex_t g_mesh_screen[MESH_VERTEX_CAP] SAT_SCN;
static sat_scene3d_face_t g_scene_faces[SCENE_FACE_CAP] SAT_SCN;
static uint32_t g_scene_keys[SCENE_ORDER_CAP] SAT_SCN;
static uint16_t g_scene_order[SCENE_ORDER_CAP] SAT_SCN;
static sat_scene_t g_scene;
static sat_scene3d_instance_t g_instance;
static sat_scene3d_instance_binding_t g_binding;
static uint16_t g_face_materials[VISIBLE_FACE_COUNT];
static sat_scene3d_material_t g_materials[ROBOT_POLICE_SKELETAL_TEXTURE_COUNT];
static sat_vdp1_texture_t g_model_textures[ROBOT_POLICE_SKELETAL_TEXTURE_COUNT];
static sat_ascii_font_t g_font;
static sat_loading_screen_t g_loading;
static sat_orbit_camera3d_t g_orbit;
static int g_pause;
static uint32_t g_fps;
static uint32_t g_fps_frames;
static uint32_t g_fps_mark;
static sat_result_t g_last_submit = SAT_OK;
/* Last completed frame costs in milliseconds; available in harness RAM dumps. */
volatile struct {
    uint32_t frames, sample_ms, skeleton_ms, skin_ms, submit_ms, flush_ms, total_ms;
} g_skeletal_profile;
static uint32_t g_animation_mark;
static sat_fx16_t g_floor_y, g_floor_step;
static sat_vec3_t g_floor_center;

#ifndef SKELETAL_USE_PARALLEL
#define SKELETAL_USE_PARALLEL 1
#endif
/* Harness-visible startup state and recovery count. */
volatile struct {
    uint32_t init_error, slave_ready, serial_fallbacks;
} g_skeletal_parallel;

#if SKELETAL_USE_PARALLEL
typedef struct {
    uint32_t split, count;
    sat_result_t status[2];
} skin_job_t;

static void deform_partition(void* context, uint32_t first, uint32_t last) {
    skin_job_t* job = (skin_job_t*)context;
    for (uint32_t lane = first; lane < last; ++lane) {
        uint32_t begin = lane ? job->split : 0u;
        uint32_t end = lane ? job->count : job->split;
        job->status[lane] = sat_skin_deform_range(&robot_police_skeletal_skin,
            g_joint_world, robot_police_skeletal_skeleton.joint_count,
            begin, end - begin, g_mesh.vertices, g_mesh.vertex_cap);
    }
}
#endif

static sat_result_t update_skin(void) {
#if SKELETAL_USE_PARALLEL
    if (g_skeletal_parallel.slave_ready && sat_parallel_slave_available()) {
        /* Rigid records are cheaper: Master takes them plus half the blends. */
        skin_job_t job = {
            robot_police_skeletal_skin.rigid_count + robot_police_skeletal_skin.blend_count / 2u,
            sat_skin_vertex_count(&robot_police_skeletal_skin), {SAT_OK, SAT_OK}};
        sat_result_t result = sat_parallel_for(0u, 2u, 1u, deform_partition, &job,
            g_mesh.vertices, job.count * sizeof(sat_vec3_t));
        sat_parallel_cache_sync_range(job.status, sizeof(job.status));
        if (result == SAT_OK && job.status[0] == SAT_OK && job.status[1] == SAT_OK) {
            g_mesh.vertex_count = (uint16_t)job.count;
            return SAT_OK;
        }
        if (result != SAT_OK) {
            /* A failed join may still own the output. Stop the runtime before
             * serial recovery; never write over an unconfirmed worker. */
            sat_result_t stopped = sat_parallel_shutdown(60000u);
            if (stopped != SAT_OK) return stopped;
            g_skeletal_parallel.slave_ready = 0u;
        }
        ++g_skeletal_parallel.serial_fallbacks;
    }
#endif
    return sat_skin_update_mesh(&robot_police_skeletal_skin, g_joint_world,
        robot_police_skeletal_skeleton.joint_count, &g_mesh, 0, NULL);
}

static void draw_stage(void) {
    sat_scene3d_material_t material = {0};
    material.kind = SAT_SCENE3D_RGB;
    material.color_calc_slot = SAT_INDEXED_SOLID_OPAQUE;
    for (int z = -1; z <= 1; ++z) for (int x = -1; x <= 1; ++x) {
        sat_fx16_t left = g_floor_center.x + (2 * x - 1) * g_floor_step;
        sat_fx16_t right = left + 2 * g_floor_step;
        sat_fx16_t front = g_floor_center.z + (2 * z - 1) * g_floor_step;
        sat_fx16_t back = front + 2 * g_floor_step;
        sat_quad3_t tile = {{{left, g_floor_y, front}, {right, g_floor_y, front},
                             {right, g_floor_y, back}, {left, g_floor_y, back}}};
        material.rgb555 = ((x + z) & 1) ? SAT_RGB555(5, 6, 8) : SAT_RGB555(6, 7, 9);
        sat_example_must(sat_scene_submit_quad(&g_scene, &tile, &material, 0u));
    }
    /* Cheap contact shadow: eight solid triangles, independent of skin size. */
    static const int16_t ring[9][2] = {{256,0},{181,181},{0,256},{-181,181},
        {-256,0},{-181,-181},{0,-256},{181,-181},{256,0}};
    material.rgb555 = SAT_RGB555(2, 3, 4);
    sat_fx16_t rx = g_floor_step, rz = g_floor_step / 2;
    sat_vec3_t center = {g_floor_center.x, g_floor_y + 64, g_floor_center.z};
    for (uint16_t i = 0; i < 8; ++i) {
        sat_vec3_t a = {center.x + rx * ring[i][0] / 256, center.y,
                        center.z + rz * ring[i][1] / 256};
        sat_vec3_t b = {center.x + rx * ring[i+1][0] / 256, center.y,
                        center.z + rz * ring[i+1][1] / 256};
        sat_quad3_t shadow = {{center, a, b, b}};
        sat_example_must(sat_scene_submit_quad(&g_scene, &shadow, &material, 1u));
    }
}

static void update_fps(void) {
    g_fps_frames++;
    uint32_t now = sat_time_ms();
    if (now - g_fps_mark >= 1000u) {
        g_fps = (g_fps_frames * 10000u + (now - g_fps_mark) / 2u) / (now - g_fps_mark);
        g_fps_mark = now;
        g_fps_frames = 0;
    }
}

static void handle_input(const sat_pad_state_t* pad) {
    if ((pad->pressed & SAT_PAD_START) != 0u) g_pause = !g_pause;
    sat_example_must(sat_orbit_camera3d_apply_pad(&g_orbit, pad, SAT_PAD_C));
}

static void draw_hud(void) {
    char line[64];
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(
        &g_font, "ROBOT POLICE / SKELETAL DEMO",
        8, 8, 8, 0u, 0u));
    sat_example_must(sat_fmt_label_u32("SUB/S   ", g_fps / 10u, line, sizeof(line), 0));
    unsigned length = 0;
    while (line[length]) ++length;
    line[length++] = '.';
    line[length++] = (char)('0' + g_fps % 10u);
    line[length] = 0;
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(&g_font, line, 8, 24, 8, 0u, 0u));
    const char* status = (g_last_submit == SAT_OK) ? "OK"
                       : (g_last_submit == SAT_ERR_CAPACITY) ? "VDP1 OVERFLOW"
                       : "ERROR";
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(
        &g_font, status, 272, 24, 8, 0u, 0u));
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(
        &g_font, g_pause ? "PAUSED" : "LOOPING", 240, 40, 8, 0u, 0u));
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(
        &g_font, "D-PAD ORBIT  L/R ZOOM  START PAUSE", 8, 208, 8, 0u, 0u));
}

int main(void) {
    sat_video_config_t video = {SCREEN_W, SCREEN_H, SAT_VIDEO_AUTO, 0u};
    sat_example_must(sat_init(&video));
#if SKELETAL_USE_PARALLEL
    sat_parallel_config_t parallel = {SAT_PARALLEL_AUTO, NULL, 0u, 0u, 60000u};
    g_skeletal_parallel.init_error = (uint32_t)(-sat_parallel_init(&parallel));
    g_skeletal_parallel.slave_ready = sat_parallel_slave_available();
#endif
    sat_example_must(sat_vdp2_back_color_set(SAT_BGR555(3, 4, 6)));
    sat_example_must(sat_ascii_font_init_8x8_indexed8(
        &g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, FONT_PALETTE));

    sat_loading_screen_default(&g_loading);
    sat_example_loading_frame(&g_font, &g_loading, "SKELETAL ANIMATION 3D",
        "INITIALISING SCENE", 8);

    sat_example_must(sat_scene_init(&g_scene, g_scene_faces, g_scene_keys,
        g_scene_order, SCENE_FACE_CAP));
    sat_example_must(sat_mesh_init(&g_mesh, g_mesh_vertices, MESH_VERTEX_CAP,
        g_mesh_indices, VISIBLE_FACE_COUNT));

    sat_example_must(sat_model_validate(&robot_police_skeletal_asset));
    sat_example_must(sat_model_copy_to_mesh(&robot_police_skeletal_asset, &g_mesh));
    sat_example_must(sat_model_upload_textures(&robot_police_skeletal_asset,
        g_model_textures, ROBOT_POLICE_SKELETAL_TEXTURE_COUNT));
    for (uint16_t t = 0; t < ROBOT_POLICE_SKELETAL_TEXTURE_COUNT; ++t) {
        g_materials[t].kind = SAT_SCENE3D_INDEXED_TEXTURED;
        g_materials[t].texture = &g_model_textures[t];
        g_materials[t].color_calc_slot = SAT_INDEXED_SOLID_OPAQUE;
    }
    for (uint16_t f = 0; f < VISIBLE_FACE_COUNT; ++f)
        g_face_materials[f] = robot_police_skeletal_asset.face_texture_indices[f];

    sat_example_loading_frame(&g_font, &g_loading, "SKELETAL ANIMATION 3D",
        "INITIALISING SKELETON", 32);
    sat_example_must(sat_skeleton_instance_init(&g_skeleton,
        &robot_police_skeletal_skeleton, g_skeleton_nodes, 128,
        g_skeleton_scratch, 128));
    sat_example_must(sat_skel_clip_player_init(&g_player,
        &robot_police_skeletal_clip__3903460652032_TempMotion));

    sat_example_loading_frame(&g_font, &g_loading, "SKELETAL ANIMATION 3D",
        "INITIALISING INSTANCE", 64);

    g_instance.mesh = &g_mesh;
    g_instance.materials = g_materials;
    g_instance.material_count = ROBOT_POLICE_SKELETAL_TEXTURE_COUNT;
    g_instance.face_materials = g_face_materials;
    g_instance.world = NULL;
    g_instance.pass = 2u;
    g_instance.cull_backfaces = 1u;
    sat_example_must(sat_scene3d_instance_binding_init(&g_binding, &g_mesh,
        g_materials, ROBOT_POLICE_SKELETAL_TEXTURE_COUNT, g_face_materials));
    g_instance.validated_binding = &g_binding;

    /* Keep the original source bounds even when quantized skin records share
     * a representative bind vertex. Camera framing must not change. */
    sat_vec3_t mn = robot_police_skeletal_bind_min;
    sat_vec3_t mx = robot_police_skeletal_bind_max;
    g_floor_y = mn.y - SAT_FX16_ONE / 32;
    g_floor_step = (mx.y - mn.y) / 5;
    g_floor_center.x = (mn.x + mx.x) / 2;
    g_floor_center.z = (mn.z + mx.z) / 2;
    sat_orbit_camera3d_fit_t fit = {0};
    fit.min_extent = SAT_FX16_ONE >> 6;
    fit.initial_distance_factor = SAT_FX16_ONE + (SAT_FX16_ONE >> 3);
    fit.min_distance_factor = SAT_FX16_ONE / 3;
    fit.max_distance_factor = sat_fx16_from_int(16);
    fit.near_plane_factor = SAT_FX16_ONE / 8;
    fit.near_plane_floor = 1;
    fit.fov_y = sat_fx16_from_int(60);
    fit.aspect = sat_fx16_div(sat_fx16_from_int(SCREEN_W), sat_fx16_from_int(SCREEN_H));
    fit.far_z = sat_fx16_from_int(1000);
    fit.pitch_min_deg = -60;
    fit.pitch_max_deg = 60;
    sat_example_must(sat_orbit_camera3d_fit_bounds(&g_orbit, &mn, &mx, &fit));

    sat_example_loading_frame(&g_font, &g_loading, "SKELETAL ANIMATION 3D",
        "READY", 100);

    g_pause = 0;
    g_fps = 0;
    g_fps_frames = 0;
    g_fps_mark = sat_time_ms();
    g_animation_mark = sat_time_ms();

    while (1) {
        sat_pad_state_t pad = {0};
        sat_example_must(sat_wait_vblank());
        sat_example_must(sat_begin_frame());
        sat_example_must(sat_pad_poll(&pad));
        handle_input(&pad);
        uint32_t frame_start = sat_time_ms();
        uint32_t elapsed = frame_start - g_animation_mark;
        g_animation_mark = frame_start;
        if (elapsed > 250u) elapsed = 250u;

        if (!g_pause) {
            sat_example_must(sat_skel_clip_player_advance(
                &g_player, (sat_fx16_t)(elapsed * SAT_FX16_ONE / 1000u), NULL, 0, NULL, NULL));
            sat_example_must(sat_skel_clip_player_sample(
                &g_player, g_pose, robot_police_skeletal_skeleton.joint_count));
            uint32_t stamp = sat_time_ms();
            g_skeletal_profile.sample_ms = stamp - frame_start;
            sat_example_must(sat_skeleton_evaluate(
                &g_skeleton, g_pose, robot_police_skeletal_skeleton.joint_count));
            g_skeletal_profile.skeleton_ms = sat_time_ms() - stamp;
            stamp = sat_time_ms();
            for (uint16_t j = 0; j < robot_police_skeletal_skeleton.joint_count; ++j) {
                g_joint_world[j] = g_skeleton_nodes[j].world;
            }
            sat_example_must(update_skin());
            g_skeletal_profile.skin_ms = sat_time_ms() - stamp;
        }
        {
            sat_camera3d_t camera = {0};
            camera.eye = g_orbit.eye;
            camera.target = g_orbit.target;
            camera.up = (sat_vec3_t){0, SAT_FX16_ONE, 0};
            camera.view_proj = g_orbit.view_proj;
            sat_example_must(sat_scene_begin(&g_scene, &camera, g_orbit.near_z,
                sat_video_width(), sat_video_height(), 96u));
            draw_stage();
            uint32_t stamp = sat_time_ms();
            /* Preserve the material's opaque slot for the shared scene path. */
            g_last_submit = sat_scene_submit_instance(&g_scene, &g_instance,
                SAT_SCENE3D_SLOT_INHERIT,
                g_mesh_screen, NULL);
            g_skeletal_profile.submit_ms = sat_time_ms() - stamp;
            stamp = sat_time_ms();
            sat_example_must(sat_scene_flush(&g_scene));
            g_skeletal_profile.flush_ms = sat_time_ms() - stamp;
        }
        update_fps();
        draw_hud();
        g_skeletal_profile.total_ms = sat_time_ms() - frame_start;
        g_skeletal_profile.frames++;
        sat_example_must(sat_end_frame());
    }
    return 0;
}
