#include "ikemen_cns.h"

int16_t ik_cns_q8_from_int(int16_t value) {
    return (int16_t)(value * IK_CNS_Q8_ONE);
}

int16_t ik_cns_q8_to_int(int32_t value) {
    if (value >= 0) return (int16_t)(value / IK_CNS_Q8_ONE);
    return (int16_t)-((-value) / IK_CNS_Q8_ONE);
}

const ik_cns_state_t* ik_cns_find_state(const ik_cns_asset_t* asset,
                                        int16_t state_number) {
    if (!asset || !asset->states) return 0;
    for (uint16_t i = 0u; i < asset->state_count; ++i) {
        if (asset->states[i].number == state_number) return &asset->states[i];
    }
    return 0;
}

int ik_cns_trigger_now(uint8_t trigger_kind, int16_t trigger_value,
                       uint16_t state_time, uint16_t anim_element,
                       uint16_t anim_element_time, int anim_ended) {
    switch ((ik_cns_trigger_kind_t)trigger_kind) {
        case IK_CNS_TRIGGER_ALWAYS:
            return 1;
        case IK_CNS_TRIGGER_TIME_EQ:
            return state_time ==
                   (uint16_t)(trigger_value < 0 ? 0 : trigger_value);
        case IK_CNS_TRIGGER_ANIM_ELEM_EQ:
            return anim_element ==
                       (uint16_t)(trigger_value < 1 ? 1 : trigger_value) &&
                   anim_element_time == 0u;
        case IK_CNS_TRIGGER_ANIM_END:
            return anim_ended != 0;
        default:
            return 0;
    }
}

int ik_cns_controller_trigger_context_now(
    const ik_cns_controller_t* controller,
    const ik_cns_controller_context_t* context) {
    if (!controller || !context) return 0;

    switch ((ik_cns_trigger_kind_t)controller->trigger_kind) {
        case IK_CNS_TRIGGER_ANIM_ELEM_RANGE: {
            const uint16_t first =
                (uint16_t)(controller->trigger_value < 1
                               ? 1
                               : controller->trigger_value);
            const uint16_t last =
                (uint16_t)(controller->trigger_value2 <=
                                   controller->trigger_value
                               ? controller->trigger_value + 1
                               : controller->trigger_value2);
            return context->anim_element >= first &&
                   context->anim_element < last;
        }

        case IK_CNS_TRIGGER_MOVE_CONTACT_ELEM_WINDOW: {
            if (!context->move_contact) return 0;
            const uint16_t first =
                (uint16_t)(controller->trigger_value < 1
                               ? 1
                               : controller->trigger_value);
            const uint16_t last =
                (uint16_t)(controller->trigger_value2 <=
                                   controller->trigger_value
                               ? controller->trigger_value + 1
                               : controller->trigger_value2);

            if (context->anim_element == first) {
                return context->anim_element_time > 0u;
            }
            if (context->anim_element > first &&
                context->anim_element < last) {
                return 1;
            }
            return context->anim_element == last &&
                   context->anim_element_time == 0u;
        }

        case IK_CNS_TRIGGER_COMMAND_ACTIVE:
            return (context->command_mask &
                    (uint16_t)controller->trigger_value) != 0u;

        case IK_CNS_TRIGGER_COMMAND_INACTIVE:
            return (context->command_mask &
                    (uint16_t)controller->trigger_value) == 0u;

        case IK_CNS_TRIGGER_ABS_VX_LT_Q8: {
            const int32_t limit = controller->trigger_value < 0
                ? -(int32_t)controller->trigger_value
                : (int32_t)controller->trigger_value;
            return context->vx_q8 < limit && context->vx_q8 > -limit;
        }

        case IK_CNS_TRIGGER_VY_GT_Q8_AT_FLOOR:
            return context->vy_q8 > controller->trigger_value &&
                   context->y_q8 >= context->floor_y_q8;

        case IK_CNS_TRIGGER_ANIM_EQ_AND_END:
            return context->anim == controller->trigger_value &&
                   context->anim_ended != 0;

        case IK_CNS_TRIGGER_HIT_SLIDE_TIME:
            return context->state_time == context->hit_slide_time;

        case IK_CNS_TRIGGER_HIT_SLIDE_GE:
            return context->state_time >= context->hit_slide_time;

        case IK_CNS_TRIGGER_HIT_CTRL_TIME:
            return context->state_time == context->hit_ctrl_time;

        case IK_CNS_TRIGGER_HIT_OVER:
            return context->hitstun == 0u;

        case IK_CNS_TRIGGER_HIT_LAUNCH:
            return context->hit_launch != 0u;

        case IK_CNS_TRIGGER_HIT_NO_LAUNCH:
            return context->hit_launch == 0u;

        case IK_CNS_TRIGGER_NOT_ALIVE:
            return context->alive == 0u;

        case IK_CNS_TRIGGER_ANIM_ELEM_BEFORE:
            return context->anim_element <
                   (uint16_t)(controller->trigger_value < 1
                                  ? 1
                                  : controller->trigger_value);

        case IK_CNS_TRIGGER_STATE_AXIS_FWD_ANIM_ELEM_EQ:
            return context->state_axis > 0 &&
                   context->anim_element ==
                       (uint16_t)(controller->trigger_value < 1
                                      ? 1
                                      : controller->trigger_value) &&
                   context->anim_element_time == 0u;

        case IK_CNS_TRIGGER_NOT_BOUND:
            return context->is_bound == 0u;

        case IK_CNS_TRIGGER_THROW_GROUND_RECOVERY:
            return context->alive != 0u &&
                   context->can_recover != 0u &&
                   (context->command_mask & IK_CNS_COMMAND_RECOVERY) != 0u &&
                   context->vy_q8 > 0 &&
                   context->y_q8 >=
                       context->floor_y_q8 + controller->trigger_value;

        case IK_CNS_TRIGGER_THROW_AIR_RECOVERY:
            return context->alive != 0u &&
                   context->can_recover != 0u &&
                   (context->command_mask & IK_CNS_COMMAND_RECOVERY) != 0u &&
                   context->vy_q8 > 0;

        default:
            return ik_cns_trigger_now(
                controller->trigger_kind, controller->trigger_value,
                context->state_time, context->anim_element,
                context->anim_element_time, context->anim_ended);
    }
}

