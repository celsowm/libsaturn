#include "saturn/mesh3d.h"
#include "saturn/vdp1_color_calc.h"
#include "src/core/render3d_logic.hpp"

namespace {

bool valid_render(const sat_indexed_solid_render3d_t* p) {
    return p != nullptr && p->view_proj != nullptr &&
           p->near_depth > 0 && p->width != 0u && p->height != 0u &&
           (p->forward.x != 0 || p->forward.y != 0 || p->forward.z != 0) &&
           (p->color_calc_slot == SAT_INDEXED_SOLID_OPAQUE ||
            p->color_calc_slot < 8u);
}

uint32_t face_depth(const sat_quad3_t& quad,
                    const sat_indexed_solid_mesh3d_draw_t& draw) {
    int64_t sx=0, sy=0, sz=0;
    for (uint8_t v=0u;v<4u;++v) {
        sx+=quad.v[v].x;
        sy+=quad.v[v].y;
        sz+=quad.v[v].z;
    }
    /* Translate the LOCAL face center only once; use 64-bit accumulation
     * before reducing to a bounded 16.16 camera-forward painter key. */
    const int64_t dx=sx/4 + draw.position.x - draw.render.eye.x;
    const int64_t dy=sy/4 + draw.position.y - draw.render.eye.y;
    const int64_t dz=sz/4 + draw.position.z - draw.render.eye.z;
    const int64_t depth=((dx*draw.render.forward.x)>>16) +
                        ((dy*draw.render.forward.y)>>16) +
                        ((dz*draw.render.forward.z)>>16);
    if(depth<=0) return 0u;
    return depth>0xFFFFFFFFLL ? 0xFFFFFFFFu : static_cast<uint32_t>(depth);
}

} // namespace

extern "C" sat_result_t sat_draw_indexed_solid_quad3(
    const sat_quad3_t* quad, const sat_indexed_solid_render3d_t* p,
    const sat_vdp1_texture_t* texture
) {
    if (quad==nullptr || texture==nullptr || !valid_render(p))
        return SAT_ERR_INVALID_ARG;
    sat_quad3_t pieces[4];
    uint8_t piece_count=0u;
    sat_result_t st=sat_clip_quad_near(
        quad,&p->eye,&p->forward,p->near_depth,pieces,&piece_count);
    if(st!=SAT_OK) return st;
    for(uint8_t piece=0u;piece<piece_count;++piece) {
        sat_quad2_t projected;
        sat_quad2_t visible[6];
        uint8_t visible_count=0u;
        st=sat_project_quad(p->view_proj,&pieces[piece],&projected);
        if(st==SAT_ERR_UNSUPPORTED) continue;
        if(st!=SAT_OK) return st;
        st=sat_clip_quad_screen(&projected,p->width,p->height,visible,
                                &visible_count);
        if(st==SAT_ERR_UNSUPPORTED) continue;
        if(st!=SAT_OK) return st;
        for(uint8_t i=0u;i<visible_count;++i) {
            sat_distorted_sprite_cmd_t cmd={};
            for(uint8_t v=0u;v<4u;++v) {
                cmd.x[v]=visible[i].x[v];
                cmd.y[v]=visible[i].y[v];
            }
            cmd.texture=texture;
            st=p->color_calc_slot==SAT_INDEXED_SOLID_OPAQUE
                ? sat_draw_sprite_distorted(&cmd)
                : sat_draw_sprite_distorted_color_calc(&cmd,p->color_calc_slot);
            if(st!=SAT_OK && st!=SAT_ERR_UNSUPPORTED) return st;
        }
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_draw_indexed_solid_mesh3(
    const sat_mesh_t* mesh,const sat_indexed_solid_mesh3d_draw_t* p
) {
    if(mesh==nullptr || p==nullptr || !valid_render(&p->render) ||
       mesh->vertices==nullptr || mesh->indices==nullptr ||
       (mesh->face_count!=0u &&
        (p->textures==nullptr || p->face_materials==nullptr ||
         p->depth==nullptr || p->order==nullptr)))
        return SAT_ERR_INVALID_ARG;
    /* Reject a malformed mesh or material table BEFORE emitting commands. */
    for(uint16_t face=0u;face<mesh->face_count;++face) {
        if(p->face_materials[face]>=p->texture_count)
            return SAT_ERR_INVALID_ARG;
        const uint16_t* ix=&mesh->indices[static_cast<uint32_t>(face)*4u];
        for(uint8_t v=0u;v<4u;++v) {
            if(ix[v]>=mesh->vertex_count) return SAT_ERR_INVALID_ARG;
        }
    }
    for(uint16_t face=0u;face<mesh->face_count;++face) {
        const uint16_t* ix=&mesh->indices[static_cast<uint32_t>(face)*4u];
        sat_quad3_t quad;
        for(uint8_t v=0u;v<4u;++v) quad.v[v]=mesh->vertices[ix[v]];
        p->depth[face]=face_depth(quad,*p);
        p->order[face]=face;
    }
    /* Deterministic O(n log n) sort with no heap allocation. */
    saturn::core::render3d::sort_indices16_desc(
        p->order,p->depth,mesh->face_count);
    for(uint16_t i=0u;i<mesh->face_count;++i) {
        const uint16_t face=p->order[i];
        const uint16_t* ix=&mesh->indices[static_cast<uint32_t>(face)*4u];
        sat_quad3_t quad;
        for(uint8_t v=0u;v<4u;++v) {
            quad.v[v]=mesh->vertices[ix[v]];
            quad.v[v].x+=p->position.x;
            quad.v[v].y+=p->position.y;
            quad.v[v].z+=p->position.z;
        }
        const sat_result_t st=sat_draw_indexed_solid_quad3(
            &quad,&p->render,&p->textures[p->face_materials[face]]);
        if(st!=SAT_OK && st!=SAT_ERR_UNSUPPORTED) return st;
    }
    return SAT_OK;
}
