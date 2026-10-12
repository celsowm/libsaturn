#include <cstdio>
#include <cstdlib>
#include "saturn/skeleton3d.h"

#define CHECK(expr) do { if (!(expr)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); std::exit(1); } } while (0)
#define FX(n) ((sat_fx16_t)((n) * 65536))

static int32_t absdiff(int32_t a, int32_t b) { return a > b ? a - b : b - a; }

/* 5 frames, 2 joints, 4 frames per second. Joint 0 translates +1 unit in x
 * per frame; joint 1 rotates 0 -> 90 degrees about Z over the clip. */
static sat_skeleton_pose_joint_t kFrames[5 * 2];
static const sat_clip_event_t kEvents[2] = {{1, 10}, {3, 30}};

static void build_frames() {
    static const sat_quat16_t rot[5] = {
        {0, 0, 0, 32767}, {0, 0, 6270, 32138}, {0, 0, 12540, 30274},
        {0, 0, 18205, 27246}, {0, 0, 23170, 23170}};
    for (int f = 0; f < 5; ++f) {
        kFrames[f * 2 + 0].translation = {FX(f), 0, 0};
        kFrames[f * 2 + 0].rotation = {0, 0, 0, 32767};
        kFrames[f * 2 + 1].translation = {0, FX(1), 0};
        kFrames[f * 2 + 1].rotation = rot[f];
    }
}

static sat_skeleton_clip_t clip(uint16_t flags) {
    sat_skeleton_clip_t c{};
    c.frames = kFrames;
    c.events = kEvents;
    c.frame_count = 5;
    c.joint_count = 2;
    c.event_count = 2;
    c.rate_num = 4;
    c.rate_den = 1;
    c.flags = flags;
    return c;
}

static void validation_and_duration() {
    sat_skeleton_clip_t c = clip(0);
    CHECK(sat_clip_validate(&c, nullptr) == SAT_OK);
    CHECK(sat_clip_validate(nullptr, nullptr) == SAT_ERR_INVALID_ARG);
    sat_skeleton_clip_t bad = c;
    bad.frame_count = 0;
    CHECK(sat_clip_validate(&bad, nullptr) == SAT_ERR_INVALID_ARG);
    bad = c; bad.rate_num = 0;
    CHECK(sat_clip_validate(&bad, nullptr) == SAT_ERR_INVALID_ARG);
    bad = c; bad.rate_den = 0;
    CHECK(sat_clip_validate(&bad, nullptr) == SAT_ERR_INVALID_ARG);
    bad = c; bad.frames = nullptr;
    CHECK(sat_clip_validate(&bad, nullptr) == SAT_ERR_INVALID_ARG);
    static const sat_clip_event_t unsorted[2] = {{3, 1}, {1, 2}};
    bad = c; bad.events = unsorted;
    CHECK(sat_clip_validate(&bad, nullptr) == SAT_ERR_INVALID_ARG);
    static const sat_clip_event_t beyond[1] = {{9, 1}};
    bad = c; bad.events = beyond; bad.event_count = 1;
    CHECK(sat_clip_validate(&bad, nullptr) == SAT_ERR_INVALID_ARG);

    sat_skeleton_joint_t j[3] = {};
    for (auto& x : j) { x.parent = SAT_TRANSFORM3D_ROOT; sat_quat16_identity(&x.rest_rotation); }
    sat_skeleton_asset_t three{j, 3, 0}, two{j, 2, 0};
    CHECK(sat_clip_validate(&c, &three) == SAT_ERR_INVALID_ARG);
    CHECK(sat_clip_validate(&c, &two) == SAT_OK);

    CHECK(sat_clip_duration(&c) == FX(1));              /* one-shot: 4 frame intervals */
    sat_skeleton_clip_t l = clip(SAT_CLIP_FLAG_LOOP);
    CHECK(sat_clip_duration(&l) == FX(1) + FX(1) / 4);  /* loop: 5 frames */
}

static void sampling() {
    sat_skeleton_pose_joint_t out[2];
    sat_skeleton_clip_t c = clip(0);
    CHECK(sat_clip_sample(&c, 0, out, 1) == SAT_ERR_CAPACITY);
    CHECK(sat_clip_sample(&c, 0, out, 2) == SAT_OK);
    CHECK(out[0].translation.x == 0);
    CHECK(sat_clip_sample(&c, FX(1) / 8, out, 2) == SAT_OK);   /* frame 0.5 */
    CHECK(out[0].translation.x == FX(1) / 2);
    CHECK(absdiff(out[1].rotation.z, 3135) < 300);
    CHECK(sat_clip_sample(&c, FX(1) / 4, out, 2) == SAT_OK);   /* exactly frame 1 */
    CHECK(out[0].translation.x == FX(1) && out[1].rotation.z == 6270);
    CHECK(sat_clip_sample(&c, FX(50), out, 2) == SAT_OK);      /* one-shot clamps */
    CHECK(out[0].translation.x == FX(4) && out[1].rotation.z == 23170);
    CHECK(sat_clip_sample(&c, -FX(3), out, 2) == SAT_OK);      /* negative clamps to 0 */
    CHECK(out[0].translation.x == 0);

    sat_skeleton_clip_t l = clip(SAT_CLIP_FLAG_LOOP);
    CHECK(sat_clip_sample(&l, FX(1) + FX(1) / 8, out, 2) == SAT_OK); /* frame 4.5: 4 -> 0 */
    CHECK(out[0].translation.x == FX(2));
    CHECK(sat_clip_sample(&l, FX(1) + FX(1) / 4, out, 2) == SAT_OK); /* wrapped to frame 0 */
    CHECK(out[0].translation.x == 0);
    CHECK(sat_clip_sample(&l, FX(10) + FX(1) / 4, out, 2) == SAT_OK); /* many loops: frame 1 */
    CHECK(out[0].translation.x == FX(1));
}

