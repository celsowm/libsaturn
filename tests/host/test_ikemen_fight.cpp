/* test_ikemen_fight.cpp — host tests for the shared Ikemen subset sim. */
#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "examples/common/ikemen_fight.h"

#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    fprintf(stderr, "FAIL %d: %s (%ld) != %s (%ld)\n", __LINE__, \
            #a, (long)(a), #b, (long)(b)); exit(1); } } while(0)
#define ASSERT_TRUE(c) do { if (!(c)) { \
    fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c); exit(1); } } while(0)

static sat_pad_state_t held(uint16_t b) { sat_pad_state_t p = {}; p.held = b; return p; }
static sat_pad_state_t pressed(uint16_t b) { sat_pad_state_t p = {}; p.held = b; p.pressed = b; return p; }

int main(void) {
    ik_fight_t g;
    ik_fight_init(&g);
    ASSERT_EQ(g.fighters[0].hp, IK_MAX_HP);
    ASSERT_EQ(g.fighters[0].x, 110);
    ASSERT_EQ(g.timer_frames, IK_ROUND_TIME_FRAMES);

    /* Walk right moves P1. */
    {
        int16_t x0 = g.fighters[0].x;
        sat_pad_state_t p = held(SAT_PAD_RIGHT);
        ik_fight_update(&g, &p, NULL);
        ASSERT_TRUE(g.fighters[0].x > x0);
        ASSERT_EQ(g.fighters[0].state, IK_STATE_WALK);
    }
    /* Jump leaves ground and gravity returns. */
    {
        ik_fight_init(&g);
        sat_pad_state_t p = pressed(SAT_PAD_UP);
        ik_fight_update(&g, &p, NULL);
        ASSERT_TRUE(!g.fighters[0].on_ground);
        sat_pad_state_t idle = {};
        for (int i = 0; i < 40; i++) ik_fight_update(&g, &idle, NULL);
        ASSERT_TRUE(g.fighters[0].on_ground);
        ASSERT_EQ(g.fighters[0].y, IK_FLOOR_Y);
    }
    /* Punch in range damages dummy. */
    {
        ik_fight_init(&g);
        g.fighters[0].x = 150; g.fighters[1].x = 170;
        g.frame = 10; /* avoid guard window */
        int hp0 = g.fighters[1].hp;
        sat_pad_state_t p = pressed(SAT_PAD_A);
        ik_fight_update(&g, &p, NULL);
        /* swing needs a few frames to become active */
        sat_pad_state_t idle = {};
        for (int i = 0; i < 6; i++) ik_fight_update(&g, &idle, NULL);
        ASSERT_TRUE(g.fighters[1].hp < hp0);
        ASSERT_TRUE((g.events & IK_EVENT_HIT) != 0 || g.hits_p1 == 1);
    }
    /* Touching edges are not a hit (strict overlap). */
    ASSERT_TRUE(!ik_boxes_overlap(0, 0, 10, 10, 10, 0, 20, 10));
    ASSERT_TRUE(ik_boxes_overlap(0, 0, 10, 10, 9, 0, 20, 10));

    /* KO ends round. */
    {
        ik_fight_init(&g);
        g.fighters[1].hp = 10;
        g.fighters[0].x = 150; g.fighters[1].x = 170;
        g.frame = 10;
        sat_pad_state_t p = pressed(SAT_PAD_B);
        ik_fight_update(&g, &p, NULL);
        sat_pad_state_t idle = {};
        for (int i = 0; i < 8; i++) ik_fight_update(&g, &idle, NULL);
        ASSERT_TRUE(g.round_over);
        ASSERT_EQ(g.winner, 1);
        ASSERT_TRUE(ik_fight_status_needs_start(&g));
    }
    /* START resets. */
    {
        sat_pad_state_t p = pressed(SAT_PAD_START);
        ik_fight_update(&g, &p, NULL);
        ASSERT_TRUE(!g.round_over);
        ASSERT_EQ(g.fighters[0].hp, IK_MAX_HP);
    }
    printf("[test] ikemen_fight OK\n");
    return 0;
}
