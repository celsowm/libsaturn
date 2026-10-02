/* Which HitDef/ReversalDef/HitOverride is currently active. */
#include "ikemen_fight_internal.h"

uint8_t ikf_reversal_state_bit(
    const ik_fight_t* fight,
    const ik_fighter_t* attacker
) {
    switch ((ik_cns_state_type_t)ik_fight_state_type(fight, attacker)) {
        case IK_CNS_STATE_STAND:
            return IK_CNS_REVERSAL_STATE_STAND;
        case IK_CNS_STATE_CROUCH:
            return IK_CNS_REVERSAL_STATE_CROUCH;
        case IK_CNS_STATE_AIR:
            return IK_CNS_REVERSAL_STATE_AIR;
        default:
            return 0u;
    }
}

int ikf_fighter_not_hit_by_blocks(
    const ik_fight_t* fight,
    const ik_fighter_t* victim,
    uint8_t attacker_state_bit,
    const ik_cns_hitdef_t* incoming
) {
    if (!fight || !victim || !incoming ||
        victim->not_hit_by_time == 0u ||
        victim->not_hit_by_mask == 0u) {
        return 0;
    }
    if ((victim->not_hit_by_mask & attacker_state_bit) == 0u) {
        return 0;
    }
    if (victim->not_hit_by_attr_mask == 0u) {
        return 1;
    }
    return (victim->not_hit_by_attr_mask &
            incoming->attack_attr_mask) != 0u;
}

const ik_cns_reversaldef_t* ikf_active_reversaldef(
    const ik_fight_t* fight,
    const ik_fighter_t* defender,
    const ik_fighter_t* attacker,
    const ik_cns_hitdef_t* incoming
) {
    if (!fight || !defender || !attacker || !incoming) return 0;
    const ik_cns_asset_t* state_cns = cns_for_fighter(fight, defender);
    if (!state_cns || !state_cns->reversals) return 0;

    const ik_cns_state_t* state =
        ik_cns_find_state(state_cns, defender->state);
    if (!state || state->reversal_count == 0u) return 0;

    const uint8_t attacker_bit = ikf_reversal_state_bit(fight, attacker);
    for (uint8_t i = 0u; i < state->reversal_count; ++i) {
        const uint16_t index = (uint16_t)(state->reversal_ofs + i);
        if (index >= state_cns->reversal_count) break;
        const ik_cns_reversaldef_t* reversal =
            &state_cns->reversals[index];
        const uint16_t authored_time =
            tick_state_time(defender) > 0u
                ? (uint16_t)(tick_state_time(defender) - 1u)
                : 0u;
        if (authored_time < reversal->start_time ||
            authored_time >= reversal->end_time) {
            continue;
        }
        if ((reversal->attacker_state_mask & attacker_bit) == 0u) {
            continue;
        }
        if (reversal->incoming_attr_mask != 0u &&
            (reversal->incoming_attr_mask &
             incoming->attack_attr_mask) == 0u) {
            continue;
        }
        return reversal;
    }
    return 0;
}

uint8_t ikf_reversal_entity_state_bit(
    const ik_entity_t* attacker
) {
    if (!attacker) return 0u;
    switch ((ik_cns_state_type_t)attacker->state_type) {
        case IK_CNS_STATE_STAND:
            return IK_CNS_REVERSAL_STATE_STAND;
        case IK_CNS_STATE_CROUCH:
            return IK_CNS_REVERSAL_STATE_CROUCH;
        case IK_CNS_STATE_AIR:
            return IK_CNS_REVERSAL_STATE_AIR;
        default:
            return 0u;
    }
}

const ik_cns_reversaldef_t* ikf_active_reversaldef_entity(
    const ik_fight_t* fight,
    const ik_fighter_t* defender,
    const ik_entity_t* attacker,
    const ik_cns_hitdef_t* incoming
) {
    if (!fight || !defender || !incoming) return 0;
    const ik_cns_asset_t* state_cns = cns_for_fighter(fight, defender);
    if (!state_cns || !state_cns->reversals) return 0;

    const ik_cns_state_t* state =
        ik_cns_find_state(state_cns, defender->state);
    if (!state || state->reversal_count == 0u) return 0;

    const uint8_t attacker_bit = ikf_reversal_entity_state_bit(attacker);
    const uint16_t authored_time =
        tick_state_time(defender) > 0u
            ? (uint16_t)(tick_state_time(defender) - 1u)
            : 0u;

    for (uint8_t i = 0u; i < state->reversal_count; ++i) {
        const uint16_t index = (uint16_t)(state->reversal_ofs + i);
        if (index >= state_cns->reversal_count) break;
        const ik_cns_reversaldef_t* reversal =
            &state_cns->reversals[index];
        if (authored_time < reversal->start_time ||
            authored_time >= reversal->end_time) {
            continue;
        }
        if ((reversal->attacker_state_mask & attacker_bit) == 0u) {
            continue;
        }
        if (reversal->incoming_attr_mask != 0u &&
            (reversal->incoming_attr_mask &
             incoming->attack_attr_mask) == 0u) {
            continue;
        }
        return reversal;
    }
    return 0;
}

