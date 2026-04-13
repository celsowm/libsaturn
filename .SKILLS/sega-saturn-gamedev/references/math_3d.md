# 3D Mathematics — Fixed-Point, Vectors and Matrices for SH2

## 1. Fixed-Point 16.16

```cpp
// math/fixed.hpp
#pragma once
#include <cstdint>

using fx16 = int32_t; // 16.16: integer part in bits 31:16, fraction in 15:0
using fx64 = int64_t; // Intermediate for multiplication

constexpr fx16 FX_ONE  = 0x00010000;
constexpr fx16 FX_HALF = 0x00008000;

// Conversions
constexpr fx16 fx_int(int32_t n)    { return n << 16; }
constexpr int32_t fx_toint(fx16 f)  { return f >> 16; }
constexpr fx16 fx_floor(fx16 f)    { return f & 0xFFFF0000; }
constexpr fx16 fx_frac(fx16 f)     { return f & 0x0000FFFF; }

// Multiplication (correct result without overflow for reasonable values)
inline fx16 fx_mul(fx16 a, fx16 b) {
    return static_cast<fx16>((static_cast<fx64>(a) * b) >> 16);
}

// Multiplication using SH2 MAC instruction (better in tight loops)
inline fx16 fx_mul_mac(fx16 a, fx16 b) {
    fx16 result;
    asm volatile (
        "clrmac                \n"
        "mac.l %1, %2          \n"  // MACH:MACL += a * b (signed 32×32)
        "sts   mach, %0        \n"  // integer result in MACH
        "shll16 %0             \n"  // << 16: combine integer + high fraction
        : "=r"(result)
        : "r"(a), "r"(b)
        : "macl", "mach"
    );
    // Actually: for 16.16 × 16.16 = 32.32, the 16.16 result is in
    // MACH[15:0]:MACL[31:16]. Simplified:
    return static_cast<fx16>((static_cast<fx64>(a) * b) >> 16);
}

// Division: use SH2 hardware divider (36 cycles)
// Start: write DVSR, DVDNTH, DVDNT
// Read result 36+ cycles later: DVDNT
namespace _SH2Div {
    volatile int32_t& DVSR   = *reinterpret_cast<volatile int32_t*>(0xFFFFFF00);
    volatile int32_t& DVDNT  = *reinterpret_cast<volatile int32_t*>(0xFFFFFF04);
    volatile int32_t& DVDNTH = *reinterpret_cast<volatile int32_t*>(0xFFFFFF10);
}

inline void fx_div_begin(fx16 num, fx16 den) {
    _SH2Div::DVSR   = den;
    _SH2Div::DVDNTH = num >> 16;  // Scaled numerator integer part
    _SH2Div::DVDNT  = num << 16;  // Fractional part
}
inline fx16 fx_div_end() {
    return static_cast<fx16>(_SH2Div::DVDNT);
}

// Blocking division (simple, less efficient)
inline fx16 fx_div(fx16 a, fx16 b) {
    fx_div_begin(a, b);
    // 36 cycles latency — insert useful work here in production
    asm volatile("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n");
    return fx_div_end();
}
```

## 2. Sine/Cosine Table

Table with 512 entries, mapping angle [0, 512) → sine in fx16.
Cos(θ) = Sin(θ + 128).

```cpp
// Generate the table (run on build host, save as constant array)
// tools/gen_sintable.cpp:
#include <cstdio>
#include <cmath>
int main() {
    printf("const int32_t sin_table[512] = {\n");
    for (int i = 0; i < 512; ++i) {
        double angle = (2.0 * M_PI * i) / 512.0;
        int32_t val = static_cast<int32_t>(sin(angle) * 65536.0);
        printf("    0x%08X, // sin(%3d)\n", val, i);
    }
    printf("};\n");
}
```

```cpp
// Usage at runtime (table in WRAM-H for cache-efficient access)
extern const int32_t sin_table[512]; // defined in sin_table.c (generated)

inline fx16 fx_sin(uint16_t angle) { return sin_table[angle & 511]; }
inline fx16 fx_cos(uint16_t angle) { return sin_table[(angle + 128) & 511]; }
```

## 3. Vec3

