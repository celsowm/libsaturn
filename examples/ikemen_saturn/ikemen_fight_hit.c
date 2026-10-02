/* Applying a resolved hit from a root fighter: guard, throw, damage. */
#include "ikemen_fight_internal.h"

void ikf_release_bound_target(ik_fight_t* fight, int owner) {
    if (!fight || owner < 0 || owner > 1) return;
    ik_fighter_t* f = &fight->fighters[owner];
    if (f->target_index < 0 || f->target_index > 1) return;
    ik_fighter_t* target = &fight->fighters[(int)f->target_index];
    if (target->bound_to == owner) {
        target->bound_to = -1;
        target->bound_entity = ik_entity_invalid_handle();
    }
    f->target_index = -1;
    f->target_id = -1;
}

/* A fighter that leaves MoveType H stops being anyone's target, forgets who
 * juggled it and is released from any bind (upstream exitTarget). */
void ikf_exit_target(ik_fight_t* fight, ik_fighter_t* f) {
    const uint8_t self = ikf_fighter_player_index(fight, f);
    f->juggle_owner = -1;
    if (f->bound_to >= 0) {
        f->bound_to = -1;
        f->bound_entity = ik_entity_invalid_handle();
    }
    if (self < 2u) {
        ik_fighter_t* other = &fight->fighters[self ^ 1u];
        if (other->target_index == (int8_t)self) {
            other->target_index = -1;
            other->target_id = -1;
        }
    }
}

void ikf_release_entity_bound_fighter(
    ik_fight_t* fight,
    ik_fighter_t* fighter
) {
    if (!fight || !fighter ||
        !ik_entity_handle_is_valid(fighter->bound_entity)) {
        return;
    }
    if (fight->entities) {
        const ik_entity_t* source =
            ik_entity_get_const(
                fight->entities, fighter->bound_entity);
        if (source) {
            const uint8_t player =
                ikf_fighter_player_index(fight, fighter);
            if (player < 2u) {
                (void)ik_entity_remove_target(
                    fight->entities, fighter->bound_entity,
                    fight->player_entities[player]);
            }
        }
    }
    fighter->bound_entity = ik_entity_invalid_handle();
}

void ikf_apply_guard(ik_fight_t* fight, int victim,
                        const ik_fight_controls_t* controls,
                        const ik_cns_hitdef_t* hitdef) {
    ik_fighter_t* v = &fight->fighters[victim];
    ik_fighter_t* a = &fight->fighters[victim ^ 1];
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
    a->hit_pause = hitdef->pause_p1;
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

    a->move_contact = 1u;
    a->move_contact_time = 0u; /* a fresh contact restamps as 1 */
    ikf_credit_hit_power(a, v, hitdef, 1);
    ikf_hit_snap(a, v, hitdef, 1);
    a->target_index = (int8_t)victim;
    a->target_id = hitdef->id;
    a->hitdef_target = (int8_t)victim;

    int16_t state = 150;
    if (type == IK_CNS_STATE_CROUCH) state = 152;
    else if (type == IK_CNS_STATE_AIR) state = 154;

    const ik_cns_asset_t* victim_cns =
        cns_for_owner(fight, v->owner_player);
    if (ik_cns_find_state(victim_cns, state)) {
        v->cur_move_type = IK_CNS_MOVE_HIT;
        v->juggle_owner = (int8_t)ikf_fighter_player_index(fight, a);
        v->ctrl = 0;
        ikf_enter_state_deferred(fight, v, state);
    } else {
        v->cur_move_type = IK_CNS_MOVE_HIT;
        v->juggle_owner = (int8_t)ikf_fighter_player_index(fight, a);
        v->ctrl = 0;
        ikf_enter_state_deferred(fight, v,
                    type == IK_CNS_STATE_CROUCH ? 131 :
                    type == IK_CNS_STATE_AIR ? 132 : 130);
    }

    ikf_queue_hit_effect(
        fight, a, v, hitdef, hitdef->guard_spark_no, 0);
    ikf_queue_sound_event(
        fight, hitdef->guard_sound_group, hitdef->guard_sound_item);
    fight->events |= IK_EVENT_GUARD;
    if (guard_ko) {
        fight->winner = (uint8_t)((victim ^ 1) + 1);
        fight->events |= IK_EVENT_KO;
        if (!ik_cns_find_state(victim_cns, 5050)) {
            v->cur_move_type = IK_CNS_MOVE_HIT;
            v->juggle_owner = (int8_t)ikf_fighter_player_index(fight, a);
            v->ctrl = 0;
            ikf_enter_state_deferred(fight, v, IK_STATE_KO);
            fight->round_over = 1;
            fight->events |= IK_EVENT_ROUND_OVER;
            fight->ko_freeze = IK_KO_FREEZE_FRAMES;
        }
    }
}

