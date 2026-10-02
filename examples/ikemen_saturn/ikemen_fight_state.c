/* State transitions: ChangeState bookkeeping and controlled input. */
#include "ikemen_fight_internal.h"

uint8_t ik_fight_state_type(const ik_fight_t* fight,
                            const ik_fighter_t* fighter) {
    (void)fight;
    return fighter ? fighter->cur_state_type : IK_CNS_STATE_UNCHANGED;
}

int ik_action_for_state(const ik_cns_asset_t* cns, int16_t state) {
    const ik_cns_state_t* spec = ik_cns_find_state(cns, state);
    if (spec) return spec->anim;

    switch (state) {
        case IK_STATE_WALK: return 20;
        case IK_STATE_CROUCH: return 11;
        case IK_STATE_JUMP: return 41;
        case IK_STATE_HIT: return 105;
        case IK_STATE_KO: return 120;
        case IK_STATE_GUARD: return 130;
        default: return 0;
    }
}

static int default_ctrl_for_state(int16_t state) {
    return state == IK_STATE_IDLE || state == IK_STATE_WALK ||
           state == IK_STATE_CROUCH || state == IK_STATE_JUMP;
}

/* Upstream stateChange1: only the state number and its clocks change. */
static void change_state_number(ik_fighter_t* f, int16_t state) {
    f->prev_state = f->state;
    f->state = state;
    f->state_time = 0u;
    ++f->state_entries;
    f->pause_fired = 0u;
    f->one_shot_controller_mask = 0u;
}

/* Fallback state type for characters without a compiled StateDef. */
static uint8_t legacy_state_type(const ik_fighter_t* f) {
    if (!f->on_ground) return IK_CNS_STATE_AIR;
    if (f->state == IK_STATE_CROUCH) return IK_CNS_STATE_CROUCH;
    return IK_CNS_STATE_STAND;
}

/* Upstream stateChange2: the StateDef parameters (anim, ctrl, velset, type,
 * movetype...) take effect. */
void ikf_init_statedef(ik_fight_t* fight, ik_fighter_t* f) {
    const ik_cns_asset_t* state_cns = cns_for_fighter(fight, f);
    const ik_cns_state_t* spec = ik_cns_find_state(state_cns, f->state);

    f->statedef_pending = 0u;
    if (!(spec && spec->anim < 0)) {
        f->anim = (int16_t)ik_action_for_state(state_cns, f->state);
        f->anim_owner = f->owner_player;
        f->anim_time = 0u;
    }
    f->move_contact = 0u;
    f->move_hit = 0u;
    f->move_contact_time = 0u;
    if (!(spec && spec->hitdef_persist)) {
        f->hitdef_hit_mask = 0u;
        f->active_hitdef_local = -1;
        f->active_hitdef_global = -1;
        f->active_hitdef_secondary = 0u;
        f->hitdef_target = -1;
    }
    f->state_axis = 0;

    if (spec && spec->move_type == IK_CNS_MOVE_ATTACK) ++f->attack_id;

    if (!spec) {
        f->ctrl = (int8_t)default_ctrl_for_state(f->state);
        f->spr_priority = 0;
        f->cur_state_type = legacy_state_type(f);
        f->cur_move_type = IK_CNS_MOVE_IDLE;
        return;
    }

    int power = (int)f->power + spec->power_add;
    if (power < 0) power = 0;
    if (power > IK_MAX_POWER) power = IK_MAX_POWER;
    f->power = (int16_t)power;
    if (spec->ctrl >= 0) f->ctrl = spec->ctrl;
    f->spr_priority = spec->spr_priority;
    if (spec->state_type != IK_CNS_STATE_UNCHANGED) {
        f->cur_state_type = (uint8_t)spec->state_type;
    }
    if (spec->move_type != IK_CNS_MOVE_UNCHANGED) {
        f->cur_move_type = (uint8_t)spec->move_type;
    }
    if (f->cur_move_type != IK_CNS_MOVE_HIT && !f->reversed) {
        ikf_exit_target(fight, f);
    }
    if (spec->state_type == IK_CNS_STATE_AIR) {
        f->on_ground = 0;
    } else if (spec->state_type == IK_CNS_STATE_STAND ||
               spec->state_type == IK_CNS_STATE_CROUCH ||
               spec->state_type == IK_CNS_STATE_LIEDOWN) {
        f->on_ground = 1;
    }
    if (spec->has_velset) {
        /* StateDef velset is local: forward is where the fighter faces. */
        f->vx_q8 = spec->velset_x_q8 * f->facing;
        f->vy_q8 = spec->velset_y_q8;
    }
}

