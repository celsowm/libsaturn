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