void ikf_apply_throw(ik_fight_t* fight, int attacker,
                        const ik_fight_controls_t* attacker_controls,
                        const ik_cns_hitdef_t* hitdef) {
    if (!fight || !hitdef || attacker < 0 || attacker > 1) return;
    const int victim = attacker ^ 1;
    ik_fighter_t* a = &fight->fighters[attacker];
    ik_fighter_t* v = &fight->fighters[victim];

    a->target_index = (int8_t)victim;
    a->target_id = hitdef->id;
    a->hitdef_target = (int8_t)victim;
    v->last_hit_owner = (int8_t)attacker;
    v->last_hit_id = hitdef->id;
    v->bound_to = (int8_t)attacker;
    v->bound_entity = ik_entity_invalid_handle();
    a->move_contact = 1u;
    a->move_contact_time = 0u; /* a fresh contact restamps as 1 */
    ikf_credit_hit_power(a, v, hitdef, 0);
    ikf_hit_snap(a, v, hitdef, 0);
    a->move_hit = 1u;

    if (hitdef->p1_facing != 0) {
        const int8_t toward = v->x >= a->x ? 1 : -1;
        a->facing = hitdef->p1_facing > 0 ? toward : (int8_t)-toward;
    }
    if (hitdef->p2_facing != 0) {
        const int8_t toward = a->x >= v->x ? 1 : -1;
        v->facing = hitdef->p2_facing > 0 ? toward : (int8_t)-toward;
    }
    v->gethit_fall =
        (uint8_t)((hitdef->flags & IK_CNS_HITDEF_FALL) != 0u);
    v->gethit_fall_x_q8 = hitdef->fall_x_velocity_q8;
    v->gethit_fall_y_q8 = hitdef->fall_y_velocity_q8;
    v->gethit_fall_x_set = hitdef->fall_x_velocity_set;
    v->gethit_fall_recover = hitdef->fall_recover;
    v->gethit_fall_recover_time = hitdef->fall_recover_time;

    if (hitdef->p2_state_no >= 0) {
        v->state_owner = a->state_owner;
        v->cur_move_type = IK_CNS_MOVE_HIT;
        v->juggle_owner = (int8_t)ikf_fighter_player_index(fight, a);
        v->ctrl = 0;
        ikf_enter_state_deferred(fight, v, hitdef->p2_state_no);
    }
    if (hitdef->p1_state_no >= 0) {
        ikf_enter_state_deferred(fight, a, hitdef->p1_state_no);
        if (hitdef->p1_spr_priority != -128) {
            a->spr_priority = hitdef->p1_spr_priority;
        }
        /* KFM state 810 snapshots command="holdfwd" at Time=0. The throw
         * changes state during collision resolution, so preserve that entry
         * input in the generic state-axis scratch immediately. */
        if (attacker_controls) {
            if (attacker_controls->forward) a->state_axis = 1;
            else if (attacker_controls->back) a->state_axis = -1;
        }
    }

    ikf_queue_hit_effect(
        fight, a, v, hitdef, hitdef->spark_no, 1);
    ikf_queue_sound_event(
        fight, hitdef->hit_sound_group, hitdef->hit_sound_item);
    fight->events |= IK_EVENT_HIT;
    if (attacker == 0) ++fight->hits_p1;
    else ++fight->hits_p2;
}

