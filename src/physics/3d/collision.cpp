#include "saturn/collide3d.h"
#include "saturn/spatial3.h"
#include "src/physics/3d/collision_logic.hpp"
#include "src/physics/3d/collision_grid.hpp"
#include "src/physics/spatial/3d_logic.hpp"

using namespace saturn::core::collide3d;
extern "C" int sat_sphere_overlap(const sat_sphere_t*a,const sat_sphere_t*b){return a&&b&&a->radius>=0&&b->radius>=0&&sphere_overlap(*a,*b);}
extern "C" int sat_aabb3_overlap(const sat_aabb3_t*a,const sat_aabb3_t*b){return a&&b&&aabb_overlap(*a,*b);}
extern "C" int sat_sphere_aabb3_overlap(const sat_sphere_t*s,const sat_aabb3_t*b){return s&&b&&s->radius>=0&&sphere_aabb(*s,*b);}
extern "C" int sat_sphere_plane_overlap(const sat_sphere_t*s,const sat_plane3_t*p){sat_contact3_t c;return s&&p&&plane_contact(*s,*p,c);}
extern "C" int sat_sphere_contact(const sat_sphere_t*a,const sat_sphere_t*b,sat_contact3_t*o){return a&&b&&o&&sphere_contact(*a,*b,*o);}
extern "C" int sat_sphere_sphere_overlap(const sat_sphere_t*a,const sat_sphere_t*b){return sat_sphere_overlap(a,b);}
extern "C" int sat_sphere_sphere_contact(const sat_sphere_t*a,const sat_sphere_t*b,sat_contact3_t*o){return sat_sphere_contact(a,b,o);}
extern "C" int sat_aabb3_contact(const sat_aabb3_t*a,const sat_aabb3_t*b,sat_contact3_t*o){return a&&b&&o&&aabb_contact(*a,*b,*o);}
extern "C" int sat_sphere_aabb3_contact(const sat_sphere_t*s,const sat_aabb3_t*b,sat_contact3_t*o){return s&&b&&o&&sphere_box_contact(*s,*b,*o);}
extern "C" int sat_sphere_plane_contact(const sat_sphere_t*s,const sat_plane3_t*p,sat_contact3_t*o){return s&&p&&o&&plane_contact(*s,*p,*o);}
extern "C" int sat_raycast_sphere(const sat_sphere_t*s,const sat_ray3_t*r,sat_hit3_t*o){return s&&r&&o&&ray_sphere(*s,*r,*o);}
extern "C" int sat_raycast_aabb3(const sat_aabb3_t*b,const sat_ray3_t*r,sat_hit3_t*o){return b&&r&&o&&ray_box(*b,*r,*o);}
extern "C" int sat_raycast_quad3(const sat_quad3_t*q,const sat_ray3_t*r,sat_hit3_t*o){return q&&r&&o&&ray_quad(*q,*r,*o);}
extern "C" int sat_raycast_mesh(const sat_mesh_t*m,const sat_ray3_t*r,sat_hit3_t*o){return m&&r&&o&&ray_mesh(*m,*r,*o);}
extern "C" sat_result_t sat_sphere_mesh_contact(const sat_mesh_t*m,const sat_sphere_t*s,sat_contact3_t*o,uint16_t cap,uint16_t*n){if(!m||!s||!o||!n)return SAT_ERR_INVALID_ARG;return sphere_mesh(*m,*s,o,cap,*n);}
extern "C" sat_result_t sat_mesh3_grid_init(
    sat_mesh3_grid_t* grid,const sat_mesh_t* mesh,uint8_t cell_shift,uint16_t* heads,
    uint16_t bucket_count,sat_mesh3_grid_entry_t* entries,uint16_t entry_cap,
    uint16_t* stamps,uint16_t stamp_cap){
    if(!grid||!mesh)return SAT_ERR_INVALID_ARG;
    return mesh3_grid_init(*grid,*mesh,cell_shift,heads,bucket_count,entries,entry_cap,stamps,stamp_cap);
}
extern "C" sat_result_t sat_sphere_mesh_contact_grid(
    sat_mesh3_grid_t* grid,const sat_sphere_t* sphere,sat_contact3_t* out,
    uint16_t cap,uint16_t* count){
    if(!grid||!sphere||!count||(!out&&cap))return SAT_ERR_INVALID_ARG;
    uint16_t n=0;const sat_result_t st=sphere_mesh_grid(*grid,*sphere,out,cap,n);*count=n;return st;
}

