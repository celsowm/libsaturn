/* test_fmt_logic.cpp — host tests for the decimal formatting helpers */

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>

#include "saturn/fmt.h"
#include "src/core/fmt_logic.hpp"

#define TEST(name) static void name()
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    fprintf(stderr, "FAIL %s:%d: %s (%ld) != %s (%ld)\n", __FILE__, __LINE__, \
            #a, (long)(a), #b, (long)(b)); \
    exit(1); } } while(0)
#define ASSERT_STR(a, b) do { if (strcmp((a), (b)) != 0) { \
    fprintf(stderr, "FAIL %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b)); \
    exit(1); } } while(0)

using namespace saturn::core::fmt;

TEST(u32_basic_values) {
    char buf[SAT_FMT_U32_MAX];
    uint16_t len = 0;

    ASSERT_EQ(write_u32(0u, buf, sizeof(buf), &len), SAT_OK);
    ASSERT_STR(buf, "0");
    ASSERT_EQ(len, 1);

    ASSERT_EQ(write_u32(1250u, buf, sizeof(buf), &len), SAT_OK);
    ASSERT_STR(buf, "1250");
    ASSERT_EQ(len, 4);

    ASSERT_EQ(write_u32(4294967295u, buf, sizeof(buf), &len), SAT_OK);
    ASSERT_STR(buf, "4294967295");
    ASSERT_EQ(len, 10);
}

TEST(u32_exact_capacity_boundary) {
    /* SAT_FMT_U32_MAX must be exactly enough for the widest value. */
    char buf[SAT_FMT_U32_MAX];
    ASSERT_EQ(write_u32(4294967295u, buf, SAT_FMT_U32_MAX, nullptr), SAT_OK);
    /* One byte short must fail rather than truncate. */
    ASSERT_EQ(write_u32(4294967295u, buf, SAT_FMT_U32_MAX - 1u, nullptr),
              SAT_ERR_CAPACITY);
    /* Room for the digits but not the terminator is still a failure. */
    ASSERT_EQ(write_u32(99u, buf, 2u, nullptr), SAT_ERR_CAPACITY);
    ASSERT_EQ(write_u32(99u, buf, 3u, nullptr), SAT_OK);
}

TEST(u32_rejects_bad_arguments) {
    char buf[8];
    ASSERT_EQ(write_u32(1u, nullptr, sizeof(buf), nullptr), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(write_u32(1u, buf, 0u, nullptr), SAT_ERR_INVALID_ARG);
}

TEST(i32_signs) {
    char buf[SAT_FMT_I32_MAX];
    uint16_t len = 0;

    ASSERT_EQ(write_i32(0, buf, sizeof(buf), &len), SAT_OK);
    ASSERT_STR(buf, "0");

    ASSERT_EQ(write_i32(4321, buf, sizeof(buf), &len), SAT_OK);
    ASSERT_STR(buf, "4321");
    ASSERT_EQ(len, 4);

    ASSERT_EQ(write_i32(-4321, buf, sizeof(buf), &len), SAT_OK);
    ASSERT_STR(buf, "-4321");
    ASSERT_EQ(len, 5);
}

/* Negating INT32_MIN in 32 bits is undefined and would wrap to itself. */
TEST(i32_handles_int_min) {
    char buf[SAT_FMT_I32_MAX];
    uint16_t len = 0;
    ASSERT_EQ(write_i32(INT32_MIN, buf, sizeof(buf), &len), SAT_OK);
    ASSERT_STR(buf, "-2147483648");
    ASSERT_EQ(len, 11);
    ASSERT_EQ((uint16_t)(len + 1u), SAT_FMT_I32_MAX);
}

TEST(padded_fixed_width) {
    char buf[16];
    uint16_t len = 0;

    ASSERT_EQ(write_u32_padded(1250u, 6u, buf, sizeof(buf), &len), SAT_OK);
    ASSERT_STR(buf, "001250");
    ASSERT_EQ(len, 6);

    ASSERT_EQ(write_u32_padded(0u, 3u, buf, sizeof(buf), nullptr), SAT_OK);
    ASSERT_STR(buf, "000");
}

/* A fixed-width field must stay fixed-width: an oversized value drops its
 * high digits rather than pushing the rest of the HUD sideways. */
TEST(padded_truncates_oversized_values) {
    char buf[16];
    ASSERT_EQ(write_u32_padded(123456u, 3u, buf, sizeof(buf), nullptr), SAT_OK);
    ASSERT_STR(buf, "456");
}

TEST(padded_rejects_bad_arguments) {
    char buf[16];
    ASSERT_EQ(write_u32_padded(1u, 0u, buf, sizeof(buf), nullptr), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(write_u32_padded(1u, 11u, buf, sizeof(buf), nullptr), SAT_ERR_INVALID_ARG);
    ASSERT_EQ(write_u32_padded(1u, 6u, buf, 6u, nullptr), SAT_ERR_CAPACITY);
}

TEST(label_concatenation) {
    char buf[24];
    uint16_t len = 0;

    ASSERT_EQ(write_label_u32("SCORE ", 1250u, buf, sizeof(buf), &len), SAT_OK);
    ASSERT_STR(buf, "SCORE 1250");
    ASSERT_EQ(len, 10);

    ASSERT_EQ(write_label_u32("", 7u, buf, sizeof(buf), &len), SAT_OK);
    ASSERT_STR(buf, "7");
    ASSERT_EQ(len, 1);
}

TEST(label_respects_capacity) {
    char buf[24];
    /* "SCORE " + "1250" + NUL = 11 bytes. */
    ASSERT_EQ(write_label_u32("SCORE ", 1250u, buf, 11u, nullptr), SAT_OK);
    ASSERT_EQ(write_label_u32("SCORE ", 1250u, buf, 10u, nullptr), SAT_ERR_CAPACITY);
    /* Buffer too small for even the label. */
    ASSERT_EQ(write_label_u32("SCORE ", 0u, buf, 4u, nullptr), SAT_ERR_CAPACITY);
    ASSERT_EQ(write_label_u32(nullptr, 0u, buf, sizeof(buf), nullptr),
              SAT_ERR_INVALID_ARG);
}

int main() {
    u32_basic_values();
    u32_exact_capacity_boundary();
    u32_rejects_bad_arguments();
    i32_signs();
    i32_handles_int_min();
    padded_fixed_width();
    padded_truncates_oversized_values();
    padded_rejects_bad_arguments();
    label_concatenation();
    label_respects_capacity();

    printf("PASS: test_fmt_logic.cpp (%d tests)\n", 10);
    return 0;
}
