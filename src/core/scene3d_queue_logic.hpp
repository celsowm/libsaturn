#ifndef LIBSATURN_CORE_SCENE3D_QUEUE_LOGIC_HPP
#define LIBSATURN_CORE_SCENE3D_QUEUE_LOGIC_HPP

#include "saturn/scene3d.h"

namespace saturn::core::scene3d_queue {

/* The camera vector is computed once per frame. The 64-bit accumulator
 * retains sub-unit depth precision, and includes the vertical component.
 * Do not use squared Euclidean distance: it cannot distinguish an object
 * behind the camera from one in front. Like math3d, normal Saturn-scale
 * world coordinates (hundreds of 16.16 units) are the supported range. */
inline int64_t depth_raw(const sat_camera3d_t& camera, const sat_vec3_t& point) {
    const int64_t dx = static_cast<int64_t>(camera.target.x) - camera.eye.x;
    const int64_t dy = static_cast<int64_t>(camera.target.y) - camera.eye.y;
    const int64_t dz = static_cast<int64_t>(camera.target.z) - camera.eye.z;
    return ((static_cast<int64_t>(point.x) - camera.eye.x) * dx +
            (static_cast<int64_t>(point.y) - camera.eye.y) * dy +
            (static_cast<int64_t>(point.z) - camera.eye.z) * dz) / SAT_FX16_ONE;
}
inline bool before(const sat_scene3d_queue_item_t& a,
                   const sat_scene3d_queue_item_t& b) {
    return a.pass < b.pass ||
        (a.pass == b.pass &&
         (a.depth > b.depth ||
          (a.depth == b.depth && a.submission < b.submission)));
}
inline void swap(sat_scene3d_queue_item_t& a, sat_scene3d_queue_item_t& b) {
    sat_scene3d_queue_item_t tmp = a;
    a = b;
    b = tmp;
}
/* Heap root is the LATEST paint item, so moving it to the tail on each
 * iteration yields EARLIEST-to-LATEST (back-to-front) order in-place.
 * Deterministic O(n log n), O(1) scratch; no libc qsort or hidden heap. */
inline void sift(sat_scene3d_queue_item_t* items, uint32_t root, uint32_t end) {
    for (;;) {
        uint32_t child=root*2u+1u;
        if (child>=end) return;
        if (child+1u<end && before(items[child],items[child+1u])) ++child;
        if (!before(items[root],items[child])) return;
        swap(items[root],items[child]);
        root=child;
    }
}
inline void sort(sat_scene3d_queue_item_t* items, uint16_t count) {
    if (!items || count<2u) return;
    for (uint32_t i=count/2u; i>0u; --i) sift(items,i-1u,count);
    for (uint32_t end=count; end>1u; --end) {
        swap(items[0],items[end-1u]);
        sift(items,0,end-1u);
    }
}

} // namespace saturn::core::scene3d_queue
#endif
