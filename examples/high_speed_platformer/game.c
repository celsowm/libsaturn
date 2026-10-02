#include "game.h"

#include "saturn/input.h"
#include "saturn/math2d.h"

/* All tuning is in pixels and 16.16 pixels per 60 Hz step. The library moves the hero along the
 * surface; everything that makes it a game (acceleration, slope force, jumps, the dash pad, the
 * rail, what a ring is worth) is decided here. */

#define FXC(v) ((sat_fx16_t)((v) * 65536.0))
#define FXI(v) ((sat_fx16_t)((int32_t)(v) * 65536))

#define ACCEL FXC(0.09375)
#define DECEL FXC(0.5)
#define FRICTION FXC(0.046875)
#define TOP_SPEED FXC(8.0)
#define MAX_GROUND FXC(16.0)
#define SLOPE_FORCE FXC(0.125)
#define ROLL_SLOPE FXC(0.2)
#define ROLL_FRICTION FXC(0.0234375)
#define ROLL_DECEL FXC(0.125)
#define GRAVITY FXC(0.21875)
#define MAX_FALL FXC(16.0)
#define AIR_ACCEL FXC(0.09375)
#define JUMP_SPEED FXC(6.5)
#define DASH_SPEED FXC(14.0)
#define RAIL_GRAB FXC(16.0)
#define RAIL_HANG FXC(22.0)
#define HARD_LANDING FXC(9.0)

#define JUMP_BUTTONS (SAT_PAD_A | SAT_PAD_B | SAT_PAD_C)
#define POOL_BOX_R 8

static sat_fx16_t fx_mul(sat_fx16_t a, sat_fx16_t b) {
    return (sat_fx16_t)(((int64_t)a * (int64_t)b) >> 16);
}

static sat_fx16_t fx_abs(sat_fx16_t v) {
    return v < 0 ? -v : v;
}

static sat_vec2_t vec(sat_fx16_t x, sat_fx16_t y) {
    sat_vec2_t v;
    v.x = x;
    v.y = y;
    return v;
}

static void zero_bytes(void* p, uint32_t n) {
    uint8_t* b = (uint8_t*)p;
    while (n--) *b++ = 0u;
}

/* ----- ground control, shared by walking on terrain and walking on a platform ----- */

static sat_fx16_t ground_control(sat_fx16_t speed, int dir, int rolling) {
    if (rolling) {
        if (dir > 0 && speed < 0) speed += ROLL_DECEL;
        else if (dir < 0 && speed > 0) speed -= ROLL_DECEL;
        else speed = sat_approach(speed, 0, ROLL_FRICTION);
        return speed;
    }
    if (dir > 0) {
        if (speed < 0) speed += DECEL;
        else if (speed < TOP_SPEED) {
            speed += ACCEL;
            if (speed > TOP_SPEED) speed = TOP_SPEED;
        }
    } else if (dir < 0) {
        if (speed > 0) speed -= DECEL;
        else if (speed > -TOP_SPEED) {
            speed -= ACCEL;
            if (speed < -TOP_SPEED) speed = -TOP_SPEED;
        }
    } else {
        speed = sat_approach(speed, 0, FRICTION);
    }
    return speed;
}

/* ----- clips ----- */

static void hero_event(void* user, uint16_t event, uint16_t clip, uint16_t frame) {
    hsp_game_t* g = (hsp_game_t*)user;
    (void)clip;
    (void)frame;
    if (event == 1u) ++g->stats.footsteps;
}

static void play_hero_clip(hsp_game_t* g, uint16_t clip, sat_fx16_t rate) {
    (void)sat_clip_player_play(&g->hero_clip, clip, SAT_CLIP_SWITCH_KEEP_IF_SAME, hero_event, g);
    (void)sat_clip_player_set_rate(&g->hero_clip, rate);
}

static void update_hero_clip(hsp_game_t* g) {
    const sat_character2_t* h = &g->hero;
    const sat_vec2_t v = sat_character2_world_velocity(h);
    const sat_fx16_t speed = fx_abs(g->mode == HSP_MODE_RAIL ? g->rail_speed : sat_vec2_length(v));
    if (v.x < -FXC(0.1)) g->facing_left = 1u;
    else if (v.x > FXC(0.1)) g->facing_left = 0u;
    if (g->mode == HSP_MODE_RAIL || (!sat_character2_is_supported(h) && g->stand == SAT_COLLIDER2_NONE) || g->rolling) {
        play_hero_clip(g, STAGE_CLIP_ROLL, FXC(1.0) + (speed >> 2));
    } else if (speed < FXC(0.25)) {
        play_hero_clip(g, STAGE_CLIP_IDLE, FXC(1.0));
    } else {
        sat_fx16_t rate = speed >> 1;
        if (rate < FXC(0.5)) rate = FXC(0.5);
        if (rate > FXC(4.0)) rate = FXC(4.0);
        play_hero_clip(g, STAGE_CLIP_RUN, rate);
    }
    (void)sat_clip_player_step(&g->hero_clip, hero_event, g, 0);
}

