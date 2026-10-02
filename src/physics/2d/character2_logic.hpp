#ifndef SATURN_PHYSICS_CHARACTER2_LOGIC_HPP
#define SATURN_PHYSICS_CHARACTER2_LOGIC_HPP

/* Pure, host-testable Character2 stepping. include/saturn/character2.h wraps it.
 *
 * Conventions used below:
 *   - a "quadrant" q is 0 = +X, 1 = +Y, 2 = -X, 3 = -Y (the same numbering as
 *     Terrain2 probe directions); q & 1 is its axis, q < 2 its sign;
 *   - `down_q` is the quadrant of the support's far side (into the solid);
 *   - a position component p (16.16) belongs to the pixel `free_pixel(p, sign)`
 *     along a probe axis: a point exactly on a surface boundary is attributed to
 *     the free side, so a character standing on a floor probes from the pixel above. */

#include <limits.h>
#include <stdint.h>

#include "saturn/character2.h"
#include "src/physics/2d/terrain2_logic.hpp"

namespace saturn::physics::character2 {

namespace t2 = saturn::physics::terrain2;
namespace m2 = saturn::core::math2d;

constexpr int kDx[4] = {1, 0, -1, 0};
constexpr int kDy[4] = {0, 1, 0, -1};

inline int32_t comp(const sat_vec2_t& v, int axis) { return axis == 0 ? v.x : v.y; }
inline int32_t& comp(sat_vec2_t& v, int axis) { return axis == 0 ? v.x : v.y; }
inline int quad_axis(int q) { return q & 1; }
inline int quad_sign(int q) { return q < 2 ? 1 : -1; }
inline int32_t abs32(int32_t v) { return v < 0 ? -v : v; }
inline int32_t px16(int px) { return static_cast<int32_t>(px) * 65536; }

inline int32_t free_pixel(int32_t p16, int sign) { return sign > 0 ? (p16 - 1) >> 16 : p16 >> 16; }

/* Mode of a surface by its angle: the quadrant of the travel direction, with the
 * 45 degree angles counting as floor on both sides. */
inline int mode_quadrant(uint8_t a) {
    if (a <= 32 || a >= 224) return 0;
    if (a <= 95) return 1;
    if (a <= 160) return 2;
    return 3;
}

inline sat_terrain_query2_t make_query(const sat_character2_t& ch, const sat_character2_config_t& cfg) {
    sat_terrain_query2_t q = sat_terrain_query2_default();
    q.layer = ch.layer;
    q.category_mask = cfg.category_mask;
    q.ignore_flags = cfg.ignore_flags;
    return q;
}

inline sat_result_t config_validate(const sat_character2_config_t* c) {
    if (!c) return SAT_ERR_INVALID_ARG;
    if (c->segment_px < 1 || c->segment_px > 8) return SAT_ERR_INVALID_ARG;
    if (c->max_segments < 1 || c->max_segments > 32) return SAT_ERR_INVALID_ARG;
    if (c->gravity_quadrant > 3) return SAT_ERR_INVALID_ARG;
    if (c->foot_half_width > 64 || c->wall_radius > 64 || c->wall_height > 64 ||
        c->head_height > 128 || c->step_up > 64 || c->snap_down > 64)
        return SAT_ERR_INVALID_ARG;
    if (c->ceiling_attach > 64) return SAT_ERR_INVALID_ARG;
    return SAT_OK;
}

inline void set_support(sat_character2_t& ch, uint8_t angle, uint32_t id) {
    ch.support_angle = angle;
    ch.support_tangent = {m2::cos8(angle), m2::sin8(angle)};
    ch.support_normal = {m2::sin8(angle), static_cast<sat_fx16_t>(-m2::cos8(angle))};
    ch.support_id = id;
}

inline sat_vec2_t world_velocity(const sat_character2_t& ch) {
    if (!(ch.flags & SAT_CHARACTER2_SUPPORTED)) return ch.air_velocity;
    return {static_cast<sat_fx16_t>((static_cast<int64_t>(ch.support_tangent.x) * ch.ground_speed) >> 16),
            static_cast<sat_fx16_t>((static_cast<int64_t>(ch.support_tangent.y) * ch.ground_speed) >> 16)};
}

inline void detach(sat_character2_t& ch) {
    if (!(ch.flags & SAT_CHARACTER2_SUPPORTED)) return;
    ch.air_velocity = world_velocity(ch);
    ch.ground_speed = 0;
    ch.flags = static_cast<uint8_t>(ch.flags & ~SAT_CHARACTER2_SUPPORTED);
    ch.support_id = SAT_CHARACTER2_NO_SUPPORT;
}

/* Feet pixel for probes in direction `q`: the free-side pixel along q's axis, the
 * pixel containing the point along the other. */
inline void feet_pixel(const sat_vec2_t& pos, int q, int32_t& x, int32_t& y) {
    const int axis = quad_axis(q);
    x = axis == 0 ? free_pixel(pos.x, quad_sign(q)) : (pos.x >> 16);
    y = axis == 1 ? free_pixel(pos.y, quad_sign(q)) : (pos.y >> 16);
}

struct SupportHit {
    bool found;
    int32_t edge;      /* contact boundary along the down axis, 16.16 */
    int32_t distance;  /* whole pixels, as returned by the probe */
    uint8_t angle;
    uint32_t id;
};

/* Two sensors `half` pixels either side of the centre line probe along `down_q`
 * from `lift` pixels above the feet; the smaller distance (the nearer, higher
 * surface) wins. Only results with distance >= -max_depth count. */
inline SupportHit find_support(const sat_terrain_map2_t* map, const sat_terrain_query2_t& q,
                               const sat_vec2_t& pos, int down_q, int half, int lift,
                               int range, int max_depth) {
    SupportHit best = {false, 0, INT_MAX, 0, 0};
    int32_t fx, fy;
    feet_pixel(pos, down_q, fx, fy);
    const int across = quad_axis(down_q) ^ 1;
    for (int s = -1; s <= 1; s += 2) {
        int32_t ox = fx - kDx[down_q] * lift;
        int32_t oy = fy - kDy[down_q] * lift;
        if (across == 0) ox += s * half;
        else oy += s * half;
        sat_terrain_hit2_t hit;
        if (t2::probe(map, ox, oy, static_cast<uint8_t>(down_q), range, &q, &hit) != SAT_OK) continue;
        const int32_t d = hit.distance >> 16;
        if (d < -max_depth || d >= best.distance) continue;
        best.found = true;
        best.distance = d;
        best.edge = comp(hit.point, quad_axis(down_q));
        best.angle = hit.angle;
        best.id = hit.collider_id;
    }
    if (!best.found) return best;

    /* The contact point is where the surface crosses the centre line, not where the
     * higher foot touches it: on a slope the two differ by up to the foot half-width. A
     * ledge under only one foot has no surface at the centre, and keeps the foot's. */
    int32_t cx = fx - kDx[down_q] * lift;
    int32_t cy = fy - kDy[down_q] * lift;
    sat_terrain_hit2_t hit;
    if (t2::probe(map, cx, cy, static_cast<uint8_t>(down_q), range, &q, &hit) == SAT_OK) {
        const int32_t d = hit.distance >> 16;
        const int32_t edge = comp(hit.point, quad_axis(down_q));
        if (d >= -max_depth && abs32(edge - best.edge) <= px16(half)) best.edge = edge;
    }
    return best;
}

/* Wall sensor. Looks along `fwd_q` from `height` pixels above the feet (against
 * `down_q`) and decides whether moving `move` (16.16, along the fwd axis) would push
 * the body edge, `radius` pixels ahead of the centre line, into a surface. On a block
 * *snapped receives the centre-line coordinate that puts the edge on the surface. */
inline bool wall_check(const sat_terrain_map2_t* map, const sat_terrain_query2_t& q,
                       const sat_vec2_t& pos, int down_q, int fwd_q, int32_t move,
                       int radius, int height, int32_t* snapped, uint8_t* wall_angle = nullptr,
                       uint32_t* wall_id = nullptr, int32_t* wall_edge = nullptr) {
    const int fa = quad_axis(fwd_q);
    const int fs = quad_sign(fwd_q);
    const int da = quad_axis(down_q);
    const int ds = quad_sign(down_q);
    const int32_t pd = free_pixel(comp(pos, da), ds) - ds * height;
    const int32_t pf = free_pixel(comp(pos, fa), fs);
    const int32_t ox = fa == 0 ? pf : pd;
    const int32_t oy = fa == 0 ? pd : pf;
    int range = static_cast<int>((abs32(move) + 0xFFFF) >> 16) + radius + 1;
    if (range > 255) range = 255;
    sat_terrain_hit2_t hit;
    if (t2::probe(map, ox, oy, static_cast<uint8_t>(fwd_q), range, &q, &hit) != SAT_OK) return false;
    const int32_t edge = comp(hit.point, fa);
    const int32_t body_edge = comp(pos, fa) + move + fs * px16(radius);
    const bool blocked = fs > 0 ? body_edge > edge : body_edge < edge;
    if (blocked) {
        *snapped = edge - fs * px16(radius);
        if (wall_angle) *wall_angle = hit.angle;
        if (wall_id) *wall_id = hit.collider_id;
        if (wall_edge) *wall_edge = edge;
    }
    return blocked;
}

/* Gravity direction expressed as the angle of the level surface it presses on. */
inline uint8_t level_angle(int gravity_q) { return t2::face_angle(static_cast<uint8_t>(gravity_q)); }

inline int steepness(uint8_t angle, int gravity_q) {
    const int d = m2::angle_diff(level_angle(gravity_q), angle);
    return d < 0 ? -d : d;
}

struct StepContext {
    sat_character2_t* ch;
    const sat_character2_config_t* cfg;
    const sat_terrain_map2_t* map;
    sat_terrain_query2_t query;
    uint32_t events;
    int segments;
};

inline int32_t dot_world(sat_vec2_t v, sat_vec2_t unit) {
    return static_cast<int32_t>((static_cast<int64_t>(v.x) * unit.x + static_cast<int64_t>(v.y) * unit.y) >> 16);
}

/* Becomes supported on a surface: converts the world velocity to ground speed. */
inline void land_on(StepContext& c, uint8_t angle, uint32_t id, uint32_t extra_events) {
    sat_character2_t& ch = *c.ch;
    const sat_vec2_t v = ch.air_velocity;
    set_support(ch, angle, id);
    ch.ground_speed = dot_world(v, ch.support_tangent);
    ch.air_velocity = {0, 0};
    ch.flags = static_cast<uint8_t>(ch.flags | SAT_CHARACTER2_SUPPORTED);
    c.events |= SAT_CHARACTER2_EVENT_LANDED | extra_events;
}

inline void leave_surface(StepContext& c, uint32_t extra_events) {
    detach(*c.ch);
    c.events |= SAT_CHARACTER2_EVENT_DETACHED | extra_events;
}

inline sat_vec2_t split(sat_vec2_t v, int n) {
    return {static_cast<sat_fx16_t>(v.x / n), static_cast<sat_fx16_t>(v.y / n)};
}

inline void segment_supported(StepContext& c) {
    sat_character2_t& ch = *c.ch;
    const sat_character2_config_t& cfg = *c.cfg;
    const int mq = mode_quadrant(ch.support_angle);
    const int down_q = (mq + 1) & 3;
    const int fwd_q = ch.ground_speed >= 0 ? mq : (mq + 2) & 3;
    const int fa = quad_axis(fwd_q);
    const int da = quad_axis(down_q);
    const int ds = quad_sign(down_q);

    const sat_vec2_t before = ch.position;
    const int32_t seg = ch.ground_speed / c.segments;
    sat_vec2_t move = {static_cast<sat_fx16_t>((static_cast<int64_t>(ch.support_tangent.x) * seg) >> 16),
                       static_cast<sat_fx16_t>((static_cast<int64_t>(ch.support_tangent.y) * seg) >> 16)};

    bool blocked = false;
    if (seg != 0) {
        int32_t snapped = 0, wall_edge = 0;
        uint8_t wall_angle = 0;
        uint32_t wall_id = 0;
        const bool hit = wall_check(c.map, c.query, before, down_q, fwd_q, comp(move, fa), cfg.wall_radius,
                                    cfg.wall_height, &snapped, &wall_angle, &wall_id, &wall_edge);
        /* A surface of the support's own mode ahead is a slope to climb, left to the foot
         * sensors. One in another mode within max_angle_step is an inside corner (a quarter
         * pipe, a ramp meeting a wall) and simply becomes the support. Anything else is a wall. */
        if (hit && mode_quadrant(wall_angle) != mq) {
            if (abs32(m2::angle_diff(ch.support_angle, wall_angle)) <= cfg.max_angle_step) {
                comp(ch.position, fa) = wall_edge;
                set_support(ch, wall_angle, wall_id);
                return;
            }
            comp(ch.position, fa) = snapped;
            blocked = true;
        } else {
            ch.position.x += move.x;
            ch.position.y += move.y;
        }
    }

    const int lift = cfg.step_up;
    const int range = lift + cfg.snap_down + 1;
    const SupportHit sup = find_support(c.map, c.query, ch.position, down_q, cfg.foot_half_width, lift,
                                        range, range);
    if (!sup.found) {
        leave_surface(c, 0);
        return;
    }
    /* how far the new surface lies above (positive) or below the old feet, along -down */
    const int32_t old_edge = comp(before, da);
    const int32_t rise = ds > 0 ? old_edge - sup.edge : sup.edge - old_edge;
    const int angle_jump = abs32(m2::angle_diff(ch.support_angle, sup.angle));
    if (angle_jump > cfg.max_angle_step || rise > px16(lift)) {
        if (rise > 0) { /* too tall or too abrupt to climb: a wall */
            ch.position = before;
            ch.ground_speed = 0;
            c.events |= SAT_CHARACTER2_EVENT_HIT_WALL;
            return;
        }
        leave_surface(c, 0);
        return;
    }
    if (-rise > px16(cfg.snap_down)) {
        leave_surface(c, 0);
        return;
    }
    comp(ch.position, da) = sup.edge;
    set_support(ch, sup.angle, sup.id);
    if (blocked) {
        ch.ground_speed = 0;
        c.events |= SAT_CHARACTER2_EVENT_HIT_WALL;
    }
}

inline void segment_air(StepContext& c) {
    sat_character2_t& ch = *c.ch;
    const sat_character2_config_t& cfg = *c.cfg;
    const int g = cfg.gravity_quadrant;
    const int fa = quad_axis(g);
    const int fs = quad_sign(g);
    const int ca = fa ^ 1;
    const sat_vec2_t seg = split(ch.air_velocity, c.segments);

    /* 1. along the axis across gravity */
    const int32_t sc = comp(seg, ca);
    if (sc != 0) {
        const int cq = ca == 0 ? (sc > 0 ? 0 : 2) : (sc > 0 ? 1 : 3);
        int32_t snapped = 0;
        if (wall_check(c.map, c.query, ch.position, g, cq, sc, cfg.wall_radius, cfg.wall_height, &snapped)) {
            comp(ch.position, ca) = snapped;
            comp(ch.air_velocity, ca) = 0;
            c.events |= SAT_CHARACTER2_EVENT_HIT_WALL;
        } else {
            comp(ch.position, ca) += sc;
        }
    }

    /* 2. along the gravity axis */
    const int32_t sf = comp(seg, fa);
    comp(ch.position, fa) += sf;
    const int travel = sf * fs >= 0 ? 1 : -1; /* 1: with gravity or still, -1: against it */
    if (travel > 0) {
        const int depth = static_cast<int>((abs32(sf) + 0xFFFF) >> 16) + 1;
        const SupportHit sup = find_support(c.map, c.query, ch.position, g, cfg.foot_half_width, 0,
                                            depth + 1, depth);
        if (sup.found && sup.distance <= 0) {
            comp(ch.position, fa) = sup.edge;
            land_on(c, sup.angle, sup.id, 0);
        }
        return;
    }

    /* rising: the head sensors */
    const int up = (g + 2) & 3;
    sat_vec2_t head = ch.position;
    comp(head, fa) -= fs * px16(cfg.head_height);
    int32_t hx, hy;
    feet_pixel(head, up, hx, hy);
    const int across = quad_axis(up) ^ 1;
    int best_d = 0;
    int32_t best_edge = 0;
    uint8_t best_angle = 0;
    uint32_t best_id = 0;
    bool hit_any = false;
    const int range = static_cast<int>((abs32(sf) + 0xFFFF) >> 16) + 2;
    for (int s = -1; s <= 1; s += 2) {
        int32_t ox = hx, oy = hy;
        if (across == 0) ox += s * cfg.foot_half_width;
        else oy += s * cfg.foot_half_width;
        sat_terrain_hit2_t hit;
        if (t2::probe(c.map, ox, oy, static_cast<uint8_t>(up), range, &c.query, &hit) != SAT_OK) continue;
        const int d = hit.distance >> 16;
        if (d >= 0) continue; /* the head pixel is free on this side */
        if (!hit_any || d < best_d) {
            hit_any = true;
            best_d = d;
            best_edge = comp(hit.point, fa);
            best_angle = hit.angle;
            best_id = hit.collider_id;
        }
    }
    if (!hit_any) return;

    c.events |= SAT_CHARACTER2_EVENT_HIT_CEILING;
    const uint8_t flat_ceiling = static_cast<uint8_t>(level_angle(g) + 128u);
    const int slant = abs32(m2::angle_diff(flat_ceiling, best_angle));
    if (cfg.ceiling_attach != 0 && slant >= cfg.ceiling_attach) {
        comp(ch.position, fa) = best_edge;
        land_on(c, best_angle, best_id, 0);
        return;
    }
    comp(ch.position, fa) = best_edge + fs * px16(cfg.head_height);
    comp(ch.air_velocity, fa) = 0;
}

inline sat_result_t step(sat_character2_t* ch, const sat_character2_config_t* cfg,
                         const sat_terrain_map2_t* map, sat_character2_result_t* result) {
    if (!ch || config_validate(cfg) != SAT_OK) return SAT_ERR_INVALID_ARG;
    StepContext c = {ch, cfg, map, make_query(*ch, *cfg), 0, 1};
    if (!t2::valid_query(map, c.query)) return SAT_ERR_INVALID_ARG;

    const sat_vec2_t v0 = world_velocity(*ch);
    if (result) *result = {};
    const int32_t mag = abs32(v0.x) > abs32(v0.y) ? abs32(v0.x) : abs32(v0.y);
    const int32_t seg_px = px16(cfg->segment_px);
    int n = static_cast<int>((static_cast<int64_t>(mag) + seg_px - 1) / seg_px);
    if (n < 1) n = 1;
    if (n > cfg->max_segments) n = cfg->max_segments;
    c.segments = n;

    for (int i = 0; i < n; ++i) {
        const bool was_supported = (ch->flags & SAT_CHARACTER2_SUPPORTED) != 0;
        if (was_supported) segment_supported(c);
        else segment_air(c);
        if ((c.events & SAT_CHARACTER2_EVENT_HIT_WALL) && was_supported) break;
    }

    if ((ch->flags & SAT_CHARACTER2_SUPPORTED) && steepness(ch->support_angle, cfg->gravity_quadrant) > cfg->steep_angle &&
        abs32(ch->ground_speed) < cfg->min_steep_speed) {
        leave_surface(c, SAT_CHARACTER2_EVENT_SLIPPED);
    }

    if (result) {
        result->events = c.events;
        result->velocity_before = v0;
        result->surface_angle = ch->support_angle;
    }
    return SAT_OK;
}

inline sat_result_t attach(sat_character2_t* ch, const sat_character2_config_t* cfg,
                           const sat_terrain_map2_t* map) {
    if (!ch || config_validate(cfg) != SAT_OK) return SAT_ERR_INVALID_ARG;
    const sat_terrain_query2_t q = make_query(*ch, *cfg);
    if (!t2::valid_query(map, q)) return SAT_ERR_INVALID_ARG;
    const int g = cfg->gravity_quadrant;
    const int lift = cfg->step_up;
    const SupportHit sup = find_support(map, q, ch->position, g, cfg->foot_half_width, lift,
                                        lift + cfg->snap_down + 1, lift);
    if (!sup.found) return SAT_ERR_NOT_FOUND;
    comp(ch->position, quad_axis(g)) = sup.edge;
    const sat_vec2_t v = ch->air_velocity;
    set_support(*ch, sup.angle, sup.id);
    ch->ground_speed = dot_world(v, ch->support_tangent);
    ch->air_velocity = {0, 0};
    ch->flags = static_cast<uint8_t>(ch->flags | SAT_CHARACTER2_SUPPORTED);
    return SAT_OK;
}

} // namespace saturn::physics::character2

#endif
