#include "saturn/mesh3d_draw.h"
#include "saturn/vdp1_color_calc.h"
#include "src/graphics/3d/rendering/logic.hpp"
#include "src/graphics/3d/scene/capture.hpp"

namespace {

/* A direct draw inside sat_scene3d_capture_begin/end queues into the scene
 * instead (see scene3d_faces.h); true when it did, with its result. */
bool captured(const sat_quad3_t* quad, sat_scene3d_material_kind_t kind,
              uint16_t rgb555, const sat_vdp1_texture_t* texture,
              const sat_indexed_tiled_quad3_t* tiled, uint8_t slot,
              const uint16_t* gouraud, sat_result_t* result) {
    sat_scene3d_material_t material={};
    material.kind=kind;
    material.rgb555=rgb555;
    material.texture=texture;
    material.tiled=tiled;
    material.color_calc_slot=slot;
    material.vertex_gouraud=gouraud;
    return saturn::graphics::scene3d::capture_quad(*quad,material,result);
}

bool valid_camera(const sat_indexed_solid_render3d_t* p) {
    return p != nullptr && p->view_proj != nullptr &&
           p->near_depth > 0 && p->width != 0u && p->height != 0u &&
           (p->forward.x != 0 || p->forward.y != 0 || p->forward.z != 0);
}

bool valid_render(const sat_indexed_solid_render3d_t* p) {
    return valid_camera(p) &&
           (p->color_calc_slot == SAT_INDEXED_SOLID_OPAQUE ||
            p->color_calc_slot < 8u);
}

/* Near clip, project and screen clip one world quad, handing every visible
 * piece to draw(near_piece, projected_near_piece, visible_piece). A piece the
 * projector or a draw call reports as UNSUPPORTED is skipped; any other
 * error stops the quad. */
template <typename Draw>
sat_result_t for_each_visible_piece(const sat_quad3_t* quad,
                                    const sat_indexed_solid_render3d_t* p,
                                    Draw draw) {
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
            st=draw(pieces[piece],projected,visible[i]);
            if(st!=SAT_OK && st!=SAT_ERR_UNSUPPORTED) return st;
        }
    }
    return SAT_OK;
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
    sat_result_t queued;
    if (captured(quad,SAT_SCENE3D_INDEXED_SOLID,0u,texture,nullptr,
                 p->color_calc_slot,nullptr,&queued))
        return queued;
    return for_each_visible_piece(quad,p,[&](const sat_quad3_t&,const sat_quad2_t&,
                                             const sat_quad2_t& piece) {
        sat_distorted_sprite_cmd_t cmd={};
        for(uint8_t v=0u;v<4u;++v) {
            cmd.x[v]=piece.x[v];
            cmd.y[v]=piece.y[v];
        }
        cmd.texture=texture;
        return p->color_calc_slot==SAT_INDEXED_SOLID_OPAQUE
            ? sat_draw_sprite_distorted(&cmd)
            : sat_draw_sprite_distorted_color_calc(&cmd,p->color_calc_slot);
    });
}

extern "C" sat_result_t sat_draw_polygon_quad3(
    const sat_quad3_t* quad, const sat_indexed_solid_render3d_t* p,
    uint16_t rgb555
) {
    if (quad==nullptr || !valid_camera(p))
        return SAT_ERR_INVALID_ARG;
    sat_result_t queued;
    if (captured(quad,SAT_SCENE3D_RGB,rgb555,nullptr,nullptr,
                 SAT_INDEXED_SOLID_OPAQUE,nullptr,&queued))
        return queued;
    return for_each_visible_piece(quad,p,[rgb555](const sat_quad3_t&,const sat_quad2_t&,
                                                  const sat_quad2_t& piece) {
        return sat_draw_quad2_polygon(&piece,rgb555);
    });
}