const ik_cns_hitoverride_t* ikf_active_hitoverride(
    const ik_fight_t* fight,
    const ik_fighter_t* victim,
    const ik_cns_hitdef_t* incoming
) {
    if (!fight || !victim || !incoming ||
        incoming->attack_attr_mask == 0u) return 0;
    const ik_cns_asset_t* state_cns = cns_for_fighter(fight, victim);
    if (!state_cns || !state_cns->hitoverrides) return 0;

    const ik_cns_state_t* state =
        ik_cns_find_state(state_cns, victim->state);
    if (!state || state->hitoverride_count == 0u) return 0;

    const uint8_t self_bit = ikf_reversal_state_bit(fight, victim);
    const uint16_t authored_time =
        tick_state_time(victim) > 0u
            ? (uint16_t)(tick_state_time(victim) - 1u)
            : 0u;
    for (uint8_t i = 0u; i < state->hitoverride_count; ++i) {
        const uint16_t index = (uint16_t)(state->hitoverride_ofs + i);
        if (index >= state_cns->hitoverride_count) break;
        const ik_cns_hitoverride_t* override =
            &state_cns->hitoverrides[index];
        if (authored_time < override->start_time ||
            authored_time >= override->end_time) {
            continue;
        }
        if ((override->self_state_mask & self_bit) == 0u) continue;
        if ((override->incoming_attr_mask &
             incoming->attack_attr_mask) == 0u) {
            continue;
        }
        return override;
    }
    return 0;
}

const ik_cns_hitdef_t* ikf_active_hitdef(ik_fight_t* fight,
                                            const ik_frame_table_t* frames,
                                            ik_fighter_t* fighter,
                                            const ik_fighter_t* victim,
                                            uint8_t* out_local_index) {
    if (!fight || !fighter) return 0;
    {
        const ik_frame_table_t* owned_frames =
            frames_for_fighter(fight, fighter);
        if (owned_frames) frames = owned_frames;
    }
    const ik_cns_asset_t* state_cns = cns_for_fighter(fight, fighter);
    if (!state_cns) return 0;
    const ik_cns_state_t* state =
        ik_cns_find_state(state_cns, fighter->state);
    if (!state || state->move_type != IK_CNS_MOVE_ATTACK ||
        !state_cns->hitdefs) return 0;

    if (state->hitdef_count == 0u) {
        if (state->hitdef_persist &&
            fighter->active_hitdef_global >= 0 &&
            fighter->active_hitdef_global < (int16_t)state_cns->hitdef_count) {
            if (out_local_index) {
                *out_local_index = (uint8_t)(
                    fighter->active_hitdef_local < 0
                        ? 0
                        : fighter->active_hitdef_local);
            }
            return &state_cns->hitdefs[
                (uint16_t)fighter->active_hitdef_global];
        }
        return 0;
    }

    uint16_t element = 1u;
    uint16_t element_time = 0u;
    int anim_ended = 0;
    const uint16_t eval_state_time = tick_state_time(fighter);
    ikf_anim_position(
        frames, fighter, &element, &element_time, &anim_ended);
    const int p2_dist = ikf_body_dist_x(fighter, victim);

    /* HitDef controllers execute in source order. A HitDef that triggers on
     * this tick replaces the current one and then remains active until another
     * HitDef fires or the state changes. */
    /* HitDef controllers only run on ticks the fighter is not frozen by hit
     * pause, so a paused attacker neither re-arms nor swaps its HitDef. */
    for (uint8_t i = 0u; i < state->hitdef_count && !fighter->frozen_tick;
         ++i) {
        const uint16_t global = (uint16_t)(state->hitdef_ofs + i);
        if (global >= state_cns->hitdef_count) break;
        const ik_cns_hitdef_t* hitdef = &state_cns->hitdefs[global];
        const int primary_now = ikf_hitdef_trigger_now(
            frames, fighter->anim,
            eval_state_time, fighter->anim_time,
            element, element_time, anim_ended,
            hitdef->trigger_kind, hitdef->trigger_value);
        const int secondary_now =
            hitdef->has_trigger2 &&
            ikf_hitdef_trigger_now(
                frames, fighter->anim,
                eval_state_time, fighter->anim_time,
                element, element_time, anim_ended,
                hitdef->trigger2_kind, hitdef->trigger2_value);
        if (!primary_now && !secondary_now) continue;
        if (!ikf_hitdef_p2_dist_allows(hitdef, p2_dist)) continue;

        /* A second trigger on the same HitDef controller is an intentional
         * re-activation (Fast Upper). Re-arm only this controller's hit bit. */
        if (secondary_now && i < 32u) {
            fighter->hitdef_hit_mask &= ~(1u << i);
        }
        /* A freshly executed HitDef starts with no HitDef targets; this
         * pass may run more than once in a tick, so only a change counts. */
        if (secondary_now || fighter->active_hitdef_global != (int16_t)global) {
            fighter->hitdef_target = -1;
        }
        fighter->active_hitdef_secondary =
            (uint8_t)(secondary_now != 0);
        fighter->active_hitdef_local = (int8_t)i;
        fighter->active_hitdef_global = (int16_t)global;
    }

    if (fighter->active_hitdef_global < 0 ||
        fighter->active_hitdef_global >= (int16_t)state_cns->hitdef_count) {
        return 0;
    }
    if (out_local_index) {
        *out_local_index = (uint8_t)(
            fighter->active_hitdef_local < 0
                ? 0
                : fighter->active_hitdef_local);
    }
    return &state_cns->hitdefs[
        (uint16_t)fighter->active_hitdef_global];
}

