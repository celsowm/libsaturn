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

static sat_fade3d_slots_t skybridge_slots() {
    /* Skybridge's own configuration: eight levels onto the eight slots, with
     * everything nearer than `start` drawn as an ordinary opaque sprite. */
    sat_fade3d_slots_t cfg = {};
    cfg.policy = (sat_fade3d_t){FX(0), FX(80), 8u, SAT_FADE3D_CULL_AFTER_END, 0u};
    cfg.hysteresis = 0;
    cfg.base_slot = 0u;
    cfg.slot_stride = 1u;
    cfg.opaque_before_start = 1u;
    return cfg;
}

TEST(slot_rejects_unusable_mapping) {
    sat_fade3d_slots_t cfg = skybridge_slots();
    uint8_t slot = 0u;
    ASSERT_EQ(saturn::core::fade3d::slot(nullptr, FX(10), nullptr, &slot),
              SAT_ERR_INVALID_ARG);
    ASSERT_EQ(saturn::core::fade3d::slot(&cfg, FX(10), nullptr, nullptr),
              SAT_ERR_INVALID_ARG);
    cfg.slot_stride = 0u;
    ASSERT_EQ(saturn::core::fade3d::slot(&cfg, FX(10), nullptr, &slot),
              SAT_ERR_INVALID_ARG);
    /* Eight levels at stride 2 would need sixteen slots; the hardware has
     * eight, so this is a configuration error and not a silent clamp. */
    cfg.slot_stride = 2u;
    ASSERT_EQ(saturn::core::fade3d::slot(&cfg, FX(10), nullptr, &slot),
              SAT_ERR_INVALID_ARG);
    cfg.slot_stride = 1u;
    cfg.base_slot = 1u;
    ASSERT_EQ(saturn::core::fade3d::slot(&cfg, FX(10), nullptr, &slot),
              SAT_ERR_INVALID_ARG);
}

TEST(stateless_slot_matches_eval_plus_mapping) {
    sat_fade3d_slots_t cfg = skybridge_slots();
    sat_fade3d_result_t out = {};
    uint8_t slot = 0u;

    ASSERT_EQ(saturn::core::fade3d::slot(&cfg, FX(-1), nullptr, &slot), SAT_OK);
    ASSERT_EQ(slot, SAT_INDEXED_SOLID_OPAQUE);
    ASSERT_EQ(saturn::core::fade3d::slot(&cfg, FX(0), nullptr, &slot), SAT_OK);
    ASSERT_EQ(slot, SAT_INDEXED_SOLID_OPAQUE);
    ASSERT_EQ(saturn::core::fade3d::slot(&cfg, FX(80), nullptr, &slot), SAT_OK);
    ASSERT_EQ(slot, SAT_FADE3D_SLOT_CULLED);

    for (int d = 1; d < 80; ++d) {
        ASSERT_EQ(saturn::core::fade3d::eval(&cfg.policy, FX(d), &out), SAT_OK);
        ASSERT_EQ(saturn::core::fade3d::slot(&cfg, FX(d), nullptr, &slot), SAT_OK);
        ASSERT_EQ(slot, out.level);
    }
}

TEST(stride_spreads_fewer_levels_over_the_slot_table) {
    /* distance_fade_3d's four-level mode: levels 0..3 onto slots 0,2,4,6. */
    sat_fade3d_slots_t cfg = skybridge_slots();
    cfg.policy.levels = 4u;
    cfg.slot_stride = 2u;
    uint8_t slot = 0u;
    ASSERT_EQ(saturn::core::fade3d::slot(&cfg, FX(10), nullptr, &slot), SAT_OK);
    ASSERT_EQ(slot, 0u);
    ASSERT_EQ(saturn::core::fade3d::slot(&cfg, FX(30), nullptr, &slot), SAT_OK);
    ASSERT_EQ(slot, 2u);
    ASSERT_EQ(saturn::core::fade3d::slot(&cfg, FX(50), nullptr, &slot), SAT_OK);
    ASSERT_EQ(slot, 4u);
    ASSERT_EQ(saturn::core::fade3d::slot(&cfg, FX(70), nullptr, &slot), SAT_OK);
    ASSERT_EQ(slot, 6u);
}

/* The regression this exists to prevent: a chase camera easing back and forth
 * across one transition must not toggle an object's slot every frame. */
TEST(hysteresis_suppresses_boundary_oscillation) {
    sat_fade3d_slots_t cfg = skybridge_slots();
    cfg.hysteresis = FX(2);
    /* Level 3 spans [30,40); sweep across that edge inside the band. */
    uint8_t state = SAT_INDEXED_SOLID_OPAQUE;
    uint8_t slot = 0u;
    ASSERT_EQ(saturn::core::fade3d::slot(&cfg, FX(35), &state, &slot), SAT_OK);
    ASSERT_EQ(slot, 3u);
    ASSERT_EQ(state, 3u);

    uint8_t changes = 0u;
    uint8_t previous = slot;
    for (int pass = 0; pass < 4; ++pass) {
        for (int d = 39; d <= 41; ++d) {
            ASSERT_EQ(saturn::core::fade3d::slot(&cfg, FX(d), &state, &slot), SAT_OK);
            if (slot != previous) { ++changes; previous = slot; }
        }
        for (int d = 41; d >= 39; --d) {
            ASSERT_EQ(saturn::core::fade3d::slot(&cfg, FX(d), &state, &slot), SAT_OK);
            if (slot != previous) { ++changes; previous = slot; }
        }
    }
    ASSERT_EQ(changes, 0u); /* Without the band this alternates every step. */

    /* Travelling a full band past the transition does commit the new slot. */
    ASSERT_EQ(saturn::core::fade3d::slot(&cfg, FX(43), &state, &slot), SAT_OK);
    ASSERT_EQ(slot, 4u);
    ASSERT_EQ(state, 4u);
}

TEST(hysteresis_holds_the_opaque_and_culled_edges) {
    sat_fade3d_slots_t cfg = skybridge_slots();
    cfg.hysteresis = FX(2);
    uint8_t state = SAT_INDEXED_SOLID_OPAQUE;
    uint8_t slot = 0u;
    /* Just past `start`, an already-opaque object stays opaque. */
    ASSERT_EQ(saturn::core::fade3d::slot(&cfg, FX(1), &state, &slot), SAT_OK);
    ASSERT_EQ(slot, SAT_INDEXED_SOLID_OPAQUE);
    ASSERT_EQ(saturn::core::fade3d::slot(&cfg, FX(5), &state, &slot), SAT_OK);
    ASSERT_EQ(slot, 0u);

    state = SAT_FADE3D_SLOT_CULLED;
    ASSERT_EQ(saturn::core::fade3d::slot(&cfg, FX(79), &state, &slot), SAT_OK);
    ASSERT_EQ(slot, SAT_FADE3D_SLOT_CULLED);
    ASSERT_EQ(saturn::core::fade3d::slot(&cfg, FX(70), &state, &slot), SAT_OK);
    ASSERT_EQ(slot, 7u);
}

int main() {
    rejects_invalid_config();
    quantizes_eight_levels();
    can_hold_last_level_after_end();
    slot_rejects_unusable_mapping();
    stateless_slot_matches_eval_plus_mapping();
    stride_spreads_fewer_levels_over_the_slot_table();
    hysteresis_suppresses_boundary_oscillation();
    hysteresis_holds_the_opaque_and_culled_edges();
    std::puts("test_fade3d_logic: OK");
    return 0;
}
