/* Hit eligibility: target attributes, chains, juggle, guard threat. */
#include "ikemen_fight_internal.h"

void ikf_advance_projectile_query_times(
    ik_fight_t* fight
) {
    if (!fight || !fight->entities) return;
    for (uint8_t player = 0u; player < 2u; ++player) {
        ik_entity_t* root =
            ik_entity_get(
                fight->entities, fight->player_entities[player]);
        if (!root) continue;
        if (root->proj_query_contact_time >= 0 &&
            root->proj_query_contact_time < 32767) {
            ++root->proj_query_contact_time;
        }
        if (root->proj_query_hit_time >= 0 &&
            root->proj_query_hit_time < 32767) {
            ++root->proj_query_hit_time;
        }
        if (root->proj_query_guarded_time >= 0 &&
            root->proj_query_guarded_time < 32767) {
            ++root->proj_query_guarded_time;
        }
    }
}

void ikf_mark_projectile_contact(
    ik_fight_t* fight,
    ik_entity_handle_t projectile_handle,
    int guarded
) {
    if (!fight || !fight->entities) return;
    const ik_entity_t* projectile =
        ik_entity_get_const(fight->entities, projectile_handle);
    if (!projectile || projectile->type != IK_ENTITY_PROJECTILE) return;

    ik_entity_t* root =
        ik_entity_get(fight->entities, projectile->root);
    if (!root) return;

    root->proj_query_contact = 1u;
    root->proj_query_contact_time = 0;
    root->proj_query_hit = guarded ? 0u : 1u;
    root->proj_query_guarded = guarded ? 1u : 0u;
    if (guarded) {
        root->proj_query_guarded_time = 0;
    } else {
        root->proj_query_hit_time = 0;
    }
}

int ikf_projectile_contact_consumed(
    ik_fight_t* fight,
    ik_entity_handle_t handle,
    int canceled
) {
    if (!fight || !fight->entities) return 1;
    ik_entity_t* projectile =
        ik_entity_get(fight->entities, handle);
    if (!projectile || projectile->type != IK_ENTITY_PROJECTILE) {
        return 1;
    }

    if (canceled) {
        if (projectile->projectile_cancel_anim_no >= 0) {
            projectile->anim_no = projectile->projectile_cancel_anim_no;
            projectile->anim_time = 0u;
            projectile->move_type = IK_CNS_MOVE_IDLE;
            projectile->active_hitdef_global = -1;
            projectile->remove_time = 0;
            return 0;
        }
        (void)ik_entity_destroy(fight->entities, handle);
        return 1;
    }

    if (projectile->projectile_hit_anim_no >= 0) {
        projectile->anim_no = projectile->projectile_hit_anim_no;
        projectile->anim_time = 0u;
    }

    if (projectile->projectile_hits_left > 0u) {
        --projectile->projectile_hits_left;
    }
    if (projectile->projectile_remove_on_hit ||
        projectile->projectile_hits_left == 0u) {
        if (projectile->projectile_remove_anim_no >= 0) {
            projectile->anim_no =
                projectile->projectile_remove_anim_no;
            projectile->anim_time = 0u;
            projectile->move_type = IK_CNS_MOVE_IDLE;
            projectile->active_hitdef_global = -1;
            projectile->remove_time = 0;
            return 0;
        }
        (void)ik_entity_destroy(fight->entities, handle);
        return 1;
    }

    projectile->projectile_hit_cooldown =
        projectile->projectile_miss_time;
    if (projectile->projectile_hit_cooldown == 0u) {
        projectile->hitdef_hit_mask = 0u;
        if (projectile->projectile_main_anim_no >= 0 &&
            projectile->anim_no != projectile->projectile_main_anim_no) {
            projectile->anim_no = projectile->projectile_main_anim_no;
            projectile->anim_time = 0u;
        }
    }
    return 0;
}

