#ifndef SATURN_TYPES_H
#define SATURN_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#define UNCACHED(ptr) ((void*)((uint32_t)(ptr) | 0x20000000))

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

typedef u16      color_t;
typedef u32      fix16_t;

#define FIX16_SHIFT 16
#define FIX16_ONE   (1 << FIX16_SHIFT)
#define FIX16_HALF  (1 << (FIX16_SHIFT - 1))

#define FIX16_TO_FLOAT(x) ((float)(x) / FIX16_ONE)
#define FLOAT_TO_FIX16(x) ((fix16_t)((x) * FIX16_ONE))

#define ALIGN4   __attribute__((aligned(4)))
#define ALIGN8   __attribute__((aligned(8)))
#define ALIGN16  __attribute__((aligned(16)))
#define ALIGN32  __attribute__((aligned(32)))

#define PACKED  __attribute__((packed))
#define SECTION(name) __attribute__((section(name))))
#define NO_INLINE __attribute__((noinline))

#endif
