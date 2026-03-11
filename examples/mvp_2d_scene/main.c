#include <stdint.h>
#include <stddef.h>

#include "saturn/saturn.h"

#define STAGE_COLOR_A 0x801F
#define STAGE_COLOR_B 0x83E0
#define STAGE_COLOR_C 0xFC00
#define STAGE_COLOR_D0 0xFFFF
#define STAGE_COLOR_D1 0xBDEF

static uint8_t g_player_pixels[16 * 16];
static uint8_t g_tile_pixels[16 * 16];
static uint8_t g_hud_pixels[16 * 16];
static uint16_t g_palette[256];

static void show_stage_a_marker(void) {
    volatile uint16_t* const vdp2_tvmd = (volatile uint16_t*)0x05F80000;
    volatile uint16_t* const vdp2_bgon = (volatile uint16_t*)0x05F80010;
    volatile uint16_t* const vdp2_cram = (volatile uint16_t*)0x05F00000;

    volatile uint16_t* const vdp1_tvmr = (volatile uint16_t*)0x05D00000;
    volatile uint16_t* const vdp1_fbcr = (volatile uint16_t*)0x05D00002;
    volatile uint16_t* const vdp1_ptmr = (volatile uint16_t*)0x05D00004;
    volatile uint16_t* const vdp1_ewdr = (volatile uint16_t*)0x05D00006;
    volatile uint16_t* const vdp1_ewlr = (volatile uint16_t*)0x05D00008;
    volatile uint16_t* const vdp1_ewrr = (volatile uint16_t*)0x05D0000A;

    *vdp2_tvmd = 0x0000;
    *vdp2_bgon = 0x0000;
    vdp2_cram[0] = STAGE_COLOR_A;
    *vdp2_tvmd = 0x8100;

    *vdp1_tvmr = 0x0000;
    *vdp1_fbcr = 0x0000;
    *vdp1_ewdr = STAGE_COLOR_A;
    *vdp1_ewlr = 0x0000;
    *vdp1_ewrr = (uint16_t)(((320u / 8u) << 9u) | 224u);
    *vdp1_ptmr = 0x0002;

    for (volatile uint32_t spin = 0; spin < 250000u; ++spin) {
    }
}

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
    show_stage_a_marker();

    sat_video_config_t cfg = {320, 224, 1, 0};
    sat_result_t st = sat_init(&cfg);
    if (st != SAT_OK) {
        for (;;) {
        }
    }
    sat_set_clear_color(STAGE_COLOR_B);

    build_palette();
    build_textures();
    sat_set_clear_color(STAGE_COLOR_C);

    sat_texture_t player_tex = {0};
    sat_texture_t tile_tex = {0};
    sat_texture_t hud_tex = {0};

    st = sat_tex_upload_indexed8(&player_tex, g_player_pixels, 16, 16, g_palette, 0);
    if (st != SAT_OK) {
        for (;;) {
        }
    }
    st = sat_tex_upload_indexed8(&tile_tex, g_tile_pixels, 16, 16, g_palette, 0);
    if (st != SAT_OK) {
        for (;;) {
        }
    }
    st = sat_tex_upload_indexed8(&hud_tex, g_hud_pixels, 16, 16, g_palette, 0);
    if (st != SAT_OK) {
        for (;;) {
        }
    }

    sat_fx16_t player_x = 160 * SAT_FX16_ONE;
    sat_fx16_t player_y = 112 * SAT_FX16_ONE;
    const sat_fx16_t speed = (sat_fx16_t)(2 * SAT_FX16_ONE);
    uint32_t frame_counter = 0;

    for (;;) {
        sat_pad_state_t pad = {0};
        sat_wait_vblank();
        sat_pad_poll(&pad);
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
