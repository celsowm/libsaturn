#include "saturn/character3d.h"
#include "src/graphics/2d/textures/runtime.hpp"

extern "C" sat_result_t sat_character_materials_release(sat_texture_t* owners,uint16_t count) {
    if (!owners && count) return SAT_ERR_INVALID_ARG;
    for (uint16_t i=0;i<count;++i) {
        const sat_result_t st=sat_texture_destroy(owners[i]);
        if (st!=SAT_OK) return st;
        owners[i]={};
    }
    return SAT_OK;
}
extern "C" sat_result_t sat_character_materials_upload(const sat_model_asset_t* model,
    sat_texture_t* owners,sat_vdp1_texture_t* native,sat_scene3d_material_t* materials,uint16_t capacity) {
    if (!model || !owners || !native || !materials) return SAT_ERR_INVALID_ARG;
    if (capacity<model->texture_count) return SAT_ERR_CAPACITY;
    SAT_TRY(sat_model_validate(model));
    for (uint16_t i=0;i<model->texture_count;++i)
        if (model->textures[i].flags&SAT_MODEL_TEXTURE_LUT4) return SAT_ERR_UNSUPPORTED;
    uint16_t done=0;
    for (;done<model->texture_count;++done) {
        const sat_model_texture_asset_t& t=model->textures[done];
        sat_surface_t surface{const_cast<uint8_t*>(t.pixels),t.width,t.height,t.width,
            SAT_PIXEL_INDEX8,model->palettes_rgb555+t.palette_slot*256u,256};
        const sat_result_t st=sat_texture_create_from_surface(&owners[done],&surface,SAT_TEXTURE_UPLOAD_ONLY);
        if (st!=SAT_OK) { (void)sat_character_materials_release(owners,done); return st; }
        const auto* slot=saturn::core::texture_resolve(saturn::core::g_texture_registry,owners[done]);
        if (!slot) { (void)sat_character_materials_release(owners,done+1); return SAT_ERR_VERIFY_FAILED; }
        native[done]=slot->native;
        materials[done]={}; materials[done].kind=SAT_SCENE3D_INDEXED_TEXTURED;
        materials[done].texture=&native[done]; materials[done].color_calc_slot=SAT_INDEXED_SOLID_OPAQUE;
        // Explicit near-plane fallback: VDP1 cannot freely interpolate new UVs.
        const uint8_t color=t.pixels[t.pixel_count/2u];
        materials[done].rgb555=model->palettes_rgb555[t.palette_slot*256u+color];
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_character_shadow(sat_scene_t* scene, const sat_vec3_t* center,
    sat_fx16_t y, sat_fx16_t rx, sat_fx16_t rz, uint16_t color, uint16_t pass, uint8_t segments) {
    if (!scene || !scene->active || !center || rx <= 0 || rz <= 0 ||
        segments < 3 || segments > 16 || pass > SAT_SCENE3D_PASS_MAX) return SAT_ERR_INVALID_ARG;
    if (scene->faces.capacity - scene->faces.count < segments) return SAT_ERR_CAPACITY;
    const uint16_t before = scene->faces.count;
    sat_scene3d_material_t mat{};
    mat.kind = SAT_SCENE3D_RGB; mat.rgb555 = color;
    mat.color_calc_slot = SAT_INDEXED_SOLID_OPAQUE;
    for (uint16_t i = 0; i < segments; ++i) {
        const sat_fx16_t a = (static_cast<int32_t>(i) * 360 * SAT_FX16_ONE) / segments;
        const sat_fx16_t b = (static_cast<int32_t>(i+1) * 360 * SAT_FX16_ONE) / segments;
        sat_vec3_t p{center->x + sat_fx16_mul(rx, sat_cos_deg(a)), y,
            center->z + sat_fx16_mul(rz, sat_sin_deg(a))};
        sat_vec3_t q{center->x + sat_fx16_mul(rx, sat_cos_deg(b)), y,
            center->z + sat_fx16_mul(rz, sat_sin_deg(b))};
        sat_quad3_t quad{{{center->x,y,center->z}, p, q, q}};
        const sat_result_t st = sat_scene_submit_quad(scene, &quad, &mat, pass);
        if (st != SAT_OK) { scene->faces.count = before; return st; }
    }
    return SAT_OK;
}
