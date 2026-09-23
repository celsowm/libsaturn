#include "saturn/collide3d.h"
#include <limits.h>
#include "src/physics/3d/collision_logic.hpp"

namespace {
using namespace saturn::core::collide3d;
using V=sat_vec3_t;
using F=sat_fx16_t;

/* Distance squared to a planar convex quad (triangle faces may repeat their
 * last vertex). The closest point is the plane projection if it lies inside,
 * or the nearest point on any finite edge, including its endpoints. This is
 * the SAME geometric construction as sphere_quad_contact's discrete path. */
struct Closest { V point; uint64_t distance2; uint8_t feature; };
Closest closest_point(const sat_quad3_t& quad, V center, V normal) {
    const F signed_distance=static_cast<F>(
        vec3_dot_raw(sub(center,quad.v[0]),normal)>>16);
    const V projected=sub(center,mul(normal,signed_distance));
    if(inside_quad(quad,projected,normal))
        return {projected,len2(sub(center,projected)),0};
    Closest nearest{{0,0,0},UINT64_MAX,0};
    for(uint8_t edge=0;edge<4;++edge){
        const V a=quad.v[edge];
        const V direction=sub(quad.v[(edge+1)&3],a);
        const uint64_t edge_length2=len2(direction);
        F fraction=edge_length2
            ? ratio(vec3_dot_raw(sub(center,a),direction),
                    static_cast<int64_t>(edge_length2))
            : 0;
        if(fraction<0)fraction=0;
        if(fraction>SAT_FX16_ONE)fraction=SAT_FX16_ONE;
        const V point=at(a,direction,fraction);
        const uint64_t distance2=len2(sub(center,point));
        if(distance2<nearest.distance2)
            nearest={point,distance2,static_cast<uint8_t>(
                fraction==0||fraction==SAT_FX16_ONE ? 2 : 1)};
    }
    return nearest;
}

/* AABB of a FACE expanded by the sphere radius, intersected with the
 * AABB of the center trajectory. Prevents expensive minimization for
 * distant geometry and avoids mistaking a plane extension for a quad. */
bool swept_bounds_intersect(const sat_quad3_t& quad,V start,V end,F radius) {
    const int32_t s[3]={start.x,start.y,start.z};
    const int32_t e[3]={end.x,end.y,end.z};
    for(uint8_t axis=0;axis<3;++axis){
        int32_t low=axis==0?quad.v[0].x:axis==1?quad.v[0].y:quad.v[0].z;
        int32_t high=low;
        for(uint8_t i=1;i<4;++i){
            const int32_t value=axis==0?quad.v[i].x:
                axis==1?quad.v[i].y:quad.v[i].z;
            if(value<low)low=value;
            if(value>high)high=value;
        }
        const int64_t path_lo=s[axis]<e[axis]?s[axis]:e[axis];
        const int64_t path_hi=s[axis]>e[axis]?s[axis]:e[axis];
        if(path_hi<static_cast<int64_t>(low)-radius ||
           path_lo>static_cast<int64_t>(high)+radius)return false;
    }
    return true;
}

sat_result_t cast_mesh(const sat_mesh_t& mesh,const sat_sphere_t& sphere,
                       V delta,sat_sphere_mesh_hit_t& result,uint8_t& found) {
    found=0;
    if(!mesh.vertices||!mesh.indices||!mesh.face_count||!mesh.vertex_count||
       mesh.face_count>mesh.face_cap||mesh.vertex_count>mesh.vertex_cap||
       sphere.radius<=0)return SAT_ERR_INVALID_ARG;
    const int64_t end_x=static_cast<int64_t>(sphere.center.x)+delta.x;
    const int64_t end_y=static_cast<int64_t>(sphere.center.y)+delta.y;
    const int64_t end_z=static_cast<int64_t>(sphere.center.z)+delta.z;
    if(end_x<INT32_MIN||end_x>INT32_MAX||end_y<INT32_MIN||
       end_y>INT32_MAX||end_z<INT32_MIN||end_z>INT32_MAX)
        return SAT_ERR_INVALID_ARG;
    for(uint32_t idx=0;idx<static_cast<uint32_t>(mesh.face_count)*4u;++idx)
        if(mesh.indices[idx]>=mesh.vertex_count)return SAT_ERR_INVALID_ARG;
    if(!delta.x&&!delta.y&&!delta.z)return SAT_OK;
    const V end{static_cast<F>(end_x),static_cast<F>(end_y),
                static_cast<F>(end_z)};
    const uint64_t radius2=static_cast<uint64_t>(
        static_cast<int64_t>(sphere.radius)*sphere.radius);
    for(uint16_t face=0;face<mesh.face_count;++face){
        sat_quad3_t quad{};
        if(saturn::core::geometry::face_quad(&mesh,face,&quad)!=SAT_OK)
            return SAT_ERR_INVALID_ARG;
        if(!swept_bounds_intersect(quad,sphere.center,end,sphere.radius))
            continue;
        const V normal=unit(quad_normal_scaled(quad));
        if(!normal.x&&!normal.y&&!normal.z)continue;
        const Closest initial=closest_point(quad,sphere.center,normal);
        /* Existing contact solver owns initial overlap. Only a genuinely
         * approaching/touching sweep may generate a new impact. */
        if(initial.distance2<radius2)continue;
        const auto distance_at=[&](uint32_t time)->uint64_t {
            const V center=at(sphere.center,delta,static_cast<F>(time));
            return closest_point(quad,center,normal).distance2;
        };
        /* Squared distance to a convex set along a straight line is convex.
         * First find its minimum in fixed-point time [0,1] (24 samples near
         * the minimizer), then binary search the earliest contact. */
        uint32_t lo=0,hi=SAT_FX16_ONE;
        for(uint8_t iter=0;iter<32&&hi-lo>3;++iter){
            const uint32_t left=lo+(hi-lo)/3u;
            const uint32_t right=hi-(hi-lo)/3u;
            if(distance_at(left)>distance_at(right))lo=left;
            else hi=right;
        }
        uint32_t best_time=lo;
        uint64_t best_distance=UINT64_MAX;
        for(uint32_t time=lo;time<=hi;++time){
            const uint64_t distance=distance_at(time);
            if(distance<best_distance){best_distance=distance;best_time=time;}
        }
        if(best_distance>radius2)continue;
        if(best_time==0 && initial.distance2==radius2 &&
           distance_at(1)>=radius2)continue;
        uint32_t first=0,last=best_time;
        while(first<last){
            const uint32_t middle=first+(last-first)/2u;
            if(distance_at(middle)<=radius2)last=middle;
            else first=middle+1u;
        }
        if(found&&first>=static_cast<uint32_t>(result.t))continue;
        const V center=at(sphere.center,delta,static_cast<F>(first));
        const Closest closest=closest_point(quad,center,normal);
        const V separation=sub(center,closest.point);
        const V contact_normal=unit(separation);
        if(!contact_normal.x&&!contact_normal.y&&!contact_normal.z)
            continue;
        const int64_t approach=vec3_dot_raw(delta,contact_normal);
        if(approach>=0)continue;
        result.t=static_cast<F>(first);
        result.center=center;
        result.point=closest.point;
        result.normal=contact_normal;
        result.face=face;
        result.feature=closest.feature;
        found=1;
    }
    return SAT_OK;
}
} // namespace

extern "C" sat_result_t sat_sphere_cast_mesh(
    const sat_mesh_t* mesh,const sat_sphere_t* sphere,
    const sat_vec3_t* delta,sat_sphere_mesh_hit_t* out,uint8_t* found){
    if(!mesh||!sphere||!delta||!out||!found)return SAT_ERR_INVALID_ARG;
    sat_sphere_mesh_hit_t candidate{};
    uint8_t hit=0;
    const sat_result_t status=cast_mesh(*mesh,*sphere,*delta,candidate,hit);
    if(status!=SAT_OK)return status;
    *found=hit;
    if(hit)*out=candidate;
    return SAT_OK;
}
