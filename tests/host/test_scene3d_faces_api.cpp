#include <cassert>
#include <cstdio>
#include <stdint.h>
#include "saturn/scene3d_faces.h"

static uint16_t emitted[64]={};
/* The slot each emission carried, so an instance-wide override can be checked
 * face by face rather than only "it drew something". */
static uint8_t emitted_slot[64]={};
static uint16_t emitted_count=0;
static uint16_t project_calls=0;
static uint16_t project_fail_call=0u;
static uint16_t projected_vertices=0;
static uint16_t clipped_count=0;
static sat_result_t distorted_draw_status=SAT_OK;
static sat_result_t polygon_draw_status=SAT_OK;

static bool same_texture(const sat_vdp1_texture_t* a,
                         const sat_vdp1_texture_t* b) {
    if (a == nullptr || b == nullptr) return a == b;
    return a->srca == b->srca && a->width == b->width &&
           a->height == b->height && a->palette == b->palette &&
           a->valid == b->valid;
}

static bool same_tiled(const sat_indexed_tiled_quad3_t* a,
                       const sat_indexed_tiled_quad3_t* b) {
    if (a == nullptr || b == nullptr) return a == b;
    if (!same_texture(a->full,b->full)) return false;
    for (uint8_t i=0u;i<4u;++i)
        if (!same_texture(a->tiles[i],b->tiles[i])) return false;
    return true;
}

