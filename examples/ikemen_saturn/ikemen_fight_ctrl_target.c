/* Controller family: Controllers that act on the current target (TargetBind etc). */
#include "ikemen_fight_internal.h"

int ikf_ctrl_apply_target(const ik_ctrl_exec_t* x) {
    ik_fight_t* fight = x->fight;
    ik_fighter_t* f = x->fighter;
    const ik_cns_controller_t* ctrl = x->ctrl;
    switch ((ik_cns_controller_type_t)ctrl->type) {
        case IK_CNS_CTRL_TARGET_DROP:
            if (f->target_index >= 0 && f->target_index < 2 &&
                !(ctrl->value0 >= 0 &&
                  f->target_id == ctrl->value0)) {
                ikf_release_bound_target(
                    fight, ikf_fighter_player_index(fight, f));
            }
            break;

        case IK_CNS_CTRL_TARGET_BIND:
            if (f->target_index >= 0 && f->target_index < 2 &&
                (ctrl->value2 < 0 || f->target_id == ctrl->value2)) {
                ik_fighter_t* target =
                    &fight->fighters[(int)f->target_index];
                target->bound_to =
                    (int8_t)(f == &fight->fighters[0] ? 0 : 1);
                target->bind_ticks = 1u;
                target->bound_entity = ik_entity_invalid_handle();
                target->x_q8 =
                    f->x_q8 + (int32_t)f->facing * ctrl->value0;
                target->y_q8 = f->y_q8 + ctrl->value1;
                target->vx_q8 = 0;
                target->vy_q8 = 0;
                sync_position(target);
            }
            break;

        case IK_CNS_CTRL_TARGET_FACING:
            if (f->target_index >= 0 && f->target_index < 2 &&
                (ctrl->value2 < 0 || f->target_id == ctrl->value2)) {
                ik_fighter_t* target =
                    &fight->fighters[(int)f->target_index];
                target->facing = (int8_t)(
                    f->facing * (ctrl->value0 < 0 ? -1 : 1));
            }
            break;

        case IK_CNS_CTRL_TARGET_LIFE_ADD:
            if (f->target_index >= 0 && f->target_index < 2 &&
                (ctrl->value2 < 0 || f->target_id == ctrl->value2)) {
                const int target_index = f->target_index;
                ik_fighter_t* target = &fight->fighters[target_index];
                int hp = (int)target->hp + ctrl->value0;
                const int max_hp = ik_fight_max_hp_player(
                    fight, target->owner_player);
                if (hp > max_hp) hp = max_hp;
                if (hp <= 0) {
                    hp = 0;
                    fight->winner =
                        (uint8_t)((target_index ^ 1) + 1);
                    fight->events |= IK_EVENT_KO;
                }
                target->hp = (int16_t)hp;
            }
            break;

        case IK_CNS_CTRL_TARGET_STATE:
            if (f->target_index >= 0 && f->target_index < 2 &&
                (ctrl->value2 < 0 || f->target_id == ctrl->value2)) {
                const int target_index = f->target_index;
                ik_fighter_t* target = &fight->fighters[target_index];
                target->state_owner = f->state_owner;
                ikf_enter_state_deferred(fight, target, ctrl->value0);
            }
            break;

        default:
            return IKF_CTRL_UNHANDLED;
    }
    return IKF_CTRL_NEXT;
}
