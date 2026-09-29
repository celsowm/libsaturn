#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "examples/ikemen_saturn/ikemen_fight.h"

#define ASSERT_EQ(a, b) do { if ((a) != (b)) {     std::fprintf(stderr, "FAIL %d: %s (%ld) != %s (%ld)\n", __LINE__,                  #a, (long)(a), #b, (long)(b)); std::exit(1); } } while (0)
#define ASSERT_TRUE(c) do { if (!(c)) {     std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c); std::exit(1); } } while (0)

static const ik_clsn_box_t k_boxes[] = {
    {-13, -93, 16, 0},       /* 0 idle hurt */
    {16, -80, 61, -71},      /* 1 punch attack */
    {-10, -94, 19, 0},       /* 2 punch hurt */
    {14, -53, 38, -36},      /* 3 kick attack */
    {35, -42, 56, -29},      /* 4 kick attack */
    {53, -34, 69, -23},      /* 5 kick attack */
    {-10, -99, 19, 0},       /* 6 kick hurt */
};

static const ik_frame_t k_frames[] = {
    {0u,   0u, 32u, 96u, 16, 93, 0u, 0u, 0u, 0u, 0u, 0u, 1u},

    {200u, 0u, 32u, 96u, 16, 93, 2u, 0u, 0u, 0u, 0u, 2u, 1u},
    {200u, 1u, 32u, 96u, 16, 93, 1u, 0u, 0u, 0u, 0u, 2u, 1u},
    {200u, 2u, 64u, 96u, 16, 93, 4u, 0u, 0u, 1u, 1u, 2u, 1u},
    {200u, 3u, 32u, 96u, 16, 93, 3u, 0u, 0u, 0u, 0u, 2u, 1u},
    {200u, 4u, 32u, 96u, 16, 93, 2u, 0u, 0u, 0u, 0u, 2u, 1u},

    {210u, 0u, 32u, 96u, 16, 93, 2u, 0u, 0u, 0u, 0u, 2u, 1u},
    {210u, 1u, 32u, 96u, 16, 93, 1u, 0u, 0u, 0u, 0u, 2u, 1u},
    {210u, 2u, 64u, 96u, 16, 93, 5u, 0u, 0u, 1u, 1u, 2u, 1u},
    {210u, 3u, 32u, 96u, 16, 93, 5u, 0u, 0u, 0u, 0u, 2u, 1u},

    {230u, 0u, 32u, 96u, 16, 93, 2u, 0u, 0u, 0u, 0u, 6u, 1u},
    {230u, 1u, 32u, 96u, 16, 93, 2u, 0u, 0u, 0u, 0u, 6u, 1u},
    {230u, 2u, 72u, 96u, 16, 93, 3u, 0u, 0u, 3u, 3u, 6u, 1u},
    {230u, 3u, 72u, 96u, 16, 93, 3u, 0u, 0u, 0u, 0u, 6u, 1u},
    {230u, 4u, 40u, 96u, 16, 93, 3u, 0u, 0u, 0u, 0u, 6u, 1u},
    {230u, 5u, 32u, 96u, 16, 93, 2u, 0u, 0u, 0u, 0u, 6u, 1u},

    {240u, 0u, 32u, 96u, 16, 93, 2u, 0u, 0u, 0u, 0u, 6u, 1u},
    {240u, 1u, 32u, 96u, 16, 93, 2u, 0u, 0u, 0u, 0u, 6u, 1u},
    {240u, 2u, 72u, 96u, 16, 93, 5u, 0u, 0u, 3u, 3u, 6u, 1u},
    {240u, 3u, 32u, 96u, 16, 93, 5u, 0u, 0u, 0u, 0u, 6u, 1u},
};

static const ik_frame_table_t k_table = {
    k_frames,
    sizeof(k_frames) / sizeof(k_frames[0]),
    k_boxes,
    sizeof(k_boxes) / sizeof(k_boxes[0])
};

