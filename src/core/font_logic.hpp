#ifndef SATURN_CORE_FONT_LOGIC_HPP
#define SATURN_CORE_FONT_LOGIC_HPP

#include <stdint.h>

#include "saturn/font.h"

namespace saturn::core {

inline sat_result_t pack_8x8_glyph_indexed8_impl(
    uint8_t* pixels,
    uint16_t width,
    uint16_t height,
    uint16_t dst_x,
    uint16_t dst_y,
    const uint8_t* glyph_rows,
    uint8_t scale
) {
    if (pixels == nullptr || glyph_rows == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (width == 0u || height == 0u || scale == 0u) {
        return SAT_ERR_INVALID_ARG;
    }

    const uint32_t glyph_w = static_cast<uint32_t>(8u) * static_cast<uint32_t>(scale);
    const uint32_t glyph_h = static_cast<uint32_t>(8u) * static_cast<uint32_t>(scale);
    if ((static_cast<uint32_t>(dst_x) + glyph_w) > static_cast<uint32_t>(width)) {
        return SAT_ERR_CAPACITY;
    }
    if ((static_cast<uint32_t>(dst_y) + glyph_h) > static_cast<uint32_t>(height)) {
        return SAT_ERR_CAPACITY;
    }

    for (uint32_t row = 0; row < 8u; ++row) {
        const uint8_t bits = glyph_rows[row];
        for (uint32_t col = 0; col < 8u; ++col) {
            if (((bits >> (7u - col)) & 1u) == 0u) {
                continue;
            }

            const uint32_t base_x = static_cast<uint32_t>(dst_x) + (col * static_cast<uint32_t>(scale));
            const uint32_t base_y = static_cast<uint32_t>(dst_y) + (row * static_cast<uint32_t>(scale));
            for (uint32_t sy = 0; sy < static_cast<uint32_t>(scale); ++sy) {
                const uint32_t dst_row = (base_y + sy) * static_cast<uint32_t>(width);
                for (uint32_t sx = 0; sx < static_cast<uint32_t>(scale); ++sx) {
                    pixels[dst_row + base_x + sx] = 1u;
                }
            }
        }
    }

    return SAT_OK;
}

}  // namespace saturn::core

#endif /* SATURN_CORE_FONT_LOGIC_HPP */
