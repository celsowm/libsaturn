#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "saturn/math2d.h"
#include "src/core/math2d/logic.hpp"

static const double kPi = 3.14159265358979323846;

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

static sat_fx16_t F(int x) { return static_cast<sat_fx16_t>(x * SAT_FX16_ONE); }
static int absi(int v) { return v < 0 ? -v : v; }

static void sine_table_is_exact_at_the_axes() {
    OK(sat_sin8(0) == 0);
    OK(sat_sin8(64) == SAT_FX16_ONE);
    OK(sat_sin8(128) == 0);
    OK(sat_sin8(192) == -SAT_FX16_ONE);
    OK(sat_cos8(0) == SAT_FX16_ONE);
    OK(sat_cos8(64) == 0);
    OK(sat_cos8(128) == -SAT_FX16_ONE);
    OK(sat_cos8(192) == 0);
    for (int a = 0; a < 256; ++a) {
        const double ref = std::sin(a * 2.0 * kPi / 256.0) * 65536.0;
        OK(absi(sat_sin8(static_cast<sat_angle_t>(a)) - static_cast<int>(std::lround(ref))) == 0);
        OK(sat_sin8(static_cast<sat_angle_t>(a)) == -sat_sin8(static_cast<sat_angle_t>(256 - a)));
    }
}

static void sixteen_bit_angles_interpolate() {
    int worst = 0;
    for (uint32_t a = 0; a < 65536; a += 37) {
        const double ref = std::sin(a * 2.0 * kPi / 65536.0) * 65536.0;
        const int err = absi(sat_sin16(static_cast<sat_angle16_t>(a)) - static_cast<int>(std::lround(ref)));
        if (err > worst) worst = err;
        const double cref = std::cos(a * 2.0 * kPi / 65536.0) * 65536.0;
        OK(absi(sat_cos16(static_cast<sat_angle16_t>(a)) - static_cast<int>(std::lround(cref))) < 8);
    }
    OK(worst < 8);
    OK(sat_sin16(0) == 0);
    OK(sat_sin16(16384) == SAT_FX16_ONE);
}

static void atan2_covers_the_full_circle() {
    for (int a = 0; a < 256; ++a) {
        const sat_vec2_t v = sat_vec2_from_angle(static_cast<sat_angle_t>(a));
        const sat_angle_t got = sat_atan2_8(v.y, v.x);
        OK(absi(sat_angle_diff(static_cast<sat_angle_t>(a), got)) == 0);
        const sat_vec2_t big = {v.x * 100, v.y * 100};
        OK(sat_atan2_8(big.y, big.x) == got);
    }
    OK(sat_atan2_8(0, 0) == 0);
    OK(sat_atan2_8(0, F(5)) == 0);
    OK(sat_atan2_8(F(5), 0) == 64);
    OK(sat_atan2_8(0, -F(5)) == 128);
    OK(sat_atan2_8(-F(5), 0) == 192);
    OK(sat_atan2_8(F(3), F(3)) == 32);
    OK(sat_atan2_16(F(3), F(3)) == 8192);
    /* arbitrary non-axis angles agree with libm to within one 16-bit step pair */
    for (int deg = 1; deg < 360; deg += 7) {
        const double r = deg * kPi / 180.0;
        const sat_fx16_t x = static_cast<sat_fx16_t>(std::lround(std::cos(r) * 70000));
        const sat_fx16_t y = static_cast<sat_fx16_t>(std::lround(std::sin(r) * 70000));
        const double ref = deg * 65536.0 / 360.0;
        OK(std::fabs(sat_atan2_16(y, x) - ref) < 16.0 || std::fabs(sat_atan2_16(y, x) - ref) > 65520.0);
    }
}

static void angle_helpers() {
    OK(sat_angle_diff(250, 5) == 11);
    OK(sat_angle_diff(5, 250) == -11);
    OK(sat_angle_diff(0, 128) == -128);
    OK(sat_angle_quadrant(0) == 0);
    OK(sat_angle_quadrant(31) == 0);
    OK(sat_angle_quadrant(32) == 1);
    OK(sat_angle_quadrant(64) == 1);
    OK(sat_angle_quadrant(96) == 2);
    OK(sat_angle_quadrant(128) == 2);
    OK(sat_angle_quadrant(160) == 3);
    OK(sat_angle_quadrant(192) == 3);
    OK(sat_angle_quadrant(224) == 0);
    OK(sat_angle_quadrant(255) == 0);
}

static void vector_helpers() {
    const sat_vec2_t three_four = {F(3), F(4)};
    OK(sat_vec2_length(three_four) == F(5));
    OK(sat_vec2_length({0, 0}) == 0);
    const sat_vec2_t n = sat_vec2_normalize(three_four);
    OK(absi(n.x - 39322) <= 1 && absi(n.y - 52429) <= 1);
    const sat_vec2_t z = sat_vec2_normalize({0, 0});
    OK(z.x == 0 && z.y == 0);
    /* huge vectors must not overflow */
    const sat_vec2_t big = sat_vec2_normalize({0x7FFFFFFF, 0});
    OK(big.x == SAT_FX16_ONE && big.y == 0);

    OK(sat_vec2_dot({F(2), F(3)}, {F(4), F(5)}) == F(23));
    const sat_vec2_t p = sat_vec2_perp({F(1), 0});
    OK(p.x == 0 && p.y == F(1));

    const sat_vec2_t unit = {SAT_FX16_ONE, 0};
    const sat_vec2_t v = {F(7), F(-2)};
    const sat_vec2_t along = sat_vec2_project(v, unit);
    const sat_vec2_t across = sat_vec2_reject(v, unit);
    OK(along.x == F(7) && along.y == 0);
    OK(across.x == 0 && across.y == F(-2));

    OK(sat_lerp_fx16(F(10), F(20), SAT_FX16_ONE / 2) == F(15));
    OK(sat_lerp_fx16(F(10), F(20), 0) == F(10));
    OK(sat_lerp_fx16(F(10), F(20), SAT_FX16_ONE) == F(20));
    const sat_vec2_t l = sat_vec2_lerp({0, 0}, {F(4), F(-8)}, SAT_FX16_ONE / 4);
    OK(l.x == F(1) && l.y == F(-2));
}

int main() {
    sine_table_is_exact_at_the_axes();
    sixteen_bit_angles_interpolate();
    atan2_covers_the_full_circle();
    angle_helpers();
    vector_helpers();
    std::puts("PASS: test_math2d.cpp");
    return 0;
}
