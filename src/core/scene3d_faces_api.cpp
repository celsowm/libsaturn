#include "saturn/scene3d_faces.h"
#include "saturn/vdp1_color_calc.h"
#include "src/core/scene3d_faces_logic.hpp"
#include "src/core/render3d_logic.hpp"

#include <stdint.h>

namespace {
bool valid_material(const sat_scene3d_material_t& m) {
    if (m.kind==SAT_SCENE3D_RGB)
        return m.color_calc_slot==SAT_INDEXED_SOLID_OPAQUE;
    if (m.kind!=SAT_SCENE3D_INDEXED_SOLID &&
        m.kind!=SAT_SCENE3D_INDEXED_TEXTURED) return false;
    return m.texture && m.texture->valid &&
        (m.color_calc_slot==SAT_INDEXED_SOLID_OPAQUE ||
         m.color_calc_slot<8u);
}

bool safe_projection(const sat_scene3d_faces_t& scene,
                     const sat_projected_vertex_t* vertices,
                     const uint16_t* indices) {
    const int32_t left=-static_cast<int32_t>(scene.width)/2;
    const int32_t top=-static_cast<int32_t>(scene.height)/2;
    const int32_t right=left+scene.width-1;
    const int32_t bottom=top+scene.height-1;
    for (uint8_t c=0;c<4u;++c) {
        const sat_projected_vertex_t& p=vertices[indices[c]];
        if (p.w<scene.near_depth || p.x<left || p.x>right ||
            p.y<top || p.y>bottom) return false;
    }
    return true;
}

sat_result_t append(sat_scene3d_faces_t* scene, const sat_quad3_t& world,
                    const sat_projected_vertex_t* projected,
                    const uint16_t indices[4],
                    const sat_scene3d_material_t& material, uint16_t pass) {
    bool visible=false;
    int64_t sum=0;
    for (uint8_t c=0;c<4u;++c) {
        const sat_fx16_t depth=projected[indices[c]].w;
        sum+=depth;
        if (depth>0) visible=true;
    }
    if (!visible) return SAT_OK;
    if (scene->count>=scene->capacity) return SAT_ERR_CAPACITY;
    sat_scene3d_face_t& item=scene->entries[scene->count];
    item.world=world;
    for (uint8_t c=0;c<4u;++c) {
        item.projected.x[c]=projected[indices[c]].x;
        item.projected.y[c]=projected[indices[c]].y;
    }
    item.material=material;
    item.depth=sum/4;
    item.sequence=scene->count;
    item.pass=pass;
    item.projected_safe=safe_projection(*scene,projected,indices)?1u:0u;
    ++scene->count;
    return SAT_OK;
}

sat_result_t emit(const sat_scene3d_faces_t& scene,
                  const sat_scene3d_face_t& face) {
    if (face.material.kind==SAT_SCENE3D_RGB) {
        if (!face.projected_safe) return SAT_ERR_UNSUPPORTED;
        return sat_draw_quad2_polygon(&face.projected,face.material.rgb555);
    }
    if (face.projected_safe) {
        sat_distorted_sprite_cmd_t cmd={};
        cmd.texture=face.material.texture;
        for (uint8_t i=0u;i<4u;++i) {
            cmd.x[i]=face.projected.x[i];
            cmd.y[i]=face.projected.y[i];
        }
        return face.material.color_calc_slot==SAT_INDEXED_SOLID_OPAQUE
            ? sat_draw_sprite_distorted(&cmd)
            : sat_draw_sprite_distorted_color_calc(
                &cmd,face.material.color_calc_slot);
    }
    sat_indexed_solid_render3d_t p={};
    p.view_proj=&scene.view_proj;
    p.eye=scene.eye;
    p.forward=scene.forward;
    p.near_depth=scene.near_depth;
    p.width=scene.width;p.height=scene.height;
    p.color_calc_slot=face.material.color_calc_slot;
    if (face.material.kind==SAT_SCENE3D_INDEXED_SOLID)
        return sat_draw_indexed_solid_quad3(
            &face.world,&p,face.material.texture);
    /* Textured UVs cannot be arbitrarily clipped on VDP1. */
    return sat_draw_indexed_textured_quad3(
        &face.world,&p,face.material.texture,nullptr);
}
} // namespace

