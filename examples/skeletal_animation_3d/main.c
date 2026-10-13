/* skeletal_animation_3d - Phase-8A MVP, currently MEMORY-BLOCKED.
 *
 * See assets/ROBOT_POLICE_ATTRIBUTION.md for the asset source and the CC
 * BY 4.0 license that ships with it. This source file is committed so
 * the integration is reviewable, but the example does not currently link:
 *
 *   The compressed asset is 600+ KB of static data and the runtime
 *   deforms the full 28,073-vertex pool every frame, so even with the
 *   working buffers moved to WRAM-L (see SAT_SCN below) the link fails
 *   by ~340 KB. The full mesh + 102 joints + 20,053 faces simply does not
 *   fit on a stock Saturn. Closing Phase 8A needs one of:
 *
 *     - a decimated variant of the asset (5,000-8,000 vertices keeps the
 *       rigged silhouette convincing while fitting in WRAMH);
 *     - or a smaller rigged asset (the original Phase-8A intent was a
 *       ~1,000-triangle demo character with one looping clip).
 *
 * The runtime path itself is verified by tests/host/test_skeleton3d.cpp
 * and tests/host/test_skin3d.cpp. The asset generation pipeline
 * (tools/import_model.py --target skeletal + the host-run importer) is
 * verified end-to-end on this exact GLB. What this example needs is a
 * fitting mesh; see docs/SKELETAL_ANIMATION_3D.md for the rest of the
 * status. */

#include "saturn/saturn.h"
#include "saturn/font.h"
#include "saturn/loader.h"
#include "saturn/mesh3d.h"
#include "saturn/orbit_camera3d.h"
#include "saturn/render2d.h"
#include "saturn/scene.h"
#include "saturn/skeleton3d.h"
#include "saturn/skin3d.h"
#include "saturn/video.h"

#include "examples/common/example_util.h"

/* Working buffers in Work RAM Low. WRAML is the 1 MB bank at 0x00200000 -
 * separate from WRAMH, accessed through one extra wait state that the
 * deform pass doesn't notice. crt0.s zeroes it at boot. */
#define SAT_SCN __attribute__((section(".wram_l")))

/* The asset is committed under assets/ (CC BY 4.0). The Makefile runs
 * tools/import_model.py --target skeletal --max-triangles 1000 on the
 * source GLB, dropping the 28,073-vertex / 20,053-face original down
 * to ~940 vertices / ~1000 faces (~90 KB ROM total). The exact counts
 * come from the committed gen/ header so the example stays in sync
 * with the importer. */
#include "robot_police_skeletal.h"

#define ASSET_VERTEX_COUNT ROBOT_POLICE_SKELETAL_VERTEX_COUNT
#define ASSET_FACE_COUNT ROBOT_POLICE_SKELETAL_FACE_COUNT

/* Forward declarations for the imported asset symbols. */
extern const sat_skeleton_asset_t robot_police_skeletal_skeleton;
extern const sat_skin_asset_t robot_police_skeletal_skin;
extern const uint16_t robot_police_skeletal_indices[ROBOT_POLICE_SKELETAL_FACE_COUNT * 4u];
extern const sat_skeleton_clip_t robot_police_skeletal_clip__3903460652032_TempMotion;

#define FACE_STRIDE 1u /* decimated mesh already at 1000 tris; render all */
#define VISIBLE_FACE_COUNT ASSET_FACE_COUNT
#define MESH_VERTEX_CAP ASSET_VERTEX_COUNT
#define SCENE_FACE_CAP VISIBLE_FACE_COUNT
#define SCENE_ORDER_CAP SCENE_FACE_CAP
#define FONT_PALETTE 1
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
static uint16_t g_face_materials[VISIBLE_FACE_COUNT];
static sat_scene3d_material_t g_materials[VISIBLE_FACE_COUNT];
static sat_ascii_font_t g_font;
static sat_loading_screen_t g_loading;
static sat_orbit_camera3d_t g_orbit;
static int g_pause;
static uint32_t g_fps;
static uint32_t g_fps_frames;
static uint32_t g_fps_mark;
static sat_result_t g_last_submit = SAT_OK;

static void update_fps(void) {
    g_fps_frames++;
    uint32_t now = sat_frame_count();
    if (now - g_fps_mark >= 60u) {
        g_fps = g_fps_frames * 60u / (now - g_fps_mark);
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
        &g_font, "SKELETAL ANIMATION 3D  PHASE-8A MVP",
        8, 8, 8, 0u, 0u));
    sat_example_must(sat_fmt_label_u32("FPS     ", g_fps, line, sizeof(line), 0));
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(&g_font, line, 8, 24, 8, 0u, 0u));
    sat_example_must(sat_fmt_label_u32("JOINTS  ",
        robot_police_skeletal_skeleton.joint_count, line, sizeof(line), 0));
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(&g_font, line, 8, 40, 8, 0u, 0u));
    sat_example_must(sat_fmt_label_u32("VERTS   ", MESH_VERTEX_CAP, line, sizeof(line), 0));
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(&g_font, line, 8, 56, 8, 0u, 0u));
    sat_example_must(sat_fmt_label_u32("FACES   ", VISIBLE_FACE_COUNT, line, sizeof(line), 0));
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(&g_font, line, 8, 72, 8, 0u, 0u));
    const char* status = (g_last_submit == SAT_OK) ? "OK"
                       : (g_last_submit == SAT_ERR_CAPACITY) ? "VDP1 OVERFLOW"
                       : "ERROR";
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(
        &g_font, status, 8, 96, 8, 0u, 0u));
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(
        &g_font, g_pause ? "PAUSED" : "LOOPING", 8, 112, 8, 0u, 0u));
}

