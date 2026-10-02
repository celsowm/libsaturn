/* Paused-owner, projectile-trade and entity contact resolution. */
#include "ikemen_fight_internal.h"

void ikf_resolve_paused_owner_contact(
    ik_fight_t* fight,
    int attacker_index,
    const ik_fight_controls_t* p1,
    const ik_fight_controls_t* p2,
    const ik_frame_table_t* p1_frames,
    const ik_frame_table_t* p2_frames
) {
    if (!fight || attacker_index < 0 || attacker_index > 1) return;

    const int victim_index = attacker_index ^ 1;
    ik_fighter_t* attacker = &fight->fighters[attacker_index];
    ik_fighter_t* victim = &fight->fighters[victim_index];
    const ik_frame_table_t* attacker_frames =
        frames_for_fighter(fight, attacker);
    const ik_frame_table_t* victim_frames =
        frames_for_fighter(fight, victim);
    if (!attacker_frames) {
        attacker_frames = attacker_index == 0 ? p1_frames : p2_frames;
    }
    if (!victim_frames) {
        victim_frames = victim_index == 0 ? p1_frames : p2_frames;
    }
    const ik_fight_controls_t* victim_controls =
        victim_index == 0 ? p1 : p2;

    uint8_t local_hitdef = 0u;
    const ik_cns_hitdef_t* hitdef =
        ikf_active_hitdef(
            fight, attacker_frames, attacker, victim, &local_hitdef);
    if (!hitdef || local_hitdef >= 32u) return;
    if (!ikf_hitdef_allows_target(fight, victim, hitdef)) return;
    if (ikf_fighter_not_hit_by_blocks(
            fight, victim,
            ikf_reversal_state_bit(fight, attacker), hitdef)) {
        return;
    }

    const uint32_t bit = (uint32_t)1u << local_hitdef;
    if ((attacker->hitdef_hit_mask & bit) != 0u) return;

    const ik_cns_reversaldef_t* reversal =
        ikf_active_reversaldef(fight, victim, attacker, hitdef);
    if (reversal && ikf_fighter_reversal_clsn_overlap(
            victim_frames, attacker_frames, victim, attacker)) {
        attacker->hitdef_hit_mask |= bit;
        attacker->move_contact = 1u;
        attacker->hit_pause = reversal->pause_p2;
        victim->hit_pause = reversal->pause_p1;
        if (reversal->p2_spr_priority != -128) {
            attacker->spr_priority = reversal->p2_spr_priority;
        }
        if (reversal->p1_state_no >= 0) {
            ikf_enter_state_deferred(fight, victim, reversal->p1_state_no);
        }
        if (reversal->p1_spr_priority != -128) {
            victim->spr_priority = reversal->p1_spr_priority;
        }
        ikf_queue_reversal_effect(fight, victim, reversal);
        ikf_queue_sound_event(
            fight, reversal->hit_sound_group, reversal->hit_sound_item);
        fight->events |= IK_EVENT_GUARD;
        return;
    }

    const ik_cns_hitoverride_t* override =
        ikf_active_hitoverride(fight, victim, hitdef);
    if (override && override->target_state >= 0 &&
        ikf_fighter_clsn_overlap(
            attacker_frames, victim_frames, attacker, victim)) {
        attacker->hitdef_hit_mask |= bit;
        attacker->move_contact = 1u;
        ikf_enter_state_deferred(fight, victim, override->target_state);
        return;
    }

    if (!ikf_juggle_allows_target(
            fight, attacker, victim, hitdef)) {
        return;
    }
    if (!ikf_fighter_clsn_overlap(
            attacker_frames, victim_frames, attacker, victim)) {
        return;
    }

    attacker->hitdef_hit_mask |= bit;
    if ((hitdef->flags & IK_CNS_HITDEF_THROW) != 0u) {
        const ik_fight_controls_t* attacker_controls =
            attacker_index == 0 ? p1 : p2;
        ikf_apply_throw(
            fight, attacker_index, attacker_controls, hitdef);
    } else if (ikf_can_guard_hit(
                   fight, victim, victim_controls, hitdef)) {
        ikf_apply_guard(fight, victim_index, victim_controls, hitdef);
    } else {
        ikf_apply_damage(fight, victim_index, hitdef);
    }
}

void ikf_resolve_projectile_trades(
    ik_fight_t* fight,
    const ik_frame_table_t* p1_frames,
    const ik_frame_table_t* p2_frames
) {
    if (!fight || !fight->entities) return;

    for (uint8_t a_slot = 0u; a_slot < IK_ENTITY_CAPACITY; ++a_slot) {
        ik_entity_t* a = &fight->entities->entities[a_slot];
        if (a->type != IK_ENTITY_PROJECTILE ||
            a->owner_player >= 2u ||
            a->active_hitdef_global < 0 ||
            a->projectile_priority == 0u) {
            continue;
        }

        for (uint8_t b_slot = (uint8_t)(a_slot + 1u);
             b_slot < IK_ENTITY_CAPACITY; ++b_slot) {
            ik_entity_t* b = &fight->entities->entities[b_slot];
            if (b->type != IK_ENTITY_PROJECTILE ||
                b->owner_player >= 2u ||
                b->owner_player == a->owner_player ||
                b->active_hitdef_global < 0 ||
                b->projectile_priority == 0u) {
                continue;
            }

            const ik_frame_table_t* a_frames =
                frames_for_entity(fight, a);
            const ik_frame_table_t* b_frames =
                frames_for_entity(fight, b);
            if (!a_frames) {
                a_frames = a->owner_player == 0u ? p1_frames : p2_frames;
            }
            if (!b_frames) {
                b_frames = b->owner_player == 0u ? p1_frames : p2_frames;
            }
            if (!ikf_entity_attack_clsn_overlap(a_frames, b_frames, a, b)) {
                continue;
            }

            ik_entity_handle_t ah = {
                a_slot, fight->entities->generations[a_slot]
            };
            ik_entity_handle_t bh = {
                b_slot, fight->entities->generations[b_slot]
            };

            if (a->projectile_priority > 0u) {
                --a->projectile_priority;
            }
            if (b->projectile_priority > 0u) {
                --b->projectile_priority;
            }

            const int cancel_a = a->projectile_priority == 0u;
            const int cancel_b = b->projectile_priority == 0u;
            if (cancel_a) {
                (void)ikf_projectile_contact_consumed(fight, ah, 1);
            }
            if (cancel_b) {
                (void)ikf_projectile_contact_consumed(fight, bh, 1);
            }

            a = ik_entity_get(fight->entities, ah);
            if (!a || a->type != IK_ENTITY_PROJECTILE ||
                a->active_hitdef_global < 0) {
                break;
            }
        }
    }
}

