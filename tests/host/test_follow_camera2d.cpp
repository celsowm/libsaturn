#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "saturn/follow_camera2d.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

static sat_fx16_t F(double px) { return static_cast<sat_fx16_t>(std::lround(px * SAT_FX16_ONE)); }
static sat_vec2_t V(double x, double y) { return {F(x), F(y)}; }
static double D(sat_fx16_t v) { return v / 65536.0; }
static bool near(sat_fx16_t raw, double expect, double tol = 0.01) { return std::fabs(D(raw) - expect) <= tol; }
static bool at(sat_vec2_t p, double x, double y, double tol = 0.01) { return near(p.x, x, tol) && near(p.y, y, tol); }

static sat_follow_camera2d_config_t config() {
    sat_follow_camera2d_config_t c;
    sat_follow_camera2d_config_default(&c);
    return c;
}

static sat_follow_camera2d_t make(const sat_follow_camera2d_config_t& c, double x = 160, double y = 112) {
    sat_follow_camera2d_t cam;
    OK(sat_follow_camera2d_init(&cam, &c, V(x, y)) == SAT_OK);
    return cam;
}

static sat_camera2d_t step(sat_follow_camera2d_t& cam, double tx, double ty, const sat_vec2_t* vel = nullptr) {
    sat_camera2d_t out = {};
    OK(sat_follow_camera2d_step(&cam, V(tx, ty), vel, &out) == SAT_OK);
    return out;
}

static sat_box2_t range(const sat_follow_camera2d_t& cam, sat_camera_range_t r) {
    sat_box2_t b = {};
    OK(sat_follow_camera2d_range(&cam, r, &b) == SAT_OK);
    return b;
}

static void config_checks() {
    sat_follow_camera2d_config_t c = config();
    OK(sat_follow_camera2d_config_validate(&c) == SAT_OK);
    OK(sat_follow_camera2d_config_validate(nullptr) == SAT_ERR_INVALID_ARG);
    sat_follow_camera2d_config_t bad = c;
    bad.viewport_w = 0;
    OK(sat_follow_camera2d_config_validate(&bad) == SAT_ERR_INVALID_ARG);
    bad = c; bad.dead_half_h = -1;
    OK(sat_follow_camera2d_config_validate(&bad) == SAT_ERR_INVALID_ARG);
    bad = c; bad.follow_x = 0;
    OK(sat_follow_camera2d_config_validate(&bad) == SAT_ERR_INVALID_ARG);
    bad = c; bad.follow_y = SAT_FX16_ONE + 1;
    OK(sat_follow_camera2d_config_validate(&bad) == SAT_ERR_INVALID_ARG);
    bad = c; bad.look_ease = 0;
    OK(sat_follow_camera2d_config_validate(&bad) == SAT_ERR_INVALID_ARG);
    bad = c; bad.activation_margin = -1;
    OK(sat_follow_camera2d_config_validate(&bad) == SAT_ERR_INVALID_ARG);
    sat_follow_camera2d_t cam;
    OK(sat_follow_camera2d_init(&cam, &bad, V(0, 0)) == SAT_ERR_INVALID_ARG);
    OK(sat_follow_camera2d_init(nullptr, &c, V(0, 0)) == SAT_ERR_INVALID_ARG);
    OK(sat_follow_camera2d_step(nullptr, V(0, 0), nullptr, nullptr) == SAT_ERR_INVALID_ARG);
}

