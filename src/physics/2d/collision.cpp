#include "saturn/collide2d.h"
#include "src/physics/2d/collision_logic.hpp"

using namespace saturn::core::collide2d;

extern "C" sat_vec2_t sat_vec2_add(sat_vec2_t a, sat_vec2_t b) { return add(a, b); }
extern "C" sat_vec2_t sat_vec2_sub(sat_vec2_t a, sat_vec2_t b) { return sub(a, b); }
extern "C" sat_vec2_t sat_vec2_scale(sat_vec2_t a, sat_fx16_t s) { return scale(a, s); }
extern "C" sat_fx16_t sat_approach(sat_fx16_t v, sat_fx16_t t, sat_fx16_t s) { return approach(v, t, s); }
extern "C" sat_vec2_t sat_reflect2(sat_vec2_t v, sat_vec2_t n, sat_fx16_t r) { return reflect(v, n, r); }

extern "C" int sat_box2_overlap(const sat_box2_t* a, const sat_box2_t* b) {
    return valid_box(a) && valid_box(b) && box_overlap(*a, *b);
}
extern "C" int sat_circle_overlap(const sat_circle_t* a, const sat_circle_t* b) {
    return valid_circle(a) && valid_circle(b) && circle_overlap(*a, *b);
}
extern "C" int sat_circle_box_overlap(const sat_circle_t* c, const sat_box2_t* b) {
    sat_contact2_t unused;
    return valid_circle(c) && valid_box(b) && circle_box_contact(*c, *b, unused);
}
extern "C" int sat_point_in_box2(const sat_vec2_t* p, const sat_box2_t* b) {
    return p != nullptr && valid_box(b) && point_box(*p, *b);
}
extern "C" int sat_point_in_circle(const sat_vec2_t* p, const sat_circle_t* c) {
    return p != nullptr && valid_circle(c) && point_circle(*p, *c);
}
extern "C" int sat_box2_contact(const sat_box2_t* a, const sat_box2_t* b, sat_contact2_t* o) {
    return valid_box(a) && valid_box(b) && o != nullptr && box_contact(*a, *b, *o);
}
extern "C" int sat_circle_contact(const sat_circle_t* a, const sat_circle_t* b, sat_contact2_t* o) {
    return valid_circle(a) && valid_circle(b) && o != nullptr && circle_contact(*a, *b, *o);
}
extern "C" int sat_circle_box_contact(const sat_circle_t* c, const sat_box2_t* b, sat_contact2_t* o) {
    return valid_circle(c) && valid_box(b) && o != nullptr && circle_box_contact(*c, *b, *o);
}
extern "C" int sat_raycast_box2(const sat_box2_t* b, const sat_vec2_t* o, const sat_vec2_t* d, sat_hit2_t* h) {
    return valid_box(b) && o != nullptr && d != nullptr && h != nullptr && ray_box(*b, *o, *d, *h);
}
extern "C" int sat_raycast_circle(const sat_circle_t* c, const sat_vec2_t* o, const sat_vec2_t* d, sat_hit2_t* h) {
    return valid_circle(c) && o != nullptr && d != nullptr && h != nullptr && ray_circle(*c, *o, *d, *h);
}
extern "C" int sat_sweep_box2(const sat_box2_t* moving, const sat_vec2_t* d, const sat_box2_t* target, sat_hit2_t* h) {
    if (!valid_box(moving) || !valid_box(target) || d == nullptr || h == nullptr) return 0;
    sat_box2_t expanded = *target;
    expanded.half.x += moving->half.x;
    expanded.half.y += moving->half.y;
    return ray_box(expanded, moving->center, *d, *h) ? 1 : 0;
}
