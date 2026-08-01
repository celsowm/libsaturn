/* rbg0_math.h - Pure Mode-7 ground-plane math for vdp2_rbg0_ground.
 *
 * Header-only, dependency-free reimplementation of the VDP2 rotation formula
 * (docs/sega_saturn_hardware/hard/vdp2/hon/p06_10.md section 6.1, and the
 * rotation parameter table layout in p06_30.md Figure 6.3 / coefficient
 * encoding in p06_40.md Figure 6.7) as this example configures it.
 *
 * Kept separate from main.c and compiled unmodified into BOTH:
 *   - the Saturn ROM (examples/vdp2_rbg0_ground/main.c, sh2eb-elf-gcc, C)
 *   - the host test (tests/host/test_rbg0_ground.cpp, native g++, C++)
 * so a test failure means the shipped rotation table is actually wrong, not
 * that a hand-copied "test version" of the math drifted from the real one.
 * This is what a "Yst == Py" style bug (main.c, PR1) should be caught by.
 */
#ifndef VDP2_RBG0_GROUND_RBG0_MATH_H
#define VDP2_RBG0_GROUND_RBG0_MATH_H

#include <stdint.h>

typedef struct {
    uint32_t bitmap_width;    /* texture wrap period in X (must be power of 2) */
    uint32_t bitmap_height;   /* texture wrap period in Y (must be power of 2) */
    uint32_t cx;               /* CX: screen-space horizontal center / Px / Cx */
    uint32_t horizon;          /* CY: y row of vanishing point / Py / Cy */
    uint32_t focal;            /* k(y) = focal / depth */
    uint32_t min_depth;        /* avoids the near-horizon singularity in k(y) */
    uint32_t ground_forward;   /* Yst - Py; MUST be non-zero, see rbg0_ground_build_params */
    uint32_t coef_base_word;   /* VDP2 VRAM word address of the coefficient table */
} rbg0_ground_config_t;

/* 2-word mode-0 coefficient encoding (p06_40.md Figure 6.7):
 *   word0: bit15 transparent, bit7 sign, bits6..0 integer part
 *   word1: 16-bit fraction
 * y <= cfg->horizon is transparent (sky shows through); below it,
 * k(y) = focal / ((y - horizon) + min_depth), rounded to nearest in 16.16.
 */
static inline void rbg0_ground_encode_coefficient(
    const rbg0_ground_config_t* cfg,
    uint32_t y,
    uint16_t* out_word0,
    uint16_t* out_word1
) {
    if (y <= cfg->horizon) {
        *out_word0 = 0x8000u;
        *out_word1 = 0x0000u;
        return;
    }
    uint32_t d = (y - cfg->horizon) + cfg->min_depth;
    uint32_t k16 = (((uint32_t)cfg->focal << 16) + (d / 2u)) / d;
    *out_word0 = (uint16_t)((k16 >> 16) & 0x007Fu);
    *out_word1 = (uint16_t)(k16 & 0xFFFFu);
}

/* Wraps a camera integer coordinate into [0, modulus) (modulus a power of 2)
 * and returns the parallel-translation offset (Mx or My) relative to center.
 */
static inline int32_t rbg0_ground_wrap_translation(int32_t cam, uint32_t modulus, uint32_t center) {
    int32_t wrapped = (int32_t)((uint32_t)cam & (modulus - 1u));
    return wrapped - (int32_t)center;
}

/* KAst integer word for a 2-word coefficient table starting at coef_base_word
 * (a VDP2 VRAM WORD address). Table 6.3 (p06_40.md): the LSB of the KAst
 * integer part represents 4H of byte address for 2-word coefficient data,
 * i.e. KAst = byte_address / 4.
 */
static inline uint16_t rbg0_ground_kast_word(uint32_t coef_base_word) {
    uint32_t byte_addr = coef_base_word * 2u;
    return (uint16_t)(byte_addr / 4u);
}

/* Sign-extends the low 13 bits of a rotation-table word (the Mx/My/Px/Py/etc.
 * integer field width per p06_30.md Figure 6.2).
 */
static inline int32_t rbg0_ground_sign_extend13(uint16_t word) {
    int32_t v = (int32_t)(word & 0x1FFFu);
    if ((v & 0x1000) != 0) {
        v -= 0x2000;
    }
    return v;
}

/* Fills the 48-word rotation parameter A table (word offsets per p06_30.md
 * Figure 6.3) for a Mode-7 ground plane viewed from (cam_x, cam_y). All
 * words not listed below are left zero.
 *
 * Xst = 0, Yst = horizon + ground_forward => Ysp = Yst - Py = ground_forward
 * (see p06_10.md: Ysp = E*(Yst-Py)). Yst == Py here is the PR1 regression:
 * it collapses Ysp to 0 on every scanline, so tex_y never varies with y.
 */
