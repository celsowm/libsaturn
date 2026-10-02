/* Fighter <-> entity pool mirroring and the entity runtime bridge. */
#include "ikemen_fight_internal.h"

uint8_t ikf_fighter_player_index(
    const ik_fight_t* fight,
    const ik_fighter_t* fighter
) {
    if (!fight || !fighter) return 0xFFu;
    if (fighter == &fight->fighters[0]) return 0u;
    if (fighter == &fight->fighters[1]) return 1u;
    return 0xFFu;
}

ik_entity_handle_t ikf_fighter_entity_handle(
    const ik_fight_t* fight,
    const ik_fighter_t* fighter
) {
    const uint8_t player = ikf_fighter_player_index(fight, fighter);
    if (!fight || player >= 2u) return ik_entity_invalid_handle();
    return fight->player_entities[player];
}

ik_entity_t* ikf_fighter_entity(
    ik_fight_t* fight,
    const ik_fighter_t* fighter
) {
    if (!fight || !fight->entities) return 0;
    return ik_entity_get(
        fight->entities, ikf_fighter_entity_handle(fight, fighter));
}

void ikf_sync_fighter_entity(
    ik_fight_t* fight,
    const ik_fighter_t* fighter
) {
    if (!fight || !fight->entities || !fighter) return;
    const uint8_t player = ikf_fighter_player_index(fight, fighter);
    if (player >= 2u) return;

    ik_entity_t* entity = ikf_fighter_entity(fight, fighter);
    if (!entity) return;

    entity->x_q8 = fighter->x_q8;
    entity->y_q8 = fighter->y_q8;
    entity->vx_q8 = fighter->vx_q8;
    entity->vy_q8 = fighter->vy_q8;
    entity->state_owner = fighter->state_owner;
    entity->anim_owner = fighter->anim_owner;
    entity->state_no = fighter->state;
    entity->prev_state_no = fighter->prev_state;
    entity->state_time = fighter->state_time;
    entity->anim_no = fighter->anim;
    entity->anim_time = fighter->anim_time;
    entity->life = fighter->hp;
    entity->power = fighter->power;
    entity->push_back = fighter->push_back;
    entity->push_front = fighter->push_front;
    entity->facing = fighter->facing;
    entity->spr_priority = fighter->spr_priority;
    entity->ctrl = (uint8_t)(fighter->ctrl != 0);
    entity->state_type = ik_fight_state_type(fight, fighter);
    entity->move_type = fighter->cur_move_type;
    entity->move_contact = fighter->move_contact;
    entity->move_hit = fighter->move_hit;

    const ik_entity_handle_t target =
        fighter->target_index >= 0 && fighter->target_index < 2
            ? fight->player_entities[(uint8_t)fighter->target_index]
            : ik_entity_invalid_handle();
    (void)ik_entity_set_target(
        fight->entities, fight->player_entities[player], target);
}

void ikf_sync_player_entities(ik_fight_t* fight) {
    if (!fight) return;
    ikf_sync_fighter_entity(fight, &fight->fighters[0]);
    ikf_sync_fighter_entity(fight, &fight->fighters[1]);
}

static int fighter_index_from_entity_handle(
    const ik_fight_t* fight,
    ik_entity_handle_t handle
) {
    if (!fight) return -1;
    for (int i = 0; i < 2; ++i) {
        if (ik_entity_handle_equal(
                fight->player_entities[i], handle)) {
            return i;
        }
    }
    return -1;
}

