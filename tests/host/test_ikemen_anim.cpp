#include <cstdio>
#include <cstdlib>

#include "examples/common/ikemen_anim.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

/* Two actions: 20 (walk, 3 frames, times 4/2/8 = 14 total) and
 * 200 (punch, 2 frames with a hold frame at the end). Table sorted by
 * (action, index) exactly like the generated assets. */
static const ik_frame_t k_frames[] = {
    {20u, 0u, 30u, 60u, 15, 58, 4u, 0u, 0u},
    {20u, 1u, 31u, 61u, 15, 59, 2u, IK_FRAME_FLAG_FLIP_H, 100u},
    {20u, 2u, 29u, 59u, 15, 57, 8u, 0u, 200u},
    {200u, 0u, 40u, 60u, 20, 58, 5u, 0u, 300u},
    {200u, 1u, 45u, 62u, 22, 60, 0u, IK_FRAME_FLAG_FLIP_V, 400u}, /* hold */
};

int main() {
    const ik_frame_table_t table = {k_frames, sizeof(k_frames) / sizeof(k_frames[0])};

    uint32_t first = 99u, count = 0u;
    OK(ik_frames_bounds(&table, 20, &first, &count));
    OK(first == 0u && count == 3u);
    OK(ik_frames_bounds(&table, 200, &first, &count));
    OK(first == 3u && count == 2u);
    OK(!ik_frames_bounds(&table, 999, &first, &count));
    OK(ik_frames_bounds(0, 20, 0, 0) == 0);

    /* Frame sampling honours per-frame times and wraps at action end. */
    OK(ik_frame_at_time(&table, 20, 0u)->index == 0u);
    OK(ik_frame_at_time(&table, 20, 3u)->index == 0u);
    OK(ik_frame_at_time(&table, 20, 4u)->index == 1u);
    OK(ik_frame_at_time(&table, 20, 5u)->index == 1u);
    OK(ik_frame_at_time(&table, 20, 6u)->index == 2u);
    OK(ik_frame_at_time(&table, 20, 13u)->index == 2u);
    OK(ik_frame_at_time(&table, 20, 14u)->index == 0u); /* wrapped */
    OK(ik_frame_at_time(&table, 20, 28u)->index == 0u); /* two wraps */
    OK(ik_frame_at_time(&table, 20, 9999u) != 0);

    /* Hold frames (AIR time 0) stop advancing instead of spinning. */
    OK(ik_frame_at_time(&table, 200, 0u)->index == 0u);
    OK(ik_frame_at_time(&table, 200, 5u)->index == 1u);
    OK(ik_frame_at_time(&table, 200, 5000u)->index == 1u);

    /* Missing action and degenerate tables are NULL, not a crash. */
    OK(ik_frame_at_time(&table, 4711, 0u) == 0);
    const ik_frame_table_t empty = {0, 0u};
    OK(ik_frame_at_time(&empty, 20, 0u) == 0);

    /* Anchoring: axis lands on (x, y); facing and baked flips mirror
     * the axis horizontally, V flip mirrors it vertically. */
    int16_t dx = 0, dy = 0;
    ik_frame_screen_anchor(&k_frames[0], 100, 200, 1, &dx, &dy);
    OK(dx == 100 - 15 && dy == 200 - 58);
    ik_frame_screen_anchor(&k_frames[0], 100, 200, -1, &dx, &dy);
    OK(dx == 100 - (30 - 15) && dy == 200 - 58);
    /* Frame 1 bakes an H flip: facing right already mirrors. */
    ik_frame_screen_anchor(&k_frames[1], 100, 200, 1, &dx, &dy);
    OK(dx == 100 - (31 - 15) && dy == 200 - 59);
    ik_frame_screen_anchor(&k_frames[1], 100, 200, -1, &dx, &dy);
    OK(dx == 100 - 15 && dy == 200 - 59);
    /* Frame 4 bakes a V flip: the vertical axis mirrors too. */
    ik_frame_screen_anchor(&k_frames[4], 100, 200, 1, &dx, &dy);
    OK(dx == 100 - 22 && dy == 200 - (62 - 60));
    ik_frame_screen_anchor(0, 0, 0, 1, &dx, &dy);

    std::puts("[test] ikemen_anim OK");
    return 0;
}
