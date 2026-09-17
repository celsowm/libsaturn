#include "saturn/model3d.h"

#include "src/core/model3d_logic.hpp"

extern "C" sat_result_t sat_model_validate(const sat_model_asset_t* asset) {
    return saturn::core::model3d::validate(asset);
}

extern "C" sat_result_t sat_model_copy_to_mesh(const sat_model_asset_t* asset, sat_mesh_t* mesh) {
    if (asset == nullptr || mesh == nullptr) return SAT_ERR_INVALID_ARG;
    if (saturn::core::model3d::validate(asset) != SAT_OK) return SAT_ERR_INVALID_ARG;
    if (mesh->vertices == nullptr || mesh->indices == nullptr) return SAT_ERR_INVALID_ARG;
    if (mesh->vertex_cap < asset->vertex_count || mesh->face_cap < asset->face_count) return SAT_ERR_CAPACITY;
    for (uint16_t i = 0; i < asset->vertex_count; ++i) mesh->vertices[i] = asset->vertices[i];
    for (uint32_t i = 0; i < (uint32_t)asset->face_count * 4u; ++i) mesh->indices[i] = asset->indices[i];
    mesh->vertex_count = asset->vertex_count;
    mesh->face_count = asset->face_count;
    return SAT_OK;
}

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
    for (uint16_t t = 0; t < asset->texture_count; ++t) {
        const sat_model_texture_asset_t* src = &asset->textures[t];
        const uint16_t bank = (uint16_t)(asset->palette_base + src->palette_slot);
        const sat_result_t st = sat_tex_upload_indexed8_pixels(&out_textures[t], src->pixels, src->width, src->height, bank);
        if (st != SAT_OK) return st;
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_model_bind_draw(
    const sat_model_asset_t* asset,
    const sat_mesh_t* mesh,
    const sat_vdp1_texture_t* textures,
    uint16_t texture_count,
    const sat_mat4_t* view_proj,
    const sat_vec3_t* eye,
    uint16_t color,
    const uint16_t* face_colors,
    sat_fx16_t ambient,
    uint16_t flags,
    uint8_t* order,
    uint32_t* depth,
    sat_mesh_draw_t* out_draw
) {
    return sat_model_bind_draw_ex(
        asset, mesh, textures, texture_count, view_proj, eye, color,
        face_colors, ambient, flags, order, nullptr, depth, out_draw);
}

extern "C" sat_result_t sat_model_bind_draw_ex(
    const sat_model_asset_t* asset,
    const sat_mesh_t* mesh,
    const sat_vdp1_texture_t* textures,
    uint16_t texture_count,
    const sat_mat4_t* view_proj,
    const sat_vec3_t* eye,
    uint16_t color,
    const uint16_t* face_colors,
    sat_fx16_t ambient,
    uint16_t flags,
    uint8_t* order,
    uint16_t* order16,
    uint32_t* depth,
    sat_mesh_draw_t* out_draw
) {
    if (asset == nullptr || mesh == nullptr || view_proj == nullptr || eye == nullptr || out_draw == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    if (asset->texture_count > 0u && textures == nullptr) return SAT_ERR_INVALID_ARG;
    if (asset->face_texture_indices == nullptr || (asset->texture_count > 0u && asset->textures == nullptr)) {
        return SAT_ERR_INVALID_ARG;
    }
    if (mesh->face_count != asset->face_count || mesh->vertex_count != asset->vertex_count) return SAT_ERR_INVALID_ARG;
    if (texture_count < asset->texture_count) return SAT_ERR_INVALID_ARG;
    const bool sorted = (flags & SAT_MESH_SORT) != 0u;
    const bool wide = asset->face_count > 255u;
    if (sorted && wide && (order16 == nullptr || depth == nullptr)) return SAT_ERR_INVALID_ARG;
    if (sorted && !wide && (order == nullptr || depth == nullptr)) return SAT_ERR_INVALID_ARG;
    out_draw->view_proj = view_proj;
    out_draw->eye = *eye;
    out_draw->color = color;
    out_draw->face_colors = face_colors;
    out_draw->textures = textures;
    out_draw->texture_count = texture_count;
    out_draw->face_texture_indices = asset->face_texture_indices;
    out_draw->ambient = ambient;
    out_draw->flags = flags;
    out_draw->order = order;
    out_draw->depth = depth;
    out_draw->order16 = order16;
    out_draw->screen = nullptr;
    out_draw->vertex_gouraud = nullptr;
    return SAT_OK;
}

extern "C" sat_result_t sat_model_compute_bounds(const sat_model_asset_t* asset, sat_vec3_t* out_min, sat_vec3_t* out_max) {
    return saturn::core::model3d::compute_bounds(asset, out_min, out_max);
}
extern "C" sat_result_t sat_model_compute_center(const sat_model_asset_t* asset, sat_vec3_t* out_center) {
    return saturn::core::model3d::compute_center(asset, out_center);
}
extern "C" uint32_t sat_model_texture_bytes(const sat_model_asset_t* asset) {
    return saturn::core::model3d::texture_bytes(asset);
}
extern "C" uint32_t sat_model_vram_estimate_bytes(const sat_model_asset_t* asset) {
    return saturn::core::model3d::vram_estimate_bytes(asset);
}
extern "C" sat_result_t sat_model_face_base_colors(const sat_model_asset_t* asset, uint16_t* out_colors, uint16_t color_cap) {
    return saturn::core::model3d::face_base_colors(asset, out_colors, color_cap);
}
