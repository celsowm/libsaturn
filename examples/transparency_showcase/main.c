/* Native Saturn transparency comparison: VDP1 RGB blend, VDP2 sprite
 * color calculation, and VDP1 checkerboard mesh. No external assets.
 * START exits; A cycles the VDP2 sprite alpha preset samples; B switches to
 * the second page (additive blend, which cannot share a frame with ratio
 * alpha: the VDP2 add/ratio mode is one bit for the whole screen). */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/example_util.h"
#include "saturn/fmt.h"
#include "saturn/font.h"
#include "saturn/input.h"
#include "saturn/render2d.h"
#include "saturn/vdp1.h"
#include "saturn/vdp2.h"
#include "saturn/vdp2_color_calc.h"
#include "saturn/video.h"

#define BG_W 64u
#define BG_H 64u
#define TEX_W 16u
#define TEX_H 16u
#define BG_BANK 0u
#define FONT_BANK 2u

static uint8_t bg_pixels[BG_W * BG_H];
static uint16_t bg_palette[256];
static uint16_t bg_map[SAT_VDP2_NBG0_MAP_CELLS];
static uint8_t sprite_pixels[TEX_W * TEX_H];
static uint16_t sprite_palette[256];
static sat_texture_t sprite;
static sat_ascii_font_t font;

static void prepare_assets(void) {
    for (uint16_t i = 0; i < 256u; ++i) {
        bg_palette[i] = SAT_RGB555(1u, 2u, 3u);
        sprite_palette[i] = SAT_RGB555(0u, 0u, 0u);
    }
    bg_palette[1] = SAT_RGB555(2u, 7u, 23u);
    bg_palette[2] = SAT_RGB555(8u, 24u, 14u);
    bg_palette[3] = SAT_RGB555(24u, 9u, 12u);
    bg_palette[4] = SAT_RGB555(18u, 19u, 5u);
    for (uint32_t y = 0u; y < BG_H; ++y) {
        for (uint32_t x = 0u; x < BG_W; ++x) {
            bg_pixels[y * BG_W + x] = (uint8_t)(1u + (((x >> 3u) + (y >> 3u)) & 3u));
        }
    }

    /* Sprite index 0 stays transparent; nonzero indices are opaque VDP1
     * palette pixels. Color calculation happens in the VDP2 compositor. */
    sprite_palette[1] = SAT_RGB555(31u, 27u, 4u);
    sprite_palette[2] = SAT_RGB555(31u, 31u, 31u);
    sprite_palette[3] = SAT_RGB555(3u, 24u, 31u);
    for (uint32_t y = 0u; y < TEX_H; ++y) {
        for (uint32_t x = 0u; x < TEX_W; ++x) {
            uint8_t v = (x == 0u || x == TEX_W - 1u ||
                         y == 0u || y == TEX_H - 1u) ? 0u : 1u;
            if (x > 4u && x < 11u && y > 4u && y < 11u) v = 2u;
            if (x > 6u && x < 9u && y > 6u && y < 9u) v = 3u;
            sprite_pixels[y * TEX_W + x] = v;
        }
    }
}

static void draw_text(const char* text, int16_t x, int16_t y) {
    (void)sat_ascii_font_draw_text_screen_indexed8(
        &font, text, x, y, 8u, SAT_COLOR_WHITE, 0u);
}

