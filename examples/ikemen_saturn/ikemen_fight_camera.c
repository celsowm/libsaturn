/* Camera: horizontal tension tracking and the player bounds it implies.
 *
 * Mirrors upstream Ikemen GO `Camera.action` for the default configuration
 * (zoom disabled, vertical scroll not simulated): the camera follows the
 * outermost fighters once they come within `tension` of the view edge, is
 * smoothed, snapped to 1/4 px and bounded by the stage. The resulting
 * `xmin/xmax` bound the fighters after hit detection (`xScreenBound`).
 *
 * Camera values are stage pixels in Q16.16 (0 = stage centre); fighters live
 * in Q8.8 with the stage centre at IK_STAGE_CENTER_X. */
#include "ikemen_fight_internal.h"

#define Q16 65536
#define HALF_W ((int32_t)(IK_SCREEN_W / 2) * Q16)

static int32_t min32(int32_t a, int32_t b) { return a < b ? a : b; }
static int32_t max32(int32_t a, int32_t b) { return a > b ? a : b; }
static int32_t abs32(int32_t a) { return a < 0 ? -a : a; }

static int32_t world_q16(int32_t x_q8) {
    return (x_q8 - (int32_t)IK_STAGE_CENTER_X * IK_CNS_Q8_ONE) * 256;
}

static int32_t to_x_q8(int32_t world) {
    return (world >> 8) + (int32_t)IK_STAGE_CENTER_X * IK_CNS_Q8_ONE;
}

typedef struct ik_cam_track {
    int32_t leftest, rightest;
    int32_t leftest_vel, rightest_vel;
} ik_cam_track_t;

/* Velocity a tracked fighter contributes: its own when it moved this tick,
 * its binder's when bound, else none. */
static int32_t tracked_velocity(const ik_fight_t* fight, const ik_fighter_t* f) {
    if (f->hit_pause == 0u && f->bound_to < 0 &&
        !ik_entity_handle_is_valid(f->bound_entity)) {
        return f->vx_q8 * 256;
    }
    if (f->bound_to >= 0 && f->bound_to < 2) {
        return fight->fighters[f->bound_to].vx_q8 * 256;
    }
    return 0;
}

static ik_cam_track_t track_fighters(const ik_fight_t* fight) {
    ik_cam_track_t t = {0x7fffffff, -0x7fffffff, 0, 0};
    for (int i = 0; i < 2; ++i) {
        const ik_fighter_t* f = &fight->fighters[i];
        if (!f->move_camera_x) continue;
        /* The Width controller's edge widths extend the tracked box. */
        const int32_t x = world_q16(f->x_q8);
        const int32_t back = (int32_t)f->edge_back * Q16;
        const int32_t front = (int32_t)f->edge_front * Q16;
        const int32_t left = f->facing > 0 ? x - back : x - front;
        const int32_t right = f->facing > 0 ? x + front : x + back;
        const int32_t vel = tracked_velocity(fight, f);
        if (left < t.leftest) {
            t.leftest = left;
            t.leftest_vel = vel;
        }
        if (right > t.rightest) {
            t.rightest = right;
            t.rightest_vel = vel;
        }
    }
    return t;
}

/* Shrinks a target wider than the view back to the view width. */
static void limit_width(const ik_stage_params_t* st, const ik_cam_track_t* t,
                        int32_t tension, int32_t min_left, int32_t max_right,
                        int32_t* tl, int32_t* tr) {
    int32_t diff = (*tr - *tl) - 2 * HALF_W;
    const int32_t r_left = max32(*tl + tension - t->leftest, 0);
    const int32_t r_right = max32(t->rightest - (*tr - tension), 0);
    if (r_left > r_right) {
        const int32_t take = min32(r_left - r_right, diff);
        *tr -= take;
        diff -= take;
    } else if (r_right > r_left) {
        const int32_t take = min32(r_right - r_left, diff);
        *tl += take;
        diff -= take;
    }
    *tl += diff / 2;
    *tr -= diff / 2;

    const int32_t sl = (int32_t)st->screen_left * Q16;
    const int32_t sr = (int32_t)st->screen_right * Q16;
    if (t->leftest - *tl < sl) {
        int32_t d = min32(sl - (t->leftest - *tl), *tl - min_left);
        if (*tr - t->rightest < sr) {
            d -= min32(sr - (*tr - t->rightest), max_right - *tr);
        }
        *tl -= d;
        *tr -= d;
    } else if (*tr - t->rightest < sr) {
        const int32_t d =
            min32(sr - (*tr - t->rightest), max_right - *tr);
        *tl += d;
        *tr += d;
    }
}

/* Moves one view edge toward its target: 5% easing, a 0.1 px minimum step,
 * and never slower than the fighter pushing that edge. */
