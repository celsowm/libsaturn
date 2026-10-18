/* VDP2 Parallax Lab.
 *
 * The four hand-painted layers of the Parallax Lab art pack move at
 * independent speeds over a fixed horizon:
 *
 *   NBG3  sky and clouds        tilemap, 8bpp cells, 256-colour bank 2
 *   NBG2  mountains and town    tilemap, 4bpp cells, 16-colour block 4
 *   NBG1  sea, cliffs, lighthouse  512x256 8bpp bitmap, CRAM bank 1
 *   NBG0  palms, rocks, flowers    512x256 8bpp bitmap, CRAM bank 0
 *
 * The SH-2 only writes four scroll registers per field.  All four planes are
 * 512x256 crops of the source panorama, so the camera pans 176 dots and back
 * instead of wrapping over a visible seam.  Cell layers use two-word pattern
 * names, which lets every 8x8 tile of the 512x256 image have its own 15-bit
 * character number.
 *
 * Controls: A pauses/resumes the pan; START exits.
 *
 * g_parallax_lab is the telemetry block checked by
 * harness/tests/test_vdp2_parallax_lab.py. */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/saturn.h"
#include "saturn/time.h"
#include "saturn/vdp1.h"
#include "saturn/vdp2.h"
#include "saturn/vdp2_layers.h"
#include "vdp2_parallax_lab/assets.h"

#define LAB_MAGIC SAT_FOURCC('P', 'L', 'A', 'B')
#define SCREEN_W 320u
#define SCREEN_H 224u
#define PAN_MAX 176u
#define PAN_HALF_FRAMES 720u
#define PAN_PERIOD (PAN_HALF_FRAMES * 2u)

#define FG_BITMAP_BASE 0x00000u
#define SEA_BITMAP_BASE 0x20000u
#define MOUNTAIN_MAP_BASE 0x40000u
#define SKY_MAP_BASE 0x44000u
#define MOUNTAIN_CHAR_BASE 0x48000u
#define SKY_CHAR_BASE 0x60000u

#define PAL_FOREGROUND 0u
#define PAL_SEA 1u
#define PAL_MOUNTAIN 4u
#define PAL_SKY 2u
#define FONT_BANK 7u

typedef struct parallax_lab_results {
    uint32_t magic;
    uint32_t init_ok;
    uint32_t layers_ok;
    uint32_t foreground_ok;
    uint32_t sea_ok;
    uint32_t sky_ok;
    uint32_t mountain_ok;
    uint32_t frames;
    uint32_t pan;
    uint32_t sky_x;
    uint32_t mountain_x;
    uint32_t sea_x;
    uint32_t foreground_x;
    uint32_t paused;
    uint32_t update_us_last;
    uint32_t update_us_max;
    uint32_t vdp1_commands;
} parallax_lab_results_t;

volatile parallax_lab_results_t g_parallax_lab;

static sat_ascii_font_t g_font;

static uint32_t ticks_us(uint64_t ticks) {
    const uint32_t hz = sat_time_frequency_hz();
    if (hz == 0u) return 0u;
    return (uint32_t)((ticks * 1000000u) / hz);
}

static sat_result_t configure_bitmap(sat_vdp2_layer_t layer, uint32_t base,
                                     uint8_t palette, uint8_t priority) {
    sat_vdp2_layer_config_t c;
    sat_vdp2_layer_config_default(layer, &c);
    c.bitmap = 1u;
    c.bitmap_size = SAT_VDP2_BITMAP_512X256;
    c.color_mode = SAT_VDP2_COLOR_MODE_256;
    c.palette = palette;
    c.priority = priority;
    c.transparent = 1u;
    c.plane_address[0] = base;
    for (uint8_t i = 1u; i < 4u; ++i) c.plane_address[i] = base;
    return sat_vdp2_layer_configure(&c);
}

static sat_result_t configure_cells(sat_vdp2_layer_t layer, uint32_t map_base,
                                    uint8_t bank_mask, uint8_t color_mode,
                                    uint8_t priority, uint8_t transparent) {
    sat_vdp2_layer_config_t c;
    sat_vdp2_layer_config_default(layer, &c);
    c.bitmap = 0u;
    c.color_mode = color_mode;
    c.char_size = SAT_VDP2_CHAR_SIZE_1X1;
    c.pattern_name_words = 2u;
    c.plane_pages_x = 1u;
    c.plane_pages_y = 1u;
    c.priority = priority;
    c.transparent = transparent;
    c.char_base_address = 0u;   /* the whole character number is in the name */
    c.char_bank_mask = bank_mask;
    for (uint8_t i = 0u; i < 4u; ++i) c.plane_address[i] = map_base;
    return sat_vdp2_layer_configure(&c);
}

