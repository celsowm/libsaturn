#ifndef LIBSATURN_CORE_SCENE3D_FACES_LOGIC_HPP
#define LIBSATURN_CORE_SCENE3D_FACES_LOGIC_HPP
#include "saturn/scene3d_faces.h"

namespace saturn::core::scene3d_faces {
inline bool before(const sat_scene3d_face_t& a, const sat_scene3d_face_t& b) {
    return a.pass < b.pass ||
        (a.pass == b.pass &&
         (a.depth > b.depth ||
          (a.depth == b.depth && a.sequence < b.sequence)));
}
inline void sift(sat_scene3d_face_t* faces, uint32_t root, uint32_t end) {
    for (;;) {
        const uint32_t left=root*2u+1u;
        if (left>=end) return;
        uint32_t child=left;
        if (left+1u<end && before(faces[left],faces[left+1u])) ++child;
        if (!before(faces[root],faces[child])) return;
        const sat_scene3d_face_t tmp=faces[root];
        faces[root]=faces[child]; faces[child]=tmp;
        root=child;
    }
}
/* Deterministic O(n log n) in-place heapsort, O(1) extra scratch. */
inline void sort(sat_scene3d_face_t* faces, uint16_t count) {
    if (!faces || count<2u) return;
    for (uint32_t i=count/2u;i>0u;--i) sift(faces,i-1u,count);
    for (uint32_t end=count;end>1u;--end) {
        const sat_scene3d_face_t tmp=faces[0];
        faces[0]=faces[end-1u];faces[end-1u]=tmp;
        sift(faces,0,end-1u);
    }
}
} // namespace saturn::core::scene3d_faces
#endif
