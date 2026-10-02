/* Fight lifecycle: spawn, init, reset and status text. */
#include "ikemen_fight_internal.h"

static void fighter_spawn(ik_fight_t* fight, ik_fighter_t* f,
                          int16_t x, int8_t facing, int hp,
                          uint8_t owner_player) {
    f->owner_player = owner_player;
    f->state_owner = owner_player;
    f->anim_owner = owner_player;
    const ik_cns_constants_t* c = constants_for_fighter(fight, f);
    set_position(f, x, IK_FLOOR_Y);
    f->vx_q8 = 0;
    f->vy_q8 = 0;
    f->facing = facing;
    f->on_ground = 1;
    f->state = IK_STATE_IDLE;
    f->prev_state = IK_STATE_IDLE;
    f->state_time = 0;
    f->state_entries = 0u;
    f->anim_clock_pending = 0u;
    f->statedef_pending = 0u;
    f->in_guard_dist = 0u;
    f->cur_state_type = IK_CNS_STATE_STAND;
    f->cur_move_type = IK_CNS_MOVE_IDLE;
    f->anim = 0;
    f->anim_time = 0;
    f->state_axis = 0;
    f->air_jumps_used = 0u;
    f->up_latched = 0u;
    f->ctrl = 1;
    f->spr_priority = 0;
    f->hp = (int16_t)hp;
    f->pending_damage = 0;
    f->power = 0;
    f->hitstun = 0;
    f->hit_pause = 0;
    f->hit_shake_time = 0u;
    f->gravity_carry = 0u;
    f->gravity_carry_q16 = 0;
    f->edge_front = f->edge_back = 0;
    f->snap_x_q8 = f->snap_y_q8 = 0;
    f->snap_flags = 0u;
    f->reversed = 0u;
    f->frozen_tick = 0u;
    f->drop_target_skip = 0u;
    f->screen_bound = f->move_camera_x = f->move_camera_y = 1u;
    f->fine_x = f->fine_y = f->fine_vx = f->fine_vy = (ik_fine_t){0};
    f->gethit_yaccel_q16 = 0;
    f->pending_power = 0;
    f->hit_slide_time = 0;
    f->hit_ctrl_time = 0;
    f->gethit_vx_q8 = 0;
    f->gethit_vy_q8 = 0;
    f->gethit_yaccel_q8 = 0;
    f->gethit_ground_type = IK_CNS_GROUND_NORMAL;
    f->gethit_anim_type = 0u;
    f->gethit_fall = 0u;
    f->gethit_fall_x_q8 = 0;
    f->gethit_fall_y_q8 = -1152;
    f->gethit_fall_x_set = 0u;
    f->gethit_fall_recover = 1u;
    f->gethit_fall_recover_time = 4u;
    f->gethit_fall_damage = 0;
    f->gethit_fall_envshake_time = 0u;
    f->gethit_fall_envshake_ampl = 0;
    f->gethit_fall_envshake_freq = 60u;
    f->fall_time = 0u;
    f->juggle_points =
        (int16_t)((c && c->air_juggle > 0) ? c->air_juggle : 15);
    f->guard_type = IK_CNS_STATE_STAND;
    f->push_back = c ? c->ground_back : 15;
    f->push_front = c ? c->ground_front : 16;
    f->body_height = c ? c->height : 60;
    f->hitdef_hit_mask = 0u;
    f->attack_id = 0;
    f->move_contact = 0;
    f->move_hit = 0;
    f->move_contact_time = 0u;
    f->move_contact_type = 0u;
    f->afterimage_time = 0u;
    f->afterimage_length = 0u;
    f->afterimage_timegap = 1u;
    f->afterimage_framegap = 1u;
    f->afterimage_bright_rgb = 0u;
    f->afterimage_contrast_rgb = 0x00ffffffu;
    f->afterimage_add_rgb = 0u;
    f->afterimage_mul_rgb = 0x00ffffffu;
    f->palfx_time = 0u;
    f->palfx_add_r = 0;
    f->palfx_add_g = 0;
    f->palfx_add_b = 0;
    f->palfx_sin_r = 0;
    f->palfx_sin_g = 0;
    f->palfx_sin_b = 0;
    f->palfx_cycle = 1u;
    f->palfx_phase = 0u;
    f->palfx_mul_r = 256u;
    f->palfx_mul_g = 256u;
    f->palfx_mul_b = 256u;
    f->palfx_sinmul_r = 0;
    f->palfx_sinmul_g = 0;
    f->palfx_sinmul_b = 0;
    f->palfx_sinmul_cycle = 1u;
    f->palfx_sinmul_phase = 0u;
    f->active_hitdef_local = -1;
    f->active_hitdef_global = -1;
    f->active_hitdef_secondary = 0u;
    f->pos_freeze_x = 0u;
    f->pos_freeze_y = 0u;
    f->pause_fired = 0u;
    f->one_shot_controller_mask = 0u;
    f->not_hit_by_mask = 0u;
    f->not_hit_by_attr_mask = 0u;
    f->not_hit_by_time = 0u;
    f->target_index = -1;
    f->hitdef_target = -1;
    f->juggle_owner = -1;
    f->target_id = -1;
    f->last_hit_owner = -1;
    f->last_hit_id = -1;
    f->bound_to = -1;
    f->bind_ticks = 0u;
    f->xmin_q8 = (int32_t)IK_STAGE_MIN_X * IK_CNS_Q8_ONE;
    f->xmax_q8 = (int32_t)IK_STAGE_MAX_X * IK_CNS_Q8_ONE;
    f->bound_entity = ik_entity_invalid_handle();
}

