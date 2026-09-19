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

extern "C" sat_result_t sat_draw_indexed_textured_quad3(
    const sat_quad3_t* quad, const sat_indexed_solid_render3d_t* p,
    const sat_vdp1_texture_t* texture, uint8_t* out_drawn
) {
    if(out_drawn!=nullptr) *out_drawn=0u;
    if(quad==nullptr || texture==nullptr || !valid_render(p) ||
       p->width<2u || p->height<2u || p->width>2048u || p->height>2048u)
        return SAT_ERR_INVALID_ARG;

    /* A distorted VDP1 sprite carries no independent UVs at its corners.
     * In particular, one "clipped" triangle may still have a count of one,
     * but it is NOT the source quad. Test each ORIGINAL corner instead. */
    for(uint8_t i=0u;i<4u;++i) {
        const sat_vec3_t& v=quad->v[i];
        const int64_t dx=static_cast<int64_t>(v.x)-p->eye.x;
        const int64_t dy=static_cast<int64_t>(v.y)-p->eye.y;
        const int64_t dz=static_cast<int64_t>(v.z)-p->eye.z;
        const int64_t depth=(dx*p->forward.x+dy*p->forward.y+
                             dz*p->forward.z)/SAT_FX16_ONE;
        if(depth < p->near_depth) return SAT_OK;
    }

    sat_quad2_t projected;
    const sat_result_t proj=sat_project_quad(p->view_proj,quad,&projected);
    if(proj==SAT_ERR_UNSUPPORTED) return SAT_OK;
    if(proj!=SAT_OK) return proj;
    const int32_t min_x=-static_cast<int32_t>(p->width)/2;
    const int32_t max_x=static_cast<int32_t>(p->width+1u)/2-1;
    const int32_t min_y=-static_cast<int32_t>(p->height)/2;
    const int32_t max_y=static_cast<int32_t>(p->height+1u)/2-1;
    for(uint8_t i=0u;i<4u;++i) {
        if(projected.x[i]<min_x || projected.x[i]>max_x ||
           projected.y[i]<min_y || projected.y[i]>max_y)
            return SAT_OK;
    }

    sat_distorted_sprite_cmd_t cmd={};
    for(uint8_t i=0u;i<4u;++i) {
        cmd.x[i]=projected.x[i];
        cmd.y[i]=projected.y[i];
    }
    cmd.texture=texture;
    const sat_result_t st=p->color_calc_slot==SAT_INDEXED_SOLID_OPAQUE
        ? sat_draw_sprite_distorted(&cmd)
        : sat_draw_sprite_distorted_color_calc(&cmd,p->color_calc_slot);
    if(st==SAT_OK && out_drawn!=nullptr) *out_drawn=1u;
    return st;
}

namespace {
inline sat_vec3_t tile_midpoint(const sat_vec3_t& a,const sat_vec3_t& b) {
    return sat_vec3_t{
        static_cast<sat_fx16_t>((static_cast<int64_t>(a.x)+b.x)/2),
        static_cast<sat_fx16_t>((static_cast<int64_t>(a.y)+b.y)/2),
        static_cast<sat_fx16_t>((static_cast<int64_t>(a.z)+b.z)/2)
    };
}
inline sat_vec3_t tile_center(const sat_quad3_t& q) {
    return tile_midpoint(
        tile_midpoint(q.v[0],q.v[2]),
        tile_midpoint(q.v[1],q.v[3]));
}
inline bool valid_tiled_regions(const sat_indexed_tiled_quad3_t* regions) {
    if(regions==nullptr || regions->full==nullptr ||
       regions->full->valid==0u ||
       regions->full->width<16u || (regions->full->width&15u)!=0u ||
       regions->full->height<2u || (regions->full->height&1u)!=0u)
        return false;
    const sat_vdp1_texture_t& whole=*regions->full;
    for(uint8_t i=0u;i<4u;++i) {
        const sat_vdp1_texture_t* tile=regions->tiles[i];
        if(tile==nullptr || tile->valid==0u ||
           tile->width!=whole.width/2u ||
           tile->height!=whole.height/2u ||
           tile->palette!=whole.palette)
            return false;
    }
    return true;
}
}

extern "C" sat_result_t sat_draw_indexed_tiled_quad3(
    const sat_quad3_t* quad,const sat_indexed_solid_render3d_t* params,
    const sat_indexed_tiled_quad3_t* regions,uint8_t* out_submitted
) {
    if(out_submitted!=nullptr) *out_submitted=0u;
    if(quad==nullptr || !valid_render(params) || !valid_tiled_regions(regions))
        return SAT_ERR_INVALID_ARG;
    uint8_t drawn=0u;
    sat_result_t st=sat_draw_indexed_textured_quad3(
        quad,params,regions->full,&drawn);
    if(st!=SAT_OK) return st;
    if(drawn!=0u) {
        if(out_submitted!=nullptr) *out_submitted=1u;
        return SAT_OK;
    }

    /* UV regions correspond to these exact midpoint-bounded quads. Caller
     * must supply a planar affine surface for the intended texel mapping. */
    const sat_vec3_t ab=tile_midpoint(quad->v[0],quad->v[1]);
    const sat_vec3_t bc=tile_midpoint(quad->v[1],quad->v[2]);
    const sat_vec3_t cd=tile_midpoint(quad->v[2],quad->v[3]);
    const sat_vec3_t da=tile_midpoint(quad->v[3],quad->v[0]);
    const sat_vec3_t center=tile_center(*quad);
    const sat_quad3_t sub[4]={
        {{quad->v[0],ab,center,da}}, /* TL */
        {{ab,quad->v[1],bc,center}}, /* TR */
        {{da,center,cd,quad->v[3]}}, /* BL */
        {{center,bc,quad->v[2],cd}}  /* BR */
    };
    for(uint8_t i=0u;i<4u;++i) {
        drawn=0u;
        st=sat_draw_indexed_textured_quad3(
            &sub[i],params,regions->tiles[i],&drawn);
        if(st!=SAT_OK) return st;
        if(out_submitted!=nullptr)
            *out_submitted=static_cast<uint8_t>(*out_submitted+drawn);
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
