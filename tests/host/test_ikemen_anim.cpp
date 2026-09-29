#include <cstdio>
#include <cstdlib>

#include "examples/ikemen_saturn/ikemen_anim.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

static const ik_clsn_box_t k_boxes[] = {
    {16, -80, 61, -71},
    {-13, -93, 16, 0},
};

static const ik_frame_t k_frames[] = {
    {20u, 0u, 30u, 60u, 15, 58, 4u, 0u, 0u,   0u, 0u, 1u, 1u},
    {20u, 1u, 31u, 61u, 15, 59, 2u, IK_FRAME_FLAG_FLIP_H, 100u, 0u, 0u, 1u, 1u},
    {20u, 2u, 29u, 59u, 15, 57, 8u, 0u, 200u, 0u, 0u, 1u, 1u},
    {200u, 0u, 40u, 60u, 20, 58, 5u, 0u, 300u, 0u, 1u, 1u, 1u},
    {200u, 1u, 45u, 62u, 22, 60, 0u, IK_FRAME_FLAG_FLIP_V, 400u, 0u, 0u, 1u, 1u},
};

int main() {
    const ik_frame_table_t table = {
        k_frames,
        sizeof(k_frames) / sizeof(k_frames[0]),
        k_boxes,
        sizeof(k_boxes) / sizeof(k_boxes[0])
    };

    uint32_t first = 99u, count = 0u;
    OK(ik_frames_bounds(&table, 20, &first, &count));
    OK(first == 0u && count == 3u);
    OK(ik_frames_bounds(&table, 200, &first, &count));
    OK(first == 3u && count == 2u);
    OK(!ik_frames_bounds(&table, 999, &first, &count));

    OK(ik_frame_at_time(&table, 20, 0u)->index == 0u);
    OK(ik_frame_at_time(&table, 20, 4u)->index == 1u);
    OK(ik_frame_at_time(&table, 20, 6u)->index == 2u);
    OK(ik_frame_at_time(&table, 20, 14u)->index == 0u);
    OK(ik_action_duration_ticks(&table, 20) == 14u);

    OK(ik_frame_at_time(&table, 200, 5u)->index == 1u);
    OK(ik_frame_at_time(&table, 200, 5000u)->index == 1u);
    OK(ik_action_duration_ticks(&table, 200) == 0u);

    int16_t dx = 0, dy = 0;
    ik_frame_screen_anchor(&k_frames[0], 100, 200, 1, &dx, &dy);
    OK(dx == 85 && dy == 142);
    ik_frame_screen_anchor(&k_frames[0], 100, 200, -1, &dx, &dy);
    OK(dx == 85 && dy == 142);

    int l = 0, t = 0, r = 0, b = 0;
    OK(ik_frame_clsn_count(&k_frames[3], IK_CLSN_ATTACK) == 1u);
    OK(ik_frame_clsn_world(&table, &k_frames[3], IK_CLSN_ATTACK, 0u,
                           100, 180, 1, &l, &t, &r, &b));
    OK(l == 116 && r == 161 && t == 100 && b == 109);
    OK(ik_frame_clsn_world(&table, &k_frames[3], IK_CLSN_ATTACK, 0u,
                           100, 180, -1, &l, &t, &r, &b));
    OK(l == 39 && r == 84 && t == 100 && b == 109);

    std::puts("[test] ikemen_anim OK");
    return 0;
}
