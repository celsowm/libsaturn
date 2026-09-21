/* runtime_3d - native high-level 3D acceptance surface.
 *
 * The generated model is exposed to gameplay as a bounded logical data asset;
 * camera, transform, sorting, projection scratch and draw lowering are owned
 * by sat_scene_t. Gameplay never constructs VDP1 commands.
 */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/asset.h"
#include "saturn/color.h"
#include "saturn/example_util.h"
#include "saturn/font.h"
#include "saturn/hud.h"
#include "saturn/fmt.h"
#include "saturn/input.h"
#include "saturn/math3d.h"
#include "saturn/orbit_camera3d.h"
#include "saturn/scene.h"
#include "saturn/transform3d.h"
#include "saturn/time.h"

#include "runtime_3d/sonic_model.h"

#define SCREEN_W 320
#define SCREEN_H 224
#define MODEL_VERTEX_CAP 320u
#define MODEL_FACE_CAP 200u
#define MODEL_TEXTURE_CAP 64u

static sat_vec3_t g_vertices[MODEL_VERTEX_CAP];
static uint16_t g_indices[MODEL_FACE_CAP * 4u];
static sat_projected_vertex_t g_screen[MODEL_VERTEX_CAP];
static sat_vec3_t g_world[MODEL_VERTEX_CAP];
static sat_mesh_t g_mesh;
static uint16_t g_face_materials[MODEL_FACE_CAP];
static sat_scene3d_material_t g_materials[MODEL_TEXTURE_CAP];
static sat_scene3d_face_t g_faces[MODEL_FACE_CAP];
static uint32_t g_keys[MODEL_FACE_CAP];
static uint16_t g_order[MODEL_FACE_CAP];
static sat_vdp1_texture_t g_textures[MODEL_TEXTURE_CAP];
static sat_ascii_font_t g_font;
static sat_hud_t g_hud;
static sat_scene3d_instance_t g_instance;
static sat_transform3d_node_t g_transform_nodes[2];
static uint16_t g_transform_scratch[2];
static sat_transform3d_world_t g_transforms;
static uint16_t g_model_node;

static void draw_hud(uint32_t now, uint8_t automatic) {
    sat_example_must(sat_hud_text(&g_hud, "LIBSATURN RUNTIME 3D", 8, 5));
    sat_example_must(sat_hud_text(&g_hud, "LEFT/RIGHT ORBIT  UP/DOWN PITCH", 8, 17));
    sat_example_must(sat_hud_text(&g_hud, "A AUTO  B RESET  START EXIT", 8, 29));
    sat_example_must(sat_hud_value(&g_hud, "MS ", now, 248, 5));
    sat_example_must(sat_hud_text(&g_hud,
        automatic != 0u ? "AUTO" : "MANUAL", 248, 205));
}

