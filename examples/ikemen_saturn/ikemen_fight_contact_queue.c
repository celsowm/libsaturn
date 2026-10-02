/* Global contact queue: gather, arbitrate, apply. */
#include "ikemen_fight_internal.h"

enum { IK_CONTACT_QUEUE_CAPACITY = IK_ENTITY_CAPACITY + 2u };

typedef struct ik_contact_candidate {
    uint8_t owner;
    uint8_t is_entity;
    uint8_t attacker_index;
    uint8_t normal;
    uint8_t lands;
    uint8_t consume;
    uint8_t local_hitdef;
    ik_entity_handle_t entity;
    const ik_cns_hitdef_t* hitdef;
    const ik_cns_reversaldef_t* reversal;
    const ik_cns_hitoverride_t* override;
    uint32_t bit;
} ik_contact_candidate_t;

static uint8_t contact_priority(const ik_contact_candidate_t* candidate) {
    if (!candidate || !candidate->hitdef) return 4u;
    return candidate->hitdef->priority ? candidate->hitdef->priority : 4u;
}

static void arbitrate_contact_pair(
    ik_contact_candidate_t* a,
    ik_contact_candidate_t* b
) {
    if (!a || !b || !a->normal || !b->normal ||
        !a->lands || !b->lands || a->owner == b->owner ||
        !a->hitdef || !b->hitdef) {
        return;
    }

    const uint8_t pa = contact_priority(a);
    const uint8_t pb = contact_priority(b);
    if (pa > pb) {
        b->lands = 0u;
        b->consume = 1u;
        return;
    }
    if (pb > pa) {
        a->lands = 0u;
        a->consume = 1u;
        return;
    }

    const uint8_t ta = a->hitdef->priority_type;
    const uint8_t tb = b->hitdef->priority_type;
    if (ta == IK_CNS_PRIORITY_DODGE ||
        tb == IK_CNS_PRIORITY_DODGE) {
        a->lands = 0u;
        b->lands = 0u;
        return;
    }
    if (ta == IK_CNS_PRIORITY_HIT &&
        tb == IK_CNS_PRIORITY_HIT) {
        return;
    }
    if (ta == IK_CNS_PRIORITY_HIT &&
        tb == IK_CNS_PRIORITY_MISS) {
        b->lands = 0u;
        b->consume = 1u;
        return;
    }
    if (tb == IK_CNS_PRIORITY_HIT &&
        ta == IK_CNS_PRIORITY_MISS) {
        a->lands = 0u;
        a->consume = 1u;
        return;
    }

    a->lands = 0u;
    b->lands = 0u;
}

static void consume_contact_candidate(
    ik_fight_t* fight,
    const ik_contact_candidate_t* candidate
) {
    if (!fight || !candidate || candidate->bit == 0u) return;
    if (!candidate->is_entity) {
        fight->fighters[candidate->attacker_index].hitdef_hit_mask |=
            candidate->bit;
        return;
    }
    if (!fight->entities) return;
    ik_entity_t* attacker =
        ik_entity_get(fight->entities, candidate->entity);
    if (attacker) attacker->hitdef_hit_mask |= candidate->bit;
}

