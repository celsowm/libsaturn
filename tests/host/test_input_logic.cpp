/* test_input_logic.cpp — host tests for input logic helpers */

#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "saturn/input.h"
#include "src/core/logic.hpp"

#define TEST(name) static void name()
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    fprintf(stderr, "FAIL %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
    exit(1); } } while(0)

TEST(pad_state_no_change) {
    auto state = saturn::core::compute_pad_state(0, 0);
    ASSERT_EQ(state.held, 0);
    ASSERT_EQ(state.pressed, 0);
    ASSERT_EQ(state.released, 0);
}

TEST(pad_state_press_a) {
    auto state = saturn::core::compute_pad_state(0, SAT_PAD_A);
    ASSERT_EQ(state.held, SAT_PAD_A);
    ASSERT_EQ(state.pressed, SAT_PAD_A);
    ASSERT_EQ(state.released, 0);
}

TEST(pad_state_hold_a) {
    auto state = saturn::core::compute_pad_state(SAT_PAD_A, SAT_PAD_A);
    ASSERT_EQ(state.held, SAT_PAD_A);
    ASSERT_EQ(state.pressed, 0);
    ASSERT_EQ(state.released, 0);
}

TEST(pad_state_release_a) {
    auto state = saturn::core::compute_pad_state(SAT_PAD_A, 0);
    ASSERT_EQ(state.held, 0);
    ASSERT_EQ(state.pressed, 0);
    ASSERT_EQ(state.released, SAT_PAD_A);
}

TEST(pad_state_multi_button) {
    uint16_t prev = SAT_PAD_A | SAT_PAD_B;
    uint16_t cur = SAT_PAD_A | SAT_PAD_C;
    auto state = saturn::core::compute_pad_state(prev, cur);
    ASSERT_EQ(state.held, SAT_PAD_A | SAT_PAD_C);
    ASSERT_EQ(state.pressed, SAT_PAD_C);
    ASSERT_EQ(state.released, SAT_PAD_B);
}

int main() {
    pad_state_no_change();
    pad_state_press_a();
    pad_state_hold_a();
    pad_state_release_a();
    pad_state_multi_button();

    printf("PASS: test_input_logic.cpp (%d tests)\n", 5);
    return 0;
}
