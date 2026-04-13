#include "saturn/font.h"

#include "src/core/font_logic.hpp"

extern "C" sat_result_t sat_font_pack_8x8_glyph_indexed8(
    uint8_t* pixels,
    uint16_t width,
    uint16_t height,
    uint16_t dst_x,
    uint16_t dst_y,
    const uint8_t* glyph_rows,
    uint8_t scale
) {
    return saturn::core::pack_8x8_glyph_indexed8_impl(
        pixels,
        width,
        height,
        dst_x,
        dst_y,
        glyph_rows,
        scale
    );
}
