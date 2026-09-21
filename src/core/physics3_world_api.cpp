#include "saturn/physics3_world.h"
#include <limits.h>
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
V at(V begin,V end,uint16_t step,uint16_t total){
    return {(F)((int64_t)begin.x+((int64_t)end.x-begin.x)*step/total),
            (F)((int64_t)begin.y+((int64_t)end.y-begin.y)*step/total),
            (F)((int64_t)begin.z+((int64_t)end.z-begin.z)*step/total)};
}
void resolve(sat_physics3_actor_t& ball,const sat_physics3_actor_t& box,
             const sat_contact3_t& c) {
    ball.sphere.shape.center=add(ball.sphere.shape.center,scale(c.normal,c.depth));
    V relative=sub(ball.sphere.vel,box.frame_motion);
    const int64_t vn=dot(relative,c.normal);
    if(vn<0){
        const F rest=mn(ball.material.restitution,box.material.restitution);
        const F impulse=(F)((vn*(SAT_FX16_ONE+(int64_t)rest))>>16);
        relative=sub(relative,scale(c.normal,impulse));
    }
    if(c.normal.y>45875){
        ball.sphere.flags|=SAT_BODY3_GROUNDED;
        const F f=SAT_FX16_ONE-mn(ball.material.friction,box.material.friction);
        relative.x=mul(relative.x,f); relative.z=mul(relative.z,f);
    } else ball.sphere.flags|=SAT_BODY3_HIT_WALL;
    ball.sphere.vel=add(relative,box.frame_motion);
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
    w->count=(uint16_t)(next+1u);*id=next;return SAT_OK;
}
extern "C" sat_result_t sat_physics3_set_kinematic_target(
    sat_physics3_world_t* w,uint16_t id,const V* center){
    if(!live(w,id)||!center||
       w->actors[id].kind!=SAT_PHYSICS3_KINEMATIC_BOX)
        return SAT_ERR_INVALID_ARG;
    w->actors[id].target_center=*center;return SAT_OK;
}
extern "C" sat_result_t sat_physics3_set_velocity(
    sat_physics3_world_t* w,uint16_t id,const V* velocity){
    if(!live(w,id)||!velocity||
       w->actors[id].kind!=SAT_PHYSICS3_DYNAMIC_SPHERE)
        return SAT_ERR_INVALID_ARG;
    w->actors[id].sphere.vel=*velocity;return SAT_OK;
}
extern "C" sat_result_t sat_physics3_get_actor(
    const sat_physics3_world_t* w,uint16_t id,sat_physics3_actor_t* out){
    if(!live(w,id)||!out)return SAT_ERR_INVALID_ARG;
    *out=w->actors[id];return SAT_OK;
}
extern "C" sat_result_t sat_physics3_world_step(sat_physics3_world_t* w){
    if(!valid(w))return SAT_ERR_INVALID_ARG;
    if(!w->count)return SAT_OK;
    int64_t kinematic_speed=0,dynamic_speed=0;
    F smallest_radius=INT32_MAX;
    bool spheres=false;
    for(uint16_t i=0;i<w->count;++i){
        const sat_physics3_actor_t& a=w->actors[i];
        if(a.kind==SAT_PHYSICS3_KINEMATIC_BOX){
            const int64_t dx=(int64_t)a.target_center.x-a.box.center.x;
            const int64_t dy=(int64_t)a.target_center.y-a.box.center.y;
            const int64_t dz=(int64_t)a.target_center.z-a.box.center.z;
            kinematic_speed=mx(kinematic_speed,mx(ab(dx),mx(ab(dy),ab(dz))));
            if(!fits(dx)||!fits(dy)||!fits(dz))return SAT_ERR_INVALID_ARG;
        }else if(a.kind==SAT_PHYSICS3_DYNAMIC_SPHERE){
            if(a.sphere.shape.radius<=0 || !add_fits(a.sphere.vel,w->gravity))
                return SAT_ERR_INVALID_ARG;
            const V v=add(a.sphere.vel,w->gravity);
            if(!add_fits(a.sphere.shape.center,v))return SAT_ERR_INVALID_ARG;
            spheres=true;smallest_radius=mn(smallest_radius,a.sphere.shape.radius);
            dynamic_speed=mx(dynamic_speed,greatest(v));
        }else if(a.kind!=SAT_PHYSICS3_STATIC_BOX)return SAT_ERR_INVALID_ARG;
    }
    uint16_t steps=1;
    if(spheres){
        const int64_t stride=mx(1,smallest_radius/2);
        const int64_t distance=dynamic_speed+kinematic_speed;
        const int64_t required=distance?(distance+stride-1)/stride:1;
        if(required>w->max_substeps)return SAT_ERR_CAPACITY;
        steps=(uint16_t)required;
    }
    for(uint16_t i=0;i<w->count;++i){
        sat_physics3_actor_t& a=w->actors[i];
        if(a.kind==SAT_PHYSICS3_KINEMATIC_BOX){
            a.frame_motion=sub(a.target_center,a.box.center);
        }else if(a.kind==SAT_PHYSICS3_DYNAMIC_SPHERE){
            a.sphere.flags=0;a.sphere.vel=add(a.sphere.vel,w->gravity);
        }
    }
    for(uint16_t step=1;step<=steps;++step){
        for(uint16_t i=0;i<w->count;++i){
            sat_physics3_actor_t& a=w->actors[i];
            if(a.kind!=SAT_PHYSICS3_KINEMATIC_BOX)continue;
            const V start=sub(a.target_center,a.frame_motion);
            a.box.center=at(start,a.target_center,step,steps);
        }
        for(uint16_t i=0;i<w->count;++i){
            sat_physics3_actor_t& ball=w->actors[i];
            if(ball.kind!=SAT_PHYSICS3_DYNAMIC_SPHERE)continue;
            const V d={(F)(ball.sphere.vel.x/steps),
                       (F)(ball.sphere.vel.y/steps),
                       (F)(ball.sphere.vel.z/steps)};
            ball.sphere.shape.center=add(ball.sphere.shape.center,d);
            for(uint8_t iteration=0;iteration<w->iterations;++iteration){
                bool hit=false;
                for(uint16_t j=0;j<w->count;++j){
                    const sat_physics3_actor_t& box=w->actors[j];
                    if(box.kind==SAT_PHYSICS3_DYNAMIC_SPHERE)continue;
                    sat_contact3_t c{};
                    if(!sat_sphere_aabb3_contact(&ball.sphere.shape,&box.box,&c))
                        continue;
                    resolve(ball,box,c);hit=true;
                }
                if(!hit)break;
            }
        }
    }
    return SAT_OK;
}
