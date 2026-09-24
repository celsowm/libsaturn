#include <cassert>
#include <climits>
#include <cstdio>
#include "src/physics/3d/broadphase.hpp"

namespace bp=saturn::core::physics3::broadphase;
#define FX(n) ((sat_fx16_t)((n)*65536))

int main(){
    const sat_vec3_t low{0,0,0},high{FX(2),FX(2),FX(2)};
    assert(bp::overlaps({FX(3),FX(1),FX(1)},FX(1),low,high));
    assert(!bp::overlaps({FX(3)+1,FX(1),FX(1)},FX(1),low,high));
    assert(!bp::overlaps({FX(1),FX(4),FX(1)},FX(1),low,high));
    assert(!bp::overlaps({FX(1),FX(1),FX(1)},-1,low,high));
    assert(bp::swept_overlaps({-FX(4),FX(1),FX(1)},
        {FX(8),0,0},FX(1),low,high));
    assert(!bp::swept_overlaps({-FX(4),FX(4),FX(1)},
        {FX(8),0,0},FX(1),low,high));
    assert(bp::swept_overlaps({FX(4),FX(1),FX(1)},
        {-FX(4),0,0},FX(1),low,high));
    assert(!bp::swept_overlaps({FX(4),FX(1),FX(4)},
        {-FX(4),0,0},FX(1),low,high));
    const sat_vec3_t box_center{0,0,0};
    const sat_vec3_t box_half{FX(2),FX(1),FX(2)};
    assert(bp::overlaps_box({FX(3),0,0},FX(1),box_center,box_half));
    assert(!bp::overlaps_box({FX(3)+1,0,0},FX(1),box_center,box_half));
    assert(!bp::overlaps_box({0,FX(3),0},FX(1),box_center,box_half));
    assert(!bp::overlaps_box({0,0,0},-1,box_center,box_half));
    assert(!bp::overlaps_box({0,0,0},FX(1),box_center,{-1,0,0}));
    assert(bp::overlaps_box({INT32_MAX,0,0},FX(1),
        {INT32_MAX,0,0},{INT32_MAX,0,0}));
    assert(!bp::overlaps_box({INT32_MIN,0,0},FX(1),
        {INT32_MAX,0,0},{0,0,0}));
    // No Q16 radius expansion overflow at extreme world coordinates.
    const sat_vec3_t edge{INT32_MAX,0,0};
    assert(bp::overlaps(edge,FX(1),edge,edge));
    assert(!bp::overlaps({INT32_MIN,0,0},FX(1),edge,edge));
    assert(!bp::swept_overlaps({INT32_MIN,0,0},{FX(2),0,0},
        FX(1),edge,edge));
    std::puts("mesh AABB broadphase: inclusive, swept, extreme coordinates OK");
}
