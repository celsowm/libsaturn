#include "saturn/physics.h"
#include "src/core/physics_logic.hpp"

using namespace saturn::core::physics;

extern "C" void sat_step_clock_init(sat_step_clock_t* c) { if (c) clock_init(*c); }
extern "C" uint16_t sat_step_clock_steps(sat_step_clock_t* c, uint16_t max_steps) {
    return c ? clock_steps(*c, max_steps) : 0;
}
extern "C" void sat_body2_step(sat_body2_t* b, const sat_body2_params_t* p) {
    if (b && p) body_step(*b, *p);
}
extern "C" sat_result_t sat_body2_move_tiles(sat_body2_t* b, const sat_grid_t* g,
    sat_tile_fn fn, void* user) {
    return b && g ? move_tiles(*b, *g, fn, user) : SAT_ERR_INVALID_ARG;
}
extern "C" sat_result_t sat_body2_move_boxes(sat_body2_t* b, const sat_box2_t* boxes, uint16_t count) {
    return b ? move_boxes(*b, boxes, count) : SAT_ERR_INVALID_ARG;
}
extern "C" int sat_body2_separate(sat_body2_t* a, sat_body2_t* b) {
    return a && b ? separate(*a, *b) : 0;
}