int ikf_hitdef_allows_target(const ik_fight_t* fight,
                                const ik_fighter_t* victim,
                                const ik_cns_hitdef_t* hitdef) {
    if (!victim || !hitdef) return 0;

    const uint8_t flags = hitdef->hit_flags != 0u
        ? hitdef->hit_flags
        : IK_CNS_HIT_DEFAULT;
    const ik_cns_state_t* spec = fighter_state_spec(fight, victim);
    const int gethit =
        (spec && spec->move_type == IK_CNS_MOVE_HIT) ||
        victim->state == IK_STATE_HIT ||
        victim->state == IK_STATE_KO;

    if ((flags & IK_CNS_HIT_ONLY_GETHIT) != 0u && !gethit) return 0;
    if ((flags & IK_CNS_HIT_NOT_GETHIT) != 0u && gethit) return 0;

    switch ((ik_cns_state_type_t)ik_fight_state_type(fight, victim)) {
        case IK_CNS_STATE_LIEDOWN:
            return (flags & IK_CNS_HIT_DOWN) != 0u;
        case IK_CNS_STATE_AIR:
            return victim->gethit_fall
                ? (flags & IK_CNS_HIT_FALL) != 0u
                : (flags & IK_CNS_HIT_AIR) != 0u;
        case IK_CNS_STATE_CROUCH:
            return (flags & IK_CNS_HIT_CROUCH) != 0u;
        case IK_CNS_STATE_STAND:
        case IK_CNS_STATE_UNCHANGED:
        default:
            return (flags & IK_CNS_HIT_STAND) != 0u;
    }
}

int ikf_hitdef_chain_allows_target(
    const ik_fighter_t* victim,
    uint8_t attacker_owner,
    const ik_cns_hitdef_t* hitdef
) {
    if (!victim || !hitdef) return 0;
    const int same_attacker =
        victim->last_hit_owner == (int8_t)attacker_owner;

    /* Legacy hand-authored C HitDefs are commonly zero-initialized. HitDef
     * IDs may legitimately be zero, but ChainID/NoChainID authored selectors
     * are positive in the compatibility surface. Treat zero like "unset" so
     * older aggregate initializers do not accidentally require chain 0. */
    if (hitdef->chain_id > 0) {
        if (!same_attacker || victim->last_hit_id != hitdef->chain_id) {
            return 0;
        }
    }
    if (same_attacker &&
        ((hitdef->no_chain_id > 0 &&
          victim->last_hit_id == hitdef->no_chain_id) ||
         (hitdef->no_chain_id2 > 0 &&
          victim->last_hit_id == hitdef->no_chain_id2))) {
        return 0;
    }
    return 1;
}

int ikf_juggle_cost(const ik_fight_t* fight,
                       const ik_fighter_t* attacker,
                       const ik_cns_hitdef_t* hitdef) {
    int cost = hitdef ? hitdef->air_juggle : 0;
    const ik_cns_state_t* state =
        attacker ? fighter_state_spec(fight, attacker) : 0;
    if (state && state->has_juggle && state->juggle > 0) {
        cost += state->juggle;
    }
    return cost < 0 ? 0 : cost;
}

int ikf_is_juggle_target(const ik_fight_t* fight,
                            const ik_fighter_t* victim) {
    if (!victim) return 0;
    return victim->gethit_fall ||
           ik_fight_state_type(fight, victim) == IK_CNS_STATE_LIEDOWN;
}

int ikf_juggle_allows_target(const ik_fight_t* fight,
                                const ik_fighter_t* attacker,
                                const ik_fighter_t* victim,
                                const ik_cns_hitdef_t* hitdef) {
    if (!ikf_is_juggle_target(fight, victim)) return 1;
    return ikf_juggle_cost(fight, attacker, hitdef) <= victim->juggle_points;
}

int ikf_entity_juggle_cost(
    const ik_fight_t* fight,
    const ik_entity_t* attacker,
    const ik_cns_hitdef_t* hitdef
) {
    int cost = hitdef ? hitdef->air_juggle : 0;
    const ik_cns_state_t* state =
        attacker ? entity_state_spec(fight, attacker) : 0;
    if (state && state->has_juggle && state->juggle > 0) {
        cost += state->juggle;
    }
    return cost < 0 ? 0 : cost;
}

int ikf_entity_juggle_allows_target(
    const ik_fight_t* fight,
    const ik_entity_t* attacker,
    const ik_fighter_t* victim,
    const ik_cns_hitdef_t* hitdef
) {
    if (!ikf_is_juggle_target(fight, victim)) return 1;
    return ikf_entity_juggle_cost(fight, attacker, hitdef) <=
           victim->juggle_points;
}

