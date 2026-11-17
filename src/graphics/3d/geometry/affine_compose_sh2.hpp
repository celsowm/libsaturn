#pragma once
#include "src/core/math3d/logic.hpp"

namespace saturn::core::hierarchy {
#if defined(__sh__)
extern "C" void sat_affine_compose_sh2(sat_fx16_t* out,const sat_fx16_t* a,const sat_fx16_t* b);
#endif
inline bool affine(const sat_mat4_t& a) {
    return !a.m[12]&&!a.m[13]&&!a.m[14]&&a.m[15]==SAT_FX16_ONE;
}
/* Experimental checked facade. The native leaf requires disjoint output
 * and canonical affine rows; aliasing or general matrices use production.
 * No approximate MAC accumulation and no divider. Input arithmetic follows
 * the existing fixed-point-safe mat4 contract. */
[[gnu::noinline]] inline sat_result_t compose_asm(sat_mat4_t* out,const sat_mat4_t* a,const sat_mat4_t* b) {
    if(!out||!a||!b)return SAT_ERR_INVALID_ARG;
#if defined(__sh__)
    if(out!=a&&out!=b&&affine(*a)&&affine(*b)){
        sat_affine_compose_sh2(out->m,a->m,b->m);
        return SAT_OK;
    }
#endif
    return sat_mat4_multiply(out,a,b);
}
/* Independent unrolled C++ candidate. Include output and dispatch cost. */
[[gnu::noinline]] inline sat_result_t compose_cpp(sat_mat4_t* out,const sat_mat4_t* a,const sat_mat4_t* b) {
    if(!out||!a||!b)return SAT_ERR_INVALID_ARG;
    if(out==a||out==b||!affine(*a)||!affine(*b))return sat_mat4_multiply(out,a,b);
    using saturn::core::math3d::fx_mul;
    for(unsigned r=0;r<12;r+=4){
        const auto x=a->m[r],y=a->m[r+1],z=a->m[r+2];
        for(unsigned c=0;c<4;++c)
            out->m[r+c]=fx_mul(x,b->m[c])+fx_mul(y,b->m[c+4])+fx_mul(z,b->m[c+8])+(c==3?a->m[r+3]:0);
    }
    out->m[12]=out->m[13]=out->m[14]=0;out->m[15]=SAT_FX16_ONE;
    return SAT_OK;
}
}
