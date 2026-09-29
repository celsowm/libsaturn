#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "examples/ikemen_saturn/ikemen_fight.h"

#define ASSERT_EQ(a, b) do { if ((a) != (b)) {     std::fprintf(stderr, "FAIL %d: %s (%ld) != %s (%ld)\n", __LINE__,                  #a, (long)(a), #b, (long)(b)); std::exit(1); } } while (0)
#define ASSERT_TRUE(c) do { if (!(c)) {     std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c); std::exit(1); } } while (0)

/* Compact copy of the KFM AIR geometry relevant to standing light punch/kick.
 * The important regression is reach: punch Clsn1 extends to +61 from the
 * player axis, not the old synthetic +30 rectangle. */
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

    {230u, 0u, 32u, 96u, 16, 93, 2u, 0u, 0u, 0u, 0u, 6u, 1u},
    {230u, 1u, 32u, 96u, 16, 93, 2u, 0u, 0u, 0u, 0u, 6u, 1u},
    {230u, 2u, 72u, 96u, 16, 93, 3u, 0u, 0u, 3u, 3u, 6u, 1u},
    {230u, 3u, 72u, 96u, 16, 93, 3u, 0u, 0u, 0u, 0u, 6u, 1u},
    {230u, 4u, 40u, 96u, 16, 93, 3u, 0u, 0u, 0u, 0u, 6u, 1u},
    {230u, 5u, 32u, 96u, 16, 93, 2u, 0u, 0u, 0u, 0u, 6u, 1u},
};

static const ik_frame_table_t k_table = {
    k_frames,
    sizeof(k_frames) / sizeof(k_frames[0]),
    k_boxes,
    sizeof(k_boxes) / sizeof(k_boxes[0])
};

static void tick(ik_fight_t* g, const ik_fight_controls_t* p1) {
    ik_fight_update(g, p1, nullptr, &k_table);
}

int main() {
    ik_fight_t g;
    ik_fight_init(&g);
    ASSERT_EQ(g.fighters[0].hp, IK_MAX_HP);
    ASSERT_EQ(IK_STATE_KICK, 230);
    ASSERT_EQ(IK_PUNCH_DAMAGE, 23);
    ASSERT_EQ(IK_KICK_DAMAGE, 26);
    ASSERT_EQ(ik_action_duration_ticks(&k_table, 200), 12u);
    ASSERT_EQ(ik_action_duration_ticks(&k_table, 230), 15u);

    {
        const int16_t x0 = g.fighters[0].x;
        ik_fight_controls_t p = {};
        p.forward = 1u;
        tick(&g, &p);
        ASSERT_TRUE(g.fighters[0].x > x0);
    }

    /* Exact KFM punch reach: origins 50 px apart still connect because the
     * real Clsn1 reaches +61 and the idle Clsn2 begins at -13. */
    {
        ik_fight_init(&g);
        g.fighters[0].x = 100;
        g.fighters[1].x = 150;
        const int hp0 = g.fighters[1].hp;

        ik_fight_controls_t attack = {};
        attack.x = 1u;
        tick(&g, &attack);
        ik_fight_controls_t idle = {};
        tick(&g, &idle); /* Time 1 */
        tick(&g, &idle); /* Time 2 */
        tick(&g, &idle); /* Time 3 -> AIR element 3 / Clsn1 */

        ASSERT_EQ(hp0 - g.fighters[1].hp, IK_PUNCH_DAMAGE);
        ASSERT_EQ(g.hits_p1, 1u);
        ASSERT_EQ(g.fighters[0].hit_pause, IK_PUNCH_HITPAUSE);
        ASSERT_EQ(g.fighters[1].hitstun, IK_PUNCH_HITSTUN);
    }

    /* Outside the real +61 Clsn1 there is no hit. */
    {
        ik_fight_init(&g);
        g.fighters[0].x = 100;
        g.fighters[1].x = 180;
        const int hp0 = g.fighters[1].hp;
        ik_fight_controls_t attack = {};
        attack.x = 1u;
        tick(&g, &attack);
        ik_fight_controls_t idle = {};
        for (int i = 0; i < 12; ++i) tick(&g, &idle);
        ASSERT_EQ(g.fighters[1].hp, hp0);
    }

    /* Kick uses its three real AIR Clsn1 boxes and exact CNS damage. */
    {
        ik_fight_init(&g);
        g.fighters[0].x = 100;
        g.fighters[1].x = 150;
        const int hp0 = g.fighters[1].hp;
        ik_fight_controls_t attack = {};
        attack.a = 1u;
        tick(&g, &attack);
        ik_fight_controls_t idle = {};
        for (int i = 0; i < 4; ++i) tick(&g, &idle);
        ASSERT_EQ(hp0 - g.fighters[1].hp, IK_KICK_DAMAGE);
        ASSERT_EQ(g.hits_p1, 1u);
    }

    ASSERT_TRUE(!ik_boxes_overlap(0, 0, 10, 10, 10, 0, 20, 10));
    ASSERT_TRUE(ik_boxes_overlap(0, 0, 10, 10, 9, 0, 20, 10));

    {
        ik_fight_init(&g);
        g.fighters[1].hp = 20;
        g.fighters[0].x = 100;
        g.fighters[1].x = 150;
        ik_fight_controls_t attack = {};
        attack.a = 1u;
        tick(&g, &attack);
        ik_fight_controls_t idle = {};
        for (int i = 0; i < 4; ++i) tick(&g, &idle);
        ASSERT_TRUE(g.round_over);
        ASSERT_EQ(g.winner, 1);
    }

    {
        ik_fight_controls_t reset = {};
        reset.start = 1u;
        tick(&g, &reset);
        ASSERT_TRUE(!g.round_over);
        ASSERT_EQ(g.fighters[0].hp, IK_MAX_HP);
    }

    std::puts("[test] ikemen_fight OK");
    return 0;
}
