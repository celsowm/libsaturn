#pragma once
#include <stdint.h>

// Exact Q15 two-influence blend: w*a + (32768-w)*b == 32768*b + w*(a-b),
// so when a-b does not overflow the blend is b plus one rounded product.
// Validated weights are nonnegative and at most 32768.
namespace saturn::skin3d::weighted_delta {
inline int32_t round_one(int32_t a,int32_t w) {
#if defined(__sh__)
    uint32_t hi,lo;
    asm volatile("dmuls.l %[a],%[w]\n\t"
        "sts mach,%[hi]\n\tsts macl,%[lo]\n\t"
        "clrt\n\taddc %[round],%[lo]\n\taddc %[zero],%[hi]\n\t"
        "shll %[lo]\n\trotcl %[hi]\n\txtrct %[hi],%[lo]"
        :[hi]"=&r"(hi),[lo]"=&r"(lo)
        :[a]"r"(a),[w]"r"(w),[round]"r"(16384u),[zero]"r"(0u)
        :"mach","macl","t");
    return int32_t(lo);
#else
    return int32_t((int64_t(a)*w+16384)>>15);
#endif
}
// The 64-bit sum, for the rare deltas that overflow. Out of line: inlined
// at every store it cost Tekken Ball's full Work RAM High 700 bytes.
[[gnu::noinline,gnu::cold]] inline int32_t pair_wide(int32_t a,int32_t b,int32_t w) {
    return int32_t((int64_t(w)*a+int64_t(32768-w)*b+16384)>>15);
}
// The SH-2 signed-subtract overflow flag guards the delta form; on
// overflow the 64-bit sum gives the same result.
inline int32_t pair(int32_t a,int32_t b,int32_t w) {
    int32_t d=a;
#if defined(__sh__)
    uint32_t overflow;
    asm volatile("subv %[b],%[d]\n\tmovt %[overflow]"
        :[d]"+&r"(d),[overflow]"=&r"(overflow):[b]"r"(b):"t");
    if(!overflow)return int32_t(uint32_t(b)+uint32_t(round_one(d,w)));
#else
    const uint32_t x=uint32_t(a),y=uint32_t(b),z=x-y;
    d=int32_t(z);
    if(((x^y)&(x^z)&0x80000000u)==0)return int32_t(uint32_t(b)+uint32_t(round_one(d,w)));
#endif
    return pair_wide(a,b,w);
}
}
