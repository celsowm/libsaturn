#include "saturn/vdp2_sprite_shadow.h"

#include "src/core/runtime/state.hpp"
#include "src/hal/vdp2/vdp2.hpp"

namespace {

namespace hal = saturn::hal::vdp2;

sat_result_t to_result(hal::SpriteShadowResult result) {
    switch (result) {
        case hal::SpriteShadowResult::Ok: return SAT_OK;
        case hal::SpriteShadowResult::Unsupported: return SAT_ERR_UNSUPPORTED;
        case hal::SpriteShadowResult::Busy: return SAT_ERR_BUSY;
        default: return SAT_ERR_INVALID_ARG;
    }
}

}  // namespace

extern "C" sat_result_t sat_vdp2_sprite_shadow_configure(
    const sat_vdp2_sprite_shadow_config_t* config) {
    SAT_TRY(saturn::core::require_initialized());
    if (config == nullptr) return SAT_ERR_INVALID_ARG;
    return to_result(hal::set_sprite_shadow(config->mode, config->target_mask,
                                            config->transparent_shadow != 0u,
                                            config->base_priority,
                                            config->shadow_priority));
}

extern "C" sat_result_t sat_vdp2_sprite_shadow_disable(void) {
    SAT_TRY(saturn::core::require_initialized());
    hal::disable_sprite_shadow();
    return SAT_OK;
}

extern "C" uint8_t sat_vdp2_sprite_shadow_active(void) {
    return hal::sprite_shadow_active() ? 1u : 0u;
}