static const ik_cns_hitdef_t k_hitdefs[] = {
    {200, IK_CNS_TRIGGER_ANIM_ELEM_EQ, 3,
     23, 0, 3u, 8u, 8u,
     IK_CNS_GROUND_HIGH, 5u, 11u, 15u,
     -4 * IK_CNS_Q8_ONE, 0, -358, -3 * IK_CNS_Q8_ONE,
     0, -10, -76, 5, 0, 6, 0, 0u},
    {210, IK_CNS_TRIGGER_ANIM_ELEM_EQ, 3,
     57, 0, 4u, 12u, 12u,
     IK_CNS_GROUND_HIGH, 12u, 16u, 16u,
     -1408, 0, -640, -4 * IK_CNS_Q8_ONE,
     1, -10, -70, 5, 2, 6, 0, IK_CNS_HITDEF_FORCE_NO_FALL},
    {230, IK_CNS_TRIGGER_TIME_EQ, 0,
     26, 0, 4u, 12u, 12u,
     IK_CNS_GROUND_LOW, 10u, 14u, 14u,
     -5 * IK_CNS_Q8_ONE, 0, -640, -896,
     0, -10, -37, 5, 1, 6, 0, 0u},
    {240, IK_CNS_TRIGGER_TIME_EQ, 0,
     63, 0, 4u, 12u, 12u,
     IK_CNS_GROUND_LOW, 12u, 17u, 17u,
     -6 * IK_CNS_Q8_ONE, 0, -563, -819,
     1, -10, -60, 5, 2, 6, 0, 0u},
};

static const ik_cns_state_t k_states[] = {
    {200, 200, 10, 0, 0, IK_CNS_STATE_STAND, IK_CNS_MOVE_ATTACK,
     IK_CNS_PHYS_STAND, 0, 2, 1u, 0u, 1u, 0u, 0u},
    {210, 210, 30, 0, 0, IK_CNS_STATE_STAND, IK_CNS_MOVE_ATTACK,
     IK_CNS_PHYS_STAND, 0, -1, 1u, 1u, 1u, 0u, 0u},
    {230, 230, 11, 0, 0, IK_CNS_STATE_STAND, IK_CNS_MOVE_ATTACK,
     IK_CNS_PHYS_STAND, 0, 2, 1u, 2u, 1u, 0u, 0u},
    {240, 240, 30, 0, 0, IK_CNS_STATE_STAND, IK_CNS_MOVE_ATTACK,
     IK_CNS_PHYS_STAND, 0, 2, 1u, 3u, 1u, 0u, 0u},
};

static const ik_cns_asset_t k_cns = {
    {1000, 15, 16, 12, 12, 60,
     614, -563, 1178, 0, -1152, -973,
     0, -2150, -653, 640,
     113, 218, 210, 512, 13},
    k_states, 4u,
    k_hitdefs, 4u,
    nullptr, 0u
};

static void tick(ik_fight_t* g, const ik_fight_controls_t* p1) {
    ik_fight_update(g, p1, nullptr, &k_table);
}

static void idle_ticks(ik_fight_t* g, int count) {
    ik_fight_controls_t idle = {};
    for (int i = 0; i < count; ++i) tick(g, &idle);
}

