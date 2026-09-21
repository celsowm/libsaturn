#include <cstdio>
#include <cstdlib>
#include "saturn/physics3_world.h"

#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);std::exit(1);}}while(0)
#define FX(n) ((sat_fx16_t)((n)*65536))
static const sat_physics3_material_t rough={SAT_FX16_ONE,0};
static const sat_physics3_material_t bouncy={0,SAT_FX16_ONE};
static const sat_vec3_t zero={0,0,0};
static sat_physics3_actor_t read(const sat_physics3_world_t* w,uint16_t id){
    sat_physics3_actor_t out{};
    CHECK(sat_physics3_get_actor(w,id,&out)==SAT_OK);
    return out;
}
static void validation_and_capacity(){
    sat_physics3_actor_t actors[2]{};
    sat_physics3_world_t w{};
    const sat_aabb3_t box={{0,0,0},{FX(2),FX(1),FX(2)}};
    const sat_sphere_t sphere={{0,FX(4),0},FX(1)};
    const sat_physics3_material_t invalid={-1,0};
    uint16_t id=999;
    CHECK(sat_physics3_world_init(&w,actors,2,zero,0,2)==SAT_ERR_INVALID_ARG);
    CHECK(sat_physics3_world_init(&w,actors,2,zero,2,2)==SAT_OK);
    CHECK(sat_physics3_add_box(&w,SAT_PHYSICS3_DYNAMIC_SPHERE,&box,&rough,&id)==SAT_ERR_INVALID_ARG);
    CHECK(sat_physics3_add_box(&w,SAT_PHYSICS3_STATIC_BOX,&box,&invalid,&id)==SAT_ERR_INVALID_ARG);
    CHECK(sat_physics3_add_box(&w,SAT_PHYSICS3_STATIC_BOX,&box,&rough,&id)==SAT_OK && id==0);
    CHECK(sat_physics3_set_kinematic_target(&w,id,&zero)==SAT_ERR_INVALID_ARG);
    CHECK(sat_physics3_add_sphere(&w,&sphere,&zero,&rough,&id)==SAT_OK && id==1);
    CHECK(sat_physics3_add_sphere(&w,&sphere,&zero,&rough,&id)==SAT_ERR_CAPACITY);
    CHECK(sat_physics3_get_actor(&w,2,&actors[0])==SAT_ERR_INVALID_ARG);
    const sat_vec3_t too_fast={FX(3),0,0};
    CHECK(sat_physics3_set_velocity(&w,id,&too_fast)==SAT_OK);
    const sat_physics3_actor_t before=read(&w,id);
    CHECK(sat_physics3_world_step(&w)==SAT_ERR_CAPACITY);
    const sat_physics3_actor_t after=read(&w,id);
    CHECK(after.sphere.shape.center.x==before.sphere.shape.center.x);
    CHECK(after.sphere.vel.x==before.sphere.vel.x);
    CHECK(read(&w,0).box.center.x==0);
    sat_physics3_world_reset(&w);
    CHECK(w.count==0);
    CHECK(sat_physics3_add_sphere(&w,&sphere,&zero,&rough,&id)==SAT_OK && id==0);
}
static void floor_contact_and_bounce(){
    sat_physics3_actor_t actors[2]{};
    sat_physics3_world_t w{};
    const sat_vec3_t gravity={0,-FX(1)/4,0};
    CHECK(sat_physics3_world_init(&w,actors,2,gravity,16,3)==SAT_OK);
    const sat_aabb3_t floor={{0,0,0},{FX(8),FX(1)/2,FX(8)}};
    const sat_sphere_t sphere={{0,FX(3)/2,0},FX(1)};
    const sat_vec3_t velocity={FX(1)/4,0,0};
    uint16_t floor_id,ball_id;
    CHECK(sat_physics3_add_box(&w,SAT_PHYSICS3_STATIC_BOX,&floor,&rough,&floor_id)==SAT_OK);
    CHECK(sat_physics3_add_sphere(&w,&sphere,&velocity,&rough,&ball_id)==SAT_OK);
    CHECK(sat_physics3_world_step(&w)==SAT_OK);
    sat_physics3_actor_t ball=read(&w,ball_id);
    CHECK(ball.sphere.flags & SAT_BODY3_GROUNDED);
    CHECK(ball.sphere.shape.center.y==FX(3)/2);
    CHECK(ball.sphere.vel.y==0);
    CHECK(ball.sphere.vel.x==0);
    CHECK(floor_id==0 && ball_id==1);

    sat_physics3_world_reset(&w);
    CHECK(sat_physics3_add_box(&w,SAT_PHYSICS3_STATIC_BOX,&floor,&bouncy,&floor_id)==SAT_OK);
    const sat_vec3_t fall={0,-FX(1)/2,0};
    CHECK(sat_physics3_add_sphere(&w,&sphere,&fall,&bouncy,&ball_id)==SAT_OK);
    w.gravity=zero;
    CHECK(sat_physics3_world_step(&w)==SAT_OK);
    ball=read(&w,ball_id);
    CHECK(ball.sphere.flags & SAT_BODY3_GROUNDED);
    CHECK(ball.sphere.vel.y==FX(1)/2);
}
static void moving_platform_and_multiple_balls(){
    sat_physics3_actor_t actors[3]{};
    sat_physics3_world_t w{};
    const sat_vec3_t gravity={0,-FX(1)/4,0};
    CHECK(sat_physics3_world_init(&w,actors,3,gravity,16,3)==SAT_OK);
    const sat_aabb3_t platform={{0,0,0},{FX(4),FX(1)/2,FX(4)}};
    const sat_sphere_t sphere={{0,FX(3)/2,0},FX(1)};
    const sat_sphere_t other={{FX(2),FX(3)/2,0},FX(1)};
    uint16_t platform_id,ball_id,other_id;
    CHECK(sat_physics3_add_box(&w,SAT_PHYSICS3_KINEMATIC_BOX,&platform,&rough,&platform_id)==SAT_OK);
    CHECK(sat_physics3_add_sphere(&w,&sphere,&zero,&rough,&ball_id)==SAT_OK);
    CHECK(sat_physics3_add_sphere(&w,&other,&zero,&rough,&other_id)==SAT_OK);
    const sat_vec3_t target={FX(1),0,0};
    CHECK(sat_physics3_set_kinematic_target(&w,platform_id,&target)==SAT_OK);
    CHECK(sat_physics3_world_step(&w)==SAT_OK);
    CHECK(read(&w,platform_id).box.center.x==FX(1));
    CHECK(read(&w,ball_id).sphere.shape.center.x>0);
    CHECK(read(&w,ball_id).sphere.flags & SAT_BODY3_GROUNDED);
    CHECK(read(&w,other_id).sphere.flags & SAT_BODY3_GROUNDED);
    CHECK(sat_physics3_set_velocity(&w,platform_id,&zero)==SAT_ERR_INVALID_ARG);
    const sat_vec3_t next={FX(2),0,0};
    CHECK(sat_physics3_set_kinematic_target(&w,platform_id,&next)==SAT_OK);
    CHECK(sat_physics3_world_step(&w)==SAT_OK);
    CHECK(read(&w,platform_id).box.center.x==FX(2));
}
static void deterministic_replay(){
    sat_physics3_actor_t a[2]{},b[2]{};
    sat_physics3_world_t wa{},wb{};
    const sat_vec3_t g={0,-FX(1)/8,0};
    const sat_aabb3_t floor={{0,0,0},{FX(3),FX(1)/2,FX(3)}};
    const sat_sphere_t s={{0,FX(4),0},FX(1)};
    const sat_vec3_t v={FX(1)/8,0,0};
    CHECK(sat_physics3_world_init(&wa,a,2,g,16,3)==SAT_OK);
    CHECK(sat_physics3_world_init(&wb,b,2,g,16,3)==SAT_OK);
    uint16_t id;
    CHECK(sat_physics3_add_box(&wa,SAT_PHYSICS3_STATIC_BOX,&floor,&rough,&id)==SAT_OK);
    CHECK(sat_physics3_add_sphere(&wa,&s,&v,&rough,&id)==SAT_OK);
    CHECK(sat_physics3_add_box(&wb,SAT_PHYSICS3_STATIC_BOX,&floor,&rough,&id)==SAT_OK);
    CHECK(sat_physics3_add_sphere(&wb,&s,&v,&rough,&id)==SAT_OK);
    for(int i=0;i<8;++i){
        CHECK(sat_physics3_world_step(&wa)==SAT_OK);
        CHECK(sat_physics3_world_step(&wb)==SAT_OK);
        const sat_physics3_actor_t x=read(&wa,1),y=read(&wb,1);
        CHECK(x.sphere.shape.center.x==y.sphere.shape.center.x);
        CHECK(x.sphere.shape.center.y==y.sphere.shape.center.y);
        CHECK(x.sphere.vel.x==y.sphere.vel.x);
        CHECK(x.sphere.vel.y==y.sphere.vel.y);
        CHECK(x.sphere.flags==y.sphere.flags);
    }
}
static void inclined_plane_and_tangent_friction() {
    sat_physics3_actor_t actors[2]{};
    sat_physics3_world_t w{};
    const sat_vec3_t gravity={0,-FX(1)/8,0};
    CHECK(sat_physics3_world_init(&w,actors,2,gravity,16,3)==SAT_OK);
    const sat_plane3_t invalid={{0,0,0},{0,0,0}};
    uint16_t plane_id=555,ball_id=555;
    CHECK(sat_physics3_add_plane(&w,&invalid,&rough,&plane_id)==SAT_ERR_INVALID_ARG);
    CHECK(plane_id==555);
    const sat_plane3_t slope={{0,0,0},{0,SAT_FX16_ONE,SAT_FX16_ONE}};
    CHECK(sat_physics3_add_plane(&w,&slope,&rough,&plane_id)==SAT_OK);
    const sat_sphere_t sphere={{0,FX(1),0},FX(1)};
    CHECK(sat_physics3_add_sphere(&w,&sphere,&zero,&rough,&ball_id)==SAT_OK);
    CHECK(sat_physics3_world_step(&w)==SAT_OK);
    const sat_physics3_actor_t b=read(&w,ball_id);
    CHECK(b.sphere.flags & SAT_BODY3_GROUNDED);
    CHECK(b.sphere.vel.x==0);
    CHECK(b.sphere.vel.y>=-2 && b.sphere.vel.y<=2);
    CHECK(b.sphere.vel.z>=-2 && b.sphere.vel.z<=2);
    CHECK(sat_physics3_set_kinematic_target(&w,plane_id,&zero)==SAT_ERR_INVALID_ARG);
    CHECK(sat_physics3_add_plane(&w,&slope,&rough,&plane_id)==SAT_ERR_CAPACITY);
}
int main(){
    validation_and_capacity();
    floor_contact_and_bounce();
    moving_platform_and_multiple_balls();
    deterministic_replay();
    inclined_plane_and_tangent_friction();
    std::puts("test_physics3_world: 5 tests passed");
    return 0;
}