int ik_fight_max_hp_player(
    const ik_fight_t* fight, uint8_t player
) {
    if (fight && player < 2u) {
        const ik_cns_asset_t* cns = cns_for_owner(fight, player);
        if (cns && cns->constants.life > 0) {
            return cns->constants.life;
        }
    }
    return IK_MAX_HP;
}

int ik_fight_max_hp(const ik_fight_t* fight) {
    return ik_fight_max_hp_player(fight, 0u);
}

void ik_fight_init_players(
    ik_fight_t* fight,
    const ik_cns_asset_t* p1_cns,
    const ik_cns_asset_t* p2_cns
) {
    if (!fight) return;
    if (!p1_cns) p1_cns = p2_cns;
    if (!p2_cns) p2_cns = p1_cns;

    fight->cns = p1_cns;
    fight->player_cns[0] = p1_cns;
    fight->player_cns[1] = p2_cns;
    fight->player_frames[0] = 0;
    fight->player_frames[1] = 0;
    fight->entities = 0;
    fight->player_entities[0] = ik_entity_invalid_handle();
    fight->player_entities[1] = ik_entity_invalid_handle();

    {
        const ik_stage_params_t stage0 = {
            -125, 125, 50, 15, 15, -1000, 1000, -70, 70
        };
        fight->stage = stage0;
    }

    fighter_spawn(
        fight, &fight->fighters[0],
        IK_STAGE_CENTER_X + fight->stage.p1_start_x, 1,
        ik_fight_max_hp_player(fight, 0u), 0u);
    fighter_spawn(
        fight, &fight->fighters[1],
        IK_STAGE_CENTER_X + fight->stage.p2_start_x, -1,
        ik_fight_max_hp_player(fight, 1u), 1u);

    fight->frame = 0;
    fight->timer_frames = IK_ROUND_TIME_FRAMES;
    fight->events = IK_EVENT_NONE;
    fight->round_over = 0;
    fight->winner = 0;
    fight->hits_p1 = 0;
    fight->hits_p2 = 0;
    fight->ko_freeze = 0;
    fight->pause_time = 0u;
    fight->pause_move_time = 0u;
    fight->pause_end_cmd_buffer_time = 0u;
    fight->pause_owner = -1;
    fight->pause_is_super = 0u;
    fight->env_shake_time = 0u;
    fight->env_shake_ampl = 0;
    fight->env_shake_freq = 60u;
    fight->env_shake_phase = 0u;
    fight->super_darken_time = 0u;

    const int p1_intro =
        p1_cns && ik_cns_find_state(p1_cns, 191);
    const int p2_intro =
        p2_cns && ik_cns_find_state(p2_cns, 191);
    fight->round_state = (p1_intro || p2_intro) ? 0u : 2u;
    fight->intro_asserted = 0u;
    fight->round_wait = 0u;
    if (fight->round_state == 0u) {
        if (p1_intro) {
            ikf_enter_state(fight, &fight->fighters[0], 191);
        }
        if (p2_intro) {
            ikf_enter_state(fight, &fight->fighters[1], 191);
        }
    }

    fight->effect_count = 0u;
    fight->sound_count = 0u;
    ikf_camera_reset(fight);
}