static int32_t ease_edge(int32_t now, int32_t old, int32_t target,
                         int32_t left_vel, int32_t right_vel) {
    const int32_t step = Q16 / 10;
    now += (int32_t)(((int64_t)(target - now) * 3277) >> 16);
    const int32_t diff = target - now;
    if (abs32(diff) <= step) now = target;
    else if (diff > 0) now += step;
    else now -= step;
    if (now - old > 0 && now - old < right_vel) {
        now = min32(old + right_vel, target);
    } else if (now - old < 0 && now - old > left_vel) {
        now = max32(old + left_vel, target);
    }
    return now;
}

/* Upstream drops the camera x to 1/4 px: ceil(x * 4 - 0.5) / 4. */
static int32_t snap_quarter(int32_t x) {
    const int64_t t = (int64_t)x * 4 - Q16 / 2;
    int64_t c = t / Q16;
    if (t % Q16 > 0) ++c;
    return (int32_t)(c * Q16 / 4);
}

static void apply_bounds(ik_fight_t* fight, int32_t min_left, int32_t max_right) {
    const ik_stage_params_t* st = &fight->stage;
    int32_t xmin = fight->cam_x_q16 - fight->cam_half_q16 +
                   (int32_t)st->screen_left * Q16;
    int32_t xmax = fight->cam_x_q16 + fight->cam_half_q16 -
                   (int32_t)st->screen_right * Q16;
    if (xmin > xmax) xmin = xmax = (xmin + xmax) / 2;
    if (abs32(max_right - xmax) < 7) xmax = max_right;
    if (abs32(min_left - xmin) < 7) xmin = min_left;
    fight->xmin_q8 = to_x_q8(xmin);
    fight->xmax_q8 = to_x_q8(xmax);
    for (int i = 0; i < 2; ++i) {
        fight->fighters[i].xmin_q8 = fight->xmin_q8;
        fight->fighters[i].xmax_q8 = fight->xmax_q8;
    }
}

void ikf_camera_reset(ik_fight_t* fight) {
    if (!fight) return;
    fight->cam_x_q16 = 0;
    fight->cam_half_q16 = HALF_W;
    fight->cam_skip_smoothing = 1u;
    const ik_stage_params_t* st = &fight->stage;
    apply_bounds(
        fight,
        (int32_t)st->bound_left * Q16 - HALF_W,
        (int32_t)st->bound_right * Q16 + HALF_W);
}

int32_t ik_fight_camera_x_q8(const ik_fight_t* fight) {
    return fight ? to_x_q8(fight->cam_x_q16)
                 : (int32_t)IK_STAGE_CENTER_X * IK_CNS_Q8_ONE;
}

/* After hit detection: track, move the camera, then clamp the fighters. */
void ikf_camera_step(ik_fight_t* fight) {
    if (!fight) return;
    const ik_stage_params_t* st = &fight->stage;
    const int32_t tension = (int32_t)st->tension * Q16;
    const int32_t min_left = (int32_t)st->bound_left * Q16 - HALF_W;
    const int32_t max_right = (int32_t)st->bound_right * Q16 + HALF_W;
    const ik_cam_track_t t = track_fighters(fight);

    const int32_t old_l = fight->cam_x_q16 - fight->cam_half_q16;
    const int32_t old_r = fight->cam_x_q16 + fight->cam_half_q16;
    int32_t tl = old_l;
    int32_t tr = old_r;
    if (t.leftest < tl + tension) {
        const int32_t nl = max32(t.leftest - tension, min_left);
        const int32_t diff = tl - nl;
        tl = nl;
        tr = max32(old_r - diff, min32(t.rightest + tension, max_right));
    } else if (t.rightest > tr - tension) {
        const int32_t nr = min32(t.rightest + tension, max_right);
        const int32_t diff = tr - nr;
        tr = nr;
        tl = min32(old_l - diff, max32(t.leftest - tension, min_left));
    }
    if (tr - tl > 2 * HALF_W) {
        limit_width(st, &t, tension, min_left, max_right, &tl, &tr);
    }

    int32_t nl = tl;
    int32_t nr = tr;
    if (!fight->cam_skip_smoothing) {
        nl = old_l;
        nr = old_r;
        for (int i = 0; i < 3; ++i) {
            nl = ease_edge(nl, old_l, tl, t.leftest_vel, t.rightest_vel);
            nr = ease_edge(nr, old_r, tr, t.leftest_vel, t.rightest_vel);
        }
    }
    fight->cam_skip_smoothing = 0u;

    fight->cam_half_q16 = max32((nr - nl) / 2, HALF_W);
    fight->cam_x_q16 = snap_quarter((nl + nr) / 2);
    apply_bounds(fight, min_left, max_right);

    for (int i = 0; i < 2; ++i) {
        ik_fighter_t* f = &fight->fighters[i];
        if (f->screen_bound) {
            /* Facing right: [back + xmin, xmax - front]; mirrored left. */
            const int32_t lo = (f->facing > 0 ? f->edge_back : f->edge_front);
            const int32_t hi = (f->facing > 0 ? f->edge_front : f->edge_back);
            f->x_q8 = max32(fight->xmin_q8 + lo * IK_CNS_Q8_ONE,
                            min32(f->x_q8, fight->xmax_q8 - hi * IK_CNS_Q8_ONE));
        }
        sync_position(f);
    }
}