```cpp
// math/vec3.hpp
struct Vec3 {
    fx16 x, y, z;

    constexpr Vec3 operator+(Vec3 o) const { return {x+o.x, y+o.y, z+o.z}; }
    constexpr Vec3 operator-(Vec3 o) const { return {x-o.x, y-o.y, z-o.z}; }

    Vec3 operator*(fx16 s) const {
        return { fx_mul(x,s), fx_mul(y,s), fx_mul(z,s) };
    }

    fx16 dot(Vec3 o) const {
        return fx_mul(x,o.x) + fx_mul(y,o.y) + fx_mul(z,o.z);
    }

    Vec3 cross(Vec3 o) const {
        return {
            fx_mul(y,o.z) - fx_mul(z,o.y),
            fx_mul(z,o.x) - fx_mul(x,o.z),
            fx_mul(x,o.y) - fx_mul(y,o.x)
        };
    }

    // Length² (no sqrt — use for comparisons)
    fx16 len_sq() const { return fx_mul(x,x) + fx_mul(y,y) + fx_mul(z,z); }
};
```

## 4. Mat4 (4×4 Fixed-Point)

```cpp
// math/mat4.hpp
struct Mat4 {
    fx16 m[4][4]; // Row-major

    static Mat4 identity() {
        Mat4 r = {};
        r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = FX_ONE;
        return r;
    }

    // Mat4 × Vec3 multiplication (w=1 implicit)
    // Uses SH2 MAC.L instruction for maximum performance
    Vec3 transform(Vec3 v) const {
        Vec3 r;
        // Each component = dot product of row with vector [x,y,z,1]
        // Generic C++ version (compiler should generate MAC.L with -O2):
        for (int row = 0; row < 3; ++row) {
            fx64 acc = 0;
            acc += static_cast<fx64>(m[row][0]) * v.x;
            acc += static_cast<fx64>(m[row][1]) * v.y;
            acc += static_cast<fx64>(m[row][2]) * v.z;
            acc += static_cast<fx64>(m[row][3]) * FX_ONE; // w=1
            (&r.x)[row] = static_cast<fx16>(acc >> 16);
        }
        return r;
    }

    // Mat4 × Mat4
    Mat4 operator*(const Mat4& b) const {
        Mat4 r = {};
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j) {
                fx64 acc = 0;
                for (int k = 0; k < 4; ++k)
                    acc += static_cast<fx64>(m[i][k]) * b.m[k][j];
                r.m[i][j] = static_cast<fx16>(acc >> 16);
            }
        return r;
    }

    // Builders
    static Mat4 translation(fx16 tx, fx16 ty, fx16 tz) {
        Mat4 r = identity();
        r.m[0][3] = tx; r.m[1][3] = ty; r.m[2][3] = tz;
        return r;
    }

    static Mat4 scale(fx16 sx, fx16 sy, fx16 sz) {
        Mat4 r = {};
        r.m[0][0] = sx; r.m[1][1] = sy; r.m[2][2] = sz; r.m[3][3] = FX_ONE;
        return r;
    }

    static Mat4 rotation_y(uint16_t angle) { // angle in [0, 512)
        Mat4 r = identity();
        fx16 c = fx_cos(angle), s = fx_sin(angle);
        r.m[0][0] =  c; r.m[0][2] = s;
        r.m[2][0] = -s; // negative for right-handed coordinates (adjust if needed)
        r.m[2][2] =  c;
        return r;
    }

    static Mat4 rotation_x(uint16_t angle) {
        Mat4 r = identity();
        fx16 c = fx_cos(angle), s = fx_sin(angle);
        r.m[1][1] =  c; r.m[1][2] = -s;
        r.m[2][1] =  s; r.m[2][2] =  c;
        return r;
    }

    static Mat4 look_at(Vec3 eye, Vec3 target, Vec3 up);
};
```

## 5. Mat4 × Vec3 in SH2 Assembly (optimized version)

For batch transforms (without SCU DSP), use `MAC.L` with auto-increment:

