#ifndef SATURN_PHYSICS_H
#define SATURN_PHYSICS_H

#include <stdint.h>

#include "saturn/collide2d.h"
#include "saturn/grid.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sat_step_clock {
    uint32_t last_frame;
} sat_step_clock_t;
void sat_step_clock_init(sat_step_clock_t* clock);
uint16_t sat_step_clock_steps(sat_step_clock_t* clock, uint16_t max_steps);

enum {
    SAT_TILE_EMPTY = 0,
    SAT_TILE_SOLID = 1,
    SAT_TILE_ONE_WAY = 2,
    SAT_TILE_SLOPE_UP = 3,
    SAT_TILE_SLOPE_DOWN = 4
};
typedef int (*sat_tile_fn)(int col, int row, void* user);

enum {
    SAT_BODY_GROUNDED = 1u << 0,
    SAT_BODY_HIT_LEFT = 1u << 1,
    SAT_BODY_HIT_RIGHT = 1u << 2,
    SAT_BODY_HIT_CEILING = 1u << 3
};
typedef struct sat_body2 { sat_box2_t box; sat_vec2_t vel; uint16_t flags; } sat_body2_t;
typedef struct sat_body2_params {
    sat_fx16_t gravity, max_fall, drag_x, restitution;
} sat_body2_params_t;

void sat_body2_step(sat_body2_t* body, const sat_body2_params_t* params);
sat_result_t sat_body2_move_tiles(sat_body2_t* body, const sat_grid_t* grid,
    sat_tile_fn tile_fn, void* user);
sat_result_t sat_body2_move_boxes(sat_body2_t* body, const sat_box2_t* boxes, uint16_t count);
int sat_body2_separate(sat_body2_t* a, sat_body2_t* b);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_PHYSICS_H */
