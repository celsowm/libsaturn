/* Controller family: Position, velocity, width and jump-launch controllers. */
#include "ikemen_fight_internal.h"

int ikf_ctrl_apply_motion(const ik_ctrl_exec_t* x) {
    ik_fight_t* fight = x->fight;
    ik_fighter_t* f = x->fighter;
    const ik_cns_controller_t* ctrl = x->ctrl;
    const uint16_t elem = x->anim_elem;
    switch ((ik_cns_controller_type_t)ctrl->type) {
        case IK_CNS_CTRL_POS_ADD:
            f->x_q8 += (int32_t)f->facing * ctrl->value0;
            f->y_q8 += ctrl->value1;
            sync_position(f);
            break;

        case IK_CNS_CTRL_WIDTH: {
            const ik_cns_constants_t* c =
                constants_for_fighter(fight, f);
            /* The base widths follow the state type, as reset_body_size. */
            const int air = f->cur_state_type == IK_CNS_STATE_AIR;
            const int16_t base_front =
                c ? (air ? c->air_front : c->ground_front) : 16;
            const int16_t base_back =
                c ? (air ? c->air_back : c->ground_back) : 15;
            f->push_front = (int16_t)(base_front + ctrl->value0);
            f->push_back = (int16_t)(base_back + ctrl->value1);
            f->edge_front = (int16_t)ctrl->value2;
            f->edge_back = (int16_t)ctrl->value3;
            break;
        }

        case IK_CNS_CTRL_SCREEN_BOUND:
            /* value0: bound flag; value1/value2: movecamera x/y; value3
             * bit 0: `value` given, bit 1: `movecamera` given. */
            if (ctrl->value3 & 1) f->screen_bound = ctrl->value0 != 0;
            if (ctrl->value3 & 2) {
                f->move_camera_x = ctrl->value1 != 0;
                f->move_camera_y = ctrl->value2 != 0;
            }
            break;

        case IK_CNS_CTRL_VEL_SET:
            /* value4/5 carry the authored value in Q16.16 (0: only Q8.8). */
            if ((ctrl->flags & IK_CNS_CTRL_AXIS_X) != 0u) {
                int32_t vx = ikf_q16_or_q8(ctrl->value4, ctrl->value0);
                if ((ctrl->flags & IK_CNS_CTRL_LOCAL_X) != 0u) {
                    vx *= f->facing;
                }
                ikf_fine_set(vx, &f->vx_q8, &f->fine_vx);
            }
            if ((ctrl->flags & IK_CNS_CTRL_AXIS_Y) != 0u) {
                ikf_fine_set(ikf_q16_or_q8(ctrl->value5, ctrl->value1),
                             &f->vy_q8, &f->fine_vy);
            }
            break;

        case IK_CNS_CTRL_VEL_MUL:
            if ((ctrl->flags & IK_CNS_CTRL_AXIS_X) != 0u) {
                f->vx_q8 =
                    (f->vx_q8 * (int32_t)ctrl->value0) / IK_CNS_Q8_ONE;
            }
            if ((ctrl->flags & IK_CNS_CTRL_AXIS_Y) != 0u) {
                f->vy_q8 =
                    (f->vy_q8 * (int32_t)ctrl->value1) / IK_CNS_Q8_ONE;
            }
            break;

        case IK_CNS_CTRL_POS_SET:
            if ((ctrl->flags & IK_CNS_CTRL_AXIS_X) != 0u) {
                f->x_q8 = ctrl->value0;
            }
            if ((ctrl->flags & IK_CNS_CTRL_AXIS_Y) != 0u) {
                f->y_q8 =
                    (int32_t)IK_FLOOR_Y * IK_CNS_Q8_ONE + ctrl->value1;
            }
            sync_position(f);
            break;

        case IK_CNS_CTRL_JUMP_LAUNCH: {
            const ik_cns_constants_t* c =
                constants_for_fighter(fight, f);
            if (!c) break;
            int32_t vx = c->jump_neu_x_q8;
            if (f->state_axis < 0) {
                vx = c->jump_back_q8;
            } else if (f->state_axis > 0) {
                vx = (f->prev_state == 100)
                    ? c->run_jump_fwd_x_q8
                    : c->jump_fwd_q8;
            }
            f->vx_q8 = (int32_t)f->facing * vx;
            f->vy_q8 = c->jump_neu_y_q8;
            break;
        }

        case IK_CNS_CTRL_AIR_JUMP_LAUNCH: {
            const ik_cns_constants_t* c =
                constants_for_fighter(fight, f);
            if (!c) break;
            int32_t vx = c->air_jump_neu_x_q8;
            if (f->state_axis < 0) vx = c->air_jump_back_q8;
            else if (f->state_axis > 0) vx = c->air_jump_fwd_q8;
            f->vx_q8 = (int32_t)f->facing * vx;
            f->vy_q8 = c->air_jump_neu_y_q8;
            break;
        }

        case IK_CNS_CTRL_STATE_TYPE_SET:
            f->cur_state_type = (uint8_t)ctrl->value0;
            if (ctrl->value0 == IK_CNS_STATE_AIR) f->on_ground = 0;
            break;

        case IK_CNS_CTRL_MOVE_TYPE_SET:
            f->cur_move_type = (uint8_t)ctrl->value0;
            break;

        case IK_CNS_CTRL_HIT_VEL_SET:
            if ((ctrl->flags & IK_CNS_CTRL_AXIS_X) != 0u) {
                f->vx_q8 = (int32_t)f->facing * f->gethit_vx_q8;
            }
            if ((ctrl->flags & IK_CNS_CTRL_AXIS_Y) != 0u) {
                f->vy_q8 = f->gethit_vy_q8;
            }
            break;

        case IK_CNS_CTRL_FALL_BOUNCE_VEL:
            if (f->gethit_fall_x_set) {
                f->vx_q8 =
                    (int32_t)f->facing * f->gethit_fall_x_q8;
            }
            f->vy_q8 = f->gethit_fall_y_q8;
            f->on_ground = 0;
            break;

        case IK_CNS_CTRL_POS_ADD_VEL:
            if ((ctrl->flags & IK_CNS_CTRL_AXIS_X) != 0u) {
                f->x_q8 += f->vx_q8;
            }
            if ((ctrl->flags & IK_CNS_CTRL_AXIS_Y) != 0u) {
                f->y_q8 += f->vy_q8;
            }
            sync_position(f);
            break;

        case IK_CNS_CTRL_VEL_ADD:
            if ((ctrl->flags & IK_CNS_CTRL_AXIS_X) != 0u) {
                int32_t vx = ikf_q16_or_q8(ctrl->value4, ctrl->value0);
                if ((ctrl->flags & IK_CNS_CTRL_LOCAL_X) != 0u) {
                    vx *= f->facing;
                }
                ikf_fine_set(
                    ikf_fine_get(f->vx_q8, &f->fine_vx) + vx,
                    &f->vx_q8, &f->fine_vx);
            }
            if ((ctrl->flags & IK_CNS_CTRL_AXIS_Y) != 0u) {
                const ik_cns_constants_t* c =
                    constants_for_fighter(fight, f);
                const int32_t add_y =
                    (ctrl->flags & IK_CNS_CTRL_USE_YACCEL) != 0u && c
                        ? ikf_q16_or_q8(c->yaccel_q16, c->yaccel_q8)
                        : ikf_q16_or_q8(ctrl->value5, ctrl->value1);
                ikf_fine_set(
                    ikf_fine_get(f->vy_q8, &f->fine_vy) + add_y,
                    &f->vy_q8, &f->fine_vy);
            }
            break;

        case IK_CNS_CTRL_VEL_MUL_X_BY_ANIM_ELEM: {
            const int16_t mul =
                elem < (uint16_t)(ctrl->trigger_value < 1
                                      ? 1
                                      : ctrl->trigger_value)
                    ? ctrl->value0
                    : ctrl->value1;
            f->vx_q8 =
                (f->vx_q8 * (int32_t)mul) / IK_CNS_Q8_ONE;
            break;
        }

        case IK_CNS_CTRL_POS_ADD_FROM_BACK_EDGE: {
            const int32_t delta =
                (int32_t)ctrl->value0 -
                x->context->back_edge_body_dist_q8;
            f->x_q8 += (int32_t)f->facing * delta;
            sync_position(f);
            break;
        }

        case IK_CNS_CTRL_POS_FREEZE:
            if ((ctrl->flags & IK_CNS_CTRL_AXIS_X) != 0u) {
                f->pos_freeze_x = 1u;
            }
            if ((ctrl->flags & IK_CNS_CTRL_AXIS_Y) != 0u) {
                f->pos_freeze_y = 1u;
            }
            break;

        default:
            return IKF_CTRL_UNHANDLED;
    }
    return IKF_CTRL_NEXT;
}
