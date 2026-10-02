/* Applying a resolved hit from a helper/projectile owner. */
#include "ikemen_fight_internal.h"

static void enter_entity_contact_state(
    ik_fight_t* fight,
    ik_entity_handle_t handle,
    int16_t state,
    const ik_frame_table_t* p1_frames,
    const ik_frame_table_t* p2_frames
) {
    if (!fight || !fight->entities) return;
    ik_entity_runtime_t runtime;
    ik_entity_runtime_init(
        &runtime, fight->entities, fight->cns,
        p1_frames, p2_frames);
    ikf_configure_fight_entity_runtime(fight, &runtime);
    (void)ik_entity_runtime_enter_state(&runtime, handle, state);
}

void ikf_apply_guard_from_entity(
    ik_fight_t* fight,
    ik_entity_handle_t attacker_handle,
    int victim,
    const ik_fight_controls_t* controls,
    const ik_cns_hitdef_t* hitdef
) {
    if (!fight || !fight->entities || !hitdef ||
        victim < 0 || victim > 1) {
        return;
    }
    ik_entity_t* attacker =
        ik_entity_get(fight->entities, attacker_handle);
    if (!attacker || attacker->owner_player >= 2u) return;

    ik_fighter_t* v = &fight->fighters[victim];
    const uint8_t type = ikf_guard_type_for(fight, v, controls);

    int guard_ko = 0;
    if (hitdef->guard_damage > 0) {
        v->hp = (int16_t)(v->hp - hitdef->guard_damage);
        if (v->hp <= 0) {
            if (hitdef->guard_kill) {
                v->hp = 0;
                guard_ko = 1;
            } else {
                v->hp = 1;
            }
        }
    }

    v->hit_shake_time = hitdef->pause_p2;
    attacker->hit_pause = hitdef->pause_p1;
    v->hitstun = hitdef->guard_hit_time;
    v->hit_slide_time = hitdef->guard_slide_time;
    v->hit_ctrl_time = hitdef->guard_ctrl_time;
    v->guard_type = type;

    if (type == IK_CNS_STATE_AIR) {
        v->gethit_vx_q8 = hitdef->air_guard_velocity_x_q8;
        v->gethit_vy_q8 = hitdef->air_guard_velocity_y_q8;
    } else {
        v->gethit_vx_q8 = hitdef->guard_velocity_x_q8;
        v->gethit_vy_q8 = 0;
    }

    attacker->move_contact = 1u;
    ikf_credit_hit_power(
        &fight->fighters[attacker->owner_player], v, hitdef, 1);

    int16_t state = 150;
    if (type == IK_CNS_STATE_CROUCH) state = 152;
    else if (type == IK_CNS_STATE_AIR) state = 154;

    const ik_cns_asset_t* victim_cns =
        cns_for_owner(fight, v->owner_player);
    if (ik_cns_find_state(victim_cns, state)) {
        v->cur_move_type = IK_CNS_MOVE_HIT;
        v->juggle_owner = (int8_t)attacker->owner_player;
        v->ctrl = 0;
        ikf_enter_state_deferred(fight, v, state);
    } else {
        v->cur_move_type = IK_CNS_MOVE_HIT;
        v->juggle_owner = (int8_t)attacker->owner_player;
        v->ctrl = 0;
        ikf_enter_state_deferred(
            fight, v,
            type == IK_CNS_STATE_CROUCH ? 131 :
            type == IK_CNS_STATE_AIR ? 132 : 130);
    }

    ikf_queue_entity_hit_effect(
        fight, attacker, v, hitdef, hitdef->guard_spark_no, 0);
    ikf_queue_sound_event(
        fight, hitdef->guard_sound_group, hitdef->guard_sound_item);
    fight->events |= IK_EVENT_GUARD;
    if (guard_ko) {
        fight->winner = (uint8_t)(attacker->owner_player + 1u);
        fight->events |= IK_EVENT_KO;
        if (!ik_cns_find_state(victim_cns, 5050)) {
            v->cur_move_type = IK_CNS_MOVE_HIT;
            v->juggle_owner = (int8_t)attacker->owner_player;
            v->ctrl = 0;
            ikf_enter_state_deferred(fight, v, IK_STATE_KO);
            fight->round_over = 1;
            fight->events |= IK_EVENT_ROUND_OVER;
            fight->ko_freeze = IK_KO_FREEZE_FRAMES;
        }
    }
}