static void camera_output() {
    sat_follow_camera2d_t cam = make(config());
    const sat_camera2d_t out = step(cam, 160, 112);
    /* the target lands in the middle of the viewport, identity zoom and rotation */
    OK(near(out.offset_x, 160) && near(out.offset_y, 112));
    OK(near(out.target_x, 160) && near(out.target_y, 112));
    OK(out.rotation == 0 && out.zoom == SAT_FX16_ONE);
    /* the world point under the middle of the screen */
    OK(at(sat_follow_camera2d_world_to_screen(&cam, V(160, 112)), 160, 112));
    OK(at(sat_follow_camera2d_world_to_screen(&cam, V(0, 0)), 0, 0));
    cam = make(config(), 1000, 500);
    step(cam, 1000, 500);
    OK(at(sat_follow_camera2d_world_to_screen(&cam, V(840, 388)), 0, 0));
    const sat_vec2_t w = sat_follow_camera2d_screen_to_world(&cam, V(37.5, 80.25));
    OK(at(sat_follow_camera2d_world_to_screen(&cam, w), 37.5, 80.25, 0.001));
}

static void dead_zone() {
    sat_follow_camera2d_t cam = make(config());
    /* inside the 16 x 32 dead zone nothing moves */
    step(cam, 168, 128);
    OK(at(cam.centre, 160, 112) && at(cam.delta, 0, 0));
    step(cam, 152, 96);
    OK(at(cam.centre, 160, 112));
    /* leaving it drags the view exactly far enough to keep the target on the edge */
    step(cam, 200, 112);
    OK(at(cam.centre, 192, 112) && at(cam.delta, 32, 0));
    step(cam, 100, 112);
    OK(at(cam.centre, 108, 112) && at(cam.delta, -84, 0));
    step(cam, 108, 200);
    OK(at(cam.centre, 108, 184));
    step(cam, 108, 0);
    OK(at(cam.centre, 108, 16));

    /* a dead zone shifted down keeps the target below the middle of the screen */
    sat_follow_camera2d_config_t c = config();
    c.shift_y = F(24);
    c.dead_half_h = 0;
    cam = make(c);
    step(cam, 160, 136);
    OK(at(cam.centre, 160, 112));
    step(cam, 160, 236);
    OK(at(cam.centre, 160, 212));
    OK(sat_follow_camera2d_snap(&cam, V(500, 300)) == SAT_OK);
    OK(at(cam.centre, 500, 276) && at(cam.delta, 0, 0));
}

static void smoothing() {
    sat_follow_camera2d_config_t c = config();
    c.follow_x = F(0.5);
    sat_follow_camera2d_t cam = make(c);
    step(cam, 200, 112); /* 32 px outside the zone, half of it closed */
    OK(at(cam.centre, 176, 112));
    step(cam, 200, 112); /* zone edge now 184: 16 outside */
    OK(at(cam.centre, 184, 112));
    double last = D(cam.centre.x);
    for (int i = 0; i < 40; ++i) {
        step(cam, 200, 112);
        OK(D(cam.centre.x) >= last);
        last = D(cam.centre.x);
    }
    OK(std::fabs(last - 192.0) < 0.5); /* the target ends up at the zone edge */

    c = config();
    c.max_step_x = F(4);
    cam = make(c);
    step(cam, 400, 112);
    OK(at(cam.centre, 164, 112));
    step(cam, 400, 112);
    OK(at(cam.centre, 168, 112));
    step(cam, 100, 112); /* the cap holds in both directions */
    OK(at(cam.centre, 164, 112));
}

static void look_ahead() {
    sat_follow_camera2d_config_t c = config();
    c.dead_half_w = 0;
    c.look_max_x = F(24);
    c.look_gain_x = F(4);
    c.look_ease = SAT_FX16_ONE;
    sat_follow_camera2d_t cam = make(c);
    sat_vec2_t v = V(3, 0);
    step(cam, 160, 112, &v);
    OK(at(cam.look, 12, 0));
    OK(at(cam.centre, 172, 112)); /* the camera aims ahead of the target */
    v = V(10, 0);
    step(cam, 160, 112, &v);
    OK(at(cam.look, 24, 0)); /* capped */
    v = V(-10, 0);
    step(cam, 160, 112, &v);
    OK(at(cam.look, -24, 0));
    step(cam, 160, 112, nullptr); /* no velocity: back to centred */
    OK(at(cam.look, 0, 0));
    /* the vertical axis has its own range and is off by default */
    v = V(0, 50);
    step(cam, 160, 112, &v);
    OK(at(cam.look, 0, 0));

    /* eased look-ahead approaches its goal without overshoot */
    c.look_ease = F(0.25);
    cam = make(c);
    v = V(6, 0);
    double last = 0;
    for (int i = 0; i < 60; ++i) {
        step(cam, 160, 112, &v);
        OK(D(cam.look.x) >= last && D(cam.look.x) <= 24.0 + 1e-3);
        last = D(cam.look.x);
    }
    OK(std::fabs(last - 24.0) < 0.1);
    /* snapping forgets it */
    OK(sat_follow_camera2d_snap(&cam, V(160, 112)) == SAT_OK);
    OK(at(cam.look, 0, 0));
}

