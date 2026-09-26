/* First Voxel Space terrain prototype. CPU renderer -> INDEX8 surface ->
 * existing LibSaturn dynamic VDP1 texture. No external assets or RAM cart.
 * This intentionally does NOT claim to benchmark the pending VDP2 bitmap
 * presenter; see docs/VOXEL_TERRAIN_PLAN.md for its acceptance criteria. */
#include <stdint.h>

#include "saturn/color.h"
#include "saturn/example_util.h"
#include "saturn/font.h"
#include "saturn/fmt.h"
#include "saturn/input.h"
#include "saturn/math3d.h"
#include "saturn/render2d.h"
#include "saturn/surface.h"
#include "saturn/texture.h"
#include "saturn/time.h"
#include "saturn/video.h"
#include "saturn/voxel_terrain.h"

#define MAP_W 256u
#define MAP_H 256u
#define FRAME_W 160u
#define FRAME_H 112u
#define VIEW_DISTANCE 120u
#define FRAME_SCRATCH_BYTES (4u * (VIEW_DISTANCE + 1u) + 2u * FRAME_W)
#define FX_INT(v) ((sat_fx16_t)((v) * 65536))

static uint8_t g_heights[MAP_W * MAP_H];
static uint8_t g_colors[MAP_W * MAP_H];
static uint8_t g_frame[FRAME_W * FRAME_H];
static uint16_t g_palette[256];
static uint32_t g_scratch[(FRAME_SCRATCH_BYTES + 3u) / 4u];
static sat_ascii_font_t g_font;
static sat_texture_t g_texture;
static sat_voxel_camera_t g_camera;
static sat_voxel_terrain_t g_terrain = {
    g_heights, g_colors, MAP_W, MAP_H, MAP_W, 1u, SAT_VOXEL_EDGE_WRAP
};
static sat_voxel_target_t g_target = {
    g_frame, FRAME_W, FRAME_H, FRAME_W, 1u
};
static sat_surface_t g_surface = {
    g_frame, FRAME_W, FRAME_H, FRAME_W, SAT_PIXEL_INDEX8, g_palette, 256u
};

static uint32_t g_render_ms;
static uint32_t g_upload_ms;
static uint32_t g_frame_delta;

static uint32_t abs32(int32_t value) {
    return (uint32_t)(value < 0 ? -value : value);
}

static void make_world(void) {
    for (uint16_t i = 0u; i < 256u; ++i) {
        g_palette[i] = SAT_RGB555(6u, 15u, 8u);
    }
    g_palette[0] = SAT_RGB555(0u, 0u, 0u);
    g_palette[1] = SAT_RGB555(9u, 20u, 30u);
    g_palette[2] = SAT_RGB555(4u, 10u, 25u);
    g_palette[3] = SAT_RGB555(5u, 17u, 29u);
    g_palette[4] = SAT_RGB555(26u, 25u, 12u);
    g_palette[5] = SAT_RGB555(7u, 20u, 9u);
    g_palette[6] = SAT_RGB555(11u, 22u, 12u);
    g_palette[7] = SAT_RGB555(20u, 19u, 18u);
    g_palette[8] = SAT_RGB555(27u, 27u, 26u);
    for (uint32_t z = 0u; z < MAP_H; ++z) {
        for (uint32_t x = 0u; x < MAP_W; ++x) {
            const uint32_t dx = abs32((int32_t)x - 128);
            const uint32_t dz = abs32((int32_t)z - 128);
            const uint32_t mountain = dx + dz / 2u;
            const uint32_t noise = (x * 13u ^ z * 37u) & 15u;
            uint32_t height = 5u + noise / 3u;
            if (mountain < 105u) height += (105u - mountain) / 2u;
            if (mountain < 38u) height += (38u - mountain) / 3u;
            if (height > 110u) height = 110u;
            g_heights[z * MAP_W + x] = (uint8_t)height;
            g_colors[z * MAP_W + x] = height < 8u ? 2u :
                height < 15u ? 4u : height < 40u ? 5u :
                height < 65u ? 6u : height < 85u ? 7u : 8u;
        }
    }
}

