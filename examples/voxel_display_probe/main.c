/* Experimental VDP1-versus-VDP2 INDEX8 presentation probe. */
#include <stdint.h>
#include "saturn/color.h"
#include "saturn/core.h"
#include "saturn/example_util.h"
#include "saturn/fmt.h"
#include "saturn/font.h"
#include "saturn/input.h"
#include "saturn/render2d.h"
#include "saturn/surface.h"
#include "saturn/texture.h"
#include "saturn/time.h"
#include "saturn/vdp2.h"
#include "saturn/video.h"
#include "voxel_display_math.h"

#define SCREEN_W 320u
#define SCREEN_H 224u
#define FONT_PALETTE 2u

static uint8_t g_pixels[VOXEL_PROBE_SRC_W * VOXEL_PROBE_SRC_H];
static uint16_t g_palette[256];
static uint16_t g_words[VOXEL_PROBE_SRC_W / 2u];
static sat_surface_t g_surface = {
    g_pixels, VOXEL_PROBE_SRC_W, VOXEL_PROBE_SRC_H, VOXEL_PROBE_SRC_W,
    SAT_PIXEL_INDEX8, g_palette, 256u
};
static sat_texture_t g_texture;
static sat_ascii_font_t g_font;

static void init_palette(void) {
    for (uint16_t i = 0u; i < 256u; ++i) {
        g_palette[i] = SAT_RGB555((i >> 3u) & 31u,
                                  (i * 3u) & 31u, (i * 5u) & 31u);
    }
    g_palette[0] = SAT_RGB555(0u, 0u, 0u);
    g_palette[1] = SAT_RGB555(5u, 13u, 27u);
    g_palette[2] = SAT_RGB555(31u, 24u, 3u);
    g_palette[3] = SAT_RGB555(7u, 29u, 15u);
    g_palette[4] = SAT_RGB555(30u, 5u, 10u);
    g_palette[5] = SAT_RGB555(31u, 31u, 31u);
}

static void make_pattern(uint32_t frame) {
    for (uint16_t y = 0u; y < VOXEL_PROBE_SRC_H; ++y) {
        for (uint16_t x = 0u; x < VOXEL_PROBE_SRC_W; ++x) {
            const uint16_t sx = (uint16_t)((x + (frame & 255u)) & 255u);
            const uint16_t sy = (uint16_t)((y + (frame & 255u)) & 255u);
            uint8_t index = 1u;
            if (x < 2u || y < 2u ||
                x >= VOXEL_PROBE_SRC_W - 2u ||
                y >= VOXEL_PROBE_SRC_H - 2u) index = 5u;
            else if (((sx >> 3u) ^ (sy >> 3u)) & 1u) index = 2u;
            else if (((sx >> 4u) + (sy >> 4u)) & 1u) index = 3u;
            else index = 4u;
            g_pixels[(uint32_t)y * VOXEL_PROBE_SRC_W + x] = index;
        }
    }
}

static void init_rbg0(void) {
    uint16_t params[48];
    voxel_probe_rbg0_build_params(params);
    sat_example_must(sat_vdp2_vram_write_words(
        VOXEL_PROBE_PARAMS_BASE_WORD, params, 48u));
    const sat_vdp2_rbg0_config_t config = {
        SAT_VDP2_RBG0_BITMAP_512x256, SAT_VDP2_COLOR_MODE_256,
        VOXEL_PROBE_BITMAP_BASE_WORD, VOXEL_PROBE_PARAMS_BASE_WORD
    };
    sat_example_must(sat_vdp2_rbg0_init(&config));
    sat_example_must(sat_vdp2_rbg0_set_transparent_code_enabled(0u));
    sat_example_must(sat_vdp2_rbg0_set_priority(4u));
    sat_example_must(sat_vdp2_sprite_set_priority(7u));
}

/* Visible-bank, synchronous CPU VRAM write intentionally left unbuffered
 * to measure tearing and fetch contention before designing a production
 * synchronized presenter. */
