#include "saturn/font.h"

#include "saturn/render2d.h"
#include "src/graphics/2d/font/text_logic.hpp"

namespace {

sat_result_t text_style_scale(const sat_text_style_t* style, sat_fx16_t* out_scale) {
    if (out_scale == nullptr) return SAT_ERR_INVALID_ARG;
    const sat_fx16_t scale = style == nullptr ? SAT_FX16_ONE : style->scale;
    if (scale <= 0) return SAT_ERR_INVALID_ARG;
    *out_scale = scale;
    return SAT_OK;
}

sat_result_t text_draw_impl(
    const sat_font_t* font,
    const char* text,
    int16_t x,
    int16_t y,
    const sat_text_style_t* style,
    int16_t line_spacing
) {
    sat_result_t status = saturn::core::validate_font(font);
    if (status != SAT_OK || text == nullptr) return SAT_ERR_INVALID_ARG;
    sat_fx16_t scale = SAT_FX16_ONE;
    SAT_TRY(text_style_scale(style, &scale));
    const sat_color_t tint = style == nullptr ? sat_color_rgba(255u, 255u, 255u, 255u) : style->tint;
    const uint16_t blend = style == nullptr ? static_cast<uint16_t>(SAT_BLEND_NONE) : style->blend_mode;
    const uint16_t flags = style == nullptr ? 0u : style->flags;
    const int32_t scaled_line_height = saturn::core::scale_value(font->line_height, scale);
    const int32_t scaled_spacing = saturn::core::scale_value(line_spacing, scale);
    int32_t pen_x = x;
    int32_t pen_y = y;

    for (const char* p = text; *p != '\0';) {
        if (*p == '\n') {
            pen_x = x;
            pen_y += scaled_line_height + scaled_spacing;
            if (pen_y < -32768 || pen_y > 32767) return SAT_ERR_INVALID_ARG;
            p += 1;
            continue;
        }
        if (*p == '\r') {
            p += 1;
            continue;
        }
        saturn::core::Utf8Decode decoded{};
        SAT_TRY(saturn::core::decode_utf8(p, &decoded));
        uint16_t glyph_index = 0u;
        SAT_TRY(saturn::core::find_glyph_impl(font, decoded.codepoint, &glyph_index));
        const sat_font_glyph_t& glyph = font->glyphs[glyph_index];
        const int32_t draw_x = pen_x + saturn::core::scale_value(glyph.bearing_x, scale);
        const int32_t draw_y = pen_y + saturn::core::scale_value(glyph.bearing_y, scale);
        if (glyph.source.width != 0u && glyph.source.height != 0u) {
            const int32_t width = saturn::core::scale_value(glyph.source.width, scale);
            const int32_t height = saturn::core::scale_value(glyph.source.height, scale);
            if (width <= 0 || height <= 0 || width > 65535 || height > 65535 ||
                draw_x < -32768 || draw_x > 32767 || draw_y < -32768 || draw_y > 32767) {
                return SAT_ERR_INVALID_ARG;
            }
            const sat_rect_t dst = {
                static_cast<int16_t>(draw_x), static_cast<int16_t>(draw_y),
                static_cast<uint16_t>(width), static_cast<uint16_t>(height)
            };
            sat_draw_params_t params = sat_draw_params_default();
            params.tint = tint;
            params.blend_mode = blend;
            params.flags = flags;
            SAT_TRY(sat_draw_texture(font->atlas, &glyph.source, &dst, &params));
        }
        pen_x += saturn::core::scale_value(glyph.advance_x, scale);
        if (pen_x < -32768 || pen_x > 32767) return SAT_ERR_INVALID_ARG;
        p += decoded.length;
    }
    return SAT_OK;
}

}  // namespace

extern "C" sat_result_t sat_font_init(
    sat_font_t* out_font,
    sat_texture_t atlas,
    const sat_font_glyph_t* glyphs,
    uint16_t glyph_count,
    uint16_t line_height,
    uint16_t fallback_glyph
) {
    if (out_font == nullptr || glyphs == nullptr || glyph_count == 0u || line_height == 0u ||
        atlas.slot == 0xFFFFu || atlas.generation == 0u ||
        (fallback_glyph != SAT_FONT_NO_FALLBACK && fallback_glyph >= glyph_count)) {
        return SAT_ERR_INVALID_ARG;
    }
    *out_font = {atlas, glyphs, glyph_count, line_height, fallback_glyph, 0u};
    return SAT_OK;
}

extern "C" sat_result_t sat_font_prepare(const sat_font_t* font) {
    SAT_TRY(saturn::core::validate_font(font));
    for (uint16_t i = 0u; i < font->glyph_count; ++i) {
        if (font->glyphs[i].source.width == 0u || font->glyphs[i].source.height == 0u) continue;
        SAT_TRY(sat_texture_prepare_region(font->atlas, &font->glyphs[i].source));
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_font_find_glyph(
    const sat_font_t* font,
    uint32_t codepoint,
    uint16_t* out_glyph_index
) {
    return saturn::core::find_glyph_impl(font, codepoint, out_glyph_index);
}

extern "C" sat_result_t sat_text_measure(
    const sat_font_t* font,
    const char* text,
    const sat_text_style_t* style,
    sat_text_metrics_t* out_metrics
) {
    sat_fx16_t scale = SAT_FX16_ONE;
    SAT_TRY(text_style_scale(style, &scale));
    return saturn::core::measure_text_impl(font, text, scale, out_metrics);
}

extern "C" sat_result_t sat_text_draw(
    const sat_font_t* font,
    const char* text,
    int16_t x,
    int16_t y,
    const sat_text_style_t* style
) {
    return text_draw_impl(font, text, x, y, style, 0);
}

extern "C" sat_result_t sat_text_draw_ex(
    const sat_font_t* font,
    const char* text,
    int16_t x,
    int16_t y,
    const sat_text_style_t* style,
    int16_t line_spacing
) {
    return text_draw_impl(font, text, x, y, style, line_spacing);
}
