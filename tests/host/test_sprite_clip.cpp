#include <cstdio>
#include <cstdlib>
#include <vector>

#include "saturn/sprite_clip.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

static sat_fx16_t PX(double v) { return static_cast<sat_fx16_t>(v * SAT_FX16_ONE); }

static sat_clip_frame_t F(uint16_t dur, uint16_t ev = 0, uint16_t shape_first = 0, uint8_t shape_count = 0) {
    sat_clip_frame_t f = {};
    f.source = {0, 0, 32, 48};
    f.pivot_x = 10;
    f.pivot_y = 40;
    f.duration = dur;
    f.event = ev;
    f.shape_first = shape_first;
    f.shape_count = shape_count;
    return f;
}

/* ----- a small set: walk, jump, pong, run, bounce ----- */

static const sat_clip_frame_t kWalk[] = {F(2, 10), F(3), F(1, 12), F(2)};
static const sat_clip_frame_t kJump[] = {F(2, 5), F(2), F(2, 7)};
static const sat_clip_frame_t kPong[] = {F(1), F(1), F(1)};
static const sat_clip_frame_t kRun[] = {F(1, 20), F(1), F(1), F(1), F(1), F(1)};
static const sat_clip_frame_t kBounce[] = {F(1), F(1), F(1)};
static const sat_clip_frame_t kOne[] = {F(2, 30)};

static const sat_clip_shape_t kShapes[] = {
    {4, -30, 12, 8, 1, 0, 0},
    {-8, -20, 6, 6, 1, 1, 0},
    {20, -10, 0, 0, 2, 0, 0},
};
static const sat_clip_frame_t kBoxed[] = {F(1, 0, 0, 3), F(1, 0, 2, 1), F(1)};

enum { WALK, JUMP, PONG, RUN, BOUNCE, SINGLE, BOXED };
static const sat_clip_t kClips[] = {
    {kWalk, 4, 0, SAT_CLIP_LOOP, {}},
    {kJump, 3, 0, SAT_CLIP_ONCE, {}},
    {kPong, 3, 0, SAT_CLIP_PING_PONG, {}},
    {kRun, 6, 0, SAT_CLIP_LOOP, {}},
    {kBounce, 3, 1, SAT_CLIP_LOOP, {}},
    {kOne, 1, 0, SAT_CLIP_PING_PONG, {}},
    {kBoxed, 3, 0, SAT_CLIP_ONCE, {}},
};
static const sat_clip_set_t kSet = {kClips, 7, 3, kShapes};

struct Log {
    std::vector<uint16_t> events, clips, frames;
};
static void on_event(void* user, uint16_t ev, uint16_t clip, uint16_t frame) {
    Log* l = static_cast<Log*>(user);
    l->events.push_back(ev);
    l->clips.push_back(clip);
    l->frames.push_back(frame);
}

static sat_clip_player_t make(uint16_t clip, Log* log = nullptr) {
    sat_clip_player_t p;
    OK(sat_clip_player_init(&p, &kSet) == SAT_OK);
    OK(!sat_clip_player_is_playing(&p));
    OK(sat_clip_player_frame(&p) == nullptr);
    OK(sat_clip_player_play(&p, clip, SAT_CLIP_SWITCH_RESTART, log ? on_event : nullptr, log) == SAT_OK);
    return p;
}

static sat_clip_step_result_t step(sat_clip_player_t* p, Log* log = nullptr) {
    sat_clip_step_result_t r;
    OK(sat_clip_player_step(p, log ? on_event : nullptr, log, &r) == SAT_OK);
    return r;
}

