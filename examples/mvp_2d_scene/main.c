#include <stdint.h>
#include <stddef.h>

#include "saturn/saturn.h"

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

static void set_backdrop_color(uint16_t rgb555) {
    (void)sat_vdp2_back_color_set(rgb555);
}

static void panic_color_loop(uint16_t color_a, uint16_t color_b) {
    for (;;) {
        set_backdrop_color(color_a);
        for (volatile uint32_t spin = 0; spin < 250000u; ++spin) {
        }
        set_backdrop_color(color_b);
        for (volatile uint32_t spin = 0; spin < 250000u; ++spin) {
        }
    }
}

static uint8_t hello_glyph_row(char ch, uint16_t row) {
    if (row >= 8u) {
        return 0x00u;
    }

    switch (ch) {
        case 'H': {
            static const uint8_t kRows[8] = {0x81u, 0x81u, 0x81u, 0xFFu, 0x81u, 0x81u, 0x81u, 0x00u};
            return kRows[row];
        }
        case 'E': {
            static const uint8_t kRows[8] = {0xFFu, 0x80u, 0x80u, 0xFEu, 0x80u, 0x80u, 0xFFu, 0x00u};
            return kRows[row];
        }
        case 'L': {
            static const uint8_t kRows[8] = {0x80u, 0x80u, 0x80u, 0x80u, 0x80u, 0x80u, 0xFFu, 0x00u};
            return kRows[row];
        }
        case 'O': {
            static const uint8_t kRows[8] = {0x7Eu, 0x81u, 0x81u, 0x81u, 0x81u, 0x81u, 0x7Eu, 0x00u};
            return kRows[row];
        }
        case 'W': {
            static const uint8_t kRows[8] = {0x81u, 0x81u, 0x81u, 0x91u, 0x91u, 0x91u, 0x6Eu, 0x00u};
            return kRows[row];
        }
        case 'R': {
            static const uint8_t kRows[8] = {0xFEu, 0x81u, 0x81u, 0xFEu, 0x90u, 0x88u, 0x84u, 0x00u};
            return kRows[row];
        }
        case 'D': {
            static const uint8_t kRows[8] = {0xFCu, 0x82u, 0x81u, 0x81u, 0x81u, 0x82u, 0xFCu, 0x00u};
            return kRows[row];
        }
        default:
            return 0x00u;
    }
}

static void build_hello_texture(void) {
    const char* text = HELLO_TEXT;
    for (uint16_t y = 0; y < HELLO_TEX_H; ++y) {
        for (uint16_t x = 0; x < HELLO_TEX_W; ++x) {
            g_hello_pixels[(y * HELLO_TEX_W) + x] = 0u;
        }
    }

    for (uint16_t i = 0; i < HELLO_CHAR_COUNT; ++i) {
        const char ch = text[i];
        for (uint16_t row = 0; row < 8u; ++row) {
            const uint8_t bits = hello_glyph_row(ch, row);
            for (uint16_t col = 0; col < 8u; ++col) {
                const uint8_t bit = (uint8_t)((bits >> (7u - col)) & 1u);
                if (bit == 0u) {
                    continue;
                }
                const uint16_t base_x = (uint16_t)(i * HELLO_GLYPH_W + col * 2u);
                const uint16_t base_y = (uint16_t)(row * 2u);
                g_hello_pixels[(base_y * HELLO_TEX_W) + base_x] = 1u;
                g_hello_pixels[(base_y * HELLO_TEX_W) + (base_x + 1u)] = 1u;
                g_hello_pixels[((base_y + 1u) * HELLO_TEX_W) + base_x] = 1u;
                g_hello_pixels[((base_y + 1u) * HELLO_TEX_W) + (base_x + 1u)] = 1u;
            }
        }
    }
}