static void world_bounds() {
    sat_follow_camera2d_t cam = make(config());
    const sat_camera_bounds2_t world = {F(0), F(0), F(1000), F(500)};
    OK(sat_follow_camera2d_set_bounds(&cam, &world) == SAT_OK);
    step(cam, -300, -300);
    OK(at(cam.centre, 160, 112)); /* the window never shows the outside */
    step(cam, 5000, 5000);
    OK(at(cam.centre, 840, 388));
    const sat_box2_t view = range(cam, SAT_CAMERA_RANGE_VIEW);
    OK(near(view.center.x + view.half.x, 1000) && near(view.center.y + view.half.y, 500));
    /* a stage smaller than the screen is centred */
    const sat_camera_bounds2_t small = {F(0), F(0), F(200), F(100)};
    OK(sat_follow_camera2d_set_bounds(&cam, &small) == SAT_OK);
    step(cam, 50, 50);
    OK(at(cam.centre, 100, 50));
    /* snap respects the bounds too */
    OK(sat_follow_camera2d_set_bounds(&cam, &world) == SAT_OK);
    OK(sat_follow_camera2d_snap(&cam, V(-50, 900)) == SAT_OK);
    OK(at(cam.centre, 160, 388));
    /* removing the bounds frees the view */
    OK(sat_follow_camera2d_set_bounds(&cam, nullptr) == SAT_OK);
    step(cam, -300, 112);
    OK(near(cam.centre.x, -292)); /* the target on the dead zone's left edge */
    const sat_camera_bounds2_t inverted = {F(10), F(0), F(10), F(5)};
    OK(sat_follow_camera2d_set_bounds(&cam, &inverted) == SAT_ERR_INVALID_ARG);
    OK(sat_follow_camera2d_set_bounds(nullptr, &world) == SAT_ERR_INVALID_ARG);
}

static bool inside(const sat_box2_t& b, const sat_camera_bounds2_t& r) {
    return b.center.x - b.half.x >= r.min_x - 1 && b.center.x + b.half.x <= r.max_x + 1 &&
           b.center.y - b.half.y >= r.min_y - 1 && b.center.y + b.half.y <= r.max_y + 1;
}