static void test_validate() {
    OK(sat_clip_set_validate(&kSet) == SAT_OK);
    OK(sat_clip_set_region_count(&kSet) == 4 + 3 + 3 + 6 + 3 + 1 + 3);
    OK(sat_clip_set_validate(nullptr) == SAT_ERR_INVALID_ARG);

    {   /* zero duration */
        sat_clip_frame_t f[] = {F(1), F(0)};
        sat_clip_t c = {f, 2, 0, SAT_CLIP_LOOP, {}};
        sat_clip_set_t s = {&c, 1, 0, nullptr};
        OK(sat_clip_set_validate(&s) == SAT_ERR_INVALID_ARG);
    }
    {   /* empty source */
        sat_clip_frame_t f[] = {F(1)};
        f[0].source.width = 0;
        sat_clip_t c = {f, 1, 0, SAT_CLIP_LOOP, {}};
        sat_clip_set_t s = {&c, 1, 0, nullptr};
        OK(sat_clip_set_validate(&s) == SAT_ERR_INVALID_ARG);
    }
    {   /* loop start past the end, bad mode, no frames, bad shape range */
        sat_clip_frame_t f[] = {F(1), F(1)};
        sat_clip_t c = {f, 2, 2, SAT_CLIP_LOOP, {}};
        sat_clip_set_t s = {&c, 1, 0, nullptr};
        OK(sat_clip_set_validate(&s) == SAT_ERR_INVALID_ARG);
        c = {f, 2, 0, 9, {}};
        OK(sat_clip_set_validate(&s) == SAT_ERR_INVALID_ARG);
        c = {f, 0, 0, SAT_CLIP_LOOP, {}};
        OK(sat_clip_set_validate(&s) == SAT_ERR_INVALID_ARG);
        c = {nullptr, 2, 0, SAT_CLIP_LOOP, {}};
        OK(sat_clip_set_validate(&s) == SAT_ERR_INVALID_ARG);
        sat_clip_frame_t g[] = {F(1, 0, 2, 2)};
        c = {g, 1, 0, SAT_CLIP_LOOP, {}};
        sat_clip_set_t s2 = {&c, 1, 3, kShapes};
        OK(sat_clip_set_validate(&s2) == SAT_ERR_INVALID_ARG);
        g[0].shape_count = 1; /* the same frame with a range that fits */
        OK(sat_clip_set_validate(&s2) == SAT_OK);
    }
}

static void test_loop_and_events() {
    Log log;
    sat_clip_player_t p = make(WALK, &log);
    OK(log.events.size() == 1 && log.events[0] == 10 && log.frames[0] == 0);

    const uint16_t expect_frame[] = {0, 1, 1, 1, 2, 3, 3, 0};
    for (int i = 0; i < 8; ++i) {
        const sat_clip_step_result_t r = step(&p, &log);
        OK(sat_clip_player_frame_index(&p) == expect_frame[i]);
        OK(((r.flags & SAT_CLIP_STEP_FRAME_CHANGED) != 0) == (i == 1 || i == 4 || i == 5 || i == 7));
        OK(((r.flags & SAT_CLIP_STEP_LOOPED) != 0) == (i == 7));
    }
    OK(p.loops == 1);
    /* events: first frame on play, frame 2, then frame 0 again after the loop */
    OK(log.events.size() == 3 && log.events[1] == 12 && log.events[2] == 10);
    OK(sat_clip_player_clip(&p) == WALK && sat_clip_player_is_playing(&p) && !sat_clip_player_is_finished(&p));
}

static void test_one_shot() {
    Log log;
    sat_clip_player_t p = make(JUMP, &log);
    for (int i = 0; i < 5; ++i) {
        const sat_clip_step_result_t r = step(&p, &log);
        OK(!(r.flags & SAT_CLIP_STEP_FINISHED));
    }
    OK(sat_clip_player_frame_index(&p) == 2 && !sat_clip_player_is_finished(&p));
    const sat_clip_step_result_t r = step(&p, &log);
    OK(r.flags & SAT_CLIP_STEP_FINISHED);
    OK(r.frames_advanced == 0);
    OK(sat_clip_player_is_finished(&p) && sat_clip_player_frame_index(&p) == 2);
    OK(log.events.size() == 2 && log.events[0] == 5 && log.events[1] == 7);

    /* finished: nothing moves and nothing is delivered */
    for (int i = 0; i < 10; ++i) OK(step(&p, &log).flags == 0);
    OK(log.events.size() == 2);

    /* KEEP_IF_SAME leaves a finished clip alone, RESTART plays it again */
    OK(sat_clip_player_play(&p, JUMP, SAT_CLIP_SWITCH_KEEP_IF_SAME, on_event, &log) == SAT_OK);
    OK(sat_clip_player_is_finished(&p) && log.events.size() == 2);
    OK(sat_clip_player_play(&p, JUMP, SAT_CLIP_SWITCH_RESTART, on_event, &log) == SAT_OK);
    OK(!sat_clip_player_is_finished(&p) && sat_clip_player_frame_index(&p) == 0 && log.events.size() == 3 && log.events[2] == 5);

    /* restart() after finishing */
    for (int i = 0; i < 7; ++i) step(&p);
    OK(sat_clip_player_is_finished(&p));
    OK(sat_clip_player_restart(&p, on_event, &log) == SAT_OK);
    OK(!sat_clip_player_is_finished(&p) && p.loops == 0 && sat_clip_player_frame_index(&p) == 0 && log.events.size() == 4);
}