static uint8_t gather_root_contacts(
    ik_fight_t* fight,
    const ik_frame_table_t* p1_frames,
    const ik_frame_table_t* p2_frames,
    ik_contact_candidate_t* queue,
    uint8_t count
) {
    for (int atk = 0; atk < 2 &&
         count < IK_CONTACT_QUEUE_CAPACITY; ++atk) {
        ik_fighter_t* a = &fight->fighters[atk];
        ik_fighter_t* v = &fight->fighters[atk ^ 1];
        const ik_frame_table_t* attacker_frames =
            frames_for_fighter(fight, a);
        const ik_frame_table_t* victim_frames =
            frames_for_fighter(fight, v);
        if (!attacker_frames) {
            attacker_frames = atk == 0 ? p1_frames : p2_frames;
        }
        if (!victim_frames) {
            victim_frames = atk == 0 ? p2_frames : p1_frames;
        }
        uint8_t local_hitdef = 0u;
        const ik_cns_hitdef_t* hitdef =
            ikf_active_hitdef(fight, attacker_frames, a, v, &local_hitdef);
        if (!hitdef || local_hitdef >= 32u) continue;
        if (!ikf_hitdef_allows_target(fight, v, hitdef)) continue;
        if (!ikf_hitdef_chain_allows_target(v, (uint8_t)atk, hitdef)) continue;
        if (ikf_fighter_not_hit_by_blocks(
                fight, v, ikf_reversal_state_bit(fight, a), hitdef)) {
            continue;
        }

        const uint32_t bit = (uint32_t)1u << local_hitdef;
        if ((a->hitdef_hit_mask & bit) != 0u) continue;

        ik_contact_candidate_t candidate = {0};
        candidate.owner = (uint8_t)atk;
        candidate.attacker_index = (uint8_t)atk;
        candidate.hitdef = hitdef;
        candidate.local_hitdef = local_hitdef;
        candidate.bit = bit;

        const ik_cns_reversaldef_t* reversal =
            ikf_active_reversaldef(fight, v, a, hitdef);
        if (reversal && ikf_fighter_reversal_clsn_overlap(
                victim_frames, attacker_frames, v, a)) {
            candidate.reversal = reversal;
            candidate.consume = 1u;
            queue[count++] = candidate;
            continue;
        }

        const ik_cns_hitoverride_t* override =
            ikf_active_hitoverride(fight, v, hitdef);
        if (override && ikf_fighter_clsn_overlap(
                attacker_frames, victim_frames, a, v)) {
            candidate.override = override;
            candidate.consume = 1u;
            queue[count++] = candidate;
            continue;
        }

        if (!ikf_juggle_allows_target(fight, a, v, hitdef)) continue;
        if (!ikf_fighter_clsn_overlap(
                attacker_frames, victim_frames, a, v)) {
            continue;
        }

        candidate.normal = 1u;
        candidate.lands = 1u;
        queue[count++] = candidate;
    }
    return count;
}

static uint8_t gather_entity_contacts(
    ik_fight_t* fight,
    const ik_frame_table_t* p1_frames,
    const ik_frame_table_t* p2_frames,
    ik_contact_candidate_t* queue,
    uint8_t count
) {
    if (!fight || !fight->entities || !fight->cns) return count;

    for (uint8_t slot = 0u;
         slot < IK_ENTITY_CAPACITY &&
         count < IK_CONTACT_QUEUE_CAPACITY; ++slot) {
        ik_entity_t* attacker = &fight->entities->entities[slot];
        if ((attacker->type != IK_ENTITY_HELPER &&
             attacker->type != IK_ENTITY_PROJECTILE) ||
            attacker->owner_player >= 2u) {
            continue;
        }

        const int victim = (int)(attacker->owner_player ^ 1u);
        ik_fighter_t* v = &fight->fighters[victim];
        const ik_frame_table_t* attacker_frames =
            frames_for_entity(fight, attacker);
        const ik_frame_table_t* victim_frames =
            frames_for_fighter(fight, v);
        if (!attacker_frames) {
            attacker_frames =
                attacker->owner_player == 0u ? p1_frames : p2_frames;
        }
        if (!victim_frames) {
            victim_frames = victim == 0 ? p1_frames : p2_frames;
        }

        uint8_t local_hitdef = 0u;
        const ik_cns_hitdef_t* hitdef =
            ikf_active_entity_hitdef(
                fight, attacker_frames, attacker, v, &local_hitdef);
        if (!hitdef || local_hitdef >= 32u) continue;

        const int dynamic_throw =
            (hitdef->flags & IK_CNS_HITDEF_THROW) != 0u;
        if (dynamic_throw && attacker->type != IK_ENTITY_HELPER) continue;
        if (!ikf_hitdef_allows_target(fight, v, hitdef)) continue;
        if (ikf_fighter_not_hit_by_blocks(
                fight, v, ikf_reversal_entity_state_bit(attacker), hitdef)) {
            continue;
        }

        const uint32_t bit = (uint32_t)1u << local_hitdef;
        if ((attacker->hitdef_hit_mask & bit) != 0u) continue;
        if (!ikf_entity_clsn_overlap(
                attacker_frames, victim_frames, attacker, v)) {
            continue;
        }

        ik_contact_candidate_t candidate = {0};
        candidate.owner = attacker->owner_player;
        candidate.is_entity = 1u;
        candidate.entity.slot = slot;
        candidate.entity.generation =
            fight->entities->generations[slot];
        candidate.hitdef = hitdef;
        candidate.local_hitdef = local_hitdef;
        candidate.bit = bit;

        const ik_cns_reversaldef_t* reversal =
            ikf_active_reversaldef_entity(fight, v, attacker, hitdef);
        if (reversal && ikf_entity_reversal_clsn_overlap(
                victim_frames, attacker_frames, v, attacker)) {
            candidate.reversal = reversal;
            candidate.consume = 1u;
            queue[count++] = candidate;
            continue;
        }

        const ik_cns_hitoverride_t* override =
            ikf_active_hitoverride(fight, v, hitdef);
        if (override && override->target_state >= 0) {
            candidate.override = override;
            candidate.consume = 1u;
            queue[count++] = candidate;
            continue;
        }

        if (!ikf_entity_juggle_allows_target(
                fight, attacker, v, hitdef)) {
            continue;
        }

        candidate.normal = 1u;
        candidate.lands = 1u;
        queue[count++] = candidate;
    }
    return count;
}

