#include <cstdio>
#include <cstdint>
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
static void optional_collider_index_matches_full_scan(){
    sat_physics3_actor_t slow_actors[5]{}, fast_actors[5]{};
    sat_physics3_world_t slow{},fast{};
    uint16_t scratch[5]={};
    CHECK(sat_physics3_world_init(&slow,slow_actors,5,zero,16,3)==SAT_OK);
    CHECK(sat_physics3_world_init(&fast,fast_actors,5,zero,16,3)==SAT_OK);
    CHECK(sat_physics3_set_collider_index_scratch(&fast,scratch,4)==SAT_ERR_CAPACITY);
    CHECK(sat_physics3_set_collider_index_scratch(&fast,nullptr,1)==SAT_ERR_INVALID_ARG);
    CHECK(sat_physics3_set_collider_index_scratch(&fast,scratch,5)==SAT_OK);
    const sat_sphere_t a{{0,FX(3)/2,0},FX(1)};
    const sat_sphere_t b{{FX(2),FX(3)/2,0},FX(1)};
    const sat_aabb3_t floor{{0,0,0},{FX(5),FX(1)/2,FX(5)}};
    const sat_vec3_t gravity{0,-FX(1)/8,0};
    slow.gravity=fast.gravity=gravity;
    uint16_t id=0u;
    /* A dynamic sphere comes first: type filtering must preserve the
     * original collider IDs and tie order, not compact in kind order. */
    sat_physics3_world_t* worlds[2]={&slow,&fast};
    for(sat_physics3_world_t* w:worlds) {
        CHECK(sat_physics3_add_sphere(w,&a,&zero,&rough,&id)==SAT_OK && id==0u);
        CHECK(sat_physics3_add_box(
            w,SAT_PHYSICS3_STATIC_BOX,&floor,&rough,&id)==SAT_OK && id==1u);
        CHECK(sat_physics3_add_sphere(w,&b,&zero,&rough,&id)==SAT_OK && id==2u);
    }
    for(uint16_t frame=0u;frame<6u;++frame) {
        CHECK(sat_physics3_world_step(&slow)==SAT_OK);
        CHECK(sat_physics3_world_step(&fast)==SAT_OK);
        for(uint16_t i=0u;i<3u;++i) {
            const sat_physics3_actor_t x=read(&slow,i),y=read(&fast,i);
            CHECK(x.kind==y.kind);
            CHECK(x.sphere.shape.center.x==y.sphere.shape.center.x);
            CHECK(x.sphere.shape.center.y==y.sphere.shape.center.y);
            CHECK(x.sphere.vel.x==y.sphere.vel.x);
            CHECK(x.sphere.vel.y==y.sphere.vel.y);
            CHECK(x.sphere.flags==y.sphere.flags);
        }
    }
    CHECK(sat_physics3_set_collider_index_scratch(&fast,nullptr,0)==SAT_OK);
    CHECK(fast.collider_indices==nullptr && fast.collider_index_capacity==0u);
}
static void mesh_aabb_caches_reference_bounds(){
    sat_physics3_actor_t actors[3]{};
    sat_physics3_world_t w{};
    sat_contact3_t contacts[1]{};
    CHECK(sat_physics3_world_init(&w,actors,3,zero,16,3)==SAT_OK);
    CHECK(sat_physics3_set_mesh_contacts(&w,contacts,1)==SAT_OK);
    sat_vec3_t vertices[4]={
        {-FX(3),FX(1),-FX(2)},{FX(5),FX(1),-FX(2)},
        {FX(5),FX(1),FX(7)},{-FX(3),FX(1),FX(7)}
    };
    uint16_t indices[4]={0,1,2,3};
    sat_mesh_t mesh{vertices,indices,4,4,1,1};
    uint16_t id=0u;
    CHECK(sat_physics3_add_mesh(&w,&mesh,&rough,&id)==SAT_OK);
    const sat_physics3_actor_t cached=read(&w,id);
    CHECK(cached.mesh_bounds_min.x==-FX(3));
    CHECK(cached.mesh_bounds_min.y==FX(1));
    CHECK(cached.mesh_bounds_min.z==-FX(2));
    CHECK(cached.mesh_bounds_max.x==FX(5));
    CHECK(cached.mesh_bounds_max.y==FX(1));
    CHECK(cached.mesh_bounds_max.z==FX(7));
}
static void structural_spatial_bvh_matches_linear_world() {
    sat_physics3_actor_t a[5]{},b[5]{};
    sat_physics3_world_t linear{},indexed{};
    const sat_vec3_t gravity={0,-FX(1)/8,0};
    CHECK(sat_physics3_world_init(&linear,a,5,gravity,32,3)==SAT_OK);
    CHECK(sat_physics3_world_init(&indexed,b,5,gravity,32,3)==SAT_OK);
    sat_physics3_spatial_node_t nodes[9]{};
    uint16_t fallback[5]{},candidates[5]{};
    CHECK(sat_physics3_set_spatial_broadphase(
        &indexed,nodes,9,candidates,5)==SAT_ERR_INVALID_ARG);
    CHECK(sat_physics3_set_collider_index_scratch(
        &indexed,fallback,4)==SAT_ERR_CAPACITY);
    CHECK(sat_physics3_set_collider_index_scratch(
        &indexed,fallback,5)==SAT_OK);
    CHECK(sat_physics3_set_spatial_broadphase(
        &indexed,nodes,8,candidates,5)==SAT_ERR_CAPACITY);
    CHECK(sat_physics3_set_spatial_broadphase(
        &indexed,nodes,9,candidates,4)==SAT_ERR_CAPACITY);
    CHECK(indexed.spatial_nodes==nullptr);
    CHECK(sat_physics3_set_spatial_broadphase(
        &indexed,nodes,9,candidates,5)==SAT_OK);
    CHECK(sat_physics3_set_collider_index_scratch(
        &indexed,nullptr,0)==SAT_ERR_INVALID_ARG);

    sat_physics3_world_t* worlds[2]={&linear,&indexed};
    for(sat_physics3_world_t* w:worlds) {
        uint16_t id=999u;
        const sat_sphere_t sphere{{0,FX(3)/2,0},FX(1)};
        CHECK(sat_physics3_add_sphere(w,&sphere,&zero,&rough,&id)==SAT_OK &&
              id==0u);
        const sat_aabb3_t distant={{FX(100),0,0},{FX(1),FX(1),FX(1)}};
        CHECK(sat_physics3_add_box(
            w,SAT_PHYSICS3_STATIC_BOX,&distant,&rough,&id)==SAT_OK && id==1u);
        const sat_aabb3_t floor={{0,0,0},{FX(5),FX(1)/2,FX(5)}};
        CHECK(sat_physics3_add_box(
            w,SAT_PHYSICS3_STATIC_BOX,&floor,&rough,&id)==SAT_OK && id==2u);
        const sat_aabb3_t moving={{FX(3),0,0},{FX(1)/2,FX(1)/2,FX(1)/2}};
        CHECK(sat_physics3_add_box(
            w,SAT_PHYSICS3_KINEMATIC_BOX,&moving,&rough,&id)==SAT_OK && id==3u);
        const sat_plane3_t plane={{0,-FX(20),0},{0,SAT_FX16_ONE,0}};
        CHECK(sat_physics3_add_plane(
            w,&plane,&rough,&id)==SAT_OK && id==4u);
        const sat_vec3_t target={FX(2),0,0};
        CHECK(sat_physics3_set_kinematic_target(w,3u,&target)==SAT_OK);
        CHECK(sat_physics3_set_kinematic_box_ccd(w,1)==SAT_OK);
    }
    for(int frame=0;frame<5;++frame) {
        CHECK(sat_physics3_world_step(&linear)==SAT_OK);
        CHECK(sat_physics3_world_step(&indexed)==SAT_OK);
        const sat_physics3_actor_t x=read(&linear,0u),y=read(&indexed,0u);
        CHECK(x.sphere.shape.center.x==y.sphere.shape.center.x);
        CHECK(x.sphere.shape.center.y==y.sphere.shape.center.y);
        CHECK(x.sphere.vel.x==y.sphere.vel.x);
        CHECK(x.sphere.vel.y==y.sphere.vel.y);
        CHECK(x.sphere.flags==y.sphere.flags);
        CHECK(read(&linear,3u).box.center.x==read(&indexed,3u).box.center.x);
        CHECK(indexed.spatial_leaf_count==3u &&
              indexed.spatial_fallback_count==1u);
        CHECK(indexed.spatial_queries>0u);
        /* The far static box is excluded from per-ball narrowphase. */
        CHECK(indexed.spatial_candidates_checked<
              indexed.spatial_queries*4u);
    }
    sat_physics3_world_reset(&indexed);
    CHECK(indexed.spatial_leaf_count==0u && indexed.spatial_queries==0u);
    CHECK(sat_physics3_set_spatial_broadphase(
        &indexed,nullptr,0u,nullptr,0u)==SAT_OK);
    CHECK(sat_physics3_set_collider_index_scratch(
        &indexed,nullptr,0u)==SAT_OK);
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

static void finite_mesh_ramp_edge_and_gap() {
    sat_physics3_actor_t actors[3]{};
    sat_physics3_world_t w{};
    sat_contact3_t contacts[2]{};
    const sat_vec3_t gravity={0,-FX(1)/8,0};
    CHECK(sat_physics3_world_init(&w,actors,3,gravity,16,4)==SAT_OK);
    /* Upward-facing quad; its far edge is higher along positive Z. */
    sat_vec3_t ramp_vertices[4]={
        {-FX(4),-FX(1),-FX(4)},{FX(4),-FX(1),-FX(4)},
        {FX(4),FX(1),FX(4)},{-FX(4),FX(1),FX(4)}
    };
    uint16_t ramp_indices[4]={0,1,2,3};
    sat_mesh_t ramp{ramp_vertices,ramp_indices,4,4,1,1};
    uint16_t mesh_id=777,ball_id=777,outside_id=777;
    CHECK(sat_physics3_add_mesh(&w,&ramp,&rough,&mesh_id)==SAT_ERR_CAPACITY);
    CHECK(mesh_id==777 && w.count==0);
    CHECK(sat_physics3_set_mesh_contacts(&w,contacts,2)==SAT_OK);
    CHECK(sat_physics3_add_mesh(&w,&ramp,&rough,&mesh_id)==SAT_OK);
    CHECK(mesh_id==0);
    const sat_sphere_t on_ramp={{0,FX(1),0},FX(1)};
    const sat_sphere_t outside={{FX(9),FX(1),0},FX(1)};
    CHECK(sat_physics3_add_sphere(&w,&on_ramp,&zero,&rough,&ball_id)==SAT_OK);
    CHECK(sat_physics3_add_sphere(&w,&outside,&zero,&rough,&outside_id)==SAT_OK);
    CHECK(sat_physics3_world_step(&w)==SAT_OK);
    CHECK(read(&w,ball_id).sphere.flags & SAT_BODY3_GROUNDED);
    CHECK(read(&w,outside_id).sphere.flags==0);
    CHECK(read(&w,outside_id).sphere.shape.center.y==FX(1)-FX(1)/8);
    const sat_physics3_actor_t before=read(&w,ball_id);
    /* Caller cannot replace scratch with less than the registered face count. */
    CHECK(sat_physics3_set_mesh_contacts(&w,contacts,0)==SAT_ERR_INVALID_ARG);
    CHECK(read(&w,ball_id).sphere.shape.center.y==before.sphere.shape.center.y);

    sat_physics3_world_reset(&w);
    sat_vec3_t platform_vertices[8]={
        {-FX(6),0,-FX(2)},{-FX(2),0,-FX(2)},
        {-FX(2),0,FX(2)},{-FX(6),0,FX(2)},
        {FX(2),0,-FX(2)},{FX(6),0,-FX(2)},
        {FX(6),0,FX(2)},{FX(2),0,FX(2)}
    };
    uint16_t platform_indices[8]={0,1,2,3,4,5,6,7};
    sat_mesh_t platforms{platform_vertices,platform_indices,8,8,2,2};
    CHECK(sat_physics3_add_mesh(&w,&platforms,&rough,&mesh_id)==SAT_OK);
    /* Unsupported: the centre of the gap is more than one radius from both edges. */
    const sat_sphere_t gap={{0,FX(1),0},FX(1)};
    CHECK(sat_physics3_add_sphere(&w,&gap,&zero,&rough,&ball_id)==SAT_OK);
    CHECK(sat_physics3_world_step(&w)==SAT_OK);
    CHECK(read(&w,ball_id).sphere.flags==0);
    CHECK(read(&w,ball_id).sphere.shape.center.y==FX(1)-FX(1)/8);
    /* A centre near the outer lip still collides with the finite face edge. */
    const sat_sphere_t edge={{FX(6)+FX(1)/2,FX(3)/4,0},FX(1)};
    CHECK(sat_physics3_add_sphere(&w,&edge,&zero,&rough,&outside_id)==SAT_OK);
    CHECK(sat_physics3_world_step(&w)==SAT_OK);
    CHECK(read(&w,outside_id).sphere.flags & SAT_BODY3_GROUNDED);
}
static void mesh_registration_validates_indices_and_contact_capacity() {
    sat_physics3_actor_t actors[2]{};
    sat_physics3_world_t w{};
    sat_contact3_t contacts[2]{};
    CHECK(sat_physics3_world_init(&w,actors,2,zero,8,2)==SAT_OK);
    CHECK(sat_physics3_set_mesh_contacts(&w,contacts,1)==SAT_OK);
    sat_vec3_t vertices[8]={
        {-FX(3),0,-FX(2)},{-FX(1),0,-FX(2)},
        {-FX(1),0,FX(2)},{-FX(3),0,FX(2)},
        {FX(1),0,-FX(2)},{FX(3),0,-FX(2)},
        {FX(3),0,FX(2)},{FX(1),0,FX(2)}
    };
    uint16_t indices[8]={0,1,2,3,4,5,6,7};
    sat_mesh_t mesh{vertices,indices,8,8,2,2};
    uint16_t id=400;
    CHECK(sat_physics3_add_mesh(&w,&mesh,&rough,&id)==SAT_ERR_CAPACITY);
    CHECK(id==400 && w.count==0);
    CHECK(sat_physics3_set_mesh_contacts(&w,contacts,2)==SAT_OK);
    indices[7]=9;
    CHECK(sat_physics3_add_mesh(&w,&mesh,&rough,&id)==SAT_ERR_INVALID_ARG);
    CHECK(id==400 && w.count==0);
    indices[7]=7;
    CHECK(sat_physics3_add_mesh(&w,&mesh,&rough,&id)==SAT_OK);
    CHECK(id==0);
    CHECK(sat_physics3_set_mesh_contacts(&w,contacts,1)==SAT_ERR_CAPACITY);
    const sat_sphere_t ball={{0,FX(4),0},FX(1)};
    CHECK(sat_physics3_add_sphere(&w,&ball,&zero,&rough,&id)==SAT_OK);
    const sat_physics3_actor_t before=read(&w,id);
    w.mesh_contact_capacity=1; /* Demonstrate preflight even if caller tampers. */
    CHECK(sat_physics3_world_step(&w)==SAT_ERR_INVALID_ARG);
    CHECK(read(&w,id).sphere.shape.center.y==before.sphere.shape.center.y);
}

static void accelerated_mesh_matches_linear_contacts_and_gaps() {
    sat_physics3_actor_t linear_actors[4]{}, grid_actors[4]{};
    sat_physics3_world_t linear{}, accelerated{};
    sat_contact3_t linear_contacts[2]{}, accelerated_contacts[2]{};
    const sat_vec3_t gravity={0,-FX(1)/8,0};
    CHECK(sat_physics3_world_init(
        &linear,linear_actors,4,gravity,16,4)==SAT_OK);
    CHECK(sat_physics3_world_init(
        &accelerated,grid_actors,4,gravity,16,4)==SAT_OK);

    sat_vec3_t vertices[8]={
        {-FX(2),0,-FX(2)},{FX(2),0,-FX(2)},
        {FX(2),0,FX(2)},{-FX(2),0,FX(2)},
        {FX(30),0,-FX(2)},{FX(34),0,-FX(2)},
        {FX(34),0,FX(2)},{FX(30),0,FX(2)}
    };
    uint16_t indices[8]={0,1,2,3,4,5,6,7};
    sat_mesh_t mesh{vertices,indices,8,8,2,2};
    uint16_t heads[16]{};
    sat_mesh3_grid_entry_t entries[64]{};
    uint16_t stamps[2]{};
    sat_mesh3_grid_t grid{};
    uint16_t linear_id=777,grid_id=777;
    CHECK(sat_physics3_add_mesh_grid(
        &accelerated,&grid,&rough,&grid_id)==SAT_ERR_INVALID_ARG);
    CHECK(grid_id==777 && accelerated.count==0);
    CHECK(sat_mesh3_grid_init(
        &grid,&mesh,3,heads,16,entries,64,stamps,2)==SAT_OK);
    CHECK(sat_physics3_add_mesh_grid(
        &accelerated,&grid,&rough,&grid_id)==SAT_ERR_CAPACITY);
    CHECK(grid_id==777 && accelerated.count==0);
    CHECK(sat_physics3_set_mesh_contacts(
        &linear,linear_contacts,2)==SAT_OK);
    CHECK(sat_physics3_set_mesh_contacts(
        &accelerated,accelerated_contacts,2)==SAT_OK);
    CHECK(sat_physics3_add_mesh(
        &linear,&mesh,&rough,&linear_id)==SAT_OK);
    CHECK(sat_physics3_add_mesh_grid(
        &accelerated,&grid,&rough,&grid_id)==SAT_OK);
    CHECK(linear_id==0 && grid_id==0);
    CHECK(accelerated.actors[grid_id].mesh_grid==&grid);
    CHECK(accelerated.actors[grid_id].mesh==&mesh);
    /* The far-away sphere is over the true gap, not a bounding-box floor. */
    const sat_sphere_t spheres[3]={
        {{0,FX(1),0},FX(1)},
        {{FX(16),FX(1),0},FX(1)},
        {{FX(32),FX(1),0},FX(1)}
    };
    for(uint16_t i=0;i<3;++i){
        uint16_t id_a=777,id_b=777;
        CHECK(sat_physics3_add_sphere(
            &linear,&spheres[i],&zero,&rough,&id_a)==SAT_OK);
        CHECK(sat_physics3_add_sphere(
            &accelerated,&spheres[i],&zero,&rough,&id_b)==SAT_OK);
        CHECK(id_a==i+1u && id_b==i+1u);
    }
    for(int step=0;step<3;++step){
        CHECK(sat_physics3_world_step(&linear)==SAT_OK);
        CHECK(sat_physics3_world_step(&accelerated)==SAT_OK);
        for(uint16_t id=1;id<=3;++id){
            const sat_physics3_actor_t a=read(&linear,id);
            const sat_physics3_actor_t b=read(&accelerated,id);
            CHECK(a.sphere.shape.center.x==b.sphere.shape.center.x);
            CHECK(a.sphere.shape.center.y==b.sphere.shape.center.y);
            CHECK(a.sphere.shape.center.z==b.sphere.shape.center.z);
            CHECK(a.sphere.vel.x==b.sphere.vel.x);
            CHECK(a.sphere.vel.y==b.sphere.vel.y);
            CHECK(a.sphere.flags==b.sphere.flags);
        }
        CHECK(read(&accelerated,1).sphere.flags & SAT_BODY3_GROUNDED);
        CHECK(!(read(&accelerated,2).sphere.flags & SAT_BODY3_GROUNDED));
        CHECK(read(&accelerated,3).sphere.flags & SAT_BODY3_GROUNDED);
    }

    /* Replacing a registered grid's mesh must fail before any actor moves. */
    sat_mesh_t wrong=mesh;
    const sat_physics3_actor_t before=read(&accelerated,2);
    grid.mesh=&wrong;
    CHECK(sat_physics3_world_step(&accelerated)==SAT_ERR_INVALID_ARG);
    CHECK(read(&accelerated,2).sphere.shape.center.y==
          before.sphere.shape.center.y);
    grid.mesh=&mesh;
    CHECK(sat_physics3_world_step(&accelerated)==SAT_OK);
}

static void face_interior_ccd_prevents_through_floor() {
    sat_physics3_actor_t actors[2]{};
    sat_physics3_world_t w{};
    sat_contact3_t contacts[1]{};
    const sat_vec3_t gravity{};
    CHECK(sat_physics3_world_init(&w,actors,2,gravity,2,3)==SAT_OK);
    CHECK(sat_physics3_set_mesh_contacts(&w,contacts,1)==SAT_OK);
    sat_vec3_t vertices[4]={
        {-FX(4),0,-FX(4)},{FX(4),0,-FX(4)},
        {FX(4),0,FX(4)},{-FX(4),0,FX(4)}
    };
    uint16_t indices[4]={0,1,2,3};
    sat_mesh_t mesh{vertices,indices,4,4,1,1};
    uint16_t mesh_id=555,ball_id=555;
    CHECK(sat_physics3_add_mesh(&w,&mesh,&rough,&mesh_id)==SAT_OK);
    const sat_sphere_t sphere{{0,FX(3),0},FX(1)/2};
    const sat_vec3_t down{0,-FX(6),0};
    CHECK(sat_physics3_add_sphere(&w,&sphere,&down,&rough,&ball_id)==SAT_OK);
    const sat_physics3_actor_t before=read(&w,ball_id);
    CHECK(sat_physics3_world_step(&w)==SAT_ERR_CAPACITY);
    CHECK(read(&w,ball_id).sphere.shape.center.y==
          before.sphere.shape.center.y);
    CHECK(sat_physics3_set_mesh_face_ccd(&w,1)==SAT_OK);
    CHECK(sat_physics3_world_step(&w)==SAT_OK);
    const sat_physics3_actor_t after=read(&w,ball_id);
    CHECK(after.sphere.shape.center.y>=FX(1)/2-8);
    CHECK(after.sphere.shape.center.y<=FX(1)/2+8);
    CHECK(after.sphere.flags & SAT_BODY3_GROUNDED);
    CHECK(after.sphere.vel.y==0);
    sat_physics3_world_reset(&w);
    CHECK(sat_physics3_add_mesh(&w,&mesh,&rough,&mesh_id)==SAT_OK);
    const sat_sphere_t missed{{FX(9),FX(3),0},FX(1)/2};
    CHECK(sat_physics3_add_sphere(&w,&missed,&down,&rough,&ball_id)==SAT_OK);
    CHECK(sat_physics3_world_step(&w)==SAT_OK);
    CHECK(read(&w,ball_id).sphere.shape.center.y==-FX(3));
    CHECK(read(&w,ball_id).sphere.flags==0);
}
static void fast_mesh_lip_and_corner_are_not_tunneled() {
    /* A small fixed substep budget cannot catch these outside the quad
     * with a plane-only cast: the sphere approaches the lip/corner from
     * above and should hit the finite boundary before it falls through. */
    for(uint8_t feature=0;feature<2;++feature) {
        sat_physics3_actor_t actors[2]{};
        sat_physics3_world_t world{};
        sat_contact3_t contact[1]{};
        CHECK(sat_physics3_world_init(&world,actors,2,zero,2,3)==SAT_OK);
        CHECK(sat_physics3_set_mesh_contacts(&world,contact,1)==SAT_OK);
        sat_vec3_t vertices[4]={
            {-FX(4),0,-FX(4)},{FX(4),0,-FX(4)},
            {FX(4),0,FX(4)},{-FX(4),0,FX(4)}
        };
        uint16_t indices[4]={0,1,2,3};
        sat_mesh_t mesh{vertices,indices,4,4,1,1};
        uint16_t mesh_id=777,ball_id=777;
        CHECK(sat_physics3_add_mesh(&world,&mesh,&rough,&mesh_id)==SAT_OK);
        const sat_sphere_t sphere{{
            FX(4)+FX(1)/4,FX(3),feature?FX(4)+FX(1)/4:0},FX(1)/2};
        const sat_vec3_t velocity{0,-FX(6),0};
        CHECK(sat_physics3_add_sphere(
            &world,&sphere,&velocity,&rough,&ball_id)==SAT_OK);
        CHECK(sat_physics3_set_mesh_face_ccd(&world,1)==SAT_OK);
        CHECK(sat_physics3_world_step(&world)==SAT_OK);
        const sat_physics3_actor_t ball=read(&world,ball_id);
        CHECK(ball.sphere.shape.center.y>0);
        CHECK(ball.sphere.flags & SAT_BODY3_GROUNDED);
        /* Normalization and 16.16 projection can leave a few raw units
         * of residual velocity at the rounded edge/corner impact. */
        CHECK(ball.sphere.vel.x>=-16 && ball.sphere.vel.x<=16);
        CHECK(ball.sphere.vel.y>=-16 && ball.sphere.vel.y<=16);
        CHECK(ball.sphere.vel.z>=-16 && ball.sphere.vel.z<=16);
        CHECK(ball.sphere.shape.center.x>FX(4));
    }
}

static void opt_in_solid_sphere_rolling_and_orientation() {
    sat_physics3_actor_t storage[2]{};
    sat_physics3_world_t world{};
    const sat_vec3_t gravity{0,-FX(1)/8,0};
    CHECK(sat_physics3_world_init(&world,storage,2,gravity,16,3)==SAT_OK);
    const sat_aabb3_t floor{{0,0,0},{FX(8),FX(1)/2,FX(8)}};
    const sat_sphere_t sphere{{0,FX(3)/2,0},FX(1)};
    const sat_vec3_t linear{FX(1)/2,0,0};
    uint16_t floor_id,ball_id;
    CHECK(sat_physics3_add_box(
        &world,SAT_PHYSICS3_STATIC_BOX,&floor,&rough,&floor_id)==SAT_OK);
    CHECK(sat_physics3_add_sphere(
        &world,&sphere,&linear,&rough,&ball_id)==SAT_OK);
    CHECK(sat_physics3_set_rolling(&world,floor_id,1)==SAT_ERR_INVALID_ARG);
    const sat_physics3_actor_t initial=read(&world,ball_id);
    CHECK(initial.rolling_enabled==0);
    CHECK(initial.orientation.w==SAT_FX16_ONE);
    CHECK(initial.orientation.x==0 && initial.orientation.y==0 &&
          initial.orientation.z==0);
    CHECK(sat_physics3_set_rolling(&world,ball_id,1)==SAT_OK);
    CHECK(sat_physics3_world_step(&world)==SAT_OK);
    const sat_physics3_actor_t ball=read(&world,ball_id);
    CHECK(ball.sphere.flags & SAT_BODY3_GROUNDED);
    /* Homogeneous solid sphere: 2/7 of the initial contact slip
     * becomes angular momentum instead of disappearing as legacy drag. */
    CHECK(ball.sphere.vel.x>FX(1)/3);
    CHECK(ball.sphere.vel.x<FX(3)/8);
    CHECK(ball.angular_velocity.z<-FX(1)/3);
    CHECK(ball.angular_velocity.z>-FX(3)/8);
    CHECK(ball.angular_velocity.x==0 && ball.angular_velocity.y==0);
    CHECK(ball.orientation.z<0);
    sat_mat4_t model{};
    CHECK(sat_physics3_sphere_model_matrix(
        &world,ball_id,&model)==SAT_OK);
    CHECK(model.m[3]==ball.sphere.shape.center.x);
    CHECK(model.m[7]==ball.sphere.shape.center.y);
    CHECK(model.m[11]==ball.sphere.shape.center.z);
    CHECK(model.m[4]<0); /* Spinning along -Z rotates local +X toward -Y. */
    CHECK(model.m[15]==SAT_FX16_ONE);
    CHECK(model.m[12]==0 && model.m[13]==0 && model.m[14]==0);
    CHECK(sat_physics3_sphere_model_matrix(
        &world,floor_id,&model)==SAT_ERR_INVALID_ARG);
    CHECK(ball.orientation.w<SAT_FX16_ONE && ball.orientation.w>0);
    CHECK(ball.orientation.x==0 && ball.orientation.y==0);

    const sat_vec3_t forbidden_spin{FX(9),0,0};
    CHECK(sat_physics3_set_angular_velocity(
        &world,ball_id,&forbidden_spin)==SAT_ERR_INVALID_ARG);
    CHECK(sat_physics3_set_angular_velocity(
        &world,floor_id,&linear)==SAT_ERR_INVALID_ARG);
    CHECK(sat_physics3_set_rolling(&world,ball_id,0)==SAT_OK);
    const sat_physics3_quat_t previous=read(&world,ball_id).orientation;
    world.gravity=zero;
    CHECK(sat_physics3_world_step(&world)==SAT_OK);
    CHECK(read(&world,ball_id).orientation.z==previous.z);
    CHECK(read(&world,ball_id).orientation.w==previous.w);
}
static void free_flight_spin_and_kinematic_relative_rolling() {
    sat_physics3_actor_t actors[2]{};
    sat_physics3_world_t world{};
    CHECK(sat_physics3_world_init(&world,actors,2,zero,16,3)==SAT_OK);
    const sat_sphere_t sphere{{0,FX(4),0},FX(1)};
    const sat_vec3_t spin{0,0,-FX(1)/2};
    uint16_t ball_id;
    CHECK(sat_physics3_add_sphere(
        &world,&sphere,&zero,&rough,&ball_id)==SAT_OK);
    CHECK(sat_physics3_set_angular_velocity(&world,ball_id,&spin)==SAT_OK);
    CHECK(sat_physics3_set_rolling(&world,ball_id,1)==SAT_OK);
    CHECK(sat_physics3_world_step(&world)==SAT_OK);
    const sat_physics3_actor_t flying=read(&world,ball_id);
    CHECK(flying.sphere.flags==0);
    CHECK(flying.angular_velocity.z==spin.z);
    CHECK(flying.orientation.z<0);
    CHECK(flying.sphere.shape.center.y==FX(4));
    sat_physics3_world_reset(&world);

    const sat_aabb3_t platform{{0,0,0},{FX(4),FX(1)/2,FX(4)}};
    const sat_sphere_t supported{{0,FX(3)/2,0},FX(1)};
    const sat_vec3_t moving{FX(1)/2,0,0};
    CHECK(sat_physics3_add_box(
        &world,SAT_PHYSICS3_KINEMATIC_BOX,&platform,&rough,&ball_id)==SAT_OK);
    const uint16_t platform_id=ball_id;
    CHECK(sat_physics3_add_sphere(
        &world,&supported,&moving,&rough,&ball_id)==SAT_OK);
    CHECK(sat_physics3_set_rolling(&world,ball_id,1)==SAT_OK);
    world.gravity={0,-FX(1)/8,0};
    const sat_vec3_t platform_target{FX(1)/4,0,0};
    CHECK(sat_physics3_set_kinematic_target(
        &world,platform_id,&platform_target)==SAT_OK);
    CHECK(sat_physics3_world_step(&world)==SAT_OK);
    const sat_physics3_actor_t ball=read(&world,ball_id);
    CHECK(ball.sphere.flags & SAT_BODY3_GROUNDED);
    CHECK(ball.sphere.vel.x>FX(2)/5 && ball.sphere.vel.x<FX(9)/20);
    CHECK(ball.angular_velocity.z<-FX(1)/6 &&
          ball.angular_velocity.z>-FX(1)/5);
    CHECK(read(&world,platform_id).box.center.x==platform_target.x);
}

static void swept_rising_platform_catches_fast_fall() {
    sat_physics3_actor_t actors[2]{};
    sat_physics3_world_t world{};
    CHECK(sat_physics3_world_init(&world,actors,2,zero,2,3)==SAT_OK);
    const sat_aabb3_t platform{{0,0,0},{FX(3),FX(1)/8,FX(3)}};
    const sat_sphere_t sphere{{0,FX(4),0},FX(1)/2};
    const sat_vec3_t fast_fall{0,-FX(8),0};
    uint16_t platform_id=555,ball_id=555;
    CHECK(sat_physics3_add_box(&world,SAT_PHYSICS3_KINEMATIC_BOX,
                              &platform,&rough,&platform_id)==SAT_OK);
    CHECK(sat_physics3_add_sphere(&world,&sphere,&fast_fall,&rough,
                                  &ball_id)==SAT_OK);
    const sat_vec3_t up={0,FX(1),0};
    CHECK(sat_physics3_set_kinematic_target(&world,platform_id,&up)==SAT_OK);
    const sat_physics3_actor_t before=read(&world,ball_id);
    CHECK(sat_physics3_world_step(&world)==SAT_ERR_CAPACITY);
    CHECK(read(&world,ball_id).sphere.shape.center.y==before.sphere.shape.center.y);
    CHECK(read(&world,platform_id).box.center.y==0);
    CHECK(sat_physics3_set_kinematic_box_ccd(&world,1)==SAT_OK);
    CHECK(sat_physics3_world_step(&world)==SAT_OK);
    const sat_physics3_actor_t ball=read(&world,ball_id);
    CHECK(ball.sphere.flags & SAT_BODY3_GROUNDED);
    CHECK(ball.sphere.shape.center.y>FX(3)/2);
    CHECK(ball.sphere.shape.center.y<FX(7)/4);
    CHECK(ball.sphere.vel.y>=FX(1)-32 && ball.sphere.vel.y<=FX(1)+32);
    CHECK(read(&world,platform_id).box.center.y==FX(1));
}
static void swept_translating_wall_pushes_sphere_without_tunneling() {
    sat_physics3_actor_t actors[2]{};
    sat_physics3_world_t world{};
    CHECK(sat_physics3_world_init(&world,actors,2,zero,1,3)==SAT_OK);
    const sat_aabb3_t wall{{-FX(4),0,0},{FX(1)/8,FX(2),FX(2)}};
    const sat_sphere_t sphere{{0,0,0},FX(1)/2};
    uint16_t wall_id=999,ball_id=999;
    CHECK(sat_physics3_add_box(&world,SAT_PHYSICS3_KINEMATIC_BOX,
                              &wall,&rough,&wall_id)==SAT_OK);
    CHECK(sat_physics3_add_sphere(&world,&sphere,&zero,&rough,
                                  &ball_id)==SAT_OK);
    const sat_vec3_t right={FX(4),0,0};
    CHECK(sat_physics3_set_kinematic_target(&world,wall_id,&right)==SAT_OK);
    CHECK(sat_physics3_set_kinematic_box_ccd(&world,1)==SAT_OK);
    CHECK(sat_physics3_world_step(&world)==SAT_OK);
    const sat_physics3_actor_t ball=read(&world,ball_id);
    CHECK(ball.sphere.flags & SAT_BODY3_HIT_WALL);
    CHECK(ball.sphere.shape.center.x>FX(4)+FX(1)/2);
    CHECK(ball.sphere.shape.center.x<FX(5));
    CHECK(ball.sphere.vel.x>FX(7));
    CHECK(read(&world,wall_id).box.center.x==FX(4));
}
static void distant_kinematic_sweeps_preserve_free_flight(){
    // An intentionally large per-tick displacement exercises the opt-in CCD
    // path with a distant translating box. Its reference-space AABB must be
    // rejected without reporting a wall hit or disturbing the platform.
    {
        sat_physics3_actor_t actors[2]{};
        sat_physics3_world_t w{};
        CHECK(sat_physics3_world_init(&w,actors,2,zero,1,2)==SAT_OK);
        const sat_aabb3_t distant{{FX(100),0,0},{FX(1),FX(1),FX(1)}};
        const sat_sphere_t ball{{0,0,0},FX(1)/2};
        const sat_vec3_t fast{FX(8),0,0};
        const sat_vec3_t moved{FX(101),0,0};
        uint16_t box_id=999,ball_id=999;
        CHECK(sat_physics3_add_box(&w,SAT_PHYSICS3_KINEMATIC_BOX,
                                   &distant,&rough,&box_id)==SAT_OK);
        CHECK(sat_physics3_add_sphere(&w,&ball,&fast,&rough,&ball_id)==SAT_OK);
        CHECK(sat_physics3_set_kinematic_target(&w,box_id,&moved)==SAT_OK);
        CHECK(sat_physics3_set_kinematic_box_ccd(&w,1)==SAT_OK);
        CHECK(sat_physics3_world_step(&w)==SAT_OK);
        CHECK(read(&w,ball_id).sphere.shape.center.x==FX(8));
        CHECK(read(&w,ball_id).sphere.flags==0u);
        CHECK(read(&w,box_id).box.center.x==moved.x);
    }
    // The same remote geometry is stored in immutable LOCAL coordinates.
    // Translation-relative sweeping must not accidentally compare against
    // the mesh's unshifted AABB in world coordinates.
    {
        sat_physics3_actor_t actors[2]{};
        sat_physics3_world_t w{};
        sat_contact3_t contacts[1]{};
        CHECK(sat_physics3_world_init(&w,actors,2,zero,1,2)==SAT_OK);
        CHECK(sat_physics3_set_mesh_contacts(&w,contacts,1)==SAT_OK);
        sat_vec3_t vertices[4]={
            {-FX(2),0,-FX(2)},{FX(2),0,-FX(2)},
            {FX(2),0,FX(2)},{-FX(2),0,FX(2)}};
        uint16_t indices[4]={0,1,2,3};
        sat_mesh_t mesh{vertices,indices,4,4,1,1};
        const sat_vec3_t start{FX(100),0,0},moved{FX(101),0,0};
        const sat_sphere_t ball{{0,FX(2),0},FX(1)/2};
        const sat_vec3_t fast{FX(8),0,0};
        uint16_t mesh_id=999,ball_id=999;
        CHECK(sat_physics3_add_kinematic_mesh(&w,&mesh,nullptr,
                &start,&rough,&mesh_id)==SAT_OK);
        CHECK(sat_physics3_add_sphere(&w,&ball,&fast,&rough,&ball_id)==SAT_OK);
        CHECK(sat_physics3_set_kinematic_mesh_target(&w,mesh_id,&moved)==SAT_OK);
        CHECK(sat_physics3_set_mesh_face_ccd(&w,1)==SAT_OK);
        CHECK(sat_physics3_world_step(&w)==SAT_OK);
        CHECK(read(&w,ball_id).sphere.shape.center.x==FX(8));
        CHECK(read(&w,ball_id).sphere.flags==0u);
        CHECK(read(&w,mesh_id).mesh_offset.x==moved.x);
    }
}
static void discrete_colliders_preserve_capacity_guard_with_swept_platforms() {
    sat_physics3_actor_t actors[3]{};
    sat_physics3_world_t world{};
    CHECK(sat_physics3_world_init(&world,actors,3,zero,1,2)==SAT_OK);
    const sat_aabb3_t platform{{0,0,0},{FX(3),FX(1)/8,FX(3)}};
    const sat_aabb3_t static_box{{FX(20),0,0},{FX(1),FX(1),FX(1)}};
    const sat_sphere_t sphere{{0,FX(4),0},FX(1)/2};
    const sat_vec3_t fast_fall{0,-FX(8),0};
    uint16_t id=999;
    CHECK(sat_physics3_add_box(&world,SAT_PHYSICS3_KINEMATIC_BOX,
                              &platform,&rough,&id)==SAT_OK);
    CHECK(sat_physics3_add_box(&world,SAT_PHYSICS3_STATIC_BOX,
                              &static_box,&rough,&id)==SAT_OK);
    CHECK(sat_physics3_add_sphere(&world,&sphere,&fast_fall,&rough,
                                  &id)==SAT_OK);
    CHECK(sat_physics3_set_kinematic_box_ccd(&world,1)==SAT_OK);
    CHECK(sat_physics3_world_step(&world)==SAT_ERR_CAPACITY);
    CHECK(read(&world,id).sphere.shape.center.y==FX(4));
}

static void translating_finite_mesh_ccd_preserves_geometry() {
    sat_physics3_actor_t actors[2]{};
    sat_physics3_world_t world{};
    sat_contact3_t contacts[1]{};
    CHECK(sat_physics3_world_init(&world,actors,2,zero,2,3)==SAT_OK);
    CHECK(sat_physics3_set_mesh_contacts(&world,contacts,1)==SAT_OK);
    sat_vec3_t vertices[4]={
        {-FX(3),0,-FX(3)},{FX(3),0,-FX(3)},
        {FX(3),0,FX(3)},{-FX(3),0,FX(3)}
    };
    uint16_t indices[4]={0,1,2,3};
    sat_mesh_t mesh{vertices,indices,4,4,1,1};
    const sat_sphere_t sphere{{0,FX(4),0},FX(1)/2};
    const sat_vec3_t fall{0,-FX(8),0};
    const sat_vec3_t rising{0,FX(1),0};
    uint16_t mesh_id=700,ball_id=700;
    CHECK(sat_physics3_add_kinematic_mesh(
        &world,&mesh,nullptr,&zero,&rough,&mesh_id)==SAT_OK);
    CHECK(sat_physics3_add_sphere(
        &world,&sphere,&fall,&rough,&ball_id)==SAT_OK);
    CHECK(sat_physics3_set_kinematic_mesh_target(
        &world,ball_id,&rising)==SAT_ERR_INVALID_ARG);
    CHECK(sat_physics3_set_kinematic_target(
        &world,mesh_id,&rising)==SAT_ERR_INVALID_ARG);
    CHECK(sat_physics3_set_kinematic_mesh_target(
        &world,mesh_id,&rising)==SAT_OK);
    CHECK(sat_physics3_world_step(&world)==SAT_ERR_CAPACITY);
    CHECK(read(&world,mesh_id).mesh_offset.y==0);
    CHECK(read(&world,ball_id).sphere.shape.center.y==FX(4));
    CHECK(sat_physics3_set_mesh_face_ccd(&world,1)==SAT_OK);
    CHECK(sat_physics3_world_step(&world)==SAT_OK);
    const sat_physics3_actor_t ball=read(&world,ball_id);
    const sat_physics3_actor_t platform=read(&world,mesh_id);
    CHECK(ball.sphere.flags & SAT_BODY3_GROUNDED);
    CHECK(ball.sphere.shape.center.y>FX(3)/2-16);
    CHECK(ball.sphere.shape.center.y<FX(3)/2+16);
    CHECK(ball.sphere.vel.y>=FX(1)-32 && ball.sphere.vel.y<=FX(1)+32);
    CHECK(platform.mesh_offset.y==FX(1));
    CHECK(platform.frame_motion.y==FX(1));
    CHECK(platform.mesh==&mesh && mesh.vertices==vertices);
    for(int i=0;i<4;++i)CHECK(vertices[i].y==0);
    CHECK(sat_physics3_set_mesh_contacts(&world,contacts,0)==SAT_ERR_INVALID_ARG);
    sat_physics3_world_reset(&world);
    CHECK(sat_physics3_add_kinematic_mesh(
        &world,&mesh,nullptr,&zero,&rough,&mesh_id)==SAT_OK);
    const sat_sphere_t gap{{FX(9),FX(4),0},FX(1)/2};
    CHECK(sat_physics3_add_sphere(
        &world,&gap,&fall,&rough,&ball_id)==SAT_OK);
    CHECK(sat_physics3_set_kinematic_mesh_target(
        &world,mesh_id,&rising)==SAT_OK);
    CHECK(sat_physics3_world_step(&world)==SAT_OK);
    CHECK(read(&world,ball_id).sphere.flags==0);
    CHECK(read(&world,ball_id).sphere.shape.center.y==-FX(4));
}
static void translating_mesh_grid_matches_linear_contacts() {
    sat_vec3_t vertices[8]={
        {-FX(6),0,-FX(2)},{-FX(2),0,-FX(2)},
        {-FX(2),0,FX(2)},{-FX(6),0,FX(2)},
        {FX(2),0,-FX(2)},{FX(6),0,-FX(2)},
        {FX(6),0,FX(2)},{FX(2),0,FX(2)}
    };
    uint16_t indices[8]={0,1,2,3,4,5,6,7};
    sat_mesh_t mesh{vertices,indices,8,8,2,2};
    uint16_t heads[16]{};
    sat_mesh3_grid_entry_t entries[64]{};
    uint16_t stamps[2]{};
    sat_mesh3_grid_t grid{};
    CHECK(sat_mesh3_grid_init(
        &grid,&mesh,3,heads,16,entries,64,stamps,2)==SAT_OK);
    sat_physics3_actor_t a[3]{},b[3]{};
    sat_physics3_world_t linear{},accelerated{};
    sat_contact3_t contacts_a[2]{},contacts_b[2]{};
    const sat_vec3_t gravity{0,-FX(1)/8,0};
    CHECK(sat_physics3_world_init(&linear,a,3,gravity,16,3)==SAT_OK);
    CHECK(sat_physics3_world_init(&accelerated,b,3,gravity,16,3)==SAT_OK);
    CHECK(sat_physics3_set_mesh_contacts(&linear,contacts_a,2)==SAT_OK);
    CHECK(sat_physics3_set_mesh_contacts(&accelerated,contacts_b,2)==SAT_OK);
    uint16_t id=700;
    CHECK(sat_physics3_add_kinematic_mesh(
        &linear,&mesh,nullptr,&zero,&rough,&id)==SAT_OK && id==0);
    CHECK(sat_physics3_add_kinematic_mesh(
        &accelerated,&mesh,&grid,&zero,&rough,&id)==SAT_OK && id==0);
    const sat_sphere_t supported{{-FX(4),FX(1),0},FX(1)};
    const sat_sphere_t gap{{0,FX(1),0},FX(1)};
    sat_physics3_world_t* worlds[2]={&linear,&accelerated};
    for(sat_physics3_world_t* w : worlds) {
        CHECK(sat_physics3_add_sphere(
            w,&supported,&zero,&rough,&id)==SAT_OK && id==1);
        CHECK(sat_physics3_add_sphere(
            w,&gap,&zero,&rough,&id)==SAT_OK && id==2);
    }
    const sat_vec3_t right{FX(1)/4,0,0};
    CHECK(sat_physics3_set_kinematic_mesh_target(&linear,0,&right)==SAT_OK);
    CHECK(sat_physics3_set_kinematic_mesh_target(&accelerated,0,&right)==SAT_OK);
    CHECK(sat_physics3_world_step(&linear)==SAT_OK);
    CHECK(sat_physics3_world_step(&accelerated)==SAT_OK);
    for(uint16_t i=0;i<3;++i){
        const sat_physics3_actor_t x=read(&linear,i),y=read(&accelerated,i);
        if(i==0)CHECK(x.mesh_offset.x==y.mesh_offset.x);
        else {
            CHECK(x.sphere.shape.center.x==y.sphere.shape.center.x);
            CHECK(x.sphere.shape.center.y==y.sphere.shape.center.y);
            CHECK(x.sphere.vel.x==y.sphere.vel.x);
            CHECK(x.sphere.vel.y==y.sphere.vel.y);
            CHECK(x.sphere.flags==y.sphere.flags);
        }
    }
    CHECK(read(&accelerated,1).sphere.flags & SAT_BODY3_GROUNDED);
    CHECK(read(&accelerated,2).sphere.flags==0);
    sat_mesh_t wrong=mesh;
    grid.mesh=&wrong;
    const sat_physics3_actor_t before=read(&accelerated,1);
    CHECK(sat_physics3_world_step(&accelerated)==SAT_ERR_INVALID_ARG);
    CHECK(read(&accelerated,1).sphere.shape.center.y==
          before.sphere.shape.center.y);
}
static void translating_mesh_validates_grid_and_target_bounds() {
    sat_physics3_actor_t actors[1]{};
    sat_physics3_world_t world{};
    sat_contact3_t contacts[1]{};
    sat_vec3_t vertices[4]={
        {-FX(1),0,-FX(1)},{FX(1),0,-FX(1)},
        {FX(1),0,FX(1)},{-FX(1),0,FX(1)}
    };
    uint16_t indices[4]={0,1,2,3};
    sat_mesh_t mesh{vertices,indices,4,4,1,1};
    sat_mesh3_grid_t invalid_grid{};
    CHECK(sat_physics3_world_init(&world,actors,1,zero,1,2)==SAT_OK);
    CHECK(sat_physics3_set_mesh_contacts(&world,contacts,1)==SAT_OK);
    uint16_t id=700;
    CHECK(sat_physics3_add_kinematic_mesh(
        &world,&mesh,&invalid_grid,&zero,&rough,&id)==SAT_ERR_INVALID_ARG);
    CHECK(id==700 && world.count==0);
    const sat_vec3_t huge{INT32_MAX,0,0};
    CHECK(sat_physics3_add_kinematic_mesh(
        &world,&mesh,nullptr,&huge,&rough,&id)==SAT_ERR_INVALID_ARG);
    CHECK(id==700 && world.count==0);
    CHECK(sat_physics3_add_kinematic_mesh(
        &world,&mesh,nullptr,&zero,&rough,&id)==SAT_OK);
    CHECK(sat_physics3_set_kinematic_mesh_target(
        &world,id,&huge)==SAT_OK);
    CHECK(sat_physics3_world_step(&world)==SAT_ERR_INVALID_ARG);
    CHECK(read(&world,id).mesh_offset.x==0);
    CHECK(read(&world,id).mesh==&mesh);
}

static void tilted_kinematic_mesh_contact_and_grid_match() {
    sat_vec3_t vertices[4]={
        {-FX(2),0,-FX(2)},{FX(2),0,-FX(2)},
        {FX(2),0,FX(2)},{-FX(2),0,FX(2)}
    };
    uint16_t indices[4]={0,1,2,3};
    sat_mesh_t mesh{vertices,indices,4,4,1,1};
    uint16_t heads[16]{};
    sat_mesh3_grid_entry_t entries[32]{};
    uint16_t stamps[1]{};
    sat_mesh3_grid_t grid{};
    CHECK(sat_mesh3_grid_init(
        &grid,&mesh,3,heads,16,entries,32,stamps,1)==SAT_OK);
    sat_physics3_actor_t storage_a[2]{},storage_b[2]{};
    sat_physics3_world_t worlds[2]{};
    sat_contact3_t contacts[2][1]{};
    const sat_vec3_t gravity{0,-FX(1)/8,0};
    const sat_physics3_quat_t tilt{0,0,16962,63303}; // 30 degrees around +Z
    const sat_sphere_t ball{{FX(1),FX(1),0},FX(1)};
    for(uint8_t k=0;k<2;++k){
        sat_physics3_world_t& w=worlds[k];
        CHECK(sat_physics3_world_init(
            &w,k?storage_b:storage_a,2,gravity,64,3)==SAT_OK);
        CHECK(sat_physics3_set_mesh_contacts(&w,contacts[k],1)==SAT_OK);
        uint16_t id=999;
        CHECK(sat_physics3_add_kinematic_mesh(
            &w,&mesh,k?&grid:nullptr,&zero,&rough,&id)==SAT_OK&&id==0);
        CHECK(sat_physics3_add_sphere(
            &w,&ball,&zero,&rough,&id)==SAT_OK&&id==1);
        CHECK(sat_physics3_set_kinematic_mesh_orientation_target(
            &w,0,&tilt)==SAT_OK);
        CHECK(sat_physics3_world_step(&w)==SAT_OK);
        const sat_physics3_actor_t platform=read(&w,0);
        const sat_physics3_actor_t sphere=read(&w,1);
        CHECK(platform.mesh_orientation.z>FX(1)/5);
        CHECK(platform.mesh_orientation.w>FX(9)/10);
        CHECK(sphere.sphere.flags & SAT_BODY3_GROUNDED);
        CHECK(sphere.sphere.shape.center.x<FX(1));
        CHECK(sphere.sphere.shape.center.y>FX(1));
        CHECK(platform.mesh_offset.x==0);
    }
    const sat_physics3_actor_t a=read(&worlds[0],1),b=read(&worlds[1],1);
    CHECK(a.sphere.shape.center.x==b.sphere.shape.center.x);
    CHECK(a.sphere.shape.center.y==b.sphere.shape.center.y);
    CHECK(a.sphere.vel.x==b.sphere.vel.x);
    CHECK(a.sphere.vel.y==b.sphere.vel.y);
    CHECK(a.sphere.flags==b.sphere.flags);
    for(uint8_t k=0;k<4;++k)CHECK(vertices[k].y==0);
}
static void rotation_rejects_unsafe_budget_and_invalid_targets() {
    sat_physics3_actor_t actors[2]{};
    sat_physics3_world_t w{};
    sat_contact3_t contacts[1]{};
    sat_vec3_t vertices[4]={
        {-FX(2),0,-FX(2)},{FX(2),0,-FX(2)},
        {FX(2),0,FX(2)},{-FX(2),0,FX(2)}
    };
    uint16_t indices[4]={0,1,2,3};
    sat_mesh_t mesh{vertices,indices,4,4,1,1};
    CHECK(sat_physics3_world_init(&w,actors,2,zero,1,2)==SAT_OK);
    CHECK(sat_physics3_set_mesh_contacts(&w,contacts,1)==SAT_OK);
    uint16_t mesh_id=999,ball_id=999;
    CHECK(sat_physics3_add_kinematic_mesh(
        &w,&mesh,nullptr,&zero,&rough,&mesh_id)==SAT_OK);
    const sat_sphere_t sphere{{0,FX(1),0},FX(1)};
    CHECK(sat_physics3_add_sphere(
        &w,&sphere,&zero,&rough,&ball_id)==SAT_OK);
    const sat_physics3_quat_t invalid{FX(1),FX(1),0,FX(1)};
    const sat_physics3_quat_t too_large{0,0,46341,46341}; // 90 degrees
    const sat_physics3_quat_t tilt{0,0,16962,63303};
    CHECK(sat_physics3_set_kinematic_mesh_orientation_target(
        &w,ball_id,&tilt)==SAT_ERR_INVALID_ARG);
    CHECK(sat_physics3_set_kinematic_mesh_orientation_target(
        &w,mesh_id,&invalid)==SAT_ERR_INVALID_ARG);
    CHECK(sat_physics3_set_kinematic_mesh_orientation_target(
        &w,mesh_id,&too_large)==SAT_ERR_CAPACITY);
    CHECK(sat_physics3_set_kinematic_mesh_orientation_target(
        &w,mesh_id,&tilt)==SAT_OK);
    CHECK(sat_physics3_set_mesh_face_ccd(&w,1)==SAT_OK);
    CHECK(sat_physics3_world_step(&w)==SAT_ERR_CAPACITY);
    CHECK(read(&w,mesh_id).mesh_orientation.w==SAT_FX16_ONE);
    CHECK(read(&w,ball_id).sphere.shape.center.y==FX(1));
    w.max_substeps=64;
    CHECK(sat_physics3_world_step(&w)==SAT_OK);
    CHECK(read(&w,mesh_id).mesh_orientation.z>FX(1)/5);
}
int main(){
    validation_and_capacity();
    floor_contact_and_bounce();
    mesh_aabb_caches_reference_bounds();
    optional_collider_index_matches_full_scan();
    structural_spatial_bvh_matches_linear_world();
    moving_platform_and_multiple_balls();
    deterministic_replay();
    inclined_plane_and_tangent_friction();
    finite_mesh_ramp_edge_and_gap();
    mesh_registration_validates_indices_and_contact_capacity();
    accelerated_mesh_matches_linear_contacts_and_gaps();
    face_interior_ccd_prevents_through_floor();
    fast_mesh_lip_and_corner_are_not_tunneled();
    opt_in_solid_sphere_rolling_and_orientation();
    free_flight_spin_and_kinematic_relative_rolling();
    swept_rising_platform_catches_fast_fall();
    swept_translating_wall_pushes_sphere_without_tunneling();
    distant_kinematic_sweeps_preserve_free_flight();
    discrete_colliders_preserve_capacity_guard_with_swept_platforms();
    translating_finite_mesh_ccd_preserves_geometry();
    translating_mesh_grid_matches_linear_contacts();
    translating_mesh_validates_grid_and_target_bounds();
    tilted_kinematic_mesh_contact_and_grid_match();
    rotation_rejects_unsafe_budget_and_invalid_targets();
    std::puts("test_physics3_world: 23 tests passed");
    return 0;
}
