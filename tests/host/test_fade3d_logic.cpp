#include <cstdio>
#include <cstdlib>

#include "src/core/fade3d_logic.hpp"

#define TEST(name) static void name()
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    std::fprintf(stderr, "FAIL %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
    std::exit(1); } } while (0)
#define FX(v) ((sat_fx16_t)((v) * 65536))

TEST(rejects_invalid_config) {
    sat_fade3d_result_t out = {};
    sat_fade3d_t fade = {FX(10), FX(10), 8u, SAT_FADE3D_CULL_AFTER_END, 0u};
    ASSERT_EQ(saturn::core::fade3d::eval(&fade, FX(10), &out), SAT_ERR_INVALID_ARG);
    fade.end = FX(20);
    fade.levels = 0u;
    ASSERT_EQ(saturn::core::fade3d::eval(&fade, FX(10), &out), SAT_ERR_INVALID_ARG);
    fade.levels = 8u;
    fade.flags = 0x80u;
    ASSERT_EQ(saturn::core::fade3d::eval(&fade, FX(10), &out), SAT_ERR_INVALID_ARG);
}

TEST(quantizes_eight_levels) {
    sat_fade3d_t fade = {FX(0), FX(100), 8u, SAT_FADE3D_CULL_AFTER_END, 0u};
    sat_fade3d_result_t out = {};

    ASSERT_EQ(saturn::core::fade3d::eval(&fade, FX(-5), &out), SAT_OK);
    ASSERT_EQ(out.level, 0u);
    ASSERT_EQ(out.culled, 0u);

    ASSERT_EQ(saturn::core::fade3d::eval(&fade, FX(13), &out), SAT_OK);
    ASSERT_EQ(out.level, 1u);

    ASSERT_EQ(saturn::core::fade3d::eval(&fade, FX(50), &out), SAT_OK);
    ASSERT_EQ(out.level, 4u);

    ASSERT_EQ(saturn::core::fade3d::eval(&fade, FX(99), &out), SAT_OK);
    ASSERT_EQ(out.level, 7u);
    ASSERT_EQ(out.culled, 0u);

    ASSERT_EQ(saturn::core::fade3d::eval(&fade, FX(100), &out), SAT_OK);
    ASSERT_EQ(out.level, 7u);
    ASSERT_EQ(out.culled, 1u);
}

TEST(can_hold_last_level_after_end) {
    sat_fade3d_t fade = {FX(20), FX(40), 4u, 0u, 0u};
    sat_fade3d_result_t out = {};
    ASSERT_EQ(saturn::core::fade3d::eval(&fade, FX(100), &out), SAT_OK);
    ASSERT_EQ(out.level, 3u);
    ASSERT_EQ(out.culled, 0u);
}

int main() {
    rejects_invalid_config();
    quantizes_eight_levels();
    can_hold_last_level_after_end();
    std::puts("test_fade3d_logic: OK");
    return 0;
}
