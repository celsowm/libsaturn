#ifndef SATURN_CORE_FONT_TEXT_LOGIC_HPP
#define SATURN_CORE_FONT_TEXT_LOGIC_HPP

#include <stdint.h>

#include "saturn/font.h"

namespace saturn::core {

struct Utf8Decode {
    uint32_t codepoint;
    uint8_t length;
};

inline sat_result_t decode_utf8(const char* text, Utf8Decode* out) {
    if (text == nullptr || out == nullptr || *text == '\0') return SAT_ERR_INVALID_ARG;
    const uint8_t b0 = static_cast<uint8_t>(text[0]);
    if (b0 < 0x80u) {
        out->codepoint = b0;
        out->length = 1u;
        return SAT_OK;
    }

    uint8_t length = 0u;
    uint32_t codepoint = 0u;
    uint32_t minimum = 0u;
    if (b0 >= 0xC2u && b0 <= 0xDFu) {
        length = 2u;
        codepoint = b0 & 0x1Fu;
        minimum = 0x80u;
    } else if (b0 >= 0xE0u && b0 <= 0xEFu) {
        length = 3u;
        codepoint = b0 & 0x0Fu;
        minimum = 0x800u;
    } else if (b0 >= 0xF0u && b0 <= 0xF4u) {
        length = 4u;
        codepoint = b0 & 0x07u;
        minimum = 0x10000u;
    } else {
        return SAT_ERR_INVALID_ARG;
    }

    for (uint8_t i = 1u; i < length; ++i) {
        const uint8_t byte = static_cast<uint8_t>(text[i]);
        if ((byte & 0xC0u) != 0x80u) return SAT_ERR_INVALID_ARG;
        codepoint = (codepoint << 6u) | (byte & 0x3Fu);
    }
    if (codepoint < minimum || codepoint > 0x10FFFFu ||
        (codepoint >= 0xD800u && codepoint <= 0xDFFFu)) {
        return SAT_ERR_INVALID_ARG;
    }
    out->codepoint = codepoint;
    out->length = length;
    return SAT_OK;
}

inline sat_result_t validate_font(const sat_font_t* font) {
    if (font == nullptr || font->glyphs == nullptr || font->glyph_count == 0u ||
        font->line_height == 0u || font->atlas.slot == 0xFFFFu ||
        font->atlas.generation == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    if (font->fallback_glyph != SAT_FONT_NO_FALLBACK &&
        font->fallback_glyph >= font->glyph_count) {
        return SAT_ERR_INVALID_ARG;
    }
    return SAT_OK;
}

inline sat_result_t find_glyph_impl(
    const sat_font_t* font,
    uint32_t codepoint,
    uint16_t* out_glyph_index
) {
    sat_result_t status = validate_font(font);
    if (status != SAT_OK || out_glyph_index == nullptr) return SAT_ERR_INVALID_ARG;
    for (uint16_t i = 0u; i < font->glyph_count; ++i) {
        if (font->glyphs[i].codepoint == codepoint) {
            *out_glyph_index = i;
            return SAT_OK;
        }
    }
    if (font->fallback_glyph != SAT_FONT_NO_FALLBACK) {
        *out_glyph_index = font->fallback_glyph;
        return SAT_OK;
    }
    return SAT_ERR_NOT_FOUND;
}

inline int32_t scale_value(int32_t value, sat_fx16_t scale) {
    const int64_t scaled = static_cast<int64_t>(value) * scale;
    return static_cast<int32_t>(scaled >= 0 ?
        (scaled + 0x8000) / 0x10000 : (scaled - 0x8000) / 0x10000);
}

inline sat_result_t measure_text_impl(
    const sat_font_t* font,
    const char* text,
    sat_fx16_t scale,
    sat_text_metrics_t* out_metrics
) {
    sat_result_t status = validate_font(font);
    if (status != SAT_OK || text == nullptr || out_metrics == nullptr || scale <= 0) {
        return SAT_ERR_INVALID_ARG;
    }
    out_metrics->width = 0;
    out_metrics->height = scale_value(font->line_height, scale);
    out_metrics->line_count = 1u;
    out_metrics->reserved = 0u;
    int32_t line_width = 0;

    for (const char* p = text; *p != '\0';) {
        if (*p == '\n') {
            if (line_width > out_metrics->width) out_metrics->width = line_width;
            line_width = 0;
            if (out_metrics->line_count == 0xFFFFu) return SAT_ERR_CAPACITY;
            ++out_metrics->line_count;
            p += 1;
            continue;
        }
        if (*p == '\r') {
            p += 1;
            continue;
        }
        Utf8Decode decoded{};
        status = decode_utf8(p, &decoded);
        if (status != SAT_OK) return status;
        uint16_t glyph_index = 0u;
        status = find_glyph_impl(font, decoded.codepoint, &glyph_index);
        if (status != SAT_OK) return status;
        line_width += scale_value(font->glyphs[glyph_index].advance_x, scale);
        if (line_width < 0) return SAT_ERR_INVALID_ARG;
        p += decoded.length;
    }
    if (line_width > out_metrics->width) out_metrics->width = line_width;
    out_metrics->height = scale_value(
        static_cast<int32_t>(font->line_height) * out_metrics->line_count, scale);
    return SAT_OK;
}

}  // namespace saturn::core

#endif /* SATURN_CORE_FONT_TEXT_LOGIC_HPP */
