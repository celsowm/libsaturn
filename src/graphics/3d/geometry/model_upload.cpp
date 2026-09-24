#include "saturn/model3d.h"

#include "src/graphics/3d/geometry/model_logic.hpp"

/* Opt-in VDP1 asset upload: core model validation and geometry are linkable
 * without pulling hardware texture-upload functions into pure consumers. */
extern "C" sat_result_t sat_model_upload_textures(
    const sat_model_asset_t* asset,
    sat_vdp1_texture_t* out_textures,
    uint16_t out_cap
) {
    if (asset == nullptr || out_textures == nullptr) return SAT_ERR_INVALID_ARG;
    if (saturn::core::model3d::validate(asset) != SAT_OK) return SAT_ERR_INVALID_ARG;
    if (out_cap < asset->texture_count) return SAT_ERR_CAPACITY;
    for (uint16_t p = 0; p < asset->palette_count; ++p) {
        const uint16_t* pal = &asset->palettes_rgb555[(uint32_t)p * 256u];
        const uint16_t bank = (uint16_t)(asset->palette_base + p);
        const sat_result_t st = sat_palette_upload_indexed8(pal, bank);
        if (st != SAT_OK) return st;
    }
    /* LUTs go first and back to back, so table i's handle is the first
     * handle plus i * 32 bytes / 8 -- no per-table handle storage. */
    uint16_t first_lut = 0u;
    for (uint16_t l = 0; l < asset->lut_count; ++l) {
        uint16_t handle = 0u;
        const sat_result_t st = sat_vdp1_upload_lut(&asset->luts_rgb555[(uint32_t)l * 16u], &handle);
        if (st != SAT_OK) return st;
        if (l == 0u) first_lut = handle;
        else if (handle != (uint16_t)(first_lut + l * 4u)) return SAT_ERR_VERIFY_FAILED;
    }
    for (uint16_t t = 0; t < asset->texture_count; ++t) {
        const sat_model_texture_asset_t* src = &asset->textures[t];
        sat_result_t st;
        if ((src->flags & SAT_MODEL_TEXTURE_LUT4) != 0u) {
            st = sat_tex_upload_lut4_pixels(&out_textures[t], src->pixels, src->width, src->height,
                                            (uint16_t)(first_lut + src->palette_slot * 4u));
        } else {
            const uint16_t bank = (uint16_t)(asset->palette_base + src->palette_slot);
            st = sat_tex_upload_indexed8_pixels(&out_textures[t], src->pixels, src->width, src->height, bank);
        }
        if (st != SAT_OK) return st;
    }
    return SAT_OK;
}