/* Upstream's inGuardState(): every state of the guard family. */
int ikf_in_guard_state(int16_t state) {
    return state == 120 || (state >= 130 && state <= 132) || state == 140 ||
           (state >= 150 && state <= 155);
}

int ikf_is_active_guard_state(int16_t state) {
    return state == 120 || state == 130 || state == 131 || state == 132;
}

uint8_t ikf_guard_type_for(const ik_fight_t* fight,
                              const ik_fighter_t* f,
                              const ik_fight_controls_t* controls) {
    if (!f || !f->on_ground) return IK_CNS_STATE_AIR;
    if ((controls && controls->down) ||
        ik_fight_state_type(fight, f) == IK_CNS_STATE_CROUCH) {
        return IK_CNS_STATE_CROUCH;
    }
    return IK_CNS_STATE_STAND;
}

static uint8_t guard_mask_for_type(uint8_t state_type) {
    if (state_type == IK_CNS_STATE_AIR) return IK_CNS_GUARD_AIR;
    if (state_type == IK_CNS_STATE_CROUCH) return IK_CNS_GUARD_CROUCH;
    return IK_CNS_GUARD_STAND;
}

/* HitDef guard.dist when the attacker's current HitDef set one, otherwise the
 * character's size.attack.dist. */
static int attacker_guard_dist(
    const ik_fight_t* fight,
    const ik_cns_hitdef_t* hitdef,
    uint8_t attacker_owner
) {
    if (hitdef && hitdef->guard_dist >= 0) return hitdef->guard_dist;
    const ik_cns_asset_t* cns = cns_for_owner(fight, attacker_owner);
    return cns ? cns->constants.attack_dist : 0;
}

/* Upstream inGuardDist: the foe is in front of an attacker (MoveType A) and
 * closer than its guard distance. */
static int attacker_reaches(
    const ik_fight_t* fight,
    const ik_cns_hitdef_t* hitdef,
    uint8_t attacker_owner,
    int32_t attacker_x_q8,
    int8_t attacker_facing,
    const ik_fighter_t* victim
) {
    const int32_t dist_q8 =
        (victim->x_q8 - attacker_x_q8) * (int32_t)attacker_facing;
    const int32_t front_q8 =
        (int32_t)attacker_guard_dist(fight, hitdef, attacker_owner) *
        IK_CNS_Q8_ONE;
    return dist_q8 > 0 && dist_q8 < front_q8;
}

/* Recomputed after hit detection every tick and read by the next tick's
 * guard decisions, as upstream does. */
void ikf_update_guard_dist(ik_fight_t* fight) {
    fight->fighters[0].in_guard_dist = 0u;
    fight->fighters[1].in_guard_dist = 0u;
    for (int i = 0; i < 2; ++i) {
        const ik_fighter_t* attacker = &fight->fighters[i];
        ik_fighter_t* victim = &fight->fighters[i ^ 1];
        if (attacker->cur_move_type != IK_CNS_MOVE_ATTACK) continue;
        const ik_cns_asset_t* state_cns = cns_for_fighter(fight, attacker);
        const ik_cns_hitdef_t* hitdef =
            state_cns && state_cns->hitdefs &&
            attacker->active_hitdef_global >= 0 &&
            attacker->active_hitdef_global < (int16_t)state_cns->hitdef_count
                ? &state_cns->hitdefs[(uint16_t)attacker->active_hitdef_global]
                : 0;
        if (attacker_reaches(
                fight, hitdef, attacker->owner_player, attacker->x_q8,
                attacker->facing, victim)) {
            victim->in_guard_dist = 1u;
        }
    }
    if (!fight->entities) return;
    for (uint8_t slot = 0u; slot < IK_ENTITY_CAPACITY; ++slot) {
        ik_entity_t* entity = &fight->entities->entities[slot];
        if ((entity->type != IK_ENTITY_HELPER &&
             entity->type != IK_ENTITY_PROJECTILE) ||
            entity->owner_player > 1u) {
            continue;
        }
        ik_fighter_t* victim = &fight->fighters[entity->owner_player ^ 1u];
        const ik_cns_hitdef_t* hitdef = ikf_active_entity_hitdef(
            fight, fight->player_frames[entity->anim_owner < 2u
                                            ? entity->anim_owner : 0],
            entity, victim, 0);
        if (hitdef && attacker_reaches(
                fight, hitdef, entity->owner_player, entity->x_q8,
                entity->facing, victim)) {
            victim->in_guard_dist = 1u;
        }
    }
}

