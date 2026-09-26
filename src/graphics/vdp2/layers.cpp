#include "saturn/vdp2_layers.h"

#include "src/core/runtime/state.hpp"
#include "src/hal/vdp2/vdp2.hpp"

namespace {

namespace hal = saturn::hal::vdp2;
namespace nbg = saturn::hal::vdp2::nbg;

constexpr uint32_t kZoomMin = 0x4000u;      /* 1/4 */
constexpr uint32_t kZoomMax = 0x80000u;     /* 8x */

sat_result_t to_result(hal::NbgResult r) {
    switch (r) {
        case hal::NbgResult::Ok: return SAT_OK;
        case hal::NbgResult::Busy: return SAT_ERR_BUSY;
        case hal::NbgResult::NoCyclePattern: return SAT_ERR_CAPACITY;
        default: return SAT_ERR_INVALID_ARG;
    }
}

/* Coordinate increment (16.16) for a magnification (16.16). */
uint32_t increment_for(uint32_t zoom) {
    return static_cast<uint32_t>((1ull << 32) / zoom);
}

uint8_t reduction_for(uint32_t x_increment) {
    if (x_increment <= 0x10000u) return 0u;
    if (x_increment <= 0x20000u) return 1u;
    return 2u;
}

bool valid_layer(sat_vdp2_layer_t layer) {
    return static_cast<uint32_t>(layer) < nbg::kLayerCount;
}

}  // namespace

extern "C" void sat_vdp2_layer_config_default(sat_vdp2_layer_t layer,
                                              sat_vdp2_layer_config_t* out_config) {
    if (out_config == nullptr) return;
    *out_config = {};
    out_config->layer = static_cast<uint8_t>(layer);
    out_config->color_mode = SAT_VDP2_COLOR_MODE_16;
    out_config->char_size = SAT_VDP2_CHAR_SIZE_1X1;
    out_config->pattern_name_words = 1u;
    out_config->plane_pages_x = 1u;
    out_config->plane_pages_y = 1u;
    out_config->priority = 1u;
    out_config->transparent = 1u;
}

extern "C" sat_result_t sat_vdp2_layer_configure(const sat_vdp2_layer_config_t* config) {
    SAT_TRY(saturn::core::require_initialized());
    if (config == nullptr || config->layer >= nbg::kLayerCount || config->color_mode > 4u ||
        config->char_size > 1u || config->priority > 7u) {
        return SAT_ERR_INVALID_ARG;
    }
    nbg::Layer layer{};
    layer.enabled = true;
    layer.bitmap = config->bitmap != 0u;
    layer.colors = static_cast<nbg::Colors>(config->color_mode);
    layer.char_2x2 = config->char_size == SAT_VDP2_CHAR_SIZE_2X2;
    layer.pn_one_word = config->pattern_name_words != 2u;
    if (config->pattern_name_words != 1u && config->pattern_name_words != 2u) return SAT_ERR_INVALID_ARG;
    layer.pages_x = config->plane_pages_x;
    layer.pages_y = config->plane_pages_y;
    layer.bitmap_size = config->bitmap_size;
    layer.priority = config->priority;
    layer.transparent = config->transparent != 0u;
    layer.palette = static_cast<uint8_t>(config->palette & 7u);
    layer.palette_supp = static_cast<uint8_t>(config->palette & 7u);
    layer.char_banks = static_cast<uint8_t>(config->char_bank_mask & 0x0Fu);
    for (uint8_t p = 0u; p < 4u; ++p) layer.plane_address[p] = config->plane_address[p];
    layer.vcs = config->vertical_cell_scroll != 0u;
    layer.vcs_address = config->vertical_cell_scroll_address;
    if (layer.vcs && (layer.vcs_address & 3u) != 0u) return SAT_ERR_INVALID_ARG;
    if (!layer.bitmap && layer.pn_one_word) {
        /* The 10-bit character numbers of 1-word names index a window; the
         * five auxiliary bits of the pattern name control register place it. */
        const uint32_t window = layer.char_2x2 ? 0x20000u : 0x8000u;
        if (config->char_base_address % window != 0u || config->char_base_address >= nbg::kVramBytes) {
            return SAT_ERR_INVALID_ARG;
        }
        const uint32_t units = config->char_base_address / window;
        layer.char_number_supp = static_cast<uint8_t>(layer.char_2x2 ? ((units & 7u) << 2u)
                                                                     : (units & 0x1Fu));
    } else if (!layer.bitmap && config->char_base_address != 0u) {
        return SAT_ERR_INVALID_ARG;   /* 2-word names carry the whole character number */
    }
    /* A layer already configured keeps its zoom. */
    nbg::Layer previous{};
    if (hal::nbg_layer(config->layer, &previous)) layer.reduction = previous.reduction;
    return to_result(hal::nbg_configure(config->layer, layer));
}

