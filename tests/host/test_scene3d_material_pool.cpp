#include <cassert>
#include <cstdio>
#include "saturn/scene3d_material_pool.h"

static uint16_t upload_count=0;
static uint16_t palette_count=0;
static uint16_t last_bank=0;
static uint16_t pixel_indices[8]={};
static uint16_t uploaded_colors[256]={};

extern "C" sat_result_t sat_tex_upload_indexed8_pixels(
    sat_vdp1_texture_t* texture,const uint8_t* pixels,
    uint16_t width,uint16_t height,uint16_t bank) {
    assert(width==8u && height==8u && bank==4u);
    assert(upload_count<8u);
    for (uint16_t i=1;i<64u;++i) assert(pixels[i]==pixels[0]);
    pixel_indices[upload_count]=pixels[0];
    texture->valid=1u;
    texture->srca=static_cast<uint16_t>(100u+upload_count++);
    return SAT_OK;
}
extern "C" sat_result_t sat_palette_upload_indexed8(
    const uint16_t* palette,uint16_t bank) {
    ++palette_count;
    last_bank=bank;
    for (uint16_t i=0;i<256u;++i) uploaded_colors[i]=palette[i];
    return SAT_OK;
}
int main() {
    sat_scene3d_solid_pool_t pool={};
    sat_scene3d_material_t materials[3]={};
    sat_vdp1_texture_t textures[3]={};
    uint16_t colors[3]={};
    uint8_t scratch[64]={};
    assert(sat_scene3d_solid_pool_init(
        &pool,materials,textures,colors,scratch,3u,4u)==SAT_OK);
    uint16_t a=999,b=999,c=999,d=999;
    assert(sat_scene3d_solid_pool_register(&pool,0x801fu,&a)==SAT_OK);
    assert(sat_scene3d_solid_pool_register(&pool,0x83e0u,&b)==SAT_OK);
    assert(sat_scene3d_solid_pool_register(&pool,0x801fu,&c)==SAT_OK);
    assert(a==0u && b==1u && c==a);
    assert(upload_count==2u && pool.count==2u);
    assert(pixel_indices[0]==1u && pixel_indices[1]==2u);
    assert(materials[a].texture==&textures[a]);
    assert(materials[a].kind==SAT_SCENE3D_INDEXED_SOLID);
    assert(materials[a].color_calc_slot==SAT_INDEXED_SOLID_OPAQUE);
    assert(sat_scene3d_solid_pool_register(&pool,0x8000u,&d)==SAT_OK);
    assert(sat_scene3d_solid_pool_register(&pool,0x8fffu,&d)==SAT_ERR_CAPACITY);
    assert(pool.count==3u && upload_count==3u);
    assert(sat_scene3d_solid_pool_upload_palette(&pool)==SAT_OK);
    assert(palette_count==1u && last_bank==4u);
    assert(uploaded_colors[0]==0u);
    assert(uploaded_colors[1]==0x801fu);
    assert(uploaded_colors[2]==0x83e0u);
    assert(uploaded_colors[3]==0x8000u);
    std::puts("scene3d solid material pool: OK");
    return 0;
}
