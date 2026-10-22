#pragma GCC diagnostic ignored "-Wunused-function"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <stdint.h>
#include "saturn/scene3d_faces.h"
#include "src/graphics/vdp1/projected.hpp"
#include "src/graphics/3d/scene/capture.hpp"

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
/* 0: the textured-quad stub reports the sprite as not drawable (a corner
 * behind the near plane), the case a fallback colour exists for. */
static uint8_t textured_drawable=1u;

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
    if(drawn) *drawn=textured_drawable;
    if(textured_drawable) emitted[emitted_count++]=tex->srca;
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

/* ---- skinned characters through the canonical scene painter ---- */
#include "saturn/skin3d.h"

extern "C" sat_result_t sat_transform3d_get_world(
    const sat_transform3d_world_t*, uint16_t, sat_mat4_t*) {
    return SAT_ERR_UNSUPPORTED; /* not used: matrices are supplied directly */
}

#define FXI(n) ((sat_fx16_t)((n) * SAT_FX16_ONE))

static sat_mat4_t at_z(int32_t z) {
    sat_mat4_t m = {};
    m.m[0] = m.m[5] = m.m[10] = m.m[15] = SAT_FX16_ONE;
    m.m[11] = FXI(z);
    return m;
}

/* A two-joint "character": two unit quads, one rigid to each joint. */
struct Character {
    sat_skin_rigid_t rigid[8];
    sat_skin_asset_t skin;
    sat_vec3_t verts[8];
    uint16_t indices[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    sat_mesh_t mesh;
    sat_scene3d_material_t materials[2];
    uint16_t face_materials[2] = {0, 1};
    Character(uint16_t color_a, uint16_t color_b) {
        static const int16_t corner[4][2] = {{-256, -256}, {256, -256}, {256, 256}, {-256, 256}};
        for (int i = 0; i < 8; ++i)
            rigid[i] = {{corner[i & 3][0], corner[i & 3][1], 0}, (uint8_t)(i >> 2), 0};
        skin = {};
        skin.rigid = rigid; skin.rigid_count = 8; skin.joint_count = 2;
        skin.pos_shift = 8; skin.max_joint = 1;
        mesh = {verts, indices, 8, 0, 2, 2};
        materials[0] = {SAT_SCENE3D_RGB, color_a, nullptr, nullptr, SAT_INDEXED_SOLID_OPAQUE, nullptr};
        materials[1] = {SAT_SCENE3D_RGB, color_b, nullptr, nullptr, SAT_INDEXED_SOLID_OPAQUE, nullptr};
    }
    sat_scene3d_instance_t instance(const sat_aabb3_t* bounds) const {
        sat_scene3d_instance_t i = {};
        i.mesh = &mesh; i.materials = materials; i.material_count = 2;
        i.face_materials = face_materials; i.bounds = bounds;
        return i;
    }
};

static void two_characters_share_one_painter_order() {
    Character a(0x8001, 0x8002), b(0x8010, 0x8011);
    assert(sat_skin_validate(&a.skin) == SAT_OK);
    assert(sat_skin_validate_mesh(&a.skin, &a.mesh) == SAT_OK);
    const uint16_t index_copy[8] = {0, 1, 2, 3, 4, 5, 6, 7};

    /* A's quads at depth 12 and 4, B's at 8 and 20: a whole-object sort could
     * never produce the interleaving 20(B), 12(A), 8(B), 4(A). */
    sat_mat4_t ma[2] = {at_z(12), at_z(4)};
    sat_mat4_t mb[2] = {at_z(8), at_z(20)};
    sat_aabb3_t ba = {}, bb = {};
    assert(sat_skin_update_mesh(&a.skin, ma, 2, &a.mesh, 0, &ba) == SAT_OK);
    assert(sat_skin_update_mesh(&b.skin, mb, 2, &b.mesh, 0, &bb) == SAT_OK);
    assert(a.mesh.vertex_count == 8 && b.mesh.vertex_count == 8);
    assert(std::memcmp(a.indices, index_copy, sizeof index_copy) == 0);
    assert(ba.center.z == FXI(8) && ba.half.z == FXI(4) && ba.half.x == SAT_FX16_ONE);

    sat_scene3d_faces_t scene = {};
    sat_scene3d_face_t storage[8] = {};
    uint32_t keys[8] = {};
    uint16_t order[8] = {};
    assert(sat_scene3d_faces_init(&scene, storage, keys, order, 8) == SAT_OK);
    sat_mat4_t vp = {};
    vp.m[0] = SAT_FX16_ONE;
    const sat_vec3_t eye = {0, 0, 0}, forward = {0, 0, SAT_FX16_ONE};
    emitted_count = 0;
    assert(sat_scene3d_faces_begin(&scene, &vp, &eye, &forward, SAT_FX16_ONE, 320, 224) == SAT_OK);
    sat_projected_vertex_t screen[8] = {};
    sat_vec3_t world[8] = {};
    sat_scene3d_instance_t ia = a.instance(nullptr), ib = b.instance(nullptr);
    assert(sat_scene3d_faces_submit_instance(&scene, &ia, SAT_SCENE3D_SLOT_INHERIT, screen, world) == SAT_OK);
    assert(sat_scene3d_faces_submit_instance(&scene, &ib, SAT_SCENE3D_SLOT_INHERIT, screen, world) == SAT_OK);
    assert(scene.count == 4);
    assert(sat_scene3d_faces_flush(&scene) == SAT_OK);
    assert(emitted_count == 4);
    assert(emitted[0] == 0x8011 && emitted[1] == 0x8001 && emitted[2] == 0x8010 && emitted[3] == 0x8002);

    /* Next frame: swap the poses. Topology is untouched, the order follows the
     * deformed geometry, and nothing was rebuilt. */
    sat_mat4_t ma2[2] = {at_z(30), at_z(2)};
    assert(sat_skin_update_mesh(&a.skin, ma2, 2, &a.mesh, 0, &ba) == SAT_OK);
    assert(std::memcmp(a.indices, index_copy, sizeof index_copy) == 0);
    emitted_count = 0;
    assert(sat_scene3d_faces_begin(&scene, &vp, &eye, &forward, SAT_FX16_ONE, 320, 224) == SAT_OK);
    assert(sat_scene3d_faces_submit_instance(&scene, &ia, SAT_SCENE3D_SLOT_INHERIT, screen, world) == SAT_OK);
    assert(sat_scene3d_faces_submit_instance(&scene, &ib, SAT_SCENE3D_SLOT_INHERIT, screen, world) == SAT_OK);
    assert(sat_scene3d_faces_flush(&scene) == SAT_OK);
    assert(emitted_count == 4);
    assert(emitted[0] == 0x8001 && emitted[1] == 0x8011 && emitted[2] == 0x8010 && emitted[3] == 0x8002);
}

static void update_mesh_contracts() {
    Character a(1, 2);
    sat_mat4_t m[2] = {at_z(1), at_z(2)};
    a.verts[0] = {7, 8, 9};
    sat_mesh_t small = {a.verts, a.indices, 4, 0, 2, 2};
    assert(sat_skin_update_mesh(&a.skin, m, 2, &small, 0, nullptr) == SAT_ERR_CAPACITY);
    assert(small.vertex_count == 0 && a.verts[0].x == 7);
    assert(sat_skin_validate_mesh(&a.skin, &small) == SAT_ERR_CAPACITY);
    assert(sat_skin_update_mesh(&a.skin, m, 2, &a.mesh, -1, nullptr) == SAT_ERR_INVALID_ARG);
    uint16_t bad_indices[8] = {0, 1, 2, 3, 4, 5, 6, 8};
    sat_mesh_t bad = {a.verts, bad_indices, 8, 0, 2, 2};
    assert(sat_skin_validate_mesh(&a.skin, &bad) == SAT_ERR_INVALID_ARG);
    sat_aabb3_t box;
    assert(sat_skin_bounds(nullptr, 1, 0, &box) == SAT_ERR_INVALID_ARG);
    sat_vec3_t one[1] = {{FXI(1), FXI(-2), FXI(3)}};
    assert(sat_skin_bounds(one, 1, FXI(1), &box) == SAT_OK);
    assert(box.center.x == FXI(1) && box.half.x == FXI(1) && box.half.z == FXI(1));
}

int main() {
    two_characters_share_one_painter_order();
    update_mesh_contracts();
    std::printf("test_skin_scene: 2 tests passed\n");
    return 0;
}

extern "C" sat_result_t sat_draw_sprite_distorted_gouraud(const sat_distorted_sprite_cmd_t* cmd,const uint16_t*) { return sat_draw_sprite_distorted(cmd); }
extern "C" sat_result_t sat_draw_indexed_textured_quad3_gouraud(const sat_quad3_t* q,const sat_indexed_solid_render3d_t* p,const sat_vdp1_texture_t* t,const uint16_t*,uint8_t* out) { return sat_draw_indexed_textured_quad3(q,p,t,out); }

// Preserve the scene tests' draw/failure spy for the fused opaque route.
sat_result_t saturn::internal::draw_projected_sprite(const sat_quad2_t& quad,
    const sat_vdp1_texture_t& texture, uint16_t flags) {
    sat_distorted_sprite_cmd_t cmd{};
    cmd.texture=&texture;cmd.flags=flags;
    for(unsigned i=0;i<4;++i){cmd.x[i]=quad.x[i];cmd.y[i]=quad.y[i];}
    return sat_draw_sprite_distorted(&cmd);
}

sat_result_t saturn::internal::draw_projected_program(const sat_quad2_t& quad,
    const sat_scene3d_material_program_t& program) {
    return draw_projected_sprite(quad,*program.texture,
        (program.command[2]&0x0100u)?SAT_SPRITE_FLAG_MESH:0u);
}

#include "tests/host_stubs/projected_program_spy.hpp"