/* ----- shake ----- */

static void shake(hsp_game_t* g, int amplitude, uint16_t steps) {
    sat_shake2d_params_t p;
    zero_bytes(&p, sizeof(p));
    p.amplitude_x = FXI(amplitude);
    p.amplitude_y = FXI(amplitude);
    p.duration = steps;
    p.waveform = SAT_SHAKE2D_SINE;
    p.envelope = SAT_SHAKE2D_LINEAR;
    p.sign = SAT_SHAKE2D_BOTH;
    p.update_every = 1u;
    p.frequency = 0x5555u;
    p.seed = g->stats.ticks;
    (void)sat_follow_camera2d_shake(&g->camera, &p);
}

/* ----- the hero ----- */

static void respawn(hsp_game_t* g) {
    sat_character2_init(&g->hero, g->respawn.x, g->respawn.y);
    (void)sat_character2_attach(&g->hero, &g->cfg_stand, &g->terrain);
    g->mode = HSP_MODE_RUN;
    g->rolling = 0u;
    g->jump_active = 0u;
    g->stand = SAT_COLLIDER2_NONE;
    g->rail_cooldown = 0u;
    (void)sat_follow_camera2d_snap(&g->camera, vec(g->hero.position.x, g->hero.position.y - FXI(HSP_HERO_HALF_H)));
    if (g->in_room && g->respawn.x < FXI(HSP_ROOM_X)) {
        g->in_room = 0u;
        (void)sat_follow_camera2d_clear_clamp(&g->camera, 0);
    }
}

static void leave_rail(hsp_game_t* g, sat_vec2_t velocity) {
    sat_character2_t* h = &g->hero;
    h->flags = 0u;
    h->support_id = SAT_CHARACTER2_NO_SUPPORT;
    h->air_velocity = velocity;
    h->ground_speed = 0;
    g->mode = HSP_MODE_RUN;
    g->rail_cooldown = 30u;
    g->stand = SAT_COLLIDER2_NONE;
}

static void try_grab_rail(hsp_game_t* g) {
    sat_path2_nearest_t near_point;
    const sat_character2_t* h = &g->hero;
    const sat_vec2_t head = vec(h->position.x, h->position.y - RAIL_HANG);
    sat_fx16_t vx = h->air_velocity.x;
    if (g->rail_cooldown != 0u) return;
    if (sat_path2_nearest(&g->rail, head, &near_point) != SAT_OK) return;
    if (near_point.gap > RAIL_GRAB) return;
    if (near_point.sample.distance <= 0 && vx < 0) return; /* leaving the rail's start backwards */
    g->mode = HSP_MODE_RAIL;
    g->rail_distance = near_point.sample.distance;
    g->rail_direction = vx < -FXC(0.5) ? (int8_t)-1 : (int8_t)1;
    g->rail_speed = fx_abs(vx);
    if (g->rail_speed < FXC(6.0)) g->rail_speed = FXC(6.0);
    g->rolling = 0u;
    ++g->stats.rail_grabs;
}

/* The rail is a Bezier curve. Riding it is nothing but advancing a distance along the path, with
 * gravity speeding the hero up where the curve points down. */
static void rail_step(hsp_game_t* g, uint16_t pressed) {
    sat_character2_t* h = &g->hero;
    sat_path2_sample_t s;
    uint8_t flags = 0u;
    sat_vec2_t velocity;
    if (sat_path2_sample(&g->rail, g->rail_distance, &s) != SAT_OK) {
        g->mode = HSP_MODE_RUN;
        return;
    }
    g->rail_speed += fx_mul(GRAVITY >> 1, s.tangent.y * g->rail_direction);
    if (g->rail_speed < FXC(4.0)) g->rail_speed = FXC(4.0);
    if (g->rail_speed > MAX_GROUND) g->rail_speed = MAX_GROUND;
    (void)sat_path2_advance(&g->rail, g->rail_distance, g->rail_speed * g->rail_direction, SAT_PATH2_CLAMP,
        &g->rail_distance, &flags);
    (void)sat_path2_sample(&g->rail, g->rail_distance, &s);
    h->position = vec(s.position.x, s.position.y + RAIL_HANG);
    velocity = vec(fx_mul(s.tangent.x, g->rail_speed) * g->rail_direction,
        fx_mul(s.tangent.y, g->rail_speed) * g->rail_direction);
    h->air_velocity = velocity;
    h->flags = 0u;
    if ((pressed & JUMP_BUTTONS) != 0u) {
        velocity.y -= FXC(4.5);
        leave_rail(g, velocity);
        g->jump_active = 1u;
    } else if ((flags & (SAT_PATH2_HIT_START | SAT_PATH2_HIT_END)) != 0u) {
        leave_rail(g, velocity);
    }
}