#if MVP_HELLO_VISUAL_ONLY
static const uint32_t SAT_UNCACHED_BASE = 0x20000000u;
static void hello_visual_loop(const sat_texture_t* hello_tex) {
    volatile uint16_t* const vdp1_fbcr = (volatile uint16_t*)(SAT_UNCACHED_BASE | 0x05D00002u);
    volatile uint16_t* const vdp1_ptmr = (volatile uint16_t*)(SAT_UNCACHED_BASE | 0x05D00004u);
    uint32_t counter = 0;
    for (;;) {
        sat_wait_vblank();
        const uint16_t bg = ((counter & 32u) != 0u) ? HELLO_BG_A : HELLO_BG_B;
        set_backdrop_color(bg);
        sat_set_clear_color(0x0000);
        sat_begin_frame();

        sat_sprite_cmd_t hello_cmd = {
            64 * SAT_FX16_ONE,
            104 * SAT_FX16_ONE,
            HELLO_TEX_W,
            HELLO_TEX_H,
            hello_tex,
            0,
            0
        };
        sat_draw_sprite(&hello_cmd);
        sat_end_frame();
        *vdp1_fbcr = 0x0003;
        *vdp1_ptmr = 0x0001;

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
    sat_video_config_t cfg = {320, 224, 1, 0};
    sat_result_t st = sat_init(&cfg);
    if (st != SAT_OK) {
        panic_color_loop(0xFC00, 0x801F);
    }
    sat_set_clear_color(STAGE_COLOR_B);
    set_backdrop_color(HELLO_BG_A);

    build_palette();
    build_textures();
    build_hello_texture();
    g_palette[0] = 0x0000;
    g_palette[1] = 0xFFFF;
    sat_set_clear_color(STAGE_COLOR_C);

    sat_texture_t player_tex = {0};
    sat_texture_t tile_tex = {0};
    sat_texture_t hud_tex = {0};
    sat_texture_t hello_tex = {0};

    st = sat_tex_upload_indexed8(&player_tex, g_player_pixels, 16, 16, g_palette, 0);
    if (st != SAT_OK) {
        panic_color_loop(0x83E0, 0xFC00);
    }
    st = sat_tex_upload_indexed8(&tile_tex, g_tile_pixels, 16, 16, g_palette, 0);
    if (st != SAT_OK) {
        panic_color_loop(0x83E0, 0xFC00);
    }
    st = sat_tex_upload_indexed8(&hud_tex, g_hud_pixels, 16, 16, g_palette, 0);
    if (st != SAT_OK) {
        panic_color_loop(0x83E0, 0xFC00);
    }
    st = sat_tex_upload_indexed8(&hello_tex, g_hello_pixels, HELLO_TEX_W, HELLO_TEX_H, g_palette, 0);
    if (st != SAT_OK) {
        panic_color_loop(0x83E0, 0xFC00);
    }

#if MVP_HELLO_VISUAL_ONLY
    hello_visual_loop(&hello_tex);
#endif

    sat_fx16_t player_x = 160 * SAT_FX16_ONE;
    sat_fx16_t player_y = 112 * SAT_FX16_ONE;
    const sat_fx16_t speed = (sat_fx16_t)(2 * SAT_FX16_ONE);
    uint32_t frame_counter = 0;

    for (;;) {
        sat_pad_state_t pad = {0};
        sat_wait_vblank();
        sat_pad_poll(&pad);
        set_backdrop_color((frame_counter & 32u) ? HELLO_BG_A : HELLO_BG_B);
        sat_set_clear_color((frame_counter & 1u) ? STAGE_COLOR_D0 : STAGE_COLOR_D1);
        ++frame_counter;

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

        sat_begin_frame();

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
                sat_draw_sprite(&tile_cmd);

                tile_cmd.x += (16 * SAT_FX16_ONE);
                sat_draw_sprite(&tile_cmd);
                tile_cmd.y += (16 * SAT_FX16_ONE);
                sat_draw_sprite(&tile_cmd);
                tile_cmd.x -= (16 * SAT_FX16_ONE);
                sat_draw_sprite(&tile_cmd);
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
        sat_draw_sprite(&player_cmd);

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
            sat_draw_sprite(&hud_cmd);
        }

        sat_end_frame();
    }
}