int ikf_can_guard_hit(const ik_fight_t* fight,
                         const ik_fighter_t* victim,
                         const ik_fight_controls_t* controls,
                         const ik_cns_hitdef_t* hitdef) {
    if (!fight || !victim || !hitdef || hitdef->guard_flags == 0u) return 0;
    if ((!controls || !controls->back) &&
        !ikf_is_active_guard_state(victim->state)) {
        return 0;
    }
    const uint8_t type = ikf_guard_type_for(fight, victim, controls);
    return (hitdef->guard_flags & guard_mask_for_type(type)) != 0u;
}

/* Power a connecting HitDef earns: its defaults are 70% of the damage for the
 * attacker (nothing for hypers) and 60% for the victim, halved when guarded.
 * It lands on the next tick, like upstream's mhv/ghv power. */
void ikf_hit_snap(const ik_fighter_t* attacker, ik_fighter_t* victim,
                  const ik_cns_hitdef_t* hitdef, int guarded) {
    victim->snap_flags = 0u;
    if (hitdef->dist_flags == 0u) return;
    const int32_t by_x = attacker->x_q8;
    const int32_t by_y = attacker->y_q8;
    const int32_t vx = victim->x_q8;
    const int32_t vy = victim->y_q8;
    const int left = attacker->facing < 0;
    int have_x = 0, have_y = 0;
    int32_t snap_x = 0, snap_y = 0;

    if (hitdef->dist_flags & 1u) {
        const int32_t limit = left ? by_x - hitdef->mindist_x_q8
                                   : by_x + hitdef->mindist_x_q8;
        if (left ? vx > limit : vx < limit) { snap_x = limit; have_x = 1; }
    }
    if (hitdef->dist_flags & 4u) {
        const int32_t limit = left ? by_x - hitdef->maxdist_x_q8
                                   : by_x + hitdef->maxdist_x_q8;
        if (left ? vx < limit : vx > limit) { snap_x = limit; have_x = 1; }
    }
    /* Vertical limits only apply to a hit, or to a guarded airborne victim. */
    if (!guarded || victim->cur_state_type == IK_CNS_STATE_AIR) {
        if ((hitdef->dist_flags & 2u) &&
            vy < by_y + hitdef->mindist_y_q8) {
            snap_y = by_y + hitdef->mindist_y_q8;
            have_y = 1;
        }
        if ((hitdef->dist_flags & 8u) &&
            vy > by_y + hitdef->maxdist_y_q8) {
            snap_y = by_y + hitdef->maxdist_y_q8;
            have_y = 1;
        }
    }
    if (have_x) {
        victim->snap_x_q8 = snap_x - vx;
        victim->snap_flags |= 1u;
    }
    if (have_y) {
        victim->snap_y_q8 = snap_y - vy;
        victim->snap_flags |= 2u;
    }
}

void ikf_credit_hit_power(
    ik_fighter_t* attacker,
    ik_fighter_t* victim,
    const ik_cns_hitdef_t* hitdef,
    int guarded
) {
    int get = ((int)hitdef->damage * 7) / 10;
    int give = ((int)hitdef->damage * 6) / 10;
    if ((hitdef->attack_attr_mask &
         (IK_CNS_ATTR_HYPER_ATTACK | IK_CNS_ATTR_HYPER_PROJECTILE |
          IK_CNS_ATTR_HYPER_THROW)) != 0u) {
        get = 0;
    }
    if (guarded) {
        get /= 2;
        give /= 2;
    }
    if (hitdef->power_flags & 1u) {
        get = guarded ? hitdef->get_power_guard : hitdef->get_power_hit;
    }
    if (hitdef->power_flags & 2u) {
        give = guarded ? hitdef->give_power_guard : hitdef->give_power_hit;
    }
    victim->reversed = 0u;
    attacker->move_contact_type = guarded ? 1u : 0u;
    attacker->pending_power = (int16_t)(attacker->pending_power + get);
    victim->pending_power = (int16_t)(victim->pending_power + give);
}