static void find_platform_support(hsp_game_t* g, int landing_allowed) {
    sat_character2_t* h = &g->hero;
    sat_physics2_support_query_t q;
    sat_physics2_support_t sup;
    const sat_collider2_t previous = g->stand;
    if (sat_character2_is_supported(h) || !landing_allowed) {
        g->stand = SAT_COLLIDER2_NONE;
        return;
    }
    zero_bytes(&q, sizeof(q));
    q.origin = h->position;
    q.half_width = FXI(5);
    q.down_quadrant = 1u;
    q.reach = 6u;
    q.embed = 16u;
    q.category_mask = HSP_CAT_PLATFORM;
    if (sat_physics2_find_support(&g->world, &q, &sup) == SAT_OK) {
        const sat_fx16_t relative_vy = h->air_velocity.y - sup.delta.y;
        const int was_above = (h->position.y - h->air_velocity.y) <= sup.point.y + FXI(1);
        /* a hero that already stands here was carried along this step, so a platform sinking
         * under it is not a reason to let go */
        if ((relative_vy >= 0 || previous == sup.id) && was_above) {
            h->position.y = sup.point.y;
            h->air_velocity.y = 0;
            g->stand = sup.id;
            g->jump_active = 0u;
            return;
        }
    }
    g->stand = SAT_COLLIDER2_NONE;
}

static void run_step(hsp_game_t* g, uint16_t held, uint16_t pressed) {
    sat_character2_t* h = &g->hero;
    const int dir = ((held & SAT_PAD_RIGHT) != 0u) - ((held & SAT_PAD_LEFT) != 0u);
    const int jump = (pressed & JUMP_BUTTONS) != 0u;
    sat_character2_result_t res;
    const sat_character2_config_t* cfg = &g->cfg_stand;
    int on_platform = g->stand != SAT_COLLIDER2_NONE;
    int allow_landing = 1;

    if (on_platform && sat_physics2_carry(&g->world, g->stand, &h->position) != SAT_OK) {
        g->stand = SAT_COLLIDER2_NONE;
        on_platform = 0;
    }

    if (sat_character2_is_supported(h)) {
        sat_fx16_t gs = h->ground_speed;
        const sat_vec2_t normal = h->support_normal;
        gs += fx_mul(g->rolling ? ROLL_SLOPE : SLOPE_FORCE, sat_sin8(h->support_angle));
        if ((held & SAT_PAD_DOWN) != 0u && fx_abs(gs) >= FXC(1.0)) g->rolling = 1u;
        if (g->rolling && fx_abs(gs) < FXC(0.5)) g->rolling = 0u;
        gs = ground_control(gs, dir, g->rolling);
        if (gs > MAX_GROUND) gs = MAX_GROUND;
        if (gs < -MAX_GROUND) gs = -MAX_GROUND;
        h->ground_speed = gs;
        if (jump) {
            sat_character2_detach(h);
            h->air_velocity.x += fx_mul(normal.x, JUMP_SPEED);
            h->air_velocity.y += fx_mul(normal.y, JUMP_SPEED);
            g->jump_active = 1u;
        }
        if (g->rolling) cfg = &g->cfg_roll;
    } else if (on_platform) {
        /* standing on a platform: Character2 sees an airborne hero, the game keeps it on the face */
        h->air_velocity.x = ground_control(h->air_velocity.x, dir, g->rolling);
        h->air_velocity.y = 0;
        if (jump) {
            sat_vec2_t launch = vec(0, 0);
            (void)sat_physics2_launch_velocity(&g->world, g->stand, &launch);
            h->air_velocity.x += launch.x;
            h->air_velocity.y = launch.y - JUMP_SPEED;
            g->stand = SAT_COLLIDER2_NONE;
            g->jump_active = 1u;
            allow_landing = 0;
        }
    } else {
        h->air_velocity.y += GRAVITY;
        if (h->air_velocity.y > MAX_FALL) h->air_velocity.y = MAX_FALL;
        if (dir > 0 && h->air_velocity.x < TOP_SPEED) h->air_velocity.x += AIR_ACCEL;
        if (dir < 0 && h->air_velocity.x > -TOP_SPEED) h->air_velocity.x -= AIR_ACCEL;
        if (g->jump_active && (held & JUMP_BUTTONS) == 0u && h->air_velocity.y < -FXI(4)) h->air_velocity.y = -FXI(4);
    }

    zero_bytes(&res, sizeof(res));
    (void)sat_character2_step(h, cfg, &g->terrain, &res);

    if ((res.events & SAT_CHARACTER2_EVENT_LANDED) != 0u) {
        ++g->stats.landings;
        g->jump_active = 0u;
        if (res.velocity_before.y > HARD_LANDING) shake(g, 3, 10u);
    }
    if (!sat_character2_is_supported(h)) g->rolling = g->rolling && g->jump_active;

    /* `allow_landing` keeps the jump that just left the platform from snapping straight back */
    find_platform_support(g, allow_landing);
    if (g->stand != SAT_COLLIDER2_NONE) ++g->stats.platform_ticks;

    if (!sat_character2_is_supported(h) && g->stand == SAT_COLLIDER2_NONE && g->mode == HSP_MODE_RUN) {
        try_grab_rail(g);
    }
    if (g->rail_cooldown != 0u) --g->rail_cooldown;
}