void ikf_resolve_entity_contacts(
    ik_fight_t* fight,
    const ik_fight_controls_t* p1,
    const ik_fight_controls_t* p2,
    const ik_frame_table_t* p1_frames,
    const ik_frame_table_t* p2_frames,
    int owner_filter
) {
    if (!fight || !fight->entities || !fight->cns) return;

    ik_entity_handle_t attackers[IK_ENTITY_CAPACITY];
    uint8_t attacker_count = 0u;
    for (uint8_t slot = 0u; slot < IK_ENTITY_CAPACITY; ++slot) {
        const ik_entity_t* entity = &fight->entities->entities[slot];
        if ((entity->type != IK_ENTITY_HELPER &&
             entity->type != IK_ENTITY_PROJECTILE) ||
            entity->owner_player >= 2u ||
            (owner_filter >= 0 &&
             entity->owner_player != (uint8_t)owner_filter)) {
            continue;
        }
        attackers[attacker_count].slot = slot;
        attackers[attacker_count].generation =
            fight->entities->generations[slot];
        ++attacker_count;
    }

    for (uint8_t n = 0u; n < attacker_count; ++n) {
        ik_entity_t* attacker =
            ik_entity_get(fight->entities, attackers[n]);
        if (!attacker || attacker->owner_player >= 2u) continue;

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
        if (dynamic_throw && attacker->type != IK_ENTITY_HELPER) {
            continue;
        }
        if (!ikf_hitdef_allows_target(fight, v, hitdef)) continue;
        if (!ikf_hitdef_chain_allows_target(
                v, attacker->owner_player, hitdef)) continue;
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

        const ik_cns_reversaldef_t* reversal =
            ikf_active_reversaldef_entity(fight, v, attacker, hitdef);
        if (reversal && ikf_entity_reversal_clsn_overlap(
                victim_frames, attacker_frames, v, attacker)) {
            attacker->hitdef_hit_mask |= bit;
            attacker->move_contact = 1u;
            attacker->hit_pause = reversal->pause_p2;
            if (reversal->p2_spr_priority != -128) {
                attacker->spr_priority = reversal->p2_spr_priority;
            }
            v->hit_pause = reversal->pause_p1;
            if (reversal->p1_state_no >= 0) {
                ikf_enter_state_deferred(fight, v, reversal->p1_state_no);
            }
            if (reversal->p1_spr_priority != -128) {
                v->spr_priority = reversal->p1_spr_priority;
            }
            ikf_queue_reversal_effect(fight, v, reversal);
            ikf_queue_sound_event(
                fight, reversal->hit_sound_group,
                reversal->hit_sound_item);
            fight->events |= IK_EVENT_GUARD;
            if (attacker->type == IK_ENTITY_PROJECTILE) {
                (void)ikf_projectile_contact_consumed(
                    fight, attackers[n], 1);
            }
            continue;
        }

        const ik_cns_hitoverride_t* override =
            ikf_active_hitoverride(fight, v, hitdef);
        if (override && override->target_state >= 0) {
            attacker->hitdef_hit_mask |= bit;
            attacker->move_contact = 1u;
            ikf_enter_state_deferred(fight, v, override->target_state);
            if (attacker->type == IK_ENTITY_PROJECTILE) {
                (void)ikf_projectile_contact_consumed(
                    fight, attackers[n], 0);
            }
            continue;
        }

        if (!ikf_entity_juggle_allows_target(
                fight, attacker, v, hitdef)) {
            continue;
        }

        attacker->hitdef_hit_mask |= bit;
        const ik_fight_controls_t* victim_controls =
            victim == 0 ? p1 : p2;
        int projectile_guarded = 0;
        if (dynamic_throw) {
            ikf_apply_throw_from_entity(
                fight, attackers[n], victim, hitdef,
                p1_frames, p2_frames);
        } else if (ikf_can_guard_hit(
                       fight, v, victim_controls, hitdef)) {
            projectile_guarded = 1;
            ikf_apply_guard_from_entity(
                fight, attackers[n], victim, victim_controls, hitdef);
        } else {
            ikf_apply_damage_from_entity(
                fight, attackers[n], victim, hitdef,
                p1_frames, p2_frames);
        }

        if (attacker->type == IK_ENTITY_PROJECTILE) {
            ikf_mark_projectile_contact(
                fight, attackers[n], projectile_guarded);
            (void)ikf_projectile_contact_consumed(
                fight, attackers[n], 0);
        }

        if (fight->round_over) return;
    }
}
