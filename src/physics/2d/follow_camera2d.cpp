#include "saturn/follow_camera2d.h"

#include <limits.h>

#include "src/core/math2d/logic.hpp"

/* Follow Camera2D: dead zone, look-ahead, smoothing, bounds, sliding clamps, shake and the
 * visibility/activation/prefetch ranges. The contract is documented in
 * include/saturn/follow_camera2d.h. */

namespace {

namespace m2 = saturn::core::math2d;

constexpr int64_t kOne = SAT_FX16_ONE;
constexpr int64_t kHalfPx = 0x8000;
constexpr uint32_t kDefaultSeed = 0x9E3779B9u;

inline int32_t clamp32(int64_t v) {
    return v > INT32_MAX ? INT32_MAX : (v < INT32_MIN ? INT32_MIN : static_cast<int32_t>(v));
}
inline int64_t mul(int64_t a, int64_t b) { return (a * b) >> 16; }
inline int64_t clamp64(int64_t v, int64_t lo, int64_t hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int64_t round_px(int64_t v) { return ((v + kHalfPx) >> 16) << 16; }
inline int64_t floor_px(int64_t v) { return (v >> 16) << 16; }

/* ----- shake ----- */

uint32_t next_random(uint32_t& state) {
    uint32_t x = state ? state : kDefaultSeed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state = x;
    return x;
}

/* A fresh value in [-ONE, ONE). */
int64_t noise_unit(uint32_t& state) {
    const uint32_t r = next_random(state) >> 8;
    return (static_cast<int64_t>(r & 0xFFFFu) - 32768) * 2;
}

int64_t apply_sign(int64_t unit, uint8_t sign) {
    if (sign == SAT_SHAKE2D_POSITIVE) return unit < 0 ? -unit : unit;
    if (sign == SAT_SHAKE2D_NEGATIVE) return unit < 0 ? unit : -unit;
    return unit;
}

/* ----- follow ----- */

bool valid_bounds(const sat_camera_bounds2_t* b) { return b && b->min_x < b->max_x && b->min_y < b->max_y; }

/* The bounds the view window has to stay in: the world and the clamp together. */
bool effective_bounds(const sat_follow_camera2d_t& cam, sat_camera_bounds2_t& out) {
    if (!cam.has_world && !cam.has_clamp) return false;
    if (!cam.has_clamp) { out = cam.world; return true; }
    if (!cam.has_world) { out = cam.clamp; return true; }
    out = cam.clamp;
    const int32_t lo_x = cam.world.min_x > cam.clamp.min_x ? cam.world.min_x : cam.clamp.min_x;
    const int32_t hi_x = cam.world.max_x < cam.clamp.max_x ? cam.world.max_x : cam.clamp.max_x;
    if (lo_x < hi_x) { out.min_x = lo_x; out.max_x = hi_x; }
    const int32_t lo_y = cam.world.min_y > cam.clamp.min_y ? cam.world.min_y : cam.clamp.min_y;
    const int32_t hi_y = cam.world.max_y < cam.clamp.max_y ? cam.world.max_y : cam.clamp.max_y;
    if (lo_y < hi_y) { out.min_y = lo_y; out.max_y = hi_y; }
    return true;
}

int64_t fit_axis(int64_t centre, int64_t bmin, int64_t bmax, int64_t half) {
    if (bmax - bmin <= 2 * half) return (bmin + bmax) / 2;
    return clamp64(centre, bmin + half, bmax - half);
}

void fit_centre(sat_follow_camera2d_t& cam) {
    sat_camera_bounds2_t b;
    if (!effective_bounds(cam, b)) return;
    cam.centre.x = clamp32(fit_axis(cam.centre.x, b.min_x, b.max_x, cam.config.viewport_w / 2));
    cam.centre.y = clamp32(fit_axis(cam.centre.y, b.min_y, b.max_y, cam.config.viewport_h / 2));
}

int32_t slide_edge(int32_t from, int32_t to, int32_t speed) {
    if (speed <= 0) return to;
    if (to > from) return to - from > speed ? from + speed : to;
    return from - to > speed ? from - speed : to;
}

void slide_clamp(sat_follow_camera2d_t& cam) {
    if (!cam.has_clamp) return;
    const int32_t s = cam.clamp_speed;
    cam.clamp.min_x = slide_edge(cam.clamp.min_x, cam.clamp_goal.min_x, s);
    cam.clamp.min_y = slide_edge(cam.clamp.min_y, cam.clamp_goal.min_y, s);
    cam.clamp.max_x = slide_edge(cam.clamp.max_x, cam.clamp_goal.max_x, s);
    cam.clamp.max_y = slide_edge(cam.clamp.max_y, cam.clamp_goal.max_y, s);
    if (cam.clamp_releasing && cam.clamp.min_x == cam.clamp_goal.min_x && cam.clamp.min_y == cam.clamp_goal.min_y &&
        cam.clamp.max_x == cam.clamp_goal.max_x && cam.clamp.max_y == cam.clamp_goal.max_y) {
        cam.has_clamp = 0;
        cam.clamp_releasing = 0;
    }
}

/* One axis of the dead zone: how far `target` is outside [lo, hi], scaled by the follow
 * fraction and limited by the speed cap. */
int64_t follow_axis(int64_t target, int64_t lo, int64_t hi, int64_t follow, int64_t max_step) {
    int64_t excess = 0;
    if (target > hi) excess = target - hi;
    else if (target < lo) excess = target - lo;
    int64_t move = follow >= kOne ? excess : mul(excess, follow);
    if (max_step > 0) move = clamp64(move, -max_step, max_step);
    return move;
}

int64_t look_axis(int64_t current, int64_t velocity, int64_t gain, int64_t max, int64_t ease) {
    const int64_t desired = clamp64(mul(velocity, gain), -max, max);
    return current + (ease >= kOne ? desired - current : mul(desired - current, ease));
}

/* The window the camera would draw, shake and snapping included: left/top and size. */
struct Window {
    int64_t left, top, w, h;
};

Window view_window(const sat_follow_camera2d_t& cam) {
    const int64_t w = cam.config.viewport_w, h = cam.config.viewport_h;
    int64_t left = static_cast<int64_t>(cam.centre.x) + cam.shake_offset.x - w / 2;
    int64_t top = static_cast<int64_t>(cam.centre.y) + cam.shake_offset.y - h / 2;
    if (cam.config.flags & SAT_FOLLOW_CAMERA2D_PIXEL_SNAP) {
        left = round_px(left);
        top = round_px(top);
    }
    return {left, top, w, h};
}

sat_box2_t box_from_edges(int64_t l, int64_t t, int64_t r, int64_t b) {
    sat_box2_t box;
    box.center.x = clamp32((l + r) >> 1);
    box.center.y = clamp32((t + b) >> 1);
    box.half.x = clamp32((r - l) >> 1);
    box.half.y = clamp32((b - t) >> 1);
    return box;
}

bool valid_config(const sat_follow_camera2d_config_t& c) {
    if (c.viewport_w <= 0 || c.viewport_h <= 0) return false;
    if (c.dead_half_w < 0 || c.dead_half_h < 0) return false;
    if (c.look_max_x < 0 || c.look_max_y < 0 || c.look_gain_x < 0 || c.look_gain_y < 0) return false;
    if (c.look_ease <= 0 || c.look_ease > kOne) return false;
    if (c.follow_x <= 0 || c.follow_x > kOne || c.follow_y <= 0 || c.follow_y > kOne) return false;
    if (c.max_step_x < 0 || c.max_step_y < 0) return false;
    if (c.activation_margin < 0 || c.prefetch_margin < 0) return false;
    return true;
}

}  // namespace

/* ----- shake ----- */

extern "C" sat_result_t sat_shake2d_start(sat_shake2d_t* shake, const sat_shake2d_params_t* params) {
    if (!shake || !params) return SAT_ERR_INVALID_ARG;
    if (params->amplitude_x < 0 || params->amplitude_y < 0 || params->decay < 0) return SAT_ERR_INVALID_ARG;
    if (params->waveform > SAT_SHAKE2D_NOISE || params->envelope > SAT_SHAKE2D_STEP ||
        params->sign > SAT_SHAKE2D_NEGATIVE) return SAT_ERR_INVALID_ARG;
    if (params->envelope == SAT_SHAKE2D_LINEAR && params->duration == 0) return SAT_ERR_INVALID_ARG;
    shake->params = *params;
    shake->offset = {0, 0};
    shake->rng = params->seed;
    shake->phase = static_cast<sat_angle16_t>(params->seed);
    shake->elapsed = 0;
    shake->active = 1;
    return SAT_OK;
}

extern "C" void sat_shake2d_stop(sat_shake2d_t* shake) {
    if (!shake) return;
    shake->active = 0;
    shake->offset = {0, 0};
}

extern "C" int sat_shake2d_is_active(const sat_shake2d_t* shake) { return shake && shake->active ? 1 : 0; }

extern "C" sat_vec2_t sat_shake2d_step(sat_shake2d_t* shake) {
    if (!shake || !shake->active) return {0, 0};
    const sat_shake2d_params_t& p = shake->params;
    const uint32_t n = shake->elapsed;

    int64_t ax = p.amplitude_x, ay = p.amplitude_y;
    int64_t scale = kOne;
    bool over = p.duration != 0 && n >= p.duration;
    if (p.envelope == SAT_SHAKE2D_LINEAR) {
        scale = over ? 0 : kOne - static_cast<int64_t>(n) * kOne / p.duration;
    } else if (p.envelope == SAT_SHAKE2D_STEP) {
        const int64_t lost = static_cast<int64_t>(p.decay) * static_cast<int64_t>(n);
        ax = ax > lost ? ax - lost : 0;
        ay = ay > lost ? ay - lost : 0;
        if (p.decay > 0 && ax == 0 && ay == 0) over = true;
    }
    if (over) {
        shake->active = 0;
        shake->offset = {0, 0};
        return {0, 0};
    }

    const uint32_t every = p.update_every > 1 ? p.update_every : 1u;
    if (n % every == 0) {
        int64_t ux, uy;
        if (p.waveform == SAT_SHAKE2D_NOISE) {
            ux = noise_unit(shake->rng);
            uy = noise_unit(shake->rng);
        } else {
            ux = m2::sin16(shake->phase);
            uy = m2::sin16(static_cast<sat_angle16_t>(shake->phase - 16384u));
        }
        ux = apply_sign(ux, p.sign);
        uy = apply_sign(uy, p.sign);
        shake->offset.x = clamp32(mul(mul(ax, ux), scale));
        shake->offset.y = clamp32(mul(mul(ay, uy), scale));
    }
    shake->phase = static_cast<sat_angle16_t>(shake->phase + p.frequency);
    if (shake->elapsed < UINT32_MAX) ++shake->elapsed;
    return shake->offset;
}

/* ----- config ----- */

extern "C" void sat_follow_camera2d_config_default(sat_follow_camera2d_config_t* config) {
    if (!config) return;
    *config = {};
    config->viewport_w = 320 * SAT_FX16_ONE;
    config->viewport_h = 224 * SAT_FX16_ONE;
    config->dead_half_w = 8 * SAT_FX16_ONE;
    config->dead_half_h = 16 * SAT_FX16_ONE;
    config->look_ease = SAT_FX16_ONE / 8;
    config->follow_x = SAT_FX16_ONE;
    config->follow_y = SAT_FX16_ONE;
    config->activation_margin = 32 * SAT_FX16_ONE;
    config->prefetch_margin = 64 * SAT_FX16_ONE;
}

extern "C" sat_result_t sat_follow_camera2d_config_validate(const sat_follow_camera2d_config_t* config) {
    return config && valid_config(*config) ? SAT_OK : SAT_ERR_INVALID_ARG;
}

extern "C" sat_result_t sat_follow_camera2d_init(sat_follow_camera2d_t* cam, const sat_follow_camera2d_config_t* config,
    sat_vec2_t centre) {
    if (!cam || !config || !valid_config(*config)) return SAT_ERR_INVALID_ARG;
    *cam = {};
    cam->config = *config;
    cam->centre = centre;
    return SAT_OK;
}

/* ----- bounds and clamps ----- */

extern "C" sat_result_t sat_follow_camera2d_set_bounds(sat_follow_camera2d_t* cam, const sat_camera_bounds2_t* bounds) {
    if (!cam) return SAT_ERR_INVALID_ARG;
    if (!bounds) {
        cam->has_world = 0;
        if (cam->clamp_releasing) { cam->has_clamp = 0; cam->clamp_releasing = 0; }
        return SAT_OK;
    }
    if (!valid_bounds(bounds)) return SAT_ERR_INVALID_ARG;
    cam->world = *bounds;
    cam->has_world = 1;
    if (cam->clamp_releasing) cam->clamp_goal = *bounds;
    return SAT_OK;
}

extern "C" sat_result_t sat_follow_camera2d_set_clamp(sat_follow_camera2d_t* cam, const sat_camera_bounds2_t* bounds,
    sat_fx16_t slide_speed) {
    if (!cam || !valid_bounds(bounds) || slide_speed < 0) return SAT_ERR_INVALID_ARG;
    if (!cam->has_clamp) {
        if (cam->has_world) {
            cam->clamp = cam->world;
        } else {
            const Window w = view_window(*cam);
            cam->clamp = {clamp32(w.left), clamp32(w.top), clamp32(w.left + w.w), clamp32(w.top + w.h)};
        }
    }
    cam->clamp_goal = *bounds;
    cam->clamp_speed = slide_speed;
    cam->clamp_releasing = 0;
    cam->has_clamp = 1;
    if (slide_speed == 0) cam->clamp = *bounds;
    return SAT_OK;
}

extern "C" sat_result_t sat_follow_camera2d_clear_clamp(sat_follow_camera2d_t* cam, sat_fx16_t slide_speed) {
    if (!cam || slide_speed < 0) return SAT_ERR_INVALID_ARG;
    if (!cam->has_clamp) return SAT_OK;
    if (!cam->has_world || slide_speed == 0) {
        cam->has_clamp = 0;
        cam->clamp_releasing = 0;
        return SAT_OK;
    }
    cam->clamp_goal = cam->world;
    cam->clamp_speed = slide_speed;
    cam->clamp_releasing = 1;
    return SAT_OK;
}

/* ----- stepping ----- */

extern "C" sat_result_t sat_follow_camera2d_snap(sat_follow_camera2d_t* cam, sat_vec2_t target) {
    if (!cam) return SAT_ERR_INVALID_ARG;
    cam->centre.x = clamp32(static_cast<int64_t>(target.x) - cam->config.shift_x);
    cam->centre.y = clamp32(static_cast<int64_t>(target.y) - cam->config.shift_y);
    cam->look = {0, 0};
    cam->delta = {0, 0};
    fit_centre(*cam);
    return SAT_OK;
}

extern "C" sat_result_t sat_follow_camera2d_shake(sat_follow_camera2d_t* cam, const sat_shake2d_params_t* params) {
    if (!cam) return SAT_ERR_INVALID_ARG;
    return sat_shake2d_start(&cam->shake, params);
}

extern "C" sat_result_t sat_follow_camera2d_step(sat_follow_camera2d_t* cam, sat_vec2_t target,
    const sat_vec2_t* velocity, sat_camera2d_t* out) {
    if (!cam) return SAT_ERR_INVALID_ARG;
    const sat_follow_camera2d_config_t& c = cam->config;
    const sat_vec2_t before = cam->centre;

    const sat_vec2_t v = velocity ? *velocity : sat_vec2_t{0, 0};
    cam->look.x = clamp32(look_axis(cam->look.x, v.x, c.look_gain_x, c.look_max_x, c.look_ease));
    cam->look.y = clamp32(look_axis(cam->look.y, v.y, c.look_gain_y, c.look_max_y, c.look_ease));

    slide_clamp(*cam);

    const int64_t tx = static_cast<int64_t>(target.x) + cam->look.x;
    const int64_t ty = static_cast<int64_t>(target.y) + cam->look.y;
    const int64_t zx = static_cast<int64_t>(cam->centre.x) + c.shift_x;
    const int64_t zy = static_cast<int64_t>(cam->centre.y) + c.shift_y;
    cam->centre.x = clamp32(static_cast<int64_t>(cam->centre.x) +
        follow_axis(tx, zx - c.dead_half_w, zx + c.dead_half_w, c.follow_x, c.max_step_x));
    cam->centre.y = clamp32(static_cast<int64_t>(cam->centre.y) +
        follow_axis(ty, zy - c.dead_half_h, zy + c.dead_half_h, c.follow_y, c.max_step_y));
    fit_centre(*cam);

    cam->delta.x = clamp32(static_cast<int64_t>(cam->centre.x) - before.x);
    cam->delta.y = clamp32(static_cast<int64_t>(cam->centre.y) - before.y);
    cam->shake_offset = sat_shake2d_step(&cam->shake);
    return out ? sat_follow_camera2d_get(cam, out) : SAT_OK;
}

extern "C" sat_result_t sat_follow_camera2d_get(const sat_follow_camera2d_t* cam, sat_camera2d_t* out) {
    if (!cam || !out) return SAT_ERR_INVALID_ARG;
    const Window w = view_window(*cam);
    /* The screen point the target lands on is the middle of the viewport; with snapping it is a whole
     * pixel so the window's left and top stay whole pixels. */
    int64_t half_w = w.w / 2, half_h = w.h / 2;
    if (cam->config.flags & SAT_FOLLOW_CAMERA2D_PIXEL_SNAP) {
        half_w = floor_px(half_w);
        half_h = floor_px(half_h);
    }
    out->offset_x = clamp32(half_w);
    out->offset_y = clamp32(half_h);
    out->target_x = clamp32(w.left + half_w);
    out->target_y = clamp32(w.top + half_h);
    out->rotation = 0;
    out->zoom = SAT_FX16_ONE;
    return SAT_OK;
}

/* ----- ranges ----- */

extern "C" sat_result_t sat_follow_camera2d_range(const sat_follow_camera2d_t* cam, sat_camera_range_t range,
    sat_box2_t* out) {
    if (!cam || !out) return SAT_ERR_INVALID_ARG;
    if (range == SAT_CAMERA_RANGE_VIEW) {
        const Window w = view_window(*cam);
        *out = box_from_edges(w.left, w.top, w.left + w.w, w.top + w.h);
        return SAT_OK;
    }
    if (range != SAT_CAMERA_RANGE_ACTIVATION && range != SAT_CAMERA_RANGE_PREFETCH) return SAT_ERR_INVALID_ARG;
    const sat_follow_camera2d_config_t& c = cam->config;
    const int64_t margin = range == SAT_CAMERA_RANGE_ACTIVATION ? c.activation_margin : c.prefetch_margin;
    int64_t l = static_cast<int64_t>(cam->centre.x) - c.viewport_w / 2 - margin;
    int64_t t = static_cast<int64_t>(cam->centre.y) - c.viewport_h / 2 - margin;
    int64_t r = static_cast<int64_t>(cam->centre.x) + (c.viewport_w - c.viewport_w / 2) + margin;
    int64_t b = static_cast<int64_t>(cam->centre.y) + (c.viewport_h - c.viewport_h / 2) + margin;
    const int64_t px = static_cast<int64_t>(cam->delta.x) * c.predict_steps;
    const int64_t py = static_cast<int64_t>(cam->delta.y) * c.predict_steps;
    if (px > 0) r += px; else l += px;
    if (py > 0) b += py; else t += py;
    *out = box_from_edges(l, t, r, b);
    return SAT_OK;
}

extern "C" int sat_follow_camera2d_overlaps(const sat_follow_camera2d_t* cam, sat_camera_range_t range,
    const sat_box2_t* box) {
    sat_box2_t r;
    if (!box || sat_follow_camera2d_range(cam, range, &r) != SAT_OK) return 0;
    const int64_t dx = static_cast<int64_t>(r.center.x) - box->center.x;
    const int64_t dy = static_cast<int64_t>(r.center.y) - box->center.y;
    const int64_t hx = static_cast<int64_t>(r.half.x) + box->half.x;
    const int64_t hy = static_cast<int64_t>(r.half.y) + box->half.y;
    return (dx < hx && -dx < hx && dy < hy && -dy < hy) ? 1 : 0;
}

extern "C" sat_vec2_t sat_follow_camera2d_world_to_screen(const sat_follow_camera2d_t* cam, sat_vec2_t world) {
    sat_camera2d_t c;
    if (sat_follow_camera2d_get(cam, &c) != SAT_OK) return {0, 0};
    return {clamp32(static_cast<int64_t>(world.x) - c.target_x + c.offset_x),
            clamp32(static_cast<int64_t>(world.y) - c.target_y + c.offset_y)};
}

extern "C" sat_vec2_t sat_follow_camera2d_screen_to_world(const sat_follow_camera2d_t* cam, sat_vec2_t screen) {
    sat_camera2d_t c;
    if (sat_follow_camera2d_get(cam, &c) != SAT_OK) return {0, 0};
    return {clamp32(static_cast<int64_t>(screen.x) - c.offset_x + c.target_x),
            clamp32(static_cast<int64_t>(screen.y) - c.offset_y + c.target_y)};
}