static void upload_rbg0(void) {
    for (uint16_t y = 0u; y < VOXEL_PROBE_SRC_H; ++y) {
        const uint8_t* row = g_pixels + (uint32_t)y * VOXEL_PROBE_SRC_W;
        voxel_probe_rbg0_pack_row(row, VOXEL_PROBE_SRC_W, g_words);
        sat_example_must(sat_vdp2_vram_write_words(
            voxel_probe_rbg0_row_offset(y), g_words,
            VOXEL_PROBE_SRC_W / 2u));
    }
}

static void label(const char* text, int16_t x, int16_t y) {
    sat_example_must(sat_ascii_font_draw_text_screen_indexed8(
        &g_font, text, x, y, 8u, 0u, 0u));
}

static void stat(const char* prefix, uint32_t n, int16_t x, int16_t y) {
    sat_example_must(sat_ascii_font_draw_label_u32(
        &g_font, prefix, n, x, y, 8u, 0u, 0u));
}

int main(void) {
    const sat_video_config_t video = {SCREEN_W, SCREEN_H, 1u, 0u};
    sat_example_must(sat_init(&video));
    init_palette();
    /* Reserve CRAM bank 0 for the RBG0 bitmap before creating the logical
     * VDP1 texture, which must use a different palette bank. */
    sat_example_must(sat_vdp2_palette_upload(g_palette, 256u, 0u));
    make_pattern(0u);
    sat_example_must(sat_texture_create_from_surface(
        &g_texture, &g_surface, SAT_TEXTURE_DYNAMIC));
    sat_example_must(sat_ascii_font_init_8x8_indexed8(
        &g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, FONT_PALETTE));
    init_rbg0();
    sat_example_must(sat_vdp1_set_erase_transparent());

    uint8_t use_rbg0 = 0u;
    uint32_t frame = 0u;
    uint32_t elapsed_vblanks = 0u;
    uint32_t generate_ms = 0u;
    uint32_t update_ms = 0u;
    uint32_t previous_display = sat_frame_count();
    const sat_rect_t fullscreen = {0, 0, SCREEN_W, SCREEN_H};

    for (;;) {
        sat_pad_state_t pad = {0};
        sat_example_must(sat_wait_vblank());
        sat_example_must(sat_pad_poll(&pad));
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
        if ((pad.pressed & SAT_PAD_A) != 0u) use_rbg0 = (uint8_t)!use_rbg0;
        sat_example_must(sat_vdp2_rbg0_set_enabled(use_rbg0));
        sat_example_must(sat_vdp2_layers_commit());
        sat_example_must(sat_vdp1_set_erase_transparent());

        uint32_t t = sat_time_ms();
        make_pattern(frame);
        generate_ms = sat_time_ms() - t;
        t = sat_time_ms();
        if (use_rbg0 != 0u) upload_rbg0();
        else sat_example_must(sat_texture_update(g_texture, &g_surface));
        update_ms = sat_time_ms() - t;

        sat_example_must(sat_begin_frame());
        if (use_rbg0 == 0u) {
            sat_example_must(sat_draw_texture(g_texture, 0, &fullscreen, 0));
        }
        label(use_rbg0 != 0u ? "VDP2 RBG0 CPU UPLOAD" :
                               "VDP1 DYNAMIC TEXTURE", 4, 4);
        label("A TOGGLE BACKEND  START EXIT", 4, 14);
        stat("GEN MS ", generate_ms, 4, 204);
        stat("COPY MS ", update_ms, 112, 204);
        stat("VBLS ", elapsed_vblanks, 230, 204);
        sat_example_must(sat_end_frame());

        const uint32_t now = sat_frame_count();
        elapsed_vblanks = now - previous_display;
        previous_display = now;
        ++frame;
    }
    sat_example_must(sat_texture_destroy(g_texture));
    sat_example_must(sat_shutdown());
    return 0;
}
