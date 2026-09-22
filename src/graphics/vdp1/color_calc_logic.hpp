#ifndef SATURN_CORE_VDP1_COLOR_CALC_LOGIC_HPP
#define SATURN_CORE_VDP1_COLOR_CALC_LOGIC_HPP

#include <stdint.h>

#include "saturn/core.h"

namespace saturn::core::vdp1_color_calc {

/* LibSaturn's indexed8 textures are emitted through VDP1 color mode 100B and
 * interpreted by VDP2 as 16-bit palette Sprite Type 0:
 *
 *   bits 15..14 PR1..0
 *   bits 13..11 CC2..0
 *   bits 10..0  color-RAM address
 *
 * Existing vdp1.cpp builds CMDCOLR by shifting its palette selector left by
 * eight. Therefore this helper returns the corresponding packed high byte:
 * priority selector 1 (01B), CC slot, and the three-bit 256-color CRAM bank.
 * The packing stays internal; public callers only provide bank + slot. */
inline sat_result_t encode_palette_selector(
    uint16_t palette_bank,
    uint8_t color_calc_slot,
    uint16_t* out_selector
) {
    if (out_selector == nullptr || palette_bank > 7u || color_calc_slot > 7u) {
        return SAT_ERR_INVALID_ARG;
    }
    *out_selector = static_cast<uint16_t>(
        0x0040u |
        (static_cast<uint16_t>(color_calc_slot) << 3u) |
        palette_bank);
    return SAT_OK;
}

}  // namespace saturn::core::vdp1_color_calc

#endif /* SATURN_CORE_VDP1_COLOR_CALC_LOGIC_HPP */
