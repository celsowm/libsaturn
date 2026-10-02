/* Controller family: ChangeState family, variables, guard and hit-recovery routing. */
#include "ikemen_fight_internal.h"

int ikf_ctrl_apply_state(const ik_ctrl_exec_t* x) {
    ik_fight_t* fight = x->fight;
    ik_fighter_t* f = x->fighter;
    const ik_cns_controller_t* ctrl = x->ctrl;
    const ik_cns_asset_t* native_cns = x->native_cns;
    const uint16_t command_mask = x->context->command_mask;
    switch ((ik_cns_controller_type_t)ctrl->type) {
        case IK_CNS_CTRL_CHANGE_STATE: {
            const int has_ctrl = (ctrl->flags & IK_CNS_CTRL_HAS_CTRL) != 0u;
            const int16_t target = ctrl->value0;
            ikf_turn_before_change(fight, f);
            ikf_enter_state(fight, f, target);
            if (has_ctrl) f->ctrl = (int8_t)(ctrl->value1 != 0);
            return IKF_CTRL_END_TICK;
        }

        case IK_CNS_CTRL_CTRL_SET:
            f->ctrl = (int8_t)(ctrl->value0 != 0);
            break;

        case IK_CNS_CTRL_VAR_SET:

        case IK_CNS_CTRL_VAR_ADD: {
            ik_entity_t* entity = ikf_fighter_entity(fight, f);
            const int32_t index = ctrl->value0;
            if (entity && index >= 0 &&
                index < (int32_t)IK_ENTITY_VAR_COUNT) {
                if (ctrl->type == IK_CNS_CTRL_VAR_SET) {
                    entity->vars[index] = ctrl->value1;
                } else {
                    entity->vars[index] += ctrl->value1;
                }
            }
            break;
        }

        case IK_CNS_CTRL_SPR_PRIORITY:
            f->spr_priority = (int8_t)ctrl->value0;
            break;

        case IK_CNS_CTRL_CAPTURE_COMMAND_AXIS:
            if ((command_mask & IK_CNS_COMMAND_HOLD_BACK) != 0u) {
                f->state_axis = -1;
            } else if ((command_mask & IK_CNS_COMMAND_HOLD_FWD) != 0u) {
                f->state_axis = 1;
            }
            break;

        case IK_CNS_CTRL_GUARD_STATE_BY_TYPE: {
            int16_t target = ctrl->value0;
            if (f->guard_type == IK_CNS_STATE_CROUCH) target++;
            else if (f->guard_type == IK_CNS_STATE_AIR) target += 2;
            ikf_enter_state(fight, f, target);
            return IKF_CTRL_END_TICK;
        }

        case IK_CNS_CTRL_GUARD_END: {
            int16_t target = 0;
            if (f->guard_type == IK_CNS_STATE_CROUCH) target = 11;
            else if (f->guard_type == IK_CNS_STATE_AIR) {
                /* Upstream's hardcoded exit of state 140: crouch 11, air 51
                 * (the empty jump-down state). */
                target = ik_cns_find_state(
                    cns_for_owner(fight, f->owner_player), 51) ? 51 : 50;
            }
            ikf_enter_state(fight, f, target);
            f->ctrl = 1;
            return IKF_CTRL_END_TICK;
        }

        case IK_CNS_CTRL_HIT_RECOVER_STATE: {
            const int16_t target = f->gethit_fall ? 5050 : 5040;
            ikf_enter_state(fight, f, target);
            if (!f->gethit_fall) f->ctrl = 1;
            return IKF_CTRL_END_TICK;
        }

        case IK_CNS_CTRL_FALL_GROUND_BRANCH:
            if (f->gethit_fall_y_q8 == 0) {
                ikf_enter_state(fight, f, ctrl->value0);
                return IKF_CTRL_END_TICK;
            }
            break;

        case IK_CNS_CTRL_DOWNED_HIT_BRANCH:
            if (f->gethit_vy_q8 != 0 &&
                ik_cns_find_state(native_cns, 5030)) {
                f->anim = 5090;
                f->anim_time = 0u;
                ikf_enter_state(fight, f, 5030);
            } else if (ik_cns_find_state(native_cns, 5081)) {
                f->anim = 5080;
                f->anim_time = 0u;
                ikf_enter_state(fight, f, 5081);
            }
            return IKF_CTRL_END_TICK;

        case IK_CNS_CTRL_TURN:
            f->facing = (int8_t)-f->facing;
            break;

        case IK_CNS_CTRL_SELF_STATE:
            if (f->bound_to >= 0 && f->bound_to < 2) {
                ik_fighter_t* owner =
                    &fight->fighters[(int)f->bound_to];
                owner->target_index = -1;
                owner->target_id = -1;
            }
            ikf_release_entity_bound_fighter(fight, f);
            f->bound_to = -1;
            f->state_owner = f->owner_player;
            ikf_enter_state(fight, f, ctrl->value0);
            return IKF_CTRL_END_TICK;

        case IK_CNS_CTRL_FALL_RECOVERY: {
            const ik_cns_constants_t* c =
                constants_for_fighter(fight, f);
            if (!c ||
                (command_mask & IK_CNS_COMMAND_RECOVERY) == 0u ||
                !f->gethit_fall_recover ||
                f->fall_time < f->gethit_fall_recover_time) {
                break;
            }

            const int32_t rel_y_q8 =
                f->y_q8 -
                (int32_t)IK_FLOOR_Y * IK_CNS_Q8_ONE;
            if (f->vy_q8 > 0 &&
                rel_y_q8 >=
                    c->air_gethit_groundrecover_threshold_q8 &&
                ik_cns_find_state(native_cns, 5200)) {
                ikf_enter_state(fight, f, 5200);
                return IKF_CTRL_END_TICK;
            }
            if (f->vy_q8 >
                    c->air_gethit_airrecover_threshold_q8 &&
                ik_cns_find_state(native_cns, 5210)) {
                ikf_enter_state(fight, f, 5210);
                return IKF_CTRL_END_TICK;
            }
            break;
        }

        default:
            return IKF_CTRL_UNHANDLED;
    }
    return IKF_CTRL_NEXT;
}