static void clamps() {
    sat_follow_camera2d_t cam = make(config());
    const sat_camera_bounds2_t world = {F(0), F(0), F(2000), F(500)};
    OK(sat_follow_camera2d_set_bounds(&cam, &world) == SAT_OK);
    step(cam, 600, 250);
    /* a locked screen: a clamp exactly one screen wide pins the view */
    const sat_camera_bounds2_t room = {F(400), F(0), F(720), F(224)};
    OK(sat_follow_camera2d_set_clamp(&cam, &room, 0) == SAT_OK);
    step(cam, 600, 250);
    OK(at(cam.centre, 560, 112));
    step(cam, 1500, 400); /* the target can leave, the view cannot */
    OK(at(cam.centre, 560, 112));
    step(cam, 0, 0);
    OK(at(cam.centre, 560, 112));
    /* the window stays in the clamp (and so the world) the whole time */
    OK(inside(range(cam, SAT_CAMERA_RANGE_VIEW), room));
    /* releasing with no slide returns control at once */
    OK(sat_follow_camera2d_clear_clamp(&cam, 0) == SAT_OK);
    OK(!cam.has_clamp);
    step(cam, 1500, 400);
    OK(cam.centre.x > F(1000));

    /* a clamp that closes in slides each edge at its speed from the old bounds */
    cam = make(config(), 560, 250);
    OK(sat_follow_camera2d_set_bounds(&cam, &world) == SAT_OK);
    OK(sat_follow_camera2d_set_clamp(&cam, &room, F(10)) == SAT_OK);
    sat_camera_bounds2_t prev = cam.clamp;
    OK(prev.min_x == F(0) && prev.max_x == F(2000)); /* it starts from the world */
    for (int i = 0; i < 200; ++i) {
        step(cam, 560, 250);
        OK(cam.clamp.min_x >= prev.min_x && cam.clamp.max_x <= prev.max_x);
        OK(cam.clamp.min_x - prev.min_x <= F(10) && prev.max_x - cam.clamp.max_x <= F(10));
        OK(inside(range(cam, SAT_CAMERA_RANGE_VIEW), cam.clamp));
        prev = cam.clamp;
    }
    OK(cam.clamp.min_x == room.min_x && cam.clamp.max_x == room.max_x);
    OK(cam.clamp.min_y == room.min_y && cam.clamp.max_y == room.max_y);
    OK(at(cam.centre, 560, 112));

    /* and slides back out when released, then goes away */
    OK(sat_follow_camera2d_clear_clamp(&cam, F(10)) == SAT_OK);
    OK(cam.has_clamp && cam.clamp_releasing);
    bool gone = false;
    for (int i = 0; i < 300 && !gone; ++i) {
        step(cam, 1500, 400);
        gone = !cam.has_clamp;
    }
    OK(gone);
    for (int i = 0; i < 4; ++i) step(cam, 1500, 400);
    OK(cam.centre.x > F(1000));

    /* a clamp with no world slides from the current window */
    sat_follow_camera2d_t free_cam = make(config(), 300, 200);
    const sat_camera_bounds2_t box = {F(250), F(100), F(450), F(300)};
    OK(sat_follow_camera2d_set_clamp(&free_cam, &box, F(5)) == SAT_OK);
    OK(free_cam.clamp.min_x == F(140) && free_cam.clamp.max_x == F(460));
    step(free_cam, 300, 200);
    OK(free_cam.clamp.min_x == F(145) && free_cam.clamp.max_x == F(455));
    OK(sat_follow_camera2d_clear_clamp(&free_cam, F(5)) == SAT_OK);
    OK(!free_cam.has_clamp); /* nothing to slide back to */

    const sat_camera_bounds2_t inverted = {F(10), F(0), F(10), F(5)};
    OK(sat_follow_camera2d_set_clamp(&cam, &inverted, 0) == SAT_ERR_INVALID_ARG);
    OK(sat_follow_camera2d_set_clamp(&cam, &room, -1) == SAT_ERR_INVALID_ARG);
    OK(sat_follow_camera2d_set_clamp(nullptr, &room, 0) == SAT_ERR_INVALID_ARG);
}

static sat_shake2d_params_t shake_params() {
    sat_shake2d_params_t p = {};
    p.amplitude_x = F(4);
    p.amplitude_y = F(4);
    p.duration = 60;
    p.waveform = SAT_SHAKE2D_SINE;
    p.envelope = SAT_SHAKE2D_CONSTANT;
    p.sign = SAT_SHAKE2D_BOTH;
    p.update_every = 1;
    p.frequency = 3000;
    p.seed = 0;
    return p;
}

