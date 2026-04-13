/* red_square.c - quadrado vermelho movido com o direcional */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/vdp1.h"
#include "saturn/vdp2.h"
#include "saturn/example_util.h"

static uint8_t g_square_pixels[16 * 16];
static uint16_t g_square_palette[256];
static sat_texture_t g_square_tex;
static sat_ascii_font_t g_font;

static void build_square_palette(void) {
    for (uint16_t i = 0; i < 256; ++i) {
        g_square_palette[i] = SAT_COLOR_BLACK;
    }

    g_square_palette[0] = SAT_COLOR_BLACK;
    g_square_palette[1] = SAT_COLOR_RED;
}

static const char* color_name_for_rgb555(uint16_t rgb555) {
    switch (rgb555) {
        case SAT_COLOR_RED: return "RED";
        case SAT_COLOR_GREEN: return "GREEN";
        case SAT_COLOR_BLUE: return "BLUE";
        case SAT_COLOR_YELLOW: return "YELLOW";
        case SAT_COLOR_MAGENTA: return "MAGENTA";
        case SAT_COLOR_CYAN: return "CYAN";
        case SAT_COLOR_WHITE: return "WHITE";
        case SAT_COLOR_ORANGE: return "ORANGE";
        case SAT_COLOR_VIOLET: return "VIOLET";
        case SAT_COLOR_GRAY: return "GRAY";
        case SAT_COLOR_TEAL: return "TEAL";
        case SAT_COLOR_OLIVE: return "OLIVE";
        case SAT_COLOR_BROWN: return "BROWN";
        default: return "RED";
    }
}

static uint16_t choose_square_color(const sat_pad_state_t* pad) {
    if ((pad->held & SAT_PAD_UP) != 0u) return SAT_COLOR_BLUE;
    if ((pad->held & SAT_PAD_DOWN) != 0u) return SAT_COLOR_GREEN;
    if ((pad->held & SAT_PAD_LEFT) != 0u) return SAT_COLOR_RED;
    if ((pad->held & SAT_PAD_RIGHT) != 0u) return SAT_COLOR_YELLOW;
    if ((pad->held & SAT_PAD_START) != 0u) return SAT_COLOR_WHITE;
    if ((pad->held & SAT_PAD_A) != 0u) return SAT_COLOR_MAGENTA;
    if ((pad->held & SAT_PAD_B) != 0u) return SAT_COLOR_CYAN;
    if ((pad->held & SAT_PAD_C) != 0u) return SAT_COLOR_ORANGE;
    if ((pad->held & SAT_PAD_X) != 0u) return SAT_COLOR_VIOLET;
    if ((pad->held & SAT_PAD_Y) != 0u) return SAT_COLOR_GRAY;
    if ((pad->held & SAT_PAD_Z) != 0u) return SAT_COLOR_TEAL;
    if ((pad->held & SAT_PAD_L) != 0u) return SAT_COLOR_OLIVE;
    if ((pad->held & SAT_PAD_R) != 0u) return SAT_COLOR_BROWN;
    return SAT_COLOR_RED;
}

static sat_result_t update_square_palette(uint16_t rgb555) {
    g_square_palette[1] = rgb555;
    return sat_vdp2_palette_upload(g_square_palette, 2, 0);
}

static void build_square(void) {
    for (uint16_t y = 0; y < 16; ++y) {
        for (uint16_t x = 0; x < 16; ++x) {
            g_square_pixels[(y * 16u) + x] = 1u;
        }
    }
}

static void set_button_char(char* line, uint16_t pos, uint16_t is_down) {
    line[pos] = (is_down != 0u) ? '1' : '0';
}

int main(void) {
    sat_example_must(sat_app_init_default());

    build_square_palette();
    build_square();

    sat_example_must(sat_tex_upload_indexed8(&g_square_tex, g_square_pixels, 16, 16, g_square_palette, 0));
    sat_example_must(sat_ascii_font_init_8x8_indexed8(&g_font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 1));

    sat_fx16_t square_x = -8 * SAT_FX16_ONE;
    sat_fx16_t square_y = 40 * SAT_FX16_ONE;
    const sat_fx16_t speed = (sat_fx16_t)(16 * SAT_FX16_ONE);
    const sat_fx16_t min_x = -160 * SAT_FX16_ONE;
    const sat_fx16_t min_y = -112 * SAT_FX16_ONE;
    const sat_fx16_t max_x = 144 * SAT_FX16_ONE;
    const sat_fx16_t max_y = 96 * SAT_FX16_ONE;

    while (1) {
        sat_pad_state_t pad = {0};
        sat_example_must(sat_app_frame_begin(SAT_COLOR_BLUE, SAT_COLOR_BLACK, &pad));

        if ((pad.held & SAT_PAD_LEFT) != 0u) {
            square_x -= speed;
        }
        if ((pad.held & SAT_PAD_RIGHT) != 0u) {
            square_x += speed;
        }
        if ((pad.held & SAT_PAD_UP) != 0u) {
            square_y -= speed;
        }
        if ((pad.held & SAT_PAD_DOWN) != 0u) {
            square_y += speed;
        }

        const uint16_t square_color = choose_square_color(&pad);
        sat_example_must(update_square_palette(square_color));

        if (square_x < min_x) {
            square_x = min_x;
        }
        if (square_y < min_y) {
            square_y = min_y;
        }
        if (square_x > max_x) {
            square_x = max_x;
        }
        if (square_y > max_y) {
            square_y = max_y;
        }

        sat_sprite_cmd_t cmd = {
            square_x,
            square_y,
            16,
            16,
            &g_square_tex,
            0,
            0
        };
        sat_example_must(sat_draw_sprite(&cmd));

        const char* color_name = color_name_for_rgb555(square_color);
        char line1[] = "U:0 D:0 L:0 R:0 S:0";
        char line2[] = "A:0 B:0 C:0 X:0 Y:0 Z:0";

        set_button_char(line1, 2u, (pad.held & SAT_PAD_UP) != 0u);
        set_button_char(line1, 6u, (pad.held & SAT_PAD_DOWN) != 0u);
        set_button_char(line1, 10u, (pad.held & SAT_PAD_LEFT) != 0u);
        set_button_char(line1, 14u, (pad.held & SAT_PAD_RIGHT) != 0u);
        set_button_char(line1, 18u, (pad.held & SAT_PAD_START) != 0u);

        set_button_char(line2, 2u, (pad.held & SAT_PAD_A) != 0u);
        set_button_char(line2, 6u, (pad.held & SAT_PAD_B) != 0u);
        set_button_char(line2, 10u, (pad.held & SAT_PAD_C) != 0u);
        set_button_char(line2, 14u, (pad.held & SAT_PAD_X) != 0u);
        set_button_char(line2, 18u, (pad.held & SAT_PAD_Y) != 0u);
        set_button_char(line2, 22u, (pad.held & SAT_PAD_Z) != 0u);

        sat_example_must(sat_ascii_font_draw_text_indexed8(&g_font, "COLOR: ", -156, -110, 8, 0, 0));
        sat_example_must(sat_ascii_font_draw_text_indexed8(&g_font, color_name, -100, -110, 8, 0, 0));
        sat_example_must(sat_ascii_font_draw_text_indexed8(&g_font, line1, -156, -100, 8, 0, 0));
        sat_example_must(sat_ascii_font_draw_text_indexed8(&g_font, line2, -156, -90, 8, 0, 0));

        sat_example_must(sat_app_frame_end());
    }
}
