/* Controller family: ChangeAnim family and hit/guard animation selection. */
#include "ikemen_fight_internal.h"

int ikf_ctrl_apply_anim(const ik_ctrl_exec_t* x) {
    ik_fight_t* fight = x->fight;
    ik_fighter_t* f = x->fighter;
    const ik_cns_controller_t* ctrl = x->ctrl;
    const ik_frame_table_t* frames = x->frames;
    const int anim_ended = x->anim_ended;
    switch ((ik_cns_controller_type_t)ctrl->type) {
        case IK_CNS_CTRL_CHANGE_ANIM:
            f->anim_owner = f->owner_player;
            f->anim = ctrl->value0;
            f->anim_time = ikf_anim_element_start_tick(
                frames_for_fighter(fight, f), f->anim,
                (uint16_t)(ctrl->value1 < 1 ? 1 : ctrl->value1));
            return IKF_CTRL_END_PASS;

        /* ChangeAnim that lets the controllers after it run. */
        case IK_CNS_CTRL_RESTART_ANIM:
            f->anim_owner = f->owner_player;
            f->anim = ctrl->value0;
            f->anim_time = 0u;
            break;

        /* Common state 0: keep the turn anim (5) until it ends, else idle. */
        case IK_CNS_CTRL_IDLE_ANIM:
            if ((f->anim == 5 && anim_ended) ||
                (f->anim != 5 && f->anim != 0)) {
                f->anim_owner = f->owner_player;
                f->anim = 0;
                f->anim_time = 0u;
            }
            break;

        /* Air get-hit anims (5030 / 5035): keep a 5000-5199 anim the shake
         * state picked (5051 for Up hits), otherwise use the flight pose. */
        case IK_CNS_CTRL_AIR_HIT_ANIM: {
            const int16_t pose = ctrl->value0;
            uint32_t first = 0u;
            uint32_t count = 0u;
            const int keep = ctrl->value1 == 0
                ? (f->anim >= 5000 && f->anim <= 5199)
                : (f->state_time != 0u ||
                   (f->anim >= 5051 && f->anim <= 5059) || f->anim == 5090);
            if (!keep && frames &&
                ik_frames_bounds(frames, pose, &first, &count) &&
                count > 0u) {
                f->anim_owner = f->owner_player;
                f->anim = pose;
                f->anim_time = 0u;
            }
            break;
        }

        /* Common state 5050 (falling): start the fall pose unless a launch
         * anim (5051-5059 / 5061-5069 / 5090) is playing, then switch a
         * 505x pose to its +10 variant once it is moving down. */
        case IK_CNS_CTRL_LAND_HIT_ANIM:
            /* State 5040: ChangeAnim 5040 when the current animation ends,
             * or on entry unless the 5035 pose is still playing. */
            if ((anim_ended && f->anim != ctrl->value0) ||
                (f->state_time == 0u && f->anim != 5035)) {
                f->anim_owner = f->owner_player;
                f->anim = ctrl->value0;
                f->anim_time = 0u;
            }
            break;

        case IK_CNS_CTRL_FALL_ANIM: {
            uint32_t first = 0u;
            uint32_t count = 0u;
            const int launch_anim =
                (f->anim >= 5051 && f->anim <= 5059) ||
                (f->anim >= 5061 && f->anim <= 5069) || f->anim == 5090;
            if ((f->anim == 5035 && anim_ended) ||
                (f->state_time == 0u && f->anim != 5035 && !launch_anim)) {
                f->anim_owner = f->owner_player;
                f->anim = 5050;
                f->anim_time = 0u;
            }
            if (f->anim >= 5050 && f->anim <= 5059 &&
                f->vy_q8 >= (f->anim == 5050 ? IK_CNS_Q8_ONE
                                               : -2 * IK_CNS_Q8_ONE) &&
                frames &&
                ik_frames_bounds(frames, (int16_t)(f->anim + 10),
                                 &first, &count) && count > 0u) {
                f->anim = (int16_t)(f->anim + 10);
                f->anim_time = 0u;
            }
            break;
        }

        /* IkSys_SelfAnimExistAddMod10: base + anim % 10 when the current anim
         * is a variant (value1 0: 5051-59 / 5061-69, 1: 5101-09) and exists,
         * else base. */
        case IK_CNS_CTRL_ANIM_ADD_MOD10: {
            uint32_t first = 0u;
            uint32_t count = 0u;
            const int variant = ctrl->value1 == 0
                ? ((f->anim >= 5051 && f->anim <= 5059) ||
                   (f->anim >= 5061 && f->anim <= 5069))
                : (f->anim >= 5101 && f->anim <= 5109);
            int16_t action = ctrl->value0;
            if (variant) {
                const int16_t alt = (int16_t)(ctrl->value0 + f->anim % 10);
                if (frames && ik_frames_bounds(frames, alt, &first, &count) &&
                    count > 0u) {
                    action = alt;
                }
            }
            f->anim_owner = f->owner_player;
            f->anim = action;
            f->anim_time = 0u;
            break;
        }

        case IK_CNS_CTRL_CHANGE_ANIM_BY_VX: {
            const int32_t local_vx = f->vx_q8 * f->facing;
            int16_t action = ctrl->value0;
            if (local_vx > 0) action = ctrl->value1;
            else if (local_vx < 0) action = (int16_t)(ctrl->value1 + 1);
            if (action >= 0 && action != f->anim) {
                f->anim = action;
                f->anim_time = 0u;
            }
            break;
        }

        case IK_CNS_CTRL_CHANGE_ANIM_IF_END_FROM:
            if (f->anim == ctrl->value0 && anim_ended) {
                f->anim = ctrl->value1;
                f->anim_time = 0u;
            }
            break;

        case IK_CNS_CTRL_CHANGE_ANIM_IF_EXISTS: {
            uint32_t first = 0u;
            uint32_t count = 0u;
            int16_t action = ctrl->value1;
            const ik_frame_table_t* native_frames =
                frames_for_owner(fight, f->owner_player);
            if (native_frames && ik_frames_bounds(
                    native_frames, ctrl->value0,
                    &first, &count) && count > 0u) {
                action = ctrl->value0;
            }
            if (f->anim != action ||
                f->anim_owner != f->owner_player) {
                f->anim_owner = f->owner_player;
                f->anim = action;
                f->anim_time = 0u;
            }
            break;
        }

        case IK_CNS_CTRL_CHANGE_ANIM_DESCENT_IF_EXISTS: {
            const int16_t first_action = ctrl->value1;
            if (f->vy_q8 > ctrl->value0 &&
                f->anim >= first_action &&
                f->anim <= (int16_t)(first_action + 2)) {
                uint32_t first = 0u;
                uint32_t count = 0u;
                const int16_t action = (int16_t)(f->anim + 3);
                const ik_frame_table_t* native_frames =
                    frames_for_owner(fight, f->owner_player);
                if (native_frames && ik_frames_bounds(
                        native_frames, action,
                        &first, &count) && count > 0u) {
                    f->anim_owner = f->owner_player;
                    f->anim = action;
                    f->anim_time = 0u;
                }
            }
            break;
        }

        case IK_CNS_CTRL_GUARD_ANIM_BY_TYPE: {
            int16_t action = ctrl->value0;
            if (f->guard_type == IK_CNS_STATE_CROUCH) action++;
            else if (f->guard_type == IK_CNS_STATE_AIR) action += 2;
            if (f->anim != action) {
                f->anim = action;
                f->anim_time = 0u;
            }
            break;
        }

        case IK_CNS_CTRL_GET_HIT_ANIM: {
            int16_t base;
            if (ctrl->value0 == 3) {
                /* Knocked back (stand): 5005 for high hits, 5015 for low. */
                base = f->gethit_ground_type == IK_CNS_GROUND_LOW
                    ? 5015
                    : 5005;
            } else if (ctrl->value0 == 4) {
                base = 5025;
            } else if (ctrl->value0 == 1) {
                base = 5020;
            } else {
                base = f->gethit_ground_type == IK_CNS_GROUND_HIGH
                    ? 5000
                    : 5010;
            }
            int16_t action =
                (int16_t)(base + (int16_t)f->gethit_anim_type);
            uint32_t first = 0u;
            uint32_t count = 0u;
            if (ctrl->value0 <= 2) {
                /* Shaking: Up/DiagUp prefer 5051/5052, Back/Up/DiagUp fall
                 * back to the 5030 flight pose. */
                const uint8_t type = f->gethit_anim_type;
                const int16_t alt = (int16_t)(5051 + type - 4);
                if (type >= 4u && frames &&
                    ik_frames_bounds(frames, alt, &first, &count) &&
                    count > 0u) {
                    action = alt;
                } else if (type >= 3u) {
                    action = 5030;
                }
            }
            if (!frames || !ik_frames_bounds(
                    frames, action, &first, &count) || count == 0u) {
                action = base;
            }
            /* After the first tick upstream re-issues ChangeAnim with the
             * current anim every tick, so the shaking pose never advances. */
            f->anim = action;
            f->anim_time = 0u;
            break;
        }

        case IK_CNS_CTRL_CHANGE_ANIM2:
            f->anim_owner = f->state_owner;
            f->anim = ctrl->value0;
            f->anim_time = ikf_anim_element_start_tick(
                frames_for_fighter(fight, f), f->anim,
                (uint16_t)(ctrl->value1 < 1 ? 1 : ctrl->value1));
            break;

        default:
            return IKF_CTRL_UNHANDLED;
    }
    return IKF_CTRL_NEXT;
}
