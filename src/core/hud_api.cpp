#include "saturn/hud.h"
#include "saturn/vdp1.h"

extern "C" sat_result_t sat_hud_init(
    sat_hud_t* hud, const sat_ascii_font_t* font,
    uint16_t palette, uint16_t char_spacing) {
    if (!hud || !font || char_spacing == 0u) return SAT_ERR_INVALID_ARG;
    hud->font = font;
    hud->palette = palette;
    hud->char_spacing = char_spacing;
    hud->flags = 0u;
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
