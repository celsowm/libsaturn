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

    /* Nearest-colour lookup: the pool already holds the registered colours,
     * so level code asking for a shade it never registered gets the closest
     * one without keeping a parallel table of its own.
     * Registered: 0x801f (r=31,g=0,b=0), 0x83e0 (g=31), 0x8000 (black). */
    uint16_t nearest=999u;
    assert(sat_scene3d_solid_pool_find_nearest(&pool,0x801fu,&nearest)==SAT_OK);
    assert(nearest==0u); /* exact match */
    assert(sat_scene3d_solid_pool_find_nearest(&pool,0x83e0u,&nearest)==SAT_OK);
    assert(nearest==1u);
    /* r=30,g=1,b=0 is one step from the red entry and far from the others. */
    assert(sat_scene3d_solid_pool_find_nearest(&pool,0x803eu,&nearest)==SAT_OK);
    assert(nearest==0u);
    /* Near-black picks the black entry, not either saturated primary. */
    assert(sat_scene3d_solid_pool_find_nearest(&pool,0x8021u,&nearest)==SAT_OK);
    assert(nearest==2u);
    assert(sat_scene3d_solid_pool_find_nearest(&pool,0u,nullptr)==
           SAT_ERR_INVALID_ARG);
    /* Restricted to the first two entries, near-black can no longer reach the
     * black entry and falls back to the nearer of the two primaries. This is
     * what keeps a world palette from matching against a model's shade ramp
     * registered into the same pool. */
    assert(sat_scene3d_solid_pool_find_nearest_in(
        &pool,0x8021u,0u,2u,&nearest)==SAT_OK);
    assert(nearest==0u || nearest==1u);
    assert(sat_scene3d_solid_pool_find_nearest_in(
        &pool,0x83e0u,1u,2u,&nearest)==SAT_OK);
    assert(nearest==1u);
    /* An empty range has no answer; a range past the registered colours is a
     * caller bug, not a clamp. */
    assert(sat_scene3d_solid_pool_find_nearest_in(
        &pool,0x801fu,1u,0u,&nearest)==SAT_ERR_NOT_FOUND);
    assert(sat_scene3d_solid_pool_find_nearest_in(
        &pool,0x801fu,2u,2u,&nearest)==SAT_ERR_INVALID_ARG);
    {
        /* An empty pool has no answer to give, and must not report index 0. */
        sat_scene3d_solid_pool_t empty={};
        uint16_t unused=999u;
        assert(sat_scene3d_solid_pool_init(
            &empty,materials,textures,colors,scratch,3u,4u)==SAT_OK);
        assert(sat_scene3d_solid_pool_find_nearest(&empty,0x801fu,&unused)==
               SAT_ERR_NOT_FOUND);
        assert(unused==999u);
    }
    std::puts("scene3d solid material pool: OK");
    return 0;
}