int main(void) {
    sat_video_config_t video = {320u, 224u, 1u, 0u};
    sat_example_must(sat_init(&video));
    prepare_assets();

    /* Reserve bank 0 in LibSaturn's palette registry as an external palette,
     * so the high-level texture receives its own non-conflicting bank. */
    sat_example_must(sat_palette_upload_indexed8(bg_palette, BG_BANK));
    sat_vdp2_nbg0_config_t bg = {
        SAT_VDP2_CHAR_SIZE_1X1,
        SAT_VDP2_COLOR_MODE_256,
        0x3Bu, 0u, 0u
    };
    sat_example_must(sat_vdp2_nbg0_init(&bg));
    sat_example_must(sat_vdp2_palette_upload(bg_palette, 256u, BG_BANK * 256u));
    sat_example_must(sat_vdp2_nbg0_upload_indexed8(
        bg_pixels, BG_W, BG_H, BG_BANK, bg_map));
    sat_example_must(sat_vdp2_nbg0_set_priority(1u));

    sat_surface_t surf = {
        sprite_pixels, TEX_W, TEX_H, TEX_W,
        SAT_PIXEL_INDEX8, sprite_palette, 256u
    };
    sat_example_must(sat_texture_create_from_surface(
        &sprite, &surf, SAT_TEXTURE_PERSISTENT_SOURCE));
    sat_example_must(sat_ascii_font_init_8x8_indexed8(
        &font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, FONT_BANK));
    sat_example_must(sat_vdp2_sprite_color_calc_configure_alpha(6u));

    const sat_rect_t rgb_base = {16, 80, 88u, 72u};
    const sat_rect_t rgb_half = {36, 96, 64u, 56u};
    const sat_rect_t middle = {120, 80, 72u, 72u};
    const sat_rect_t right = {220, 80, 72u, 72u};
    const uint8_t alpha_samples[3] = {64u, 128u, 192u};
    uint8_t alpha_index = 1u;
    uint8_t page = 0u;

    for (;;) {
        sat_pad_state_t pad = {0};
        sat_example_must(sat_wait_vblank());
        sat_example_must(sat_pad_poll(&pad));
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
        if ((pad.pressed & SAT_PAD_A) != 0u) {
            alpha_index = (uint8_t)((alpha_index + 1u) % 3u);
        }
        if ((pad.pressed & SAT_PAD_B) != 0u) page ^= 1u;
        sat_example_must(sat_vdp2_layers_commit());
        /* Generic VDP2 layer commit replays PRISA: replay the sprite-specific
         * color-calculation priority/ratio state afterwards, in the VBlank. */
        sat_example_must(sat_vdp2_sprite_color_calc_commit());
        sat_example_must(sat_vdp1_set_erase_transparent());
        sat_example_must(sat_begin_frame());

        /* RGB-on-RGB inside VDP1 framebuffer. Must draw RGB base FIRST. */
        sat_example_must(sat_fill_rect(
            &rgb_base, sat_color_rgba(23u, 90u, 215u, 255u)));
        sat_example_must(sat_fill_rect(
            &rgb_half, sat_color_rgba(255u, 180u, 28u, 128u)));

        if (page != 0u) {
            /* Additive: sprite + NBG0 per channel, saturating. */
            sat_draw_params_t added = sat_draw_params_default();
            added.blend_mode = SAT_BLEND_ADD;
            sat_example_must(sat_draw_texture(sprite, 0, &middle, &added));
            draw_text("SATURN BLEND MODES", 8, 8);
            draw_text("VDP2 ADD", 122, 58);
            draw_text("B: PAGE  START: EXIT", 8, 185);
            sat_example_must(sat_end_frame());
            continue;
        }

        /* Indexed8 sprite + VDP2 NBG0: not VDP1 framebuffer half-blending. */
        sat_draw_params_t blended = sat_draw_params_default();
        blended.blend_mode = SAT_BLEND_ALPHA;
        blended.tint.a = alpha_samples[alpha_index];
        sat_example_must(sat_draw_texture(
            sprite, 0, &middle, &blended));

        /* Alternating-pixel mesh: leaves half the VDP1 framebuffer untouched. */
        sat_draw_params_t meshed = sat_draw_params_default();
        meshed.flags = SAT_SPRITE_FLAG_MESH;
        sat_example_must(sat_draw_texture(sprite, 0, &right, &meshed));

        draw_text("SATURN TRANSPARENCY", 8, 8);
        draw_text("VDP1 RGB 50%", 8, 58);
        draw_text("VDP2 ALPHA", 114, 58);
        draw_text("VDP1 MESH", 220, 58);
        draw_text("A: ALPHA  B: PAGE  START: EXIT", 8, 185);
        char label[36];
        if (sat_fmt_label_u32("SPRITE A ", alpha_samples[alpha_index],
                label, sizeof(label), 0u) == SAT_OK) draw_text(label, 112, 162);
        sat_example_must(sat_end_frame());
    }
    sat_example_must(sat_texture_destroy(sprite));
    (void)sat_shutdown();
    return 0;
}
