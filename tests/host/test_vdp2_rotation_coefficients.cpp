/* test_vdp2_rotation_coefficients.cpp - host tests for the
 * vdp2_rotation_coefficients scene math
 * (include/saturn/vdp2_rotation_coefficients.h).
 *
 * The reference values are recomputed here with double-precision libm -
 * a deliberately different derivation than the header's fixed-point one -
 * and the two must agree within the header's documented rounding. A
 * failure here means the shipped ocean floor / swell / wave tables are
 * wrong, not that a hand-copied model drifted. */

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "saturn/vdp2_rotation_coefficients.h"
#include "src/hal/vdp2/rotation_logic.hpp"

#define ONE 65536
static const double kPi = 3.14159265358979323846;

static sat_vdp2_rotation_coefficients_config_t demo_config(void) {
    sat_vdp2_rotation_coefficients_config_t cfg = {};
    cfg.horizon = 96u;
    cfg.cx = 160u;
    cfg.focal = 96u;
    cfg.min_depth = 8u;
    cfg.ground_forward = 96u;
    cfg.swell_amplitude = 2 * ONE;
    cfg.swell_wavelength = 64u;
    cfg.swell_period = 128u;
    cfg.wave_amplitude = 12 * ONE;
    cfg.wave_wavelength = 32u;
    cfg.wave_period = 64u;
    return cfg;
}

/* Tolerance in 16.16 units for comparing the fixed-point header against
 * the double reference: the sine series stays under 0.05% of full scale
 * (33), rounding adds 1, products truncate by at most 1. */
static const int32_t kTolerance = 40;

static void floor_k_matches_the_reference(void) {
    const sat_vdp2_rotation_coefficients_config_t cfg = demo_config();
    for (uint32_t y = 0u; y <= 223u; ++y) {
        const int32_t k = sat_vdp2_rotation_coefficients_floor_k(&cfg, y);
        if (y <= cfg.horizon) {
            assert(k == 0);
            continue;
        }
        const double d = (double)(y - cfg.horizon) + (double)cfg.min_depth;
        const double reference = std::nearbyint((double)cfg.focal / d * ONE);
        assert(std::abs((double)k - reference) <= 1.0);
        assert(k < 0x007FFFFF);   /* inside the 2-word coefficient range */
    }
    /* the near-horizon row is the most magnified one and still representable */
    const double near_ref = std::nearbyint((double)cfg.focal
                                           / (1.0 + (double)cfg.min_depth) * ONE);
    assert(std::abs((double)sat_vdp2_rotation_coefficients_floor_k(&cfg, cfg.horizon + 1u)
                    - near_ref) <= 1.0);
}

static void sine_matches_libm(void) {
    for (uint32_t t = 0u; t < 0x10000u; t += 0x400u) {
        const int32_t s = sat_vdp2_rotation_coefficients_sine(t);
        const double reference = std::sin(2.0 * kPi * (double)t / 65536.0) * ONE;
        assert(std::abs((double)s - reference) <= (double)kTolerance);
    }
    /* the symmetries of one full cycle */
    assert(sat_vdp2_rotation_coefficients_sine(0u) == 0);
    const int32_t quarter = sat_vdp2_rotation_coefficients_sine(0x4000u);
    assert(std::abs(quarter - ONE) <= kTolerance);
    assert(sat_vdp2_rotation_coefficients_sine(0x8000u) <= 0);
    assert(std::abs(sat_vdp2_rotation_coefficients_sine(0xC000u) + quarter) <= kTolerance);
    assert(sat_vdp2_rotation_coefficients_sine(0x1234u)
           == sat_vdp2_rotation_coefficients_sine(0x1234u + 0x10000u));
}