static void one_shot_playback_and_root_motion() {
    sat_skeleton_clip_t c = clip(SAT_CLIP_ROOT_X);
    sat_skel_clip_player_t p{};
    CHECK(sat_skel_clip_player_init(&p, &c) == SAT_OK);
    sat_vec3_t d{};
    uint16_t n = 99;
    CHECK(sat_skel_clip_player_advance(&p, FX(1) / 2, nullptr, 0, &n, &d) == SAT_OK);
    CHECK(d.x == FX(2) && d.y == 0 && n == 1);   /* count-only: frame 1 crossed; 0.5 s = 2 frames of root travel */
    sat_skeleton_pose_joint_t pose[2];
    CHECK(sat_skel_clip_player_sample(&p, pose, 2) == SAT_OK);
    CHECK(pose[0].translation.x == 0);           /* extracted from the pose */
    CHECK((p.flags & SAT_CLIP_PLAYER_FINISHED) == 0);
    CHECK(sat_skel_clip_player_advance(&p, FX(5), nullptr, 0, &n, &d) == SAT_OK);
    CHECK(d.x == FX(2));                         /* only up to the last frame */
    CHECK(p.flags & SAT_CLIP_PLAYER_FINISHED);
    CHECK(sat_skel_clip_player_advance(&p, FX(1), nullptr, 0, &n, &d) == SAT_OK);
    CHECK(d.x == 0 && n == 0);                   /* finished: nothing more */

    /* A clip without extraction keeps the translation in the pose, delta 0. */
    sat_skeleton_clip_t plain = clip(0);
    CHECK(sat_skel_clip_player_init(&p, &plain) == SAT_OK);
    CHECK(sat_skel_clip_player_advance(&p, FX(1) / 2, nullptr, 0, &n, &d) == SAT_OK);
    CHECK(d.x == 0);
    CHECK(sat_skel_clip_player_sample(&p, pose, 2) == SAT_OK);
    CHECK(pose[0].translation.x == FX(2));
}

static void events() {
    sat_skeleton_clip_t c = clip(0);
    sat_skel_clip_player_t p{};
    CHECK(sat_skel_clip_player_init(&p, &c) == SAT_OK);
    sat_clip_event_t ev[4];
    uint16_t n = 0;
    CHECK(sat_skel_clip_player_advance(&p, FX(3) / 5, ev, 4, &n, nullptr) == SAT_OK); /* to frame 2.4 */
    CHECK(n == 1 && ev[0].id == 10);
    /* capacity failure leaves the player untouched */
    const sat_fx16_t before = p.time;
    CHECK(sat_skel_clip_player_advance(&p, FX(1) / 2, ev, 0, &n, nullptr) == SAT_ERR_CAPACITY);
    CHECK(p.time == before);
    CHECK(sat_skel_clip_player_advance(&p, FX(1) / 2, ev, 4, &n, nullptr) == SAT_OK);
    CHECK(n == 1 && ev[0].id == 30);
    /* nothing fires twice */
    CHECK(sat_skel_clip_player_advance(&p, FX(1), ev, 4, &n, nullptr) == SAT_OK && n == 0);

    /* Looping: wrap reports the events after the seam, once per cycle. */
    sat_skeleton_clip_t l = clip(SAT_CLIP_FLAG_LOOP);
    CHECK(sat_skel_clip_player_init(&p, &l) == SAT_OK);
    p.time = FX(1) - FX(1) / 10;               /* frame 3.6 */
    CHECK(sat_skel_clip_player_advance(&p, FX(3) / 4, ev, 4, &n, nullptr) == SAT_OK); /* -> 6.6 */
    CHECK(n == 1 && ev[0].id == 10);
    p.time = 0;
    CHECK(sat_skel_clip_player_advance(&p, FX(2) + FX(1) / 8, ev, 4, &n, nullptr) == SAT_OK);
    /* 8.5 frames: crosses frame 1,3 (cycle 0) and 6,8 (cycle 1) */
    CHECK(n == 4 && ev[0].id == 10 && ev[1].id == 30 && ev[2].id == 10 && ev[3].id == 30);
}

