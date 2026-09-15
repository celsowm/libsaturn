#ifndef SATURN_CORE_MODEL3D_LOGIC_HPP
#define SATURN_CORE_MODEL3D_LOGIC_HPP

/* Pure, host-testable compiled-model validation and geometry helpers.
 * No hardware access; src/core/model3d_api.cpp adds the upload/draw paths.
 */

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/model3d.h"

namespace saturn::core::model3d {

inline sat_result_t validate(const sat_model_asset_t* asset) {
    if (asset == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (asset->vertex_count == 0u || asset->face_count == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    if (asset->vertices == nullptr || asset->indices == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (asset->face_texture_indices == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    /* Textures and their palettes are required only by textured assets; a
     * solid-color asset has none and draws every face as a polygon. */
    if (asset->texture_count > 0u) {
        if (asset->textures == nullptr) {
            return SAT_ERR_INVALID_ARG;
        }
        if (asset->palette_count == 0u || asset->palettes_rgb555 == nullptr) {
            return SAT_ERR_INVALID_ARG;
        }
        if (asset->palette_base >= 8u) {
            return SAT_ERR_INVALID_ARG;
        }
        if ((uint32_t)asset->palette_base + (uint32_t)asset->palette_count > 8u) {
            return SAT_ERR_INVALID_ARG;
        }
    }
    if (asset->shade_palette_count > 256u ||
        (asset->shade_palette_count > 0u && asset->shade_palette_rgb555 == nullptr)) {
        return SAT_ERR_INVALID_ARG;
    }
    if (asset->face_base_shades != nullptr) {
        if (asset->shade_palette_count == 0u) {
            return SAT_ERR_INVALID_ARG;
        }
        for (uint16_t f = 0; f < asset->face_count; ++f) {
            if (asset->face_base_shades[f] >= asset->shade_palette_count) {
                return SAT_ERR_INVALID_ARG;
            }
        }
    }
    for (uint16_t f = 0; f < asset->face_count; ++f) {
        const uint16_t* idx = &asset->indices[(uint32_t)f * 4u];
        for (int k = 0; k < 4; ++k) {
            if (idx[k] >= asset->vertex_count) {
                return SAT_ERR_INVALID_ARG;
            }
        }
        const uint16_t sel = asset->face_texture_indices[f];
        if (sel == SAT_MESH_TEXTURE_NONE) {
            continue;
        }
        if (sel >= asset->texture_count) {
            return SAT_ERR_INVALID_ARG;
        }
    }
    for (uint16_t t = 0; t < asset->texture_count; ++t) {
        const sat_model_texture_asset_t* tex = &asset->textures[t];
        if (tex->pixels == nullptr) {
            return SAT_ERR_INVALID_ARG;
        }
        if (tex->width == 0u || tex->height == 0u || (tex->width & 7u) != 0u) {
            return SAT_ERR_INVALID_ARG;
        }
        if (tex->width > 504u || tex->height > 255u) {
            return SAT_ERR_INVALID_ARG;
        }
        if (tex->palette_slot >= asset->palette_count) {
            return SAT_ERR_INVALID_ARG;
        }
        if (tex->pixel_count != (uint32_t)tex->width * (uint32_t)tex->height) {
            return SAT_ERR_INVALID_ARG;
        }
    }
    return SAT_OK;
}

inline sat_result_t compute_bounds(
    const sat_model_asset_t* asset,
    sat_vec3_t* out_min,
    sat_vec3_t* out_max
) {
    if (asset == nullptr || out_min == nullptr || out_max == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (asset->vertex_count == 0u || asset->vertices == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    sat_vec3_t mn = asset->vertices[0];
    sat_vec3_t mx = asset->vertices[0];
    for (uint16_t i = 1; i < asset->vertex_count; ++i) {
        const sat_vec3_t v = asset->vertices[i];
        if (v.x < mn.x) mn.x = v.x;
        if (v.y < mn.y) mn.y = v.y;
        if (v.z < mn.z) mn.z = v.z;
        if (v.x > mx.x) mx.x = v.x;
        if (v.y > mx.y) mx.y = v.y;
        if (v.z > mx.z) mx.z = v.z;
    }
    *out_min = mn;
    *out_max = mx;
    return SAT_OK;
}

inline sat_result_t compute_center(const sat_model_asset_t* asset, sat_vec3_t* out) {
    if (asset == nullptr || out == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    sat_vec3_t mn, mx;
    const sat_result_t st = compute_bounds(asset, &mn, &mx);
    if (st != SAT_OK) {
        return st;
    }
    out->x = mn.x + (mx.x - mn.x) / 2;
    out->y = mn.y + (mx.y - mn.y) / 2;
    out->z = mn.z + (mx.z - mn.z) / 2;
    return SAT_OK;
}

inline uint32_t texture_bytes(const sat_model_asset_t* asset) {
    if (asset == nullptr || asset->textures == nullptr) {
        return 0u;
    }
    uint32_t total = 0u;
    for (uint16_t t = 0; t < asset->texture_count; ++t) {
        total += asset->textures[t].pixel_count;
    }
    return total;
}

inline uint32_t vram_estimate_bytes(const sat_model_asset_t* asset) {
    if (asset == nullptr || asset->textures == nullptr) {
        return 0u;
    }
    uint32_t total = 0u;
    for (uint16_t t = 0; t < asset->texture_count; ++t) {
        const uint32_t size = asset->textures[t].pixel_count;
        total += (size + 7u) & ~7u;
    }
    return total;
}

inline sat_result_t face_base_colors(const sat_model_asset_t* asset, uint16_t* out, uint16_t cap) {
    if (asset == nullptr || out == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (asset->face_base_shades == nullptr || asset->shade_palette_rgb555 == nullptr ||
        asset->shade_palette_count == 0u) {
        return SAT_ERR_UNSUPPORTED;
    }
    if (cap < asset->face_count) {
        return SAT_ERR_CAPACITY;
    }
    const uint16_t count = asset->shade_palette_count;
    for (uint16_t f = 0; f < asset->face_count; ++f) {
        const uint8_t i = asset->face_base_shades[f];
        out[f] = asset->shade_palette_rgb555[(i < count) ? i : 0u];
    }
    return SAT_OK;
}

}  // namespace saturn::core::model3d

#endif /* SATURN_CORE_MODEL3D_LOGIC_HPP */
