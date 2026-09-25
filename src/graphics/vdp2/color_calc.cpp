#include "saturn/vdp2_color_calc.h"

#include "src/core/runtime/internal.hpp"
#include "src/core/runtime/state.hpp"
#include "src/graphics/vdp2/color_calc_logic.hpp"
#include "src/hal/vdp2/color_calc.hpp"

extern "C" sat_result_t sat_vdp2_sprite_color_calc_configure(
    const sat_vdp2_sprite_color_calc_config_t* config
) {
    sat_result_t st = saturn::core::require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    st = saturn::core::vdp2_color_calc::validate_config(config);
    if (st != SAT_OK) {
        return st;
    }
    saturn::hal::vdp2_color_calc::configure(*config);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_sprite_color_calc_set_ratio(
    uint8_t slot,
    uint8_t ratio
) {
    sat_result_t st = saturn::core::require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    if (slot >= 8u || ratio > 31u) {
        return SAT_ERR_INVALID_ARG;
    }
    saturn::hal::vdp2_color_calc::set_ratio(slot, ratio);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_sprite_color_calc_configure_alpha(uint8_t normal_priority) {
    sat_vdp2_sprite_color_calc_config_t config{};
    config.enabled = 1u;
    config.normal_priority = normal_priority;
    constexpr uint8_t ratios[8] = {0u, 4u, 8u, 12u, 16u, 20u, 24u, 31u};
    for (uint8_t i = 0u; i < 8u; ++i) config.ratio[i] = ratios[i];
    return sat_vdp2_sprite_color_calc_configure(&config);
}

extern "C" sat_result_t sat_vdp2_sprite_color_calc_alpha_slot(
    uint8_t alpha, uint8_t* out_slot, uint8_t* out_alpha) {
    SAT_TRY(saturn::core::require_initialized());
    if (out_slot == nullptr || alpha == 0u || alpha == 255u) return SAT_ERR_INVALID_ARG;
    return saturn::hal::vdp2_color_calc::select_alpha_slot(alpha, out_slot, out_alpha);
}

extern "C" sat_result_t sat_vdp2_sprite_color_calc_set_strict_alpha(uint8_t strict) {
    SAT_TRY(saturn::core::require_initialized());
    if (strict > 1u) return SAT_ERR_INVALID_ARG;
    saturn::hal::vdp2_color_calc::set_strict_alpha(strict != 0u);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_sprite_color_calc_claim_mode(
    sat_vdp2_color_calc_mode_t mode) {
    SAT_TRY(saturn::core::require_initialized());
    if (static_cast<uint32_t>(mode) > SAT_VDP2_COLOR_CALC_ADD) return SAT_ERR_INVALID_ARG;
    return saturn::hal::vdp2_color_calc::claim_mode(static_cast<uint8_t>(mode));
}

extern "C" sat_result_t sat_vdp2_sprite_color_calc_commit(void) {
    sat_result_t st = saturn::core::require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    saturn::hal::vdp2_color_calc::commit();
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_sprite_color_calc_disable(void) {
    sat_result_t st = saturn::core::require_initialized();
    if (st != SAT_OK) {
        return st;
    }
    saturn::hal::vdp2_color_calc::disable();
    return SAT_OK;
}