static void loop_root_motion() {
    sat_skeleton_clip_t l = clip(SAT_CLIP_FLAG_LOOP | SAT_CLIP_ROOT_X);
    sat_skel_clip_player_t p{};
    CHECK(sat_skel_clip_player_init(&p, &l) == SAT_OK);
    sat_vec3_t d{};
    uint16_t n;
    /* a whole loop and a half frame: 5.5 frames -> travel 4 (last-first) + 0.5 */
    CHECK(sat_skel_clip_player_advance(&p, FX(1) + FX(3) / 8, nullptr, 0, &n, &d) == SAT_OK);
    CHECK(d.x == FX(4) + FX(1) / 2);
}

static void controls() {
    sat_skeleton_clip_t c = clip(0);
    sat_skel_clip_player_t p{};
    CHECK(sat_skel_clip_player_init(&p, &c) == SAT_OK);
    CHECK(sat_skel_clip_player_set_speed(&p, -1) == SAT_ERR_INVALID_ARG);
    CHECK(sat_skel_clip_player_set_speed(&p, FX(2)) == SAT_OK);
    uint16_t n;
    CHECK(sat_skel_clip_player_advance(&p, FX(1) / 8, nullptr, 0, &n, nullptr) == SAT_OK);
    CHECK(p.time == FX(1) / 4);                 /* double speed */
    sat_skel_clip_player_set_paused(&p, 1);
    CHECK(sat_skel_clip_player_advance(&p, FX(1), nullptr, 0, &n, nullptr) == SAT_OK);
    CHECK(p.time == FX(1) / 4);
    sat_skel_clip_player_set_paused(&p, 0);
    CHECK(sat_skel_clip_player_advance(&p, -1, nullptr, 0, &n, nullptr) == SAT_ERR_INVALID_ARG);
    /* rapid switching: finished state clears, speed stays */
    for (int i = 0; i < 100; ++i) {
        CHECK(sat_skel_clip_player_set_clip(&p, &c, 0) == SAT_OK);
        CHECK(sat_skel_clip_player_advance(&p, FX(1), nullptr, 0, &n, nullptr) == SAT_OK);
    }
    CHECK(sat_skel_clip_player_set_clip(&p, &c, 0) == SAT_OK);
    CHECK(p.time == 0 && !(p.flags & SAT_CLIP_PLAYER_FINISHED) && p.speed == FX(2));
    CHECK(sat_skel_clip_player_set_clip(&p, nullptr, 0) == SAT_ERR_INVALID_ARG);
    CHECK(p.clip == &c);
}

static void crossfade() {
    sat_skeleton_pose_joint_t storage[2], from[2], live[2];
    for (int i = 0; i < 2; ++i) {
        from[i].translation = {FX(10), 0, 0}; sat_quat16_identity(&from[i].rotation);
        live[i].translation = {0, 0, 0};      sat_quat16_identity(&live[i].rotation);
    }
    sat_pose_fade_t f{};
    CHECK(sat_pose_fade_init(&f, storage, 2) == SAT_OK);
    CHECK(!sat_pose_fade_active(&f));
    CHECK(sat_pose_fade_begin(&f, from, -1) == SAT_ERR_INVALID_ARG);
    CHECK(sat_pose_fade_begin(&f, from, FX(1)) == SAT_OK);
    CHECK(sat_pose_fade_active(&f));
    sat_pose_fade_advance(&f, FX(1) / 2);
    sat_skeleton_pose_joint_t blended[2] = {live[0], live[1]};
    CHECK(sat_pose_fade_apply(&f, blended) == SAT_OK);
    CHECK(blended[0].translation.x == FX(5));
    /* interrupt mid-fade: start again from the blended pose */
    CHECK(sat_pose_fade_begin(&f, blended, FX(1)) == SAT_OK);
    sat_pose_fade_advance(&f, FX(1) / 2);
    sat_skeleton_pose_joint_t again[2] = {live[0], live[1]};
    CHECK(sat_pose_fade_apply(&f, again) == SAT_OK);
    CHECK(again[0].translation.x == FX(5) / 2);
    sat_pose_fade_advance(&f, FX(5));
    CHECK(!sat_pose_fade_active(&f));
    sat_skeleton_pose_joint_t done[2] = {live[0], live[1]};
    CHECK(sat_pose_fade_apply(&f, done) == SAT_OK);
    CHECK(done[0].translation.x == 0);
    /* zero duration is a cut */
    CHECK(sat_pose_fade_begin(&f, from, 0) == SAT_OK);
    CHECK(!sat_pose_fade_active(&f));
}

int main() {
    build_frames();
    validation_and_duration();
    sampling();
    one_shot_playback_and_root_motion();
    events();
    loop_root_motion();
    controls();
    crossfade();
    std::printf("test_skeleton_clip: 8 tests passed\n");
    return 0;
}
