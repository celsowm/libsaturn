#ifndef SATURN_PHYSICS3_BROADPHASE_HPP
#define SATURN_PHYSICS3_BROADPHASE_HPP

#include <stdint.h>
#include "saturn/core.h"
#include "saturn/math3d.h"

/* Conservative finite-mesh AABB rejection. Use 64-bit comparisons so
 * expanding a Q16.16 sphere at INT32 extrema cannot wrap to the far side
 * of the world. Boundary touch MUST remain a candidate. This does not
 * replace a mesh grid's per-face acceleration or narrowphase contact. */
namespace saturn::core::physics3::broadphase {

inline bool overlaps(const sat_vec3_t& center,sat_fx16_t radius,
                     const sat_vec3_t& low,const sat_vec3_t& high) {
    if(radius<0)return false;
    return (int64_t)center.x+radius>=low.x &&
           (int64_t)center.x-radius<=high.x &&
           (int64_t)center.y+radius>=low.y &&
           (int64_t)center.y-radius<=high.y &&
           (int64_t)center.z+radius>=low.z &&
           (int64_t)center.z-radius<=high.z;
}

/* AABB half-extents may lie near INT32_MAX; form the expanded
 * sphere-vs-box interval in int64 rather than overflowing Q16 positions.
 * This is only a conservative rejection gate before exact narrowphase. */
inline bool overlaps_box(const sat_vec3_t& center,sat_fx16_t radius,
                         const sat_vec3_t& box_center,
                         const sat_vec3_t& box_half) {
    if(radius<0 || box_half.x<0 || box_half.y<0 || box_half.z<0)
        return false;
    return (int64_t)center.x+radius>=(int64_t)box_center.x-box_half.x &&
           (int64_t)center.x-radius<=(int64_t)box_center.x+box_half.x &&
           (int64_t)center.y+radius>=(int64_t)box_center.y-box_half.y &&
           (int64_t)center.y-radius<=(int64_t)box_center.y+box_half.y &&
           (int64_t)center.z+radius>=(int64_t)box_center.z-box_half.z &&
           (int64_t)center.z-radius<=(int64_t)box_center.z+box_half.z;
}

inline bool swept_overlaps(const sat_vec3_t& start,const sat_vec3_t& delta,
                           sat_fx16_t radius,
                           const sat_vec3_t& low,const sat_vec3_t& high) {
    if(radius<0)return false;
    const int64_t end_x=(int64_t)start.x+delta.x;
    const int64_t end_y=(int64_t)start.y+delta.y;
    const int64_t end_z=(int64_t)start.z+delta.z;
    return ((int64_t)start.x<end_x?(int64_t)start.x:end_x)-radius<=high.x &&
           ((int64_t)start.x>end_x?(int64_t)start.x:end_x)+radius>=low.x &&
           ((int64_t)start.y<end_y?(int64_t)start.y:end_y)-radius<=high.y &&
           ((int64_t)start.y>end_y?(int64_t)start.y:end_y)+radius>=low.y &&
           ((int64_t)start.z<end_z?(int64_t)start.z:end_z)-radius<=high.z &&
           ((int64_t)start.z>end_z?(int64_t)start.z:end_z)+radius>=low.z;
}

} // namespace saturn::core::physics3::broadphase

#endif
