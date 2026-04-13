#include <stdint.h>
#include <stddef.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/vdp1.h"
#include "saturn/vdp2.h"
#include "saturn/example_util.h"

#define STAGE_COLOR_A 0x801F
#define STAGE_COLOR_B 0x83E0
#define STAGE_COLOR_C 0xFC00
#define STAGE_COLOR_D0 0xFFFF
#define STAGE_COLOR_D1 0xBDEF
#define HELLO_BG_A 0x001F
#define HELLO_BG_B 0x03E0
#define MVP_HELLO_VISUAL_ONLY 0
#define HELLO_TEXT "HELLO WORLD"
#define HELLO_GLYPH_W 16
#define HELLO_GLYPH_H 16
#define HELLO_CHAR_COUNT 11
#define HELLO_TEX_W (HELLO_GLYPH_W * HELLO_CHAR_COUNT)
#define HELLO_TEX_H HELLO_GLYPH_H

static uint8_t g_player_pixels[16 * 16];
static uint8_t g_tile_pixels[16 * 16];
static uint8_t g_hud_pixels[16 * 16];
static uint8_t g_hello_pixels[HELLO_TEX_W * HELLO_TEX_H];
static uint16_t g_palette[256];

static sat_result_t build_hello_texture(void) {
    const char* text = HELLO_TEXT;
    for (uint16_t y = 0; y < HELLO_TEX_H; ++y) {
        for (uint16_t x = 0; x < HELLO_TEX_W; ++x) {
            g_hello_pixels[(y * HELLO_TEX_W) + x] = 0u;
        }
    }

    for (uint16_t i = 0; i < HELLO_CHAR_COUNT; ++i) {
        const char ch = text[i];
        uint8_t glyph_rows[8];
        for (uint16_t row = 0; row < 8u; ++row) {
            glyph_rows[row] = sat_font_ascii_8x8_rows(ch)[row];
        }

        SAT_TRY(sat_font_pack_8x8_glyph_indexed8(
            g_hello_pixels,
            HELLO_TEX_W,
            HELLO_TEX_H,
            (uint16_t)(i * HELLO_GLYPH_W),
            0,
            glyph_rows,
            2
        ));
    }

    return SAT_OK;
}

#if MVP_HELLO_VISUAL_ONLY
static void hello_visual_loop(const sat_texture_t* hello_tex) {
    uint32_t counter = 0;
    for (;;) {
        const uint16_t bg = ((counter & 32u) != 0u) ? HELLO_BG_A : HELLO_BG_B;
        sat_example_must(sat_app_frame_begin(bg, 0x0000, NULL));

        sat_sprite_cmd_t hello_cmd = {
            64 * SAT_FX16_ONE,
            104 * SAT_FX16_ONE,
            HELLO_TEX_W,
            HELLO_TEX_H,
            hello_tex,
            0,
            0
        };
        sat_example_must(sat_draw_sprite(&hello_cmd));
        sat_example_must(sat_app_frame_end());

        for (volatile uint32_t spin = 0; spin < 120000u; ++spin) {
        }
        ++counter;
    }
}
#endif

static void build_palette(void) {
    for (uint16_t i = 0; i < 256; ++i) {
        uint16_t r = (uint16_t)((i & 0x1F) << 10);
        uint16_t g = (uint16_t)(((i >> 3) & 0x1F) << 5);
        uint16_t b = (uint16_t)((i >> 1) & 0x1F);
        g_palette[i] = (uint16_t)(0x8000u | r | g | b);
    }
    g_palette[0] = 0x0000;
    g_palette[1] = 0xFFFF;
    g_palette[2] = 0xFC00;
    g_palette[3] = 0x83E0;
    g_palette[4] = 0x801F;
    g_palette[5] = 0xBDEF;
}

static void build_textures(void) {
    for (uint16_t y = 0; y < 16; ++y) {
        for (uint16_t x = 0; x < 16; ++x) {
            uint16_t idx = (uint16_t)(y * 16 + x);

            g_tile_pixels[idx] = (uint8_t)(((x ^ y) & 1u) ? 5u : 3u);
            g_hud_pixels[idx] = (uint8_t)((x == 0 || y == 0 || x == 15 || y == 15) ? 1u : 4u);

            if (x == 0 || y == 0 || x == 15 || y == 15) {
                g_player_pixels[idx] = 2u;
            } else if ((x > 4 && x < 11) && (y > 4 && y < 11)) {
                g_player_pixels[idx] = 1u;
            } else {
                g_player_pixels[idx] = 0u;
            }
        }
    }
}