static void controls(const sat_pad_state_t* pad) {
    if ((pad->held & SAT_PAD_LEFT) != 0u) g_camera.angle -= FX_INT(2);
    if ((pad->held & SAT_PAD_RIGHT) != 0u) g_camera.angle += FX_INT(2);
    /* Normalize after each update; trigonometry accepts a bounded angle. */
    if (g_camera.angle < 0) g_camera.angle += FX_INT(360);
    if (g_camera.angle >= FX_INT(360)) g_camera.angle -= FX_INT(360);
    if ((pad->held & SAT_PAD_UP) != 0u ||
        (pad->held & SAT_PAD_DOWN) != 0u) {
        const sat_fx16_t step = (pad->held & SAT_PAD_UP) != 0u ?
                                SAT_FX16_ONE / 2 : -SAT_FX16_ONE / 2;
        g_camera.x += sat_fx16_mul(sat_sin_deg(g_camera.angle), step);
        g_camera.z += sat_fx16_mul(sat_cos_deg(g_camera.angle), step);
    }
    if ((pad->held & SAT_PAD_A) != 0u) g_camera.y += SAT_FX16_ONE / 2;
    if ((pad->held & SAT_PAD_B) != 0u) g_camera.y -= SAT_FX16_ONE / 2;
    if (g_camera.y < FX_INT(6)) g_camera.y = FX_INT(6);
    if (g_camera.y > FX_INT(180)) g_camera.y = FX_INT(180);
    if ((pad->held & SAT_PAD_X) != 0u && g_camera.pitch_pixels > -40) {
        --g_camera.pitch_pixels;
    }
    if ((pad->held & SAT_PAD_Y) != 0u && g_camera.pitch_pixels < 40) {
        ++g_camera.pitch_pixels;
    }
    if ((pad->pressed & SAT_PAD_C) != 0u) {
        g_camera.x = FX_INT(80);
        g_camera.y = FX_INT(80);
        g_camera.z = FX_INT(52);
        g_camera.angle = FX_INT(0);
        g_camera.pitch_pixels = 0;
    }
    /* Prevent unintentionally exceeding the API's validated coordinate
     * range while flying indefinitely through a WRAP map. */
    if (g_camera.x > FX_INT(2048)) g_camera.x -= FX_INT(256);
    if (g_camera.x < FX_INT(-2048)) g_camera.x += FX_INT(256);
    if (g_camera.z > FX_INT(2048)) g_camera.z -= FX_INT(256);
    if (g_camera.z < FX_INT(-2048)) g_camera.z += FX_INT(256);
}

static void hud(void) {
    char line[40];
    (void)sat_ascii_font_draw_text_screen_indexed8(
        &g_font, "VOXEL TERRAIN - VDP1", 4, 4, 8u, 0u, 0u);
    (void)sat_ascii_font_draw_text_screen_indexed8(
        &g_font, "ARROWS FLY  A/B HEIGHT  X/Y LOOK", 4, 14, 8u, 0u, 0u);
    (void)sat_ascii_font_draw_text_screen_indexed8(
        &g_font, "C RESET  START QUIT", 4, 24, 8u, 0u, 0u);
    if (sat_fmt_label_u32("RENDER MS ", g_render_ms,
            line, sizeof(line), 0u) == SAT_OK) {
        (void)sat_ascii_font_draw_text_screen_indexed8(
            &g_font, line, 4, 201, 8u, 0u, 0u);
    }
    if (sat_fmt_label_u32("UPLOAD MS ", g_upload_ms,
            line, sizeof(line), 0u) == SAT_OK) {
        (void)sat_ascii_font_draw_text_screen_indexed8(
            &g_font, line, 112, 201, 8u, 0u, 0u);
    }
    if (sat_fmt_label_u32("FRAME VBL ", g_frame_delta,
            line, sizeof(line), 0u) == SAT_OK) {
        (void)sat_ascii_font_draw_text_screen_indexed8(
            &g_font, line, 220, 201, 8u, 0u, 0u);
    }
}

int main(void) {
    sat_video_config_t video = {320u, 224u, SAT_VIDEO_AUTO, 0u};
    sat_example_must(sat_init(&video));
    make_world();
    sat_example_must(sat_texture_create_from_surface(
        &g_texture, &g_surface, SAT_TEXTURE_DYNAMIC));
    sat_example_must(sat_ascii_font_init_8x8_indexed8(
        &g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 2u));
    g_camera.x = FX_INT(80);
    g_camera.y = FX_INT(80);
    g_camera.z = FX_INT(52);
    g_camera.angle = 0;
    g_camera.half_fov = SAT_FX16_ONE / 2;
    g_camera.pitch_pixels = 0;
    g_camera.projection_scale = 80u;
    g_camera.view_distance = VIEW_DISTANCE;
    const sat_rect_t fullscreen = {0, 0, 320u, 224u};
    uint32_t previous_frame = sat_frame_count();

    for (;;) {
        sat_pad_state_t pad = {0};
        sat_example_must(sat_wait_vblank());
        sat_example_must(sat_pad_poll(&pad));
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
        controls(&pad);
        uint32_t t = sat_time_ms();
        sat_example_must(sat_voxel_terrain_render(
            &g_terrain, &g_camera, &g_target, g_scratch, sizeof(g_scratch)));
        g_render_ms = sat_time_ms() - t;

        /* Existing dynamic-texture path is a baseline, not a proven zero-
         * tearing video streamer: hardware DMA/bank scheduling is phase 3. */
        t = sat_time_ms();
        sat_example_must(sat_texture_update(g_texture, &g_surface));
        g_upload_ms = sat_time_ms() - t;
        sat_example_must(sat_begin_frame());
        sat_example_must(sat_draw_texture(g_texture, 0, &fullscreen, 0));
        hud();
        sat_example_must(sat_end_frame());
        const uint32_t now = sat_frame_count();
        g_frame_delta = now - previous_frame;
        previous_frame = now;
    }

    sat_example_must(sat_texture_destroy(g_texture));
    sat_example_must(sat_shutdown());
    return 0;
}