namespace {

struct Point2 { int64_t x, y; };

int64_t area2(Point2 a, Point2 b, Point2 c) {
    return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
}

/* One RGB555 Gouraud channel (5 bits at `shift`), weighted by w[] / total. */
uint16_t blend_channel(const uint16_t g[3], const int64_t w[3], int64_t total,
                       uint32_t shift) {
    int64_t sum=0;
    for (int k=0;k<3;++k) sum+=w[k]*static_cast<int64_t>((g[k]>>shift)&0x1Fu);
    int64_t value=(sum+total/2)/total;
    if (value<0) value=0;
    if (value>31) value=31;
    return static_cast<uint16_t>(value<<shift);
}

/* Gouraud value at p inside the quad corner[0..3] carrying g[0..3]: the
 * barycentric blend in whichever of triangles (0,1,2) / (0,2,3) holds p
 * (clip points lie on the quad or its edges). Outside both by rounding,
 * the nearer triangle's clamped weights are used. */
uint16_t blend_gouraud(const Point2 corner[4], const uint16_t g[4], Point2 p) {
    static const int kTri[2][3]={{0,1,2},{0,2,3}};
    int best=-1;
    int64_t best_w[3]={0,0,0}, best_total=0, best_deficit=INT64_MAX;
    for (int t=0;t<2;++t) {
        const Point2 a=corner[kTri[t][0]], b=corner[kTri[t][1]], c=corner[kTri[t][2]];
        int64_t total=area2(a,b,c);
        if (total==0) continue;
        int64_t w[3]={area2(p,b,c),area2(a,p,c),area2(a,b,p)};
        if (total<0) { total=-total; for (int k=0;k<3;++k) w[k]=-w[k]; }
        int64_t deficit=0;
        for (int k=0;k<3;++k) if (w[k]<0) { deficit-=w[k]; w[k]=0; }
        if (deficit<best_deficit) {
            best=t; best_deficit=deficit; best_total=w[0]+w[1]+w[2];
            for (int k=0;k<3;++k) best_w[k]=w[k];
        }
    }
    if (best<0 || best_total==0) return g[0];
    const uint16_t tri[3]={g[kTri[best][0]],g[kTri[best][1]],g[kTri[best][2]]};
    return static_cast<uint16_t>((g[0]&0x8000u) |
        blend_channel(tri,best_w,best_total,0u) |
        blend_channel(tri,best_w,best_total,5u) |
        blend_channel(tri,best_w,best_total,10u));
}

/* The axis to drop to see the quad face-on: its largest normal component,
 * computed in 1/256-unit steps so the products stay inside 64 bits. */
int dominant_axis(const sat_quad3_t& q) {
    const int64_t ax=(q.v[1].x-static_cast<int64_t>(q.v[0].x))>>8;
    const int64_t ay=(q.v[1].y-static_cast<int64_t>(q.v[0].y))>>8;
    const int64_t az=(q.v[1].z-static_cast<int64_t>(q.v[0].z))>>8;
    const int64_t bx=(q.v[2].x-static_cast<int64_t>(q.v[0].x))>>8;
    const int64_t by=(q.v[2].y-static_cast<int64_t>(q.v[0].y))>>8;
    const int64_t bz=(q.v[2].z-static_cast<int64_t>(q.v[0].z))>>8;
    const int64_t nx=ay*bz-az*by, ny=az*bx-ax*bz, nz=ax*by-ay*bx;
    const int64_t mx=nx<0?-nx:nx, my=ny<0?-ny:ny, mz=nz<0?-nz:nz;
    return (mx>=my && mx>=mz)?0:(my>=mz)?1:2;
}

Point2 flat_point(const sat_vec3_t& v, int drop) {
    const int64_t x=static_cast<int64_t>(v.x)>>8, y=static_cast<int64_t>(v.y)>>8,
                  z=static_cast<int64_t>(v.z)>>8;
    return drop==0 ? Point2{y,z} : drop==1 ? Point2{x,z} : Point2{x,y};
}

} // namespace

