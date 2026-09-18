#ifndef SATURN_FONT_H
#define SATURN_FONT_H

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/color.h"
#include "saturn/geometry2d.h"
#include "saturn/texture.h"
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
    sat_vdp1_texture_t glyphs[SAT_ASCII_FONT_GLYPH_COUNT];
} sat_ascii_font_t;

sat_result_t sat_ascii_font_init_8x8_indexed8(
    sat_ascii_font_t* out_font,
    uint16_t fg_rgb555,
    uint16_t bg_rgb555,
    uint16_t palette_index
);

sat_result_t sat_ascii_font_init_scaled_indexed8(
    sat_ascii_font_t* out_font,
    uint16_t fg_rgb555,
    uint16_t bg_rgb555,
    uint16_t palette_index,
    uint8_t scale
);

/* NOTE on char_spacing throughout this header: it is the per-glyph ADVANCE in
 * pixels, not a gap added between glyphs. A value of 8 places 8x8 glyphs edge
 * to edge; 10 leaves a 2-pixel gap. A non-positive value is treated as one
 * glyph width, because advancing by zero would draw every glyph of the string
 * on the same pixel -- a solid block that looks like a texture bug rather than
 * a spacing mistake.
 */
int sat_ascii_font_measure_text_indexed8(const char* text, int char_spacing);
int sat_ascii_font_measure_text_scaled_indexed8(const char* text, int char_spacing, uint8_t scale);

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

/* As sat_ascii_font_draw_text_indexed8, but in SCREEN coordinates
 * ((0,0) = top-left of the screen) instead of native VDP1 coordinates
 * ((0,0) = screen centre). Prefer these for HUD text: passing screen
 * coordinates to the native entry points silently draws in the wrong place,
 * or off-screen entirely.
 */
sat_result_t sat_ascii_font_draw_text_screen_indexed8(
    const sat_ascii_font_t* font,
    const char* text,
    int screen_x,
    int screen_y,
    int char_spacing,
    uint16_t palette_override,
    uint16_t flags
);

/* Horizontally centred on `screen_center_x`, in screen coordinates. */
sat_result_t sat_ascii_font_draw_text_screen_centered_indexed8(
    const sat_ascii_font_t* font,
    const char* text,
    int screen_center_x,
    int screen_y,
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
    const sat_vdp1_texture_t* glyph_textures,
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
    const sat_vdp1_texture_t* ascii_textures,
    const char* text,
    int x,
    int y,
    int char_spacing,
    uint16_t palette_override,
    uint16_t flags
);

sat_result_t sat_font_upload_ascii_8x8_textures_indexed8(
    sat_vdp1_texture_t* out_textures,
    uint8_t* glyph_pixels,
    uint16_t glyph_count,
    const uint16_t* palette_rgb555,
    uint16_t palette_index
);

/* ------------------------------------------------------------------ */
/* Baked atlas font runtime                                            */
/* ------------------------------------------------------------------ */
/*
 * A high-level font is a caller-owned view over a logical texture and a
 * baked glyph table.  The runtime never owns or grows the glyph table.  The
 * source rectangles are atlas pixels; bearing/advance values are logical
 * pixels at scale SAT_FX16_ONE.
 *
 * The initial runtime text contract accepts UTF-8 and maps each decoded
 * Unicode scalar to one glyph entry.  Invalid UTF-8 is rejected.  A missing
 * scalar uses fallback_glyph when it is not SAT_FONT_NO_FALLBACK; otherwise
 * the operation returns SAT_ERR_NOT_FOUND.  Newline is supported and carriage
 * return is ignored.  Other control characters are not font data.
 */
#define SAT_FONT_NO_FALLBACK ((uint16_t)0xFFFFu)

typedef struct sat_font_glyph {
    uint32_t codepoint;
    sat_rect_t source;
    int16_t bearing_x;
    int16_t bearing_y;
    int16_t advance_x;
    uint16_t reserved;
} sat_font_glyph_t;

typedef struct sat_font {
    sat_texture_t atlas;
    const sat_font_glyph_t* glyphs;
    uint16_t glyph_count;
    uint16_t line_height;
    uint16_t fallback_glyph;
    uint16_t reserved;
} sat_font_t;

typedef struct sat_text_style {
    /* Positive 16.16 pixel scale. SAT_FX16_ONE is native size. */
    sat_fx16_t scale;
    sat_color_t tint;
    uint16_t blend_mode;
    uint16_t flags;
} sat_text_style_t;

typedef struct sat_text_metrics {
    int32_t width;
    int32_t height;
    uint16_t line_count;
    uint16_t reserved;
} sat_text_metrics_t;

static inline sat_text_style_t sat_text_style_default(void) {
    sat_text_style_t style;
    style.scale = SAT_FX16_ONE;
    style.tint.r = 255u;
    style.tint.g = 255u;
    style.tint.b = 255u;
    style.tint.a = 255u;
    style.blend_mode = 0u;
    style.flags = 0u;
    return style;
}

sat_result_t sat_font_init(
    sat_font_t* out_font,
    sat_texture_t atlas,
    const sat_font_glyph_t* glyphs,
    uint16_t glyph_count,
    uint16_t line_height,
    uint16_t fallback_glyph
);

/* Explicitly materializes all non-empty glyph regions in the texture cache. */
sat_result_t sat_font_prepare(const sat_font_t* font);
sat_result_t sat_font_find_glyph(
    const sat_font_t* font,
    uint32_t codepoint,
    uint16_t* out_glyph_index
);
sat_result_t sat_text_measure(
    const sat_font_t* font,
    const char* text,
    const sat_text_style_t* style,
    sat_text_metrics_t* out_metrics
);
sat_result_t sat_text_draw(
    const sat_font_t* font,
    const char* text,
    int16_t x,
    int16_t y,
    const sat_text_style_t* style
);
sat_result_t sat_text_draw_ex(
    const sat_font_t* font,
    const char* text,
    int16_t x,
    int16_t y,
    const sat_text_style_t* style,
    int16_t line_spacing
);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_FONT_H */
