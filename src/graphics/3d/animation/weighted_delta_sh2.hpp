#pragma once
#include <stdint.h>

// Exact Q15 weighted-sum reformulation. Validated weights are nonnegative
// and sum to 32768. Products and rounded sums fit signed 64-bit.
namespace saturn::skin3d::weighted_delta {
inline bool delta(int32_t a,int32_t b,int32_t& d) {
    const uint32_t x=uint32_t(a),y=uint32_t(b),z=x-y;
    d=int32_t(z);
    return ((x^y)&(x^z)&0x80000000u)==0;
}
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
inline int32_t round_two(int32_t a,int32_t w,int32_t b,int32_t v) {
#if defined(__sh__)
    uint32_t hi,lo,h2,l2;
    asm volatile("dmuls.l %[a],%[w]\n\t"
        "sts mach,%[hi]\n\tsts macl,%[lo]\n\t"
        "dmuls.l %[b],%[v]\n\t"
        "sts mach,%[h2]\n\tsts macl,%[l2]\n\t"
        "clrt\n\taddc %[l2],%[lo]\n\taddc %[h2],%[hi]\n\t"
        "clrt\n\taddc %[round],%[lo]\n\taddc %[zero],%[hi]\n\t"
        "shll %[lo]\n\trotcl %[hi]\n\txtrct %[hi],%[lo]"
        :[hi]"=&r"(hi),[lo]"=&r"(lo),[h2]"=&r"(h2),[l2]"=&r"(l2)
        :[a]"r"(a),[w]"r"(w),[b]"r"(b),[v]"r"(v),[round]"r"(16384u),[zero]"r"(0u)
        :"mach","macl","t");
    return int32_t(lo);
#else
    return int32_t((int64_t(a)*w+int64_t(b)*v+16384)>>15);
#endif
}
inline int32_t pair(int32_t a,int32_t b,int32_t w) {
    int32_t d;
    if(delta(a,b,d))return int32_t(uint32_t(b)+uint32_t(round_one(d,w)));
    return int32_t((int64_t(w)*a+int64_t(32768-w)*b+16384)>>15);
}
inline int32_t triple(int32_t a,int32_t b,int32_t c,int32_t w,int32_t v) {
    int32_t da,db;
    if(delta(a,c,da)&&delta(b,c,db))
        return int32_t(uint32_t(c)+uint32_t(round_two(da,w,db,v)));
    return int32_t((int64_t(w)*a+int64_t(v)*b+int64_t(32768-w-v)*c+16384)>>15);
}
}
