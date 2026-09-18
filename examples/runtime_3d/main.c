/* runtime_3d - native high-level 3D acceptance surface.
 *
 * The generated model is exposed to gameplay as a bounded logical data asset;
 * camera, transform, sorting, projection scratch and draw lowering are owned
 * by sat_scene3d_t. Gameplay never constructs VDP1 commands.
 */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/asset.h"
#include "saturn/color.h"
#include "saturn/example_util.h"
#include "saturn/font.h"
#include "saturn/fmt.h"
#include "saturn/input.h"
#include "saturn/math3d.h"
#include "saturn/scene3d.h"
#include "saturn/time.h"

#include "runtime_3d/sonic_model.h"

#define SCREEN_W 320
#define SCREEN_H 224
#define MODEL_VERTEX_CAP 320u
#define MODEL_FACE_CAP 200u
#define MODEL_TEXTURE_CAP 64u

static sat_vec3_t g_vertices[MODEL_VERTEX_CAP];
static uint16_t g_indices[MODEL_FACE_CAP * 4u];
static uint8_t g_order[MODEL_FACE_CAP];
static uint32_t g_depth[MODEL_FACE_CAP];
static sat_projected_vertex_t g_screen[MODEL_VERTEX_CAP];
static sat_vdp1_texture_t g_textures[MODEL_TEXTURE_CAP];
static sat_ascii_font_t g_font;

static void draw_hud(uint32_t now, uint8_t automatic) {
    char line[32];
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(
        &g_font, "LIBSATURN RUNTIME 3D", 8, 5, 8, 0u, 0u));
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(
        &g_font, "LEFT/RIGHT ORBIT  UP/DOWN PITCH", 8, 17, 8, 0u, 0u));
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(
        &g_font, "A AUTO  B RESET  START EXIT", 8, 29, 8, 0u, 0u));
    sat_example_must(sat_fmt_label_u32("MS ", now, line, sizeof(line), 0));
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(
        &g_font, line, 248, 5, 8, 0u, 0u));
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(
        &g_font, automatic != 0u ? "AUTO" : "MANUAL", 248, 205, 8, 0u, 0u));
}

int main(void) {
    const sat_video_config_t video = {SCREEN_W, SCREEN_H, 1u, 0u};
    sat_scene3d_t scene;
    sat_camera3d_t camera;
    sat_model_transform3d_t transform;
    sat_scene3d_model_params_t params = {0};
    sat_asset_t model_asset_handle;
    const void* model_data = 0;
    uint32_t model_size = 0u;
    sat_vec3_t target;
    sat_vec3_t up = {0, SAT_FX16_ONE, 0};
    sat_fx16_t radius = sat_fx16_from_int(220);
    int32_t yaw = 0;
    int32_t pitch = 10;
    uint8_t automatic = 0u;

    sat_example_must(sat_init(&video));
    sat_example_must(sat_ascii_font_init_8x8_indexed8(
        &g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 0u));
    sat_example_must(sat_model_validate(&sonic_model_asset));
    sat_example_must(sat_model_compute_center(&sonic_model_asset, &target));
    sat_example_must(sat_model_upload_textures(
        &sonic_model_asset, g_textures, MODEL_TEXTURE_CAP));
    sat_example_must(sat_scene3d_init(
        &scene, g_vertices, MODEL_VERTEX_CAP, g_indices, MODEL_FACE_CAP,
        g_order, 0, g_depth, g_screen));
    sat_model_transform3d_identity(&transform);

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
        if ((pad.held & SAT_PAD_LEFT) != 0u) yaw -= 2;
        if ((pad.held & SAT_PAD_RIGHT) != 0u) yaw += 2;
        if ((pad.held & SAT_PAD_UP) != 0u) pitch += 2;
        if ((pad.held & SAT_PAD_DOWN) != 0u) pitch -= 2;
        if (pitch < -55) pitch = -55;
        if (pitch > 55) pitch = 55;
        if ((pad.pressed & SAT_PAD_A) != 0u) automatic = (uint8_t)(automatic == 0u);
        if ((pad.pressed & SAT_PAD_B) != 0u) {
            yaw = 0;
            pitch = 10;
            automatic = 0u;
        }
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
        if (automatic != 0u) ++yaw;
        if (yaw >= 360) yaw -= 360;
        if (yaw < 0) yaw += 360;

        const sat_fx16_t yaw_fx = sat_fx16_from_int(yaw);
        const sat_fx16_t pitch_fx = sat_fx16_from_int(pitch);
        const sat_fx16_t horizontal = sat_fx16_mul(radius, sat_cos_deg(pitch_fx));
        sat_vec3_t eye = {
            target.x + sat_fx16_mul(horizontal, sat_sin_deg(yaw_fx)),
            target.y + sat_fx16_mul(radius, sat_sin_deg(pitch_fx)),
            target.z + sat_fx16_mul(horizontal, sat_cos_deg(yaw_fx))};
        sat_example_must(sat_camera3d_init(
            &camera, &eye, &target, &up, sat_fx16_from_int(60),
            sat_fx16_div(sat_fx16_from_int(SCREEN_W), sat_fx16_from_int(SCREEN_H)),
            sat_fx16_from_int(1), sat_fx16_from_int(1000)));
        transform.rotation_deg.y = sat_fx16_from_int((int32_t)(now / 20u) % 360);
        params.textures = g_textures;
        params.texture_count = sonic_model_asset.texture_count;
        params.color = SAT_RGB555(31u, 31u, 31u);
        params.ambient = SAT_FX16_ONE;
        params.flags = SAT_MESH_CULL_BACKFACE | SAT_MESH_SORT;
        sat_example_must(sat_scene3d_begin(&scene, &camera));
        sat_example_must(sat_scene3d_draw_model(
            &scene, (const sat_model_asset_t*)model_data, &transform, &params));
        sat_example_must(sat_scene3d_end(&scene));
        draw_hud(now, automatic);
        sat_example_must(sat_app_frame_end());
    }

    (void)model_asset_handle;
    return 0;
}
