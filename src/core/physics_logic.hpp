#ifndef SATURN_CORE_PHYSICS_LOGIC_HPP
#define SATURN_CORE_PHYSICS_LOGIC_HPP

#include <stdint.h>
#include "saturn/physics.h"
#include "saturn/video.h"
#include "src/core/collide2d_logic.hpp"

namespace saturn::core::physics {
using namespace saturn::core::collide2d;

inline void clock_init(sat_step_clock_t& c) { c.last_frame = sat_frame_count(); }
inline uint16_t clock_steps(sat_step_clock_t& c, uint16_t max_steps) {
    const uint32_t now = sat_frame_count();
    const uint32_t elapsed = now - c.last_frame;
    c.last_frame = now;
    return static_cast<uint16_t>(elapsed > max_steps ? max_steps : elapsed);
}

inline void body_step(sat_body2_t& b, const sat_body2_params_t& p) {
    b.flags &= static_cast<uint16_t>(~(SAT_BODY_GROUNDED | SAT_BODY_HIT_LEFT |
                                       SAT_BODY_HIT_RIGHT | SAT_BODY_HIT_CEILING));
    b.vel.x = static_cast<sat_fx16_t>((static_cast<int64_t>(b.vel.x) * p.drag_x) >> 16);
    b.vel.y += p.gravity;
    if (p.max_fall > 0 && b.vel.y > p.max_fall) b.vel.y = p.max_fall;
}

inline sat_box2_t tile_box(const sat_grid_t& g, int col, int row) {
    const sat_fx16_t x = static_cast<sat_fx16_t>(
        (static_cast<int32_t>(g.origin_x) + col * g.tile_px + g.tile_px / 2) << 16);
    const sat_fx16_t y = static_cast<sat_fx16_t>(
        (static_cast<int32_t>(g.origin_y) + row * g.tile_px + g.tile_px / 2) << 16);
    const sat_fx16_t h = static_cast<sat_fx16_t>(g.tile_px << 15);
    return {{x, y}, {h, h}};
}
inline int floor_world(sat_fx16_t raw, int origin, int tile) {
    int64_t n = static_cast<int64_t>(raw) - (static_cast<int64_t>(origin) << 16);
    const int64_t d = static_cast<int64_t>(tile) << 16;
    if (n >= 0) return static_cast<int>(n / d);
    return static_cast<int>(-((-n + d - 1) / d));
}
inline int tile_kind(const sat_grid_t& g, int c, int r, sat_tile_fn fn, void* user) {
    if (!fn || r < 0 || r >= g.rows) return SAT_TILE_SOLID;
    c = g.wrap_cols ? sat_grid_wrap_col(&g, c) : c;
    if (c < 0 || c >= g.cols) return SAT_TILE_SOLID;
    return fn(c, r, user);
}
inline void mark_hit(sat_body2_t& b, sat_vec2_t n, sat_fx16_t restitution) {
    if (n.x < 0) b.flags |= SAT_BODY_HIT_RIGHT;
    if (n.x > 0) b.flags |= SAT_BODY_HIT_LEFT;
    if (n.y < 0) b.flags |= SAT_BODY_GROUNDED;
    if (n.y > 0) b.flags |= SAT_BODY_HIT_CEILING;
    b.vel = reflect(b.vel, n, restitution);
}

inline sat_result_t move_tiles(sat_body2_t& b, const sat_grid_t& g, sat_tile_fn fn, void* user) {
    if (!fn || g.tile_px <= 0 || g.cols <= 0 || g.rows <= 0) return SAT_ERR_INVALID_ARG;
    b.flags &= static_cast<uint16_t>(~(SAT_BODY_GROUNDED | SAT_BODY_HIT_LEFT |
                                       SAT_BODY_HIT_RIGHT | SAT_BODY_HIT_CEILING));
    const sat_fx16_t tile = static_cast<sat_fx16_t>(g.tile_px << 16);
    const sat_fx16_t restitution = 0;
    /* Split each axis into at most one-tile moves. Besides making the tile
     * span small, this is the important anti-tunnelling guarantee for arcade
     * velocities that can exceed one tile per fixed tick. */
    for (int axis = 0; axis < 2; ++axis) {
        sat_fx16_t total = axis == 0 ? b.vel.x : b.vel.y;
        sat_fx16_t remaining = total;
        const uint32_t parts = total == 0 ? 1u : static_cast<uint32_t>((absfx(total) + tile - 1) / tile);
        const sat_fx16_t piece = static_cast<sat_fx16_t>(total / static_cast<sat_fx16_t>(parts));
        for (uint32_t part = 0; part < parts; ++part) {
            sat_fx16_t d = (part + 1 == parts) ? remaining : piece;
            remaining -= d;
            sat_vec2_t delta = axis == 0 ? sat_vec2_t{d, 0} : sat_vec2_t{0, d};
            sat_hit2_t best = {};
            int found = 0;
            const sat_fx16_t minx = b.box.center.x - b.box.half.x;
            const sat_fx16_t maxx = b.box.center.x + b.box.half.x;
            const sat_fx16_t miny = b.box.center.y - b.box.half.y;
            const sat_fx16_t maxy = b.box.center.y + b.box.half.y;
            const sat_fx16_t exminx = d < 0 ? minx + d : minx;
            const sat_fx16_t exmaxx = d > 0 ? maxx + d : maxx;
            const sat_fx16_t exminy = d < 0 ? miny + d : miny;
            const sat_fx16_t exmaxy = d > 0 ? maxy + d : maxy;
            const int c0 = floor_world(exminx, g.origin_x, g.tile_px) - 1;
            const int c1 = floor_world(exmaxx, g.origin_x, g.tile_px) + 1;
            const int r0 = floor_world(exminy, g.origin_y, g.tile_px) - 1;
            const int r1 = floor_world(exmaxy, g.origin_y, g.tile_px) + 1;
            for (int r = r0; r <= r1; ++r) for (int c = c0; c <= c1; ++c) {
                const int kind = tile_kind(g, c, r, fn, user);
                if (kind != SAT_TILE_SOLID && !(axis == 1 && kind == SAT_TILE_ONE_WAY)) continue;
                if (kind == SAT_TILE_ONE_WAY) {
                    const sat_box2_t t = tile_box(g, c, r);
                    const sat_fx16_t top = t.center.y - t.half.y;
                    const sat_fx16_t old_bottom = b.box.center.y + b.box.half.y;
                    const sat_fx16_t new_bottom = old_bottom + d;
                    if (d <= 0 || old_bottom > top || new_bottom < top) continue;
                    const sat_fx16_t hit_t = div_ratio_fx16(static_cast<int64_t>(top - old_bottom), d);
                    if (hit_t < 0 || hit_t > SAT_FX16_ONE || (found && hit_t >= best.t)) continue;
                    best = {hit_t, point_at(b.box.center, delta, hit_t), {0, -SAT_FX16_ONE}};
                    best.point.y = top - b.box.half.y;
                    found = 1;
                } else {
                    sat_hit2_t hit;
                    const sat_box2_t target = tile_box(g, c, r);
                    if (sat_sweep_box2(&b.box, &delta, &target, &hit) &&
                        (!found || hit.t < best.t)) {
                        /* The sweep quotient is intentionally truncated. A
                         * landing must nevertheless be exactly on the tile
                         * edge, otherwise a one-raw-unit gap accumulates and
                         * the grounded flag flickers on the next tick. */
                        if (axis == 1) {
                            hit.point.y = hit.normal.y < 0
                                ? target.center.y - target.half.y - b.box.half.y
                                : target.center.y + target.half.y + b.box.half.y;
                        }
                        best = hit; found = 1;
                    }
                }
            }
            if (!found) {
                b.box.center = add(b.box.center, delta);
                continue;
            }
            b.box.center = best.point;
            mark_hit(b, best.normal, restitution);
            const sat_fx16_t left_t = SAT_FX16_ONE - best.t;
            sat_vec2_t rest = scale(delta, left_t);
            if (axis == 1) {
                /* Y is intentionally resolved independently. There is no
                 * remaining tangent component on a vertical tile pass. */
                rest = {0, 0};
            }
            const int64_t dn = (static_cast<int64_t>(rest.x) * best.normal.x +
                                static_cast<int64_t>(rest.y) * best.normal.y) >> 16;
            if (dn < 0) {
                rest.x -= static_cast<sat_fx16_t>((dn * best.normal.x) >> 16);
                rest.y -= static_cast<sat_fx16_t>((dn * best.normal.y) >> 16);
            }
            b.box.center = add(b.box.center, rest);
            if (axis == 0) remaining = 0;
            else remaining = 0;
        }
        /* A slope is a height field, not a solid box. Apply it after X and Y
         * motion so the foot remains on the ramp while walking across it. */
        if (axis == 1 && b.vel.y >= 0) {
            const int c0 = floor_world(b.box.center.x - b.box.half.x, g.origin_x, g.tile_px) - 1;
            const int c1 = floor_world(b.box.center.x + b.box.half.x, g.origin_x, g.tile_px) + 1;
            const int r = floor_world(b.box.center.y + b.box.half.y, g.origin_y, g.tile_px);
            for (int c = c0; c <= c1; ++c) {
                const int kind = tile_kind(g, c, r, fn, user);
                if (kind != SAT_TILE_SLOPE_UP && kind != SAT_TILE_SLOPE_DOWN) continue;
                const sat_fx16_t left = static_cast<sat_fx16_t>((g.origin_x + c * g.tile_px) << 16);
                sat_fx16_t local = b.box.center.x - left;
                if (local < 0) local = 0;
                if (local > tile) local = tile;
                const sat_fx16_t top = static_cast<sat_fx16_t>((g.origin_y + r * g.tile_px) << 16) +
                    (kind == SAT_TILE_SLOPE_UP ? tile - local : local);
                const sat_fx16_t want = top - b.box.half.y;
                if (b.box.center.y + b.box.half.y >= top && b.box.center.y >= want - tile) {
                    b.box.center.y = want; b.flags |= SAT_BODY_GROUNDED; b.vel.y = 0;
                }
            }
        }
    }
    return SAT_OK;
}

inline sat_result_t move_boxes(sat_body2_t& b, const sat_box2_t* boxes, uint16_t count) {
    if (!boxes && count) return SAT_ERR_INVALID_ARG;
    sat_vec2_t remaining = b.vel;
    for (int iteration = 0; iteration < 3; ++iteration) {
        sat_hit2_t best = {}; int found = 0;
        for (uint16_t i = 0; i < count; ++i) {
            sat_hit2_t h;
            if (sat_sweep_box2(&b.box, &remaining, &boxes[i], &h) && (!found || h.t < best.t)) { best = h; found = 1; }
        }
        if (!found) { b.box.center = add(b.box.center, remaining); break; }
        b.box.center = best.point; mark_hit(b, best.normal, 0);
        remaining = scale(remaining, static_cast<sat_fx16_t>(SAT_FX16_ONE - best.t));
        const int64_t dn = (static_cast<int64_t>(remaining.x) * best.normal.x +
                            static_cast<int64_t>(remaining.y) * best.normal.y) >> 16;
        if (dn < 0) { remaining.x -= static_cast<sat_fx16_t>((dn * best.normal.x) >> 16); remaining.y -= static_cast<sat_fx16_t>((dn * best.normal.y) >> 16); }
        if (remaining.x == 0 && remaining.y == 0) break;
    }
    return SAT_OK;
}

inline int separate(sat_body2_t& a, sat_body2_t& b) {
    sat_contact2_t c;
    if (!box_contact(a.box, b.box, c)) return 0;
    const sat_fx16_t half = c.depth / 2;
    a.box.center.x += static_cast<sat_fx16_t>((static_cast<int64_t>(c.normal.x) * half) >> 16);
    a.box.center.y += static_cast<sat_fx16_t>((static_cast<int64_t>(c.normal.y) * half) >> 16);
    b.box.center.x -= static_cast<sat_fx16_t>((static_cast<int64_t>(c.normal.x) * half) >> 16);
    b.box.center.y -= static_cast<sat_fx16_t>((static_cast<int64_t>(c.normal.y) * half) >> 16);
    const int64_t d = (static_cast<int64_t>(a.vel.x - b.vel.x) * c.normal.x +
                       static_cast<int64_t>(a.vel.y - b.vel.y) * c.normal.y) >> 16;
    if (d < 0) {
        const sat_fx16_t impulse = static_cast<sat_fx16_t>(-d);
        const sat_fx16_t ix = static_cast<sat_fx16_t>((static_cast<int64_t>(impulse) * c.normal.x) >> 17);
        const sat_fx16_t iy = static_cast<sat_fx16_t>((static_cast<int64_t>(impulse) * c.normal.y) >> 17);
        a.vel.x += ix; a.vel.y += iy; b.vel.x -= ix; b.vel.y -= iy;
    }
    return 1;
}
} // namespace saturn::core::physics

#endif
