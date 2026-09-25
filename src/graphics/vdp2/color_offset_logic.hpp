#ifndef SATURN_CORE_VDP2_COLOR_OFFSET_LOGIC_HPP
#define SATURN_CORE_VDP2_COLOR_OFFSET_LOGIC_HPP

#include <stdint.h>

#include "saturn/vdp2_color_offset.h"

namespace saturn::core::vdp2_color_offset {

/* CLOFEN / CLOFSL use bits 0..6 in SAT_VDP2_LAYER_* order (VDP2 manual 13.1). */
constexpr uint8_t kLayerMask = SAT_VDP2_LAYER_ALL;

inline bool valid_channel(int16_t value) {
    return value >= -256 && value <= 255;
}

inline sat_result_t validate_offset(const sat_vdp2_color_offset_t* offset) {
    if (offset == nullptr) return SAT_ERR_INVALID_ARG;
    if (!valid_channel(offset->r) || !valid_channel(offset->g) ||
        !valid_channel(offset->b)) {
        return SAT_ERR_INVALID_ARG;
    }
    return SAT_OK;
}

/* COxR/G/B: 9-bit two's complement, bit 8 the sign. */
inline uint16_t encode_channel(int16_t value) {
    return static_cast<uint16_t>(static_cast<uint16_t>(value) & 0x01FFu);
}

struct Layers {
    uint16_t clofen;
    uint16_t clofsl;
};

inline sat_result_t enable_layers(Layers& layers, uint8_t mask, uint8_t bank) {
    if (mask == 0u || (mask & static_cast<uint8_t>(~kLayerMask)) != 0u ||
        bank > SAT_VDP2_COLOR_OFFSET_B) {
        return SAT_ERR_INVALID_ARG;
    }
    layers.clofen = static_cast<uint16_t>(layers.clofen | mask);
    layers.clofsl = bank == SAT_VDP2_COLOR_OFFSET_B
        ? static_cast<uint16_t>(layers.clofsl | mask)
        : static_cast<uint16_t>(layers.clofsl & static_cast<uint16_t>(~mask));
    return SAT_OK;
}

inline sat_result_t disable_layers(Layers& layers, uint8_t mask) {
    if ((mask & static_cast<uint8_t>(~kLayerMask)) != 0u) return SAT_ERR_INVALID_ARG;
    layers.clofen = static_cast<uint16_t>(layers.clofen & static_cast<uint16_t>(~mask));
    return SAT_OK;
}

/* What the VDP2 outputs for one channel (manual figure 13.1): used by the host
 * tests and to document the clamp. */
inline uint8_t apply_channel(uint8_t colour, int16_t offset) {
    const int32_t sum = static_cast<int32_t>(colour) + offset;
    return static_cast<uint8_t>(sum < 0 ? 0 : (sum > 255 ? 255 : sum));
}

}  // namespace saturn::core::vdp2_color_offset

#endif /* SATURN_CORE_VDP2_COLOR_OFFSET_LOGIC_HPP */
