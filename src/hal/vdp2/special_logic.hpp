/* VDP2 special priority and special colour calculation policy (manual
 * chapters 11 and 12), kept pure so the host tests can cover every accepted
 * and refused combination without a Saturn.
 *
 * Only the four NBG screens are exposed through sat_vdp2_layer_t; RBG0's
 * SFPRMD/SFCCMD bits stay 0. The functions here take the layer's name size,
 * bitmap flag and colour format because the legal modes depend on them:
 *
 *   per-character / per-dot  need the PR/CC bits in a 2-word pattern name.
 *                            The 1-word PNCN auxiliary bits and the bitmap
 *                            BMPNA bits are not exposed by the library, so
 *                            those combinations are refused instead of
 *                            silently doing nothing.
 *   per-dot                  is prohibited for RGB colour formats (p11_20,
 *                            p12_20) and reads the special function code.
 *   colour-data MSB          is legal for palette and RGB; in RGB mode the
 *                            manual makes it "always on". */
#pragma once

#include <stdint.h>

namespace saturn::hal::vdp2::special {

inline constexpr uint8_t kLayerCount = 4;   /* NBG0..NBG3 */

enum class PriorityMode : uint8_t {
    PerScreen = 0,
    PerCharacter = 1,
    PerDot = 2,
};

enum class ColorMode : uint8_t {
    PerScreen = 0,
    PerCharacter = 1,
    PerDot = 2,
    ColorDataMSB = 3,
};

struct LayerTraits {
    bool two_word_names;
    bool bitmap;
    bool rgb;
};

inline bool priority_mode_supported(uint8_t mode, const LayerTraits& traits) {
    switch (mode) {
        case 0u: return true;
        case 1u: return traits.two_word_names && !traits.bitmap;
        case 2u: return traits.two_word_names && !traits.bitmap && !traits.rgb;
        default: return false;
    }
}

inline bool color_mode_supported(uint8_t mode, const LayerTraits& traits) {
    switch (mode) {
        case 0u: return true;
        case 1u: return traits.two_word_names && !traits.bitmap;
        case 2u: return traits.two_word_names && !traits.bitmap && !traits.rgb;
        case 3u: return true;
        default: return false;
    }
}

/* SFPRMD / SFCCMD: two bits per NBG screen, layer 0 in bits 1-0. */
inline uint16_t compose_modes(const uint8_t modes[kLayerCount]) {
    return static_cast<uint16_t>((modes[0] & 3u) | ((modes[1] & 3u) << 2u) |
                                 ((modes[2] & 3u) << 4u) | ((modes[3] & 3u) << 6u));
}

inline uint16_t compose_mode_bits(uint8_t layer, uint8_t mode) {
    return static_cast<uint16_t>((mode & 3u) << (2u * layer));
}

/* SFSEL: one bit per NBG screen; 0 selects code A, 1 selects code B. */
inline uint16_t compose_function_select(const uint8_t selects[kLayerCount]) {
    return static_cast<uint16_t>((selects[0] & 1u) | ((selects[1] & 1u) << 1u) |
                                 ((selects[2] & 1u) << 2u) | ((selects[3] & 1u) << 3u));
}

/* SFCODE: A in bits 7-0, B in bits 15-8. Bit n covers dot codes 2n and
 * 2n+1, i.e. the lower four bits shifted right by one (p10_30). */
inline uint16_t compose_function_code(uint8_t code_a, uint8_t code_b) {
    return static_cast<uint16_t>(code_a | (static_cast<uint16_t>(code_b) << 8u));
}

}  // namespace saturn::hal::vdp2::special