```asm
! mat4_transform_batch.s
! Transforms N vertices by a 4×4 matrix (16.16 fixed-point)
! r4 = pointer to Mat4 (64 bytes, row-major)
! r5 = pointer to Vec3 input array
! r6 = pointer to Vec3 output array
! r7 = count (number of vertices)

    .global mat4_transform_batch
    .align 2
mat4_transform_batch:
    ! Save registers
    sts.l   macl, @-r15
    sts.l   mach, @-r15
    mov.l   r8,   @-r15
    mov.l   r9,   @-r15
    mov.l   r14,  @-r15
    mov     r4,   r14      ! r14 = matrix base (invariant)

.loop:
    ! Load vertex: vx, vy, vz in r0, r1, r2; w=FX_ONE in r3
    mov.l   @r5+, r0       ! vx
    mov.l   @r5+, r1       ! vy
    mov.l   @r5+, r2       ! vz
    mov     r14,  r4       ! r4 = start of matrix for this vertex

    ! ── Calculate result.x = row0 · (vx,vy,vz,1) ──────────
    clrmac
    mac.l   @r4+, r0       ! MACH:MACL += M[0][0] × vx (r0 doesn't advance)
    ! But MAC.L uses @Rm+ (auto-increment) — r0 would advance!
    ! To avoid this, preload M[i][j] in temporary registers:

    ! Alternative approach: preload matrix row
    mov.l   @r14,     r8   ! M[0][0]
    mov.l   @(4,r14), r9   ! M[0][1]
    clrmac
    dmuls.l r8, r0         ! P = M[0][0] × vx (signed 32×32)
    sts     macl, r8       ! r8 = P_low
    sts     mach, r9       ! r9 = P_high → result = (r9 << 16) | (r8 >> 16)
    ! ... this approach is verbose. For N vertices, prefer SCU DSP.

    add     #-1, r7
    tst     r7, r7
    bf      .loop

    ! Restore registers
    mov.l   @r15+, r14
    mov.l   @r15+, r9
    mov.l   @r15+, r8
    lds.l   @r15+, mach
    lds.l   @r15+, macl
    rts
    nop
```

> **Recommendation:** For batch transforms, use **SCU DSP** (see `references/scu_dsp.md`) or delegate to Slave SH2 with the C++ version using `fx64` intermediate — GCC with `-O2` generates efficient `DMULS.L` for this pattern.

## 6. Camera / View Matrix

```cpp
Mat4 Mat4::look_at(Vec3 eye, Vec3 target, Vec3 up) {
    Vec3 fwd = { target.x - eye.x, target.y - eye.y, target.z - eye.z };
    // Normalize fwd (no sqrt — use length² as divisor via reciprocal)
    // Simplification: assume fwd normalized (camera with unit distance)
    Vec3 right = fwd.cross(up);
    Vec3 true_up = right.cross(fwd);

    Mat4 r;
    r.m[0][0] = right.x;   r.m[0][1] = right.y;   r.m[0][2] = right.z;
    r.m[0][3] = -right.dot(eye);
    r.m[1][0] = true_up.x; r.m[1][1] = true_up.y; r.m[1][2] = true_up.z;
    r.m[1][3] = -true_up.dot(eye);
    r.m[2][0] =-fwd.x;     r.m[2][1] =-fwd.y;     r.m[2][2] =-fwd.z;
    r.m[2][3] = fwd.dot(eye);
    r.m[3][0] = 0;          r.m[3][1] = 0;          r.m[3][2] = 0;
    r.m[3][3] = FX_ONE;
    return r;
}
```

## 7. Z-Sort (Painter's Algorithm)

```cpp
// Insertion sort — O(n²) but fast for n < 300 and nearly-sorted data frame to frame
struct ZEntry { uint16_t idx; fx16 avg_z; };

void zsort(ZEntry* entries, int n) {
    for (int i = 1; i < n; ++i) {
        ZEntry key = entries[i];
        int j = i - 1;
        // Sort descending (farthest first = Painter's)
        while (j >= 0 && entries[j].avg_z < key.avg_z) {
            entries[j+1] = entries[j];
            --j;
        }
        entries[j+1] = key;
    }
}
```

## 8. Reciprocal for Fast Division

For perspective, instead of dividing each vertex by Z individually, pre-calculate 1/Z:

```cpp
// Reciprocal table: 1/z for z in [near, far] in fixed-point
// Generate offline, store in WRAM-H
// Or use HW divider with pipeline (start div, do other work, read result)

// Pattern: 4-vertex projection pipeline with HW div
void project_quad(Vec3* v, Screen* s, fx16 focal, int cx, int cy) {
    // Start division for X of vertex 0
    fx_div_begin(fx_mul(v[0].x, focal), v[0].z);
    fx16 y0num = fx_mul(v[0].y, focal);
    fx16 sx0 = fx_div_end();          // 36+ cycles later — OK, y0num calculated above

    fx_div_begin(y0num, v[0].z);
    // During these 36 cycles: calculate numerators of next vertices
    fx16 x1num = fx_mul(v[1].x, focal);
    fx16 sy0 = fx_div_end();

    s[0] = { int16_t(fx_toint(sx0) + cx), int16_t(fx_toint(sy0) + cy) };
    // ... repeat for v[1], v[2], v[3] with similar pipeline
}
```
