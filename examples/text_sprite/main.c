/* text_sprite.c - exemplo dedicado a textura 2D gerada por asset */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/vdp1.h"
#include "saturn/example_util.h"
#include "text_sprite/sonic_head.h"

int main(void) {
    sat_example_must(sat_app_init_default());

    sat_texture_t texture = {0};
    sat_example_must(sat_tex_upload_indexed8(
        &texture,
        sonic_head_asset.pixels,
        sonic_head_asset.width,
        sonic_head_asset.height,
        sonic_head_asset.palette,
        sonic_head_asset.palette_index
    ));

    const uint16_t sprite_w = texture.width;
    const uint16_t sprite_h = texture.height;
    const sat_fx16_t min_x = (sat_fx16_t)(-160 * SAT_FX16_ONE);
    const sat_fx16_t min_y = (sat_fx16_t)(-112 * SAT_FX16_ONE);
    const sat_fx16_t max_x = (sat_fx16_t)(((int32_t)160 - (int32_t)sprite_w) * SAT_FX16_ONE);
    const sat_fx16_t max_y = (sat_fx16_t)(((int32_t)112 - (int32_t)sprite_h) * SAT_FX16_ONE);

    sat_fx16_t x = (sat_fx16_t)((-(int32_t)sprite_w / 2) * SAT_FX16_ONE);
    sat_fx16_t y = (sat_fx16_t)((-(int32_t)sprite_h / 2) * SAT_FX16_ONE);
    const sat_fx16_t speed = 4 * SAT_FX16_ONE;
    uint32_t frame = 0;

    while (1) {
        sat_pad_state_t pad = {0};
        sat_example_must(sat_app_frame_begin((frame & 32u) ? SAT_COLOR_BLUE : SAT_COLOR_BLACK, SAT_COLOR_BLACK, &pad));

        if ((pad.held & SAT_PAD_LEFT) != 0u) {
            x -= speed;
        }
        if ((pad.held & SAT_PAD_RIGHT) != 0u) {
            x += speed;
        }
        if ((pad.held & SAT_PAD_UP) != 0u) {
            y -= speed;
        }
        if ((pad.held & SAT_PAD_DOWN) != 0u) {
            y += speed;
        }

        if (x < min_x) {
            x = min_x;
        }
        if (y < min_y) {
            y = min_y;
        }
        if (x > max_x) {
            x = max_x;
        }
        if (y > max_y) {
            y = max_y;
        }

        sat_sprite_cmd_t cmd = {
            x,
            y,
            0,
            0,
            &texture,
            0,
            0
        };
        sat_example_must(sat_draw_sprite(&cmd));

        const uint8_t exit_requested = (uint8_t)((pad.pressed & SAT_PAD_START) != 0u);

        sat_example_must(sat_app_frame_end());

        if (exit_requested != 0u) {
            break;
        }

        ++frame;
    }

    return 0;
}