int main(void) {
    const sat_video_config_t video = {SCREEN_W, SCREEN_H, 1u, 0u};
    sat_scene_t scene;
    sat_camera3d_t camera;
    sat_orbit_camera3d_t orbit;
    sat_model_transform3d_t transform;
    uint16_t root_node;
    sat_asset_t model_asset_handle;
    const void* model_data = 0;
    uint32_t model_size = 0u;
    sat_vec3_t bounds_min;
    sat_vec3_t bounds_max;

    sat_example_must(sat_init(&video));
    sat_example_must(sat_ascii_font_init_8x8_indexed8(
        &g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 0u));
    sat_example_must(sat_hud_init(&g_hud, &g_font, 0u, 8u));
    sat_example_must(sat_model_validate(&sonic_model_asset));
    sat_example_must(sat_model_compute_bounds(
        &sonic_model_asset, &bounds_min, &bounds_max));
    sat_example_must(sat_model_upload_textures(
        &sonic_model_asset, g_textures, MODEL_TEXTURE_CAP));
    sat_example_must(sat_mesh_init(
        &g_mesh, g_vertices, MODEL_VERTEX_CAP, g_indices, MODEL_FACE_CAP));
    sat_example_must(sat_model_copy_to_mesh(&sonic_model_asset, &g_mesh));
    g_instance.mesh = &g_mesh;
    g_instance.materials = g_materials;
    g_instance.material_count = sonic_model_asset.texture_count;
    g_instance.face_materials = g_face_materials;
    g_instance.world = 0;
    g_instance.pass = 0u;
    g_instance.cull_backfaces = 1u;
    for (uint16_t i = 0u; i < sonic_model_asset.texture_count; ++i) {
        g_materials[i].kind = SAT_SCENE3D_INDEXED_TEXTURED;
        g_materials[i].texture = &g_textures[i];
        g_materials[i].color_calc_slot = SAT_INDEXED_SOLID_OPAQUE;
        g_face_materials[i] = i;
    }
    for (uint16_t i = 0u; i < sonic_model_asset.face_count; ++i)
        g_face_materials[i] = sonic_model_asset.face_texture_indices[i];
    sat_example_must(sat_scene_init(
        &scene, g_faces, g_keys, g_order, MODEL_FACE_CAP));
    sat_example_must(sat_transform3d_world_init(
        &g_transforms, g_transform_nodes, 2u));
    sat_example_must(sat_transform3d_create(&g_transforms, &root_node));
    sat_example_must(sat_transform3d_create(&g_transforms, &g_model_node));
    sat_example_must(sat_transform3d_set_parent(
        &g_transforms, g_model_node, root_node));
    sat_model_transform3d_identity(&transform);

    {
        sat_orbit_camera3d_fit_t fit = {0};
        fit.min_extent = SAT_FX16_ONE;
        fit.min_distance_floor = sat_fx16_from_int(10);
        fit.initial_distance_factor = sat_fx16_from_int(3);
        fit.min_distance_factor = SAT_FX16_ONE;
        fit.max_distance_factor = sat_fx16_from_int(8);
        fit.near_plane_factor = SAT_FX16_ONE / 8;
        fit.near_plane_floor = sat_fx16_from_int(1);
        fit.fov_y = sat_fx16_from_int(60);
        fit.aspect = sat_fx16_div(
            sat_fx16_from_int(SCREEN_W), sat_fx16_from_int(SCREEN_H));
        fit.far_z = sat_fx16_from_int(1000);
        fit.pitch_min_deg = -55;
        fit.pitch_max_deg = 55;
        sat_example_must(sat_orbit_camera3d_fit_bounds(
            &orbit, &bounds_min, &bounds_max, &fit));
    }

    sat_asset_desc_t desc = {0};
    desc.logical_path = "models/runtime-sonic";
    desc.data = &sonic_model_asset;
    desc.size = sizeof(sonic_model_asset);
    desc.kind = SAT_ASSET_DATA;
    sat_example_must(sat_asset_register(&desc, &model_asset_handle));
    sat_example_must(sat_asset_load_data(
        "models/runtime-sonic", &model_data, &model_size));
    if (model_data != &sonic_model_asset || model_size != sizeof(sonic_model_asset)) {
        return 1;
    }

    for (;;) {
        sat_pad_state_t pad = {0};
        const uint32_t now = sat_time_ms();
        sat_example_must(sat_app_frame_begin(SAT_COLOR_BLACK, SAT_COLOR_BLACK, &pad));
        sat_example_must(sat_orbit_camera3d_apply_pad(
            &orbit, &pad, SAT_PAD_A));
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
        camera.eye = orbit.eye;
        camera.target = orbit.target;
        camera.up = (sat_vec3_t){0, SAT_FX16_ONE, 0};
        camera.view_proj = orbit.view_proj;
        transform.rotation_deg.y = sat_fx16_from_int((int32_t)(now / 20u) % 360);
        sat_example_must(sat_transform3d_set_local(
            &g_transforms, root_node, &transform));
        sat_example_must(sat_transform3d_evaluate(
            &g_transforms, g_transform_scratch, 2u));
        sat_example_must(sat_scene_begin(
            &scene, &camera, sat_fx16_from_int(1), SCREEN_W, SCREEN_H, 96u));
        sat_example_must(sat_scene_submit_transform_instance(
            &scene, &g_transforms, g_model_node, &g_instance,
            SAT_SCENE3D_SLOT_INHERIT, g_screen, g_world));
        sat_example_must(sat_scene_flush(&scene));
        draw_hud(now, orbit.auto_orbit);
        sat_example_must(sat_app_frame_end());
    }

    (void)model_asset_handle;
    (void)sat_shutdown();
    return 0;
}