static std::vector<sat_vec2_t> run_shake(const sat_shake2d_params_t& p, int steps) {
    sat_shake2d_t s;
    OK(sat_shake2d_start(&s, &p) == SAT_OK);
    std::vector<sat_vec2_t> v;
    for (int i = 0; i < steps; ++i) v.push_back(sat_shake2d_step(&s));
    return v;
}

static double peak(const std::vector<sat_vec2_t>& v, size_t from, size_t to) {
    double m = 0;
    for (size_t i = from; i < to && i < v.size(); ++i) m = std::fmax(m, std::fmax(std::fabs(D(v[i].x)), std::fabs(D(v[i].y))));
    return m;
}

static void shake_envelopes() {
    sat_shake2d_params_t p = shake_params();
    sat_shake2d_t s;
    OK(sat_shake2d_start(&s, &p) == SAT_OK && sat_shake2d_is_active(&s));
    std::vector<sat_vec2_t> v;
    for (int i = 0; i < 60; ++i) v.push_back(sat_shake2d_step(&s));
    OK(sat_shake2d_is_active(&s));
    OK(peak(v, 0, 60) <= 4.0 + 1e-3 && peak(v, 0, 60) > 3.5);
    const sat_vec2_t end = sat_shake2d_step(&s);
    OK(end.x == 0 && end.y == 0 && !sat_shake2d_is_active(&s));
    OK(sat_shake2d_step(&s).x == 0);
    /* the sine goes through zero and Y trails X by a quarter turn */
    OK(near(v[0].x, 0, 1e-3) && near(v[0].y, -4, 0.01));

    /* linear: fades towards zero */
    p.envelope = SAT_SHAKE2D_LINEAR;
    p.waveform = SAT_SHAKE2D_NOISE;
    p.seed = 77;
    v = run_shake(p, 60);
    OK(peak(v, 0, 15) > peak(v, 45, 60));
    OK(peak(v, 55, 60) < 1.0);
    OK(peak(v, 0, 60) <= 4.0 + 1e-3);

    /* step: loses `decay` px of amplitude every step and ends on its own */
    p = shake_params();
    p.envelope = SAT_SHAKE2D_STEP;
    p.waveform = SAT_SHAKE2D_NOISE;
    p.seed = 5;
    p.decay = F(1);
    p.duration = 0;
    OK(sat_shake2d_start(&s, &p) == SAT_OK);
    for (int n = 0; n < 4; ++n) {
        const sat_vec2_t o = sat_shake2d_step(&s);
        OK(std::fabs(D(o.x)) <= 4.0 - n + 1e-3 && std::fabs(D(o.y)) <= 4.0 - n + 1e-3);
        OK(sat_shake2d_is_active(&s));
    }
    OK(sat_shake2d_step(&s).x == 0 && !sat_shake2d_is_active(&s));

    /* constant with no duration runs until stopped */
    p = shake_params();
    p.duration = 0;
    OK(sat_shake2d_start(&s, &p) == SAT_OK);
    for (int i = 0; i < 500; ++i) sat_shake2d_step(&s);
    OK(sat_shake2d_is_active(&s));
    sat_shake2d_stop(&s);
    OK(!sat_shake2d_is_active(&s) && s.offset.x == 0 && sat_shake2d_step(&s).y == 0);
}

