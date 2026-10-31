/* VDP2 sprite shadow policy (manual 14.1), kept pure so the host tests can
 * cover the encodings and every accepted/refused configuration without a
 * Saturn. The public header repeats the dot-word formulas for callers; the
 * host test asserts the two copies agree. */
#pragma once

#include <stdint.h>

namespace saturn::hal::vdp2::sprite_shadow {

inline constexpr uint8_t kModeOff = 0u;
inline constexpr uint8_t kModeNormal = 1u;
inline constexpr uint8_t kModeMsb = 2u;

inline constexpr uint16_t kTargetMask = 0x003Fu;
inline constexpr uint16_t kTransparentShadow = 0x0100u;
inline constexpr uint16_t kType2Spctl = 0x0002u; /* palette only, no window */
inline constexpr uint16_t kNormalData = 0x07FEu;  /* 11-bit normal shadow */

enum class ConfigStatus : uint8_t {
    Ok,
    Off,
    InvalidArg,
    Unsupported,
};

inline bool valid_target_mask(uint8_t targets) {
    return (targets & ~kTargetMask) == 0u;
}

/* Bits 5-0 are N0SDEN..BKSDEN, bit 8 is TPSDSL (p16_18). */
inline uint16_t sdctl_word(uint8_t targets, bool transparent) {
    return static_cast<uint16_t>((targets & kTargetMask) |
                                 (transparent ? kTransparentShadow : 0u));
}

inline uint16_t prisa_word(uint8_t base_priority, uint8_t shadow_priority) {
    return static_cast<uint16_t>((base_priority & 7u) |
                                 ((shadow_priority & 7u) << 8u));
}

inline uint16_t normal_word(uint8_t priority_selector, uint8_t color_calc_selector) {
    return static_cast<uint16_t>(((priority_selector & 1u) << 14u) |
                                 ((color_calc_selector & 7u) << 11u) | kNormalData);
}

/* All 15 bits below the MSB must be zero for a transparent shadow. Its
 * priority is therefore S0PRIN, not a selector carried in bit 14. */
inline uint16_t transparent_msb_word() {
    return 0x8000u;
}

inline uint16_t msb_word(uint16_t sprite_dot) {
    return static_cast<uint16_t>(sprite_dot | 0x8000u);
}

/* Mode 0 is a valid request that disables the feature. Modes 1/2 require a
 * 16-bit palette sprite layer: hi-res and the 8-bit rotation framebuffer are
 * type C and cannot carry the type-2 shadow words. */
inline ConfigStatus validate_config(uint8_t mode, uint8_t targets, uint8_t transparent,
                                    uint8_t base_priority, uint8_t shadow_priority,
                                    bool hires, bool sprite_8bit) {
    if (mode > kModeMsb || transparent > 1u || !valid_target_mask(targets) ||
        base_priority > 7u || shadow_priority > 7u) {
        return ConfigStatus::InvalidArg;
    }
    if (mode == kModeOff) return ConfigStatus::Off;
    if (hires || sprite_8bit) return ConfigStatus::Unsupported;
    return ConfigStatus::Ok;
}

}  // namespace saturn::hal::vdp2::sprite_shadow
