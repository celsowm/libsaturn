#include "saturn/hud.h"
#include "saturn/video.h"
#include "saturn/vdp1.h"

/* The exact scale per font cell: in 320x224 the VDP1 framebuffer is
 * 8 bits/pixel and the ASCII glyphs sit 8 px wide; in 640x224 a single
 * pixel cell carries an 8-bit palette code and the same 8x8 glyph would
 * be half the visible width of a 320x224 cell, so every font caller
 * doubles char_spacing in hi-res. */
static const uint16_t kHudLineHeight = 10u;

extern "C" sat_result_t sat_hud_init(
    sat_hud_t* hud, const sat_ascii_font_t* font,
    uint16_t palette, uint16_t char_spacing) {
    if (!hud || !font) return SAT_ERR_INVALID_ARG;
    hud->font = font;
    hud->palette = palette;
    hud->char_spacing = char_spacing;
    hud->flags = 0u;
    hud->cursor_x = 4;
    hud->cursor_y = 4;
    hud->line_height = kHudLineHeight;
    return SAT_OK;
}

extern "C" sat_result_t sat_hud_init_default(sat_hud_t* hud,
                                            sat_ascii_font_t* font) {
    if (!hud || !font) return SAT_ERR_INVALID_ARG;
    /* Initialise the font with white-on-black palette code 1 on code 0.
     * The HUD overlay does not own the palette -- examples typically have
     * a palette for the HUD already; pass it through sat_hud_init if a
     * different palette binding is needed. */
    sat_result_t st = sat_ascii_font_init_8x8_indexed8(
        font, SAT_COLOR_WHITE, SAT_COLOR_BLACK, 0u);
    if (st != SAT_OK) return st;
    const uint16_t width = sat_video_width();
    /* Hi-res doubles the cell width so the glyph keeps the same on-screen
     * proportion as in 320x224. The line height stays 10 pixels because
     * vertical resolution is unchanged. */
    const uint16_t spacing = (width >= 640u) ? 2u : 1u;
    return sat_hud_init(hud, font, 0u, spacing);
}

extern "C" void sat_hud_reset_cursor(sat_hud_t* hud, int x, int y) {
    if (!hud) return;
    hud->cursor_x = (int16_t)x;
    hud->cursor_y = (int16_t)y;
}

extern "C" sat_result_t sat_hud_line(sat_hud_t* hud, const char* label,
                                     uint32_t value) {
    if (!hud || !label) return SAT_ERR_INVALID_ARG;
    sat_result_t st = sat_hud_value(hud, label, value,
                                     hud->cursor_x, hud->cursor_y);
    if (st != SAT_OK) return st;
    hud->cursor_y = (int16_t)(hud->cursor_y + hud->line_height);
    return SAT_OK;
}

extern "C" sat_result_t sat_hud_text(
    const sat_hud_t* hud, const char* text, int x, int y) {
    if (!hud || !hud->font || !text) return SAT_ERR_INVALID_ARG;
    return sat_ascii_font_draw_text_screen_indexed8(
        hud->font, text, x, y, hud->char_spacing, hud->palette, hud->flags);
}

extern "C" sat_result_t sat_hud_text_centered(
    const sat_hud_t* hud, const char* text, int center_x, int y) {
    if (!hud || !hud->font || !text) return SAT_ERR_INVALID_ARG;
    return sat_ascii_font_draw_text_screen_centered_indexed8(
        hud->font, text, center_x, y, hud->char_spacing,
        hud->palette, hud->flags);
}

extern "C" sat_result_t sat_hud_value(
    const sat_hud_t* hud, const char* label, uint32_t value, int x, int y) {
    if (!hud || !hud->font || !label) return SAT_ERR_INVALID_ARG;
    return sat_ascii_font_draw_label_u32(
        hud->font, label, value, x, y, hud->char_spacing,
        hud->palette, hud->flags);
}

extern "C" sat_result_t sat_hud_bar(
    const sat_hud_t* hud, int x, int y, int width, int height,
    uint32_t value, uint32_t maximum, uint16_t background,
    uint16_t foreground) {
    if (!hud || width <= 0 || height <= 0 || maximum == 0u || value > maximum)
        return SAT_ERR_INVALID_ARG;
    const int filled = static_cast<int>(
        (static_cast<uint64_t>(width) * value) / maximum);
    sat_result_t st = sat_draw_rect_screen(
        static_cast<int16_t>(x), static_cast<int16_t>(y),
        static_cast<uint16_t>(width), static_cast<uint16_t>(height), background);
    if (st != SAT_OK || filled == 0) return st;
    return sat_draw_rect_screen(
        static_cast<int16_t>(x), static_cast<int16_t>(y),
        static_cast<uint16_t>(filled), static_cast<uint16_t>(height), foreground);
}