extern "C" sat_result_t sat_scene3d_faces_init(
    sat_scene3d_faces_t* scene, sat_scene3d_face_t* storage,
    uint16_t capacity) {
    if (!scene || !storage || !capacity) return SAT_ERR_INVALID_ARG;
    *scene={};
    scene->entries=storage;scene->capacity=capacity;
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_faces_begin(
    sat_scene3d_faces_t* scene, const sat_mat4_t* view_proj,
    const sat_vec3_t* eye, const sat_vec3_t* forward,
    sat_fx16_t near_depth, uint16_t width, uint16_t height) {
    if (!scene || !scene->entries || !scene->capacity || scene->active ||
        !view_proj || !eye || !forward || near_depth<=0 ||
        (!forward->x && !forward->y && !forward->z) ||
        width<2u || height<2u || width>2048u || height>2048u)
        return SAT_ERR_INVALID_ARG;
    scene->count=0;scene->view_proj=*view_proj;
    scene->eye=*eye;scene->forward=*forward;
    scene->near_depth=near_depth;scene->width=width;scene->height=height;
    scene->active=1u;
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_faces_submit_quad(
    sat_scene3d_faces_t* scene, const sat_quad3_t* world,
    const sat_scene3d_material_t* material, uint16_t pass) {
    if (!scene || !scene->active || !world || !material ||
        !valid_material(*material)) return SAT_ERR_INVALID_ARG;
    if (scene->count>=scene->capacity) return SAT_ERR_CAPACITY;
    sat_projected_vertex_t screen[4]={};
    const sat_result_t st=sat_project_vertices(
        &scene->view_proj,world->v,4u,screen);
    if (st!=SAT_OK) return st;
    const uint16_t indices[4]={0u,1u,2u,3u};
    return append(scene,*world,screen,indices,*material,pass);
}

extern "C" sat_result_t sat_scene3d_faces_submit_mesh(
    sat_scene3d_faces_t* scene, const sat_mesh_t* mesh,
    const sat_vec3_t* translation,
    const sat_scene3d_material_t* materials, uint16_t material_count,
    const uint16_t* face_materials, uint16_t pass, uint8_t cull_backfaces,
    sat_projected_vertex_t* screen_scratch,
    sat_vec3_t* world_scratch) {
    if (!scene || !scene->active || !mesh || !mesh->vertices ||
        !mesh->indices || !screen_scratch ||
        (translation && !world_scratch) ||
        (mesh->face_count && (!materials || !face_materials)))
        return SAT_ERR_INVALID_ARG;
    if (mesh->face_count>scene->capacity-scene->count)
        return SAT_ERR_CAPACITY;
    for (uint16_t f=0;f<mesh->face_count;++f) {
        if (face_materials[f]>=material_count ||
            !valid_material(materials[face_materials[f]]))
            return SAT_ERR_INVALID_ARG;
        const uint16_t* idx=&mesh->indices[static_cast<uint32_t>(f)*4u];
        for (uint8_t c=0;c<4u;++c)
            if (idx[c]>=mesh->vertex_count) return SAT_ERR_INVALID_ARG;
    }
    const sat_vec3_t* world=mesh->vertices;
    if (translation) {
        for (uint16_t v=0;v<mesh->vertex_count;++v) {
            const sat_vec3_t& p=mesh->vertices[v];
            const int64_t x=static_cast<int64_t>(p.x)+translation->x;
            const int64_t y=static_cast<int64_t>(p.y)+translation->y;
            const int64_t z=static_cast<int64_t>(p.z)+translation->z;
            if (x<INT32_MIN || x>INT32_MAX || y<INT32_MIN || y>INT32_MAX ||
                z<INT32_MIN || z>INT32_MAX) return SAT_ERR_INVALID_ARG;
            world_scratch[v]={
                static_cast<sat_fx16_t>(x),
                static_cast<sat_fx16_t>(y),
                static_cast<sat_fx16_t>(z)};
        }
        world=world_scratch;
    }
    const sat_result_t projected=sat_project_vertices(
        &scene->view_proj,world,mesh->vertex_count,screen_scratch);
    if (projected!=SAT_OK) return projected;
    /* After complete preflight all accepted faces are copied into queue
     * storage, never retaining world_scratch or projected scratch pointers. */
    for (uint16_t f=0;f<mesh->face_count;++f) {
        const uint16_t* idx=&mesh->indices[static_cast<uint32_t>(f)*4u];
        if (cull_backfaces &&
            screen_scratch[idx[0]].w>0 && screen_scratch[idx[1]].w>0 &&
            screen_scratch[idx[2]].w>0 && screen_scratch[idx[3]].w>0 &&
            saturn::core::render3d::projected_area2(screen_scratch,idx)<=0)
            continue;
        sat_quad3_t quad={};
        for (uint8_t c=0;c<4u;++c) quad.v[c]=world[idx[c]];
        const sat_result_t st=append(
            scene,quad,screen_scratch,idx,
            materials[face_materials[f]],pass);
        if (st!=SAT_OK) return st;
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_scene3d_faces_flush(
    sat_scene3d_faces_t* scene) {
    if (!scene || !scene->active) return SAT_ERR_INVALID_ARG;
    scene->active=0u;
    saturn::core::scene3d_faces::sort(scene->entries,scene->count);
    sat_result_t result=SAT_OK;
    for (uint16_t i=0;i<scene->count;++i) {
        result=emit(*scene,scene->entries[i]);
        if (result==SAT_ERR_UNSUPPORTED) continue;
        if (result!=SAT_OK) break;
    }
    scene->count=0u;
    return result==SAT_ERR_UNSUPPORTED?SAT_OK:result;
}
