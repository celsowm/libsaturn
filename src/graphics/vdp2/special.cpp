#include "saturn/vdp2_special.h"

#include "src/core/runtime/state.hpp"
#include "src/hal/vdp2/vdp2.hpp"

namespace {

namespace hal = saturn::hal::vdp2;

sat_result_t to_result(hal::SpecialResult result) {
    switch (result) {
        case hal::SpecialResult::Ok: return SAT_OK;
        case hal::SpecialResult::Unsupported: return SAT_ERR_UNSUPPORTED;
        default: return SAT_ERR_INVALID_ARG;
    }
}

}  // namespace

extern "C" sat_result_t sat_vdp2_special_priority_mode_set(sat_vdp2_layer_t layer, uint8_t mode) {
    SAT_TRY(saturn::core::require_initialized());
    return to_result(hal::set_special_priority_mode(static_cast<uint8_t>(layer), mode));
}

extern "C" sat_result_t sat_vdp2_special_color_calc_mode_set(sat_vdp2_layer_t layer, uint8_t mode) {
    SAT_TRY(saturn::core::require_initialized());
    return to_result(hal::set_special_color_mode(static_cast<uint8_t>(layer), mode));
}

extern "C" sat_result_t sat_vdp2_special_function_code_set(uint8_t code_a, uint8_t code_b) {
    SAT_TRY(saturn::core::require_initialized());
    hal::set_special_function_code(code_a, code_b);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_special_function_select(sat_vdp2_layer_t layer, uint8_t select) {
    SAT_TRY(saturn::core::require_initialized());
    return to_result(hal::set_special_function_select(static_cast<uint8_t>(layer), select));
}
