#pragma once
#include "src/core/math3d/logic.hpp"

namespace saturn::core::hierarchy {
#if defined(__sh__)
extern "C" void sat_affine_compose_sh2(sat_fx16_t* out,const sat_fx16_t* a,const sat_fx16_t* b);
#endif
inline bool affine(const sat_mat4_t& a) {
    return !a.m[12]&&!a.m[13]&&!a.m[14]&&a.m[15]==SAT_FX16_ONE;
}
/* Hierarchy compose. The SH-2 leaf needs disjoint output and canonical
 * affine rows; aliasing or general matrices use sat_mat4_multiply. Same
 * products and rounding as the mat4 contract (docs/HIERARCHY_CHARACTER_AB.md). */
[[gnu::noinline]] inline sat_result_t compose_affine(sat_mat4_t* out,const sat_mat4_t* a,const sat_mat4_t* b) {
    if(!out||!a||!b)return SAT_ERR_INVALID_ARG;
#if defined(__sh__)
    if(out!=a&&out!=b&&affine(*a)&&affine(*b)){
        sat_affine_compose_sh2(out->m,a->m,b->m);
        return SAT_OK;
    }
#endif
    return sat_mat4_multiply(out,a,b);
}

}