extern "C" sat_result_t sat_draw_polygon_quad3_gouraud(
    const sat_quad3_t* quad, const sat_indexed_solid_render3d_t* p,
    uint16_t rgb555, const uint16_t gouraud[4]
) {
    if (quad==nullptr || gouraud==nullptr || !valid_camera(p))
        return SAT_ERR_INVALID_ARG;
    sat_result_t queued;
    if (captured(quad,SAT_SCENE3D_RGB,rgb555,nullptr,nullptr,
                 SAT_INDEXED_SOLID_OPAQUE,gouraud,&queued))
        return queued;
    const int drop=dominant_axis(*quad);
    Point2 original[4];
    for (int k=0;k<4;++k) original[k]=flat_point(quad->v[k],drop);
    return for_each_visible_piece(quad,p,[&](const sat_quad3_t& near_piece,
                                             const sat_quad2_t& projected,
                                             const sat_quad2_t& piece) {
        // Colours at the near-clipped corners, interpolated in the quad's
        // own plane; then at the screen-clipped corners, in screen space,
        // which is how the VDP1 interpolates Gouraud anyway.
        uint16_t near_g[4];
        for (int k=0;k<4;++k)
            near_g[k]=blend_gouraud(original,gouraud,flat_point(near_piece.v[k],drop));
        Point2 screen[4];
        for (int k=0;k<4;++k) screen[k]=Point2{projected.x[k],projected.y[k]};
        uint16_t piece_g[4];
        for (int k=0;k<4;++k)
            piece_g[k]=blend_gouraud(screen,near_g,Point2{piece.x[k],piece.y[k]});
        return sat_draw_quad2_polygon_gouraud(&piece,rgb555,piece_g);
    });
}

namespace {

void box_face(sat_quad3_t* out,
              const sat_vec3_t& a,const sat_vec3_t& b,
              const sat_vec3_t& c,const sat_vec3_t& d) {
    out->v[0]=a;
    out->v[1]=b;
    out->v[2]=c;
    out->v[3]=d;
}

/* Validate EVERY arithmetic result before drawing any face; a half extent
 * near INT32_MAX must never wrap a solid into the opposite side of space. */
bool box_axis(sat_fx16_t center,sat_fx16_t half,
              sat_fx16_t* low,sat_fx16_t* high) {
    if(half<=0) return false;
    const int64_t lo=static_cast<int64_t>(center)-half;
    const int64_t hi=static_cast<int64_t>(center)+half;
    if(lo<INT32_MIN || hi>INT32_MAX) return false;
    *low=static_cast<sat_fx16_t>(lo);
    *high=static_cast<sat_fx16_t>(hi);
    return true;
}
} // namespace