extern "C" void sat_body3_step(sat_body3_t*b,const sat_body3_params_t*p){
    if(!b||!p)return;
    b->flags=0;b->vel.x=static_cast<sat_fx16_t>((static_cast<int64_t>(b->vel.x)*p->drag)>>16);b->vel.z=static_cast<sat_fx16_t>((static_cast<int64_t>(b->vel.z)*p->drag)>>16);b->vel=add(b->vel,p->gravity);
    if(p->max_fall>0&&b->vel.y<-p->max_fall)b->vel.y=-p->max_fall;
}
static void body_contact(sat_body3_t&b,const sat_contact3_t&c,const sat_body3_params_t& p){
    b.shape.center=add(b.shape.center,mul(c.normal,c.depth));
    const int64_t d=vec3_dot_raw(b.vel,c.normal)>>16;
    if(d<0){b.vel=sub(b.vel,mul(c.normal,static_cast<sat_fx16_t>(d)));if(p.restitution) b.vel=add(b.vel,mul(c.normal,static_cast<sat_fx16_t>((-d*p.restitution)>>16)));}
    if(c.normal.y>45875){b.flags|=SAT_BODY3_GROUNDED;b.vel.x=static_cast<sat_fx16_t>((static_cast<int64_t>(b.vel.x)*p.floor_friction)>>16);b.vel.z=static_cast<sat_fx16_t>((static_cast<int64_t>(b.vel.z)*p.floor_friction)>>16);}else b.flags|=SAT_BODY3_HIT_WALL;
}
extern "C" sat_result_t sat_body3_collide_aabbs(sat_body3_t*b,const sat_aabb3_t*boxes,uint16_t count){
    if (!b || (!boxes && count)) return SAT_ERR_INVALID_ARG;
    sat_body3_params_t p = {{0,0,0},0,0,SAT_FX16_ONE,SAT_FX16_ONE};
    const sat_fx16_t l = fx_len3(b->vel.x,b->vel.y,b->vel.z);
    const uint16_t steps = (b->shape.radius && l > b->shape.radius)
        ? static_cast<uint16_t>((l+b->shape.radius-1)/b->shape.radius) : 1;
    const sat_vec3_t d = mul(b->vel,static_cast<sat_fx16_t>(SAT_FX16_ONE/steps));
    for (uint16_t s=0;s<steps;++s) {
        b->shape.center = add(b->shape.center,d);
        for (int it=0;it<3;++it) {
            int hit=0;
            for (uint16_t i=0;i<count;++i) { sat_contact3_t c; if (sphere_box_contact(b->shape,boxes[i],c)) { body_contact(*b,c,p); hit=1; } }
            if (!hit) break;
        }
    }
    return SAT_OK;
}
extern "C" sat_result_t sat_body3_collide_spatial_aabbs(
    sat_body3_t* b, sat_spatial3_t* spatial
) {
    if (!b || b->shape.radius < 0 || !saturn::core::spatial3::valid(spatial)) {
        return SAT_ERR_INVALID_ARG;
    }
    sat_body3_params_t p = {{0,0,0},0,0,SAT_FX16_ONE,SAT_FX16_ONE};
    const sat_fx16_t l = fx_len3(b->vel.x,b->vel.y,b->vel.z);
    const uint16_t steps = (b->shape.radius && l > b->shape.radius)
        ? static_cast<uint16_t>((l+b->shape.radius-1)/b->shape.radius) : 1;
    const sat_vec3_t d = mul(b->vel,static_cast<sat_fx16_t>(SAT_FX16_ONE/steps));
    for (uint16_t s=0;s<steps;++s) {
        b->shape.center = add(b->shape.center,d);
        for (int it=0;it<3;++it) {
            const sat_fx16_t r = b->shape.radius;
            const sat_aabb3_t query = {b->shape.center,{r,r,r}};
            int hit = 0;
            const sat_result_t st = saturn::core::spatial3::query_each(
                *spatial, query, [&](uint16_t id) {
                    sat_contact3_t c;
                    if (sphere_box_contact(b->shape, spatial->items[id], c)) {
                        body_contact(*b, c, p);
                        hit = 1;
                    }
                });
            if (st != SAT_OK) return st;
            if (!hit) break;
        }
    }
    return SAT_OK;
}
extern "C" sat_result_t sat_body3_collide_mesh(sat_body3_t*b,const sat_mesh_t*m){
    if (!b || !m) return SAT_ERR_INVALID_ARG;
    const sat_fx16_t l=fx_len3(b->vel.x,b->vel.y,b->vel.z);
    const uint16_t steps=(b->shape.radius&&l>b->shape.radius)?static_cast<uint16_t>((l+b->shape.radius-1)/b->shape.radius):1;
    const sat_vec3_t d=mul(b->vel,static_cast<sat_fx16_t>(SAT_FX16_ONE/steps));
    sat_body3_params_t p={{0,0,0},0,0,SAT_FX16_ONE,SAT_FX16_ONE};
    for(uint16_t s=0;s<steps;++s){
        b->shape.center=add(b->shape.center,d);sat_contact3_t contacts[8];uint16_t n=0;
        sat_sphere_mesh_contact(m,&b->shape,contacts,8,&n);
        for(uint16_t i=0;i<n;++i) body_contact(*b,contacts[i],p);
    }
    return SAT_OK;
}
extern "C" sat_result_t sat_body3_collide_mesh_grid(sat_body3_t*b,sat_mesh3_grid_t*grid){
    if(!b||!mesh3_grid_valid(grid))return SAT_ERR_INVALID_ARG;
    const sat_fx16_t l=fx_len3(b->vel.x,b->vel.y,b->vel.z);
    const uint16_t steps=(b->shape.radius&&l>b->shape.radius)?static_cast<uint16_t>((l+b->shape.radius-1)/b->shape.radius):1;
    const sat_vec3_t d=mul(b->vel,static_cast<sat_fx16_t>(SAT_FX16_ONE/steps));
    sat_body3_params_t p={{0,0,0},0,0,SAT_FX16_ONE,SAT_FX16_ONE};
    for(uint16_t s=0;s<steps;++s){
        b->shape.center=add(b->shape.center,d);
        sat_contact3_t contacts[8];uint16_t n=0;
        const sat_result_t st=sphere_mesh_grid(*grid,b->shape,contacts,8,n);
        if(st!=SAT_OK&&st!=SAT_ERR_CAPACITY)return st;
        for(uint16_t i=0;i<n;++i)body_contact(*b,contacts[i],p);
    }
    return SAT_OK;
}
extern "C" int sat_body3_separate(sat_body3_t*a,sat_body3_t*b){if(!a||!b)return 0;sat_contact3_t c;if(!sphere_contact(a->shape,b->shape,c))return 0;const sat_fx16_t h=c.depth/2;a->shape.center=add(a->shape.center,mul(c.normal,h));b->shape.center=sub(b->shape.center,mul(c.normal,h));const int64_t d=vec3_dot_raw(sub(a->vel,b->vel),c.normal)>>16;if(d<0){const sat_fx16_t hspeed=static_cast<sat_fx16_t>(-d/2);a->vel=add(a->vel,mul(c.normal,hspeed));b->vel=sub(b->vel,mul(c.normal,hspeed));}return 1;}
