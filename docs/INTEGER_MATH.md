# Integer arithmetic helpers

`saturn/int_math.h` is a standalone C11/C++ header. It has no Saturn hardware,
initialization or linker dependency and can also be used by host tools.

```c
#include <saturn/int_math.h>

int32_t q12_product = (int32_t)a * b; /* a and b are int16_t */
int32_t rounded_down = sat_floor_shift_i32(q12_product, 12);
int64_t wide_result = sat_floor_shift_i64(wide_accumulator, 12);
```

Both helpers divide by a power of two and round toward negative infinity:
`sat_floor_shift_i32(-4097, 12)` is `-2`. They preserve the signed input width.
They do not saturate, narrow or truncate toward zero. Counts equal to or larger
than the input width return `-1` for negative values and `0` otherwise; even
`UINT_MAX` is defined. The zero-count case preserves signed minima exactly.

Use the 32-bit helper only after proving the product and every partial sum fit.
One signed 16-bit product fits; three arbitrary such products need more than
32 bits. Casting an overflowing sum afterward does not repair it. Keep a wide
accumulator for unconstrained matrix dot products.

GCC/Clang arithmetic shifts are used when the compiler defines them. Other
compilers use an unsigned-magnitude fallback that handles signed minima
without negating them. No division unit is touched. Constant shifts let the
SH-2 compiler avoid general variable-shift helpers; choosing i32 also avoids
multiword shift assembly. Neither property alone establishes a frame-time gain:
measure the complete caller and its instruction-cache footprint.

The host gate compares both widths with an independent division/remainder
oracle across signed extrema, negative fractions, random values, counts 0–80
and `UINT_MAX`. Package installation includes this header automatically.