int main() {
    ik_fight_t g;
    ik_fight_init(&g, &k_cns);
    ASSERT_EQ(g.fighters[0].hp, 1000);
    ASSERT_EQ(ik_fight_max_hp(&g), 1000);
    ASSERT_EQ(ik_action_for_state(&k_cns, IK_STATE_STRONG_PUNCH), 210);
    ASSERT_EQ(ik_action_duration_ticks(&k_table, 200), 12u);
    ASSERT_EQ(ik_action_duration_ticks(&k_table, 230), 15u);

    {
        const int16_t x0 = g.fighters[0].x;
        ik_fight_controls_t p = {};
        p.forward = 1u;
        tick(&g, &p);
        ASSERT_TRUE(g.fighters[0].x > x0);
        ASSERT_EQ(g.fighters[0].x_q8, 110 * IK_CNS_Q8_ONE + 614);
    }

    /* Real state 200 HitDef only becomes active at AnimElem 3. */
    {
        ik_fight_init(&g, &k_cns);
        g.fighters[0].x = 100;
        g.fighters[0].x_q8 = 100 * IK_CNS_Q8_ONE;
        g.fighters[1].x = 150;
        g.fighters[1].x_q8 = 150 * IK_CNS_Q8_ONE;
        const int hp0 = g.fighters[1].hp;

        ik_fight_controls_t attack = {};
        attack.x = 1u;
        tick(&g, &attack);
        idle_ticks(&g, 3);

        ASSERT_EQ(hp0 - g.fighters[1].hp, 23);
        ASSERT_EQ(g.hits_p1, 1u);
        ASSERT_EQ(g.fighters[0].hit_pause, 8u);
        ASSERT_EQ(g.fighters[1].hitstun, 11u);
    }

    /* Outside the real +61 Clsn1 there is no hit. */
    {
        ik_fight_init(&g, &k_cns);
        g.fighters[0].x = 100;
        g.fighters[0].x_q8 = 100 * IK_CNS_Q8_ONE;
        g.fighters[1].x = 180;
        g.fighters[1].x_q8 = 180 * IK_CNS_Q8_ONE;
        const int hp0 = g.fighters[1].hp;
        ik_fight_controls_t attack = {};
        attack.x = 1u;
        tick(&g, &attack);
        idle_ticks(&g, 12);
        ASSERT_EQ(g.fighters[1].hp, hp0);
    }

    /* State 230 damage and pause come from the compiled CNS data. */
    {
        ik_fight_init(&g, &k_cns);
        g.fighters[0].x = 100;
        g.fighters[0].x_q8 = 100 * IK_CNS_Q8_ONE;
        g.fighters[1].x = 150;
        g.fighters[1].x_q8 = 150 * IK_CNS_Q8_ONE;
        const int hp0 = g.fighters[1].hp;
        ik_fight_controls_t attack = {};
        attack.a = 1u;
        tick(&g, &attack);
        idle_ticks(&g, 4);
        ASSERT_EQ(hp0 - g.fighters[1].hp, 26);
        ASSERT_EQ(g.hits_p1, 1u);
    }

    /* Strong punch is state 210 / Y and preserves its fractional -5.5
     * HitDef velocity in Q8.8. */
    {
        ik_fight_init(&g, &k_cns);
        g.fighters[0].x = 100;
        g.fighters[0].x_q8 = 100 * IK_CNS_Q8_ONE;
        g.fighters[1].x = 150;
        g.fighters[1].x_q8 = 150 * IK_CNS_Q8_ONE;
        const int hp0 = g.fighters[1].hp;
        ik_fight_controls_t attack = {};
        attack.y = 1u;
        tick(&g, &attack);
        idle_ticks(&g, 3);
        ASSERT_EQ(hp0 - g.fighters[1].hp, 57);
        ASSERT_EQ(g.fighters[1].vx_q8, 1408);
    }

    /* Strong kick is state 240 / B. */
    {
        ik_fight_init(&g, &k_cns);
        g.fighters[0].x = 100;
        g.fighters[0].x_q8 = 100 * IK_CNS_Q8_ONE;
        g.fighters[1].x = 150;
        g.fighters[1].x_q8 = 150 * IK_CNS_Q8_ONE;
        const int hp0 = g.fighters[1].hp;
        ik_fight_controls_t attack = {};
        attack.b = 1u;
        tick(&g, &attack);
        idle_ticks(&g, 4);
        ASSERT_EQ(hp0 - g.fighters[1].hp, 63);
    }

    ASSERT_TRUE(!ik_boxes_overlap(0, 0, 10, 10, 10, 0, 20, 10));
    ASSERT_TRUE(ik_boxes_overlap(0, 0, 10, 10, 9, 0, 20, 10));

    {
        ik_fight_init(&g, &k_cns);
        g.fighters[1].hp = 20;
        g.fighters[0].x = 100;
        g.fighters[0].x_q8 = 100 * IK_CNS_Q8_ONE;
        g.fighters[1].x = 150;
        g.fighters[1].x_q8 = 150 * IK_CNS_Q8_ONE;
        ik_fight_controls_t attack = {};
        attack.a = 1u;
        tick(&g, &attack);
        idle_ticks(&g, 4);
        ASSERT_TRUE(g.round_over);
        ASSERT_EQ(g.winner, 1);
    }

    {
        ik_fight_controls_t reset = {};
        reset.start = 1u;
        tick(&g, &reset);
        ASSERT_TRUE(!g.round_over);
        ASSERT_EQ(g.fighters[0].hp, 1000);
    }

    std::puts("[test] ikemen_fight OK");
    return 0;
}
