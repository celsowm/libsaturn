/* LibSaturn VDP2 rotation-coefficients scene mathematics.
 * Header-only and host-testable: the vdp2_rotation_coefficients example and
 * its host test build the same perspective floor, swell and wave tables from
 * these functions, without allocating VRAM or touching VDP2 registers.
 * Reference: docs/sega_saturn_hardware/hard/vdp2/hon/p06_10.md (the display
 * coordinate formula), p06_30.md (rotation parameters), p06_40.md (the
 * coefficient table) and p06_34.md (A/B switching).
 *
 * The floor projection follows sat_vdp2_rbg0_ground.h: the screen dot
 * (H, V) samples the map at
 *   X = k(V) * H + Mx,   Y = k(V) * ground_forward + My
 * with k(V) = focal / ((V - horizon) + min_depth). The viewpoint x is 0
 * (px = 0), which keeps the rotated viewpoint coordinate Xp inside the
 * coefficient's +-128 range: Xp = (1 - A) * cx + Mx, an identity for the
 * flat set A. Set B yaws the same floor by a 2x2 rotation of the matrix.
 */
#ifndef SATURN_VDP2_ROTATION_COEFFICIENTS_H
#define SATURN_VDP2_ROTATION_COEFFICIENTS_H

#include <stdint.h>

#include "saturn/vdp2_layers.h"

#define SAT_VDP2_ROTATION_COEFFICIENTS_ONE 65536  /* 16.16 fixed point */

typedef struct {
    uint32_t horizon;         /* y row of the vanishing point; rows at or
                              * above it are hidden by the caller */
    uint32_t cx;             /* screen x of the vanishing point */
    uint32_t focal;           /* k(y) = focal / depth */
    uint32_t min_depth;       /* avoids the near-horizon singularity */
    uint32_t ground_forward;  /* Yst - horizon; MUST be non-zero, or every
                               * scanline samples the same texture row */
    int32_t swell_amplitude;  /* 16.16, added to k(y) per line */
    uint32_t swell_wavelength; /* screen lines per swell period */
    uint32_t swell_period;    /* frames per swell period */
    int32_t wave_amplitude;   /* 16.16 texels of Xp displacement */
    uint32_t wave_wavelength;
    uint32_t wave_period;
} sat_vdp2_rotation_coefficients_config_t;

/* sin(2 * pi * turns), turns a 16.16 fraction of a full cycle, result 16.16.
 * A minimax-free Taylor series on the reduced first quadrant; the error
 * stays under 0.05% of full scale for every input. */
static inline int32_t sat_vdp2_rotation_coefficients_sine(uint32_t turns) {
    /* One quadrant is 0x4000 of the 16-bit cycle fraction; pi/2 is
     * 102944 in 16.16 radians. */
    const uint32_t quarter = (turns >> 14) & 3u;
    const uint32_t in_q = turns & 0x3FFFu;
    int32_t a = (int32_t)((in_q * 102944u) >> 14);      /* 0 .. pi/2 */
    if ((quarter & 1u) != 0u) a = 102944 - a;            /* mirror odd quadrants */
    /* sin(a) = a - a^3/6 + a^5/120 - a^7/5040, all in 16.16 */
    const int64_t a2 = ((int64_t)a * a) >> 16;
    const int64_t a3 = (a2 * a) >> 16;
    const int64_t a5 = (a3 * a2) >> 16;
    const int64_t a7 = (a5 * a2) >> 16;
    int64_t v = (int64_t)a - ((a3 * 10923) >> 16) + ((a5 * 546) >> 16)
              - ((a7 * 13) >> 16);
    if ((quarter & 2u) != 0u) v = -v;
    return (int32_t)v;
}

/* k(y): the texture step per screen pixel at row y (16.16), rounded to
 * nearest. Rows at or above the horizon are the caller's to hide; the
 * value there is 0. Saturates at just under 128 (the 2-word coefficient
 * range). */
static inline int32_t sat_vdp2_rotation_coefficients_floor_k(
    const sat_vdp2_rotation_coefficients_config_t* cfg, uint32_t y) {
    if (y <= cfg->horizon) return 0;
    const uint32_t d = (y - cfg->horizon) + cfg->min_depth;
    /* round(focal / d) in 16.16: half of d added to the numerator, both
     * scaled by 2^16 so the division stays exact in 64 bits */
    const uint32_t k = (uint32_t)((((uint64_t)cfg->focal << 32) + ((uint64_t)d << 15))
                                / ((uint64_t)d << 16));
    if (k > 0x007FFFFFu) return 0x007FFFFF;
    return (int32_t)k;
}

/* The per-line coefficient value of the swell state: k(y) plus a sine that
 * travels toward the horizon, troughs clamped at 0 so no line mirrors the
 * floor. */