int ik_cns_controller_trigger_now(const ik_cns_controller_t* controller,
                                  uint16_t state_time,
                                  uint16_t anim_element,
                                  uint16_t anim_element_time,
                                  int anim_ended,
                                  int move_contact) {
    const ik_cns_controller_context_t context = {
        .state_time = state_time,
        .anim_element = anim_element,
        .anim_element_time = anim_element_time,
        .alive = 1u,
        .can_recover = 1u,
        .anim_ended = (uint8_t)(anim_ended != 0),
        .move_contact = (uint8_t)(move_contact != 0)
    };
    return ik_cns_controller_trigger_context_now(controller, &context);
}

/* HitDef is stateful: once a HitDef trigger has run it remains the current
 * attack definition until another HitDef replaces it or the state changes. */
static int hitdef_has_fired(const ik_cns_hitdef_t* hitdef,
                            uint16_t state_time, uint16_t anim_element) {
    if (!hitdef) return 0;
    switch ((ik_cns_trigger_kind_t)hitdef->trigger_kind) {
        case IK_CNS_TRIGGER_ALWAYS:
            return 1;
        case IK_CNS_TRIGGER_TIME_EQ:
            return state_time >=
                   (uint16_t)(hitdef->trigger_value < 0
                                  ? 0
                                  : hitdef->trigger_value);
        case IK_CNS_TRIGGER_ANIM_ELEM_EQ:
            return anim_element >=
                   (uint16_t)(hitdef->trigger_value < 1
                                  ? 1
                                  : hitdef->trigger_value);
        default:
            return 0;
    }
}

const ik_cns_hitdef_t* ik_cns_active_hitdef(const ik_cns_asset_t* asset,
                                            int16_t state_number,
                                            uint16_t state_time,
                                            uint16_t anim_element) {
    const ik_cns_state_t* state = ik_cns_find_state(asset, state_number);
    if (!state || !asset->hitdefs || state->hitdef_count == 0u) return 0;

    const ik_cns_hitdef_t* active = 0;
    for (uint8_t i = 0u; i < state->hitdef_count; ++i) {
        const uint16_t index = (uint16_t)(state->hitdef_ofs + i);
        if (index >= asset->hitdef_count) break;
        const ik_cns_hitdef_t* hitdef = &asset->hitdefs[index];
        if (hitdef_has_fired(hitdef, state_time, anim_element)) {
            active = hitdef;
        }
    }
    return active;
}