static int apply_entity_target_controller_one(
    ik_fight_t* fight,
    ik_entity_runtime_t* runtime,
    ik_entity_handle_t source_handle,
    ik_entity_handle_t target_handle,
    const ik_cns_controller_t* ctrl
) {
    ik_entity_t* source =
        ik_entity_get(runtime->pool, source_handle);
    ik_entity_t* target_entity =
        ik_entity_get(runtime->pool, target_handle);
    if (!source || !target_entity) return 0;

    const int target_index =
        fighter_index_from_entity_handle(fight, target_handle);
    ik_fighter_t* target_fighter =
        target_index >= 0 && target_index < 2
            ? &fight->fighters[target_index] : 0;

    switch ((ik_cns_controller_type_t)ctrl->type) {
        case IK_CNS_CTRL_TARGET_BIND:
            if (target_fighter) {
                target_fighter->bound_to = -1;
                target_fighter->bound_entity = source_handle;
                target_fighter->x_q8 =
                    source->x_q8 +
                    (int32_t)source->facing * ctrl->value0;
                target_fighter->y_q8 =
                    source->y_q8 + ctrl->value1;
                target_fighter->vx_q8 = 0;
                target_fighter->vy_q8 = 0;
                sync_position(target_fighter);
                ikf_sync_fighter_entity(fight, target_fighter);
            } else {
                target_entity->x_q8 =
                    source->x_q8 +
                    (int32_t)source->facing * ctrl->value0;
                target_entity->y_q8 =
                    source->y_q8 + ctrl->value1;
                target_entity->vx_q8 = 0;
                target_entity->vy_q8 = 0;
            }
            break;

        case IK_CNS_CTRL_TARGET_FACING:
            if (target_fighter) {
                target_fighter->facing = (int8_t)(
                    source->facing * (ctrl->value0 < 0 ? -1 : 1));
                ikf_sync_fighter_entity(fight, target_fighter);
            } else {
                target_entity->facing = (int8_t)(
                    source->facing * (ctrl->value0 < 0 ? -1 : 1));
            }
            break;

        case IK_CNS_CTRL_TARGET_LIFE_ADD:
            if (target_fighter) {
                int hp = (int)target_fighter->hp + ctrl->value0;
                const int max_hp = ik_fight_max_hp_player(
                    fight, target_fighter->owner_player);
                if (hp > max_hp) hp = max_hp;
                if (hp <= 0) {
                    hp = 0;
                    fight->winner =
                        (uint8_t)((target_index ^ 1) + 1);
                    fight->events |= IK_EVENT_KO;
                }
                target_fighter->hp = (int16_t)hp;
                ikf_sync_fighter_entity(fight, target_fighter);
            } else {
                int life = (int)target_entity->life + ctrl->value0;
                if (life < 0) life = 0;
                if (life > 32767) life = 32767;
                target_entity->life = (int16_t)life;
            }
            break;

        case IK_CNS_CTRL_TARGET_STATE:
            if (target_fighter) {
                target_fighter->bound_entity =
                    ik_entity_invalid_handle();
                target_fighter->bound_to = -1;
                target_fighter->state_owner = source->state_owner;
                ikf_enter_state(
                    fight, target_fighter,
                    (int16_t)ctrl->value0);
                ikf_sync_fighter_entity(fight, target_fighter);
            } else {
                target_entity->state_owner = source->state_owner;
                (void)ik_entity_runtime_enter_state(
                    runtime, target_handle,
                    (int16_t)ctrl->value0);
            }
            (void)ik_entity_remove_target(
                runtime->pool, source_handle, target_handle);
            break;

        default:
            break;
    }
    return 0;
}

static int entity_target_controller_bridge(
    void* user,
    ik_entity_runtime_t* runtime,
    ik_entity_handle_t source_handle,
    const ik_cns_controller_t* ctrl
) {
    ik_fight_t* fight = (ik_fight_t*)user;
    if (!fight || !runtime || !runtime->pool || !ctrl) return 0;

    const ik_entity_t* source =
        ik_entity_get_const(runtime->pool, source_handle);
    if (!source) return -1;

    ik_entity_handle_t selected[IK_ENTITY_TARGET_CAPACITY];
    uint8_t selected_count = 0u;
    uint8_t matched = 0u;
    const int32_t wanted_id = ctrl->value2;
    const int32_t wanted_index = ctrl->value3;
    for (uint8_t i = 0u; i < source->target_count &&
                        selected_count < IK_ENTITY_TARGET_CAPACITY; ++i) {
        if (wanted_id >= 0 && source->target_ids[i] != wanted_id) {
            continue;
        }
        if (!ik_entity_get_const(runtime->pool, source->targets[i])) {
            continue;
        }
        if (wanted_index >= 0 && matched++ != (uint8_t)wanted_index) {
            continue;
        }
        selected[selected_count++] = source->targets[i];
        if (wanted_index >= 0) break;
    }

    for (uint8_t i = 0u; i < selected_count; ++i) {
        (void)apply_entity_target_controller_one(
            fight, runtime, source_handle, selected[i], ctrl);
    }
    return 0;
}

void ikf_configure_fight_entity_runtime(
    ik_fight_t* fight,
    ik_entity_runtime_t* runtime
) {
    if (!fight || !runtime) return;
    ik_entity_runtime_set_player_cns(
        runtime, 0u, cns_for_owner(fight, 0u));
    ik_entity_runtime_set_player_cns(
        runtime, 1u, cns_for_owner(fight, 1u));
    ik_entity_runtime_set_target_controller(
        runtime, fight, entity_target_controller_bridge);
    ik_entity_runtime_set_stage_bounds(
        runtime, (int16_t)(fight->xmin_q8 / IK_CNS_Q8_ONE),
        (int16_t)(fight->xmax_q8 / IK_CNS_Q8_ONE));
}