static void apply_contact_override(
    ik_fight_t* fight,
    const ik_contact_candidate_t* candidate
) {
    if (!fight || !candidate || !candidate->override ||
        candidate->override->target_state < 0) {
        return;
    }

    const int victim = (int)(candidate->owner ^ 1u);
    if (!candidate->is_entity) {
        ik_fighter_t* attacker =
            &fight->fighters[candidate->attacker_index];
        attacker->move_contact = 1u;
        ikf_enter_state_deferred(
            fight, &fight->fighters[victim],
            candidate->override->target_state);
        return;
    }

    if (!fight->entities) return;
    ik_entity_t* attacker =
        ik_entity_get(fight->entities, candidate->entity);
    if (!attacker) return;
    attacker->move_contact = 1u;
    ikf_enter_state_deferred(
        fight, &fight->fighters[victim],
        candidate->override->target_state);
    if (attacker->type == IK_ENTITY_PROJECTILE) {
        (void)ikf_projectile_contact_consumed(
            fight, candidate->entity, 0);
    }
}

static void apply_contact_reversal(
    ik_fight_t* fight,
    const ik_contact_candidate_t* candidate
) {
    if (!fight || !candidate || !candidate->reversal) return;

    const int victim = (int)(candidate->owner ^ 1u);
    ik_fighter_t* defender = &fight->fighters[victim];
    const ik_cns_reversaldef_t* reversal = candidate->reversal;

    if (!candidate->is_entity) {
        ik_fighter_t* attacker =
            &fight->fighters[candidate->attacker_index];
        attacker->hit_pause = reversal->pause_p2;
        defender->hit_pause = reversal->pause_p1;
        if (reversal->p2_spr_priority != -128) {
            attacker->spr_priority = reversal->p2_spr_priority;
        }
        /* The reversed attacker is marked MC_Reversed (MoveContact stays 0)
         * with its hitdef target set to the reverser; the reverser scores a
         * hit on it. */
        attacker->move_contact_type = 2u;
        attacker->reversed = 1u;
        defender->drop_target_skip = 2u;
        attacker->move_contact_time = 1u;
        attacker->hitdef_target = (int8_t)victim;
        {
            /* The reverser now holds a juggle budget on the reversed one. */
            const ik_cns_constants_t* dc =
                constants_for_fighter(fight, defender);
            attacker->juggle_owner = (int8_t)victim;
            attacker->juggle_points =
                (int16_t)((dc && dc->air_juggle > 0) ? dc->air_juggle : 15);
        }
        defender->move_contact = 1u;
        defender->move_contact_type = 0u;
        defender->move_contact_time = 0u;
        defender->target_index = (int8_t)candidate->attacker_index;
        defender->hitdef_target = (int8_t)candidate->attacker_index;
    } else {
        if (!fight->entities) return;
        ik_entity_t* attacker =
            ik_entity_get(fight->entities, candidate->entity);
        if (!attacker) return;
        attacker->move_contact = 1u;
        attacker->hit_pause = reversal->pause_p2;
        defender->hit_pause = reversal->pause_p1;
        if (reversal->p2_spr_priority != -128) {
            attacker->spr_priority = reversal->p2_spr_priority;
        }
        if (attacker->type == IK_ENTITY_PROJECTILE) {
            (void)ikf_projectile_contact_consumed(
                fight, candidate->entity, 1);
        }
    }

    if (reversal->p1_state_no >= 0) {
        ikf_enter_state_deferred(fight, defender, reversal->p1_state_no);
    }
    if (reversal->p1_spr_priority != -128) {
        defender->spr_priority = reversal->p1_spr_priority;
    }
    ikf_queue_reversal_effect(fight, defender, reversal);
    ikf_queue_sound_event(
        fight, reversal->hit_sound_group, reversal->hit_sound_item);
    fight->events |= IK_EVENT_GUARD;
}