static bool same_face(const sat_scene3d_face_t& a,
                      const sat_scene3d_face_t& b) {
    if (a.projected_safe != b.projected_safe ||
        a.cached_projected != b.cached_projected ||
        a.gouraud_valid != b.gouraud_valid ||
        a.material.kind != b.material.kind ||
        a.material.rgb555 != b.material.rgb555 ||
        a.material.color_calc_slot != b.material.color_calc_slot ||
        a.material.vertex_gouraud != nullptr ||
        b.material.vertex_gouraud != nullptr ||
        !same_texture(a.material.texture, b.material.texture) ||
        !same_tiled(a.material.tiled, b.material.tiled)) return false;
    for (uint8_t i=0u;i<4u;++i) {
        // world is carried only for the clipping fallback.
        if ((!a.projected_safe &&
             (a.world.v[i].x != b.world.v[i].x ||
              a.world.v[i].y != b.world.v[i].y ||
              a.world.v[i].z != b.world.v[i].z)) ||
            a.projected.x[i] != b.projected.x[i] ||
            a.projected.y[i] != b.projected.y[i] ||
            (a.gouraud_valid != 0u && a.gouraud[i] != b.gouraud[i])) return false;
    }
    return true;
}

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
    if(project_fail_call && project_calls==project_fail_call)
        return SAT_ERR_UNSUPPORTED;
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
    if (distorted_draw_status!=SAT_OK) return distorted_draw_status;
    emitted_slot[emitted_count]=SAT_INDEXED_SOLID_OPAQUE;
    emitted[emitted_count++]=cmd->texture->srca;
    return SAT_OK;
}
extern "C" sat_result_t sat_draw_sprite_distorted_color_calc(
    const sat_distorted_sprite_cmd_t* cmd,uint8_t slot) {
    assert(slot<8u);
    emitted_slot[emitted_count]=slot;
    emitted[emitted_count++]=cmd->texture->srca;
    return SAT_OK;
}
extern "C" sat_result_t sat_draw_quad2_polygon(
    const sat_quad2_t*,uint16_t color) {
    if (polygon_draw_status!=SAT_OK) return polygon_draw_status;
    emitted[emitted_count++]=color;
    return SAT_OK;
}
extern "C" sat_result_t sat_draw_quad2_polygon_gouraud(
    const sat_quad2_t*, uint16_t color, const uint16_t[4]) {
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
extern "C" sat_result_t sat_draw_polygon_quad3(
    const sat_quad3_t*,const sat_indexed_solid_render3d_t*,uint16_t rgb555) {
    ++clipped_count;
    emitted[emitted_count++]=rgb555;
    return SAT_OK;
}
extern "C" sat_result_t sat_draw_polygon_quad3_gouraud(
    const sat_quad3_t*,const sat_indexed_solid_render3d_t*,uint16_t rgb555,
    const uint16_t*) {
    ++clipped_count;
    emitted[emitted_count++]=rgb555;
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
static uint16_t verified_owner_generation=0u;
static sat_result_t validate_owner_for_test(
    void*,uint16_t slot,uint16_t generation,
    const sat_vdp1_texture_t* expected) {
    return slot==0u && generation==verified_owner_generation &&
           expected && expected->valid ? SAT_OK:SAT_ERR_INVALID_ARG;
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
        SAT_SCENE3D_INDEXED_SOLID,0u,&far_tex,nullptr,
        SAT_INDEXED_SOLID_OPAQUE,nullptr};
    const sat_scene3d_material_t near_mat={
        SAT_SCENE3D_INDEXED_SOLID,0u,&near_tex,nullptr,
        SAT_INDEXED_SOLID_OPAQUE,nullptr};
    const sat_scene3d_material_t actor_mat={
        SAT_SCENE3D_INDEXED_SOLID,0u,&actor_tex,nullptr,
        SAT_INDEXED_SOLID_OPAQUE,nullptr};
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

    // Cached RGB quads and dynamic world faces enter the SAME ordered
    // painter list; the supplied linear depth is in the same 16.16 space
    // as world-face projected W (not the cache's arbitrary bake sort key).
    emitted_count=0u;
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    const sat_quad2_t cached_quad{{-5,5,5,-5},{-5,-5,5,5}};
    assert(sat_scene3d_faces_submit_quad(
        &scene,&near,&near_mat,0u)==SAT_OK);
    assert(sat_scene3d_faces_submit_projected_rgb(
        &scene,&cached_quad,8*SAT_FX16_ONE,0x8ABCu,0u)==SAT_OK);
    assert(sat_scene3d_faces_submit_quad(
        &scene,&actor,&actor_mat,0u)==SAT_OK);
    assert(sat_scene3d_faces_submit_quad(
        &scene,&far,&far_mat,0u)==SAT_OK);
    assert(scene.count==4u);
    assert(sat_scene3d_faces_flush(&scene)==SAT_OK);
    assert(emitted_count==4u && emitted[0]==10u &&
           emitted[1]==0x8ABCu && emitted[2]==30u &&
           emitted[3]==20u);
    assert(scene.emitted_faces==3u && scene.emitted_cached==1u);
    assert(scene.fallback_faces==0u && scene.skipped_faces==0u);

    // A preprojected indexed sprite occupies the SAME painter depth interval
    // as dynamic world actors, with no redundant projection at flush.
    emitted_count=0u;
    project_calls=0u;
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    sat_scene3d_material_t cached_mat=actor_mat;
    cached_mat.kind=SAT_SCENE3D_INDEXED_TEXTURED;
    cached_mat.color_calc_slot=2u;
    const uint16_t cached_texture_srca=actor_tex.srca;
    assert(sat_scene3d_faces_submit_quad(
        &scene,&near,&near_mat,0u)==SAT_OK);
    assert(sat_scene3d_faces_submit_projected_material(
        &scene,&cached_quad,8*SAT_FX16_ONE,&cached_mat,0u)==SAT_OK);
    cached_mat.texture=nullptr; // the queued descriptor was copied
    assert(sat_scene3d_faces_submit_quad(
        &scene,&actor,&actor_mat,0u)==SAT_OK);
    assert(sat_scene3d_faces_submit_quad(
        &scene,&far,&far_mat,0u)==SAT_OK);
    assert(project_calls==3u);
    assert(sat_scene3d_faces_flush(&scene)==SAT_OK);
    assert(project_calls==3u);
    assert(emitted_count==4u && emitted[0]==10u &&
           emitted[1]==cached_texture_srca &&
           emitted[2]==30u && emitted[3]==20u);
    assert(emitted_slot[1]==2u && emitted_slot[2]==SAT_INDEXED_SOLID_OPAQUE);
    assert(scene.emitted_faces==3u && scene.emitted_cached==1u);

    // The same cached quad can draw the preuploaded full image of a tiled
    // material; it never performs projected-UV re-clipping.
    emitted_count=0u;
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    sat_indexed_tiled_quad3_t tiled{};
    tiled.full=&near_tex;
    const sat_scene3d_material_t tiled_material={
        SAT_SCENE3D_INDEXED_TILED,0u,nullptr,&tiled,3u,nullptr};
    assert(sat_scene3d_faces_submit_projected_material(
        &scene,&cached_quad,3*SAT_FX16_ONE,&tiled_material,0u)==SAT_OK);
    assert(scene.entries[0].material.kind==SAT_SCENE3D_INDEXED_TEXTURED);
    assert(scene.entries[0].material.texture==&near_tex);
    assert(scene.entries[0].material.tiled==nullptr);
    /* The original tiled descriptor may be reused before flush. Its copied
     * full texture is still valid, so the queued sprite must draw unchanged. */
    tiled.full=nullptr;
    assert(sat_scene3d_faces_flush(&scene)==SAT_OK);
    assert(emitted_count==1u && emitted[0]==near_tex.srca);
    assert(emitted_slot[0]==3u && scene.emitted_cached==1u);

    // A cached texture can be evicted AFTER submission. Preflight must
    // reject the entire frame before drawing even the valid farther wall,
    // not issue one hardware command and then discover stale residency.
    emitted_count=0u;
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    assert(sat_scene3d_faces_submit_quad(
        &scene,&far,&far_mat,0u)==SAT_OK);
    assert(sat_scene3d_faces_submit_projected_material(
        &scene,&cached_quad,3*SAT_FX16_ONE,&actor_mat,0u)==SAT_OK);
    actor_tex.valid=0u;
    assert(sat_scene3d_faces_flush(&scene)==SAT_ERR_INVALID_ARG);
    assert(emitted_count==0u && scene.emitted_faces==0u &&
           scene.emitted_cached==0u && scene.count==0u && !scene.active);
    actor_tex.valid=1u;

    // Projected tiled material was normalized at submit. Its full texture
    // is still borrowed and must remain resident until the queued flush.
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    tiled.full=&near_tex;
    assert(sat_scene3d_faces_submit_projected_material(
        &scene,&cached_quad,3*SAT_FX16_ONE,&tiled_material,0u)==SAT_OK);
    tiled.full=nullptr;
    near_tex.valid=0u;
    assert(sat_scene3d_faces_flush(&scene)==SAT_ERR_INVALID_ARG);
    assert(emitted_count==0u && scene.emitted_cached==0u);
    near_tex.valid=1u;

    // Prepared/world faces use the SAME material validity gate.
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    assert(sat_scene3d_faces_submit_quad(
        &scene,&far,&far_mat,0u)==SAT_OK);
    far_tex.valid=0u;
    assert(sat_scene3d_faces_flush(&scene)==SAT_ERR_INVALID_ARG);
    assert(emitted_count==0u && scene.emitted_faces==0u);
    far_tex.valid=1u;

    // Invalid texture/slot/Gouraud descriptors never enter cache storage.
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    cached_mat=actor_mat;
    cached_mat.texture=nullptr;
    assert(sat_scene3d_faces_submit_projected_material(
        &scene,&cached_quad,SAT_FX16_ONE,&cached_mat,0u)==SAT_ERR_INVALID_ARG);
    cached_mat=actor_mat;
    cached_mat.color_calc_slot=8u;
    assert(sat_scene3d_faces_submit_projected_material(
        &scene,&cached_quad,SAT_FX16_ONE,&cached_mat,0u)==SAT_ERR_INVALID_ARG);
    cached_mat=actor_mat;
    uint16_t gouraud[4]={1u,2u,3u,4u};
    cached_mat.vertex_gouraud=gouraud;
    assert(sat_scene3d_faces_submit_projected_material(
        &scene,&cached_quad,SAT_FX16_ONE,&cached_mat,0u)==SAT_ERR_INVALID_ARG);
    assert(sat_scene3d_faces_submit_projected_material(
        &scene,&cached_quad,SAT_FX16_ONE,nullptr,0u)==SAT_ERR_INVALID_ARG);
    assert(scene.count==0u);
    assert(sat_scene3d_faces_flush(&scene)==SAT_OK);

    // Equal-depth ties remain stable in original submission order.
    emitted_count=0u;
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    assert(sat_scene3d_faces_submit_projected_rgb(
        &scene,&cached_quad,6*SAT_FX16_ONE,0x8001u,0u)==SAT_OK);
    assert(sat_scene3d_faces_submit_quad(
        &scene,&actor,&actor_mat,0u)==SAT_OK);
    assert(sat_scene3d_faces_submit_projected_rgb(
        &scene,&cached_quad,6*SAT_FX16_ONE,0x8002u,0u)==SAT_OK);
    assert(sat_scene3d_faces_flush(&scene)==SAT_OK);
    assert(emitted_count==3u && emitted[0]==0x8001u &&
           emitted[1]==30u && emitted[2]==0x8002u);
    assert(scene.emitted_faces==1u && scene.emitted_cached==2u);

    // Invalid cached requests cannot consume face capacity. A failed
    // polygon emission must not be counted as a successful cache replay.
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    assert(sat_scene3d_faces_submit_projected_rgb(
        &scene,nullptr,SAT_FX16_ONE,3u,0u)==SAT_ERR_INVALID_ARG);
    assert(sat_scene3d_faces_submit_projected_rgb(
        &scene,&cached_quad,-1,3u,0u)==SAT_ERR_INVALID_ARG);
    assert(sat_scene3d_faces_submit_projected_rgb(
        &scene,&cached_quad,SAT_FX16_ONE,3u,
        SAT_SCENE3D_PASS_MAX+1u)==SAT_ERR_INVALID_ARG);
    assert(scene.count==0u);
    for (uint16_t i=0u;i<8u;++i)
        assert(sat_scene3d_faces_submit_projected_rgb(
            &scene,&cached_quad,SAT_FX16_ONE,i,0u)==SAT_OK);
    assert(sat_scene3d_faces_submit_projected_rgb(
        &scene,&cached_quad,SAT_FX16_ONE,9u,0u)==SAT_ERR_CAPACITY);
    polygon_draw_status=SAT_ERR_CAPACITY;
    assert(sat_scene3d_faces_flush(&scene)==SAT_ERR_CAPACITY);
    assert(scene.emitted_cached==0u && scene.emitted_faces==0u);
    polygon_draw_status=SAT_OK;

    // Physical rejection of a cached indexed sprite does not count it as
    // emitted; no RGB fallback may falsely turn the failure into success.
    emitted_count=0u;
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    assert(sat_scene3d_faces_submit_projected_material(
        &scene,&cached_quad,3*SAT_FX16_ONE,&actor_mat,0u)==SAT_OK);
    distorted_draw_status=SAT_ERR_CAPACITY;
    assert(sat_scene3d_faces_flush(&scene)==SAT_ERR_CAPACITY);
    assert(scene.emitted_cached==0u && scene.emitted_faces==0u);
    assert(emitted_count==0u);
    distorted_draw_status=SAT_OK;

    // An indexed face outside the safe projected range takes the bounded
    // renderer fallback rather than silently pretending the clamped sprite
    // is equivalent. The diagnostic distinguishes that path from culling.
    emitted_count=0;
    clipped_count=0;
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    sat_quad3_t oversized=quad(2);
    for (uint8_t i=0u;i<4u;++i) oversized.v[i].x=4000*SAT_FX16_ONE;
    assert(sat_scene3d_faces_submit_quad(
        &scene,&oversized,&near_mat,0u)==SAT_OK);
    assert(scene.count==1u && scene.clipped_faces==1u);
    assert(sat_scene3d_faces_flush(&scene)==SAT_OK);
    assert(scene.fallback_faces==1u && clipped_count==1u);

    // A fully hidden face is counted as culled and never enters storage.
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    const sat_quad3_t hidden=quad(-2);
    assert(sat_scene3d_faces_submit_quad(
        &scene,&hidden,&near_mat,0u)==SAT_OK);
    assert(scene.count==0u && scene.culled_faces==1u);
    assert(sat_scene3d_faces_flush(&scene)==SAT_OK);

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
        &scene,&instance,SAT_SCENE3D_SLOT_INHERIT,screen,world)==SAT_OK);
    assert(project_calls==calls_before+1u);
    assert(local[0].z==0);
    assert(scene.count==2u);
    // Projected-safe faces keep only screen corners: the caller's world
    // scratch is free to change after submission.
    assert(scene.entries[0].projected_safe==1u);
    world[0].z=99*SAT_FX16_ONE;
    assert(sat_scene3d_faces_flush(&scene)==SAT_OK);
    assert(emitted_count==2u && emitted[0]==10u && emitted[1]==20u);
    // INHERIT left both materials on their own opaque slot.
    assert(emitted_slot[0]==SAT_INDEXED_SOLID_OPAQUE &&
           emitted_slot[1]==SAT_INDEXED_SOLID_OPAQUE);

    // A distance-faded object fades WHOLE: one slot reaches every face of the
    // instance, without the shared material table needing a copy per slot.
    emitted_count=0;
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    assert(sat_scene3d_faces_submit_instance(
        &scene,&instance,3u,screen,world)==SAT_OK);
    assert(scene.count==2u);
    assert(sat_scene3d_faces_flush(&scene)==SAT_OK);
    assert(emitted_count==2u);
    assert(emitted_slot[0]==3u && emitted_slot[1]==3u);
    // The caller's material table is untouched by the override.
    assert(materials[0].color_calc_slot==SAT_INDEXED_SOLID_OPAQUE &&
           materials[1].color_calc_slot==SAT_INDEXED_SOLID_OPAQUE);

    // An RGB material has no indexed palette to blend, so a slot override on
    // one is rejected up front rather than reaching the hardware path, and
    // rejection stays atomic: nothing is queued.
    emitted_count=0;
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    const sat_scene3d_material_t rgb_materials[2]={
        {SAT_SCENE3D_RGB,0x1234u,nullptr,nullptr,SAT_INDEXED_SOLID_OPAQUE,nullptr},
        {SAT_SCENE3D_RGB,0x1234u,nullptr,nullptr,SAT_INDEXED_SOLID_OPAQUE,nullptr}};
    sat_scene3d_instance_t rgb_instance=instance;
    rgb_instance.materials=rgb_materials;
    assert(sat_scene3d_faces_submit_instance(
        &scene,&rgb_instance,3u,screen,world)==SAT_ERR_INVALID_ARG);
    assert(scene.count==0u);
    // The same instance is legal while it keeps its own opaque slot.
    assert(sat_scene3d_faces_submit_instance(
        &scene,&rgb_instance,SAT_SCENE3D_SLOT_INHERIT,screen,world)==SAT_OK);
    assert(scene.count==2u);
    assert(sat_scene3d_faces_flush(&scene)==SAT_OK);

    emitted_count=0;
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    const uint16_t bad_indices[2]={0u,2u};
    sat_scene3d_instance_t invalid_instance=instance;
    invalid_instance.face_materials=bad_indices;
    assert(sat_scene3d_faces_submit_instance(
        &scene,&invalid_instance,SAT_SCENE3D_SLOT_INHERIT,
        screen,world)==SAT_ERR_INVALID_ARG);
    assert(scene.count==0u);
    sat_mesh_t malformed_mesh=mesh;
    sat_scene3d_instance_t malformed_instance=instance;
    malformed_instance.mesh=&malformed_mesh;
    malformed_mesh.vertex_cap=7u;
    assert(sat_scene3d_faces_submit_instance(
        &scene,&malformed_instance,SAT_SCENE3D_SLOT_INHERIT,
        screen,world)==SAT_ERR_INVALID_ARG);
    malformed_mesh=mesh;malformed_mesh.face_cap=1u;
    assert(sat_scene3d_faces_submit_instance(
        &scene,&malformed_instance,SAT_SCENE3D_SLOT_INHERIT,
        screen,world)==SAT_ERR_INVALID_ARG);
    malformed_mesh=mesh;malformed_mesh.vertex_count=0u;
    assert(sat_scene3d_faces_submit_instance(
        &scene,&malformed_instance,SAT_SCENE3D_SLOT_INHERIT,
        screen,world)==SAT_ERR_INVALID_ARG);
    malformed_mesh=mesh;malformed_mesh.face_count=0u;
    assert(sat_scene3d_faces_submit_instance(
        &scene,&malformed_instance,SAT_SCENE3D_SLOT_INHERIT,
        screen,world)==SAT_ERR_INVALID_ARG);
    assert(scene.count==0u && scene.culled_faces==0u &&
           scene.clipped_faces==0u);
    assert(sat_scene3d_faces_flush(&scene)==SAT_OK);
    emitted_count=0;
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

    // Far faces still order by depth: 300 and 1000 units apart once tied
    // (the key saturated at 256 units), leaving submission order.
    emitted_count=0;
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    const int32_t far_depth[2]={300,1000};
    for (uint16_t i=0;i<2u;++i) {
        const sat_quad3_t q=quad(far_depth[i]);
        assert(sat_scene3d_faces_submit_quad(
            &scene,&q,&order_mat[i],0u)==SAT_OK);
    }
    assert(sat_scene3d_faces_flush(&scene)==SAT_OK);
    assert(emitted_count==2u);
    assert(emitted[0]==101u && emitted[1]==100u); // 1000 units paints first

    // Batch preparation uses the same instance algorithm and can be merged
    // into the canonical queue without sorting or emitting independently.
    sat_scene3d_face_t prepared_faces[8]={};
    uint32_t prepared_keys[8]={};

    sat_scene3d_prepare_item_t prepared_item={
        &instance,screen,world,SAT_SCENE3D_SLOT_INHERIT,0u};
    sat_scene3d_prepare_batch_t batch={};
    assert(sat_scene3d_prepare_batch_init(
        &batch,&prepared_item,1u,prepared_faces,prepared_keys,
        8u)==SAT_OK);
    batch.view_proj=vp;
    batch.eye=eye;
    batch.forward=forward;
    batch.near_depth=SAT_FX16_ONE;
    batch.width=320u;
    batch.height=224u;
    assert(sat_scene3d_prepare_batch_execute(&batch)==SAT_OK);
    assert(batch.metrics.source_faces==2u &&
           batch.metrics.prepared_faces==2u && batch.metrics.culled_faces==0u);
    sat_scene3d_face_t reference_faces[8]={};
    uint32_t reference_keys[8]={};

    sat_scene3d_prepare_batch_t reference_batch={};
    assert(sat_scene3d_prepare_batch_init(
        &reference_batch,&prepared_item,1u,reference_faces,reference_keys,
        8u)==SAT_OK);
    reference_batch.view_proj=vp;
    reference_batch.eye=eye;
    reference_batch.forward=forward;
    reference_batch.near_depth=SAT_FX16_ONE;
    reference_batch.width=320u;
    reference_batch.height=224u;
    assert(sat_scene3d_prepare_batch_execute(&reference_batch)==SAT_OK);
    assert(reference_batch.metrics.source_faces==batch.metrics.source_faces);
    assert(reference_batch.metrics.prepared_faces==batch.metrics.prepared_faces);
    assert(reference_batch.metrics.culled_faces==batch.metrics.culled_faces);
    assert(reference_batch.metrics.clipped_faces==batch.metrics.clipped_faces);
    for (uint16_t i=0u;i<batch.metrics.prepared_faces;++i) {
        assert(batch.keys[i]==reference_keys[i]);
        assert(same_face(batch.faces[i],reference_faces[i]));
    }

    // Partitioning is a bounded source-order view: the selected descriptors
    // inherit camera state, while output storage and metrics stay private to
    // the slice that a second executor may prepare.
    sat_mat4_t source_worlds[3]={translation,translation,translation};
    sat_scene3d_instance_t source_instances[3]={instance,instance,instance};
    sat_projected_vertex_t source_screen[3][8]={};
    sat_vec3_t source_world_vertices[3][8]={};
    sat_scene3d_prepare_item_t source_items[3]={};
    for(uint8_t i=0u;i<3u;++i) {
        source_worlds[i].m[11]=(3u+3u*i)*SAT_FX16_ONE;
        source_instances[i].world=&source_worlds[i];
        source_items[i]=(sat_scene3d_prepare_item_t){
            &source_instances[i],source_screen[i],source_world_vertices[i],
            SAT_SCENE3D_SLOT_INHERIT,0u};
    }
    sat_scene3d_face_t source_faces[8]={};
    uint32_t source_keys[8]={};

    sat_scene3d_prepare_batch_t source_batch={};
    assert(sat_scene3d_prepare_batch_init(
        &source_batch,source_items,3u,source_faces,source_keys,
        8u)==SAT_OK);
    source_batch.view_proj=vp;
    source_batch.eye=eye;
    source_batch.forward=forward;
    source_batch.near_depth=SAT_FX16_ONE;
    source_batch.width=320u;
    source_batch.height=224u;
    assert(sat_scene3d_prepare_batch_execute(&source_batch)==SAT_OK);
    assert(source_batch.metrics.prepared_faces==6u);
    sat_scene3d_prepare_item_t slice_items[2]={};
    sat_scene3d_face_t slice_faces[8]={};
    uint32_t slice_keys[8]={};

    sat_scene3d_prepare_batch_t slice={};
    source_batch.dispatch=SAT_SCENE3D_PREPARE_DISPATCH_MASTER;
    assert(sat_scene3d_prepare_batch_slice(
        &source_batch,1u,2u,slice_items,slice_faces,slice_keys,
        8u,&slice)==SAT_OK);
    assert(slice.dispatch==SAT_SCENE3D_PREPARE_DISPATCH_MASTER);
    assert(slice.items==slice_items && slice.item_count==2u);
    assert(slice.items[0].instance==&source_instances[1] &&
           slice.items[1].instance==&source_instances[2]);
    assert(slice.view_proj.m[0]==vp.m[0] && slice.width==320u &&
           slice.height==224u);
    assert(sat_scene3d_prepare_batch_execute(&slice)==SAT_OK);
    assert(slice.metrics.source_faces==4u &&
           slice.metrics.prepared_faces==4u);
    /* Local slice ordering must not rewrite keys relative to that slice's
     * narrower depth range: merging this suffix must be byte-for-byte
     * equivalent to the corresponding suffix of a whole-batch preparation. */
    for(uint16_t i=0u;i<slice.metrics.prepared_faces;++i) {
        assert(slice_keys[i]==source_keys[i+2u]);
        assert(same_face(slice_faces[i],source_faces[i+2u]));
    }
    assert(sat_scene3d_prepare_batch_slice(
        &source_batch,2u,2u,slice_items,slice_faces,slice_keys,
        8u,&slice)==SAT_ERR_INVALID_ARG);

    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    assert(sat_scene3d_faces_merge_prepared(&scene,&batch)==SAT_OK);
    assert(scene.count==2u && scene.keys[0]==prepared_keys[0] &&
           scene.keys[1]==prepared_keys[1]);
    assert(sat_scene3d_faces_flush(&scene)==SAT_OK);

    sat_scene3d_face_t too_small_faces[1]={};
    uint32_t too_small_keys[1]={};

    sat_scene3d_prepare_batch_t too_small={};
    assert(sat_scene3d_prepare_batch_init(
        &too_small,&prepared_item,1u,too_small_faces,too_small_keys,
        1u)==SAT_OK);
    too_small.view_proj=vp;
    too_small.eye=eye;
    too_small.forward=forward;
    too_small.near_depth=SAT_FX16_ONE;
    too_small.width=320u;
    too_small.height=224u;
    assert(sat_scene3d_prepare_batch_execute(&too_small)==SAT_ERR_CAPACITY);
    assert(too_small.metrics.prepared_faces==0u);

    // Invalid descriptors reset no successful-face count. A caller that
    // rejects this batch must therefore not merge whatever bytes happened to
    // remain in its output storage from an earlier frame.
    sat_scene3d_prepare_item_t invalid_item=prepared_item;
    invalid_item.instance=nullptr;
    sat_scene3d_face_t invalid_faces[8]={};
    uint32_t invalid_keys[8]={};

    sat_scene3d_prepare_batch_t invalid_batch={};
    assert(sat_scene3d_prepare_batch_init(
        &invalid_batch,&invalid_item,1u,invalid_faces,invalid_keys,
        8u)==SAT_OK);
    invalid_batch.view_proj=vp;
    invalid_batch.eye=eye;
    invalid_batch.forward=forward;
    invalid_batch.near_depth=SAT_FX16_ONE;
    invalid_batch.width=320u;
    invalid_batch.height=224u;
    invalid_faces[0].world.v[0].x=1234;
    assert(sat_scene3d_prepare_batch_execute(&invalid_batch)==SAT_ERR_INVALID_ARG);
    assert(invalid_batch.metrics.prepared_faces==0u);

    // A failed merge is atomic: capacity exhaustion never exposes a partial
    // batch to the canonical scene.
    sat_scene3d_face_t one_face[1]={};
    uint32_t one_key[1]={};
    uint16_t one_order[1]={};
    sat_scene3d_faces_t small_scene={};
    assert(sat_scene3d_faces_init(
        &small_scene,one_face,one_key,one_order,1u)==SAT_OK);
    assert(sat_scene3d_faces_begin(
        &small_scene,&vp,&eye,&forward,SAT_FX16_ONE,320u,224u)==SAT_OK);
    assert(sat_scene3d_faces_merge_prepared(&small_scene,&batch)==SAT_ERR_CAPACITY);
    assert(small_scene.count==0u);

    // Passes occupy twelve bits. Reject unrepresentable values before
    // touching queue storage, even for compound box/instance submissions.
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    const uint16_t invalid_pass=static_cast<uint16_t>(SAT_SCENE3D_PASS_MAX+1u);
    const sat_quad3_t pass_quad=quad(2);
    assert(sat_scene3d_faces_submit_quad(
        &scene,&pass_quad,&near_mat,invalid_pass)==SAT_ERR_INVALID_ARG);
    sat_indexed_box3_t pass_box{};
    assert(sat_scene3d_faces_submit_box(
        &scene,&pass_box,SAT_INDEXED_SOLID_OPAQUE,invalid_pass)==SAT_ERR_INVALID_ARG);
    sat_scene3d_instance_t pass_instance=instance;
    pass_instance.pass=invalid_pass;
    assert(sat_scene3d_faces_submit_instance(
        &scene,&pass_instance,SAT_SCENE3D_SLOT_INHERIT,
        screen,world)==SAT_ERR_INVALID_ARG);
    assert(scene.count==0u);
    assert(sat_scene3d_faces_submit_quad(
        &scene,&pass_quad,&near_mat,SAT_SCENE3D_PASS_MAX)==SAT_OK);
    assert((scene.keys[0]>>20u)==0u);
    assert(sat_scene3d_faces_flush(&scene)==SAT_OK);
    assert(scene.emitted_faces==1u && scene.skipped_faces==0u);

    // A box's second face can fail projection AFTER its first face was
    // admitted. The compound operation must restore the previous queue and
    // all admission telemetry rather than leak an untracked orphan face.
    emitted_count=0u;
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    assert(sat_scene3d_faces_submit_quad(
        &scene,&near,&near_mat,0u)==SAT_OK);
    const uint16_t pre_count=scene.count;
    const uint16_t pre_culled=scene.culled_faces;
    const uint16_t pre_clipped=scene.clipped_faces;
    const uint32_t pre_key=scene.keys[0];
    const sat_scene3d_face_t pre_face=scene.entries[0];
    sat_indexed_box3_t box_for_failure{};
    box_for_failure.x_material=&near_tex;
    box_for_failure.z_material=&far_tex;
    project_fail_call=static_cast<uint16_t>(project_calls+2u);
    assert(sat_scene3d_faces_submit_box(
        &scene,&box_for_failure,SAT_INDEXED_SOLID_OPAQUE,0u)==SAT_ERR_UNSUPPORTED);
    project_fail_call=0u;
    assert(scene.count==pre_count);
    assert(scene.culled_faces==pre_culled && scene.clipped_faces==pre_clipped);
    assert(scene.keys[0]==pre_key && same_face(scene.entries[0],pre_face));
    assert(sat_scene3d_faces_flush(&scene)==SAT_OK);
    assert(emitted_count==1u && emitted[0]==near_tex.srca);

    // RGB geometry the projection cannot draw directly is clipped in world
    // space like indexed faces, not skipped.
    emitted_count=0;
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    sat_scene3d_material_t rgb{};
    rgb.kind=SAT_SCENE3D_RGB;
    rgb.color_calc_slot=SAT_INDEXED_SOLID_OPAQUE;
    rgb.rgb555=0x801Fu;
    assert(sat_scene3d_faces_submit_quad(
        &scene,&oversized,&rgb,0u)==SAT_OK);
    assert(sat_scene3d_faces_submit_quad(
        &scene,&pass_quad,&near_mat,0u)==SAT_OK);
    assert(sat_scene3d_faces_flush(&scene)==SAT_OK);
    assert(scene.emitted_faces==2u && scene.skipped_faces==0u);
    assert(scene.fallback_faces==1u);
    assert(emitted_count==2u);
    assert((emitted[0]==0x801Fu && emitted[1]==near_tex.srca) ||
           (emitted[1]==0x801Fu && emitted[0]==near_tex.srca));

    // An actual HAL rejection cannot count a queued face as emitted.
    emitted_count=0;
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    assert(sat_scene3d_faces_submit_quad(
        &scene,&pass_quad,&near_mat,0u)==SAT_OK);
    distorted_draw_status=SAT_ERR_CAPACITY;
    assert(sat_scene3d_faces_flush(&scene)==SAT_ERR_CAPACITY);
    assert(scene.active==0u && scene.emitted_faces==0u);
    assert(scene.skipped_faces==0u && emitted_count==0u);
    distorted_draw_status=SAT_OK;

    // Raw L1 remains independent, while an owner span is always checked
    // BEFORE even a farther valid face can be emitted. Slot reuse may leave
    // the raw texture descriptor valid, so the generation matters.
    sat_scene3d_owner_span_t spans[1]={};
    emitted_count=0u;
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    assert(sat_scene3d_faces_bind_owner_validator(
        &scene,validate_owner_for_test,nullptr,spans,1u)==SAT_ERR_INVALID_ARG);
    assert(sat_scene3d_faces_submit_projected_material(
        &scene,&cached_quad,2*SAT_FX16_ONE,&near_mat,0u)==SAT_OK);
    // No verifier bound: marking is refused, and the raw face still draws.
    assert(sat_scene3d_faces_mark_owner(
        &scene,0u,1u,0u,5u,&near_tex)==SAT_ERR_INVALID_ARG);
    assert(sat_scene3d_faces_owner_span_available(&scene)==0u);
    assert(sat_scene3d_faces_flush(&scene)==SAT_OK);
    assert(emitted_count==1u);

    // A verifier needs span storage.
    assert(sat_scene3d_faces_bind_owner_validator(
        &scene,validate_owner_for_test,nullptr,nullptr,1u)==SAT_ERR_INVALID_ARG);
    assert(sat_scene3d_faces_bind_owner_validator(
        &scene,validate_owner_for_test,nullptr,spans,0u)==SAT_ERR_INVALID_ARG);
    assert(sat_scene3d_faces_bind_owner_validator(
        &scene,validate_owner_for_test,nullptr,spans,1u)==SAT_OK);
    verified_owner_generation=6u; // Same native descriptor, recycled owner.
    emitted_count=0u;
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    assert(sat_scene3d_faces_submit_projected_material(
        &scene,&cached_quad,2*SAT_FX16_ONE,&near_mat,0u)==SAT_OK);
    // Spans must cover queued faces only and have at least one face.
    assert(sat_scene3d_faces_mark_owner(
        &scene,0u,2u,0u,5u,&near_tex)==SAT_ERR_INVALID_ARG);
    assert(sat_scene3d_faces_mark_owner(
        &scene,0u,0u,0u,5u,&near_tex)==SAT_ERR_INVALID_ARG);
    assert(sat_scene3d_faces_owner_span_available(&scene)==1u);
    assert(sat_scene3d_faces_mark_owner(
        &scene,0u,1u,0u,5u,&near_tex)==SAT_OK);
    assert(sat_scene3d_faces_owner_span_available(&scene)==0u);
    assert(sat_scene3d_faces_mark_owner(
        &scene,0u,1u,0u,5u,&near_tex)==SAT_ERR_CAPACITY);
    assert(sat_scene3d_faces_flush(&scene)==SAT_ERR_INVALID_ARG);
    assert(emitted_count==0u && scene.count==0u && !scene.active);

    // The current generation passes, and begin() clears last frame's spans.
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    assert(scene.owner_span_count==0u);
    assert(sat_scene3d_faces_submit_projected_material(
        &scene,&cached_quad,2*SAT_FX16_ONE,&near_mat,0u)==SAT_OK);
    assert(sat_scene3d_faces_mark_owner(
        &scene,0u,1u,0u,6u,&near_tex)==SAT_OK);
    assert(sat_scene3d_faces_flush(&scene)==SAT_OK);
    assert(emitted_count==1u && emitted[0]==near_tex.srca);

    // A span left beyond the queue (e.g. after an external rollback) is
    // rejected instead of validating faces that no longer exist.
    emitted_count=0u;
    assert(sat_scene3d_faces_begin(&scene,&vp,&eye,&forward,
        SAT_FX16_ONE,320u,224u)==SAT_OK);
    assert(sat_scene3d_faces_submit_projected_material(
        &scene,&cached_quad,2*SAT_FX16_ONE,&near_mat,0u)==SAT_OK);
    assert(sat_scene3d_faces_mark_owner(
        &scene,0u,1u,0u,6u,&near_tex)==SAT_OK);
    scene.count=0u;
    assert(sat_scene3d_faces_flush(&scene)==SAT_ERR_INVALID_ARG);
    assert(emitted_count==0u);

    std::puts("scene3d_faces api: OK");
    return 0;
}