void ikf_auto_turn(ik_fight_t* fight, ik_fighter_t* f) {
    const ik_cns_state_t* spec = fighter_state_spec(fight, f);
    if (spec && (spec->assert_special_flags &
                 IK_CNS_STATE_ASSERT_NO_AUTO_TURN) != 0u) {
        return;
    }
    const ik_fighter_t* foe = f == &fight->fighters[0]
        ? &fight->fighters[1] : &fight->fighters[0];
    /* Upstream: the foe is behind (distance along facing < 0). */
    if ((int64_t)(foe->x_q8 - f->x_q8) * f->facing >= 0) return;
    if (f->cur_state_type == IK_CNS_STATE_STAND && f->anim != 5) {
        f->anim = 5;
        f->anim_time = 0u;
    } else if (f->cur_state_type == IK_CNS_STATE_CROUCH && f->anim != 6) {
        f->anim = 6;
        f->anim_time = 0u;
    }
    f->facing = (int8_t)-f->facing;
}

void ikf_turn_before_change(ik_fight_t* fight, ik_fighter_t* f) {
    /* Mugen-compat quirk upstream keeps for non-Ikemen chars: a controller
     * ChangeState with ctrl during the fight turns the fighter first. */
    if (f->ctrl && fight->round_state <= 2u &&
        (f->cur_state_type == IK_CNS_STATE_STAND ||
         f->cur_state_type == IK_CNS_STATE_CROUCH)) {
        ikf_auto_turn(fight, f);
    }
}

/* ChangeState from the fighter's own controllers: the new state's StateDef
 * applies at once and its controllers run in the same tick. */
void ikf_enter_state(ik_fight_t* fight, ik_fighter_t* f, int16_t state) {
    /* The old state's VelAdd already ran this tick; that accumulates across
     * a chain of ChangeStates. */
    const int32_t ran = ikf_pre_move_gravity(fight, f);
    if (ran != 0) {
        f->gravity_carry = 1u;
        f->gravity_carry_q16 += ran;
    }
    change_state_number(f, state);
    ikf_init_statedef(fight, f);
}

/* ChangeState from outside the fighter's own tick (hit detection, landing):
 * only the number changes now; the StateDef applies when the fighter next
 * acts, as in upstream. */
void ikf_enter_state_deferred(
    ik_fight_t* fight, ik_fighter_t* f, int16_t state
) {
    (void)fight;
    change_state_number(f, state);
    f->statedef_pending = 1u;
}

int ikf_dispatch_controlled_input(ik_fight_t* fight, ik_fighter_t* f,
                                     const ik_fight_controls_t* controls) {
    if (!controls) return 0;
    const ik_cns_asset_t* native_cns =
        cns_for_owner(fight, f->owner_player);

    /* State -1 gates execute before ctrl: legal cancels can intentionally
     * ChangeState while the current attack still has ctrl = 0. */
    if (controls->has_state_request) {
        ikf_turn_before_change(fight, f);
        ikf_enter_state(fight, f, controls->requested_state);
        return 1;
    }

    if (!f->on_ground) {
        const ik_cns_constants_t* c =
            constants_for_fighter(fight, f);
        if (f->ctrl && controls->up && !f->up_latched && c &&
            c->air_jump_num > 0 &&
            f->air_jumps_used < (uint8_t)c->air_jump_num &&
            ((int32_t)IK_FLOOR_Y * IK_CNS_Q8_ONE - f->y_q8) >=
                (int32_t)c->air_jump_height * IK_CNS_Q8_ONE &&
            ik_cns_find_state(native_cns, 45)) {
            ikf_enter_state(fight, f, 45);
            f->air_jumps_used++;
            f->up_latched = 1u;
            return 1;
        }
        return 0;
    }

    if (!f->ctrl) return 0;
    if (controls->down) {
        const int16_t crouch_state =
            ik_cns_find_state(native_cns, 10)
                ? 10
                : IK_STATE_CROUCH;
        if (f->cur_state_type == IK_CNS_STATE_STAND &&
            f->state != IK_STATE_CROUCH && f->state != crouch_state) {
            /* Upstream's hardcoded stand-to-crouch stops the fighter dead,
             * except when braking out of a run (state 100). */
            if (f->state != 100) f->vx_q8 = 0;
            ikf_enter_state(fight, f, crouch_state);
            return 1;
        }
        return 0;
    }
    /* Upstream stands up from any crouching state that has ctrl. */
    if (!controls->down && f->cur_state_type == IK_CNS_STATE_CROUCH &&
        f->state != 12 && ik_cns_find_state(native_cns, 12)) {
        ikf_enter_state(fight, f, 12);
        return 1;
    }
    if (controls->up &&
        ik_cns_find_state(native_cns, IK_STATE_JUMP)) {
        ikf_enter_state(fight, f, IK_STATE_JUMP);
        f->up_latched = 1u;
        return 1;
    }
    return 0;
}
