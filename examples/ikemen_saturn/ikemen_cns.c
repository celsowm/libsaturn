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
            return state_time == (uint16_t)(trigger_value < 0 ? 0 : trigger_value);
        case IK_CNS_TRIGGER_ANIM_ELEM_EQ:
            return anim_element == (uint16_t)(trigger_value < 1 ? 1 : trigger_value) &&
                   anim_element_time == 0u;
        case IK_CNS_TRIGGER_ANIM_END:
            return anim_ended != 0;
        default:
            return 0;
    }
}

/* HitDef is a stateful controller: once its trigger has executed, that HitDef
 * remains the current attack definition until another HitDef replaces it or
 * the state changes. This is deliberately different from trigger_now(). */
static int hitdef_has_fired(const ik_cns_hitdef_t* hitdef,
                            uint16_t state_time, uint16_t anim_element) {
    if (!hitdef) return 0;
    switch ((ik_cns_trigger_kind_t)hitdef->trigger_kind) {
        case IK_CNS_TRIGGER_ALWAYS:
            return 1;
        case IK_CNS_TRIGGER_TIME_EQ:
            return state_time >=
                   (uint16_t)(hitdef->trigger_value < 0 ? 0 : hitdef->trigger_value);
        case IK_CNS_TRIGGER_ANIM_ELEM_EQ:
            return anim_element >=
                   (uint16_t)(hitdef->trigger_value < 1 ? 1 : hitdef->trigger_value);
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
        if (hitdef_has_fired(hitdef, state_time, anim_element)) active = hitdef;
    }
    return active;
}
