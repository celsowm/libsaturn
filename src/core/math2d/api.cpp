#include "saturn/math2d.h"

#include "src/core/math2d/logic.hpp"

namespace m2 = saturn::core::math2d;

extern "C" sat_fx16_t sat_sin8(sat_angle_t a) { return m2::sin8(a); }
extern "C" sat_fx16_t sat_cos8(sat_angle_t a) { return m2::cos8(a); }
extern "C" sat_fx16_t sat_sin16(sat_angle16_t a) { return m2::sin16(a); }
extern "C" sat_fx16_t sat_cos16(sat_angle16_t a) { return m2::cos16(a); }
extern "C" sat_vec2_t sat_vec2_from_angle(sat_angle_t a) { return m2::from_angle(a); }
extern "C" sat_angle_t sat_atan2_8(sat_fx16_t y, sat_fx16_t x) { return m2::atan2_8(y, x); }
extern "C" sat_angle16_t sat_atan2_16(sat_fx16_t y, sat_fx16_t x) { return m2::atan2_16(y, x); }
extern "C" int8_t sat_angle_diff(sat_angle_t from, sat_angle_t to) { return m2::angle_diff(from, to); }
extern "C" uint8_t sat_angle_quadrant(sat_angle_t a) { return m2::angle_quadrant(a); }
extern "C" sat_fx16_t sat_vec2_dot(sat_vec2_t a, sat_vec2_t b) { return m2::dot(a, b); }
extern "C" sat_fx16_t sat_vec2_length(sat_vec2_t v) { return m2::length(v); }
extern "C" sat_vec2_t sat_vec2_normalize(sat_vec2_t v) { return m2::normalize(v); }
extern "C" sat_vec2_t sat_vec2_perp(sat_vec2_t v) { return m2::perp(v); }
extern "C" sat_vec2_t sat_vec2_project(sat_vec2_t v, sat_vec2_t u) { return m2::project(v, u); }
extern "C" sat_vec2_t sat_vec2_reject(sat_vec2_t v, sat_vec2_t u) { return m2::reject(v, u); }
extern "C" sat_fx16_t sat_lerp_fx16(sat_fx16_t a, sat_fx16_t b, sat_fx16_t t) { return m2::lerp(a, b, t); }
extern "C" sat_vec2_t sat_vec2_lerp(sat_vec2_t a, sat_vec2_t b, sat_fx16_t t) { return m2::lerp(a, b, t); }