static void test_ping_pong() {
    sat_clip_player_t p = make(PONG);
    const uint16_t expect[] = {0, 1, 2, 1, 0, 1, 2, 1, 0};
    for (int i = 0; i < 9; ++i) {
        OK(sat_clip_player_frame_index(&p) == expect[i]);
        const sat_clip_step_result_t r = step(&p);
        OK(((r.flags & SAT_CLIP_STEP_LOOPED) != 0) == (i == 4 || i == 8)); /* a cycle completes back at frame 0 */
    }
    OK(p.loops == 2);

    /* a one-frame ping-pong holds still and still counts cycles and events */
    Log log;
    sat_clip_player_t s = make(SINGLE, &log);
    for (int i = 0; i < 6; ++i) step(&s, &log);
    OK(sat_clip_player_frame_index(&s) == 0 && s.loops == 3);
    OK(log.events.size() == 4); /* play + 3 cycles */
}

static void test_loop_start() {
    sat_clip_player_t p = make(BOUNCE);
    const uint16_t expect[] = {0, 1, 2, 1, 2, 1};
    for (int i = 0; i < 6; ++i) {
        OK(sat_clip_player_frame_index(&p) == expect[i]);
        step(&p);
    }
}

static void test_rate_and_fractions() {
    sat_clip_player_t p = make(BOUNCE);
    OK(sat_clip_player_set_rate(&p, PX(0.25)) == SAT_OK);
    for (int i = 0; i < 3; ++i) step(&p);
    OK(sat_clip_player_frame_index(&p) == 0);
    step(&p);
    OK(sat_clip_player_frame_index(&p) == 1);

    /* no drift over a long run: 400 quarter-ticks = 100 frames of 1 tick */
    sat_clip_player_t q = make(RUN);
    OK(sat_clip_player_set_rate(&q, PX(0.25)) == SAT_OK);
    for (int i = 0; i < 4 * 6 * 10; ++i) step(&q);
    OK(q.loops == 10 && sat_clip_player_frame_index(&q) == 0 && q.elapsed == 0);

    /* double rate crosses two frames per step */
    sat_clip_player_t d = make(RUN);
    OK(sat_clip_player_set_rate(&d, PX(2.0)) == SAT_OK);
    sat_clip_step_result_t r = step(&d);
    OK(r.frames_advanced == 2 && sat_clip_player_frame_index(&d) == 2);

    /* rate 0 freezes, a negative rate is refused */
    OK(sat_clip_player_set_rate(&d, 0) == SAT_OK);
    for (int i = 0; i < 20; ++i) OK(step(&d).flags == 0);
    OK(sat_clip_player_frame_index(&d) == 2);
    OK(sat_clip_player_set_rate(&d, -1) == SAT_ERR_INVALID_ARG && d.rate == 0);

    /* variable step */
    sat_clip_player_t v = make(WALK);
    r = {};
    OK(sat_clip_player_advance(&v, PX(0.5), nullptr, nullptr, &r) == SAT_OK && r.flags == 0 && v.elapsed == PX(0.5));
    OK(sat_clip_player_advance(&v, -1, nullptr, nullptr, &r) == SAT_ERR_INVALID_ARG);
}

static void test_multi_frame_step() {
    Log log;
    sat_clip_player_t p = make(WALK, &log);
    log = {};
    sat_clip_step_result_t r;
    OK(sat_clip_player_advance(&p, PX(5), on_event, &log, &r) == SAT_OK);
    OK(r.frames_advanced == 2 && sat_clip_player_frame_index(&p) == 2 && r.events == 1);
    OK(log.events.size() == 1 && log.events[0] == 12 && log.frames[0] == 2);

    log = {};
    OK(sat_clip_player_advance(&p, PX(8), on_event, &log, &r) == SAT_OK);
    OK(r.frames_advanced == 4 && (r.flags & SAT_CLIP_STEP_LOOPED) && p.loops == 1);
    OK(log.events.size() == 2 && log.events[0] == 10 && log.events[1] == 12);
    OK(log.frames[0] == 0 && log.frames[1] == 2 && sat_clip_player_frame_index(&p) == 2);

    /* a one-shot cut short by a huge step finishes, and delivers what lay before the end */
    Log jl;
    sat_clip_player_t j = make(JUMP, &jl);
    jl = {};
    OK(sat_clip_player_advance(&j, PX(100), on_event, &jl, &r) == SAT_OK);
    OK((r.flags & SAT_CLIP_STEP_FINISHED) && sat_clip_player_frame_index(&j) == 2 && jl.events.size() == 1 && jl.events[0] == 7);
}