static void apply_normal_contact(
    ik_fight_t* fight,
    const ik_fight_controls_t* p1,
    const ik_fight_controls_t* p2,
    const ik_frame_table_t* p1_frames,
    const ik_frame_table_t* p2_frames,
    const ik_contact_candidate_t* candidate
) {
    if (!fight || !candidate || !candidate->normal ||
        !candidate->lands || !candidate->hitdef) {
        return;
    }

    const int victim = (int)(candidate->owner ^ 1u);
    ik_fighter_t* v = &fight->fighters[victim];
    const ik_fight_controls_t* victim_controls =
        victim == 0 ? p1 : p2;
    const ik_cns_hitdef_t* hitdef = candidate->hitdef;

    if (!candidate->is_entity) {
        ik_fighter_t* attacker =
            &fight->fighters[candidate->attacker_index];
        attacker->hitdef_hit_mask |= candidate->bit;
        if ((hitdef->flags & IK_CNS_HITDEF_THROW) != 0u) {
            const ik_fight_controls_t* attacker_controls =
                candidate->attacker_index == 0 ? p1 : p2;
            ikf_apply_throw(
                fight, candidate->attacker_index,
                attacker_controls, hitdef);
        } else if (ikf_can_guard_hit(
                       fight, v, victim_controls, hitdef)) {
            ikf_apply_guard(fight, victim, victim_controls, hitdef);
        } else {
            ikf_apply_damage(fight, victim, hitdef);
        }
        return;
    }

    if (!fight->entities) return;
    ik_entity_t* attacker =
        ik_entity_get(fight->entities, candidate->entity);
    if (!attacker) return;
    attacker->hitdef_hit_mask |= candidate->bit;

    int projectile_guarded = 0;
    if ((hitdef->flags & IK_CNS_HITDEF_THROW) != 0u) {
        ikf_apply_throw_from_entity(
            fight, candidate->entity, victim, hitdef,
            p1_frames, p2_frames);
    } else if (ikf_can_guard_hit(
                   fight, v, victim_controls, hitdef)) {
        projectile_guarded = 1;
        ikf_apply_guard_from_entity(
            fight, candidate->entity, victim,
            victim_controls, hitdef);
    } else {
        ikf_apply_damage_from_entity(
            fight, candidate->entity, victim, hitdef,
            p1_frames, p2_frames);
    }

    attacker = ik_entity_get(fight->entities, candidate->entity);
    if (attacker && attacker->type == IK_ENTITY_PROJECTILE) {
        ikf_mark_projectile_contact(
            fight, candidate->entity, projectile_guarded);
        (void)ikf_projectile_contact_consumed(
            fight, candidate->entity, 0);
    }
}

void ikf_resolve_global_contacts(
    ik_fight_t* fight,
    const ik_fight_controls_t* p1,
    const ik_fight_controls_t* p2,
    const ik_frame_table_t* p1_frames,
    const ik_frame_table_t* p2_frames
) {
    if (!fight) return;

    ik_contact_candidate_t queue[IK_CONTACT_QUEUE_CAPACITY] = {{0}};
    uint8_t count = 0u;
    count = gather_root_contacts(
        fight, p1_frames, p2_frames, queue, count);
    count = gather_entity_contacts(
        fight, p1_frames, p2_frames, queue, count);

    for (uint8_t i = 0u; i < count; ++i) {
        for (uint8_t j = (uint8_t)(i + 1u); j < count; ++j) {
            arbitrate_contact_pair(&queue[i], &queue[j]);
        }
    }

    for (uint8_t i = 0u; i < count; ++i) {
        if (queue[i].consume) {
            consume_contact_candidate(fight, &queue[i]);
        }
    }

    for (uint8_t i = 0u; i < count; ++i) {
        apply_contact_override(fight, &queue[i]);
    }
    for (uint8_t i = 0u; i < count; ++i) {
        apply_contact_reversal(fight, &queue[i]);
    }
    for (uint8_t i = 0u; i < count; ++i) {
        apply_normal_contact(
            fight, p1, p2, p1_frames, p2_frames, &queue[i]);
    }
}
