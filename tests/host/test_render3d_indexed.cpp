/* Tests for the renderer-owned INDEX8 solid-geometry path. These stubs
 * exercise submission, validation and painter order without Saturn hardware. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "saturn/mesh3d.h"
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

static void reset() {
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
int main() {
    quad_validates_and_draws_opaque();
    faded_quad_and_multiple_screen_triangles();
    invisible_and_hardware_errors();
    mesh_uses_immutable_geometry_and_sorts_depth();
    mesh_invalid_face_or_material_is_atomic();
    puts("test_render3d_indexed: 5 tests passed");
    return 0;
}
