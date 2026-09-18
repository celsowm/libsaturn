/* hello_world.c - high-level baked-atlas text acceptance example */
#include <stdint.h>

#include "saturn/app.h"
#include "saturn/color.h"
#include "saturn/font.h"
#include "saturn/surface.h"
#include "saturn/texture.h"
#include "saturn/example_util.h"

#define FONT_GLYPH_COUNT 9u
#define FONT_ATLAS_WIDTH (FONT_GLYPH_COUNT * SAT_ASCII_FONT_GLYPH_WIDTH)
#define FONT_ATLAS_HEIGHT SAT_ASCII_FONT_GLYPH_HEIGHT

static uint8_t g_font_pixels[FONT_ATLAS_WIDTH * FONT_ATLAS_HEIGHT];
static uint16_t g_font_palette[256];
static sat_font_glyph_t g_font_glyphs[FONT_GLYPH_COUNT];

static void build_acceptance_font(void) {
    static const char kGlyphChars[FONT_GLYPH_COUNT] = {'H', 'E', 'L', 'O', ' ', 'W', 'R', 'D', '!'};

    g_font_palette[0] = SAT_COLOR_BLACK;
    g_font_palette[1] = SAT_COLOR_WHITE;
    for (uint16_t glyph = 0u; glyph < FONT_GLYPH_COUNT; ++glyph) {
        const uint8_t* rows = sat_font_ascii_8x8_rows(kGlyphChars[glyph]);
        const uint16_t x = (uint16_t)(glyph * SAT_ASCII_FONT_GLYPH_WIDTH);
        for (uint16_t row = 0u; row < SAT_ASCII_FONT_GLYPH_HEIGHT; ++row) {
            for (uint16_t column = 0u; column < SAT_ASCII_FONT_GLYPH_WIDTH; ++column) {
                g_font_pixels[row * FONT_ATLAS_WIDTH + x + column] =
                    (uint8_t)((rows[row] & (uint8_t)(1u << (7u - column))) != 0u);
            }
        }
        g_font_glyphs[glyph].codepoint = (uint32_t)(uint8_t)kGlyphChars[glyph];
        g_font_glyphs[glyph].source.x = (int16_t)x;
        g_font_glyphs[glyph].source.y = 0;
        g_font_glyphs[glyph].source.width = SAT_ASCII_FONT_GLYPH_WIDTH;
        g_font_glyphs[glyph].source.height = SAT_ASCII_FONT_GLYPH_HEIGHT;
        g_font_glyphs[glyph].bearing_x = 0;
        g_font_glyphs[glyph].bearing_y = 0;
        g_font_glyphs[glyph].advance_x = SAT_ASCII_FONT_GLYPH_WIDTH;
        g_font_glyphs[glyph].reserved = 0u;
    }
}

int main(void) {
    const char* text = "HELLO WORLD!";
    const uint16_t backdrop_color = SAT_COLOR_BLUE;
    sat_surface_t font_surface;
    sat_texture_t font_atlas;
    sat_font_t font;
    sat_text_metrics_t metrics;
    sat_text_style_t style = sat_text_style_default();

    sat_example_must(sat_app_init_default());
    build_acceptance_font();
    sat_example_must(sat_surface_init(
        &font_surface,
        g_font_pixels,
        FONT_ATLAS_WIDTH,
        FONT_ATLAS_HEIGHT,
        FONT_ATLAS_WIDTH,
        SAT_PIXEL_INDEX8,
        g_font_palette,
        256u));
    sat_example_must(sat_texture_create_from_surface(
        &font_atlas, &font_surface, SAT_TEXTURE_PERSISTENT_SOURCE));
    sat_example_must(sat_font_init(
        &font, font_atlas, g_font_glyphs, FONT_GLYPH_COUNT,
        SAT_ASCII_FONT_GLYPH_HEIGHT, 4u));
    sat_example_must(sat_font_prepare(&font));
    sat_example_must(sat_text_measure(&font, text, &style, &metrics));

    while (1) {
        sat_pad_state_t pad = {0};
        sat_example_must(sat_app_frame_begin(backdrop_color, backdrop_color, &pad));
        sat_example_must(sat_text_draw(
            &font,
            text,
            (int16_t)((320 - metrics.width) / 2),
            104,
            &style));
        sat_example_must(sat_app_frame_end());
        if ((pad.pressed & SAT_PAD_START) != 0u) break;
    }

    sat_example_must(sat_texture_destroy(font_atlas));
    return 0;
}
