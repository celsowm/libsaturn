#ifndef SATURN_CORE_MATH3D_LOGIC_HPP
#define SATURN_CORE_MATH3D_LOGIC_HPP

/* Pure, host-testable 3D math for libsaturn.
 *
 * Everything here is plain inline C++ with no hardware access, so the host
 * test suite can link it directly (see tests/host/test_math3d_logic.cpp).
 * The public C API in include/saturn/math3d.h is a thin wrapper over these
 * helpers.
 *
 * All values are 16.16 fixed point (sat_fx16_t). Angles are degrees. Matrices
 * are 4x4 row-major: element (row, col) lives at m[row * 4 + col].
 */

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/math3d.h"

namespace saturn::core::math3d {

/* ------------------------------------------------------------------ */
/* Scalar helpers                                                      */
/* ------------------------------------------------------------------ */

inline sat_fx16_t fx_mul(sat_fx16_t a, sat_fx16_t b) {
    return static_cast<sat_fx16_t>(
        (static_cast<int64_t>(a) * static_cast<int64_t>(b)) >> 16);
}

inline sat_fx16_t fx_div(sat_fx16_t a, sat_fx16_t b) {
    if (b == 0) {
        return 0;
    }
    return static_cast<sat_fx16_t>(
        ((static_cast<int64_t>(a) << 16) / static_cast<int64_t>(b)));
}

inline sat_fx16_t fx_from_int(int32_t v) {
    return static_cast<sat_fx16_t>(v << 16);
}

inline int32_t fx_to_int(sat_fx16_t v) {
    return static_cast<int32_t>(v >> 16);
}

inline sat_fx16_t fx_abs(sat_fx16_t v) {
    return (v < 0) ? static_cast<sat_fx16_t>(-v) : v;
}

/* Signed 64/32 division truncating toward zero, for a quotient the caller
 * guarantees fits in 32 bits.
 *
 * On the SH-2 this runs on the on-chip division unit -- a fixed 39-cycle
 * hardware divide -- instead of libgcc's __divdi3, a bit-serial software loop
 * that projection and pose decoding used to call thousands of times a frame.
 * The registers are dereferenced here, never bound to a global, for the
 * static-constructor reason given in src/hal/vdp2.cpp. The host build keeps
 * the plain operator, which has identical semantics. */
inline int32_t div_s64_s32(int64_t num, int32_t den) {
#if defined(__sh__)
    /* SH7604 DIVU: DVSR, DVDNTH, then writing DVDNTL starts the divide and
     * leaves the quotient in DVDNTL. */
    *reinterpret_cast<volatile uint32_t*>(0xFFFFFF00u) = static_cast<uint32_t>(den);
    *reinterpret_cast<volatile uint32_t*>(0xFFFFFF10u) =
        static_cast<uint32_t>(static_cast<uint64_t>(num) >> 32u);
    *reinterpret_cast<volatile uint32_t*>(0xFFFFFF14u) = static_cast<uint32_t>(num);
    return static_cast<int32_t>(*reinterpret_cast<volatile uint32_t*>(0xFFFFFF14u));
#else
    return static_cast<int32_t>(num / den);
#endif
}

/* Exact integer square root, bit by bit. Used where the value whose root is
 * wanted is a sum of squares that only fits in 64 bits. */
inline uint64_t isqrt64(uint64_t x) {
    uint64_t res = 0;
    uint64_t bit = static_cast<uint64_t>(1) << 62;
    while (bit > x) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (x >= res + bit) {
            x -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return res;
}

/* Length of a 16.16 vector, computed without ever forming a 16.16 product.
 *
 * The obvious fx_sqrt(fx_mul(x,x) + fx_mul(y,y) + fx_mul(z,z)) overflows
 * int32 once any component passes about 181.0, because fx_mul reduces by
 * 2^16 but the square itself does not fit first. A camera 260 units above a
 * board is past that, and the result is not a slightly wrong length but a
 * sign-flipped one, which turns the whole view matrix into nonsense.
 *
 * Squaring in int64 keeps the sum at the 2^32 scale, so its integer square
 * root is already the 16.16 answer. */
inline sat_fx16_t fx_len3(sat_fx16_t x, sat_fx16_t y, sat_fx16_t z) {
    const uint64_t sum = (static_cast<int64_t>(x) * x) +
                         (static_cast<int64_t>(y) * y) +
                         (static_cast<int64_t>(z) * z);
    uint64_t root = isqrt64(sum);
    if (root > 0x7FFFFFFFu) {
        root = 0x7FFFFFFFu;
    }
    return static_cast<sat_fx16_t>(root);
}

/* sqrt of a 16.16 value, returned as 16.16 (floor).
 * For v = A * 2^16, sqrt(v << 16) = sqrt(A * 2^32) = sqrt(A) * 2^16 is the
 * 16.16 result directly. It used to take a fixed 32 Newton steps, each a
 * 64-bit software divide, where the digit-by-digit root needs no divide. */
inline sat_fx16_t fx_sqrt(sat_fx16_t v) {
    if (v <= 0) {
        return 0;
    }
    return static_cast<sat_fx16_t>(
        isqrt64(static_cast<uint64_t>(static_cast<uint32_t>(v)) << 16u));
}

/* ------------------------------------------------------------------ */
/* Vector helpers                                                      */
/* ------------------------------------------------------------------ */

inline sat_vec3_t vec3(sat_fx16_t x, sat_fx16_t y, sat_fx16_t z) {
    sat_vec3_t v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

inline sat_vec3_t vec3_add(const sat_vec3_t& a, const sat_vec3_t& b) {
    return vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

inline sat_vec3_t vec3_sub(const sat_vec3_t& a, const sat_vec3_t& b) {
    return vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

inline sat_vec3_t vec3_scale(const sat_vec3_t& a, sat_fx16_t s) {
    return vec3(fx_mul(a.x, s), fx_mul(a.y, s), fx_mul(a.z, s));
}

inline sat_fx16_t vec3_dot(const sat_vec3_t& a, const sat_vec3_t& b) {
    return static_cast<sat_fx16_t>(fx_mul(a.x, b.x) + fx_mul(a.y, b.y) + fx_mul(a.z, b.z));
}

inline sat_vec3_t vec3_cross(const sat_vec3_t& a, const sat_vec3_t& b) {
    return vec3(
        static_cast<sat_fx16_t>(fx_mul(a.y, b.z) - fx_mul(a.z, b.y)),
        static_cast<sat_fx16_t>(fx_mul(a.z, b.x) - fx_mul(a.x, b.z)),
        static_cast<sat_fx16_t>(fx_mul(a.x, b.y) - fx_mul(a.y, b.x)));
}

inline sat_fx16_t vec3_length(const sat_vec3_t& v) {
    return fx_len3(v.x, v.y, v.z);
}

inline sat_vec3_t vec3_normalize(const sat_vec3_t& v) {
    const sat_fx16_t len = vec3_length(v);
    if (len == 0) {
        return vec3(0, 0, 0);
    }
    return vec3(fx_div(v.x, len), fx_div(v.y, len), fx_div(v.z, len));
}

/* Cross product that keeps its direction for edges of any length a scene is
 * likely to contain, WITHOUT normalising.
 *
 * vec3_cross reduces each product by 2^16 as it goes, so crossing two edges a
 * couple of hundred units long overflows int32 and yields a normal pointing
 * somewhere arbitrary -- which shows up as a large polygon being culled or
 * lit backwards while every small one behaves. Keeping the full 64-bit
 * products and scaling them down until they fit fixes that.
 *
 * The length is meaningless; only the direction is. That is deliberate:
 * normalising costs a square root and three 64-bit divides, and the two
 * things face normals are used for -- the sign of a dot product, and a dot
 * divided by the length -- do not need it. */
inline sat_vec3_t vec3_cross_scaled(const sat_vec3_t& a, const sat_vec3_t& b) {
    int64_t cx = (static_cast<int64_t>(a.y) * b.z) - (static_cast<int64_t>(a.z) * b.y);
    int64_t cy = (static_cast<int64_t>(a.z) * b.x) - (static_cast<int64_t>(a.x) * b.z);
    int64_t cz = (static_cast<int64_t>(a.x) * b.y) - (static_cast<int64_t>(a.y) * b.x);
    int64_t m = (cx < 0) ? -cx : cx;
    const int64_t ay = (cy < 0) ? -cy : cy;
    const int64_t az = (cz < 0) ? -cz : cz;
    if (ay > m) { m = ay; }
    if (az > m) { m = az; }
    if (m == 0) {
        return vec3(0, 0, 0);
    }
    /* 2^20 leaves room for the squares fx_len3 takes without overflow. */
    while (m > (static_cast<int64_t>(1) << 20)) {
        cx >>= 1;
        cy >>= 1;
        cz >>= 1;
        m >>= 1;
    }
    return vec3(
        static_cast<sat_fx16_t>(cx),
        static_cast<sat_fx16_t>(cy),
        static_cast<sat_fx16_t>(cz));
}

/* Unit-length cross product. Only call this where the length actually
 * matters; see the note above. */
inline sat_vec3_t vec3_cross_unit(const sat_vec3_t& a, const sat_vec3_t& b) {
    return vec3_normalize(vec3_cross_scaled(a, b));
}

inline int64_t vec3_dot_raw(const sat_vec3_t& a, const sat_vec3_t& b) {
    return (static_cast<int64_t>(a.x) * static_cast<int64_t>(b.x)) +
           (static_cast<int64_t>(a.y) * static_cast<int64_t>(b.y)) +
           (static_cast<int64_t>(a.z) * static_cast<int64_t>(b.z));
}

/* ------------------------------------------------------------------ */
/* Trigonometry (degrees) — Bhaskara I approximation for sine          */
/* ------------------------------------------------------------------ */

inline sat_fx16_t sin_deg_fx(sat_fx16_t degrees) {
    /* Range-reduce to [0, 360) in fixed point. */
    constexpr int64_t k360 = static_cast<int64_t>(360) << 16;
    int64_t d = static_cast<int64_t>(degrees) % k360;
    if (d < 0) {
        d += k360;
    }

    /* Work in [0, 180] using symmetry sin(180 - x) = sin(x). */
    if (d > (180 << 16)) {
        d -= k360;  /* now (-180, 0] */
    }
    int64_t sign = 1;
    if (d < 0) {
        sign = -1;
        d = -d;
    }
    if (d > (90 << 16)) {
        d = (180 << 16) - d;
    }

    /* Bhaskara I: sin(x) ~= 4x(180-x) / (40500 - x(180-x)), x in degrees. */
    const int64_t x = d;                        /* fx16 degrees */
    const int64_t one80 = static_cast<int64_t>(180) << 16;
    const int64_t t = (x * (one80 - x)) >> 16;  /* fx16 */
    const int64_t denom = (static_cast<int64_t>(40500) << 16) - t;
    if (denom == 0) {
        return 0;
    }
    int64_t num = 4 * t;
    int64_t result = (num << 16) / denom;       /* fx16 */
    return static_cast<sat_fx16_t>(sign < 0 ? -result : result);
}

inline sat_fx16_t cos_deg_fx(sat_fx16_t degrees) {
    return sin_deg_fx(static_cast<sat_fx16_t>(degrees + (90 << 16)));
}

inline sat_fx16_t tan_deg_fx(sat_fx16_t degrees) {
    const sat_fx16_t c = cos_deg_fx(degrees);
    if (c == 0) {
        return 0;
    }
    return fx_div(sin_deg_fx(degrees), c);
}

/* ------------------------------------------------------------------ */
/* Matrix helpers                                                      */
/* ------------------------------------------------------------------ */

inline void mat4_identity(sat_fx16_t* m) {
    for (int i = 0; i < 16; ++i) {
        m[i] = 0;
    }
    m[0] = SAT_FX16_ONE;
    m[5] = SAT_FX16_ONE;
    m[10] = SAT_FX16_ONE;
    m[15] = SAT_FX16_ONE;
}

inline void mat4_multiply(sat_fx16_t* out, const sat_fx16_t* a, const sat_fx16_t* b) {
    sat_fx16_t tmp[16];
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            sat_fx16_t acc = 0;
            for (int k = 0; k < 4; ++k) {
                acc = static_cast<sat_fx16_t>(
                    acc + fx_mul(a[row * 4 + k], b[k * 4 + col]));
            }
            tmp[row * 4 + col] = acc;
        }
    }
    for (int i = 0; i < 16; ++i) {
        out[i] = tmp[i];
    }
}

inline void mat4_translate(sat_fx16_t* m, sat_fx16_t tx, sat_fx16_t ty, sat_fx16_t tz) {
    mat4_identity(m);
    m[3] = tx;
    m[7] = ty;
    m[11] = tz;
}

inline void mat4_scale(sat_fx16_t* m, sat_fx16_t sx, sat_fx16_t sy, sat_fx16_t sz) {
    mat4_identity(m);
    m[0] = sx;
    m[5] = sy;
    m[10] = sz;
}

inline void mat4_rotate_x(sat_fx16_t* m, sat_fx16_t degrees) {
    const sat_fx16_t s = sin_deg_fx(degrees);
    const sat_fx16_t c = cos_deg_fx(degrees);
    mat4_identity(m);
    m[5] = c;
    m[6] = static_cast<sat_fx16_t>(-s);
    m[9] = s;
    m[10] = c;
}

inline void mat4_rotate_y(sat_fx16_t* m, sat_fx16_t degrees) {
    const sat_fx16_t s = sin_deg_fx(degrees);
    const sat_fx16_t c = cos_deg_fx(degrees);
    mat4_identity(m);
    m[0] = c;
    m[2] = s;
    m[8] = static_cast<sat_fx16_t>(-s);
    m[10] = c;
}

inline void mat4_rotate_z(sat_fx16_t* m, sat_fx16_t degrees) {
    const sat_fx16_t s = sin_deg_fx(degrees);
    const sat_fx16_t c = cos_deg_fx(degrees);
    mat4_identity(m);
    m[0] = c;
    m[1] = static_cast<sat_fx16_t>(-s);
    m[4] = s;
    m[5] = c;
}

/* Right-handed look-at, matching the standard OpenGL convention:
 *   f = normalize(center - eye), s = normalize(f x up), u = s x f
 * The view matrix maps world space into camera space with the camera looking
 * down -Z. */
inline void mat4_look_at(
    sat_fx16_t* m,
    sat_fx16_t ex, sat_fx16_t ey, sat_fx16_t ez,
    sat_fx16_t cx, sat_fx16_t cy, sat_fx16_t cz,
    sat_fx16_t ux, sat_fx16_t uy, sat_fx16_t uz
) {
    sat_fx16_t fx = cx - ex;
    sat_fx16_t fy = cy - ey;
    sat_fx16_t fz = cz - ez;
    const sat_fx16_t flen = fx_len3(fx, fy, fz);
    if (flen != 0) {
        fx = fx_div(fx, flen);
        fy = fx_div(fy, flen);
        fz = fx_div(fz, flen);
    }

    /* s = f x up */
    sat_fx16_t sx = fx_mul(fy, uz) - fx_mul(fz, uy);
    sat_fx16_t sy = fx_mul(fz, ux) - fx_mul(fx, uz);
    sat_fx16_t sz = fx_mul(fx, uy) - fx_mul(fy, ux);
    const sat_fx16_t slen = fx_len3(sx, sy, sz);
    if (slen != 0) {
        sx = fx_div(sx, slen);
        sy = fx_div(sy, slen);
        sz = fx_div(sz, slen);
    }

    /* u = s x f */
    const sat_fx16_t ux2 = fx_mul(sy, fz) - fx_mul(sz, fy);
    const sat_fx16_t uy2 = fx_mul(sz, fx) - fx_mul(sx, fz);
    const sat_fx16_t uz2 = fx_mul(sx, fy) - fx_mul(sy, fx);

    m[0] = sx;  m[1] = sy;  m[2] = sz;  m[3] = -(fx_mul(sx, ex) + fx_mul(sy, ey) + fx_mul(sz, ez));
    m[4] = ux2; m[5] = uy2; m[6] = uz2; m[7] = -(fx_mul(ux2, ex) + fx_mul(uy2, ey) + fx_mul(uz2, ez));
    m[8] = -fx; m[9] = -fy; m[10] = -fz; m[11] = (fx_mul(fx, ex) + fx_mul(fy, ey) + fx_mul(fz, ez));
    m[12] = 0;  m[13] = 0;  m[14] = 0;  m[15] = SAT_FX16_ONE;
}

/* Perspective projection. fov_y is in degrees, aspect = width/height, near/far
 * in world units. Result maps view space to OpenGL-style clip space. */
inline sat_result_t mat4_perspective(
    sat_fx16_t* m,
    sat_fx16_t fov_y,
    sat_fx16_t aspect,
    sat_fx16_t near_z,
    sat_fx16_t far_z
) {
    if (aspect <= 0 || near_z <= 0 || far_z <= near_z) {
        return SAT_ERR_INVALID_ARG;
    }
    const sat_fx16_t half = static_cast<sat_fx16_t>(fov_y >> 1);
    const sat_fx16_t t = tan_deg_fx(half);
    if (t == 0) {
        return SAT_ERR_INVALID_ARG;
    }
    const sat_fx16_t f = fx_div(SAT_FX16_ONE, t);

    for (int i = 0; i < 16; ++i) {
        m[i] = 0;
    }
    m[0] = fx_div(f, aspect);
    m[5] = f;
    m[10] = fx_div(static_cast<sat_fx16_t>(far_z + near_z), static_cast<sat_fx16_t>(near_z - far_z));
    m[11] = fx_div(static_cast<sat_fx16_t>(2 * fx_mul(far_z, near_z)),
                   static_cast<sat_fx16_t>(near_z - far_z));
    m[14] = static_cast<sat_fx16_t>(-SAT_FX16_ONE);
    m[15] = 0;
    return SAT_OK;
}

inline sat_vec4_t mat4_transform_vec4(const sat_fx16_t* m, sat_fx16_t x, sat_fx16_t y, sat_fx16_t z, sat_fx16_t w) {
    sat_vec4_t out = {};
    out.x = static_cast<sat_fx16_t>(
        fx_mul(m[0], x) + fx_mul(m[1], y) + fx_mul(m[2], z) + fx_mul(m[3], w));
    out.y = static_cast<sat_fx16_t>(
        fx_mul(m[4], x) + fx_mul(m[5], y) + fx_mul(m[6], z) + fx_mul(m[7], w));
    out.z = static_cast<sat_fx16_t>(
        fx_mul(m[8], x) + fx_mul(m[9], y) + fx_mul(m[10], z) + fx_mul(m[11], w));
    out.w = static_cast<sat_fx16_t>(
        fx_mul(m[12], x) + fx_mul(m[13], y) + fx_mul(m[14], z) + fx_mul(m[15], w));
    return out;
}

/* Projects a world point through a view-projection matrix to screen pixels.
 * screen_x/y use (0,0) top-left. Returns SAT_ERR_UNSUPPORTED when the point is
 * behind the camera (w <= 0), which callers must treat as "do not draw". */
inline sat_result_t project_to_screen(
    const sat_fx16_t* view_proj,
    sat_fx16_t x, sat_fx16_t y, sat_fx16_t z,
    int16_t screen_w,
    int16_t screen_h,
    int16_t* out_x,
    int16_t* out_y
) {
    if (out_x == nullptr || out_y == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    const sat_vec4_t clip = mat4_transform_vec4(view_proj, x, y, z, SAT_FX16_ONE);
    if (clip.w <= 0) {
        return SAT_ERR_UNSUPPORTED;
    }
    const sat_fx16_t ndc_x = fx_div(clip.x, clip.w);
    const sat_fx16_t ndc_y = fx_div(clip.y, clip.w);
    const sat_fx16_t half_w = static_cast<sat_fx16_t>(static_cast<int32_t>(screen_w) << 15);
    const sat_fx16_t half_h = static_cast<sat_fx16_t>(static_cast<int32_t>(screen_h) << 15);
    *out_x = static_cast<int16_t>(fx_to_int(fx_mul(ndc_x + SAT_FX16_ONE, half_w)));
    *out_y = static_cast<int16_t>(fx_to_int(fx_mul(SAT_FX16_ONE - ndc_y, half_h)));
    return SAT_OK;
}

}  // namespace saturn::core::math3d

#endif /* SATURN_CORE_MATH3D_LOGIC_HPP */
