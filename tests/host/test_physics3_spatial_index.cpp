#include <cassert>
#include <cstdint>
#include <cstdio>
#include "src/physics/3d/world_spatial.hpp"

#define FX(n) ((sat_fx16_t)((n)*SAT_FX16_ONE))

int main() {
    namespace bvh=saturn::core::physics3::spatial;
    sat_physics3_actor_t actors[6]{};
    sat_physics3_spatial_node_t nodes[11]{};
    uint16_t fallback[6]{},candidates[6]{};
    sat_physics3_world_t world{};
    world.actors=actors;world.count=world.capacity=6u;
    world.spatial_nodes=nodes;world.collider_indices=fallback;
    world.spatial_candidates=candidates;

    actors[0].kind=SAT_PHYSICS3_DYNAMIC_SPHERE;
    actors[1].kind=SAT_PHYSICS3_STATIC_BOX;
    actors[1].box={{FX(100),0,0},{FX(1),FX(1),FX(1)}};
    actors[2].kind=SAT_PHYSICS3_KINEMATIC_BOX;
    actors[2].box={{FX(10),0,0},{FX(1),FX(1),FX(1)}};
    actors[2].target_center={0,0,0}; // FULL-TICK sweep reaches the sphere.
    actors[3].kind=SAT_PHYSICS3_STATIC_PLANE;
    actors[4].kind=SAT_PHYSICS3_STATIC_MESH;
    actors[4].mesh_bounds_min={-FX(1),-FX(1),-FX(1)};
    actors[4].mesh_bounds_max={FX(1),FX(1),FX(1)};
    actors[5].kind=SAT_PHYSICS3_KINEMATIC_MESH;
    actors[5].mesh_bounds_min={-FX(1),-FX(1),-FX(1)};
    actors[5].mesh_bounds_max={FX(1),FX(1),FX(1)};
    actors[5].mesh_offset={FX(9),0,0};
    actors[5].target_center={FX(1),0,0};
    actors[5].mesh_orientation={0,0,0,SAT_FX16_ONE};
    actors[5].mesh_target_orientation={0,0,FX(1)/2,FX(3)/4};

    bvh::build(world);
    assert(world.spatial_leaf_count==4u && world.spatial_fallback_count==1u);
    assert(world.spatial_root>=4u && world.spatial_root<7u);
    const sat_vec3_t zero={0,0,0};
    uint16_t count=bvh::query(world,zero,zero,FX(1));
    assert(count==3u);
    assert(candidates[0]==2u && candidates[1]==4u && candidates[2]==5u);
    bvh::Cursor ordered{world,count,world.spatial_fallback_count,0u,0u,0u};
    uint16_t id=0u;
    const uint16_t expected_order[4]={2u,3u,4u,5u};
    for(const uint16_t expected:expected_order) {
        assert(ordered.next(id) && id==expected);
    }
    assert(!ordered.next(id));

    /* A swept sphere hitting a far collider must not be culled based
     * only on its initial position. */
    const sat_vec3_t forward={FX(100),0,0};
    count=bvh::query(world,zero,forward,FX(1));
    assert(count==4u && candidates[0]==1u);
    assert(world.spatial_queries==2u);

    /* Signed Q16.16 extrema cannot wrap the query or BVH bounds. */
    actors[1].box.center={INT32_MAX,0,0};
    bvh::build(world);
    const sat_vec3_t edge={INT32_MAX,0,0};
    count=bvh::query(world,edge,zero,FX(1));
    bool found_edge=false;
    for(uint16_t i=0u;i<count;++i)
        if(candidates[i]==1u)found_edge=true;
    assert(found_edge);
    std::puts("physics world caller-owned BVH: OK");
}
