#include "saturn/character2.h"

#include "src/physics/2d/character2_logic.hpp"

namespace c2 = saturn::physics::character2;

extern "C" void sat_character2_init(sat_character2_t* ch, sat_fx16_t x, sat_fx16_t y) {
    if (!ch) return;
    *ch = {};
    ch->position = {x, y};
    ch->support_id = SAT_CHARACTER2_NO_SUPPORT;
}

extern "C" void sat_character2_config_default(sat_character2_config_t* cfg) {
    if (!cfg) return;
    *cfg = {};
    cfg->foot_half_width = 5;
    cfg->wall_radius = 6;
    cfg->wall_height = 8;
    cfg->head_height = 24;
    cfg->step_up = 8;
    cfg->snap_down = 8;
    cfg->max_angle_step = 40;
    cfg->steep_angle = 48;
    cfg->min_steep_speed = 5 * SAT_FX16_ONE / 2;
    cfg->ceiling_attach = 0;
    cfg->gravity_quadrant = 1;
    cfg->segment_px = 4;
    cfg->max_segments = 32;
    cfg->category_mask = 0xFFu;
    cfg->ignore_flags = 0;
}

extern "C" sat_result_t sat_character2_config_validate(const sat_character2_config_t* cfg) {
    return c2::config_validate(cfg);
}
extern "C" int sat_character2_is_supported(const sat_character2_t* ch) {
    return ch && (ch->flags & SAT_CHARACTER2_SUPPORTED) ? 1 : 0;
}
extern "C" sat_vec2_t sat_character2_world_velocity(const sat_character2_t* ch) {
    return ch ? c2::world_velocity(*ch) : sat_vec2_t{0, 0};
}
extern "C" sat_result_t sat_character2_attach(sat_character2_t* ch, const sat_character2_config_t* cfg,
    const sat_terrain_map2_t* map) {
    return c2::attach(ch, cfg, map);
}
extern "C" void sat_character2_detach(sat_character2_t* ch) {
    if (ch) c2::detach(*ch);
}
extern "C" sat_result_t sat_character2_step(sat_character2_t* ch, const sat_character2_config_t* cfg,
    const sat_terrain_map2_t* map, sat_character2_result_t* result) {
    return c2::step(ch, cfg, map, result);
}