static void swell_and_wave_match_the_reference(void) {
    const sat_vdp2_rotation_coefficients_config_t cfg = demo_config();
    for (uint32_t y = cfg.horizon + 1u; y < 224u; y += 9u) {
        for (uint32_t f = 0u; f <= 200u; f += 17u) {
            const double phase = (double)y / (double)cfg.swell_wavelength
                              - (double)f / (double)cfg.swell_period;
            const double k_ref = (double)sat_vdp2_rotation_coefficients_floor_k(&cfg, y);
            double swell_ref = k_ref
                + (double)cfg.swell_amplitude * std::sin(2.0 * kPi * phase);
            if (swell_ref < 0.0) swell_ref = 0.0;
            /* the sine's 16.16 error scales with the amplitude */
            const double swell_tol = (double)kTolerance
                * (1.0 + std::abs((double)cfg.swell_amplitude) / ONE);
            assert(std::abs((double)sat_vdp2_rotation_coefficients_swell(&cfg, y, f)
                            - swell_ref) <= swell_tol);

            const double wave_phase = (double)y / (double)cfg.wave_wavelength
                                    - (double)f / (double)cfg.wave_period;
            const double wave_ref = (double)cfg.wave_amplitude
                                  * std::sin(2.0 * kPi * wave_phase);
            const double wave_tol = (double)kTolerance
                * (1.0 + std::abs((double)cfg.wave_amplitude) / ONE);
            assert(std::abs((double)sat_vdp2_rotation_coefficients_wave(&cfg, y, f)
                            - wave_ref) <= wave_tol);
        }
    }
    /* one period of frames reproduces the same table: the animation loops */
    assert(sat_vdp2_rotation_coefficients_swell(&cfg, 112u, 30u)
           == sat_vdp2_rotation_coefficients_swell(&cfg, 112u, 30u + cfg.swell_period));
    assert(sat_vdp2_rotation_coefficients_wave(&cfg, 112u, 30u)
           == sat_vdp2_rotation_coefficients_wave(&cfg, 112u, 30u + cfg.wave_period));
}

static void parameter_sets_and_xp_base(void) {
    const sat_vdp2_rotation_coefficients_config_t cfg = demo_config();
    sat_vdp2_rotation_params_t a;
    sat_vdp2_rotation_coefficients_build_params_a(&cfg, 0x24200u, &a);
    assert(a.matrix[0] == ONE && a.matrix[4] == ONE);
    assert(a.matrix[1] == 0 && a.matrix[3] == 0);
    assert(a.px == 0 && a.py == (int16_t)cfg.horizon);
    assert(a.yst == (int32_t)(cfg.horizon + cfg.ground_forward) * ONE);
    assert(a.dx == ONE && a.kx == ONE && a.ky == ONE);
    assert(a.coefficient_address == 0x24200u);
    assert(a.coefficient_step == ONE);

    const int32_t cos30 = 56756;   /* round(cos(30 degrees) * 65536) */
    const int32_t sin30 = 32768;   /* sin(30 degrees) exactly */
    assert(std::abs((double)cos30 / ONE - std::cos(30.0 * kPi / 180.0)) * ONE <= 1.0);
    sat_vdp2_rotation_params_t b;
    sat_vdp2_rotation_coefficients_build_params_b(&cfg, cos30, sin30, 0x24600u, &b);
    assert(b.matrix[0] == cos30 && b.matrix[4] == cos30);
    assert(b.matrix[1] == -sin30 && b.matrix[3] == sin30);
    assert(b.yst == a.yst && b.px == a.px && b.py == a.py);
    assert(b.coefficient_address == 0x24600u);

    /* The flat set's rotated viewpoint coordinate is exactly the camera
     * offset; the yawed set's is (1 - cos) * cx + Mx for the same
     * quantized cos - both inside the coefficient's +-128 range
     * (p06_40 figure 6.7). */
    const int32_t mx = 40 * ONE;
    assert(sat_vdp2_rotation_coefficients_xp_base(ONE, cfg.cx, mx) == mx);
    const int32_t base_b = sat_vdp2_rotation_coefficients_xp_base(cos30, cfg.cx, mx);
    const double reference = (double)(ONE - cos30) * (double)cfg.cx + (double)mx;
    assert(std::abs((double)base_b - reference) <= 2.0);
    assert(base_b > 0 && base_b < 128 * ONE);

    /* A negative wave displacement encodes as sign + magnitude (figure
     * 6.7): -12.0 is the sign bit and integer 12, not a two's-complement
     * shift. */
    const uint32_t encoded = saturn::hal::vdp2::rot::encode_coefficient(-12 * ONE, false);
    assert(encoded == 0x008C0000u);
    const uint32_t flagged = saturn::hal::vdp2::rot::encode_coefficient(12 * ONE, true);
    assert(flagged == 0x800C0000u);
}

int main(void) {
    floor_k_matches_the_reference();
    sine_matches_libm();
    swell_and_wave_match_the_reference();
    parameter_sets_and_xp_base();
    std::puts("test_vdp2_rotation_coefficients: ok");
    return 0;
}