void ikf_apply_damage_from_entity(
    ik_fight_t* fight,
    ik_entity_handle_t attacker_handle,
    int victim,
    const ik_cns_hitdef_t* hitdef,
    const ik_frame_table_t* p1_frames,
    const ik_frame_table_t* p2_frames
) {
    if (!fight || !fight->entities || !hitdef ||
        victim < 0 || victim > 1) {
        return;
    }
    ik_entity_t* attacker =
        ik_entity_get(fight->entities, attacker_handle);
    if (!attacker || attacker->owner_player >= 2u) return;

    ik_fighter_t* v = &fight->fighters[victim];
    (void)ik_entity_add_target(
        fight->entities, attacker_handle,
        fight->player_entities[victim], hitdef->id);
    v->last_hit_owner = (int8_t)attacker->owner_player;
    v->last_hit_id = hitdef->id;
    ikf_release_bound_target(fight, victim);
    ikf_release_entity_bound_fighter(fight, v);

    const uint8_t victim_type = ik_fight_state_type(fight, v);
    const int downed = victim_type == IK_CNS_STATE_LIEDOWN;
    const int airborne = !v->on_ground || victim_type == IK_CNS_STATE_AIR;
    const int16_t velocity_x = downed
        ? hitdef->down_velocity_x_q8
        : airborne
            ? hitdef->air_velocity_x_q8
            : hitdef->ground_velocity_x_q8;
    const int16_t velocity_y = downed
        ? hitdef->down_velocity_y_q8
        : airborne
            ? hitdef->air_velocity_y_q8
            : hitdef->ground_velocity_y_q8;
    const int16_t hit_time = downed
        ? (velocity_y == 0
            ? (int16_t)hitdef->down_hit_time
            : (int16_t)hitdef->air_hit_time)
        : airborne
            ? (int16_t)hitdef->air_hit_time
            : (int16_t)hitdef->ground_hit_time;
    const int was_juggle_target = ikf_is_juggle_target(fight, v);
    const int attack_juggle =
        ikf_entity_juggle_cost(fight, attacker, hitdef);
    const int downed_launch = downed && velocity_y != 0;
    const int launch = airborne || downed_launch ||
        (hitdef->flags & IK_CNS_HITDEF_FALL) != 0u ||
        velocity_y != 0;

    int damage = hitdef->damage;
    if (hitdef->has_alt_damage &&
        attacker->prev_state_no == hitdef->alt_damage_prev_state) {
        damage = hitdef->alt_damage;
    }

    v->pending_damage = (int16_t)(v->pending_damage + damage);
    const int hp_after = v->hp - v->pending_damage;
    /* Upstream's HitOver is hittime < 0, so it takes hit_time + 1 ticks. */
    v->hitstun = (uint16_t)(hit_time < 0 ? 0 : hit_time + 1);
    v->hit_shake_time = hitdef->pause_p2;
    v->hit_slide_time = downed && velocity_y == 0
        ? hitdef->down_hit_time
        : hitdef->ground_slide_time;
    v->hit_ctrl_time = (uint16_t)(hit_time < 0 ? 0 : hit_time);
    v->gethit_vx_q8 = velocity_x;
    v->gethit_vy_q8 = velocity_y;
    v->gethit_yaccel_q8 = hitdef->yaccel_q8;
    v->gethit_yaccel_q16 = hitdef->yaccel_q16;
    v->gethit_ground_type = hitdef->ground_type;
    v->gethit_fall = (uint8_t)(
        ((hitdef->flags & IK_CNS_HITDEF_FALL) != 0u) ||
        (airborne &&
         (hitdef->flags & IK_CNS_HITDEF_AIR_FALL) != 0u) ||
        downed_launch);
    v->gethit_anim_type = ikf_gethit_anim_type(
        hitdef, v->gethit_fall, airborne, velocity_y);
    v->gethit_fall_x_q8 = hitdef->fall_x_velocity_q8;
    v->gethit_fall_y_q8 =
        (downed_launch && !hitdef->down_bounce)
            ? 0 : hitdef->fall_y_velocity_q8;
    v->gethit_fall_x_set =
        (uint8_t)(hitdef->fall_x_velocity_set &&
                  (!downed_launch || hitdef->down_bounce));
    v->gethit_fall_recover = hitdef->fall_recover;
    v->gethit_fall_recover_time = hitdef->fall_recover_time;
    v->gethit_fall_damage = hitdef->fall_damage;
    v->gethit_fall_envshake_time = hitdef->fall_envshake_time;
    v->gethit_fall_envshake_ampl = hitdef->fall_envshake_ampl;
    v->gethit_fall_envshake_freq = hitdef->fall_envshake_freq;
    v->fall_time = 0u;

    if (was_juggle_target) {
        v->juggle_points = (int16_t)(
            v->juggle_points > attack_juggle
                ? v->juggle_points - attack_juggle : 0);
    } else if ((hitdef->flags & IK_CNS_HITDEF_FALL) != 0u) {
        const ik_cns_constants_t* constants =
            constants_for_fighter(fight, v);
        const int initial =
            (constants && constants->air_juggle > 0)
                ? constants->air_juggle : 15;
        v->juggle_points = (int16_t)(
            initial > attack_juggle
                ? initial - attack_juggle : 0);
    }

    attacker->hit_pause = hitdef->pause_p1;
    if (hitdef->envshake_time > 0u) {
        fight->env_shake_time = hitdef->envshake_time;
        fight->env_shake_ampl = hitdef->envshake_ampl;
        fight->env_shake_freq = hitdef->envshake_freq;
        fight->env_shake_phase = 0u;
    }
    attacker->move_contact = 1u;
    ikf_credit_hit_power(
        &fight->fighters[attacker->owner_player], v, hitdef, 0);
    attacker->move_hit = 1u;
    if (launch) v->on_ground = 0;

    if (!airborne && !downed &&
        hitdef->ground_cornerpush_veloff_q8 != 0) {
        int left = 0, top = 0, right = 0, bottom = 0;
        ik_body_box(v, &left, &top, &right, &bottom);
        if (left <= (v->xmin_q8 / IK_CNS_Q8_ONE) ||
            right >= (v->xmax_q8 / IK_CNS_Q8_ONE)) {
            attacker->vx_q8 =
                (int32_t)attacker->facing *
                hitdef->ground_cornerpush_veloff_q8;
        }
    }

    int16_t target = IK_STATE_HIT;
    int custom_p2_state = 0;
    const ik_cns_asset_t* attacker_cns =
        cns_for_owner(fight, attacker->state_owner);
    const ik_cns_asset_t* victim_cns =
        cns_for_owner(fight, v->owner_player);
    if (hitdef->p2_state_no >= 0 &&
        ik_cns_find_state(attacker_cns, hitdef->p2_state_no)) {
        target = hitdef->p2_state_no;
        custom_p2_state = 1;
    } else if (victim_type == IK_CNS_STATE_LIEDOWN &&
               ik_cns_find_state(victim_cns, 5080)) {
        target = 5080;
    } else if (!airborne &&
               hitdef->ground_type == IK_CNS_GROUND_TRIP &&
               ik_cns_find_state(victim_cns, 5070)) {
        target = 5070;
    } else if (airborne &&
               ik_cns_find_state(victim_cns, 5020)) {
        target = 5020;
    } else if (victim_type == IK_CNS_STATE_CROUCH &&
               (hitdef->flags & IK_CNS_HITDEF_FORCE_STAND) == 0u &&
               ik_cns_find_state(victim_cns, 5010)) {
        target = 5010;
    } else if (ik_cns_find_state(victim_cns, 5000)) {
        target = 5000;
    }

    if (hitdef->p2_facing != 0) {
        const int attacker_x = ik_cns_q8_to_int(attacker->x_q8);
        const int8_t toward = attacker_x >= v->x ? 1 : -1;
        v->facing =
            hitdef->p2_facing > 0 ? toward : (int8_t)-toward;
    }

    /* A compiled get-hit state keeps the current velocity until its StateDef
     * (velset) applies on the victim's next tick. */
    if (target == IK_STATE_HIT) {
        v->vx_q8 = (int32_t)v->facing * velocity_x;
        v->vy_q8 = velocity_y;
    }

    ikf_queue_entity_hit_effect(
        fight, attacker, v, hitdef, hitdef->spark_no, 1);
    ikf_queue_sound_event(
        fight, hitdef->hit_sound_group, hitdef->hit_sound_item);
    if (hp_after <= 0) {
        v->pending_damage = v->hp;
        fight->winner = (uint8_t)(attacker->owner_player + 1u);
        fight->events |= (uint16_t)(IK_EVENT_HIT | IK_EVENT_KO);
    } else {
        fight->events |= IK_EVENT_HIT;
    }

    if (target == IK_STATE_HIT && hp_after <= 0) {
        v->cur_move_type = IK_CNS_MOVE_HIT;
        v->juggle_owner = (int8_t)attacker->owner_player;
        v->ctrl = 0;
        ikf_enter_state_deferred(fight, v, IK_STATE_KO);
        fight->round_over = 1;
        fight->events |= IK_EVENT_ROUND_OVER;
        fight->ko_freeze = IK_KO_FREEZE_FRAMES;
    } else {
        if (custom_p2_state) v->state_owner = attacker->state_owner;
        v->cur_move_type = IK_CNS_MOVE_HIT;
        v->juggle_owner = (int8_t)attacker->owner_player;
        v->ctrl = 0;
        ikf_enter_state_deferred(fight, v, target);
    }

    if (hitdef->p1_state_no >= 0 &&
        ik_cns_find_state(
            cns_for_owner(fight, attacker->state_owner),
            hitdef->p1_state_no)) {
        enter_entity_contact_state(
            fight, attacker_handle, hitdef->p1_state_no,
            p1_frames, p2_frames);
    }

    if (attacker->owner_player == 0u) ++fight->hits_p1;
    else ++fight->hits_p2;
}

