/* Tests for the renderer-owned INDEX8 solid-geometry path. These stubs
 * exercise submission, validation and painter order without Saturn hardware. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "saturn/mesh3d_draw.h"
#include "saturn/vdp1_color_calc.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while (0)
#define EQ(a,b) CHECK((a)==(b))
#define FX(v) ((sat_fx16_t)((v)*SAT_FX16_ONE))

static int g_near_calls,g_project_calls,g_screen_calls,g_opaque,g_faded;
static int g_clip_mode,g_screen_mode;
static uint8_t g_last_slot;
static uint16_t g_srca[64];
static uint16_t g_x[64];
static sat_result_t g_submit_status=SAT_OK;
static sat_vdp1_texture_t g_texture[2]={{10,8,8,4,1,0},{20,8,8,4,1,0}};
static sat_mat4_t g_matrix={};
static int g_upload_calls;
static uint16_t g_uploaded_palette[4];
static uint16_t g_uploaded_width[4],g_uploaded_height[4];
static uint8_t g_upload_bytes[4][64];
static sat_result_t g_upload_status=SAT_OK;

extern "C" sat_result_t sat_clip_quad_near(
    const sat_quad3_t* quad,const sat_vec3_t*,const sat_vec3_t*,
    sat_fx16_t,sat_quad3_t out[4],uint8_t* count) {
    ++g_near_calls;
    if(g_clip_mode==2) return SAT_ERR_BUSY;
    *count=(g_clip_mode==1)?0u:1u;
    if(*count!=0u) out[0]=*quad;
    return SAT_OK;
}
extern "C" sat_result_t sat_project_quad(
    const sat_mat4_t*,const sat_quad3_t* q,sat_quad2_t* out) {
    ++g_project_calls;
    for(int i=0;i<4;++i) {
        out->x[i]=(int16_t)(q->v[i].x/SAT_FX16_ONE);
        out->y[i]=(int16_t)(q->v[i].y/SAT_FX16_ONE);
    }
    return SAT_OK;
}
extern "C" sat_result_t sat_clip_quad_screen(
    const sat_quad2_t* q,uint16_t,uint16_t,
    sat_quad2_t out[6],uint8_t* count) {
    ++g_screen_calls;
    *count=(g_screen_mode==1)?0u:(g_screen_mode==2)?2u:1u;
    for(uint8_t i=0;i<*count;++i) out[i]=*q;
    return SAT_OK;
}
extern "C" sat_result_t sat_tex_upload_indexed8_pixels(
    sat_vdp1_texture_t* out,const uint8_t* pixels,
    uint16_t width,uint16_t height,uint16_t palette_bank
) {
    const int n=g_upload_calls++;
    if(g_upload_status!=SAT_OK) return g_upload_status;
    CHECK(n<4);
    CHECK(static_cast<uint32_t>(width)*height<=64u);
    g_uploaded_width[n]=width;
    g_uploaded_height[n]=height;
    g_uploaded_palette[n]=palette_bank;
    for(uint32_t i=0u;i<static_cast<uint32_t>(width)*height;++i)
        g_upload_bytes[n][i]=pixels[i];
    *out=(sat_vdp1_texture_t){
        static_cast<uint16_t>(200+n),width,height,palette_bank,1u,0u};
    return SAT_OK;
}
extern "C" sat_result_t sat_draw_sprite_distorted(
    const sat_distorted_sprite_cmd_t* cmd) {
    g_srca[g_opaque+g_faded]=cmd->texture->srca;
    g_x[g_opaque+g_faded]=(uint16_t)cmd->x[0];
    ++g_opaque;
    return g_submit_status;
}
extern "C" sat_result_t sat_draw_sprite_distorted_color_calc(
    const sat_distorted_sprite_cmd_t* cmd,uint8_t slot) {
    g_srca[g_opaque+g_faded]=cmd->texture->srca;
    g_x[g_opaque+g_faded]=(uint16_t)cmd->x[0];
    g_last_slot=slot;
    ++g_faded;
    return g_submit_status;
}

static int g_polygons;
static uint16_t g_polygon_color;
static sat_result_t g_polygon_status=SAT_OK;
extern "C" sat_result_t sat_draw_quad2_polygon(
    const sat_quad2_t*,uint16_t color) {
    ++g_polygons;
    g_polygon_color=color;
    return g_polygon_status;
}

static void reset() {
    g_polygons=0;
    g_polygon_color=0u;
    g_polygon_status=SAT_OK;
    g_near_calls=g_project_calls=g_screen_calls=g_opaque=g_faded=0;
    g_clip_mode=g_screen_mode=0;
    g_last_slot=255u;
    g_submit_status=SAT_OK;
}
static sat_indexed_solid_render3d_t render() {
    sat_indexed_solid_render3d_t p={};
    p.view_proj=&g_matrix;
    p.eye=(sat_vec3_t){0,0,0};
    p.forward=(sat_vec3_t){0,0,SAT_FX16_ONE};
    p.near_depth=FX(8);
    p.width=320u;
    p.height=224u;
    p.color_calc_slot=SAT_INDEXED_SOLID_OPAQUE;
    return p;
}
static sat_quad3_t quad(int x,int z) {
    sat_quad3_t q={};
    q.v[0]=(sat_vec3_t){FX(x),FX(2),FX(z)};
    q.v[1]=(sat_vec3_t){FX(x+2),FX(2),FX(z)};
    q.v[2]=(sat_vec3_t){FX(x+2),0,FX(z)};
    q.v[3]=(sat_vec3_t){FX(x),0,FX(z)};
    return q;
}
static void quad_validates_and_draws_opaque() {
    reset();
    const sat_quad3_t q=quad(3,12);
    sat_indexed_solid_render3d_t p=render();
    EQ(sat_draw_indexed_solid_quad3(&q,&p,&g_texture[0]),SAT_OK);
    EQ(g_near_calls,1);
    EQ(g_project_calls,1);
    EQ(g_screen_calls,1);
    EQ(g_opaque,1);
    EQ(g_faded,0);
    EQ(g_srca[0],10u);
    EQ(g_x[0],3u);
    EQ(sat_draw_indexed_solid_quad3(nullptr,&p,&g_texture[0]),SAT_ERR_INVALID_ARG);
    p.color_calc_slot=8u;
    EQ(sat_draw_indexed_solid_quad3(&q,&p,&g_texture[0]),SAT_ERR_INVALID_ARG);
    EQ(g_near_calls,1);
}
static void faded_quad_and_multiple_screen_triangles() {
    reset();
    sat_quad3_t q=quad(0,12);
    sat_indexed_solid_render3d_t p=render();
    p.color_calc_slot=3u;
    g_screen_mode=2;
    EQ(sat_draw_indexed_solid_quad3(&q,&p,&g_texture[1]),SAT_OK);
    EQ(g_faded,2);
    EQ(g_opaque,0);
    EQ(g_last_slot,3u);
    EQ(g_srca[0],20u);
    EQ(g_srca[1],20u);
}
static void invisible_and_hardware_errors() {
    reset();
    sat_quad3_t q=quad(0,12);
    sat_indexed_solid_render3d_t p=render();
    g_clip_mode=1;
    EQ(sat_draw_indexed_solid_quad3(&q,&p,&g_texture[0]),SAT_OK);
    EQ(g_project_calls,0);
    g_clip_mode=2;
    EQ(sat_draw_indexed_solid_quad3(&q,&p,&g_texture[0]),SAT_ERR_BUSY);
    g_clip_mode=0;
    g_submit_status=SAT_ERR_CAPACITY;
    EQ(sat_draw_indexed_solid_quad3(&q,&p,&g_texture[0]),SAT_ERR_CAPACITY);
    EQ(g_opaque,1);
}
static void rgb_polygon_clips_like_solid_quads() {
    reset();
    sat_quad3_t q=quad(0,12);
    sat_indexed_solid_render3d_t p=render();
    p.color_calc_slot=8u; /* ignored: flat RGB has no fade selector */
    g_screen_mode=2;
    EQ(sat_draw_polygon_quad3(&q,&p,0x801Fu),SAT_OK);
    EQ(g_near_calls,1);
    EQ(g_polygons,2);
    EQ(g_polygon_color,0x801Fu);
    EQ(g_opaque+g_faded,0);
    g_clip_mode=1; /* wholly behind the near plane */
    EQ(sat_draw_polygon_quad3(&q,&p,0x801Fu),SAT_OK);
    EQ(g_polygons,2);
    g_clip_mode=0;
    g_screen_mode=0;
    g_polygon_status=SAT_ERR_CAPACITY;
    EQ(sat_draw_polygon_quad3(&q,&p,0x801Fu),SAT_ERR_CAPACITY);
    EQ(sat_draw_polygon_quad3(nullptr,&p,0x801Fu),SAT_ERR_INVALID_ARG);
    p.near_depth=0;
    EQ(sat_draw_polygon_quad3(&q,&p,0x801Fu),SAT_ERR_INVALID_ARG);
}
static sat_indexed_solid_mesh3d_draw_t mesh_params(
    const uint16_t* mats,uint16_t* order,uint32_t* depth) {
    sat_indexed_solid_mesh3d_draw_t p={};
    p.render=render();
    p.textures=g_texture;
    p.texture_count=2u;
    p.face_materials=mats;
    p.order=order;
    p.depth=depth;
    return p;
}
static void patterned_texture_draws_only_with_original_four_corners() {
    reset();
    sat_quad3_t q=quad(3,12);
    sat_indexed_solid_render3d_t p=render();
    uint8_t emitted=99u;
    EQ(sat_draw_indexed_textured_quad3(&q,&p,&g_texture[1],&emitted),SAT_OK);
    EQ(emitted,1u);
    EQ(g_opaque,1);
    EQ(g_srca[0],20u);
    EQ(g_x[0],3u);
    EQ(g_near_calls,0);
    EQ(g_screen_calls,0);

    /* One near-plane crossing corner must NOT become a triangle with the
     * original texture stretched across it, even if a clipper says count=1. */
    reset();
    q.v[0].z=FX(7);
    emitted=99u;
    EQ(sat_draw_indexed_textured_quad3(&q,&p,&g_texture[1],&emitted),SAT_OK);
    EQ(emitted,0u);
    EQ(g_project_calls,0);
    EQ(g_opaque,0);
    q.v[0].z=FX(12);

    /* A corner just past the viewport edge is still drawn: the VDP1's system
     * clipping trims the sprite, and refusing it made patterned floors vanish
     * whenever the camera reached the near edge of a platform. */
    reset();
    q.v[1].x=FX(161);
    emitted=99u;
    EQ(sat_draw_indexed_textured_quad3(&q,&p,&g_texture[1],&emitted),SAT_OK);
    EQ(emitted,1u);
    EQ(g_project_calls,1);
    EQ(g_opaque,1);

    /* Far enough out, though, projection would clamp the corner and skew the
     * texture, so the caller's subdivision fallback takes over instead. */
    reset();
    q.v[1].x=FX(400);
    emitted=99u;
    EQ(sat_draw_indexed_textured_quad3(&q,&p,&g_texture[1],&emitted),SAT_OK);
    EQ(emitted,0u);
    EQ(g_project_calls,1);
    EQ(g_opaque,0);
    q.v[1].x=FX(5);

    /* The ordinary and VDP2 faded submission paths share the safety gate. */
    reset();
    p.color_calc_slot=4u;
    EQ(sat_draw_indexed_textured_quad3(&q,&p,&g_texture[0],&emitted),SAT_OK);
    EQ(emitted,1u);
    EQ(g_faded,1);
    EQ(g_last_slot,4u);
}

