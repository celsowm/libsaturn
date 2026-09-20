#include "saturn/scene3d_material_pool.h"

extern "C" sat_result_t sat_scene3d_solid_pool_init(
    sat_scene3d_solid_pool_t* pool,
    sat_scene3d_material_t* material_storage,
    sat_vdp1_texture_t* texture_storage,
    uint16_t* color_storage, uint8_t pixels[64],
    uint16_t capacity, uint16_t palette_bank) {
    if (!pool || !material_storage || !texture_storage || !color_storage ||
        !pixels || !capacity || capacity>255u || palette_bank>=8u)
        return SAT_ERR_INVALID_ARG;
    *pool={};
    pool->materials=material_storage;
    pool->textures=texture_storage;
    pool->colors=color_storage;
    pool->pixels=pixels;
    pool->capacity=capacity;
    pool->palette_bank=palette_bank;
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_solid_pool_register(
    sat_scene3d_solid_pool_t* pool, uint16_t rgb555,
    uint16_t* out_material_index) {
    if (!pool || !pool->materials || !pool->textures || !pool->colors ||
        !pool->pixels || !out_material_index || !pool->capacity)
        return SAT_ERR_INVALID_ARG;
    for (uint16_t i=0;i<pool->count;++i) {
        if (pool->colors[i]==rgb555) {
            *out_material_index=i;
            return SAT_OK;
        }
    }
    if (pool->count>=pool->capacity) return SAT_ERR_CAPACITY;
    const uint16_t index=pool->count;
    const uint8_t palette_index=static_cast<uint8_t>(index+1u);
    for (uint16_t i=0;i<64u;++i) pool->pixels[i]=palette_index;
    const sat_result_t st=sat_tex_upload_indexed8_pixels(
        &pool->textures[index],pool->pixels,8u,8u,pool->palette_bank);
    if (st!=SAT_OK) return st;
    pool->colors[index]=rgb555;
    sat_scene3d_material_t material={};
    material.kind=SAT_SCENE3D_INDEXED_SOLID;
    material.texture=&pool->textures[index];
    material.color_calc_slot=SAT_INDEXED_SOLID_OPAQUE;
    pool->materials[index]=material;
    pool->count=static_cast<uint16_t>(index+1u);
    *out_material_index=index;
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_solid_pool_upload_palette(
    const sat_scene3d_solid_pool_t* pool) {
    if (!pool || !pool->materials || !pool->textures || !pool->colors ||
        !pool->capacity || pool->count>pool->capacity) return SAT_ERR_INVALID_ARG;
    uint16_t palette[256]={};
    for (uint16_t i=0;i<pool->count;++i) palette[i+1u]=pool->colors[i];
    return sat_palette_upload_indexed8(palette,pool->palette_bank);
}