static void test_pause_seek() {
    Log log;
    sat_clip_player_t p = make(WALK, &log);
    OK(sat_clip_player_pause(&p) == SAT_OK && sat_clip_player_is_paused(&p));
    for (int i = 0; i < 10; ++i) OK(step(&p, &log).flags == 0);
    OK(sat_clip_player_frame_index(&p) == 0 && p.elapsed == 0);
    OK(sat_clip_player_resume(&p) == SAT_OK && !sat_clip_player_is_paused(&p));
    step(&p);
    step(&p);
    OK(sat_clip_player_frame_index(&p) == 1);

    log = {};
    OK(sat_clip_player_seek(&p, 2, on_event, &log) == SAT_OK);
    OK(sat_clip_player_frame_index(&p) == 2 && p.elapsed == 0 && log.events.size() == 1 && log.events[0] == 12);
    OK(sat_clip_player_seek(&p, 4, nullptr, nullptr) == SAT_ERR_INVALID_ARG);

    sat_clip_player_t idle;
    OK(sat_clip_player_init(&idle, &kSet) == SAT_OK);
    OK(step(&idle).flags == 0);
    OK(sat_clip_player_seek(&idle, 0, nullptr, nullptr) == SAT_ERR_INVALID_ARG);
    OK(sat_clip_player_restart(&idle, nullptr, nullptr) == SAT_ERR_INVALID_ARG);
}

static void test_switching() {
    Log log;
    sat_clip_player_t p = make(WALK, &log);
    for (int i = 0; i < 3; ++i) step(&p);
    OK(sat_clip_player_frame_index(&p) == 1);

    /* KEEP_IF_SAME on the same clip changes nothing, and delivers nothing */
    log = {};
    OK(sat_clip_player_play(&p, WALK, SAT_CLIP_SWITCH_KEEP_IF_SAME, on_event, &log) == SAT_OK);
    OK(sat_clip_player_frame_index(&p) == 1 && p.elapsed == PX(1) && log.events.empty());
    /* ... and on another clip it restarts */
    OK(sat_clip_player_play(&p, RUN, SAT_CLIP_SWITCH_KEEP_IF_SAME, on_event, &log) == SAT_OK);
    OK(sat_clip_player_clip(&p) == RUN && sat_clip_player_frame_index(&p) == 0 && log.events.size() == 1 && log.events[0] == 20);

    /* KEEP_PHASE: walk frame 2 half done -> run (6 frames) frame 3, half done, no event */
    sat_clip_player_t w = make(WALK);
    for (int i = 0; i < 5; ++i) step(&w);
    OK(sat_clip_player_frame_index(&w) == 2);
    OK(sat_clip_player_advance(&w, PX(0.5), nullptr, nullptr, nullptr) == SAT_OK);
    log = {};
    OK(sat_clip_player_play(&w, RUN, SAT_CLIP_SWITCH_KEEP_PHASE, on_event, &log) == SAT_OK);
    OK(sat_clip_player_clip(&w) == RUN && sat_clip_player_frame_index(&w) == 3 && w.elapsed == PX(0.5) && log.events.empty());
    OK(w.loops == 0 && !w.reverse);

    /* KEEP_PHASE with nothing playing is a plain start */
    sat_clip_player_t n;
    OK(sat_clip_player_init(&n, &kSet) == SAT_OK);
    OK(sat_clip_player_play(&n, RUN, SAT_CLIP_SWITCH_KEEP_PHASE, on_event, &log) == SAT_OK);
    OK(sat_clip_player_frame_index(&n) == 0 && log.events.size() == 1);

    /* an unknown clip is refused and leaves the player alone */
    OK(sat_clip_player_play(&n, 99, SAT_CLIP_SWITCH_RESTART, nullptr, nullptr) == SAT_ERR_INVALID_ARG);
    OK(sat_clip_player_clip(&n) == RUN);

    /* rate and pause survive a switch */
    OK(sat_clip_player_set_rate(&n, PX(2)) == SAT_OK && sat_clip_player_pause(&n) == SAT_OK);
    OK(sat_clip_player_play(&n, WALK, SAT_CLIP_SWITCH_RESTART, nullptr, nullptr) == SAT_OK);
    OK(n.rate == PX(2) && sat_clip_player_is_paused(&n));
}

