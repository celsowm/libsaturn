#ifndef SATURN_CORE_VDP2_COLOR_CALC_LOGIC_HPP
#define SATURN_CORE_VDP2_COLOR_CALC_LOGIC_HPP

#include <stdint.h>

#include "saturn/vdp2_color_calc.h"

namespace saturn::core::vdp2_color_calc {

constexpr uint16_t kCcctlSpriteEnable = 0x0040u;
constexpr uint16_t kSpctlConditionEqual = 0x1000u; /* SPCCCS = 01B */

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

/* Do not rewrite the ratio table per sprite: that would also change earlier
 * queued sprites. The alpha API only picks a near-enough preconfigured slot. */
inline sat_result_t choose_alpha_slot(
    uint8_t alpha, const uint8_t ratios[8], uint8_t* out_slot) {
    if (ratios == nullptr || out_slot == nullptr || alpha == 0u || alpha == 255u) {
        return SAT_ERR_INVALID_ARG;
    }
    const uint16_t raw_target = static_cast<uint16_t>((255u - alpha) * 32u / 255u);
    const uint8_t target = raw_target > 31u ? 31u : static_cast<uint8_t>(raw_target);
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
    if (best_distance > 2u) return SAT_ERR_UNSUPPORTED;
    *out_slot = best;
    return SAT_OK;
}

inline uint16_t compose_spctl(uint8_t color_calc_priority) {
    /* Type 0, palette-only mode, equality condition. */
    return static_cast<uint16_t>(
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