static sat_result_t upload_assets(void) {
    sat_result_t st = SAT_OK;

    st = sat_vdp2_vram_write_words(FG_BITMAP_BASE / 2u, plab_foreground_words,
                                   PARALLAX_LAB_WIDTH * PARALLAX_LAB_HEIGHT / 2u);
    if (st != SAT_OK) return st;
    st = sat_vdp2_vram_write_words(SEA_BITMAP_BASE / 2u, plab_sea_words,
                                   PARALLAX_LAB_WIDTH * PARALLAX_LAB_HEIGHT / 2u);
    if (st != SAT_OK) return st;
    st = sat_vdp2_vram_write_words(MOUNTAIN_MAP_BASE / 2u, plab_mountain_map,
                                   PARALLAX_LAB_MAP_WORDS);
    if (st != SAT_OK) return st;
    st = sat_vdp2_vram_write_words(SKY_MAP_BASE / 2u, plab_sky_map,
                                   PARALLAX_LAB_MAP_WORDS);
    if (st != SAT_OK) return st;
    st = sat_vdp2_vram_write_words(MOUNTAIN_CHAR_BASE / 2u, plab_mountain_chars,
                                   sizeof(plab_mountain_chars) / sizeof(plab_mountain_chars[0]));
    if (st != SAT_OK) return st;
    st = sat_vdp2_vram_write_words(SKY_CHAR_BASE / 2u, plab_sky_chars,
                                   sizeof(plab_sky_chars) / sizeof(plab_sky_chars[0]));
    if (st != SAT_OK) return st;

    st = sat_vdp2_palette_upload(plab_foreground_palette, 256u, PAL_FOREGROUND * 256u);
    if (st != SAT_OK) return st;
    st = sat_vdp2_palette_upload(plab_sea_palette, 256u, PAL_SEA * 256u);
    if (st != SAT_OK) return st;
    st = sat_vdp2_palette_upload(plab_mountain_palette, 16u, PAL_MOUNTAIN * 16u);
    if (st != SAT_OK) return st;
    st = sat_vdp2_palette_upload(plab_sky_palette, 256u, PAL_SKY * 256u);
    return st;
}

static sat_result_t setup_scene(void) {
    sat_result_t st = SAT_OK;

    g_parallax_lab.foreground_ok =
        configure_bitmap(SAT_VDP2_NBG0, FG_BITMAP_BASE, PAL_FOREGROUND, 6u) == SAT_OK;
    g_parallax_lab.sea_ok =
        configure_bitmap(SAT_VDP2_NBG1, SEA_BITMAP_BASE, PAL_SEA, 4u) == SAT_OK;
    g_parallax_lab.mountain_ok =
        configure_cells(SAT_VDP2_NBG2, MOUNTAIN_MAP_BASE,
                        SAT_VDP2_BANK_B0, SAT_VDP2_COLOR_MODE_16, 2u, 1u) == SAT_OK;
    g_parallax_lab.sky_ok =
        configure_cells(SAT_VDP2_NBG3, SKY_MAP_BASE,
                        SAT_VDP2_BANK_B1, SAT_VDP2_COLOR_MODE_256, 1u, 0u) == SAT_OK;

    if (!g_parallax_lab.foreground_ok || !g_parallax_lab.sea_ok ||
        !g_parallax_lab.mountain_ok || !g_parallax_lab.sky_ok) {
        st = SAT_ERR_CAPACITY;
    }
    if (upload_assets() != SAT_OK) st = SAT_ERR_CAPACITY;
    if (sat_vdp2_back_color_set(SAT_RGB555(1, 3, 12)) != SAT_OK) st = SAT_ERR_CAPACITY;
    if (sat_vdp2_sprite_set_priority(7u) != SAT_OK) st = SAT_ERR_CAPACITY;
    g_parallax_lab.layers_ok = st == SAT_OK ? 1u : 0u;
    return st;
}

static uint32_t pan_for_tick(uint32_t tick) {
    const uint32_t phase = tick % PAN_PERIOD;
    const uint32_t t = phase < PAN_HALF_FRAMES ? phase : PAN_PERIOD - phase;
    return (t * PAN_MAX) / PAN_HALF_FRAMES;
}