int main(void) {
    sat_video_config_t video = {SCREEN_W, SCREEN_H, SAT_VIDEO_AUTO, 0u};
    sat_example_must(sat_init(&video));
    sat_example_must(sat_ascii_font_init_8x8_indexed8(
        &g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, FONT_PALETTE));

    sat_loading_screen_default(&g_loading);
    sat_example_loading_frame(&g_font, &g_loading, "SKELETAL ANIMATION 3D",
        "INITIALISING SCENE", 8);

    sat_example_must(sat_scene_init(&g_scene, g_scene_faces, g_scene_keys,
        g_scene_order, SCENE_FACE_CAP));
    sat_example_must(sat_mesh_init(&g_mesh, g_mesh_vertices, MESH_VERTEX_CAP,
        g_mesh_indices, VISIBLE_FACE_COUNT));

    for (uint32_t f = 0; f < VISIBLE_FACE_COUNT; ++f) {
        uint32_t src = f * FACE_STRIDE;
        for (uint32_t k = 0; k < 4u; ++k) {
            g_mesh_indices[f * 4u + k] =
                robot_police_skeletal_indices[src * 4u + k];
        }
        g_face_materials[f] = (uint16_t)f;
        g_materials[f].kind = SAT_SCENE3D_RGB;
        g_materials[f].color_calc_slot = SAT_INDEXED_SOLID_OPAQUE;
        g_materials[f].rgb555 = SAT_RGB555(20, 22, 26);
    }
    g_mesh.face_count = VISIBLE_FACE_COUNT;

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
    g_instance.material_count = VISIBLE_FACE_COUNT;
    g_instance.face_materials = g_face_materials;
    g_instance.world = NULL;
    g_instance.pass = 0u;
    g_instance.cull_backfaces = 1u;

    g_orbit = (sat_orbit_camera3d_t){
        .target = (sat_vec3_t){0, 0, 0},
        .distance = sat_fx16_from_int(8),
        .default_distance = sat_fx16_from_int(8),
        .min_distance = sat_fx16_from_int(2),
        .max_distance = sat_fx16_from_int(20),
        .near_z = 64,
        .far_z = sat_fx16_from_int(1000),
        .fov_y = sat_fx16_from_int(60),
        .aspect = sat_fx16_from_int(4) / 3,
        .yaw_deg = 0,
        .pitch_deg = 0,
    };

    sat_example_loading_frame(&g_font, &g_loading, "SKELETAL ANIMATION 3D",
        "READY", 100);

    g_pause = 0;
    g_fps = 0;
    g_fps_frames = 0;
    g_fps_mark = sat_frame_count();

    while (1) {
        sat_pad_state_t pad = {0};
        sat_example_must(sat_wait_vblank());
        sat_example_must(sat_begin_frame());
        sat_example_must(sat_pad_poll(&pad));
        handle_input(&pad);

        if (!g_pause) {
            sat_example_must(sat_skel_clip_player_advance(
                &g_player, SAT_FX16_ONE / 60, NULL, 0, NULL, NULL));
            sat_example_must(sat_skel_clip_player_sample(
                &g_player, g_pose, robot_police_skeletal_skeleton.joint_count));
            sat_example_must(sat_skeleton_evaluate(
                &g_skeleton, g_pose, robot_police_skeletal_skeleton.joint_count));
            for (uint16_t j = 0; j < robot_police_skeletal_skeleton.joint_count; ++j) {
                g_joint_world[j] = g_skeleton_nodes[j].world;
            }
            sat_example_must(sat_skin_update_mesh(
                &robot_police_skeletal_skin, g_joint_world,
                (uint16_t)(sizeof(g_joint_world) / sizeof(g_joint_world[0])),
                &g_mesh, 0, NULL));

            sat_example_must(sat_orbit_camera3d_update(&g_orbit));
            sat_camera3d_t camera = {0};
            camera.eye = g_orbit.eye;
            camera.target = g_orbit.target;
            camera.up = (sat_vec3_t){0, SAT_FX16_ONE, 0};
            camera.view_proj = g_orbit.view_proj;
            sat_example_must(sat_scene_begin(&g_scene, &camera, 64,
                sat_video_width(), sat_video_height(), 64u));
            g_last_submit = sat_scene_submit_instance(&g_scene, &g_instance, 0,
                g_mesh_screen, NULL);
            sat_example_must(sat_scene_flush(&g_scene));
        }
        update_fps();
        draw_hud();
        sat_example_must(sat_end_frame());
    }
    return 0;
}