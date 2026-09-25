#ifndef SATURN_CORE_VDP2_COLOR_CALC_LOGIC_HPP
#define SATURN_CORE_VDP2_COLOR_CALC_LOGIC_HPP

#include <stdint.h>

#include "saturn/vdp2_color_calc.h"

namespace saturn::core::vdp2_color_calc {

constexpr uint16_t kCcctlSpriteEnable = 0x0040u;
constexpr uint16_t kSpctlConditionEqual = 0x1000u; /* SPCCCS = 01B */
/* CCCTL CCMD (bit 8): 1 adds the top and second images as they are, ignoring
 * every ratio register (VDP2 manual 12.1). One bit for the whole screen. */
constexpr uint16_t kCcctlAddAsIs = 0x0100u;

inline uint16_t compose_ccctl(bool enabled, uint8_t mode) {
    if (!enabled) return 0u;
    return static_cast<uint16_t>(
        kCcctlSpriteEnable | (mode == SAT_VDP2_COLOR_CALC_ADD ? kCcctlAddAsIs : 0u));
}

/* Frame-scoped claim on the screen-global mode. Every draw that selects the
 * colour-calculated sprite priority claims the mode it needs; the first claim
 * of a frame wins and a claim of the other mode is refused with BUSY, so a
 * frame can never show additive sprites with ratio fades silently turned
 * additive too (or the reverse). */
struct ModeClaim {
    uint8_t claimed;
    uint8_t mode;
};

inline sat_result_t claim_mode(ModeClaim& claim, uint8_t mode) {
    if (mode > SAT_VDP2_COLOR_CALC_ADD) return SAT_ERR_INVALID_ARG;
    if (claim.claimed != 0u && claim.mode != mode) return SAT_ERR_BUSY;
    claim.claimed = 1u;
    claim.mode = mode;
    return SAT_OK;
}

inline void claim_reset(ModeClaim& claim) {
    claim.claimed = 0u;
}

inline sat_result_t validate_config(const sat_vdp2_sprite_color_calc_config_t* config) {
    if (config == nullptr || config->enabled > 1u) {
        return SAT_ERR_INVALID_ARG;
    }
    if (config->normal_priority == 0u || config->normal_priority > 7u) {
        return SAT_ERR_INVALID_ARG;
    }
    if (config->enabled != 0u && config->normal_priority < 2u) {
        return SAT_ERR_INVALID_ARG;
    }
    for (uint8_t i = 0u; i < 8u; ++i) {
        if (config->ratio[i] > 31u) {
            return SAT_ERR_INVALID_ARG;
        }
    }
    return SAT_OK;
}

/* The hardware ratio r (0..31) mixes (31 - r)/32 of the top (sprite) image
 * with (r + 1)/32 of the second image (VDP2 manual 12.1, CCRSA-D table). */
inline uint8_t ratio_to_alpha(uint8_t ratio) {
    const uint8_t r = ratio > 31u ? 31u : ratio;
    return static_cast<uint8_t>((31u - r) * 8u);
}

inline uint8_t alpha_to_ratio(uint8_t alpha) {
    const uint16_t raw = static_cast<uint16_t>((255u - alpha) * 32u / 255u);
    return raw > 31u ? 31u : static_cast<uint8_t>(raw);
}

/* Do not rewrite the ratio table per sprite: that would also change earlier
 * queued sprites. The alpha API only picks a preconfigured slot: the nearest
 * one, reporting the alpha it really yields in *out_alpha (optional). With
 * strict set, a slot more than 2 hardware ratio units away is refused with
 * SAT_ERR_UNSUPPORTED instead of snapped. */
inline sat_result_t choose_alpha_slot(
    uint8_t alpha, const uint8_t ratios[8], bool strict,
    uint8_t* out_slot, uint8_t* out_alpha) {
    if (ratios == nullptr || out_slot == nullptr || alpha == 0u || alpha == 255u) {
        return SAT_ERR_INVALID_ARG;
    }
    const uint8_t target = alpha_to_ratio(alpha);
    uint8_t best = 0u;
    uint8_t best_distance = 32u;
    for (uint8_t i = 0u; i < 8u; ++i) {
        if (ratios[i] > 31u) return SAT_ERR_INVALID_ARG;
        const uint8_t distance = ratios[i] > target ?
            static_cast<uint8_t>(ratios[i] - target) :
            static_cast<uint8_t>(target - ratios[i]);
        if (distance < best_distance) {
            best_distance = distance;
            best = i;
        }
    }
    if (strict && best_distance > 2u) return SAT_ERR_UNSUPPORTED;
    *out_slot = best;
    if (out_alpha != nullptr) *out_alpha = ratio_to_alpha(ratios[best]);
    return SAT_OK;
}

/* SPCTL SPCLMD (bit 5): palette and RGB sprite data mixed. Without it the
 * VDP2 reads an RGB pixel (MSB 1) as palette data and it vanishes, so every
 * RGB polygon disappeared the moment colour calculation was configured.
 * Must stay clear at 8 bits/dot (hi-res type C). */
constexpr uint16_t kSpctlMixedRgb = 0x0020u;

inline uint16_t compose_spctl(uint8_t color_calc_priority,
                              uint16_t sprite_format_bits = kSpctlMixedRgb) {
    /* Sprite type/colour mode from the caller, equality condition. */
    return static_cast<uint16_t>(
        sprite_format_bits |
        kSpctlConditionEqual |
        (static_cast<uint16_t>(color_calc_priority & 0x07u) << 8u));
}

inline uint16_t compose_prisa(uint8_t normal_priority) {
    const uint16_t normal = static_cast<uint16_t>(normal_priority & 0x07u);
    const uint16_t faded = static_cast<uint16_t>((normal_priority - 1u) & 0x07u);
    /* Selector 0 -> normal; selector 1 -> faded/color-calculated. */
    return static_cast<uint16_t>(normal | (faded << 8u));
}

inline uint16_t compose_prisa_disabled(uint8_t normal_priority) {
    const uint16_t p = static_cast<uint16_t>(normal_priority & 0x07u);
    return static_cast<uint16_t>(p | (p << 8u));
}

inline uint16_t compose_ratio_pair(uint8_t even_slot, uint8_t odd_slot) {
    return static_cast<uint16_t>(
        static_cast<uint16_t>(even_slot & 0x1Fu) |
        (static_cast<uint16_t>(odd_slot & 0x1Fu) << 8u));
}

}  // namespace saturn::core::vdp2_color_calc

#endif /* SATURN_CORE_VDP2_COLOR_CALC_LOGIC_HPP */