const ik_cns_hitdef_t* ikf_active_entity_hitdef(
    ik_fight_t* fight,
    const ik_frame_table_t* frames,
    ik_entity_t* entity,
    const ik_fighter_t* victim,
    uint8_t* out_local_index
) {
    if (!fight || !entity || !victim) return 0;
    const ik_cns_asset_t* state_cns =
        cns_for_owner(fight, entity->state_owner);
    if (!state_cns) return 0;

    if (entity->type == IK_ENTITY_PROJECTILE &&
        entity->state_no < 0 &&
        entity->active_hitdef_global >= 0 &&
        entity->active_hitdef_global < (int16_t)state_cns->hitdef_count) {
        if (entity->projectile_hit_cooldown > 0u) return 0;
        if (out_local_index) *out_local_index = 0u;
        return &state_cns->hitdefs[
            (uint16_t)entity->active_hitdef_global];
    }

    const ik_cns_state_t* state =
        ik_cns_find_state(state_cns, entity->state_no);
    if (!state || state->move_type != IK_CNS_MOVE_ATTACK ||
        !state_cns->hitdefs) {
        return 0;
    }

    if (state->hitdef_count == 0u) {
        if (state->hitdef_persist &&
            entity->active_hitdef_global >= 0 &&
            entity->active_hitdef_global <
                (int16_t)state_cns->hitdef_count) {
            if (out_local_index) {
                *out_local_index = (uint8_t)(
                    entity->active_hitdef_local < 0
                        ? 0 : entity->active_hitdef_local);
            }
            return &state_cns->hitdefs[
                (uint16_t)entity->active_hitdef_global];
        }
        return 0;
    }

    uint16_t element = 1u;
    uint16_t element_time = 0u;
    int anim_ended = 0;
    ikf_entity_anim_position(
        frames, entity, &element, &element_time, &anim_ended);
    const int p2_dist = ikf_entity_body_dist_x(entity, victim);
    /* state_time already advanced at the end of the entity's tick. */
    const uint16_t eval_time =
        entity->state_time > 0u ? (uint16_t)(entity->state_time - 1u) : 0u;

    for (uint8_t n = 0u; n < state->hitdef_count; ++n) {
        const uint16_t global =
            (uint16_t)(state->hitdef_ofs + n);
        if (global >= state_cns->hitdef_count) break;
        const ik_cns_hitdef_t* hitdef =
            &state_cns->hitdefs[global];
        const int primary_now = ikf_hitdef_trigger_now(
            frames, entity->anim_no,
            eval_time, entity->anim_time,
            element, element_time, anim_ended,
            hitdef->trigger_kind, hitdef->trigger_value);
        const int secondary_now =
            hitdef->has_trigger2 &&
            ikf_hitdef_trigger_now(
                frames, entity->anim_no,
                eval_time, entity->anim_time,
                element, element_time, anim_ended,
                hitdef->trigger2_kind, hitdef->trigger2_value);
        if (!primary_now && !secondary_now) continue;
        if (!ikf_hitdef_p2_dist_allows(hitdef, p2_dist)) continue;

        if (secondary_now && n < 32u) {
            entity->hitdef_hit_mask &= ~(1u << n);
        }
        entity->active_hitdef_secondary =
            (uint8_t)(secondary_now != 0);
        entity->active_hitdef_local = (int8_t)n;
        entity->active_hitdef_global = (int16_t)global;
    }

    if (entity->active_hitdef_global < 0 ||
        entity->active_hitdef_global >=
            (int16_t)state_cns->hitdef_count) {
        return 0;
    }
    if (out_local_index) {
        *out_local_index = (uint8_t)(
            entity->active_hitdef_local < 0
                ? 0 : entity->active_hitdef_local);
    }
    return &state_cns->hitdefs[
        (uint16_t)entity->active_hitdef_global];
}