/* ----- moving platforms ----- */

static sat_vec2_t platform_position(const hsp_platform_state_t* p) {
    sat_path2_sample_t s;
    (void)sat_path2_sample(&p->path, p->distance, &s);
    return s.position;
}

static void update_platforms(hsp_game_t* g) {
    uint32_t i;
    for (i = 0; i < HSP_PLATFORM_COUNT; ++i) {
        hsp_platform_state_t* p = &g->platform[i];
        const hsp_platform_t* def = &hsp_platforms[i];
        uint8_t flags = 0u;
        const sat_fx16_t step = (sat_fx16_t)def->speed_q4 << 12;
        (void)sat_path2_advance(&p->path, p->distance, step * p->direction,
            def->path_kind == 0u ? SAT_PATH2_CLAMP : SAT_PATH2_WRAP, &p->distance, &flags);
        if ((flags & SAT_PATH2_HIT_END) != 0u) p->direction = -1;
        if ((flags & SAT_PATH2_HIT_START) != 0u) p->direction = 1;
        (void)sat_physics2_set_center(&g->world, p->collider, platform_position(p));
    }
}

void hsp_game_platform_box(const hsp_game_t* game, uint32_t index, sat_box2_t* out) {
    out->center = platform_position(&game->platform[index]);
    out->half = vec(FXI(hsp_platforms[index].half_w), FXI(hsp_platforms[index].half_h));
}

/* ----- entities: descriptors become objects while the camera is near ----- */

static sat_entity_activate_result_t activate_entity(void* user, uint32_t index, const sat_entity_desc2_t* desc) {
    hsp_game_t* g = (hsp_game_t*)user;
    uint32_t i;
    uint16_t clip;
    uint8_t kind;
    switch (desc->kind) {
        case HSP_KIND_RING: kind = HSP_OBJ_RING; clip = STAGE_CLIP_RING; break;
        case HSP_KIND_DASH: kind = HSP_OBJ_DASH; clip = STAGE_CLIP_DASH; break;
        case HSP_KIND_SPRING: kind = HSP_OBJ_SPRING; clip = STAGE_CLIP_SPRING; break;
        case HSP_KIND_CHECKPOINT: kind = HSP_OBJ_FLAG; clip = STAGE_CLIP_FLAG; break;
        case HSP_KIND_GOAL: kind = HSP_OBJ_GOAL; clip = STAGE_CLIP_FLAG; break;
        default: return SAT_ENTITY_DECLINE;
    }
    for (i = 0; i < HSP_MAX_OBJECTS; ++i) {
        hsp_object_t* o = &g->objects[i];
        if (o->kind != HSP_OBJ_FREE) continue;
        zero_bytes(o, sizeof(*o));
        o->kind = kind;
        o->desc = (uint16_t)index;
        o->x = (int16_t)desc->x;
        o->y = (int16_t)desc->y;
        o->data = desc->data;
        (void)sat_clip_player_init(&o->clip, &stage_clip_set);
        (void)sat_clip_player_play(&o->clip, clip, SAT_CLIP_SWITCH_RESTART, 0, 0);
        if (kind == HSP_OBJ_RING) (void)sat_clip_player_seek(&o->clip, index & 3u, 0, 0); /* rings spin out of step */
        ++g->stats.spawned;
        return SAT_ENTITY_ACTIVATED;
    }
    ++g->stats.pool_full;
    return SAT_ENTITY_DEFER;
}

