#ifndef SATURN_PHYSICS3_WORLD_SPATIAL_HPP
#define SATURN_PHYSICS3_WORLD_SPATIAL_HPP

#include <limits.h>
#include <stdint.h>
#include "saturn/physics3_world.h"

/* No STL, heap, hardware, singletons or game-specific types. Leaves use
 * caller-owned slots [0,leaf_count), branches [leaf_count,2*leaf_count-1).
 * All bounds are 64-bit Q16.16 world-coordinate intervals, inclusive. */
namespace saturn::core::physics3::spatial {
using Node=sat_physics3_spatial_node_t;
using Actor=sat_physics3_actor_t;
using V=sat_vec3_t;

inline int64_t min64(int64_t a,int64_t b){return a<b?a:b;}
inline int64_t max64(int64_t a,int64_t b){return a>b?a:b;}
inline int64_t abs64(int64_t a){return a<0?-a:a;}

inline void box_bounds(const sat_aabb3_t& box,const V& target,
                       bool moving,Node& out) {
    out.low_x=min64(box.center.x,moving?target.x:box.center.x)-box.half.x;
    out.low_y=min64(box.center.y,moving?target.y:box.center.y)-box.half.y;
    out.low_z=min64(box.center.z,moving?target.z:box.center.z)-box.half.z;
    out.high_x=max64(box.center.x,moving?target.x:box.center.x)+box.half.x;
    out.high_y=max64(box.center.y,moving?target.y:box.center.y)+box.half.y;
    out.high_z=max64(box.center.z,moving?target.z:box.center.z)+box.half.z;
}

inline void mesh_bounds(const Actor& actor,Node& out) {
    const V low=actor.mesh_bounds_min,high=actor.mesh_bounds_max;
    if(actor.kind==SAT_PHYSICS3_STATIC_MESH){
        out.low_x=low.x;out.low_y=low.y;out.low_z=low.z;
        out.high_x=high.x;out.high_y=high.y;out.high_z=high.z;
        return;
    }
    const V start=actor.mesh_offset,end=actor.target_center;
    const sat_physics3_quat_t a=actor.mesh_orientation;
    const sat_physics3_quat_t b=actor.mesh_target_orientation;
    const bool no_rotation=
        a.x==0 && a.y==0 && a.z==0 && a.w==SAT_FX16_ONE &&
        b.x==0 && b.y==0 && b.z==0 && b.w==SAT_FX16_ONE;
    if(no_rotation) {
        out.low_x=min64((int64_t)start.x+low.x,(int64_t)end.x+low.x);
        out.low_y=min64((int64_t)start.y+low.y,(int64_t)end.y+low.y);
        out.low_z=min64((int64_t)start.z+low.z,(int64_t)end.z+low.z);
        out.high_x=max64((int64_t)start.x+high.x,(int64_t)end.x+high.x);
        out.high_y=max64((int64_t)start.y+high.y,(int64_t)end.y+high.y);
        out.high_z=max64((int64_t)start.z+high.z,(int64_t)end.z+high.z);
        return;
    }
    /* All unit-quaternion rotations keep every local point inside this
     * conservative L1 radius. Covers EVERY intermediate orientation, even
     * without a rotational CCD implementation. */
    const int64_t radius=
        max64(abs64(low.x),abs64(high.x))+
        max64(abs64(low.y),abs64(high.y))+
        max64(abs64(low.z),abs64(high.z));
    out.low_x=min64(start.x,end.x)-radius;
    out.low_y=min64(start.y,end.y)-radius;
    out.low_z=min64(start.z,end.z)-radius;
    out.high_x=max64(start.x,end.x)+radius;
    out.high_y=max64(start.y,end.y)+radius;
    out.high_z=max64(start.z,end.z)+radius;
}

inline bool less_x(const Node& a,const Node& b) {
    return a.low_x<b.low_x ||
        (a.low_x==b.low_x && a.actor_id<b.actor_id);
}
inline void swap_nodes(Node& a,Node& b){const Node tmp=a;a=b;b=tmp;}

inline void node_heap_down(Node* nodes,uint32_t index,uint32_t count) {
    while(index<count/2u){
        const uint32_t left=2u*index+1u;
        if(left>=count)break;
        uint32_t largest=left;
        if(left+1u<count && less_x(nodes[left],nodes[left+1u]))
            largest=left+1u;
        if(!less_x(nodes[index],nodes[largest]))break;
        swap_nodes(nodes[index],nodes[largest]);
        index=largest;
    }
}
inline void sort_nodes(Node* nodes,uint16_t count) {
    for(uint32_t i=count/2u;i>0u;--i)
        node_heap_down(nodes,i-1u,count);
    for(uint32_t end=count;end>1u;--end){
        swap_nodes(nodes[0],nodes[end-1u]);
        node_heap_down(nodes,0u,end-1u);
    }
}
inline void id_heap_down(uint16_t* ids,uint16_t index,uint16_t count){
    while(static_cast<uint32_t>(index)*2u+1u<count){
        const uint16_t left=static_cast<uint16_t>(2u*index+1u);
        uint16_t largest=left;
        if(static_cast<uint32_t>(left)+1u<count && ids[left]<ids[left+1u])
            largest=left+1u;
        if(ids[index]>=ids[largest])break;
        const uint16_t tmp=ids[index];ids[index]=ids[largest];
        ids[largest]=tmp;index=largest;
    }
}
inline void sort_ids(uint16_t* ids,uint16_t count){
    for(uint32_t i=count/2u;i>0u;--i)
        id_heap_down(ids,static_cast<uint16_t>(i-1u),count);
    for(uint32_t end=count;end>1u;--end){
        const uint16_t tmp=ids[0];ids[0]=ids[end-1u];
        ids[end-1u]=tmp;
        id_heap_down(ids,0u,static_cast<uint16_t>(end-1u));
    }
}

inline Node unite(const Node& left,const Node& right) {
    Node n{};
    n.actor_id=UINT16_MAX;
    n.low_x=min64(left.low_x,right.low_x);
    n.low_y=min64(left.low_y,right.low_y);
    n.low_z=min64(left.low_z,right.low_z);
    n.high_x=max64(left.high_x,right.high_x);
    n.high_y=max64(left.high_y,right.high_y);
    n.high_z=max64(left.high_z,right.high_z);
    return n;
}
inline uint32_t make_branch(Node* nodes,uint32_t start,uint32_t end,
                            uint32_t& next) {
    if(end-start==1u)return start;
    const uint32_t middle=start+(end-start)/2u;
    const uint32_t left=make_branch(nodes,start,middle,next);
    const uint32_t right=make_branch(nodes,middle,end,next);
    const uint32_t result=next++;
    nodes[result]=unite(nodes[left],nodes[right]);
    nodes[result].left=left;nodes[result].right=right;
    return result;
}

inline void build(sat_physics3_world_t& world) {
    uint16_t count=0u,unbounded=0u;
    for(uint16_t id=0u;id<world.count;++id){
        const Actor& actor=world.actors[id];
        if(actor.kind==SAT_PHYSICS3_DYNAMIC_SPHERE)continue;
        if(actor.kind==SAT_PHYSICS3_STATIC_PLANE){
            world.collider_indices[unbounded++]=id;
            continue;
        }
        Node leaf{};
        leaf.actor_id=id;
        if(actor.kind==SAT_PHYSICS3_STATIC_BOX ||
           actor.kind==SAT_PHYSICS3_KINEMATIC_BOX)
            box_bounds(actor.box,actor.target_center,
                       actor.kind==SAT_PHYSICS3_KINEMATIC_BOX,leaf);
        else
            mesh_bounds(actor,leaf);
        world.spatial_nodes[count++]=leaf;
    }
    world.spatial_leaf_count=count;
    world.spatial_fallback_count=unbounded;
    world.spatial_queries=0u;
    world.spatial_candidates_checked=0u;
    world.spatial_root=UINT32_MAX;
    if(count==0u)return;
    sort_nodes(world.spatial_nodes,count);
    uint32_t next=count;
    world.spatial_root=make_branch(world.spatial_nodes,0u,count,next);
}

/* Closed intervals: a sphere touching a wall or platform must be included. */
inline bool intersects(const Node& bounds,const Node& query) {
    return bounds.low_x<=query.high_x && bounds.high_x>=query.low_x &&
           bounds.low_y<=query.high_y && bounds.high_y>=query.low_y &&
           bounds.low_z<=query.high_z && bounds.high_z>=query.low_z;
}
inline Node swept_sphere_bounds(V center,V displacement,
                                sat_fx16_t radius) {
    Node result{};
    const int64_t x=(int64_t)center.x+displacement.x;
    const int64_t y=(int64_t)center.y+displacement.y;
    const int64_t z=(int64_t)center.z+displacement.z;
    result.low_x=min64(center.x,x)-radius;
    result.low_y=min64(center.y,y)-radius;
    result.low_z=min64(center.z,z)-radius;
    result.high_x=max64(center.x,x)+radius;
    result.high_y=max64(center.y,y)+radius;
    result.high_z=max64(center.z,z)+radius;
    return result;
}

inline void query_tree(const Node* nodes,uint32_t index,const Node& query,
                       uint16_t* out,uint16_t& count) {
    const Node& node=nodes[index];
    if(!intersects(node,query))return;
    if(node.actor_id!=UINT16_MAX) {
        out[count++]=node.actor_id;
        return;
    }
    query_tree(nodes,node.left,query,out,count);
    query_tree(nodes,node.right,query,out,count);
}

inline uint16_t query(sat_physics3_world_t& world,V center,
                      V displacement,sat_fx16_t radius) {
    ++world.spatial_queries;
    if(world.spatial_root==UINT32_MAX)return 0u;
    const Node bounds=swept_sphere_bounds(center,displacement,radius);
    uint16_t count=0u;
    query_tree(world.spatial_nodes,world.spatial_root,bounds,
               world.spatial_candidates,count);
    sort_ids(world.spatial_candidates,count);
    return count;
}

struct Cursor {
    const sat_physics3_world_t& world;
    uint16_t spatial_count;
    uint16_t fallback_count;
    uint16_t spatial_at;
    uint16_t fallback_at;
    uint16_t linear_at;

    bool next(uint16_t& id) {
        if(!world.spatial_nodes) {
            const uint16_t total=world.collider_indices?
                fallback_count:world.count;
            if(linear_at>=total)return false;
            id=world.collider_indices?
                world.collider_indices[linear_at++]:linear_at++;
            return true;
        }
        if(spatial_at>=spatial_count && fallback_at>=fallback_count)
            return false;
        if(fallback_at>=fallback_count ||
           (spatial_at<spatial_count &&
            world.spatial_candidates[spatial_at] <
            world.collider_indices[fallback_at])) {
            id=world.spatial_candidates[spatial_at++];
        } else {
            id=world.collider_indices[fallback_at++];
        }
        return true;
    }
};

} // namespace saturn::core::physics3::spatial

#endif