static void shake_shaping() {
    sat_shake2d_params_t p = shake_params();
    p.waveform = SAT_SHAKE2D_NOISE;
    p.seed = 1234;
    p.duration = 200;

    /* an axis with no amplitude stays put */
    p.amplitude_y = 0;
    std::vector<sat_vec2_t> v = run_shake(p, 100);
    bool moved = false;
    for (const sat_vec2_t& o : v) {
        OK(o.y == 0);
        moved = moved || o.x != 0;
    }
    OK(moved);
    p.amplitude_y = F(4);

    /* the sign constraint keeps the offset on one side */
    p.sign = SAT_SHAKE2D_POSITIVE;
    v = run_shake(p, 100);
    for (const sat_vec2_t& o : v) OK(o.x >= 0 && o.y >= 0);
    p.sign = SAT_SHAKE2D_NEGATIVE;
    v = run_shake(p, 100);
    for (const sat_vec2_t& o : v) OK(o.x <= 0 && o.y <= 0);
    p.sign = SAT_SHAKE2D_BOTH;

    /* update_every holds the value between recomputations */
    p.update_every = 4;
    v = run_shake(p, 40);
    int changes = 0;
    for (size_t i = 1; i < v.size(); ++i) {
        const bool same = v[i].x == v[i - 1].x && v[i].y == v[i - 1].y;
        if (i % 4 != 0) OK(same);
        else if (!same) ++changes;
    }
    OK(changes >= 5);
    p.update_every = 1;

    /* same seed, same shake; another seed, another one; zero seed still works */
    const std::vector<sat_vec2_t> a = run_shake(p, 50), b = run_shake(p, 50);
    for (size_t i = 0; i < a.size(); ++i) OK(a[i].x == b[i].x && a[i].y == b[i].y);
    p.seed = 99;
    const std::vector<sat_vec2_t> c = run_shake(p, 50);
    bool differs = false;
    for (size_t i = 0; i < a.size(); ++i) differs = differs || a[i].x != c[i].x;
    OK(differs);
    p.seed = 0;
    OK(peak(run_shake(p, 50), 0, 50) > 1.0);

    /* the noise is not stuck on one side */
    p.seed = 31;
    v = run_shake(p, 100);
    int neg = 0, pos = 0;
    for (const sat_vec2_t& o : v) { neg += o.x < 0; pos += o.x > 0; }
    OK(neg > 20 && pos > 20);

    /* bad parameters */
    sat_shake2d_t s;
    p = shake_params();
    p.amplitude_x = -1;
    OK(sat_shake2d_start(&s, &p) == SAT_ERR_INVALID_ARG);
    p = shake_params(); p.decay = -1;
    OK(sat_shake2d_start(&s, &p) == SAT_ERR_INVALID_ARG);
    p = shake_params(); p.waveform = 7;
    OK(sat_shake2d_start(&s, &p) == SAT_ERR_INVALID_ARG);
    p = shake_params(); p.envelope = SAT_SHAKE2D_LINEAR; p.duration = 0;
    OK(sat_shake2d_start(&s, &p) == SAT_ERR_INVALID_ARG);
    p = shake_params(); p.sign = 9;
    OK(sat_shake2d_start(&s, &p) == SAT_ERR_INVALID_ARG);
    OK(sat_shake2d_start(nullptr, &p) == SAT_ERR_INVALID_ARG && sat_shake2d_start(&s, nullptr) == SAT_ERR_INVALID_ARG);
    OK(sat_shake2d_step(nullptr).x == 0 && !sat_shake2d_is_active(nullptr));
}

static void shake_on_camera() {
    sat_follow_camera2d_t shaken = make(config(), 500, 300), steady = make(config(), 500, 300);
    sat_shake2d_params_t p = shake_params();
    p.waveform = SAT_SHAKE2D_NOISE;
    p.seed = 8;
    OK(sat_follow_camera2d_shake(&shaken, &p) == SAT_OK);
    bool trembled = false;
    for (int i = 0; i < 30; ++i) {
        const sat_camera2d_t a = step(shaken, 520, 300), b = step(steady, 520, 300);
        /* gameplay state is untouched by the shake ... */
        OK(shaken.centre.x == steady.centre.x && shaken.centre.y == steady.centre.y);
        const sat_box2_t sa = range(shaken, SAT_CAMERA_RANGE_ACTIVATION), sb = range(steady, SAT_CAMERA_RANGE_ACTIVATION);
        OK(sa.center.x == sb.center.x && sa.half.x == sb.half.x && sa.center.y == sb.center.y);
        /* ... only the camera handed to the renderer moves */
        OK(a.target_x - b.target_x == shaken.shake_offset.x && a.target_y - b.target_y == shaken.shake_offset.y);
        const sat_box2_t view = range(shaken, SAT_CAMERA_RANGE_VIEW), ref = range(steady, SAT_CAMERA_RANGE_VIEW);
        OK(view.center.x - ref.center.x == shaken.shake_offset.x);
        trembled = trembled || shaken.shake_offset.x != 0;
    }
    OK(trembled);
    for (int i = 0; i < 40; ++i) step(shaken, 520, 300);
    OK(shaken.shake_offset.x == 0 && shaken.shake_offset.y == 0 && !sat_shake2d_is_active(&shaken.shake));
    OK(sat_follow_camera2d_shake(nullptr, &p) == SAT_ERR_INVALID_ARG);
}

