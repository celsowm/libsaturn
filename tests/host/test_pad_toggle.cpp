/* test_pad_toggle.cpp — host-side unit test for the inline
 * sat_pad_toggle helper. The function lives in include/saturn/input.h
 * so this test only needs the header and exercises the four outcome
 * branches (both args present, pad NULL, flag NULL, button not
 * pressed). */

#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "saturn/core.h"
#include "saturn/input.h"

#define ASSERT_EQ(a, b) do { auto __ae = (a); auto __be = (b); \
    if (__ae != __be) { \
    fprintf(stderr, "FAIL %s:%d: %s=%lld != %s=%lld\n", __FILE__, __LINE__, \
        #a, (long long)__ae, #b, (long long)__be); \
    exit(1); } } while (0)

static void test_toggle_flips_on_press(void) {
    sat_pad_state_t pad{};
    uint8_t flag = 0u;
    pad.pressed = SAT_PAD_A;
    sat_pad_toggle(&pad, SAT_PAD_A, &flag);
    ASSERT_EQ(flag, 1u);
    pad.pressed = SAT_PAD_A;
    sat_pad_toggle(&pad, SAT_PAD_A, &flag);
    ASSERT_EQ(flag, 0u);
}

static void test_toggle_ignores_unrelated_button(void) {
    sat_pad_state_t pad{};
    uint8_t flag = 1u;
    pad.pressed = SAT_PAD_B;     /* not what we asked for */
    sat_pad_toggle(&pad, SAT_PAD_A, &flag);
    ASSERT_EQ(flag, 1u);          /* unchanged */
}

static void test_toggle_tolerates_null_pad(void) {
    uint8_t flag = 0u;
    /* If pad is NULL the call is a no-op rather than a crash. */
    sat_pad_toggle(nullptr, SAT_PAD_A, &flag);
    ASSERT_EQ(flag, 0u);
}

static void test_toggle_tolerates_null_flag(void) {
    sat_pad_state_t pad{};
    pad.pressed = SAT_PAD_A;
    /* If flag is NULL the call is a no-op rather than a crash. */
    sat_pad_toggle(&pad, SAT_PAD_A, nullptr);
    /* No way to detect that anything happened, but the test runs to
     * completion without a SIGSEGV -- enough for the contract. */
}

int main() {
    test_toggle_flips_on_press();
    test_toggle_ignores_unrelated_button();
    test_toggle_tolerates_null_pad();
    test_toggle_tolerates_null_flag();
    printf("PASS: test_pad_toggle.cpp (4 tests)\n");
    return 0;
}