static void deactivate_entity(void* user, uint32_t index, const sat_entity_desc2_t* desc) {
    hsp_game_t* g = (hsp_game_t*)user;
    uint32_t i;
    (void)desc;
    for (i = 0; i < HSP_MAX_OBJECTS; ++i) {
        hsp_object_t* o = &g->objects[i];
        if (o->kind != HSP_OBJ_FREE && o->desc == (uint16_t)index) {
            o->kind = HSP_OBJ_FREE;
            ++g->stats.despawned;
            return;
        }
    }
}

/* a clip shape of the object's current frame, as a box in the world */
static int object_shape_box(const hsp_object_t* o, uint8_t shape_kind, sat_box2_t* out) {
    const sat_clip_frame_t* frame = sat_clip_player_frame(&o->clip);
    sat_clip_shape_t shape;
    sat_rect_t r;
    if (!frame) return 0;
    if (!sat_clip_frame_find_shape(&stage_clip_set, frame, shape_kind, 0xFFu, &shape)) return 0;
    if (sat_clip_shape_rect(&shape, o->x, o->y, 0u, &r) != SAT_OK) return 0;
    out->half = vec(FXI(r.width) >> 1, FXI(r.height) >> 1);
    out->center = vec(FXI(r.x) + out->half.x, FXI(r.y) + out->half.y);
    return 1;
}

static void hero_box(const hsp_game_t* g, sat_box2_t* out) {
    out->half = vec(FXI(HSP_HERO_HALF_W), FXI(HSP_HERO_HALF_H));
    out->center = vec(g->hero.position.x, g->hero.position.y - FXI(HSP_HERO_HALF_H));
}

static void update_objects(hsp_game_t* g) {
    uint32_t i;
    sat_box2_t hero;
    hero_box(g, &hero);
    for (i = 0; i < HSP_MAX_OBJECTS; ++i) {
        hsp_object_t* o = &g->objects[i];
        sat_box2_t box;
        if (o->kind == HSP_OBJ_FREE) continue;
        (void)sat_clip_player_step(&o->clip, 0, 0, 0);
        switch (o->kind) {
            case HSP_OBJ_RING:
                if (g->mode != HSP_MODE_CLEAR && object_shape_box(o, HSP_SHAPE_PICKUP, &box) && sat_box2_overlap(&hero, &box)) {
                    ++g->stats.rings;
                    (void)sat_entity_stream2_retire(&g->stream, o->desc);
                    o->kind = HSP_OBJ_SPARKLE;
                    o->desc = HSP_NO_DESC;
                    (void)sat_clip_player_play(&o->clip, STAGE_CLIP_SPARKLE, SAT_CLIP_SWITCH_RESTART, 0, 0);
                }
                break;
            case HSP_OBJ_SPARKLE:
                if (sat_clip_player_is_finished(&o->clip)) o->kind = HSP_OBJ_FREE;
                break;
            case HSP_OBJ_DASH:
                box.center = vec(FXI(o->x), FXI(o->y) - FXI(4));
                box.half = vec(FXI(10), FXI(6));
                if (sat_box2_overlap(&hero, &box) && sat_character2_is_supported(&g->hero)) {
                    const sat_fx16_t speed = o->data == 2u ? -DASH_SPEED : DASH_SPEED;
                    if (fx_abs(g->hero.ground_speed) < DASH_SPEED || (g->hero.ground_speed < 0) != (speed < 0)) {
                        g->hero.ground_speed = speed;
                        ++g->stats.dashes;
                        shake(g, 2, 8u);
                    }
                }
                break;
            case HSP_OBJ_SPRING:
                if (sat_clip_player_is_finished(&o->clip) && sat_clip_player_clip(&o->clip) == STAGE_CLIP_SPRUNG) {
                    (void)sat_clip_player_play(&o->clip, STAGE_CLIP_SPRING, SAT_CLIP_SWITCH_RESTART, 0, 0);
                }
                box.center = vec(FXI(o->x), FXI(o->y) - FXI(6));
                box.half = vec(FXI(8), FXI(7));
                if (g->mode == HSP_MODE_RUN && sat_box2_overlap(&hero, &box) && sat_clip_player_clip(&o->clip) == STAGE_CLIP_SPRING) {
                    sat_vec2_t v = sat_character2_world_velocity(&g->hero);
                    sat_character2_detach(&g->hero);
                    g->hero.air_velocity = vec(v.x, -FXI(o->data));
                    g->stand = SAT_COLLIDER2_NONE;
                    g->jump_active = 0u;
                    ++g->stats.springs;
                    (void)sat_clip_player_play(&o->clip, STAGE_CLIP_SPRUNG, SAT_CLIP_SWITCH_RESTART, 0, 0);
                    shake(g, 2, 8u);
                }
                break;
            case HSP_OBJ_FLAG:
                box.center = vec(FXI(o->x), FXI(o->y) - FXI(16));
                box.half = vec(FXI(12), FXI(16));
                if (sat_box2_overlap(&hero, &box)) {
                    g->respawn = vec(FXI(o->x), FXI(o->y));
                    o->flags |= 1u;
                }
                break;
            case HSP_OBJ_GOAL:
                if (g->hero.position.x >= FXI(o->x) && g->mode != HSP_MODE_CLEAR) {
                    g->mode = HSP_MODE_CLEAR;
                    g->stats.cleared = 1u;
                    shake(g, 4, 40u);
                }
                break;
            default:
                break;
        }
    }
}