void ikf_apply_throw_from_entity(
    ik_fight_t* fight,
    ik_entity_handle_t attacker_handle,
    int victim,
    const ik_cns_hitdef_t* hitdef,
    const ik_frame_table_t* p1_frames,
    const ik_frame_table_t* p2_frames
) {
    if (!fight || !fight->entities || !hitdef ||
        victim < 0 || victim > 1) {
        return;
    }

    ik_entity_t* attacker =
        ik_entity_get(fight->entities, attacker_handle);
    if (!attacker || attacker->type != IK_ENTITY_HELPER ||
        attacker->owner_player >= 2u) {
        return;
    }

    ik_fighter_t* v = &fight->fighters[victim];
    ikf_release_bound_target(fight, victim);
    ikf_release_entity_bound_fighter(fight, v);

    (void)ik_entity_add_target(
        fight->entities, attacker_handle,
        fight->player_entities[victim], hitdef->id);
    v->bound_to = -1;
    v->bound_entity = attacker_handle;
    v->last_hit_owner = (int8_t)attacker->owner_player;
    v->last_hit_id = hitdef->id;
    attacker->move_contact = 1u;
    ikf_credit_hit_power(
        &fight->fighters[attacker->owner_player], v, hitdef, 0);
    attacker->move_hit = 1u;

    if (hitdef->p1_facing != 0) {
        const int attacker_x =
            ik_cns_q8_to_int(attacker->x_q8);
        const int8_t toward = v->x >= attacker_x ? 1 : -1;
        attacker->facing =
            hitdef->p1_facing > 0 ? toward : (int8_t)-toward;
    }
    if (hitdef->p2_facing != 0) {
        const int attacker_x =
            ik_cns_q8_to_int(attacker->x_q8);
        const int8_t toward = attacker_x >= v->x ? 1 : -1;
        v->facing =
            hitdef->p2_facing > 0 ? toward : (int8_t)-toward;
    }

    v->gethit_fall =
        (uint8_t)((hitdef->flags & IK_CNS_HITDEF_FALL) != 0u);
    v->gethit_fall_x_q8 = hitdef->fall_x_velocity_q8;
    v->gethit_fall_y_q8 = hitdef->fall_y_velocity_q8;
    v->gethit_fall_x_set = hitdef->fall_x_velocity_set;
    v->gethit_fall_recover = hitdef->fall_recover;
    v->gethit_fall_recover_time = hitdef->fall_recover_time;

    if (hitdef->p2_state_no >= 0) {
        v->state_owner = attacker->state_owner;
        v->cur_move_type = IK_CNS_MOVE_HIT;
        v->juggle_owner = (int8_t)attacker->owner_player;
        v->ctrl = 0;
        ikf_enter_state_deferred(fight, v, hitdef->p2_state_no);
    }
    if (hitdef->p1_state_no >= 0) {
        enter_entity_contact_state(
            fight, attacker_handle, hitdef->p1_state_no,
            p1_frames, p2_frames);
        attacker = ik_entity_get(
            fight->entities, attacker_handle);
        if (attacker && hitdef->p1_spr_priority != -128) {
            attacker->spr_priority = hitdef->p1_spr_priority;
        }
    }

    attacker = ik_entity_get(fight->entities, attacker_handle);
    if (attacker) {
        ikf_queue_entity_hit_effect(
            fight, attacker, v, hitdef, hitdef->spark_no, 1);
    }
    ikf_queue_sound_event(
        fight, hitdef->hit_sound_group, hitdef->hit_sound_item);
    fight->events |= IK_EVENT_HIT;
    if (attacker && attacker->owner_player == 0u) ++fight->hits_p1;
    else ++fight->hits_p2;
}
