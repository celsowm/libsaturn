/* test_font_logic.cpp — host tests for bitmap font packing */

#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "saturn/font.h"
#include "src/core/font_logic.hpp"

#define TEST(name) static void name()
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    fprintf(stderr, "FAIL %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
    exit(1); } } while(0)

extern "C" sat_result_t sat_tex_upload_indexed8(
    sat_texture_t*,
    const uint8_t*,
    uint16_t,
    uint16_t,
    const uint16_t*,
    uint16_t
) {
    return SAT_OK;
}

extern "C" sat_result_t sat_draw_sprite(const sat_sprite_cmd_t*) {
    return SAT_OK;
}

TEST(pack_glyph_basic) {
    uint8_t pixels[8 * 8] = {};
    const uint8_t glyph[8] = {
        0x80u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u
    };

    ASSERT_EQ(saturn::core::pack_8x8_glyph_indexed8_impl(pixels, 8, 8, 0, 0, glyph, 1), SAT_OK);
    ASSERT_EQ(pixels[0], 1u);
    ASSERT_EQ(pixels[1], 0u);
    ASSERT_EQ(pixels[8], 0u);
}

TEST(ascii_rows_lookup_supported) {
    const uint8_t* rows = sat_font_ascii_8x8_rows('A');
    ASSERT_EQ(rows[0], 0x38u);
    ASSERT_EQ(rows[7], 0x00u);
}

TEST(ascii_rows_lookup_fallback) {
    const uint8_t* rows = sat_font_ascii_8x8_rows('\x01');
    ASSERT_EQ(rows[0], 0x00u);
    ASSERT_EQ(rows[7], 0x00u);
}

TEST(measure_ascii_text_empty) {
    ASSERT_EQ(saturn::core::measure_ascii_text_indexed8_impl("", 10), 0);
}

TEST(measure_ascii_text_single_char) {
    ASSERT_EQ(saturn::core::measure_ascii_text_indexed8_impl("A", 10), 8);
}

TEST(measure_ascii_text_multiple_chars) {
    ASSERT_EQ(saturn::core::measure_ascii_text_indexed8_impl("HELLO", 10), 48);
}

TEST(measure_ascii_text_with_overlap) {
    ASSERT_EQ(saturn::core::measure_ascii_text_indexed8_impl("AB", 6), 14);
}

TEST(pack_glyph_scale_2) {
    uint8_t pixels[16 * 16] = {};
    const uint8_t glyph[8] = {
        0x80u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u
    };

    ASSERT_EQ(saturn::core::pack_8x8_glyph_indexed8_impl(pixels, 16, 16, 0, 0, glyph, 2), SAT_OK);
    ASSERT_EQ(pixels[0], 1u);
    ASSERT_EQ(pixels[1], 1u);
    ASSERT_EQ(pixels[16], 1u);
    ASSERT_EQ(pixels[17], 1u);
    ASSERT_EQ(pixels[2], 0u);
    ASSERT_EQ(pixels[32], 0u);
}

TEST(pack_glyph_multi_layout) {
    uint8_t pixels[16 * 8] = {};
    const uint8_t glyph_a[8] = {
        0x80u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u
    };
    const uint8_t glyph_b[8] = {
        0x01u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u
    };

    ASSERT_EQ(saturn::core::pack_8x8_glyph_indexed8_impl(pixels, 16, 8, 0, 0, glyph_a, 1), SAT_OK);
    ASSERT_EQ(saturn::core::pack_8x8_glyph_indexed8_impl(pixels, 16, 8, 8, 0, glyph_b, 1), SAT_OK);
    ASSERT_EQ(pixels[0], 1u);
    ASSERT_EQ(pixels[15], 1u);
    ASSERT_EQ(pixels[1], 0u);
    ASSERT_EQ(pixels[8], 0u);
}

TEST(pack_glyph_invalid_args) {
    uint8_t pixels[8 * 8] = {};
    const uint8_t glyph[8] = {};

    ASSERT_EQ(saturn::core::pack_8x8_glyph_indexed8_impl(nullptr, 8, 8, 0, 0, glyph, 1), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(saturn::core::pack_8x8_glyph_indexed8_impl(pixels, 8, 8, 0, 0, nullptr, 1), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(saturn::core::pack_8x8_glyph_indexed8_impl(pixels, 8, 8, 0, 0, glyph, 0), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(saturn::core::pack_8x8_glyph_indexed8_impl(pixels, 8, 8, 1, 0, glyph, 1), SAT_ERR_CAPACITY);
}

int main() {
    pack_glyph_basic();
    ascii_rows_lookup_supported();
    ascii_rows_lookup_fallback();
    measure_ascii_text_empty();
    measure_ascii_text_single_char();
    measure_ascii_text_multiple_chars();
    measure_ascii_text_with_overlap();
    pack_glyph_scale_2();
    pack_glyph_multi_layout();
    pack_glyph_invalid_args();

    printf("PASS: test_font_logic.cpp (%d tests)\n", 10);
    return 0;
}
