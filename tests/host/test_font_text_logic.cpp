#include <cstdio>
#include <cstdlib>

#include "saturn/font.h"
#include "src/graphics/2d/font/text_logic.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

static sat_font_t make_font(const sat_font_glyph_t* glyphs, uint16_t count, uint16_t fallback) {
    sat_font_t font{};
    font.atlas.slot = 0u;
    font.atlas.generation = 1u;
    font.glyphs = glyphs;
    font.glyph_count = count;
    font.line_height = 10u;
    font.fallback_glyph = fallback;
    return font;
}

int main() {
    const sat_font_glyph_t glyphs[] = {
        {static_cast<uint32_t>('A'), {0, 0, 5, 7}, 0, 1, 6, 0},
        {static_cast<uint32_t>('B'), {5, 0, 4, 7}, 1, 1, 5, 0},
        {0xFFFDu, {9, 0, 3, 7}, 0, 1, 4, 0}
    };
    const sat_font_t font = make_font(glyphs, 3u, 2u);
    uint16_t index = 0u;
    OK(saturn::core::find_glyph_impl(&font, 'B', &index) == SAT_OK && index == 1u);
    OK(saturn::core::find_glyph_impl(&font, 0x03A9u, &index) == SAT_OK && index == 2u);

    sat_text_metrics_t metrics{};
    OK(saturn::core::measure_text_impl(&font, "AB\nA", SAT_FX16_ONE, &metrics) == SAT_OK);
    OK(metrics.width == 11 && metrics.height == 20 && metrics.line_count == 2u);

    OK(saturn::core::measure_text_impl(&font, "A\xC2\xA2", SAT_FX16_ONE, &metrics) == SAT_OK);
    OK(metrics.width == 10); /* A (6) + fallback glyph (4) */
    OK(saturn::core::measure_text_impl(&font, "A\xC0\x80", SAT_FX16_ONE, &metrics) == SAT_ERR_INVALID_ARG);

    sat_text_style_t half = sat_text_style_default();
    half.scale = SAT_FX16_ONE / 2;
    OK(saturn::core::measure_text_impl(&font, "AB", half.scale, &metrics) == SAT_OK);
    OK(metrics.width == 6 && metrics.height == 5);

    sat_font_t no_fallback = make_font(glyphs, 2u, SAT_FONT_NO_FALLBACK);
    OK(saturn::core::measure_text_impl(&no_fallback, "A?", SAT_FX16_ONE, &metrics) == SAT_ERR_NOT_FOUND);
    std::puts("font text logic: OK");
    return 0;
}