static void test_geometry() {
    const sat_clip_frame_t f = F(1); /* 32 x 48, pivot (10, 40) */
    sat_rect_t r;
    OK(sat_clip_frame_dest(&f, 100, 200, 0, &r) == SAT_OK);
    OK(r.x == 90 && r.y == 160 && r.width == 32 && r.height == 48);
    OK(sat_clip_frame_dest(&f, 100, 200, SAT_CLIP_FLIP_X, &r) == SAT_OK);
    OK(r.x == 78 && r.y == 160); /* the pivot is still 10 px from the (now right) edge: 78 + 32 - 10 = 100 */
    OK(sat_clip_frame_dest(&f, 100, 200, SAT_CLIP_FLIP_Y, &r) == SAT_OK);
    OK(r.x == 90 && r.y == 192);
    OK(sat_clip_frame_dest(&f, 100, 200, SAT_CLIP_FLIP_X | SAT_CLIP_FLIP_Y, &r) == SAT_OK);
    OK(r.x == 78 && r.y == 192);
    OK(sat_clip_frame_dest(&f, -32768, 0, 0, &r) == SAT_OK && r.x == -32768); /* clamped, not wrapped */
    OK(sat_clip_frame_dest(nullptr, 0, 0, 0, &r) == SAT_ERR_INVALID_ARG);

    const sat_clip_shape_t& box = kShapes[0]; /* 4,-30 12x8 */
    OK(sat_clip_shape_rect(&box, 100, 200, 0, &r) == SAT_OK);
    OK(r.x == 104 && r.y == 170 && r.width == 12 && r.height == 8);
    OK(sat_clip_shape_rect(&box, 100, 200, SAT_CLIP_FLIP_X, &r) == SAT_OK);
    OK(r.x == 84 && r.y == 170 && r.width == 12 && r.height == 8); /* 104..116 mirrored about 100 */
    OK(sat_clip_shape_rect(&box, 100, 200, SAT_CLIP_FLIP_Y, &r) == SAT_OK);
    OK(r.x == 104 && r.y == 222 && r.height == 8); /* -30..-22 above the pivot becomes 22..30 below it */

    const sat_clip_shape_t& pt = kShapes[2]; /* point at (20, -10) */
    sat_point_t q = sat_clip_shape_point(&pt, 100, 200, 0);
    OK(q.x == 120 && q.y == 190);
    q = sat_clip_shape_point(&pt, 100, 200, SAT_CLIP_FLIP_X);
    OK(q.x == 80 && q.y == 190);
    q = sat_clip_shape_point(&pt, 100, 200, SAT_CLIP_FLIP_Y);
    OK(q.x == 120 && q.y == 210);
}

static void test_shapes() {
    uint32_t n = 99;
    const sat_clip_frame_t* f0 = &kBoxed[0];
    const sat_clip_shape_t* s = sat_clip_frame_shapes(&kSet, f0, &n);
    OK(s == kShapes && n == 3);
    s = sat_clip_frame_shapes(&kSet, &kBoxed[1], &n);
    OK(s == kShapes + 2 && n == 1);
    s = sat_clip_frame_shapes(&kSet, &kBoxed[2], &n);
    OK(s == nullptr && n == 0);

    sat_clip_shape_t out = {};
    OK(sat_clip_frame_find_shape(&kSet, f0, 1, 1, &out) && out.x == -8 && out.w == 6);
    OK(sat_clip_frame_find_shape(&kSet, f0, 1, 0xFF, &out) && out.x == 4);
    OK(sat_clip_frame_find_shape(&kSet, f0, 2, 0xFF, &out) && out.x == 20);
    OK(!sat_clip_frame_find_shape(&kSet, f0, 3, 0xFF, &out));
    OK(!sat_clip_frame_find_shape(&kSet, &kBoxed[1], 1, 0xFF, &out));
    OK(sat_clip_frame_find_shape(&kSet, f0, 1, 0, nullptr));

    /* through the player */
    sat_clip_player_t p = make(BOXED);
    OK(sat_clip_player_frame(&p) == &kBoxed[0]);
    step(&p);
    OK(sat_clip_player_frame(&p) == &kBoxed[1]);
}

int main() {
    test_validate();
    test_loop_and_events();
    test_one_shot();
    test_ping_pong();
    test_loop_start();
    test_rate_and_fractions();
    test_multi_frame_step();
    test_pause_seek();
    test_switching();
    test_geometry();
    test_shapes();
    std::puts("test_sprite_clip ok");
    return 0;
}