static void pixel_snap() {
    sat_follow_camera2d_config_t c = config();
    c.flags = SAT_FOLLOW_CAMERA2D_PIXEL_SNAP;
    sat_follow_camera2d_t cam = make(c, 100.37, 50.61);
    sat_shake2d_params_t p = shake_params();
    p.waveform = SAT_SHAKE2D_NOISE;
    p.seed = 4;
    OK(sat_follow_camera2d_shake(&cam, &p) == SAT_OK);
    for (int i = 0; i < 20; ++i) {
        const sat_camera2d_t out = step(cam, 100.37 + i * 0.3, 50.61);
        OK((out.offset_x & 0xFFFF) == 0 && (out.offset_y & 0xFFFF) == 0);
        OK((out.target_x & 0xFFFF) == 0 && (out.target_y & 0xFFFF) == 0);
        const sat_box2_t view = range(cam, SAT_CAMERA_RANGE_VIEW);
        OK(((view.center.x - view.half.x) & 0xFFFF) == 0 && ((view.center.y - view.half.y) & 0xFFFF) == 0);
    }
    /* the whole-pixel window is within half a pixel of the exact one */
    cam = make(c, 100.37, 50.61);
    const sat_camera2d_t out = step(cam, 100.37, 50.61);
    OK(near(out.target_x - out.offset_x, 100.37 - 160, 0.5) && near(out.target_y - out.offset_y, 50.61 - 112, 0.5));
}