extern "C" sat_result_t sat_vdp2_layer_release(sat_vdp2_layer_t layer) {
    SAT_TRY(saturn::core::require_initialized());
    if (!valid_layer(layer)) return SAT_ERR_INVALID_ARG;
    nbg::Layer current{};
    if (!hal::nbg_layer(static_cast<uint8_t>(layer), &current)) return SAT_ERR_NOT_FOUND;
    hal::nbg_release(static_cast<uint8_t>(layer));
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_layer_set_enabled(sat_vdp2_layer_t layer, uint8_t enabled) {
    SAT_TRY(saturn::core::require_initialized());
    if (!valid_layer(layer)) return SAT_ERR_INVALID_ARG;
    nbg::Layer current{};
    if (!hal::nbg_layer(static_cast<uint8_t>(layer), &current)) return SAT_ERR_NOT_FOUND;
    current.enabled = enabled != 0u;
    return to_result(hal::nbg_configure(static_cast<uint8_t>(layer), current));
}

extern "C" sat_result_t sat_vdp2_layer_set_priority(sat_vdp2_layer_t layer, uint8_t priority) {
    SAT_TRY(saturn::core::require_initialized());
    if (!valid_layer(layer) || priority > 7u) return SAT_ERR_INVALID_ARG;
    nbg::Layer current{};
    if (!hal::nbg_layer(static_cast<uint8_t>(layer), &current)) return SAT_ERR_NOT_FOUND;
    current.priority = priority;
    return to_result(hal::nbg_configure(static_cast<uint8_t>(layer), current));
}

extern "C" sat_result_t sat_vdp2_layer_set_scroll(sat_vdp2_layer_t layer, int32_t x, int32_t y) {
    SAT_TRY(saturn::core::require_initialized());
    if (!valid_layer(layer)) return SAT_ERR_INVALID_ARG;
    nbg::Layer current{};
    if (!hal::nbg_layer(static_cast<uint8_t>(layer), &current)) return SAT_ERR_NOT_FOUND;
    /* Integer part 11 bits (the screen repeats past the display area), and the
     * fraction's top 8 bits for NBG0/NBG1. */
    const uint16_t xi = static_cast<uint16_t>((x >> 16) & 0x07FF);
    const uint16_t yi = static_cast<uint16_t>((y >> 16) & 0x07FF);
    const uint16_t xf = static_cast<uint16_t>(((x >> 8) & 0xFF) << 8u);
    const uint16_t yf = static_cast<uint16_t>(((y >> 8) & 0xFF) << 8u);
    hal::nbg_set_scroll(static_cast<uint8_t>(layer), xi, xf, yi, yf);
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_layer_set_zoom(sat_vdp2_layer_t layer, uint32_t zoom_x,
                                                uint32_t zoom_y) {
    SAT_TRY(saturn::core::require_initialized());
    if (static_cast<uint32_t>(layer) > 1u) return SAT_ERR_INVALID_ARG;
    if (zoom_x < kZoomMin || zoom_x > kZoomMax || zoom_y < kZoomMin || zoom_y > kZoomMax) {
        return SAT_ERR_INVALID_ARG;
    }
    nbg::Layer current{};
    if (!hal::nbg_layer(static_cast<uint8_t>(layer), &current)) return SAT_ERR_NOT_FOUND;
    const uint32_t xi = increment_for(zoom_x);
    const uint32_t yi = increment_for(zoom_y);
    const uint8_t reduction = reduction_for(xi);
    if (reduction != current.reduction) {
        current.reduction = reduction;
        SAT_TRY(to_result(hal::nbg_configure(static_cast<uint8_t>(layer), current)));
    }
    hal::nbg_set_zoom_increment(static_cast<uint8_t>(layer),
                                static_cast<uint16_t>((xi >> 16) & 0x7u),
                                static_cast<uint16_t>(((xi >> 8) & 0xFFu) << 8u),
                                static_cast<uint16_t>((yi >> 16) & 0x7u),
                                static_cast<uint16_t>(((yi >> 8) & 0xFFu) << 8u));
    return SAT_OK;
}

extern "C" sat_result_t sat_vdp2_layer_cycle_patterns(uint16_t out_registers[8]) {
    SAT_TRY(saturn::core::require_initialized());
    if (out_registers == nullptr) return SAT_ERR_INVALID_ARG;
    for (uint8_t i = 0u; i < 8u; ++i) out_registers[i] = hal::nbg_cycle_word(i);
    return SAT_OK;
}