static inline int32_t sat_vdp2_rotation_coefficients_swell(
    const sat_vdp2_rotation_coefficients_config_t* cfg, uint32_t y, uint32_t frame) {
    const int32_t k = sat_vdp2_rotation_coefficients_floor_k(cfg, y);
    if (y <= cfg->horizon) return 0;
    const uint32_t phase = ((y << 16) / cfg->swell_wavelength)
                         - ((frame << 16) / cfg->swell_period);
    const int32_t ripple = (int32_t)(((int64_t)sat_vdp2_rotation_coefficients_sine(phase)
                                      * cfg->swell_amplitude) >> 16);
    const int32_t v = k + ripple;
    return v < 0 ? 0 : v;
}

/* The per-line Xp displacement of the wave state (16.16): negative values
 * displace the line's sampling origin left, positive right. */
static inline int32_t sat_vdp2_rotation_coefficients_wave(
    const sat_vdp2_rotation_coefficients_config_t* cfg, uint32_t y, uint32_t frame) {
    const uint32_t phase = ((y << 16) / cfg->wave_wavelength)
                         - ((frame << 16) / cfg->wave_period);
    return (int32_t)(((int64_t)sat_vdp2_rotation_coefficients_sine(phase)
                      * cfg->wave_amplitude) >> 16);
}

/* The base the rotated viewpoint coordinate Xp takes for a set whose matrix
 * coefficient A is `matrix_a` (16.16), before the wave displaces it:
 * Xp = A * (px - cx) + cx + Mx with px = 0. The flat set (A = 1) is Mx. */
static inline int32_t sat_vdp2_rotation_coefficients_xp_base(int32_t matrix_a,
                                                             uint32_t cx, int32_t mx) {
    const int32_t one_minus_a = SAT_VDP2_ROTATION_COEFFICIENTS_ONE - matrix_a;
    /* 16.16 times an integer stays 16.16: no rescale */
    return (int32_t)((int64_t)one_minus_a * (int64_t)cx) + mx;
}

/* Parameter set A: the flat floor. The camera lives in Mx/My (parallel
 * translation), so the per-line coefficient never scales the camera, only
 * the per-dot deltas. Yst = horizon + ground_forward keeps
 * Ysp = E * (Yst - Py) = ground_forward non-zero (see sat_vdp2_rbg0_ground.h
 * for the regression this avoids). */
static inline void sat_vdp2_rotation_coefficients_build_params_a(
    const sat_vdp2_rotation_coefficients_config_t* cfg,
    uint32_t coefficient_address, sat_vdp2_rotation_params_t* out) {
    out->xst = 0;
    out->yst = (int32_t)(cfg->horizon + cfg->ground_forward)
             * SAT_VDP2_ROTATION_COEFFICIENTS_ONE;
    out->zst = 0;
    out->dxst = 0;
    out->dyst = 0;
    out->dx = SAT_VDP2_ROTATION_COEFFICIENTS_ONE;
    out->dy = 0;
    out->matrix[0] = SAT_VDP2_ROTATION_COEFFICIENTS_ONE;   /* A = 1 */
    out->matrix[1] = 0;
    out->matrix[2] = 0;
    out->matrix[3] = 0;
    out->matrix[4] = SAT_VDP2_ROTATION_COEFFICIENTS_ONE;   /* E = 1 */
    out->matrix[5] = 0;
    out->px = 0;                                            /* keeps Xp in range */
    out->py = (int16_t)cfg->horizon;
    out->pz = 0;
    out->cx = (int16_t)cfg->cx;
    out->cy = (int16_t)cfg->horizon;
    out->cz = 0;
    out->mx = 0;                                            /* the caller adds the camera */
    out->my = 0;
    out->kx = SAT_VDP2_ROTATION_COEFFICIENTS_ONE;
    out->ky = SAT_VDP2_ROTATION_COEFFICIENTS_ONE;
    out->coefficient_address = coefficient_address;
    out->coefficient_step = SAT_VDP2_ROTATION_COEFFICIENTS_ONE;
}

/* Parameter set B: the same floor yawed by cos/sin (16.16) about the
 * vanishing point. */
static inline void sat_vdp2_rotation_coefficients_build_params_b(
    const sat_vdp2_rotation_coefficients_config_t* cfg,
    int32_t cos16, int32_t sin16,
    uint32_t coefficient_address, sat_vdp2_rotation_params_t* out) {
    sat_vdp2_rotation_coefficients_build_params_a(cfg, coefficient_address, out);
    out->matrix[0] = cos16;    /* A =  cos */
    out->matrix[1] = -sin16;   /* B = -sin */
    out->matrix[3] = sin16;    /* D =  sin */
    out->matrix[4] = cos16;    /* E =  cos */
}

#endif
