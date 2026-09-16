#include "saturn/vdp2_color_calc.h"

#include "src/core/internal.hpp"
#include "src/core/vdp2_color_calc_logic.hpp"
#include "src/hal/vdp2_color_calc.hpp"

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
