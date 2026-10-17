#ifndef SATURN_CORE_MEMORY_BYTES_H
#define SATURN_CORE_MEMORY_BYTES_H
#include <stdint.h>
#include <stddef.h>

/* Aligned normal-RAM operations used by the freestanding libc. SH-2 word
 * accesses require alignment; unrelated source/destination alignments keep
 * the byte path. may_alias permits copying arbitrary object representations. */
#if defined(__GNUC__)
typedef uint32_t saturn_alias_word_t __attribute__((__may_alias__));
#endif
static inline void* saturn_copy_bytes(void* destination,const void* source,size_t count) {
    unsigned char* d=(unsigned char*)destination;
    const unsigned char* s=(const unsigned char*)source;
#if defined(__GNUC__)
    if((((uintptr_t)d^(uintptr_t)s)&3u)==0u) {
        while(count && ((uintptr_t)d&3u)){*d++=*s++;--count;}
        while(count>=16u) {
            saturn_alias_word_t* dw=(saturn_alias_word_t*)d;
            const saturn_alias_word_t* sw=(const saturn_alias_word_t*)s;
            dw[0]=sw[0];dw[1]=sw[1];dw[2]=sw[2];dw[3]=sw[3];
            d+=16;s+=16;count-=16;
        }
        while(count>=4u) {
            *(saturn_alias_word_t*)d=*(const saturn_alias_word_t*)s;
            d+=4;s+=4;count-=4;
        }
    }
#endif
    while(count--){*d++=*s++;}
    return destination;
}
static inline void* saturn_fill_bytes(void* destination,int value,size_t count) {
    unsigned char* d=(unsigned char*)destination;
    const unsigned char c=(unsigned char)value;
#if defined(__GNUC__)
    const uint32_t word=(uint32_t)c*0x01010101u;
    while(count && ((uintptr_t)d&3u)){*d++=c;--count;}
    while(count>=16u) {
        saturn_alias_word_t* dw=(saturn_alias_word_t*)d;
        dw[0]=word;dw[1]=word;dw[2]=word;dw[3]=word;
        d+=16;count-=16;
    }
    while(count>=4u){*(saturn_alias_word_t*)d=word;d+=4;count-=4;}
#endif
    while(count--){*d++=c;}
    return destination;
}
#endif