/* ----- the layer triggers ----- */

static void apply_trigger(hsp_game_t* g, uint32_t index) {
    const hsp_trigger_t* t = &hsp_triggers[index];
    const sat_vec2_t v = sat_character2_world_velocity(&g->hero);
    if (t->dir > 0 && v.x <= 0) return;
    if (t->dir < 0 && v.x >= 0) return;
    if (g->hero.layer != t->layer) {
        g->hero.layer = t->layer;
        ++g->stats.layer_switches;
    }
}

static void run_physics_world(hsp_game_t* g) {
    sat_physics2_event_t events[8];
    sat_physics2_step_result_t result;
    uint16_t i;
    const sat_vec2_t n = sat_character2_is_supported(&g->hero) ? g->hero.support_normal : vec(0, -SAT_FX16_ONE);
    (void)sat_physics2_set_center(&g->world, g->player_collider,
        vec(g->hero.position.x + fx_mul(n.x, FXI(HSP_HERO_HALF_H)), g->hero.position.y + fx_mul(n.y, FXI(HSP_HERO_HALF_H))));
    (void)sat_physics2_step(&g->world, events, 8u, &result);
    for (i = 0; i < result.events; ++i) {
        sat_collider2_desc_t desc;
        if (events[i].type != SAT_PHYSICS2_EVENT_ENTER || events[i].other != g->player_collider) continue;
        if (sat_physics2_get(&g->world, events[i].sensor, &desc) != SAT_OK) continue;
        apply_trigger(g, desc.user);
    }
}

/* ----- camera ----- */

static void configure_camera(hsp_game_t* g) {
    sat_follow_camera2d_config_t c;
    sat_camera_bounds2_t bounds;
    sat_follow_camera2d_config_default(&c);
    c.viewport_w = FXI(HSP_VIEW_W);
    c.viewport_h = FXI(HSP_VIEW_H);
    c.dead_half_w = FXI(10);
    c.dead_half_h = FXI(28);
    c.shift_y = FXI(8);
    c.look_max_x = FXI(56);
    c.look_gain_x = FXI(5);
    c.look_ease = SAT_FX16_ONE >> 3;
    c.follow_x = SAT_FX16_ONE >> 2;
    c.follow_y = SAT_FX16_ONE >> 3;
    c.max_step_x = FXI(24);
    c.max_step_y = FXI(14);
    c.activation_margin = FXI(64);
    c.prefetch_margin = FXI(128);
    c.predict_steps = 8u;
    c.flags = SAT_FOLLOW_CAMERA2D_PIXEL_SNAP;
    (void)sat_follow_camera2d_init(&g->camera, &c, vec(g->hero.position.x, g->hero.position.y - FXI(HSP_HERO_HALF_H)));
    bounds.min_x = 0;
    bounds.min_y = 0;
    bounds.max_x = FXI(HSP_STAGE_W);
    bounds.max_y = FXI(HSP_STAGE_H);
    (void)sat_follow_camera2d_set_bounds(&g->camera, &bounds);
}

void hsp_game_view_origin(const hsp_game_t* game, sat_fx16_t* out_x, sat_fx16_t* out_y) {
    *out_x = game->camera_out.target_x - game->camera_out.offset_x;
    *out_y = game->camera_out.target_y - game->camera_out.offset_y;
}

