#ifndef SATURN_FONT_H
#define SATURN_FONT_H

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/vdp1.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Bitmap font packing helper                                          */
/* ------------------------------------------------------------------ */
/* ASCII 8x8 font helpers                                              */
#define SAT_ASCII_FONT_GLYPH_COUNT 96u
#define SAT_ASCII_FONT_GLYPH_WIDTH 8u
#define SAT_ASCII_FONT_GLYPH_HEIGHT 8u

typedef struct sat_ascii_font {
    sat_texture_t glyphs[SAT_ASCII_FONT_GLYPH_COUNT];
} sat_ascii_font_t;

sat_result_t sat_ascii_font_init_8x8_indexed8(
    sat_ascii_font_t* out_font,
    uint16_t fg_rgb555,
    uint16_t bg_rgb555,
    uint16_t palette_index
);

int sat_ascii_font_measure_text_indexed8(const char* text, int char_spacing);

sat_result_t sat_ascii_font_draw_text_indexed8(
    const sat_ascii_font_t* font,
    const char* text,
    int x,
    int y,
    int char_spacing,
    uint16_t palette_override,
    uint16_t flags
);

sat_result_t sat_ascii_font_draw_text_centered_indexed8(
    const sat_ascii_font_t* font,
    const char* text,
    int center_x,
    int y,
    int char_spacing,
    uint16_t palette_override,
    uint16_t flags
);

const uint8_t* sat_font_ascii_8x8_rows(char c);

sat_result_t sat_font_pack_8x8_glyph_indexed8(
    uint8_t* pixels,
    uint16_t width,
    uint16_t height,
    uint16_t dst_x,
    uint16_t dst_y,
    const uint8_t* glyph_rows,
    uint8_t scale
);

sat_result_t sat_font_draw_text_line_indexed8(
    const sat_texture_t* glyph_textures,
    const char* glyph_chars,
    uint16_t glyph_count,
    const char* text,
    int x,
    int y,
    int char_spacing,
    uint16_t palette_override,
    uint16_t flags
);

sat_result_t sat_font_draw_text_ascii_indexed8(
    const sat_texture_t* ascii_textures,
    const char* text,
    int x,
    int y,
    int char_spacing,
    uint16_t palette_override,
    uint16_t flags
);

sat_result_t sat_font_upload_ascii_8x8_textures_indexed8(
    sat_texture_t* out_textures,
    uint8_t* glyph_pixels,
    uint16_t glyph_count,
    const uint16_t* palette_rgb555,
    uint16_t palette_index
);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_FONT_H */
