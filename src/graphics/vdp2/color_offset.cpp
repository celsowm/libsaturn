#include "saturn/vdp2_color_offset.h"

#include "src/core/runtime/internal.hpp"
#include "src/core/runtime/state.hpp"
#include "src/graphics/vdp2/color_offset_logic.hpp"
#include "src/hal/vdp2/vdp2.hpp"

namespace {

/* .bss: zero is "no layer uses an offset", matching reset_color_ops(). */
saturn::core::vdp2_color_offset::Layers g_layers;

}  // namespace

extern "C" sat_result_t sat_vdp2_color_offset_set(
    sat_vdp2_color_offset_bank_t bank,
    const sat_vdp2_color_offset_t* offset
) {
    using namespace saturn::core::vdp2_color_offset;
    SAT_TRY(saturn::core::require_initialized());
    if (static_cast<uint32_t>(bank) > SAT_VDP2_COLOR_OFFSET_B) return SAT_ERR_INVALID_ARG;
    SAT_TRY(validate_offset(offset));
    saturn::hal::vdp2::set_color_offset(
        static_cast<uint8_t>(bank),
        encode_channel(offset->r), encode_channel(offset->g), encode_channel(offset->b));
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_color_offset_enable(
    uint8_t layer_mask,
    sat_vdp2_color_offset_bank_t bank
) {
    using namespace saturn::core::vdp2_color_offset;
    SAT_TRY(saturn::core::require_initialized());
    if (static_cast<uint32_t>(bank) > SAT_VDP2_COLOR_OFFSET_B) return SAT_ERR_INVALID_ARG;
    Layers next = g_layers;
    SAT_TRY(enable_layers(next, layer_mask, static_cast<uint8_t>(bank)));
    g_layers = next;
    saturn::hal::vdp2::set_color_offset_layers(g_layers.clofen, g_layers.clofsl);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_color_offset_disable(uint8_t layer_mask) {
    using namespace saturn::core::vdp2_color_offset;
    SAT_TRY(saturn::core::require_initialized());
    Layers next = g_layers;
    SAT_TRY(disable_layers(next, layer_mask));
    g_layers = next;
    saturn::hal::vdp2::set_color_offset_layers(g_layers.clofen, g_layers.clofsl);
    return SAT_OK;
}