static void ranges() {
    sat_follow_camera2d_t cam = make(config(), 500, 300);
    sat_box2_t view = range(cam, SAT_CAMERA_RANGE_VIEW);
    OK(at(view.center, 500, 300) && at(view.half, 160, 112));
    sat_box2_t act = range(cam, SAT_CAMERA_RANGE_ACTIVATION);
    OK(at(act.center, 500, 300) && at(act.half, 192, 144)); /* margin 32 */
    sat_box2_t pre = range(cam, SAT_CAMERA_RANGE_PREFETCH);
    OK(at(pre.center, 500, 300) && at(pre.half, 224, 176)); /* margin 64 */

    /* the margins stretch towards where the view is heading, not away from it */
    sat_follow_camera2d_config_t c = config();
    c.predict_steps = 10;
    cam = make(c, 500, 300);
    step(cam, 512, 300); /* centre 504, moving +4 px/step */
    OK(at(cam.delta, 4, 0));
    act = range(cam, SAT_CAMERA_RANGE_ACTIVATION);
    OK(at(act.center, 524, 300) && at(act.half, 212, 144));
    pre = range(cam, SAT_CAMERA_RANGE_PREFETCH);
    OK(at(pre.center, 524, 300) && at(pre.half, 244, 176));
    step(cam, 400, 300); /* now heading left, fast: -96 + 8 */
    act = range(cam, SAT_CAMERA_RANGE_ACTIVATION);
    const double dx = D(cam.delta.x);
    OK(dx < 0);
    OK(near(act.center.x - act.half.x, D(cam.centre.x) - 160 - 32 + dx * 10, 0.02));
    OK(near(act.center.x + act.half.x, D(cam.centre.x) + 160 + 32, 0.02));
    /* vertical motion likewise */
    cam = make(c, 500, 300);
    step(cam, 500, 340); /* zone edge 316: moves 24 */
    act = range(cam, SAT_CAMERA_RANGE_ACTIVATION);
    OK(near(act.center.y + act.half.y, 324 + 112 + 32 + 240, 0.02));
    OK(near(act.center.y - act.half.y, 324 - 112 - 32, 0.02));

    /* standing still adds nothing */
    step(cam, 500, 340);
    act = range(cam, SAT_CAMERA_RANGE_ACTIVATION);
    OK(at(act.half, 192, 144));

    /* overlap tests: edges that only touch are outside */
    cam = make(config(), 500, 300);
    sat_box2_t b = {V(500 + 160 + 8, 300), V(8, 8)};
    OK(!sat_follow_camera2d_overlaps(&cam, SAT_CAMERA_RANGE_VIEW, &b));
    OK(sat_follow_camera2d_overlaps(&cam, SAT_CAMERA_RANGE_ACTIVATION, &b));
    b.center.x -= 1;
    OK(sat_follow_camera2d_overlaps(&cam, SAT_CAMERA_RANGE_VIEW, &b));
    b.center = V(500, 300 - 112 - 8);
    OK(!sat_follow_camera2d_overlaps(&cam, SAT_CAMERA_RANGE_VIEW, &b));
    b.center = V(5000, 5000);
    OK(!sat_follow_camera2d_overlaps(&cam, SAT_CAMERA_RANGE_PREFETCH, &b));
    OK(!sat_follow_camera2d_overlaps(&cam, SAT_CAMERA_RANGE_VIEW, nullptr));
    OK(!sat_follow_camera2d_overlaps(nullptr, SAT_CAMERA_RANGE_VIEW, &b));
    sat_box2_t out;
    OK(sat_follow_camera2d_range(&cam, static_cast<sat_camera_range_t>(9), &out) == SAT_ERR_INVALID_ARG);
    OK(sat_follow_camera2d_range(nullptr, SAT_CAMERA_RANGE_VIEW, &out) == SAT_ERR_INVALID_ARG);
}

static void determinism() {
    sat_follow_camera2d_config_t c = config();
    c.follow_x = F(0.3);
    c.follow_y = F(0.6);
    c.look_max_x = F(30);
    c.look_gain_x = F(5);
    c.look_ease = F(0.2);
    c.max_step_y = F(6);
    c.predict_steps = 8;
    const sat_camera_bounds2_t world = {F(0), F(0), F(3000), F(900)};
    sat_shake2d_params_t p = shake_params();
    p.waveform = SAT_SHAKE2D_NOISE;
    p.seed = 21;
    p.duration = 90;
    sat_follow_camera2d_t a = make(c), b = make(c);
    OK(sat_follow_camera2d_set_bounds(&a, &world) == SAT_OK && sat_follow_camera2d_set_bounds(&b, &world) == SAT_OK);
    sat_follow_camera2d_shake(&a, &p);
    sat_follow_camera2d_shake(&b, &p);
    for (int i = 0; i < 300; ++i) {
        const double tx = 160 + i * 7.3, ty = 112 + 80 * std::sin(i * 0.05);
        const sat_vec2_t v = V(7.3, 0);
        const sat_camera2d_t ca = step(a, tx, ty, &v), cb = step(b, tx, ty, &v);
        OK(ca.target_x == cb.target_x && ca.target_y == cb.target_y);
        OK(ca.offset_x == cb.offset_x && ca.offset_y == cb.offset_y);
    }
}

int main() {
    config_checks();
    camera_output();
    dead_zone();
    smoothing();
    look_ahead();
    world_bounds();
    clamps();
    shake_envelopes();
    shake_shaping();
    shake_on_camera();
    pixel_snap();
    ranges();
    determinism();
    std::puts("follow_camera2d: ok");
    return 0;
}
