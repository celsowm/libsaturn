/* Sound and effect events queued for the presentation layer. */
#include "ikemen_fight_internal.h"

void ikf_queue_sound_event(
    ik_fight_t* fight,
    int16_t group,
    int16_t item
) {
    if (!fight || group < 0 || item < 0 ||
        fight->sound_count >= IK_MAX_SOUND_EVENTS) {
        return;
    }
    ik_sound_event_t* event =
        &fight->sound_events[fight->sound_count++];
    event->group = group;
    event->item = item;
}

void ikf_queue_reversal_effect(
    ik_fight_t* fight,
    const ik_fighter_t* defender,
    const ik_cns_reversaldef_t* reversal
) {
    if (!fight || !defender || !reversal || reversal->spark_no < 0 ||
        fight->effect_count >= IK_MAX_EFFECT_EVENTS) {
        return;
    }
    ik_effect_event_t* effect =
        &fight->effect_events[fight->effect_count++];
    effect->action = reversal->spark_no;
    effect->x = (int16_t)(
        defender->x + (int16_t)defender->facing * reversal->spark_x);
    effect->y = (int16_t)(defender->y + reversal->spark_y);
}

void ikf_queue_hit_effect(ik_fight_t* fight,
                             const ik_fighter_t* attacker,
                             const ik_fighter_t* victim,
                             const ik_cns_hitdef_t* hitdef,
                             int16_t action,
                             int use_trigger2_y) {
    if (!fight || !attacker || !victim || !hitdef || action < 0 ||
        fight->effect_count >= IK_MAX_EFFECT_EVENTS) return;
    ik_effect_event_t* effect =
        &fight->effect_events[fight->effect_count++];
    effect->action = action;
    effect->x = (int16_t)(
        victim->x + (int16_t)attacker->facing * hitdef->spark_x);
    /* MUGEN sparkxy: X is relative to P2, Y is relative to P1. Fast Upper
     * reuses one HitDef through trigger2 and supplies a second spark Y. */
    const int16_t spark_y =
        use_trigger2_y && attacker->active_hitdef_secondary &&
        hitdef->has_trigger2
            ? hitdef->trigger2_spark_y
            : hitdef->spark_y;
    effect->y = (int16_t)(attacker->y + spark_y);
}

void ikf_queue_entity_hit_effect(
    ik_fight_t* fight,
    const ik_entity_t* attacker,
    const ik_fighter_t* victim,
    const ik_cns_hitdef_t* hitdef,
    int16_t action,
    int use_trigger2_y
) {
    if (!fight || !attacker || !victim || !hitdef || action < 0 ||
        fight->effect_count >= IK_MAX_EFFECT_EVENTS) {
        return;
    }
    ik_effect_event_t* effect =
        &fight->effect_events[fight->effect_count++];
    effect->action = action;
    effect->x = (int16_t)(
        victim->x + (int16_t)attacker->facing * hitdef->spark_x);
    const int16_t spark_y =
        use_trigger2_y && attacker->active_hitdef_secondary &&
        hitdef->has_trigger2
            ? hitdef->trigger2_spark_y
            : hitdef->spark_y;
    effect->y = (int16_t)(
        ik_cns_q8_to_int(attacker->y_q8) + spark_y);
}