sat_fx16_t hsp_game_hero_speed(const hsp_game_t* game) {
    return sat_vec2_length(sat_character2_world_velocity(&game->hero));
}

/* ----- setup ----- */

static sat_result_t init_world(hsp_game_t* g) {
    sat_physics2_storage_t st;
    sat_collider2_desc_t d;
    sat_box2_t box;
    uint32_t i;
    sat_result_t r;

    r = sat_spatial_init(&g->spatial, g->spatial_heads, 48u, 6u, 6u, g->spatial_entries, HSP_COLLIDER_CAP * 9u,
        g->spatial_stamps, g->spatial_items, HSP_COLLIDER_CAP);
    if (r != SAT_OK) return r;
    st.slots = g->slots;
    st.slot_cap = HSP_COLLIDER_CAP;
    st.pairs = g->pairs;
    st.next_pairs = g->next_pairs;
    st.pair_cap = HSP_COLLIDER_CAP;
    st.candidates = g->candidates;
    st.spatial = &g->spatial;
    r = sat_physics2_world_init(&g->world, &st);
    if (r != SAT_OK) return r;

    /* the hero: a kinematic box that the layer sensors report */
    box.center = g->hero.position;
    box.half = vec(FXI(HSP_HERO_HALF_W), FXI(HSP_HERO_HALF_H));
    sat_collider2_desc_init(&d, SAT_COLLIDER2_KINEMATIC, &box);
    d.category = HSP_CAT_PLAYER;
    r = sat_physics2_add(&g->world, &d, &g->player_collider);
    if (r != SAT_OK) return r;

    for (i = 0; i < HSP_TRIGGER_COUNT; ++i) {
        const hsp_trigger_t* t = &hsp_triggers[i];
        box.half = vec(FXI(t->w) >> 1, FXI(t->h) >> 1);
        box.center = vec(FXI(t->x) + box.half.x, FXI(t->y) + box.half.y);
        sat_collider2_desc_init(&d, SAT_COLLIDER2_SENSOR, &box);
        d.category = 0u;
        d.mask = HSP_CAT_PLAYER;
        d.user = i;
        r = sat_physics2_add(&g->world, &d, &g->trigger_collider[i]);
        if (r != SAT_OK) return r;
    }

    for (i = 0; i < HSP_PLATFORM_COUNT; ++i) {
        const hsp_platform_t* def = &hsp_platforms[i];
        hsp_platform_state_t* p = &g->platform[i];
        if (def->path_kind == 0u) {
            r = sat_path2_init_quadratic(&p->path, stage_path_swing_points[0], stage_path_swing_points[1],
                stage_path_swing_points[2]);
            if (r != SAT_OK) return r;
            r = sat_path2_attach_table(&p->path, stage_path_swing_table, STAGE_PATH_SWING_TABLE_ENTRIES);
        } else {
            r = sat_path2_init_circle(&p->path, vec(FXI(def->cx), FXI(def->cy)), FXI(def->radius));
        }
        if (r != SAT_OK) return r;
        p->distance = 0;
        p->direction = 1;
        box.center = platform_position(p);
        box.half = vec(FXI(def->half_w), FXI(def->half_h));
        sat_collider2_desc_init(&d, SAT_COLLIDER2_KINEMATIC, &box);
        d.category = HSP_CAT_PLATFORM;
        d.user = i;
        if (def->one_way) {
            d.flags = SAT_COLLIDER2_ONE_WAY;
            d.one_way_face = 3u; /* solid on top: land on it from above, jump up through it */
        }
        r = sat_physics2_add(&g->world, &d, &p->collider);
        if (r != SAT_OK) return r;
    }
    return SAT_OK;
}

static sat_result_t init_terrain(hsp_game_t* g) {
    sat_result_t r = sat_terrain_map2_init(&g->terrain, stage_profiles, STAGE_PROFILE_COUNT, stage_terrain_metatiles,
        STAGE_TERRAIN_METATILE_COUNT, STAGE_TERRAIN_METATILE_SHIFT, STAGE_TERRAIN_COLS, STAGE_TERRAIN_ROWS);
    if (r != SAT_OK) return r;
    r = sat_terrain_map2_add_layer(&g->terrain, stage_terrain_layer_0);
    if (r != SAT_OK) return r;
    r = sat_terrain_map2_add_layer(&g->terrain, stage_terrain_layer_1);
    if (r != SAT_OK) return r;
    return sat_terrain_map2_set_outside(&g->terrain, STAGE_TERRAIN_OUTSIDE);
}