static void patterned_texture_invalid_input_and_command_failure() {
    reset();
    sat_quad3_t q=quad(0,12);
    sat_indexed_solid_render3d_t p=render();
    uint8_t emitted=99u;
    EQ(sat_draw_indexed_textured_quad3(nullptr,&p,&g_texture[0],&emitted),
       SAT_ERR_INVALID_ARG);
    EQ(emitted,0u);
    p.width=1u;
    EQ(sat_draw_indexed_textured_quad3(&q,&p,&g_texture[0],&emitted),
       SAT_ERR_INVALID_ARG);
    EQ(g_project_calls,0);
    p=render();
    g_submit_status=SAT_ERR_CAPACITY;
    EQ(sat_draw_indexed_textured_quad3(&q,&p,&g_texture[0],&emitted),
       SAT_ERR_CAPACITY);
    EQ(emitted,0u);
    EQ(g_opaque,1);
}

static void quadrant_upload_owns_correct_source_rows_and_palette() {
    reset();
    uint8_t pixels[16u*20u]={0};
    for(uint8_t row=0u;row<16u;++row) for(uint8_t col=0u;col<16u;++col)
        pixels[static_cast<uint32_t>(row)*20u+col]=
            static_cast<uint8_t>(row*16u+col);
    sat_vdp1_texture_t tiles[4]={};
    uint8_t scratch[64]={};
    g_upload_calls=0;
    EQ(sat_upload_indexed8_quadrants(
        pixels,16u,16u,20u,5u,tiles,scratch,sizeof(scratch)),SAT_OK);
    EQ(g_upload_calls,4);
    for(uint8_t tile=0u;tile<4u;++tile) {
        EQ(tiles[tile].width,8u);
        EQ(tiles[tile].height,8u);
        EQ(g_uploaded_palette[tile],5u);
        EQ(g_uploaded_width[tile],8u);
        EQ(g_uploaded_height[tile],8u);
        const uint8_t row=tile/2u,col=tile%2u;
        for(uint8_t y=0u;y<8u;++y) for(uint8_t x=0u;x<8u;++x)
            EQ(g_upload_bytes[tile][y*8u+x],
               pixels[static_cast<uint32_t>(row*8u+y)*20u+col*8u+x]);
    }
}
static void quadrant_upload_rejects_bad_source_and_short_scratch_atomically() {
    reset();
    uint8_t pixels[256]={0},scratch[64]={0};
    sat_vdp1_texture_t tiles[4]={};
    g_upload_calls=0;
    EQ(sat_upload_indexed8_quadrants(
        pixels,16u,16u,16u,4u,tiles,scratch,63u),SAT_ERR_CAPACITY);
    EQ(g_upload_calls,0);
    EQ(sat_upload_indexed8_quadrants(
        pixels,16u,16u,15u,4u,tiles,scratch,64u),SAT_ERR_INVALID_ARG);
    EQ(g_upload_calls,0);
    EQ(sat_upload_indexed8_quadrants(
        pixels,16u,15u,16u,4u,tiles,scratch,64u),SAT_ERR_INVALID_ARG);
    EQ(g_upload_calls,0);
}

