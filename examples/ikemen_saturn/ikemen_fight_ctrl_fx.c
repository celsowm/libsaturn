/* Controller family: Pause, palette/afterimage/shake effects and hit-reaction side effects. */
#include "ikemen_fight_internal.h"

int ikf_ctrl_apply_fx(const ik_ctrl_exec_t* x) {
    ik_fight_t* fight = x->fight;
    ik_fighter_t* f = x->fighter;
    const ik_cns_controller_t* ctrl = x->ctrl;
    switch ((ik_cns_controller_type_t)ctrl->type) {
        case IK_CNS_CTRL_SUPER_PAUSE: {
            if (f->pause_fired || ctrl->value0 <= 0) break;
            f->pause_fired = 1u;
            int power = (int)f->power + ctrl->value1;
            if (power < 0) power = 0;
            if (power > IK_MAX_POWER) power = IK_MAX_POWER;
            f->power = (int16_t)power;
            fight->pause_time = (uint16_t)(
                ctrl->value0 > 65535 ? 65535 : ctrl->value0);
            fight->pause_move_time = (uint16_t)(
                ctrl->value4 < 0 ? 0 :
                ctrl->value4 > 65535 ? 65535 : ctrl->value4);
            fight->pause_end_cmd_buffer_time = (uint16_t)(
                ctrl->value5 < 0 ? 0 :
                ctrl->value5 > 65535 ? 65535 : ctrl->value5);
            fight->pause_is_super = 1u;
            fight->pause_owner =
                (int8_t)ikf_fighter_player_index(fight, f);
            if ((ctrl->flags & IK_CNS_CTRL_SUPER_DARKEN) != 0u) {
                fight->super_darken_time = fight->pause_time;
            }
            if ((ctrl->flags & IK_CNS_CTRL_HAS_SUPER_ANIM) != 0u &&
                fight->effect_count < IK_MAX_EFFECT_EVENTS) {
                const int16_t px = (int16_t)(
                    ((uint32_t)ctrl->value3 >> 16) & 0xffffu);
                const int16_t py = (int16_t)(
                    (uint32_t)ctrl->value3 & 0xffffu);
                ik_effect_event_t* effect =
                    &fight->effect_events[fight->effect_count++];
                effect->action = (int16_t)ctrl->value2;
                effect->x = (int16_t)(
                    f->x + (int16_t)f->facing * px);
                effect->y = (int16_t)(f->y + py);
            }
            /* Like upstream the rest of this tick keeps running; the pause
             * takes effect from the next tick. */
            break;
        }

        case IK_CNS_CTRL_PAUSE: {
            if (f->pause_fired || ctrl->value0 <= 0) break;
            f->pause_fired = 1u;
            fight->pause_time = (uint16_t)(
                ctrl->value0 > 65535 ? 65535 : ctrl->value0);
            fight->pause_move_time = (uint16_t)(
                ctrl->value1 < 0 ? 0 :
                ctrl->value1 > 65535 ? 65535 : ctrl->value1);
            fight->pause_end_cmd_buffer_time = (uint16_t)(
                ctrl->value2 < 0 ? 0 :
                ctrl->value2 > 65535 ? 65535 : ctrl->value2);
            fight->pause_is_super = 0u;
            fight->pause_owner =
                (int8_t)ikf_fighter_player_index(fight, f);
            break;
        }

        case IK_CNS_CTRL_ASSERT_INTRO:
            fight->intro_asserted = 1u;
            break;

        case IK_CNS_CTRL_PAL_FX: {
            const uint32_t add = (uint32_t)ctrl->value1;
            const uint32_t sinadd = (uint32_t)ctrl->value2;
            f->palfx_time = (uint16_t)(
                ctrl->value0 <= 0 ? 1 :
                ctrl->value0 > 65535 ? 65535 : ctrl->value0);
            f->palfx_add_r =
                (int16_t)((int32_t)(add << 23) >> 23);
            f->palfx_add_g =
                (int16_t)((int32_t)(add << 14) >> 23);
            f->palfx_add_b =
                (int16_t)((int32_t)(add << 5) >> 23);
            f->palfx_sin_r =
                (int16_t)((int32_t)(sinadd << 23) >> 23);
            f->palfx_sin_g =
                (int16_t)((int32_t)(sinadd << 14) >> 23);
            f->palfx_sin_b =
                (int16_t)((int32_t)(sinadd << 5) >> 23);
            f->palfx_cycle = (uint16_t)(
                ctrl->value3 <= 0 ? 1 :
                ctrl->value3 > 65535 ? 65535 : ctrl->value3);
            f->palfx_phase = 0u;
            {
                const uint32_t mul = (uint32_t)ctrl->value4;
                const uint32_t sinmul = (uint32_t)ctrl->value5;
                f->palfx_mul_r = (uint16_t)(mul & 0x1ffu);
                f->palfx_mul_g = (uint16_t)((mul >> 9) & 0x1ffu);
                f->palfx_mul_b = (uint16_t)((mul >> 18) & 0x1ffu);
                f->palfx_sinmul_r =
                    (int16_t)((int32_t)(sinmul << 23) >> 23);
                f->palfx_sinmul_g =
                    (int16_t)((int32_t)(sinmul << 14) >> 23);
                f->palfx_sinmul_b =
                    (int16_t)((int32_t)(sinmul << 5) >> 23);
            }
            f->palfx_sinmul_cycle = (uint16_t)(
                ctrl->value6 <= 0 ? 1 :
                ctrl->value6 > 65535 ? 65535 : ctrl->value6);
            f->palfx_sinmul_phase = 0u;
            break;
        }

        case IK_CNS_CTRL_AFTER_IMAGE:
            f->afterimage_time = (uint16_t)(
                ctrl->value0 <= 0 ? 1 :
                ctrl->value0 > 65535 ? 65535 : ctrl->value0);
            f->afterimage_length = (uint8_t)(
                ctrl->value1 <= 0 ? 1 :
                ctrl->value1 > 32 ? 32 : ctrl->value1);
            f->afterimage_timegap = (uint8_t)(
                ctrl->value2 <= 0 ? 1 :
                ctrl->value2 > 255 ? 255 : ctrl->value2);
            f->afterimage_framegap = (uint8_t)(
                ctrl->value3 <= 0 ? 1 :
                ctrl->value3 > 255 ? 255 : ctrl->value3);
            f->afterimage_bright_rgb = (uint32_t)ctrl->value4;
            f->afterimage_contrast_rgb = (uint32_t)ctrl->value5;
            f->afterimage_add_rgb = (uint32_t)ctrl->value6;
            f->afterimage_mul_rgb = (uint32_t)ctrl->value7;
            break;

        case IK_CNS_CTRL_AFTER_IMAGE_TIME:
            f->afterimage_time = (uint16_t)(
                ctrl->value0 <= 0 ? 1 :
                ctrl->value0 > 65535 ? 65535 : ctrl->value0);
            break;

        case IK_CNS_CTRL_MAKE_DUST:
            if (fight->effect_count < IK_MAX_EFFECT_EVENTS) {
                ik_effect_event_t* effect =
                    &fight->effect_events[fight->effect_count++];
                effect->action =
                    ctrl->value0 != 0 ? (int16_t)ctrl->value0 : 120;
                effect->x = f->x;
                effect->y = f->y;
            }
            break;

        case IK_CNS_CTRL_ENV_SHAKE:
            fight->env_shake_time = (uint16_t)(
                ctrl->value0 < 0 ? 0 :
                ctrl->value0 > 65535 ? 65535 : ctrl->value0);
            fight->env_shake_ampl = (int16_t)ctrl->value1;
            fight->env_shake_freq = 60u;
            fight->env_shake_phase = 0u;
            break;

        case IK_CNS_CTRL_HIT_FALL_DAMAGE:
            if (f->gethit_fall_damage > 0) {
                f->hp = (int16_t)(
                    f->hp > f->gethit_fall_damage
                        ? f->hp - f->gethit_fall_damage
                        : 0);
                f->gethit_fall_damage = 0;
                if (f->hp == 0) {
                    fight->winner = (uint8_t)(
                        ikf_fighter_player_index(fight, f) ^ 1u);
                    ++fight->winner;
                    fight->events |=
                        (uint16_t)(IK_EVENT_HIT | IK_EVENT_KO);
                }
            }
            break;

        case IK_CNS_CTRL_FALL_ENV_SHAKE:
            if (f->gethit_fall_envshake_time > 0u) {
                fight->env_shake_time =
                    f->gethit_fall_envshake_time;
                fight->env_shake_ampl =
                    f->gethit_fall_envshake_ampl;
                fight->env_shake_freq =
                    f->gethit_fall_envshake_freq;
                fight->env_shake_phase = 0u;
                f->gethit_fall_envshake_time = 0u;
            }
            break;

        case IK_CNS_CTRL_NOT_HIT_BY:
            f->not_hit_by_mask = (uint8_t)(ctrl->value0 & 0xff);
            f->not_hit_by_attr_mask =
                (uint16_t)(((uint32_t)ctrl->value0 >> 8) & 0xffffu);
            f->not_hit_by_time = (uint16_t)(
                ctrl->value1 <= 0 ? 1 :
                ctrl->value1 > 65535 ? 65535 : ctrl->value1);
            break;

        default:
            return IKF_CTRL_UNHANDLED;
    }
    return IKF_CTRL_NEXT;
}