sat_result_t hsp_game_init(hsp_game_t* g) {
    sat_entity_stream2_config_t sc;
    sat_result_t r;
    zero_bytes(g, sizeof(*g));

    r = init_terrain(g);
    if (r != SAT_OK) return r;

    sat_character2_config_default(&g->cfg_stand);
    g->cfg_stand.segment_px = 4u;
    g->cfg_stand.max_segments = 8u;
    g->cfg_roll = g->cfg_stand;
    g->cfg_roll.head_height = 14u;
    g->cfg_roll.wall_height = 6u;
    r = sat_character2_config_validate(&g->cfg_stand);
    if (r != SAT_OK) return r;
    r = sat_character2_config_validate(&g->cfg_roll);
    if (r != SAT_OK) return r;

    g->respawn = vec(FXI(HSP_START_X), FXI(HSP_START_Y));
    g->stand = SAT_COLLIDER2_NONE;
    sat_character2_init(&g->hero, g->respawn.x, g->respawn.y);
    r = sat_character2_attach(&g->hero, &g->cfg_stand, &g->terrain);
    if (r != SAT_OK) return r;

    r = sat_path2_init_cubic(&g->rail, stage_path_rail_points[0], stage_path_rail_points[1], stage_path_rail_points[2],
        stage_path_rail_points[3]);
    if (r != SAT_OK) return r;
    r = sat_path2_attach_table(&g->rail, stage_path_rail_table, STAGE_PATH_RAIL_TABLE_ENTRIES);
    if (r != SAT_OK) return r;

    r = sat_clip_set_validate(&stage_clip_set);
    if (r != SAT_OK) return r;
    r = sat_clip_player_init(&g->hero_clip, &stage_clip_set);
    if (r != SAT_OK) return r;
    r = sat_clip_player_play(&g->hero_clip, STAGE_CLIP_IDLE, SAT_CLIP_SWITCH_RESTART, 0, 0);
    if (r != SAT_OK) return r;

    r = init_world(g);
    if (r != SAT_OK) return r;

    configure_camera(g);
    (void)sat_follow_camera2d_get(&g->camera, &g->camera_out);

    zero_bytes(&sc, sizeof(sc));
    sc.index = &stage_entity_index;
    sc.state = g->stream_state;
    sc.state_words = HSP_STREAM_WORDS;
    sc.hysteresis = FXI(32);
    sc.activate = activate_entity;
    sc.deactivate = deactivate_entity;
    sc.user = g;
    return sat_entity_stream2_init(&g->stream, &sc);
}

/* ----- one tick ----- */

void hsp_game_step(hsp_game_t* g, uint16_t held, uint16_t pressed) {
    sat_vec2_t velocity;
    sat_vec2_t target;
    sat_box2_t activation;
    ++g->stats.ticks;
    if (g->mode == HSP_MODE_CLEAR) {
        held &= (uint16_t)~(SAT_PAD_LEFT | SAT_PAD_RIGHT | JUMP_BUTTONS);
        pressed = 0u;
    }

    update_platforms(g);
    if (g->mode == HSP_MODE_RAIL) rail_step(g, pressed);
    else run_step(g, held, pressed);
    if (g->hero.position.y > FXI(HSP_DEATH_Y)) {
        ++g->stats.deaths;
        respawn(g);
    }
    run_physics_world(g);

    velocity = sat_character2_world_velocity(&g->hero);
    if (hsp_game_hero_speed(g) > g->stats.top_speed) g->stats.top_speed = hsp_game_hero_speed(g);
    target = vec(g->hero.position.x, g->hero.position.y - FXI(HSP_HERO_HALF_H));
    if (!g->in_room && g->hero.position.x >= FXI(HSP_ROOM_X)) {
        sat_camera_bounds2_t room;
        room.min_x = FXI(HSP_ROOM_X);
        room.min_y = 0;
        room.max_x = FXI(HSP_STAGE_W);
        room.max_y = FXI(HSP_STAGE_H);
        (void)sat_follow_camera2d_set_clamp(&g->camera, &room, FXI(4));
        g->in_room = 1u;
    }
    (void)sat_follow_camera2d_step(&g->camera, target, &velocity, &g->camera_out);

    (void)sat_follow_camera2d_range(&g->camera, SAT_CAMERA_RANGE_ACTIVATION, &activation);
    (void)sat_entity_stream2_update(&g->stream, &activation, 0);
    update_objects(g);
    update_hero_clip(g);
}