void ikf_apply_damage(ik_fight_t* fight, int victim,
                         const ik_cns_hitdef_t* hitdef) {
    ik_fighter_t* v = &fight->fighters[victim];
    const int attacker = victim ^ 1;
    fight->fighters[attacker].target_index = (int8_t)victim;
    fight->fighters[attacker].target_id = hitdef ? hitdef->id : 0;
    fight->fighters[attacker].hitdef_target = (int8_t)victim;
    v->last_hit_owner = (int8_t)attacker;
    v->last_hit_id = hitdef ? hitdef->id : 0;
    /* Losing a throw owner releases its bound target. State 820's compiled
     * !isbound SelfState then returns the target to its own fall graph. */
    ikf_release_bound_target(fight, victim);
    ikf_release_entity_bound_fighter(fight, v);
    ik_fighter_t* a = &fight->fighters[victim ^ 1];

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
    const int attack_juggle = ikf_juggle_cost(fight, a, hitdef);
    const int downed_launch = downed && velocity_y != 0;
    const int launch = airborne || downed_launch ||
        (hitdef->flags & IK_CNS_HITDEF_FALL) != 0u ||
        velocity_y != 0;

    int damage = hitdef->damage;
    if (hitdef->has_alt_damage &&
        a->prev_state == hitdef->alt_damage_prev_state) {
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
    /* A liedown victim launched by down.velocity always enters the fall
     * graph so it returns to a downed state on landing. down.bounce only
     * controls whether state 5100 receives a non-zero fall Y velocity and
     * therefore proceeds through the single 5101 ground bounce. */
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
            ? 0
            : hitdef->fall_y_velocity_q8;
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
                ? v->juggle_points - attack_juggle
                : 0);
    } else if ((hitdef->flags & IK_CNS_HITDEF_FALL) != 0u) {
        const ik_cns_constants_t* c = constants_for_fighter(fight, v);
        const int initial = (c && c->air_juggle > 0) ? c->air_juggle : 15;
        v->juggle_points =
            (int16_t)(initial > attack_juggle ? initial - attack_juggle : 0);
    }

    a->hit_pause = hitdef->pause_p1;

    if (hitdef->envshake_time > 0u) {
        fight->env_shake_time = hitdef->envshake_time;
        fight->env_shake_ampl = hitdef->envshake_ampl;
        fight->env_shake_freq = hitdef->envshake_freq;
        fight->env_shake_phase = 0u;
    }

    if (launch) v->on_ground = 0;

    a->move_contact = 1u;
    a->move_contact_time = 0u; /* a fresh contact restamps as 1 */
    ikf_credit_hit_power(a, v, hitdef, 0);
    ikf_hit_snap(a, v, hitdef, 0);
    a->move_hit = 1u;

    if (!airborne && !downed &&
        hitdef->ground_cornerpush_veloff_q8 != 0) {
        int left=0, top=0, right=0, bottom=0;
        ik_body_box(v, &left, &top, &right, &bottom);
        if (left <= (v->xmin_q8 / IK_CNS_Q8_ONE) ||
            right >= (v->xmax_q8 / IK_CNS_Q8_ONE)) {
            a->vx_q8 =
                (int32_t)a->facing * hitdef->ground_cornerpush_veloff_q8;
        }
    }

    {
        int16_t target = IK_STATE_HIT;
        int custom_p2_state = 0;
        const ik_cns_asset_t* attacker_cns = cns_for_fighter(fight, a);
        const ik_cns_asset_t* victim_cns =
            cns_for_owner(fight, v->owner_player);
        if (hitdef->p2_state_no >= 0 &&
            ik_cns_find_state(attacker_cns, hitdef->p2_state_no)) {
            target = hitdef->p2_state_no;
            custom_p2_state = 1;
        } else if (victim_type == IK_CNS_STATE_LIEDOWN &&
            ik_cns_find_state(victim_cns, 5080)) {
            target = 5080;
        } else if (!airborne && hitdef->ground_type == IK_CNS_GROUND_TRIP &&
                   ik_cns_find_state(victim_cns, 5070)) {
            target = 5070;
        } else if (airborne && ik_cns_find_state(victim_cns, 5020)) {
            target = 5020;
        } else if (victim_type == IK_CNS_STATE_CROUCH &&
                   (hitdef->flags & IK_CNS_HITDEF_FORCE_STAND) == 0u &&
                   ik_cns_find_state(victim_cns, 5010)) {
            target = 5010;
        } else if (ik_cns_find_state(victim_cns, 5000)) {
            target = 5000;
        }

        if (hitdef->p2_facing != 0) {
            const int8_t toward = a->x >= v->x ? 1 : -1;
            v->facing =
                hitdef->p2_facing > 0 ? toward : (int8_t)-toward;
        }

        /* A compiled get-hit state keeps the current velocity until its
         * StateDef (velset) applies on the victim's next tick. */
        if (target == IK_STATE_HIT) {
            v->vx_q8 = (int32_t)v->facing * velocity_x;
            v->vy_q8 = velocity_y;
        }

        ikf_queue_hit_effect(
            fight, a, v, hitdef, hitdef->spark_no, 1);
        ikf_queue_sound_event(
            fight, hitdef->hit_sound_group, hitdef->hit_sound_item);
        if (hp_after <= 0) {
            v->pending_damage = v->hp;
            fight->winner = (uint8_t)((victim ^ 1) + 1);
            fight->events |= (uint16_t)(IK_EVENT_HIT | IK_EVENT_KO);
        } else {
            fight->events |= IK_EVENT_HIT;
        }

        if (target == IK_STATE_HIT && hp_after <= 0) {
            v->cur_move_type = IK_CNS_MOVE_HIT;
            v->juggle_owner = (int8_t)ikf_fighter_player_index(fight, a);
            v->ctrl = 0;
            ikf_enter_state_deferred(fight, v, IK_STATE_KO);
            fight->round_over = 1;
            fight->events |= IK_EVENT_ROUND_OVER;
            fight->ko_freeze = IK_KO_FREEZE_FRAMES;
        } else {
            if (custom_p2_state) v->state_owner = a->state_owner;
            v->cur_move_type = IK_CNS_MOVE_HIT;
            v->juggle_owner = (int8_t)ikf_fighter_player_index(fight, a);
            v->ctrl = 0;
            ikf_enter_state_deferred(fight, v, target);
        }
    }

    if (hitdef->p1_state_no >= 0 &&
        ik_cns_find_state(
            cns_for_fighter(fight, a), hitdef->p1_state_no)) {
        ikf_enter_state_deferred(fight, a, hitdef->p1_state_no);
    }

    if ((victim ^ 1) == 0) fight->hits_p1++;
    else fight->hits_p2++;
}
