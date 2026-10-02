/* test_panic_logic.cpp — host tests for the fatal-error screen's pure logic */

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>

#include "saturn/core.h"
#include "src/core/runtime/panic_logic.hpp"

#define TEST(name) static void name()
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    fprintf(stderr, "FAIL %s:%d: %s (%ld) != %s (%ld)\n", __FILE__, __LINE__, \
            #a, (long)(a), #b, (long)(b)); \
    exit(1); } } while(0)
#define ASSERT_STR(a, b) do { if (strcmp((a), (b)) != 0) { \
    fprintf(stderr, "FAIL %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b)); \
    exit(1); } } while(0)

using namespace saturn::core::panic;

TEST(result_names_cover_every_code) {
    ASSERT_STR(result_name(SAT_OK), "OK");
    ASSERT_STR(result_name(SAT_ERR_INVALID_ARG), "INVALID ARG");
    ASSERT_STR(result_name(SAT_ERR_CAPACITY), "CAPACITY");
    ASSERT_STR(result_name(SAT_ERR_BUSY), "BUSY");
    ASSERT_STR(result_name(SAT_ERR_VERSION), "VERSION");
    /* Every defined error must have a real name, not the fallback. */
    for (int code = -1; code >= -14; --code) {
        ASSERT_EQ(strcmp(result_name(static_cast<sat_result_t>(code)), "UNKNOWN") != 0, 1);
    }
    ASSERT_STR(result_name(static_cast<sat_result_t>(-99)), "UNKNOWN");
}

TEST(basename_handles_both_separators) {
    ASSERT_STR(saturn::core::panic::basename("main.c"), "main.c");
    ASSERT_STR(saturn::core::panic::basename("examples/demo/main.c"), "main.c");
    ASSERT_STR(saturn::core::panic::basename("C:\\work\\examples\\x\\main.c"), "main.c");
    ASSERT_STR(saturn::core::panic::basename("a/b\\c.c"), "c.c");
    ASSERT_STR(saturn::core::panic::basename(""), "");
    ASSERT_STR(saturn::core::panic::basename(nullptr), "");
}

TEST(report_lines) {
    char lines[kLineCount][kLineChars + 1u];
    build_report(lines, SAT_ERR_BUSY, "examples/demo/main.c", 528);
    ASSERT_STR(lines[0], "FATAL ERROR");
    ASSERT_STR(lines[1], "BUSY");
    ASSERT_STR(lines[2], "MAIN.C");
    ASSERT_STR(lines[3], "LINE 528  CODE -7");
}

TEST(report_truncates_long_file_names) {
    char lines[kLineCount][kLineChars + 1u];
    const char* huge =
        "a_file_name_that_is_much_longer_than_the_screen_can_hold_in_one_row.c";
    build_report(lines, SAT_ERR_IO, huge, 1);
    ASSERT_EQ(strlen(lines[2]), kLineChars);
    ASSERT_EQ(lines[2][0], 'A');
}

TEST(report_handles_extreme_numbers) {
    char lines[kLineCount][kLineChars + 1u];
    build_report(lines, static_cast<sat_result_t>(INT32_MIN), "x.c", INT32_MAX);
    ASSERT_STR(lines[3], "LINE 2147483647  CODE -2147483648");
    build_report(lines, SAT_OK, nullptr, 0);
    ASSERT_STR(lines[2], "");
    ASSERT_STR(lines[3], "LINE 0  CODE 0");
}

struct Rects {
    int count = 0;
    long area = 0;
    int max_x = 0;
    int max_y = 0;
};

static Rects measure(const char* text, int scale) {
    Rects r;
    for_each_run(text, 0, 0, scale, [&r](int x, int y, int w, int h) {
        ++r.count;
        r.area += static_cast<long>(w) * h;
        if (x + w > r.max_x) r.max_x = x + w;
        if (y + h > r.max_y) r.max_y = y + h;
    });
    return r;
}

TEST(font_run_merging_and_geometry) {
    /* '0' is 7,5,5,5,7: two full rows (1 run each) + three rows of two
     * single pixels (2 runs each) = 8 runs, 3*2 + 3*... pixels = 12. */
    Rects zero = measure("0", 1);
    ASSERT_EQ(zero.count, 8);
    ASSERT_EQ(zero.area, 12);
    ASSERT_EQ(zero.max_x, 3);
    ASSERT_EQ(zero.max_y, 5);

    /* 'I' is 7,2,2,2,7: five runs. */
    ASSERT_EQ(measure("I", 1).count, 5);

    /* Scaling multiplies area quadratically and extents linearly. */
    Rects zero2 = measure("0", 2);
    ASSERT_EQ(zero2.count, 8);
    ASSERT_EQ(zero2.area, 12 * 4);
    ASSERT_EQ(zero2.max_x, 6);

    /* Space draws nothing; unknown characters draw a solid 3x5 block. */
    ASSERT_EQ(measure(" ", 1).count, 0);
    Rects unknown = measure("#", 1);
    ASSERT_EQ(unknown.area, 15);
    ASSERT_EQ(unknown.count, 5);
}

TEST(font_case_and_cell_advance) {
    ASSERT_EQ(measure("e", 1).area, measure("E", 1).area);
    /* Second character starts one 4-pixel cell later. */
    Rects two = measure("II", 1);
    ASSERT_EQ(two.max_x, 4 + 3);
}

TEST(every_report_character_is_a_real_glyph) {
    const char* all = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-:. ";
    for (const char* c = all; *c != '\0'; ++c) {
        const uint8_t* rows = glyph_rows(*c);
        const uint8_t* unknown = glyph_rows('#');
        ASSERT_EQ(rows == unknown, 0);
    }
}

TEST(command_budget_for_a_full_report) {
    char lines[kLineCount][kLineChars + 1u];
    build_report(lines, SAT_ERR_NOT_INITIALIZED,
                 "a_rather_long_example_source_file_name.c", 12345);
    int total = 0;
    for (uint8_t i = 0u; i < kLineCount; ++i) total += measure(lines[i], 2).count;
    /* The command table holds 2048; a worst-case report must leave most of it
     * free. */
    ASSERT_EQ(total < 600, 1);
}

int main() {
    result_names_cover_every_code();
    basename_handles_both_separators();
    report_lines();
    report_truncates_long_file_names();
    report_handles_extreme_numbers();
    font_run_merging_and_geometry();
    font_case_and_cell_advance();
    every_report_character_is_a_real_glyph();
    command_budget_for_a_full_report();
    puts("panic_logic: OK");
    return 0;
}