static sat_indexed_tiled_quad3_t tiled_regions() {
    static sat_vdp1_texture_t whole={100u,16u,16u,4u,1u,0u};
    static sat_vdp1_texture_t quarter[4]={
        {101u,8u,8u,4u,1u,0u},{102u,8u,8u,4u,1u,0u},
        {103u,8u,8u,4u,1u,0u},{104u,8u,8u,4u,1u,0u}
    };
    sat_indexed_tiled_quad3_t p={};
    p.full=&whole;
    for(uint8_t i=0u;i<4u;++i) p.tiles[i]=&quarter[i];
    return p;
}
static void tiled_texture_fast_path_submits_original_once() {
    reset();
    const sat_quad3_t q=quad(3,12);
    const sat_indexed_solid_render3d_t p=render();
    const sat_indexed_tiled_quad3_t regions=tiled_regions();
    uint8_t submitted=99u;
    EQ(sat_draw_indexed_tiled_quad3(&q,&p,&regions,&submitted),SAT_OK);
    EQ(submitted,1u);
    EQ(g_opaque,1);
    EQ(g_srca[0],100u);
    EQ(g_near_calls,0);
    EQ(g_screen_calls,0);
    EQ(g_project_calls,1);
}
static void tiled_texture_preserves_actual_regions_at_near_boundary() {
    reset();
    sat_quad3_t q=quad(3,12);
    /* The corner at A is behind the 8-unit near guard; only TL touches
     * that corner. All three other 8x8 subregions have safe full UV maps. */
    q.v[0].z=FX(7);
    const sat_indexed_solid_render3d_t p=render();
    const sat_indexed_tiled_quad3_t regions=tiled_regions();
    uint8_t submitted=99u;
    EQ(sat_draw_indexed_tiled_quad3(&q,&p,&regions,&submitted),SAT_OK);
    EQ(submitted,3u);
    EQ(g_opaque,3);
    EQ(g_srca[0],102u); /* top right */
    EQ(g_srca[1],103u); /* bottom left */
    EQ(g_srca[2],104u); /* bottom right */
    EQ(g_near_calls,0);
    EQ(g_project_calls,3);
}
static void tiled_texture_preserves_regions_at_screen_boundary() {
    reset();
    sat_quad3_t q=quad(3,12);
    /* Merely crossing the screen edge no longer costs the pattern: one
     * command goes out whole and the VDP1 clips it. */
    q.v[1].x=FX(162);
    const sat_indexed_solid_render3d_t p=render();
    const sat_indexed_tiled_quad3_t regions=tiled_regions();
    uint8_t submitted=99u;
    EQ(sat_draw_indexed_tiled_quad3(&q,&p,&regions,&submitted),SAT_OK);
    EQ(submitted,1u);
    EQ(g_opaque,1);
    EQ(g_srca[0],100u);

    /* Stretched far enough past the bound, the whole-quad command would be
     * clamped, so the subregions that do fit are drawn with their OWN
     * textures. The original full texture MUST NOT be stretched over them. */
    reset();
    q.v[1].x=FX(400);
    submitted=99u;
    EQ(sat_draw_indexed_tiled_quad3(&q,&p,&regions,&submitted),SAT_OK);
    EQ(submitted,3u);
    EQ(g_opaque,3);
    EQ(g_srca[0],101u); /* top left */
    EQ(g_srca[1],103u); /* bottom left */
    EQ(g_srca[2],104u); /* bottom right */
}
static void tiled_texture_rejects_inconsistent_region_dimensions() {
    reset();
    const sat_quad3_t q=quad(3,12);
    const sat_indexed_solid_render3d_t p=render();
    sat_indexed_tiled_quad3_t regions=tiled_regions();
    sat_vdp1_texture_t bad=*regions.tiles[2];
    bad.width=16u;
    regions.tiles[2]=&bad;
    uint8_t submitted=99u;
    EQ(sat_draw_indexed_tiled_quad3(&q,&p,&regions,&submitted),
       SAT_ERR_INVALID_ARG);
    EQ(submitted,0u);
    EQ(g_project_calls,0);
    regions=tiled_regions();
    bad=*regions.tiles[0];
    bad.palette=7u;
    regions.tiles[0]=&bad;
    EQ(sat_draw_indexed_tiled_quad3(&q,&p,&regions,&submitted),
       SAT_ERR_INVALID_ARG);
    EQ(g_project_calls,0);
}
static void tiled_texture_propagates_capacity_without_faking_success() {
    reset();
    sat_quad3_t q=quad(3,12);
    const sat_indexed_solid_render3d_t p=render();
    const sat_indexed_tiled_quad3_t regions=tiled_regions();
    uint8_t submitted=99u;
    g_submit_status=SAT_ERR_CAPACITY;
    EQ(sat_draw_indexed_tiled_quad3(&q,&p,&regions,&submitted),
       SAT_ERR_CAPACITY);
    EQ(submitted,0u);
    EQ(g_opaque,1);
    reset();
    q.v[0].z=FX(7);
    g_submit_status=SAT_ERR_CAPACITY;
    EQ(sat_draw_indexed_tiled_quad3(&q,&p,&regions,&submitted),
       SAT_ERR_CAPACITY);
    EQ(submitted,0u);
    EQ(g_opaque,1);
}
static void mesh_uses_immutable_geometry_and_sorts_depth() {
    reset();
    sat_vec3_t vertices[8];
    uint16_t indices[8]={0,1,2,3,4,5,6,7};
    const sat_quad3_t far=quad(5,20),near=quad(2,10);
    for(int v=0;v<4;++v) { vertices[v]=far.v[v];vertices[v+4]=near.v[v]; }
    sat_mesh_t mesh={vertices,indices,8u,8u,2u,2u};
    uint16_t mats[2]={1u,0u},order[2]={0,0};
    uint32_t depths[2]={0,0};
    sat_indexed_solid_mesh3d_draw_t p=mesh_params(mats,order,depths);
    p.position=(sat_vec3_t){FX(7),0,0};
    const sat_vec3_t unchanged=vertices[0];
    EQ(sat_draw_indexed_solid_mesh3(&mesh,&p),SAT_OK);
    EQ(g_opaque,2);
    EQ(g_srca[0],20u);
    EQ(g_srca[1],10u);
    EQ(g_x[0],12u);
    EQ(g_x[1],9u);
    EQ(order[0],0u);
    EQ(order[1],1u);
    EQ(vertices[0].x,unchanged.x);
    EQ(vertices[0].z,unchanged.z);
}
static void mesh_invalid_face_or_material_is_atomic() {
    reset();
    sat_vec3_t vertices[8]={};
    uint16_t indices[8]={0,1,2,3,4,5,6,7};
    uint16_t mats[2]={0u,3u},order[2]={0,0};
    uint32_t depths[2]={0,0};
    sat_mesh_t mesh={vertices,indices,8u,8u,2u,2u};
    sat_indexed_solid_mesh3d_draw_t p=mesh_params(mats,order,depths);
    EQ(sat_draw_indexed_solid_mesh3(&mesh,&p),SAT_ERR_INVALID_ARG);
    EQ(g_near_calls,0);
    mats[1]=1u;
    indices[7]=8u;
    EQ(sat_draw_indexed_solid_mesh3(&mesh,&p),SAT_ERR_INVALID_ARG);
    EQ(g_near_calls,0);
    indices[7]=7u;
    p.order=nullptr;
    EQ(sat_draw_indexed_solid_mesh3(&mesh,&p),SAT_ERR_INVALID_ARG);
    EQ(g_near_calls,0);
}
static sat_indexed_box3_t box3() {
    sat_indexed_box3_t b={};
    b.top_center=(sat_vec3_t){FX(4),FX(4),FX(18)};
    b.half_extents=(sat_vec3_t){FX(2),FX(2),FX(3)};
    b.top_material=&g_texture[0];
    b.x_material=&g_texture[1];
    b.z_material=&g_texture[0];
    return b;
}
static void indexed_box_owns_visibility_winding_and_material_selection() {
    reset();
    const sat_indexed_box3_t b=box3();
    sat_indexed_solid_render3d_t p=render();
    p.eye=(sat_vec3_t){FX(20),FX(30),FX(40)};
    EQ(sat_draw_indexed_box3(&b,&p),SAT_OK);
    EQ(g_near_calls,3);
    EQ(g_opaque,3);
    EQ(g_srca[0],20u); /* camera-facing +X wall */
    EQ(g_srca[1],10u); /* camera-facing +Z wall */
    EQ(g_srca[2],10u); /* top */
    EQ(g_x[0],6u);
    EQ(g_x[1],6u);
    EQ(g_x[2],2u);

    reset();
    p.eye=(sat_vec3_t){FX(-20),FX(1),FX(0)};
    EQ(sat_draw_indexed_box3(&b,&p),SAT_OK);
    EQ(g_near_calls,2); /* no false lid below the platform top */
    EQ(g_opaque,2);
    EQ(g_x[0],2u); /* camera-facing -X wall */
    EQ(g_x[1],2u); /* camera-facing -Z wall */
}
static void indexed_box_rejects_bad_geometry_before_emitting() {
    reset();
    sat_indexed_box3_t b=box3();
    sat_indexed_solid_render3d_t p=render();
    b.half_extents.x=0;
    EQ(sat_draw_indexed_box3(&b,&p),SAT_ERR_INVALID_ARG);
    b=box3(); b.top_center.z=INT32_MAX;
    EQ(sat_draw_indexed_box3(&b,&p),SAT_ERR_INVALID_ARG);
    b=box3(); b.top_material=nullptr;
    EQ(sat_draw_indexed_box3(&b,&p),SAT_ERR_INVALID_ARG);
    b=box3();
    EQ(sat_draw_indexed_box3(nullptr,&p),SAT_ERR_INVALID_ARG);
    EQ(g_near_calls,0);
    EQ(g_opaque,0);
}
static void indexed_box_propagates_capacity_without_attempting_other_faces() {
    reset();
    const sat_indexed_box3_t b=box3();
    sat_indexed_solid_render3d_t p=render();
    g_submit_status=SAT_ERR_CAPACITY;
    EQ(sat_draw_indexed_box3(&b,&p),SAT_ERR_CAPACITY);
    EQ(g_opaque,1);
    EQ(g_near_calls,1);
}
int main() {
    quad_validates_and_draws_opaque();
    faded_quad_and_multiple_screen_triangles();
    invisible_and_hardware_errors();
    rgb_polygon_clips_like_solid_quads();
    patterned_texture_draws_only_with_original_four_corners();
    patterned_texture_invalid_input_and_command_failure();
    quadrant_upload_owns_correct_source_rows_and_palette();
    quadrant_upload_rejects_bad_source_and_short_scratch_atomically();
    tiled_texture_fast_path_submits_original_once();
    tiled_texture_preserves_actual_regions_at_near_boundary();
    tiled_texture_preserves_regions_at_screen_boundary();
    tiled_texture_rejects_inconsistent_region_dimensions();
    tiled_texture_propagates_capacity_without_faking_success();
    mesh_uses_immutable_geometry_and_sorts_depth();
    mesh_invalid_face_or_material_is_atomic();
    indexed_box_owns_visibility_winding_and_material_selection();
    indexed_box_rejects_bad_geometry_before_emitting();
    indexed_box_propagates_capacity_without_attempting_other_faces();
    puts("test_render3d_indexed: 18 tests passed");
    return 0;
}
