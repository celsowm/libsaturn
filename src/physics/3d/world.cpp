#include "saturn/physics3_world.h"
#include <limits.h>
#include "src/physics/3d/collision_grid.hpp"
#include "src/graphics/3d/geometry/math_logic.hpp"
namespace {
using V=sat_vec3_t;
using F=sat_fx16_t;
bool valid(const sat_physics3_world_t* w) {
    return w && w->actors && w->capacity && w->count<=w->capacity &&
        w->max_substeps && w->max_substeps<=64 && w->iterations && w->iterations<=8;
}
bool live(const sat_physics3_world_t* w,uint16_t id) {
    return valid(w) && id<w->count;
}
bool material_ok(const sat_physics3_material_t* m) {
    return m && m->friction>=0 && m->friction<=SAT_FX16_ONE &&
        m->restitution>=0 && m->restitution<=SAT_FX16_ONE;
}
bool fits(int64_t n) {return n>=INT32_MIN && n<=INT32_MAX;}
bool add_fits(V a,V b) {return fits((int64_t)a.x+b.x) &&
    fits((int64_t)a.y+b.y) && fits((int64_t)a.z+b.z);}
V add(V a,V b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
V sub(V a,V b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
F mul(F a,F b){return (F)(((int64_t)a*b)>>16);}
V scale(V v,F f){return {mul(v.x,f),mul(v.y,f),mul(v.z,f)};}
int64_t dot(V a,V b){return ((int64_t)a.x*b.x+
    (int64_t)a.y*b.y+(int64_t)a.z*b.z)>>16;}
int64_t ab(int64_t v){return v<0?-v:v;}
int64_t mx(int64_t a,int64_t b){return a>b?a:b;}
int64_t greatest(V v){return mx(ab(v.x),mx(ab(v.y),ab(v.z)));}
F mn(F a,F b){return a<b?a:b;}
constexpr F kMaxAngular=8*SAT_FX16_ONE;
constexpr F kMinRollingRadius=SAT_FX16_ONE/8;
constexpr F kTwoSevenths=(2*SAT_FX16_ONE)/7;
constexpr F kFiveSevenths=(5*SAT_FX16_ONE)/7;
F angular_clamp(int64_t value) {
    return (F)(value>kMaxAngular?kMaxAngular:
               value<-kMaxAngular?-kMaxAngular:value);
}
V cross(V a,V b) {
    return {(F)(((int64_t)a.y*b.z-(int64_t)a.z*b.y)>>16),
            (F)(((int64_t)a.z*b.x-(int64_t)a.x*b.z)>>16),
            (F)(((int64_t)a.x*b.y-(int64_t)a.y*b.x)>>16)};
}
bool angular_valid(V v) {
    return ab(v.x)<=kMaxAngular && ab(v.y)<=kMaxAngular &&
           ab(v.z)<=kMaxAngular;
}
/* Explicit Euler quaternion step in world frame, followed by fixed-point
 * normalization; no heap, trig, or matrix decomposition required. */
void rotate_sphere(sat_physics3_actor_t& a,uint16_t steps) {
    if(!a.rolling_enabled ||
       (!a.angular_velocity.x&&!a.angular_velocity.y&&!a.angular_velocity.z))
        return;
    const sat_physics3_quat_t q=a.orientation;
    const V omega=a.angular_velocity;
    const int64_t x=(int64_t)mul(omega.x,q.w)+mul(omega.y,q.z)-mul(omega.z,q.y);
    const int64_t y=-(int64_t)mul(omega.x,q.z)+mul(omega.y,q.w)+mul(omega.z,q.x);
    const int64_t z=(int64_t)mul(omega.x,q.y)-mul(omega.y,q.x)+mul(omega.z,q.w);
    const int64_t w=-(int64_t)mul(omega.x,q.x)-mul(omega.y,q.y)-mul(omega.z,q.z);
    const int64_t divisor=2u*steps;
    const int64_t next_x=(int64_t)q.x+x/divisor;
    const int64_t next_y=(int64_t)q.y+y/divisor;
    const int64_t next_z=(int64_t)q.z+z/divisor;
    const int64_t next_w=(int64_t)q.w+w/divisor;
    const uint64_t length2=(uint64_t)(next_x*next_x)+
        (uint64_t)(next_y*next_y)+(uint64_t)(next_z*next_z)+
        (uint64_t)(next_w*next_w);
    const uint64_t length=saturn::core::math3d::isqrt64(length2);
    if(!length||length>INT32_MAX)return;
    const int32_t n=(int32_t)length;
    const auto normalized=[&](int64_t component)->F {
        return (F)saturn::core::math3d::div_s64_s32(component<<16,n);
    };
    a.orientation={normalized(next_x),normalized(next_y),
                   normalized(next_z),normalized(next_w)};
}
/* Same finite, corner-aware swept sphere vs mesh primitive used for ramps:
 * represent the moving AABB as six convex quads at the beginning of the
 * substep, then subtract the AABB's translation from sphere displacement.
 * The returned contact point is mapped back into world coordinates at TOI.
 * The stack mesh is temporary and NEVER retained by the world. */
sat_result_t swept_kinematic_box(const sat_physics3_actor_t& platform,
    V start_box,V end_box,const sat_sphere_t& sphere,V sphere_delta,
    sat_sphere_mesh_hit_t* hit,uint8_t* found){
    if(!hit||!found)return SAT_ERR_INVALID_ARG;
    if(platform.box.half.x<=0||platform.box.half.y<=0||
       platform.box.half.z<=0)return SAT_ERR_INVALID_ARG;
    const V low=sub(start_box,platform.box.half);
    const V high=add(start_box,platform.box.half);
    V vertices[8]={
        {low.x,low.y,low.z},{high.x,low.y,low.z},
        {high.x,low.y,high.z},{low.x,low.y,high.z},
        {low.x,high.y,low.z},{high.x,high.y,low.z},
        {high.x,high.y,high.z},{low.x,high.y,high.z}};
    uint16_t indices[24]={
        0,1,2,3, 4,5,6,7,
        0,1,5,4, 1,2,6,5,
        2,3,7,6, 3,0,4,7};
    const sat_mesh_t box_mesh{vertices,indices,8,8,6,6};
    const V motion=sub(end_box,start_box);
    if(!fits((int64_t)sphere_delta.x-motion.x)||
       !fits((int64_t)sphere_delta.y-motion.y)||
       !fits((int64_t)sphere_delta.z-motion.z))
        return SAT_ERR_INVALID_ARG;
    const V relative_delta=sub(sphere_delta,motion);
    sat_sphere_mesh_hit_t candidate{};
    uint8_t has_hit=0;
    const sat_result_t status=sat_sphere_cast_mesh(
        &box_mesh,&sphere,&relative_delta,&candidate,&has_hit);
    if(status!=SAT_OK)return status;
    *found=has_hit;
    if(has_hit){
        const V offset=scale(motion,candidate.t);
        if(!add_fits(candidate.center,offset)||
           !add_fits(candidate.point,offset))
            return SAT_ERR_INVALID_ARG;
        candidate.center=add(candidate.center,offset);
        candidate.point=add(candidate.point,offset);
        *hit=candidate;
    }
    return SAT_OK;
}
sat_result_t swept_kinematic_mesh(const sat_physics3_actor_t& platform,
    V start_offset,V end_offset,const sat_sphere_t& sphere,
    V sphere_delta,sat_sphere_mesh_hit_t* out,uint8_t* found) {
    if(!out||!found||!platform.mesh)return SAT_ERR_INVALID_ARG;
    const V motion=sub(end_offset,start_offset);
    if(!fits((int64_t)sphere.center.x-start_offset.x)||
       !fits((int64_t)sphere.center.y-start_offset.y)||
       !fits((int64_t)sphere.center.z-start_offset.z)||
       !fits((int64_t)sphere_delta.x-motion.x)||
       !fits((int64_t)sphere_delta.y-motion.y)||
       !fits((int64_t)sphere_delta.z-motion.z))
        return SAT_ERR_INVALID_ARG;
    const sat_sphere_t reference_sphere{
        sub(sphere.center,start_offset),sphere.radius};
    const V reference_delta=sub(sphere_delta,motion);
    sat_sphere_mesh_hit_t hit{};
    uint8_t has_hit=0;
    const sat_result_t status=sat_sphere_cast_mesh(
        platform.mesh,&reference_sphere,&reference_delta,
        &hit,&has_hit);
    if(status!=SAT_OK)return status;
    if(has_hit){
        const V impact_offset=add(start_offset,scale(motion,hit.t));
        if(!add_fits(hit.point,impact_offset)||
           !add_fits(hit.center,impact_offset))
            return SAT_ERR_INVALID_ARG;
        hit.point=add(hit.point,impact_offset);
        hit.center=add(hit.center,impact_offset);
        *out=hit;
    }
    *found=has_hit;
    return SAT_OK;
}
/* Fixed-point quaternion transforms preserve the caller-owned reference mesh.
 * Restrict rotational actors to local 128-unit geometry during registration.
 * Using inverse pose on the sphere makes the existing mesh contact/grid logic
 * reusable, then rotates normals back into the world frame. */
sat_physics3_quat_t q_identity(){return {0,0,0,SAT_FX16_ONE};}
bool q_equal(sat_physics3_quat_t a,sat_physics3_quat_t b){
    return a.x==b.x&&a.y==b.y&&a.z==b.z&&a.w==b.w;
}
bool q_unit(sat_physics3_quat_t q){
    const int64_t v[4]={q.x,q.y,q.z,q.w};
    uint64_t square=0;
    for(int k=0;k<4;++k){
        if(v[k]<-SAT_FX16_ONE||v[k]>SAT_FX16_ONE)return false;
        square+=(uint64_t)(v[k]*v[k]);
    }
    const int64_t length=(int64_t)saturn::core::math3d::isqrt64(square);
    return ab(length-SAT_FX16_ONE)<=128;
}
sat_physics3_quat_t q_inverse(sat_physics3_quat_t q){
    return {-q.x,-q.y,-q.z,q.w};
}
bool q_rotate(sat_physics3_quat_t q,V p,V& out){
    const int64_t xx=mul(q.x,q.x),yy=mul(q.y,q.y),zz=mul(q.z,q.z);
    const int64_t xy=mul(q.x,q.y),xz=mul(q.x,q.z),yz=mul(q.y,q.z);
    const int64_t wx=mul(q.w,q.x),wy=mul(q.w,q.y),wz=mul(q.w,q.z);
    const int64_t m00=SAT_FX16_ONE-2*(yy+zz),m01=2*(xy-wz),
                  m02=2*(xz+wy),m10=2*(xy+wz),
                  m11=SAT_FX16_ONE-2*(xx+zz),m12=2*(yz-wx),
                  m20=2*(xz-wy),m21=2*(yz+wx),
                  m22=SAT_FX16_ONE-2*(xx+yy);
    const int64_t x=(m00*p.x+m01*p.y+m02*p.z)>>16;
    const int64_t y=(m10*p.x+m11*p.y+m12*p.z)>>16;
    const int64_t z=(m20*p.x+m21*p.y+m22*p.z)>>16;
    if(!fits(x)||!fits(y)||!fits(z))return false;
    out={(F)x,(F)y,(F)z};
    return true;
}
bool q_lerp(sat_physics3_quat_t a,sat_physics3_quat_t b,
            uint16_t step,uint16_t total,sat_physics3_quat_t& out){
    const int64_t v[4]={a.x+(int64_t)(b.x-a.x)*step/total,
                        a.y+(int64_t)(b.y-a.y)*step/total,
                        a.z+(int64_t)(b.z-a.z)*step/total,
                        a.w+(int64_t)(b.w-a.w)*step/total};
    const uint64_t len2=(uint64_t)(v[0]*v[0])+
        (uint64_t)(v[1]*v[1])+(uint64_t)(v[2]*v[2])+
        (uint64_t)(v[3]*v[3]);
    const int64_t len=(int64_t)saturn::core::math3d::isqrt64(len2);
    if(len<=0||len>INT32_MAX)return false;
    const auto norm=[&](int64_t c)->F{
        return (F)saturn::core::math3d::div_s64_s32(c<<16,(int32_t)len);
    };
    out={norm(v[0]),norm(v[1]),norm(v[2]),norm(v[3])};
    return true;
}
bool q_rotation_bounded(sat_physics3_quat_t a,sat_physics3_quat_t b){
    /* dot >= cos(22.5deg): quaternion half-angle implies <=45deg/tick. */
    const int64_t dot4=(int64_t)a.x*b.x+(int64_t)a.y*b.y+
        (int64_t)a.z*b.z+(int64_t)a.w*b.w;
    return dot4>=((int64_t)60547<<16);
}
bool mesh_local_radius_bounded(V lo,V hi){
    return mx(ab(lo.x),ab(hi.x))<=128*SAT_FX16_ONE &&
           mx(ab(lo.y),ab(hi.y))<=128*SAT_FX16_ONE &&
           mx(ab(lo.z),ab(hi.z))<=128*SAT_FX16_ONE;
}
V at(V begin,V end,uint16_t step,uint16_t total){
    return {(F)((int64_t)begin.x+((int64_t)end.x-begin.x)*step/total),
            (F)((int64_t)begin.y+((int64_t)end.y-begin.y)*step/total),
            (F)((int64_t)begin.z+((int64_t)end.z-begin.z)*step/total)};
}
void resolve(sat_physics3_actor_t& ball,const sat_physics3_actor_t& box,
             const sat_contact3_t& c,const V* surface_motion=nullptr) {
    ball.sphere.shape.center=add(ball.sphere.shape.center,scale(c.normal,c.depth));
    const V motion=surface_motion?*surface_motion:box.frame_motion;
    V relative=sub(ball.sphere.vel,motion);
    const int64_t vn=dot(relative,c.normal);
    if(vn<0){
        const F rest=mn(ball.material.restitution,box.material.restitution);
        const F impulse=(F)((vn*(SAT_FX16_ONE+(int64_t)rest))>>16);
        relative=sub(relative,scale(c.normal,impulse));
    }
    if(c.normal.y>45875){
        ball.sphere.flags|=SAT_BODY3_GROUNDED;
        const F friction=mn(ball.material.friction,box.material.friction);
        const V tangent=sub(relative,scale(c.normal,(F)dot(relative,c.normal)));
        if(ball.rolling_enabled){
            /* Contact point is -radius*normal relative to center; surface
             * slip includes tangential translation AND angular motion.
             * For solid-sphere inertia 2/5*m*r^2, an impulse to eliminate
             * slip contributes -2/7*slip to v and +(5/7)/r*n×slip to omega. */
            const V surface_spin=scale(cross(ball.angular_velocity,c.normal),
                                       ball.sphere.shape.radius);
            const V slip=sub(tangent,surface_spin);
            const F translation=mul(friction,kTwoSevenths);
            const F angular=mul(friction,kFiveSevenths);
            relative=sub(relative,scale(slip,translation));
            const V torque=cross(c.normal,slip);
            const F radius=ball.sphere.shape.radius;
            const V spin_delta={
                angular_clamp(((int64_t)mul(torque.x,angular)<<16)/radius),
                angular_clamp(((int64_t)mul(torque.y,angular)<<16)/radius),
                angular_clamp(((int64_t)mul(torque.z,angular)<<16)/radius)};
            const V omega=ball.angular_velocity;
            ball.angular_velocity={
                angular_clamp((int64_t)omega.x+spin_delta.x),
                angular_clamp((int64_t)omega.y+spin_delta.y),
                angular_clamp((int64_t)omega.z+spin_delta.z)};
        } else relative=sub(relative,scale(tangent,friction));
    } else ball.sphere.flags|=SAT_BODY3_HIT_WALL;
    ball.sphere.vel=add(relative,motion);
}
} // namespace
extern "C" sat_result_t sat_physics3_world_init(sat_physics3_world_t* w,
    sat_physics3_actor_t* storage,uint16_t capacity,V gravity,
    uint8_t max_substeps,uint8_t iterations){
    if(!w||!storage||!capacity||capacity==0xffffu||!max_substeps||
       max_substeps>64||!iterations||iterations>8)return SAT_ERR_INVALID_ARG;
    *w={};w->actors=storage;w->capacity=capacity;w->gravity=gravity;
    w->max_substeps=max_substeps;w->iterations=iterations;return SAT_OK;
}
extern "C" void sat_physics3_world_reset(sat_physics3_world_t* w){
    if(valid(w))w->count=0;
}
extern "C" sat_result_t sat_physics3_add_box(sat_physics3_world_t* w,
    sat_physics3_kind_t kind,const sat_aabb3_t* box,
    const sat_physics3_material_t* material,uint16_t* id){
    if(!valid(w)||!box||!id||!material_ok(material)||
       (kind!=SAT_PHYSICS3_STATIC_BOX&&kind!=SAT_PHYSICS3_KINEMATIC_BOX)||
       box->half.x<=0||box->half.y<=0||box->half.z<=0)
        return SAT_ERR_INVALID_ARG;
    if(w->count==w->capacity)return SAT_ERR_CAPACITY;
    const uint16_t next=w->count;
    sat_physics3_actor_t& a=w->actors[next];a={};
    a.kind=kind;a.material=*material;a.box=*box;a.target_center=box->center;
    w->count=(uint16_t)(next+1u);*id=next;return SAT_OK;
}
extern "C" sat_result_t sat_physics3_add_plane(
    sat_physics3_world_t* w,const sat_plane3_t* plane,
    const sat_physics3_material_t* material,uint16_t* id) {
    if(!valid(w)||!plane||!material_ok(material)||!id||
       (!plane->normal.x&&!plane->normal.y&&!plane->normal.z))
        return SAT_ERR_INVALID_ARG;
    if(w->count==w->capacity)return SAT_ERR_CAPACITY;
    const uint16_t next=w->count;
    sat_physics3_actor_t& a=w->actors[next];a={};
    a.kind=SAT_PHYSICS3_STATIC_PLANE;
    a.material=*material;
    a.plane=*plane;
    w->count=(uint16_t)(next+1u);*id=next;return SAT_OK;
}
extern "C" sat_result_t sat_physics3_set_mesh_contacts(
    sat_physics3_world_t* w, sat_contact3_t* storage, uint16_t capacity) {
    if(!valid(w)||!storage||!capacity)return SAT_ERR_INVALID_ARG;
    /* Changing storage invalidates a prior capacity guarantee unless the new
     * buffer still accommodates every registered mesh. */
    for(uint16_t i=0;i<w->count;++i) {
        const sat_physics3_actor_t& a=w->actors[i];
        if((a.kind==SAT_PHYSICS3_STATIC_MESH ||
            a.kind==SAT_PHYSICS3_KINEMATIC_MESH) &&
           (!a.mesh||a.mesh->face_count>capacity))return SAT_ERR_CAPACITY;
    }
    w->mesh_contacts=storage;
    w->mesh_contact_capacity=capacity;
    return SAT_OK;
}
extern "C" sat_result_t sat_physics3_add_mesh(
    sat_physics3_world_t* w, const sat_mesh_t* mesh,
    const sat_physics3_material_t* material, uint16_t* id) {
    if(!valid(w)||!mesh||!id||!material_ok(material)||
       !mesh->vertices||!mesh->indices||!mesh->vertex_count||
       !mesh->face_count||mesh->vertex_count>mesh->vertex_cap||
       mesh->face_count>mesh->face_cap)return SAT_ERR_INVALID_ARG;
    if(!w->mesh_contacts||w->mesh_contact_capacity<mesh->face_count)
        return SAT_ERR_CAPACITY;
    if(w->count==w->capacity)return SAT_ERR_CAPACITY;
    for(uint32_t f=0;f<(uint32_t)mesh->face_count*4u;++f)
        if(mesh->indices[f]>=mesh->vertex_count)return SAT_ERR_INVALID_ARG;
    const uint16_t next=w->count;
    sat_physics3_actor_t& a=w->actors[next];a={};
    a.kind=SAT_PHYSICS3_STATIC_MESH;a.material=*material;a.mesh=mesh;
    w->count=(uint16_t)(next+1u);*id=next;return SAT_OK;
}
extern "C" sat_result_t sat_physics3_add_mesh_grid(
    sat_physics3_world_t* w, sat_mesh3_grid_t* grid,
    const sat_physics3_material_t* material, uint16_t* id) {
    if(!valid(w)||!id||!material_ok(material)||
       !saturn::core::collide3d::mesh3_grid_valid(grid)||
       !grid->mesh->vertices||!grid->mesh->indices||
       !grid->mesh->face_count||grid->mesh->face_count>grid->mesh->face_cap||
       grid->mesh->vertex_count>grid->mesh->vertex_cap||
       grid->entry_count>grid->entry_cap||!grid->entry_cap||
       !grid->bucket_count||!grid->heads||
       !grid->mesh->vertex_count||grid->cell_shift>15u)
        return SAT_ERR_INVALID_ARG;
    if(!w->mesh_contacts||w->mesh_contact_capacity<grid->mesh->face_count)
        return SAT_ERR_CAPACITY;
    if(w->count==w->capacity)return SAT_ERR_CAPACITY;
    const uint16_t next=w->count;
    sat_physics3_actor_t& a=w->actors[next];a={};
    a.kind=SAT_PHYSICS3_STATIC_MESH;
    a.material=*material;
    a.mesh=grid->mesh;
    a.mesh_grid=grid;
    w->count=(uint16_t)(next+1u);*id=next;return SAT_OK;
}
extern "C" sat_result_t sat_physics3_add_kinematic_mesh(
    sat_physics3_world_t* w,const sat_mesh_t* mesh,
    sat_mesh3_grid_t* grid,const V* initial_offset,
    const sat_physics3_material_t* material,uint16_t* id) {
    if(!valid(w)||!mesh||!initial_offset||!id||!material_ok(material))
        return SAT_ERR_INVALID_ARG;
    if(grid && (!saturn::core::collide3d::mesh3_grid_valid(grid) ||
                grid->mesh!=mesh || !grid->entry_cap ||
                grid->entry_count>grid->entry_cap || grid->cell_shift>15u))
        return SAT_ERR_INVALID_ARG;
    V low{},high{};
    if(mesh->vertices && mesh->vertex_count &&
       mesh->vertex_count<=mesh->vertex_cap){
        low=mesh->vertices[0];high=low;
        for(uint16_t i=1;i<mesh->vertex_count;++i){
            const V p=mesh->vertices[i];
            if(p.x<low.x)low.x=p.x;if(p.x>high.x)high.x=p.x;
            if(p.y<low.y)low.y=p.y;if(p.y>high.y)high.y=p.y;
            if(p.z<low.z)low.z=p.z;if(p.z>high.z)high.z=p.z;
        }
        if(!add_fits(low,*initial_offset)||
           !add_fits(high,*initial_offset))
            return SAT_ERR_INVALID_ARG;
    }
    /* Preserve the same registered-mesh scratch and index validation as
     * the static path, then enable translation only after success. */
    const sat_result_t status=sat_physics3_add_mesh(
        w,mesh,material,id);
    if(status!=SAT_OK)return status;
    sat_physics3_actor_t& a=w->actors[*id];
    a.kind=SAT_PHYSICS3_KINEMATIC_MESH;
    a.mesh_grid=grid;
    a.mesh_offset=*initial_offset;
    a.target_center=*initial_offset;
    a.mesh_bounds_min=low;
    a.mesh_bounds_max=high;
    a.mesh_orientation=q_identity();
    a.mesh_target_orientation=a.mesh_orientation;
    a.mesh_tick_start_orientation=a.mesh_orientation;
    return SAT_OK;
}
extern "C" sat_result_t sat_physics3_set_kinematic_mesh_orientation_target(
    sat_physics3_world_t* w,uint16_t id,
    const sat_physics3_quat_t* target){
    if(!live(w,id)||!target||
       w->actors[id].kind!=SAT_PHYSICS3_KINEMATIC_MESH||
       !q_unit(*target))return SAT_ERR_INVALID_ARG;
    sat_physics3_actor_t& a=w->actors[id];
    if(!mesh_local_radius_bounded(a.mesh_bounds_min,a.mesh_bounds_max)||
       !q_rotation_bounded(a.mesh_orientation,*target))
        return SAT_ERR_CAPACITY;
    a.mesh_target_orientation=*target;
    return SAT_OK;
}
extern "C" sat_result_t sat_physics3_set_kinematic_mesh_target(
    sat_physics3_world_t* w,uint16_t id,const V* target){
    if(!live(w,id)||!target ||
       w->actors[id].kind!=SAT_PHYSICS3_KINEMATIC_MESH)
        return SAT_ERR_INVALID_ARG;
    w->actors[id].target_center=*target;
    return SAT_OK;
}
extern "C" sat_result_t sat_physics3_add_sphere(sat_physics3_world_t* w,
    const sat_sphere_t* sphere,const V* velocity,
    const sat_physics3_material_t* material,uint16_t* id){
    if(!valid(w)||!sphere||!velocity||!id||sphere->radius<=0||
       !material_ok(material))return SAT_ERR_INVALID_ARG;
    if(w->count==w->capacity)return SAT_ERR_CAPACITY;
    const uint16_t next=w->count;
    sat_physics3_actor_t& a=w->actors[next];a={};
    a.kind=SAT_PHYSICS3_DYNAMIC_SPHERE;a.material=*material;
    a.sphere.shape=*sphere;a.sphere.vel=*velocity;
    a.orientation={0,0,0,SAT_FX16_ONE};
    w->count=(uint16_t)(next+1u);*id=next;return SAT_OK;
}
extern "C" sat_result_t sat_physics3_set_kinematic_target(
    sat_physics3_world_t* w,uint16_t id,const V* center){
    if(!live(w,id)||!center||
       w->actors[id].kind!=SAT_PHYSICS3_KINEMATIC_BOX)
        return SAT_ERR_INVALID_ARG;
    w->actors[id].target_center=*center;return SAT_OK;
}
extern "C" sat_result_t sat_physics3_set_rolling(
    sat_physics3_world_t* w,uint16_t id,int enabled){
    if(!live(w,id)||w->actors[id].kind!=SAT_PHYSICS3_DYNAMIC_SPHERE)
        return SAT_ERR_INVALID_ARG;
    sat_physics3_actor_t& a=w->actors[id];
    if(enabled && (a.sphere.shape.radius<kMinRollingRadius ||
                   !angular_valid(a.angular_velocity)))
        return SAT_ERR_INVALID_ARG;
    a.rolling_enabled=enabled?1u:0u;
    return SAT_OK;
}
extern "C" sat_result_t sat_physics3_set_angular_velocity(
    sat_physics3_world_t* w,uint16_t id,const V* omega){
    if(!live(w,id)||!omega ||
       w->actors[id].kind!=SAT_PHYSICS3_DYNAMIC_SPHERE||
       !angular_valid(*omega))return SAT_ERR_INVALID_ARG;
    w->actors[id].angular_velocity=*omega;
    return SAT_OK;
}
extern "C" sat_result_t sat_physics3_set_velocity(
    sat_physics3_world_t* w,uint16_t id,const V* velocity){
    if(!live(w,id)||!velocity||
       w->actors[id].kind!=SAT_PHYSICS3_DYNAMIC_SPHERE)
        return SAT_ERR_INVALID_ARG;
    w->actors[id].sphere.vel=*velocity;return SAT_OK;
}
extern "C" sat_result_t sat_physics3_sphere_model_matrix(
    const sat_physics3_world_t* world,uint16_t id,sat_mat4_t* out) {
    if(!live(world,id)||!out||
       world->actors[id].kind!=SAT_PHYSICS3_DYNAMIC_SPHERE)
        return SAT_ERR_INVALID_ARG;
    const sat_physics3_actor_t& actor=world->actors[id];
    const sat_physics3_quat_t q=actor.orientation;
    const F xx=mul(q.x,q.x), yy=mul(q.y,q.y), zz=mul(q.z,q.z);
    const F xy=mul(q.x,q.y), xz=mul(q.x,q.z), yz=mul(q.y,q.z);
    const F wx=mul(q.w,q.x), wy=mul(q.w,q.y), wz=mul(q.w,q.z);
    const V p=actor.sphere.shape.center;
    sat_mat4_t m{};
    m.m[0]=SAT_FX16_ONE-2*(yy+zz);
    m.m[1]=2*(xy-wz);
    m.m[2]=2*(xz+wy);
    m.m[3]=p.x;
    m.m[4]=2*(xy+wz);
    m.m[5]=SAT_FX16_ONE-2*(xx+zz);
    m.m[6]=2*(yz-wx);
    m.m[7]=p.y;
    m.m[8]=2*(xz-wy);
    m.m[9]=2*(yz+wx);
    m.m[10]=SAT_FX16_ONE-2*(xx+yy);
    m.m[11]=p.z;
    m.m[15]=SAT_FX16_ONE;
    *out=m;
    return SAT_OK;
}
extern "C" sat_result_t sat_physics3_get_actor(
    const sat_physics3_world_t* w,uint16_t id,sat_physics3_actor_t* out){
    if(!live(w,id)||!out)return SAT_ERR_INVALID_ARG;
    *out=w->actors[id];return SAT_OK;
}
extern "C" sat_result_t sat_physics3_set_mesh_face_ccd(
    sat_physics3_world_t* w,int enabled) {
    if(!valid(w))return SAT_ERR_INVALID_ARG;
    w->mesh_face_ccd=enabled?1u:0u;
    return SAT_OK;
}
extern "C" sat_result_t sat_physics3_set_kinematic_box_ccd(
    sat_physics3_world_t* w,int enabled){
    if(!valid(w))return SAT_ERR_INVALID_ARG;
    w->kinematic_box_ccd=enabled?1u:0u;
    return SAT_OK;
}
extern "C" sat_result_t sat_physics3_world_step(sat_physics3_world_t* w){
    if(!valid(w))return SAT_ERR_INVALID_ARG;
    if(!w->count)return SAT_OK;
    int64_t kinematic_speed=0,dynamic_speed=0;
    F smallest_radius=INT32_MAX;
    bool spheres=false,needs_discrete_budget=false;
    for(uint16_t i=0;i<w->count;++i){
        const sat_physics3_actor_t& a=w->actors[i];
        if(a.kind==SAT_PHYSICS3_KINEMATIC_BOX){
            const int64_t dx=(int64_t)a.target_center.x-a.box.center.x;
            const int64_t dy=(int64_t)a.target_center.y-a.box.center.y;
            const int64_t dz=(int64_t)a.target_center.z-a.box.center.z;
            kinematic_speed=mx(kinematic_speed,mx(ab(dx),mx(ab(dy),ab(dz))));
            if(!fits(dx)||!fits(dy)||!fits(dz))return SAT_ERR_INVALID_ARG;
            if(!w->kinematic_box_ccd)needs_discrete_budget=true;
        }else if(a.kind==SAT_PHYSICS3_DYNAMIC_SPHERE){
            if(a.sphere.shape.radius<=0 ||
               (a.rolling_enabled &&
                (a.sphere.shape.radius<kMinRollingRadius ||
                 !angular_valid(a.angular_velocity))) ||
               !add_fits(a.sphere.vel,w->gravity))
                return SAT_ERR_INVALID_ARG;
            const V v=add(a.sphere.vel,w->gravity);
            if(!add_fits(a.sphere.shape.center,v))return SAT_ERR_INVALID_ARG;
            spheres=true;smallest_radius=mn(smallest_radius,a.sphere.shape.radius);
            dynamic_speed=mx(dynamic_speed,greatest(v));
        }else if(a.kind==SAT_PHYSICS3_STATIC_MESH ||
                 a.kind==SAT_PHYSICS3_KINEMATIC_MESH){
            if(!w->mesh_face_ccd)needs_discrete_budget=true;
            if(a.kind==SAT_PHYSICS3_KINEMATIC_MESH){
                if(!q_unit(a.mesh_orientation)||
                   !q_unit(a.mesh_target_orientation)||
                   !q_rotation_bounded(a.mesh_orientation,
                                       a.mesh_target_orientation))
                    return SAT_ERR_INVALID_ARG;
                const bool rotated=!q_equal(a.mesh_orientation,q_identity())||
                    !q_equal(a.mesh_target_orientation,q_identity());
                if(rotated){
                    /* Rotational CCD is not implemented: NEVER silently
                     * accept high-speed motion under the translational guard. */
                    needs_discrete_budget=true;
                    if(!mesh_local_radius_bounded(a.mesh_bounds_min,
                                                   a.mesh_bounds_max))
                        return SAT_ERR_CAPACITY;
                    const int64_t lever=
                        mx(ab(a.mesh_bounds_min.x),ab(a.mesh_bounds_max.x))+
                        mx(ab(a.mesh_bounds_min.y),ab(a.mesh_bounds_max.y))+
                        mx(ab(a.mesh_bounds_min.z),ab(a.mesh_bounds_max.z));
                    const auto component_delta=[&](F before,F after)->int64_t{
                        return ab((int64_t)after-before);
                    };
                    const int64_t dq=mx(component_delta(a.mesh_orientation.x,
                                                         a.mesh_target_orientation.x),
                        mx(component_delta(a.mesh_orientation.y,
                                           a.mesh_target_orientation.y),
                        mx(component_delta(a.mesh_orientation.z,
                                           a.mesh_target_orientation.z),
                           component_delta(a.mesh_orientation.w,
                                           a.mesh_target_orientation.w))));
                    const int64_t swept_speed=(2*lever*dq)>>16;
                    kinematic_speed=mx(kinematic_speed,swept_speed);
                }
                const int64_t dx=(int64_t)a.target_center.x-a.mesh_offset.x;
                const int64_t dy=(int64_t)a.target_center.y-a.mesh_offset.y;
                const int64_t dz=(int64_t)a.target_center.z-a.mesh_offset.z;
                if(!fits(dx)||!fits(dy)||!fits(dz))
                    return SAT_ERR_INVALID_ARG;
                kinematic_speed=mx(kinematic_speed,
                    mx(ab(dx),mx(ab(dy),ab(dz))));
            }
            if(!a.mesh||!a.mesh->vertices||!a.mesh->indices||
               !a.mesh->face_count||!a.mesh->vertex_count||
               a.mesh->face_count>a.mesh->face_cap||
               a.mesh->vertex_count>a.mesh->vertex_cap||
               !w->mesh_contacts||
               w->mesh_contact_capacity<a.mesh->face_count||
               (a.mesh_grid &&
                   (!saturn::core::collide3d::mesh3_grid_valid(a.mesh_grid)||
                    a.mesh_grid->mesh!=a.mesh||
                    a.mesh_grid->entry_count>a.mesh_grid->entry_cap)) ||
               (a.kind==SAT_PHYSICS3_KINEMATIC_MESH &&
                (!add_fits(a.mesh_bounds_min,a.target_center) ||
                 !add_fits(a.mesh_bounds_max,a.target_center) ||
                 !add_fits(a.mesh_bounds_min,a.mesh_offset) ||
                 !add_fits(a.mesh_bounds_max,a.mesh_offset))))
                return SAT_ERR_INVALID_ARG;
        }else if(a.kind==SAT_PHYSICS3_STATIC_BOX ||
                 a.kind==SAT_PHYSICS3_STATIC_PLANE){
            needs_discrete_budget=true;
        }else return SAT_ERR_INVALID_ARG;
    }
    uint16_t steps=1;
    if(spheres){
        const int64_t stride=mx(1,smallest_radius/2);
        const int64_t distance=dynamic_speed+kinematic_speed;
        const int64_t required=distance?(distance+stride-1)/stride:1;
        if(required>w->max_substeps &&
           (needs_discrete_budget ||
            (!w->mesh_face_ccd&&!w->kinematic_box_ccd)))
            return SAT_ERR_CAPACITY;
        steps=static_cast<uint16_t>(required>w->max_substeps
            ? w->max_substeps : required);
    }
    for(uint16_t i=0;i<w->count;++i){
        sat_physics3_actor_t& a=w->actors[i];
        if(a.kind==SAT_PHYSICS3_KINEMATIC_BOX){
            a.frame_motion=sub(a.target_center,a.box.center);
        }else if(a.kind==SAT_PHYSICS3_KINEMATIC_MESH){
            a.frame_motion=sub(a.target_center,a.mesh_offset);
            a.mesh_tick_start_orientation=a.mesh_orientation;
        }else if(a.kind==SAT_PHYSICS3_DYNAMIC_SPHERE){
            a.sphere.flags=0;a.sphere.vel=add(a.sphere.vel,w->gravity);
        }
    }
    for(uint16_t step=1;step<=steps;++step){
        for(uint16_t i=0;i<w->count;++i){
            sat_physics3_actor_t& a=w->actors[i];
            if(a.kind!=SAT_PHYSICS3_KINEMATIC_BOX &&
               a.kind!=SAT_PHYSICS3_KINEMATIC_MESH)continue;
            const V start=sub(a.target_center,a.frame_motion);
            const V position=at(start,a.target_center,step,steps);
            if(a.kind==SAT_PHYSICS3_KINEMATIC_BOX)
                a.box.center=position;
            else {
                a.mesh_offset=position;
                if(!q_lerp(a.mesh_tick_start_orientation,
                           a.mesh_target_orientation,step,steps,
                           a.mesh_orientation))return SAT_ERR_INVALID_ARG;
            }
        }
        for(uint16_t i=0;i<w->count;++i){
            sat_physics3_actor_t& ball=w->actors[i];
            if(ball.kind!=SAT_PHYSICS3_DYNAMIC_SPHERE)continue;
            const V d={(F)(ball.sphere.vel.x/steps),
                       (F)(ball.sphere.vel.y/steps),
                       (F)(ball.sphere.vel.z/steps)};
            if(w->mesh_face_ccd||w->kinematic_box_ccd){
                sat_sphere_mesh_hit_t earliest{};
                uint16_t hit_actor=0xffffu;
                for(uint16_t j=0;j<w->count;++j){
                    const sat_physics3_actor_t& collider=w->actors[j];
                    sat_sphere_mesh_hit_t candidate{};
                    uint8_t found=0;
                    sat_result_t status=SAT_OK;
                    if(w->mesh_face_ccd &&
                       collider.kind==SAT_PHYSICS3_STATIC_MESH){
                        status=sat_sphere_cast_mesh(
                            collider.mesh,&ball.sphere.shape,&d,
                            &candidate,&found);
                    }else if(w->mesh_face_ccd &&
                             collider.kind==SAT_PHYSICS3_KINEMATIC_MESH &&
                             q_equal(collider.mesh_orientation,q_identity())&&
                             q_equal(collider.mesh_target_orientation,
                                     q_identity())){
                        const V tick_start=sub(
                            collider.target_center,collider.frame_motion);
                        const V start_offset=at(
                            tick_start,collider.target_center,
                            (uint16_t)(step-1u),steps);
                        status=swept_kinematic_mesh(
                            collider,start_offset,collider.mesh_offset,
                            ball.sphere.shape,d,&candidate,&found);
                    }else if(w->kinematic_box_ccd &&
                             collider.kind==SAT_PHYSICS3_KINEMATIC_BOX){
                        const V tick_start=sub(
                            collider.target_center,collider.frame_motion);
                        const V start_box=at(tick_start,collider.target_center,
                                             (uint16_t)(step-1u),steps);
                        status=swept_kinematic_box(
                            collider,start_box,collider.box.center,
                            ball.sphere.shape,d,&candidate,&found);
                    }else continue;
                    if(status!=SAT_OK)return status;
                    if(found && (hit_actor==0xffffu ||
                                 candidate.t<earliest.t)){
                        earliest=candidate;
                        hit_actor=j;
                    }
                }
                if(hit_actor!=0xffffu){
                    /* Position at the impact feature (face, edge or vertex) and
                     * resolve normal velocity before moving the residual
                     * fraction of this fixed substep. */
                    ball.sphere.shape.center=add(
                        earliest.point,
                        scale(earliest.normal,ball.sphere.shape.radius));
                    const sat_contact3_t impact{earliest.normal,0};
                    resolve(ball,w->actors[hit_actor],impact);
                    const F remaining=(F)((SAT_FX16_ONE-earliest.t)/steps);
                    ball.sphere.shape.center=add(
                        ball.sphere.shape.center,
                        scale(ball.sphere.vel,remaining));
                } else ball.sphere.shape.center=add(ball.sphere.shape.center,d);
            }else ball.sphere.shape.center=add(ball.sphere.shape.center,d);
            for(uint8_t iteration=0;iteration<w->iterations;++iteration){
                bool hit=false;
                for(uint16_t j=0;j<w->count;++j){
                    const sat_physics3_actor_t& box=w->actors[j];
                    if(box.kind==SAT_PHYSICS3_DYNAMIC_SPHERE)continue;
                    if(box.kind==SAT_PHYSICS3_STATIC_MESH ||
                       box.kind==SAT_PHYSICS3_KINEMATIC_MESH){
                        sat_sphere_t reference_sphere=ball.sphere.shape;
                        if(box.kind==SAT_PHYSICS3_KINEMATIC_MESH){
                            if(!fits((int64_t)reference_sphere.center.x-
                                     box.mesh_offset.x) ||
                               !fits((int64_t)reference_sphere.center.y-
                                     box.mesh_offset.y) ||
                               !fits((int64_t)reference_sphere.center.z-
                                     box.mesh_offset.z))
                                return SAT_ERR_INVALID_ARG;
                            reference_sphere.center=sub(
                                reference_sphere.center,box.mesh_offset);
                            if(!q_rotate(q_inverse(box.mesh_orientation),
                                         reference_sphere.center,
                                         reference_sphere.center))
                                return SAT_ERR_INVALID_ARG;
                        }
                        uint16_t count=0;
                        const sat_result_t status=box.mesh_grid
                            ? sat_sphere_mesh_contact_grid(
                                box.mesh_grid,&reference_sphere,w->mesh_contacts,
                                w->mesh_contact_capacity,&count)
                            : sat_sphere_mesh_contact(
                                box.mesh,&reference_sphere,w->mesh_contacts,
                                w->mesh_contact_capacity,&count);
                        if(status!=SAT_OK)return status;
                        for(uint16_t k=0;k<count;++k) {
                            sat_contact3_t c=w->mesh_contacts[k];
                            if(box.kind==SAT_PHYSICS3_KINEMATIC_MESH){
                                V local_point=sub(reference_sphere.center,
                                    scale(c.normal,(F)(ball.sphere.shape.radius-c.depth)));
                                V normal{};
                                if(!q_rotate(box.mesh_orientation,c.normal,normal))
                                    return SAT_ERR_INVALID_ARG;
                                c.normal=normal;
                                const V old_offset=sub(box.target_center,box.frame_motion);
                                V first{},last{};
                                if(!q_rotate(box.mesh_tick_start_orientation,
                                             local_point,first)||
                                   !q_rotate(box.mesh_target_orientation,
                                             local_point,last)||
                                   !add_fits(first,old_offset)||
                                   !add_fits(last,box.target_center))
                                    return SAT_ERR_INVALID_ARG;
                                const V before=add(first,old_offset);
                                const V after=add(last,box.target_center);
                                if(!fits((int64_t)after.x-before.x)||
                                   !fits((int64_t)after.y-before.y)||
                                   !fits((int64_t)after.z-before.z))
                                    return SAT_ERR_INVALID_ARG;
                                const V point_motion=sub(after,before);
                                resolve(ball,box,c,&point_motion);
                            }else resolve(ball,box,c);
                            hit=true;
                        }
                        continue;
                    }
                    sat_contact3_t c{};
                    const int contact=box.kind==SAT_PHYSICS3_STATIC_PLANE
                        ? sat_sphere_plane_contact(&ball.sphere.shape,&box.plane,&c)
                        : sat_sphere_aabb3_contact(&ball.sphere.shape,&box.box,&c);
                    if(!contact)continue;
                    resolve(ball,box,c);hit=true;
                }
                if(!hit)break;
            }
            rotate_sphere(ball,steps);
        }
    }
    return SAT_OK;
}
