#include <cassert>
#include <cstdio>
#include <stdint.h>
#include "saturn/scene3d_faces.h"

static uint16_t emitted[64]={};
static uint16_t emitted_count=0;
static uint16_t project_calls=0;
static uint16_t projected_vertices=0;
static uint16_t clipped_count=0;

extern "C" void sat_vec3_normalize(sat_vec3_t* out,const sat_vec3_t* in) {
    *out=*in;
    if (in->z>0 && !in->x && !in->y) out->z=SAT_FX16_ONE;
    else if (in->z<0 && !in->x && !in->y) out->z=-SAT_FX16_ONE;
}
extern "C" sat_result_t sat_mat4_transform_vec4(
    const sat_mat4_t* matrix,const sat_vec4_t* v,sat_vec4_t* out) {
    *out=*v;
    out->x+=matrix->m[3];out->y+=matrix->m[7];out->z+=matrix->m[11];
    return SAT_OK;
}
extern "C" sat_result_t sat_project_vertices(
    const sat_mat4_t* vp, const sat_vec3_t* points,
    uint16_t count, sat_projected_vertex_t* out) {
    ++project_calls;
    projected_vertices=static_cast<uint16_t>(projected_vertices+count);
    for (uint16_t i=0;i<count;++i) {
        out[i].x=static_cast<int16_t>(points[i].x>>16);
        out[i].y=static_cast<int16_t>(points[i].y>>16);
        out[i].w=vp->m[0]<0 ? -points[i].z : points[i].z;
    }
    return SAT_OK;
}
extern "C" sat_result_t sat_draw_sprite_distorted(
    const sat_distorted_sprite_cmd_t* cmd) {
    emitted[emitted_count++]=cmd->texture->srca;
    return SAT_OK;
}
extern "C" sat_result_t sat_draw_sprite_distorted_color_calc(
    const sat_distorted_sprite_cmd_t* cmd,uint8_t slot) {
    assert(slot<8u);
    emitted[emitted_count++]=cmd->texture->srca;
    return SAT_OK;
}
extern "C" sat_result_t sat_draw_quad2_polygon(
    const sat_quad2_t*,uint16_t color) {
    emitted[emitted_count++]=color;
    return SAT_OK;
}
extern "C" sat_result_t sat_draw_indexed_solid_quad3(
    const sat_quad3_t*,const sat_indexed_solid_render3d_t*,
    const sat_vdp1_texture_t* tex) {
    ++clipped_count;
    emitted[emitted_count++]=tex->srca;
    return SAT_OK;
}
extern "C" sat_result_t sat_draw_indexed_textured_quad3(
    const sat_quad3_t*,const sat_indexed_solid_render3d_t*,
    const sat_vdp1_texture_t* tex,uint8_t* drawn) {
    if(drawn) *drawn=1u;
    emitted[emitted_count++]=tex->srca;
    return SAT_OK;
}
extern "C" sat_result_t sat_draw_indexed_tiled_quad3(
    const sat_quad3_t*,const sat_indexed_solid_render3d_t*,
    const sat_indexed_tiled_quad3_t* tiled,uint8_t* drawn) {
    if(drawn) *drawn=1u;
    emitted[emitted_count++]=tiled->full->srca;
    return SAT_OK;
}
extern "C" sat_result_t sat_indexed_box3_faces(
    const sat_indexed_box3_t* box,const sat_vec3_t*,
    sat_quad3_t faces[3],const sat_vdp1_texture_t* tex[3],uint8_t* count) {
    assert(box && faces && tex && count);
    *count=2u;
    for(uint8_t i=0;i<2u;++i) {
        tex[i]=i?box->z_material:box->x_material;
        for(uint8_t v=0;v<4u;++v) faces[i].v[v]=(sat_vec3_t){0,0,10*SAT_FX16_ONE};
    }
    return SAT_OK;
}
static sat_quad3_t quad(int32_t z) {
    const sat_fx16_t depth=static_cast<sat_fx16_t>(z*SAT_FX16_ONE);
    const sat_fx16_t unit=SAT_FX16_ONE;
    const sat_quad3_t q={{{-unit,-unit,depth},
                          {unit,-unit,depth},
                          {unit,unit,depth},
                          {-unit,unit,depth}}};
    return q;
}
int main() {
    sat_scene3d_faces_t scene={};
    sat_scene3d_face_t storage[8]={};
    uint32_t keys[8]={};
    uint16_t order[8]={};
    sat_mat4_t vp={};
    const sat_vec3_t eye={0,0,0};
    const sat_vec3_t forward={0,0,SAT_FX16_ONE};
    vp.m[0]=SAT_FX16_ONE;
    sat_vdp1_texture_t far_tex={};
    sat_vdp1_texture_t near_tex={};
    sat_vdp1_texture_t actor_tex={};
    far_tex.valid=near_tex.valid=actor_tex.valid=1u;
    far_tex.srca=10u;near_tex.srca=20u;actor_tex.srca=30u;
    const sat_scene3d_material_t far_mat={
        SAT_SCENE3D_INDEXED_SOLID,0u,&far_tex,nullptr,SAT_INDEXED_SOLID_OPAQUE};
    const sat_scene3d_material_t near_mat={
        SAT_SCENE3D_INDEXED_SOLID,0u,&near_tex,nullptr,SAT_INDEXED_SOLID_OPAQUE};
    const sat_scene3d_material_t actor_mat={
        SAT_SCENE3D_INDEXED_SOLID,0u,&actor_tex,nullptr,SAT_INDEXED_SOLID_OPAQUE};
    assert(sat_scene3d_faces_init(&scene,storage,keys,order,8u)==SAT_OK);
    assert(sat_scene3d_faces_init(&scene,storage,nullptr,order,8u)==
           SAT_ERR_INVALID_ARG);
    assert(sat_scene3d_faces_init(&scene,storage,keys,nullptr,8u)==
           SAT_ERR_INVALID_ARG);
    assert(sat_scene3d_faces_init(&scene,storage,keys,order,8u)==SAT_OK);
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_ERR_INVALID_ARG);
    const sat_quad3_t far=quad(10),near=quad(2),actor=quad(6);
    // Two platform faces bracket a character; all share the SAME pass.
    assert(sat_scene3d_faces_submit_quad(&scene,&near,&near_mat,0u)==SAT_OK);
    assert(sat_scene3d_faces_submit_quad(&scene,&actor,&actor_mat,0u)==SAT_OK);
    assert(sat_scene3d_faces_submit_quad(&scene,&far,&far_mat,0u)==SAT_OK);
    sat_fx16_t world_depth=0;
    const sat_vec3_t depth_point={0,0,10*SAT_FX16_ONE};
    assert(sat_scene3d_faces_depth(&scene,&depth_point,&world_depth)==SAT_OK);
    assert(world_depth==10*SAT_FX16_ONE);
    assert(sat_scene3d_faces_flush(&scene)==SAT_OK);
    assert(sat_scene3d_faces_depth(&scene,&depth_point,&world_depth)==SAT_ERR_INVALID_ARG);
    assert(emitted_count==3u && emitted[0]==10u &&
           emitted[1]==30u && emitted[2]==20u);
    assert(sat_scene3d_faces_flush(&scene)==SAT_ERR_INVALID_ARG);
    emitted_count=0;
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    sat_vec3_t local[8]={
        {-SAT_FX16_ONE,-SAT_FX16_ONE,0},
        {SAT_FX16_ONE,-SAT_FX16_ONE,0},
        {SAT_FX16_ONE,SAT_FX16_ONE,0},
        {-SAT_FX16_ONE,SAT_FX16_ONE,0},
        {-SAT_FX16_ONE,-SAT_FX16_ONE,4*SAT_FX16_ONE},
        {SAT_FX16_ONE,-SAT_FX16_ONE,4*SAT_FX16_ONE},
        {SAT_FX16_ONE,SAT_FX16_ONE,4*SAT_FX16_ONE},
        {-SAT_FX16_ONE,SAT_FX16_ONE,4*SAT_FX16_ONE}};
    uint16_t indices[8]={0u,1u,2u,3u,4u,5u,6u,7u};
    sat_mesh_t mesh={local,indices,8u,8u,2u,2u};
    const sat_scene3d_material_t materials[2]={near_mat,far_mat};
    const uint16_t per_face[2]={0u,1u};
    sat_projected_vertex_t screen[8]={};
    sat_vec3_t world[8]={};
    sat_mat4_t translation={};
    translation.m[11]=3*SAT_FX16_ONE;
    const sat_scene3d_instance_t instance={
        &mesh,materials,2u,per_face,&translation,0u,0u};
    const uint16_t calls_before=project_calls;
    assert(sat_scene3d_faces_submit_instance(
        &scene,&instance,screen,world)==SAT_OK);
    assert(project_calls==calls_before+1u);
    assert(local[0].z==0);
    assert(scene.count==2u);
    world[0].z=99*SAT_FX16_ONE; // queued faces own their world-space copies
    assert(scene.entries[0].world.v[0].z==3*SAT_FX16_ONE);
    assert(sat_scene3d_faces_flush(&scene)==SAT_OK);
    assert(emitted_count==2u && emitted[0]==10u && emitted[1]==20u);
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    const uint16_t bad_indices[2]={0u,2u};
    sat_scene3d_instance_t invalid_instance=instance;
    invalid_instance.face_materials=bad_indices;
    assert(sat_scene3d_faces_submit_instance(
        &scene,&invalid_instance,screen,world)==SAT_ERR_INVALID_ARG);
    assert(scene.count==0u);
    assert(sat_scene3d_faces_flush(&scene)==SAT_OK);
    sat_camera3d_t camera={};
    camera.eye=eye;camera.target=depth_point;camera.view_proj=vp;
    assert(sat_scene3d_faces_begin_camera(
        &scene,&camera,SAT_FX16_ONE,320u,224u)==SAT_OK);
    assert(sat_scene3d_faces_depth(&scene,&depth_point,&world_depth)==SAT_OK);
    assert(world_depth==10*SAT_FX16_ONE);
    assert(sat_scene3d_faces_flush(&scene)==SAT_OK);
    camera.target=camera.eye;
    assert(sat_scene3d_faces_begin_camera(
        &scene,&camera,SAT_FX16_ONE,320u,224u)==SAT_ERR_INVALID_ARG);

    // Paint order, end to end: one platform has a face BEHIND and another IN
    // FRONT of the actor, which sorting object anchors cannot produce. Equal
    // depths keep submission order, and a higher pass always paints last.
    emitted_count=0;
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    sat_vdp1_texture_t order_tex[6]={};
    sat_scene3d_material_t order_mat[6]={};
    const int32_t order_depth[6]={10,2,6,7,7,30};
    const uint16_t order_pass[6]={0u,0u,0u,0u,0u,1u};
    for (uint16_t i=0;i<6u;++i) {
        order_tex[i].valid=1u;
        order_tex[i].srca=static_cast<uint16_t>(100u+i);
        order_mat[i].kind=SAT_SCENE3D_INDEXED_SOLID;
        order_mat[i].texture=&order_tex[i];
        order_mat[i].color_calc_slot=SAT_INDEXED_SOLID_OPAQUE;
        const sat_quad3_t q=quad(order_depth[i]);
        assert(sat_scene3d_faces_submit_quad(
            &scene,&q,&order_mat[i],order_pass[i])==SAT_OK);
    }
    assert(sat_scene3d_faces_flush(&scene)==SAT_OK);
    assert(emitted_count==6u);
    assert(emitted[0]==100u); // depth 10, farthest
    assert(emitted[1]==103u && emitted[2]==104u); // depth 7, submission order
    assert(emitted[3]==102u); // depth 6
    assert(emitted[4]==101u); // depth 2, nearest of pass 0
    assert(emitted[5]==105u); // pass 1 paints last despite being farthest

    std::puts("scene3d_faces api: OK");
    return 0;
}