static int32_t layer_scroll(uint32_t pan, uint32_t percent) {
    return (int32_t)(((pan * percent) / 100u) << 16u);
}

static void update_scene(uint32_t pan) {
    const uint64_t start = sat_time_ticks();
    g_parallax_lab.pan = pan;
    g_parallax_lab.sky_x = (uint32_t)layer_scroll(pan, 8u);
    g_parallax_lab.mountain_x = (uint32_t)layer_scroll(pan, 24u);
    g_parallax_lab.sea_x = (uint32_t)layer_scroll(pan, 52u);
    g_parallax_lab.foreground_x = (uint32_t)layer_scroll(pan, 100u);

    (void)sat_vdp2_layer_set_scroll(SAT_VDP2_NBG3, (int32_t)g_parallax_lab.sky_x, 0);
    (void)sat_vdp2_layer_set_scroll(SAT_VDP2_NBG2, (int32_t)g_parallax_lab.mountain_x, 0);
    (void)sat_vdp2_layer_set_scroll(SAT_VDP2_NBG1, (int32_t)g_parallax_lab.sea_x, 0);
    (void)sat_vdp2_layer_set_scroll(SAT_VDP2_NBG0, (int32_t)g_parallax_lab.foreground_x, 0);
    (void)sat_vdp2_layers_commit();

    const uint32_t us = ticks_us(sat_time_ticks() - start);
    g_parallax_lab.update_us_last = us;
    if (us > g_parallax_lab.update_us_max) g_parallax_lab.update_us_max = us;
}

static void draw_hud(uint32_t pan) {
    (void)sat_draw_rect_screen(0, 0, 210, 56, SAT_RGB555(0, 0, 0));
    (void)sat_ascii_font_draw_text_screen_indexed8(&g_font, "VDP2 PARALLAX LAB",
                                                   8, 8, 8, 0u, 0u);
    (void)sat_ascii_font_draw_label_u32(&g_font, "FRAME ", g_parallax_lab.frames,
                                        8, 24, 8, 0u, 0u);
    (void)sat_ascii_font_draw_label_u32(&g_font, "PAN ", pan, 112, 24, 8, 0u, 0u);
    (void)sat_ascii_font_draw_text_screen_indexed8(
        &g_font, g_parallax_lab.paused ? "PAUSED" : "4 LAYERS", 8, 40, 8, 0u, 0u);
    (void)sat_ascii_font_draw_text_screen_indexed8(&g_font, "A/START", 140, 40,
                                                   8, 0u, 0u);
}

int main(void) {
    const sat_video_config_t video = {SCREEN_W, SCREEN_H, SAT_VIDEO_AUTO, 0u};
    if (sat_init(&video) != SAT_OK) return 1;
    g_parallax_lab.magic = LAB_MAGIC;
    if (sat_ascii_font_init_8x8_indexed8(&g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK,
                                          FONT_BANK) != SAT_OK) {
        return 1;
    }

    const sat_result_t scene = setup_scene();
    g_parallax_lab.init_ok =
        (scene == SAT_OK && g_parallax_lab.layers_ok != 0u &&
         g_parallax_lab.foreground_ok != 0u && g_parallax_lab.sea_ok != 0u &&
         g_parallax_lab.mountain_ok != 0u && g_parallax_lab.sky_ok != 0u) ? 1u : 0u;

    uint32_t pan_tick = 0u;
    for (;;) {
        sat_pad_state_t pad = {0};
        if (sat_wait_vblank() != SAT_OK) break;
        ++g_parallax_lab.frames;

        if (g_parallax_lab.paused == 0u) ++pan_tick;
        update_scene(pan_for_tick(pan_tick));
        (void)sat_vdp1_set_erase_transparent();

        if (sat_begin_frame() != SAT_OK) break;
        (void)sat_pad_poll(&pad);
        if ((pad.pressed & SAT_PAD_A) != 0u) {
            g_parallax_lab.paused ^= 1u;
        }
        if ((pad.pressed & SAT_PAD_START) != 0u) break;

        draw_hud(g_parallax_lab.pan);
        sat_vdp1_command_stats_t stats = {0};
        (void)sat_vdp1_command_stats(&stats);
        g_parallax_lab.vdp1_commands = stats.used;
        (void)sat_end_frame();
    }
    (void)sat_shutdown();
    return 0;
}
