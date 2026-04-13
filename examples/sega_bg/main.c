/* sega_bg.c - Displays background image (Sega Saturn wallpaper) */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/vdp1.h"
#include "saturn/example_util.h"
#include "sega_bg/bg.h"

int main(void) {
    sat_example_must(sat_app_init_default());

    /* Upload background texture to VRAM */
    sat_texture_t bg_tex = {0};
    sat_example_must(sat_tex_upload_indexed8(
        &bg_tex,
        bg_asset.pixels,
        bg_asset.width,
        bg_asset.height,
        bg_asset.palette,
        bg_asset.palette_index
    ));

    uint32_t frame = 0;
    while (1) {
        sat_pad_state_t pad = {0};
        sat_example_must(sat_app_frame_begin(SAT_COLOR_BLACK, SAT_COLOR_BLACK, &pad));

        /* Draw background covering the entire screen.
         * sat_draw_sprite_screen uses (0,0) as top-left corner.
         * Position (160, 112) = center of 320x224 screen.
         */
        sat_example_must(sat_draw_sprite_screen(
            &bg_tex,
            160,                    /* screen_x = horizontal center */
            112,                    /* screen_y = vertical center */
            bg_tex.width,
            bg_tex.height,
            0                       /* palette_override */
        ));

        /* Exit on START press */
        uint8_t exit_requested = (pad.pressed & SAT_PAD_START) != 0;

        sat_example_must(sat_app_frame_end());

        if (exit_requested) {
            break;
        }

        ++frame;
    }

    return 0;
}