int main(void) {
    sat_example_must(sat_app_init_default());
    sat_example_must(sat_set_clear_color(STAGE_COLOR_B));
    sat_example_must(sat_vdp2_back_color_set(HELLO_BG_A));

    build_palette();
    build_textures();
    sat_example_must(build_hello_texture());
    g_palette[0] = 0x0000;
    g_palette[1] = 0xFFFF;
    sat_example_must(sat_set_clear_color(STAGE_COLOR_C));

    sat_texture_t player_tex = {0};
    sat_texture_t tile_tex = {0};
    sat_texture_t hud_tex = {0};
    sat_texture_t hello_tex = {0};

    sat_example_must(sat_tex_upload_indexed8(&player_tex, g_player_pixels, 16, 16, g_palette, 0));
    sat_example_must(sat_tex_upload_indexed8(&tile_tex, g_tile_pixels, 16, 16, g_palette, 0));
    sat_example_must(sat_tex_upload_indexed8(&hud_tex, g_hud_pixels, 16, 16, g_palette, 0));
    sat_example_must(sat_tex_upload_indexed8(&hello_tex, g_hello_pixels, HELLO_TEX_W, HELLO_TEX_H, g_palette, 0));

#if MVP_HELLO_VISUAL_ONLY
    hello_visual_loop(&hello_tex);
#endif

    sat_fx16_t player_x = 160 * SAT_FX16_ONE;
    sat_fx16_t player_y = 112 * SAT_FX16_ONE;
    const sat_fx16_t speed = (sat_fx16_t)(2 * SAT_FX16_ONE);
    uint32_t frame_counter = 0;

    for (;;) {
        sat_pad_state_t pad = {0};
        const uint16_t backdrop = (frame_counter & 32u) ? HELLO_BG_A : HELLO_BG_B;
        const uint16_t clear = (frame_counter & 1u) ? STAGE_COLOR_D0 : STAGE_COLOR_D1;
        sat_example_must(sat_app_frame_begin(backdrop, clear, &pad));

        if ((pad.held & SAT_PAD_LEFT) != 0u) {
            player_x -= speed;
        }
        if ((pad.held & SAT_PAD_RIGHT) != 0u) {
            player_x += speed;
        }
        if ((pad.held & SAT_PAD_UP) != 0u) {
            player_y -= speed;
        }
        if ((pad.held & SAT_PAD_DOWN) != 0u) {
            player_y += speed;
        }

        if (player_x < 0) {
            player_x = 0;
        }
        if (player_y < 0) {
            player_y = 0;
        }
        if (player_x > (304 * SAT_FX16_ONE)) {
            player_x = (304 * SAT_FX16_ONE);
        }
        if (player_y > (208 * SAT_FX16_ONE)) {
            player_y = (208 * SAT_FX16_ONE);
        }

        for (uint16_t ty = 0; ty < 7; ++ty) {
            for (uint16_t tx = 0; tx < 10; ++tx) {
                sat_sprite_cmd_t tile_cmd = {
                    (sat_fx16_t)((tx * 32) * SAT_FX16_ONE),
                    (sat_fx16_t)((ty * 32) * SAT_FX16_ONE),
                    16,
                    16,
                    &tile_tex,
                    0,
                    0
                };
                sat_example_must(sat_draw_sprite(&tile_cmd));

                tile_cmd.x += (16 * SAT_FX16_ONE);
                sat_example_must(sat_draw_sprite(&tile_cmd));
                tile_cmd.y += (16 * SAT_FX16_ONE);
                sat_example_must(sat_draw_sprite(&tile_cmd));
                tile_cmd.x -= (16 * SAT_FX16_ONE);
                sat_example_must(sat_draw_sprite(&tile_cmd));
            }
        }

        sat_sprite_cmd_t player_cmd = {
            player_x,
            player_y,
            16,
            16,
            &player_tex,
            0,
            0
        };
        sat_example_must(sat_draw_sprite(&player_cmd));

        for (uint16_t i = 0; i < 5; ++i) {
            sat_sprite_cmd_t hud_cmd = {
                (sat_fx16_t)((8 + (i * 18)) * SAT_FX16_ONE),
                8 * SAT_FX16_ONE,
                16,
                16,
                &hud_tex,
                0,
                0
            };
            sat_example_must(sat_draw_sprite(&hud_cmd));
        }

        sat_example_must(sat_app_frame_end());
        ++frame_counter;
    }
}