void ik_fight_set_stage(
    ik_fight_t* fight, const ik_stage_params_t* stage
) {
    if (!fight || !stage) return;
    fight->stage = *stage;
    ikf_camera_reset(fight);
}

void ik_fight_init(ik_fight_t* fight, const ik_cns_asset_t* cns) {
    ik_fight_init_players(fight, cns, cns);
}

void ik_fight_set_player_cns(
    ik_fight_t* fight, uint8_t player, const ik_cns_asset_t* cns
) {
    if (!fight || player >= 2u) return;
    fight->player_cns[player] = cns ? cns : fight->cns;
}

void ik_fight_bind_entities(
    ik_fight_t* fight,
    ik_entity_pool_t* pool,
    ik_entity_handle_t p1,
    ik_entity_handle_t p2
) {
    if (!fight) return;
    fight->entities = pool;
    fight->player_entities[0] = p1;
    fight->player_entities[1] = p2;
}

void ik_fight_reset(ik_fight_t* fight) {
    if (!fight) return;
    const uint32_t h1 = fight->hits_p1;
    const uint32_t h2 = fight->hits_p2;
    const ik_stage_params_t stage = fight->stage;
    const ik_cns_asset_t* cns = fight->cns;
    const ik_cns_asset_t* p1_cns = fight->player_cns[0];
    const ik_cns_asset_t* p2_cns = fight->player_cns[1];
    ik_entity_pool_t* entities = fight->entities;
    const ik_entity_handle_t p1 = fight->player_entities[0];
    const ik_entity_handle_t p2 = fight->player_entities[1];

    /* Dynamic entities are round-scoped. Keep root player handles stable,
     * but retire helpers/projectiles/explods so stale references fail by
     * generation after reset. */
    if (entities) {
        for (uint8_t slot = 0u; slot < IK_ENTITY_CAPACITY; ++slot) {
            if (entities->entities[slot].type == IK_ENTITY_NONE ||
                entities->entities[slot].type == IK_ENTITY_PLAYER) {
                continue;
            }
            ik_entity_handle_t handle = {
                slot, entities->generations[slot]
            };
            (void)ik_entity_destroy(entities, handle);
        }
    }

    (void)cns;
    ik_fight_init_players(fight, p1_cns, p2_cns);
    ik_fight_set_stage(fight, &stage);
    ik_fight_bind_entities(fight, entities, p1, p2);
    fight->hits_p1 = h1;
    fight->hits_p2 = h2;
    fight->events = IK_EVENT_RESET;
    ikf_sync_player_entities(fight);
}

const char* ik_fight_status_text(const ik_fight_t* fight) {
    if (!fight) return 0;
    if (fight->round_over) {
        if (fight->winner == 1) return "P1 WINS - START RESETS";
        if (fight->winner == 2) return "P2 WINS - START RESETS";
        return "TIME OVER - START RESETS";
    }
    return 0;
}

int ik_fight_status_needs_start(const ik_fight_t* fight) {
    return fight != 0 && fight->round_over != 0;
}
