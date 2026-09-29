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

int ik_cns_controller_trigger_now(const ik_cns_controller_t* controller,
                                  uint16_t state_time,
                                  uint16_t anim_element,
                                  uint16_t anim_element_time,
                                  int anim_ended,
                                  int move_contact) {
    if (!controller) return 0;

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
            return anim_element >= first && anim_element < last;
        }

        case IK_CNS_TRIGGER_MOVE_CONTACT_ELEM_WINDOW: {
            if (!move_contact) return 0;
            const uint16_t first =
                (uint16_t)(controller->trigger_value < 1
                               ? 1
                               : controller->trigger_value);
            const uint16_t last =
                (uint16_t)(controller->trigger_value2 <=
                                   controller->trigger_value
                               ? controller->trigger_value + 1
                               : controller->trigger_value2);

            if (anim_element == first) return anim_element_time > 0u;
            if (anim_element > first && anim_element < last) return 1;
            return anim_element == last && anim_element_time == 0u;
        }

        default:
            return ik_cns_trigger_now(
                controller->trigger_kind, controller->trigger_value,
                state_time, anim_element, anim_element_time, anim_ended);
    }
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
