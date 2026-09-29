/* Palette registration API: content-deduplicated logical palettes that
 * can override a texture's own palette at draw time (see
 * sat_render2d_set_palette). Ownership rules mirror
 * sat_texture_create_from_surface: a claimed bank uploads through the
 * HAL exactly once and failed HAL uploads leave the registry unchanged. */
#include "saturn/palette.h"

#include "src/core/runtime/state.hpp"
#include "src/graphics/2d/palette/registry.hpp"
#include "src/hal/vdp1/vdp1.hpp"

extern "C" uint16_t sat_palette_bank_count(void) {
    return saturn::core::kCramBankCount;
}

extern "C" sat_result_t sat_palette_register(
    const uint16_t* rgb555,
    sat_palette_t* out_palette
) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) return st;
    if (rgb555 == nullptr || out_palette == nullptr) return SAT_ERR_INVALID_ARG;

    uint16_t bank = 0u;
    bool needs_upload = false;
    st = palette_acquire_logical(g_palette_registry, rgb555, &bank, &needs_upload);
    if (st != SAT_OK) return st;

    if (needs_upload) {
        st = saturn::hal::vdp1::upload_palette(rgb555, bank);
        if (st != SAT_OK) {
            (void)palette_release_logical(g_palette_registry, bank);
            return st;
        }
    }

    out_palette->bank = bank;
    out_palette->generation = g_palette_registry.generation[bank];
    return SAT_OK;
}

extern "C" sat_result_t sat_palette_unregister(sat_palette_t palette) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) return st;
    if (!palette_handle_valid(g_palette_registry, palette.bank, palette.generation)) {
        return SAT_ERR_INVALID_ARG;
    }
    return palette_release_logical(g_palette_registry, palette.bank);
}

extern "C" sat_result_t sat_palette_bank(sat_palette_t palette, uint16_t* out_bank) {
    using namespace saturn::core;
    sat_result_t st = require_initialized();
    if (st != SAT_OK) return st;
    if (out_bank == nullptr) return SAT_ERR_INVALID_ARG;
    if (!palette_handle_valid(g_palette_registry, palette.bank, palette.generation)) {
        return SAT_ERR_INVALID_ARG;
    }
    *out_bank = palette.bank;
    return SAT_OK;
}
