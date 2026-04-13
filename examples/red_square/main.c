/* red_square.c - quadrado vermelho movido com o direcional */
#include <stdint.h>

#include "examples/common/demo.h"

enum {
    HUD_SPACE = 0,
    HUD_COLON,
    HUD_ZERO,
    HUD_ONE,
    HUD_A,
    HUD_B,
    HUD_C,
    HUD_D,
    HUD_E,
    HUD_F,
    HUD_G,
    HUD_H,
    HUD_I,
    HUD_J,
    HUD_K,
    HUD_L,
    HUD_M,
    HUD_N,
    HUD_O,
    HUD_P,
    HUD_Q,
    HUD_R,
    HUD_S,
    HUD_T,
    HUD_U,
    HUD_V,
    HUD_W,
    HUD_X,
    HUD_Y,
    HUD_Z,
    HUD_GLYPH_COUNT
};

static const uint8_t g_font_rows[HUD_GLYPH_COUNT][8] = {
    /* space */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* : */     {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00},
    /* 0 */     {0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3C, 0x00},
    /* 1 */     {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00},
    /* A */     {0x18, 0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x00},
    /* B */     {0xFC, 0x66, 0x66, 0x7C, 0x66, 0x66, 0xFC, 0x00},
    /* C */     {0x3C, 0x66, 0xC0, 0xC0, 0xC0, 0x66, 0x3C, 0x00},
    /* D */     {0xFC, 0x66, 0x66, 0x66, 0x66, 0x66, 0xFC, 0x00},
    /* E */     {0xFE, 0x60, 0x60, 0x78, 0x60, 0x60, 0xFE, 0x00},
    /* F */     {0xFE, 0x60, 0x60, 0x78, 0x60, 0x60, 0x60, 0x00},
    /* G */     {0x3C, 0x66, 0xC0, 0xCE, 0xC6, 0x66, 0x3C, 0x00},
    /* H */     {0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00},
    /* I */     {0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
    /* J */     {0x1E, 0x0C, 0x0C, 0x0C, 0xCC, 0xCC, 0x78, 0x00},
    /* K */     {0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00},
    /* L */     {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0xFE, 0x00},
    /* M */     {0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00},
    /* N */     {0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00},
    /* O */     {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    /* P */     {0xFC, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00},
    /* Q */     {0x3C, 0x66, 0x66, 0x66, 0x6E, 0x6C, 0x36, 0x00},
    /* R */     {0xFC, 0x66, 0x66, 0x7C, 0x6C, 0x66, 0x66, 0x00},
    /* S */     {0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00},
    /* T */     {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
    /* U */     {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    /* V */     {0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00},
    /* W */     {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00},
    /* X */     {0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00},
    /* Y */     {0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x3C, 0x00},
    /* Z */     {0xFE, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0xFE, 0x00},
};

static uint8_t g_square_pixels[16 * 16];
static uint8_t g_font_pixels[HUD_GLYPH_COUNT][8 * 8];
static uint16_t g_square_palette[256];
static uint16_t g_font_palette[256];
static sat_texture_t g_square_tex;
static sat_texture_t g_font_tex[HUD_GLYPH_COUNT];

static void build_square_palette(void) {
    for (uint16_t i = 0; i < 256; ++i) {
        g_square_palette[i] = SAT_COLOR_BLACK;
        g_font_palette[i] = SAT_COLOR_BLACK;
    }

    g_square_palette[0] = SAT_COLOR_BLACK;
    g_square_palette[1] = SAT_COLOR_RED;

    g_font_palette[0] = SAT_COLOR_BLACK;
    g_font_palette[1] = SAT_COLOR_WHITE;
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

static void build_font(void) {
    for (uint16_t glyph = 0; glyph < HUD_GLYPH_COUNT; ++glyph) {
        for (uint16_t row = 0; row < 8u; ++row) {
            const uint8_t bits = g_font_rows[glyph][row];
            for (uint16_t col = 0; col < 8u; ++col) {
                const uint8_t on = (uint8_t)((bits >> (7u - col)) & 1u);
                g_font_pixels[glyph][(row * 8u) + col] = on;
            }
        }
    }
}

static sat_result_t upload_font(void) {
    for (uint16_t i = 0; i < HUD_GLYPH_COUNT; ++i) {
        sat_result_t st = sat_tex_upload_indexed8(&g_font_tex[i], g_font_pixels[i], 8, 8, g_font_palette, 1);
        if (st != SAT_OK) {
            return st;
        }
    }
    return SAT_OK;
}

static uint16_t glyph_for_char(char c) {
    switch (c) {
        case ' ': return HUD_SPACE;
        case ':': return HUD_COLON;
        case '0': return HUD_ZERO;
        case '1': return HUD_ONE;
        case 'A': return HUD_A;
        case 'B': return HUD_B;
        case 'C': return HUD_C;
        case 'D': return HUD_D;
        case 'E': return HUD_E;
        case 'F': return HUD_F;
        case 'G': return HUD_G;
        case 'H': return HUD_H;
        case 'I': return HUD_I;
        case 'J': return HUD_J;
        case 'K': return HUD_K;
        case 'L': return HUD_L;
        case 'M': return HUD_M;
        case 'N': return HUD_N;
        case 'O': return HUD_O;
        case 'P': return HUD_P;
        case 'Q': return HUD_Q;
        case 'R': return HUD_R;
        case 'S': return HUD_S;
        case 'T': return HUD_T;
        case 'U': return HUD_U;
        case 'V': return HUD_V;
        case 'W': return HUD_W;
        case 'X': return HUD_X;
        case 'Y': return HUD_Y;
        case 'Z': return HUD_Z;
        default:  return HUD_SPACE;
    }
}

static void draw_text_line(int x, int y, const char* text) {
    for (uint16_t i = 0; text[i] != '\0'; ++i) {
        const uint16_t idx = glyph_for_char(text[i]);
        sat_sprite_cmd_t cmd = {
            (sat_fx16_t)((x + (int)(i * 8u)) * SAT_FX16_ONE),
            (sat_fx16_t)(y * SAT_FX16_ONE),
            8,
            8,
            &g_font_tex[idx],
            1,
            0
        };
        (void)sat_draw_sprite(&cmd);
    }
}

static void set_button_char(char* line, uint16_t pos, uint16_t is_down) {
    line[pos] = (is_down != 0u) ? '1' : '0';
}

int main(void) {
    sat_video_config_t cfg = {320, 224, 1, 0};
    sat_result_t st = sat_init(&cfg);
    if (st != SAT_OK) {
        for (;;) {
        }
    }

    build_square_palette();
    build_square();
    build_font();

    st = sat_tex_upload_indexed8(&g_square_tex, g_square_pixels, 16, 16, g_square_palette, 0);
    if (st != SAT_OK) {
        for (;;) {
        }
    }

    st = upload_font();
    if (st != SAT_OK) {
        for (;;) {
        }
    }

    sat_fx16_t square_x = -8 * SAT_FX16_ONE;
    sat_fx16_t square_y = 40 * SAT_FX16_ONE;
    const sat_fx16_t speed = (sat_fx16_t)(16 * SAT_FX16_ONE);
    const sat_fx16_t min_x = -160 * SAT_FX16_ONE;
    const sat_fx16_t min_y = -112 * SAT_FX16_ONE;
    const sat_fx16_t max_x = 144 * SAT_FX16_ONE;
    const sat_fx16_t max_y = 96 * SAT_FX16_ONE;

    for (;;) {
        sat_pad_state_t pad = {0};
        st = demo_frame_begin(SAT_COLOR_BLUE, SAT_COLOR_BLACK, &pad);
        if (st != SAT_OK) {
            for (;;) {
            }
        }

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
        st = update_square_palette(square_color);
        if (st != SAT_OK) {
            for (;;) {
            }
        }

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
        st = sat_draw_sprite(&cmd);
        if (st != SAT_OK) {
            for (;;) {
            }
        }

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

        draw_text_line(-156, -110, "COLOR: ");
        draw_text_line(-100, -110, color_name);
        draw_text_line(-156, -100, line1);
        draw_text_line(-156, -90, line2);

        st = demo_frame_end();
        if (st != SAT_OK) {
            for (;;) {
            }
        }
    }
}