extern "C" sat_result_t sat_indexed_box3_faces(
    const sat_indexed_box3_t* box, const sat_vec3_t* eye,
    sat_quad3_t out_faces[3],
    const sat_vdp1_texture_t* out_materials[3],
    uint8_t* out_count
) {
    if (out_count) *out_count=0u;
    if (!box || !eye || !out_faces || !out_materials || !out_count ||
        !box->top_material || !box->x_material || !box->z_material)
        return SAT_ERR_INVALID_ARG;
    sat_fx16_t lx,rx,bottom,top,bz,fz;
    const int64_t cy=static_cast<int64_t>(box->top_center.y)-box->half_extents.y;
    if (cy<INT32_MIN || cy>INT32_MAX ||
        !box_axis(box->top_center.x,box->half_extents.x,&lx,&rx) ||
        !box_axis(static_cast<sat_fx16_t>(cy),box->half_extents.y,&bottom,&top) ||
        !box_axis(box->top_center.z,box->half_extents.z,&bz,&fz))
        return SAT_ERR_INVALID_ARG;
    if (eye->x>box->top_center.x)
        box_face(&out_faces[0],{rx,top,bz},{rx,top,fz},{rx,bottom,fz},{rx,bottom,bz});
    else
        box_face(&out_faces[0],{lx,top,fz},{lx,top,bz},{lx,bottom,bz},{lx,bottom,fz});
    out_materials[0]=box->x_material;
    if (eye->z>box->top_center.z)
        box_face(&out_faces[1],{rx,top,fz},{lx,top,fz},{lx,bottom,fz},{rx,bottom,fz});
    else
        box_face(&out_faces[1],{lx,top,bz},{rx,top,bz},{rx,bottom,bz},{lx,bottom,bz});
    out_materials[1]=box->z_material;
    *out_count=2u;
    if (eye->y>top) {
        box_face(&out_faces[2],{lx,top,bz},{rx,top,bz},{rx,top,fz},{lx,top,fz});
        out_materials[2]=box->top_material;
        *out_count=3u;
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_draw_indexed_box3(
    const sat_indexed_box3_t* box,
    const sat_indexed_solid_render3d_t* p
) {
    if (!valid_render(p)) return SAT_ERR_INVALID_ARG;
    sat_quad3_t faces[3]={};
    const sat_vdp1_texture_t* materials[3]={};
    uint8_t count=0u;
    const sat_result_t st=sat_indexed_box3_faces(box,&p->eye,faces,materials,&count);
    if (st!=SAT_OK) return st;
    for (uint8_t face=0;face<count;++face) {
        const sat_result_t drawn=sat_draw_indexed_solid_quad3(
            &faces[face],p,materials[face]);
        if (drawn!=SAT_OK) return drawn;
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
    sat_result_t queued;
    if(captured(quad,SAT_SCENE3D_INDEXED_TEXTURED,0u,texture,nullptr,
                p->color_calc_slot,nullptr,&queued)) {
        if(queued==SAT_OK && out_drawn!=nullptr) *out_drawn=1u;
        return queued;
    }

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
    /* Partly off-screen is fine: the VDP1 system clipping trims the sprite.
     * Only a corner so far out that projection would clamp it, or a sprite
     * stretched well past the screen, is handed back to the caller. */
    for(uint8_t i=0u;i<4u;++i) {
        if(!saturn::core::render3d::coord_drawable(
               projected.x[i],projected.y[i],p->width,p->height))
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
inline bool valid_grid(uint8_t grid) {
    return grid==2u || grid==4u || grid==8u;
}
inline const sat_vdp1_texture_t* tiled_cell(
    const sat_indexed_tiled_quad3_t& regions,uint8_t cell) {
    return regions.grid==0u ? regions.tiles[cell] : &regions.cells[cell];
}
inline bool valid_tiled_regions(const sat_indexed_tiled_quad3_t* regions) {
    if(regions==nullptr || regions->full==nullptr ||
       regions->full->valid==0u ||
       (regions->grid!=0u && (!valid_grid(regions->grid) ||
                              regions->cells==nullptr)))
        return false;
    const sat_vdp1_texture_t& whole=*regions->full;
    const uint8_t n=regions->grid==0u ? 2u : regions->grid;
    if(whole.width==0u || whole.width%(8u*n)!=0u ||
       whole.height<n || whole.height%n!=0u)
        return false;
    for(uint8_t i=0u;i<n*n;++i) {
        const sat_vdp1_texture_t* tile=tiled_cell(*regions,i);
        if(tile==nullptr || tile->valid==0u ||
           tile->width!=whole.width/n ||
           tile->height!=whole.height/n ||
           tile->palette!=whole.palette)
            return false;
    }
    return true;
}

/* Point (col/n, row/n) of the bilinear patch over quad corners TL, TR, BR,
 * BL. Shared cell edges evaluate the same (col,row), so cells tile without
 * cracks. */
sat_vec3_t grid_point(const sat_quad3_t& q,uint8_t n,uint8_t col,uint8_t row) {
    auto axis=[&](sat_fx16_t tl,sat_fx16_t tr,sat_fx16_t br,sat_fx16_t bl) {
        const int64_t top=static_cast<int64_t>(tl)*(n-col)+
                          static_cast<int64_t>(tr)*col;
        const int64_t bottom=static_cast<int64_t>(bl)*(n-col)+
                             static_cast<int64_t>(br)*col;
        return static_cast<sat_fx16_t>((top*(n-row)+bottom*row)/(n*n));
    };
    return sat_vec3_t{
        axis(q.v[0].x,q.v[1].x,q.v[2].x,q.v[3].x),
        axis(q.v[0].y,q.v[1].y,q.v[2].y,q.v[3].y),
        axis(q.v[0].z,q.v[1].z,q.v[2].z,q.v[3].z)};
}
}

extern "C" sat_result_t sat_upload_indexed8_quadrants(
    const uint8_t* pixels,uint16_t width,uint16_t height,
    uint16_t source_pitch,uint16_t palette_bank,
    sat_vdp1_texture_t tiles[4],uint8_t* scratch,
    uint32_t scratch_capacity
) {
    if(pixels==nullptr || tiles==nullptr || scratch==nullptr ||
       width<16u || width>504u || (width&15u)!=0u ||
       height<2u || height>254u || (height&1u)!=0u ||
       source_pitch<width)
        return SAT_ERR_INVALID_ARG;
    const uint16_t tile_w=static_cast<uint16_t>(width/2u);
    const uint16_t tile_h=static_cast<uint16_t>(height/2u);
    const uint32_t tile_bytes=static_cast<uint32_t>(tile_w)*tile_h;
    if(scratch_capacity<tile_bytes) return SAT_ERR_CAPACITY;
    for(uint8_t row=0u;row<2u;++row) for(uint8_t col=0u;col<2u;++col) {
        for(uint16_t py=0u;py<tile_h;++py) for(uint16_t px=0u;px<tile_w;++px) {
            const uint32_t src_y=static_cast<uint32_t>(row)*tile_h+py;
            const uint32_t src_x=static_cast<uint32_t>(col)*tile_w+px;
            scratch[static_cast<uint32_t>(py)*tile_w+px]=
                pixels[src_y*source_pitch+src_x];
        }
        const uint8_t tile=static_cast<uint8_t>(row*2u+col);
        const sat_result_t st=sat_tex_upload_indexed8_pixels(
            &tiles[tile],scratch,tile_w,tile_h,palette_bank);
        if(st!=SAT_OK) return st;
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_draw_indexed_tiled_quad3(
    const sat_quad3_t* quad,const sat_indexed_solid_render3d_t* params,
    const sat_indexed_tiled_quad3_t* regions,uint8_t* out_submitted
) {
    if(out_submitted!=nullptr) *out_submitted=0u;
    if(quad==nullptr || !valid_render(params) || !valid_tiled_regions(regions))
        return SAT_ERR_INVALID_ARG;
    sat_result_t queued;
    if(captured(quad,SAT_SCENE3D_INDEXED_TILED,0u,nullptr,regions,
                params->color_calc_slot,nullptr,&queued)) {
        if(queued==SAT_OK && out_submitted!=nullptr) *out_submitted=1u;
        return queued;
    }
    uint8_t drawn=0u;
    sat_result_t st=sat_draw_indexed_textured_quad3(
        quad,params,regions->full,&drawn);
    if(st!=SAT_OK) return st;
    if(drawn!=0u) {
        if(out_submitted!=nullptr) *out_submitted=1u;
        return SAT_OK;
    }

    /* UV regions correspond to these exact subdivision quads. Caller
     * must supply a planar affine surface for the intended texel mapping.
     * The 2x2 tiles[] keep their original midpoint split. */
    const uint8_t n=regions->grid==0u ? 2u : regions->grid;
    const bool fill=regions->cell_rgb555!=nullptr &&
                    params->color_calc_slot==SAT_INDEXED_SOLID_OPAQUE;
    const sat_vec3_t ab=tile_midpoint(quad->v[0],quad->v[1]);
    const sat_vec3_t bc=tile_midpoint(quad->v[1],quad->v[2]);
    const sat_vec3_t cd=tile_midpoint(quad->v[2],quad->v[3]);
    const sat_vec3_t da=tile_midpoint(quad->v[3],quad->v[0]);
    const sat_vec3_t center=tile_center(*quad);
    const sat_quad3_t quadrant[4]={
        {{quad->v[0],ab,center,da}}, /* TL */
        {{ab,quad->v[1],bc,center}}, /* TR */
        {{da,center,cd,quad->v[3]}}, /* BL */
        {{center,bc,quad->v[2],cd}}  /* BR */
    };
    for(uint8_t i=0u;i<n*n;++i) {
        const uint8_t row=static_cast<uint8_t>(i/n),col=static_cast<uint8_t>(i%n);
        sat_quad3_t cell;
        if(regions->grid==0u) {
            cell=quadrant[i];
        } else {
            cell.v[0]=grid_point(*quad,n,col,row);
            cell.v[1]=grid_point(*quad,n,static_cast<uint8_t>(col+1u),row);
            cell.v[2]=grid_point(*quad,n,static_cast<uint8_t>(col+1u),
                                 static_cast<uint8_t>(row+1u));
            cell.v[3]=grid_point(*quad,n,col,static_cast<uint8_t>(row+1u));
        }
        drawn=0u;
        st=sat_draw_indexed_textured_quad3(
            &cell,params,tiled_cell(*regions,i),&drawn);
        if(st!=SAT_OK) return st;
        if(out_submitted!=nullptr)
            *out_submitted=static_cast<uint8_t>(*out_submitted+drawn);
        /* A cut cell: clip it as a solid of its average colour rather than
         * stretch its texture over the visible piece. */
        if(drawn==0u && fill && regions->cell_rgb555[i]!=0u) {
            st=sat_draw_polygon_quad3(&cell,params,regions->cell_rgb555[i]);
            if(st!=SAT_OK) return st;
        }
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_upload_indexed8_grid(
    const uint8_t* pixels,uint16_t width,uint16_t height,
    uint16_t source_pitch,uint16_t palette_bank,uint8_t grid,
    sat_vdp1_texture_t* cells,uint8_t* scratch,uint32_t scratch_capacity,
    const uint16_t* palette_rgb555,uint16_t* out_cell_rgb555
) {
    if(pixels==nullptr || cells==nullptr || scratch==nullptr ||
       !valid_grid(grid) ||
       width==0u || width>504u || width%(8u*grid)!=0u ||
       height<grid || height>255u || height%grid!=0u ||
       source_pitch<width ||
       (out_cell_rgb555!=nullptr && palette_rgb555==nullptr))
        return SAT_ERR_INVALID_ARG;
    const uint16_t cell_w=static_cast<uint16_t>(width/grid);
    const uint16_t cell_h=static_cast<uint16_t>(height/grid);
    if(scratch_capacity<static_cast<uint32_t>(cell_w)*cell_h)
        return SAT_ERR_CAPACITY;
    for(uint8_t row=0u;row<grid;++row) for(uint8_t col=0u;col<grid;++col) {
        uint32_t sum[3]={0u,0u,0u},opaque=0u;
        for(uint16_t py=0u;py<cell_h;++py) for(uint16_t px=0u;px<cell_w;++px) {
            const uint8_t index=pixels[
                (static_cast<uint32_t>(row)*cell_h+py)*source_pitch+
                static_cast<uint32_t>(col)*cell_w+px];
            scratch[static_cast<uint32_t>(py)*cell_w+px]=index;
            if(out_cell_rgb555!=nullptr && index!=0u) {
                const uint16_t rgb=palette_rgb555[index];
                sum[0]+=rgb&0x1Fu;
                sum[1]+=(rgb>>5)&0x1Fu;
                sum[2]+=(rgb>>10)&0x1Fu;
                ++opaque;
            }
        }
        const uint8_t cell=static_cast<uint8_t>(row*grid+col);
        if(out_cell_rgb555!=nullptr) {
            out_cell_rgb555[cell]=opaque==0u ? 0u : static_cast<uint16_t>(
                0x8000u |
                ((sum[0]+opaque/2u)/opaque) |
                (((sum[1]+opaque/2u)/opaque)<<5) |
                (((sum[2]+opaque/2u)/opaque)<<10));
        }
        const sat_result_t st=sat_tex_upload_indexed8_pixels(
            &cells[cell],scratch,cell_w,cell_h,palette_bank);
        if(st!=SAT_OK) return st;
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