static inline void rbg0_ground_build_params(
    const rbg0_ground_config_t* cfg,
    int32_t cam_x,
    int32_t cam_y,
    uint16_t out[48]
) {
    int32_t mx = rbg0_ground_wrap_translation(cam_x, cfg->bitmap_width, cfg->cx);
    int32_t my = rbg0_ground_wrap_translation(cam_y, cfg->bitmap_height, cfg->horizon);
    int i;

    for (i = 0; i < 48; i++) {
        out[i] = 0;
    }

    out[0] = 0x0000;                                         /* Xst integer */
    out[2] = (uint16_t)(cfg->horizon + cfg->ground_forward); /* Yst integer */

    out[10] = 0x0001; /* DeltaX = +1 per pixel */
    out[12] = 0x0000; /* DeltaY = 0 */

    out[14] = 0x0001; /* Matrix A = 1 */
    out[22] = 0x0001; /* Matrix E = 1 */

    out[26] = (uint16_t)cfg->cx;      /* Px */
    out[27] = (uint16_t)cfg->horizon; /* Py */

    out[30] = (uint16_t)cfg->cx;      /* Cx */
    out[31] = (uint16_t)cfg->horizon; /* Cy */

    out[34] = (uint16_t)((uint32_t)mx & 0x1FFFu); /* Mx integer */
    out[36] = (uint16_t)((uint32_t)my & 0x1FFFu); /* My integer */

    out[38] = 0x0001; /* kx fallback = 1.0 (unused once coefficients drive k) */
    out[40] = 0x0001; /* ky fallback = 1.0 */

    out[42] = rbg0_ground_kast_word(cfg->coef_base_word); /* KAst integer */
    out[44] = 0x0001; /* DeltaKAst: advance one coefficient row per scanline */
    out[46] = 0x0000; /* DeltaKAx: coefficient is per-line, not per-dot */
}

/* Pure reimplementation of the hardware sample point for screen column
 * screen_x on the scanline whose coefficient words were already computed via
 * rbg0_ground_encode_coefficient(). Reads the general 3x3 matrix (A..F) and
 * viewpoint/center terms straight from params[48] rather than assuming the
 * identity-matrix shortcut this example happens to use, so the test actually
 * exercises the formula rather than a hand-simplified copy of it.
 *
 * Returns 0 (and leaves the outputs untouched) if the coefficient's
 * transparent bit is set, matching the hardware's "line is transparent" rule.
 */
static inline int rbg0_ground_sample_point(
    const uint16_t params[48],
    uint16_t coef_word0,
    uint16_t coef_word1,
    int32_t screen_x,
    int32_t* out_tex_x,
    int32_t* out_tex_y
) {
    int32_t xst, yst, zst, dx, dy;
    int32_t a, b, c, d, e, f;
    int32_t px, py, pz, cx_, cy_, cz_, mx, my;
    int32_t k_int, k_sign;
    int64_t k_fx, xsp, ysp, xp, yp, ddx, ddy, hcnt, x_fx16, y_fx16;

    if ((coef_word0 & 0x8000u) != 0u) {
        return 0;
    }

    xst = (int16_t)params[0];
    yst = (int16_t)params[2];
    zst = (int16_t)params[4];
    dx  = (int16_t)params[10];
    dy  = (int16_t)params[12];
    a = (int16_t)params[14];
    b = (int16_t)params[16];
    c = (int16_t)params[18];
    d = (int16_t)params[20];
    e = (int16_t)params[22];
    f = (int16_t)params[24];
    px = (int16_t)params[26];
    py = (int16_t)params[27];
    pz = (int16_t)params[28];
    cx_ = (int16_t)params[30];
    cy_ = (int16_t)params[31];
    cz_ = (int16_t)params[32];
    mx = rbg0_ground_sign_extend13(params[34]);
    my = rbg0_ground_sign_extend13(params[36]);

    k_int  = coef_word0 & 0x007Fu;
    k_sign = (coef_word0 & 0x0080u) ? -1 : 1;
    k_fx = (int64_t)k_sign * (((int64_t)k_int << 16) + (int64_t)coef_word1);

    xsp = (int64_t)a * (xst - px) + (int64_t)b * (yst - py) + (int64_t)c * (zst - pz);
    ysp = (int64_t)d * (xst - px) + (int64_t)e * (yst - py) + (int64_t)f * (zst - pz);
    xp = (int64_t)a * (px - cx_) + (int64_t)b * (py - cy_) + (int64_t)c * (pz - cz_) + cx_ + mx;
    yp = (int64_t)d * (px - cx_) + (int64_t)e * (py - cy_) + (int64_t)f * (pz - cz_) + cy_ + my;
    ddx = (int64_t)a * dx + (int64_t)b * dy;
    ddy = (int64_t)d * dx + (int64_t)e * dy;

    hcnt = screen_x;

    x_fx16 = k_fx * (xsp + ddx * hcnt) + (xp << 16);
    y_fx16 = k_fx * (ysp + ddy * hcnt) + (yp << 16);

    *out_tex_x = (int32_t)(x_fx16 >> 16);
    *out_tex_y = (int32_t)(y_fx16 >> 16);
    return 1;
}

#endif /* VDP2_RBG0_GROUND_RBG0_MATH_H */
