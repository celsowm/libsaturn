/* test_f2ms.cpp — host-side unit test for the inline frame-to-
 * millisecond helpers in include/saturn/time.h. The header-only form
 * keeps the test free of any libsrc linkage. */

#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "saturn/time.h"

#define ASSERT_EQ(a, b) do { auto __ae = (a); auto __be = (b); \
    if (__ae != __be) { \
    fprintf(stderr, "FAIL %s:%d: %s=%lld != %s=%lld\n", __FILE__, __LINE__, \
        #a, (long long)__ae, #b, (long long)__be); \
    exit(1); } } while (0)

static void test_ntsc_one_second(void) {
    /* 60 frames at 60 Hz is exactly 1000 ms. */
    ASSERT_EQ(sat_f2ms(60u, 60u), 1000u);
    /* Round-trip: 1000 ms at 60 Hz back to 60 frames. */
    ASSERT_EQ(sat_ms2f(1000u, 60u), 60u);
}

static void test_pal_one_second(void) {
    /* 50 frames at 50 Hz is exactly 1000 ms. */
    ASSERT_EQ(sat_f2ms(50u, 50u), 1000u);
    ASSERT_EQ(sat_ms2f(1000u, 50u), 50u);
}

static void test_half_second_ntsc(void) {
    /* 30 frames at 60 Hz = 500 ms. */
    ASSERT_EQ(sat_f2ms(30u, 60u), 500u);
    ASSERT_EQ(sat_ms2f(500u, 60u), 30u);
}

static void test_ntsc_pal_diverge_in_one_second(void) {
    /* The same 60 frames covers 1.0 s on NTSC and 1.2 s on PAL --
     * the reason we cannot hard-code 60Hz conversions. */
    ASSERT_EQ(sat_f2ms(60u, 60u), 1000u);
    ASSERT_EQ(sat_f2ms(60u, 50u), 1200u);
    ASSERT_EQ(sat_f2ms(60u, 60u) != sat_f2ms(60u, 50u), true);
}

static void test_zero_hz_returns_zero(void) {
    /* A divide-by-zero would crash the SH-2; the helper returns 0. */
    ASSERT_EQ(sat_f2ms(60u, 0u), 0u);
}

int main() {
    test_ntsc_one_second();
    test_pal_one_second();
    test_half_second_ntsc();
    test_ntsc_pal_diverge_in_one_second();
    test_zero_hz_returns_zero();
    printf("PASS: test_f2ms.cpp (5 tests)\n");
    return 0;
}
