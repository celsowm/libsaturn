#include <cstdio>
#include <cstdlib>

#include "examples/ikemen_saturn/ikemen_cns.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

static const ik_cns_hitdef_t k_hitdefs[] = {
    {200, IK_CNS_TRIGGER_ANIM_ELEM_EQ, 3,
     23, 0, 3u, 8u, 8u,
     IK_CNS_GROUND_HIGH, 5u, 11u, 15u,
     -4 * IK_CNS_Q8_ONE, 0,
     -358, -3 * IK_CNS_Q8_ONE,
     0, -10, -76, 5, 0, 6, 0, 0u},
    {410, IK_CNS_TRIGGER_ANIM_ELEM_EQ, 3,
     37, 0, 4u, 12u, 12u,
     IK_CNS_GROUND_LOW, 12u, 17u, 17u,
     -4 * IK_CNS_Q8_ONE, 0,
     -3 * IK_CNS_Q8_ONE, -4 * IK_CNS_Q8_ONE,
     1, -10, -55, 5, 2, 6, 0, 0u},
    {410, IK_CNS_TRIGGER_ANIM_ELEM_EQ, 4,
     36, 0, 4u, 12u, 12u,
     IK_CNS_GROUND_HIGH, 12u, 17u, 17u,
     -7 * IK_CNS_Q8_ONE, 0,
     -3 * IK_CNS_Q8_ONE, -4 * IK_CNS_Q8_ONE,
     -1, -10, -83, 5, 2, 6, 0, 0u},
};

static const ik_cns_state_t k_states[] = {
    {200, 200, 10, 0, 0,
     IK_CNS_STATE_STAND, IK_CNS_MOVE_ATTACK, IK_CNS_PHYS_STAND,
     0, 2, 1u, 0u, 1u, 0u, 0u},
    {410, 410, 25, 0, 0,
     IK_CNS_STATE_CROUCH, IK_CNS_MOVE_ATTACK, IK_CNS_PHYS_CROUCH,
     0, 2, 0u, 1u, 2u, 0u, 0u},
};

static const ik_cns_asset_t k_asset = {
    {1000, 15, 16, 12, 12, 60,
     614, -563, 1178, 0, -1152, -973,
     0, -2150, -653, 640,
     113, 218, 210, 512, 13},
    k_states, 2u,
    k_hitdefs, 3u,
    nullptr, 0u
};

int main() {
    OK(ik_cns_find_state(&k_asset, 200) == &k_states[0]);
    OK(ik_cns_find_state(&k_asset, 999) == nullptr);

    OK(ik_cns_active_hitdef(&k_asset, 200, 2u, 2u) == nullptr);
    OK(ik_cns_active_hitdef(&k_asset, 200, 2u, 3u) == &k_hitdefs[0]);

    OK(ik_cns_active_hitdef(&k_asset, 410, 3u, 3u) == &k_hitdefs[1]);
    OK(ik_cns_active_hitdef(&k_asset, 410, 4u, 4u) == &k_hitdefs[2]);

    OK(ik_cns_trigger_fires(IK_CNS_TRIGGER_TIME_EQ, 2, 1u, 1u) == 0);
    OK(ik_cns_trigger_fires(IK_CNS_TRIGGER_TIME_EQ, 2, 2u, 1u) == 1);
    OK(ik_cns_q8_to_int(-5 * IK_CNS_Q8_ONE - 128) == -5);

    std::puts("[test] ikemen_cns OK");
    return 0;
}
