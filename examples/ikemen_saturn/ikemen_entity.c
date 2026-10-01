#include "ikemen_entity.h"

#include <stddef.h>

void *memset(void *dest, int c, size_t n);

static ik_entity_handle_t handle_for(
    const ik_entity_pool_t* pool,
    uint8_t slot
) {
    ik_entity_handle_t handle = {IK_ENTITY_INVALID_SLOT, 0u};
    if (!pool || slot >= IK_ENTITY_CAPACITY ||
        pool->entities[slot].type == IK_ENTITY_NONE) {
        return handle;
    }
    handle.slot = slot;
    handle.generation = pool->generations[slot];
    return handle;
}

ik_entity_handle_t ik_entity_invalid_handle(void) {
    ik_entity_handle_t handle = {IK_ENTITY_INVALID_SLOT, 0u};
    return handle;
}

int ik_entity_handle_is_valid(ik_entity_handle_t handle) {
    return handle.slot != IK_ENTITY_INVALID_SLOT &&
           handle.slot < IK_ENTITY_CAPACITY &&
           handle.generation != 0u;
}

int ik_entity_handle_equal(ik_entity_handle_t a, ik_entity_handle_t b) {
    return a.slot == b.slot && a.generation == b.generation;
}

void ik_entity_pool_init(ik_entity_pool_t* pool) {
    if (!pool) return;
    memset(pool, 0, sizeof(*pool));
    for (uint8_t i = 0u; i < IK_ENTITY_CAPACITY; ++i) {
        pool->generations[i] = 1u;
    }
    pool->players[0] = ik_entity_invalid_handle();
    pool->players[1] = ik_entity_invalid_handle();
}

ik_entity_t* ik_entity_get(
    ik_entity_pool_t* pool,
    ik_entity_handle_t handle
) {
    if (!pool || !ik_entity_handle_is_valid(handle)) return 0;
    if (pool->generations[handle.slot] != handle.generation) return 0;
    if (pool->entities[handle.slot].type == IK_ENTITY_NONE) return 0;
    return &pool->entities[handle.slot];
}

const ik_entity_t* ik_entity_get_const(
    const ik_entity_pool_t* pool,
    ik_entity_handle_t handle
) {
    if (!pool || !ik_entity_handle_is_valid(handle)) return 0;
    if (pool->generations[handle.slot] != handle.generation) return 0;
    if (pool->entities[handle.slot].type == IK_ENTITY_NONE) return 0;
    return &pool->entities[handle.slot];
}

int ik_entity_spawn(
    ik_entity_pool_t* pool,
    uint8_t type,
    int32_t id,
    uint8_t owner_player,
    ik_entity_handle_t parent,
    ik_entity_handle_t* out_handle
) {
    if (!pool || !out_handle ||
        type < IK_ENTITY_PLAYER || type > IK_ENTITY_EXPLOD) {
        return 0;
    }
    if (owner_player > 1u) return 0;
    if (type == IK_ENTITY_PLAYER &&
        ik_entity_get_const(pool, pool->players[owner_player])) {
        return 0;
    }

    uint8_t slot = IK_ENTITY_INVALID_SLOT;
    for (uint8_t i = 0u; i < IK_ENTITY_CAPACITY; ++i) {
        if (pool->entities[i].type == IK_ENTITY_NONE) {
            slot = i;
            break;
        }
    }
    if (slot == IK_ENTITY_INVALID_SLOT) return 0;

    ik_entity_t* entity = &pool->entities[slot];
    memset(entity, 0, sizeof(*entity));
    entity->type = type;
    entity->owner_player = owner_player;
    entity->state_owner = owner_player;
    entity->id = id;
    entity->parent = ik_entity_invalid_handle();
    entity->root = ik_entity_invalid_handle();
    entity->target = ik_entity_invalid_handle();
    for (uint8_t target_i = 0u;
         target_i < IK_ENTITY_TARGET_CAPACITY; ++target_i) {
        entity->targets[target_i] = ik_entity_invalid_handle();
        entity->target_ids[target_i] = -1;
    }
    entity->target_count = 0u;
    entity->facing = 1;
    entity->active_hitdef_global = -1;
    entity->active_hitdef_local = -1;
    entity->proj_query_contact_time = -1;
    entity->proj_query_hit_time = -1;
    entity->proj_query_guarded_time = -1;

    const ik_entity_handle_t self = handle_for(pool, slot);

    if (type == IK_ENTITY_PLAYER) {
        entity->root = self;
        pool->players[owner_player] = self;
    } else {
        const ik_entity_t* parent_entity =
            ik_entity_get_const(pool, parent);
        if (parent_entity) {
            entity->parent = parent;
            entity->root = ik_entity_get_const(pool, parent_entity->root)
                ? parent_entity->root
                : parent;
        } else {
            entity->root = self;
        }
    }

    *out_handle = self;
    return 1;
}

int ik_entity_destroy(
    ik_entity_pool_t* pool,
    ik_entity_handle_t handle
) {
    ik_entity_t* entity = ik_entity_get(pool, handle);
    if (!entity) return 0;

    if (entity->type == IK_ENTITY_PLAYER && entity->owner_player < 2u &&
        ik_entity_handle_equal(
            pool->players[entity->owner_player], handle)) {
        pool->players[entity->owner_player] = ik_entity_invalid_handle();
    }

    memset(entity, 0, sizeof(*entity));
    ++pool->generations[handle.slot];
    if (pool->generations[handle.slot] == 0u) {
        pool->generations[handle.slot] = 1u;
    }
    return 1;
}

void ik_entity_clear_targets(
    ik_entity_pool_t* pool,
    ik_entity_handle_t source
) {
    ik_entity_t* entity = ik_entity_get(pool, source);
    if (!entity) return;
    entity->target = ik_entity_invalid_handle();
    entity->target_count = 0u;
    for (uint8_t i = 0u; i < IK_ENTITY_TARGET_CAPACITY; ++i) {
        entity->targets[i] = ik_entity_invalid_handle();
        entity->target_ids[i] = -1;
    }
}

static void compact_targets(
    const ik_entity_pool_t* pool,
    ik_entity_t* entity
) {
    if (!pool || !entity) return;
    uint8_t write = 0u;
    for (uint8_t read = 0u; read < entity->target_count; ++read) {
        if (!ik_entity_get_const(pool, entity->targets[read])) continue;
        if (write != read) {
            entity->targets[write] = entity->targets[read];
            entity->target_ids[write] = entity->target_ids[read];
        }
        ++write;
    }
    for (uint8_t i = write; i < IK_ENTITY_TARGET_CAPACITY; ++i) {
        entity->targets[i] = ik_entity_invalid_handle();
        entity->target_ids[i] = -1;
    }
    entity->target_count = write;
    entity->target = write > 0u
        ? entity->targets[0] : ik_entity_invalid_handle();
}

int ik_entity_add_target(
    ik_entity_pool_t* pool,
    ik_entity_handle_t source,
    ik_entity_handle_t target,
    int32_t target_id
) {
    ik_entity_t* entity = ik_entity_get(pool, source);
    if (!entity || !ik_entity_get_const(pool, target)) return 0;
    compact_targets(pool, entity);

    for (uint8_t i = 0u; i < entity->target_count; ++i) {
        if (ik_entity_handle_equal(entity->targets[i], target)) {
            entity->target_ids[i] = target_id;
            entity->target = entity->targets[0];
            return 1;
        }
    }
    if (entity->target_count >= IK_ENTITY_TARGET_CAPACITY) return 0;

    const uint8_t slot = entity->target_count++;
    entity->targets[slot] = target;
    entity->target_ids[slot] = target_id;
    entity->target = entity->targets[0];
    return 1;
}

int ik_entity_remove_target(
    ik_entity_pool_t* pool,
    ik_entity_handle_t source,
    ik_entity_handle_t target
) {
    ik_entity_t* entity = ik_entity_get(pool, source);
    if (!entity) return 0;
    for (uint8_t i = 0u; i < entity->target_count; ++i) {
        if (!ik_entity_handle_equal(entity->targets[i], target)) continue;
        for (uint8_t j = i + 1u; j < entity->target_count; ++j) {
            entity->targets[j - 1u] = entity->targets[j];
            entity->target_ids[j - 1u] = entity->target_ids[j];
        }
        --entity->target_count;
        entity->targets[entity->target_count] = ik_entity_invalid_handle();
        entity->target_ids[entity->target_count] = -1;
        entity->target = entity->target_count > 0u
            ? entity->targets[0] : ik_entity_invalid_handle();
        return 1;
    }
    return 0;
}

int ik_entity_set_target(
    ik_entity_pool_t* pool,
    ik_entity_handle_t source,
    ik_entity_handle_t target
) {
    ik_entity_t* entity = ik_entity_get(pool, source);
    if (!entity) return 0;
    ik_entity_clear_targets(pool, source);
    if (!ik_entity_handle_is_valid(target)) return 1;
    return ik_entity_add_target(pool, source, target, -1);
}

ik_entity_handle_t ik_entity_target_at(
    const ik_entity_pool_t* pool,
    ik_entity_handle_t source,
    int32_t target_id,
    uint8_t index
) {
    const ik_entity_t* entity = ik_entity_get_const(pool, source);
    if (!entity) return ik_entity_invalid_handle();
    uint8_t matched = 0u;
    for (uint8_t i = 0u; i < entity->target_count; ++i) {
        if (!ik_entity_get_const(pool, entity->targets[i])) continue;
        if (target_id >= 0 && entity->target_ids[i] != target_id) continue;
        if (matched++ == index) return entity->targets[i];
    }
    return ik_entity_invalid_handle();
}

uint8_t ik_entity_target_count(
    const ik_entity_pool_t* pool,
    ik_entity_handle_t source,
    int32_t target_id
) {
    const ik_entity_t* entity = ik_entity_get_const(pool, source);
    if (!entity) return 0u;
    uint8_t count = 0u;
    for (uint8_t i = 0u; i < entity->target_count; ++i) {
        if (!ik_entity_get_const(pool, entity->targets[i])) continue;
        if (target_id >= 0 && entity->target_ids[i] != target_id) continue;
        ++count;
    }
    return count;
}

void ik_entity_drop_targets(
    ik_entity_pool_t* pool,
    ik_entity_handle_t source,
    int32_t exclude_id,
    uint8_t keep_one
) {
    ik_entity_t* entity = ik_entity_get(pool, source);
    if (!entity) return;
    compact_targets(pool, entity);

    ik_entity_handle_t kept[IK_ENTITY_TARGET_CAPACITY];
    int32_t kept_ids[IK_ENTITY_TARGET_CAPACITY];
    uint8_t kept_count = 0u;
    uint8_t kept_excluded = 0u;

    for (uint8_t i = 0u; i < entity->target_count; ++i) {
        const int excluded =
            exclude_id >= 0 && entity->target_ids[i] == exclude_id;
        if (!excluded) continue;
        if (keep_one && kept_excluded) continue;
        kept[kept_count] = entity->targets[i];
        kept_ids[kept_count] = entity->target_ids[i];
        ++kept_count;
        kept_excluded = 1u;
    }

    entity->target_count = kept_count;
    for (uint8_t i = 0u; i < IK_ENTITY_TARGET_CAPACITY; ++i) {
        if (i < kept_count) {
            entity->targets[i] = kept[i];
            entity->target_ids[i] = kept_ids[i];
        } else {
            entity->targets[i] = ik_entity_invalid_handle();
            entity->target_ids[i] = -1;
        }
    }
    entity->target = kept_count > 0u
        ? entity->targets[0] : ik_entity_invalid_handle();
}

ik_entity_handle_t ik_entity_redirect_target(
    const ik_entity_pool_t* pool,
    ik_entity_handle_t self,
    int32_t target_id,
    uint8_t index
) {
    return ik_entity_target_at(pool, self, target_id, index);
}

ik_entity_handle_t ik_entity_redirect(
    const ik_entity_pool_t* pool,
    ik_entity_handle_t self,
    uint8_t redirect
) {
    const ik_entity_t* entity = ik_entity_get_const(pool, self);
    if (!entity) return ik_entity_invalid_handle();

    switch ((ik_expr_redirect_t)redirect) {
        case IK_EXPR_REDIRECT_SELF:
            return self;

        case IK_EXPR_REDIRECT_P2:
            if (entity->owner_player < 2u) {
                const ik_entity_handle_t other =
                    pool->players[entity->owner_player ^ 1u];
                if (ik_entity_get_const(pool, other)) return other;
            }
            break;

        case IK_EXPR_REDIRECT_PARENT:
            if (ik_entity_get_const(pool, entity->parent)) {
                return entity->parent;
            }
            break;

        case IK_EXPR_REDIRECT_ROOT:
            if (ik_entity_get_const(pool, entity->root)) {
                return entity->root;
            }
            break;

        case IK_EXPR_REDIRECT_TARGET:
            if (ik_entity_get_const(pool, entity->target)) {
                return entity->target;
            }
            break;

        default:
            break;
    }

    return ik_entity_invalid_handle();
}

uint8_t ik_entity_count_type(
    const ik_entity_pool_t* pool,
    uint8_t type
) {
    if (!pool) return 0u;
    uint8_t count = 0u;
    for (uint8_t i = 0u; i < IK_ENTITY_CAPACITY; ++i) {
        if (pool->entities[i].type == type) ++count;
    }
    return count;
}

static int32_t q8_to_int(int32_t value) {
    if (value >= 0) return value / IK_ENTITY_Q8_ONE;
    return -((-value) / IK_ENTITY_Q8_ONE);
}

int ik_entity_expr_read_field(
    void* user,
    uint8_t redirect,
    uint8_t field,
    int16_t index,
    int32_t* out_value
) {
    if (!user || !out_value) return 0;
    const ik_entity_expr_binding_t* binding =
        (const ik_entity_expr_binding_t*)user;
    const ik_entity_t* self =
        ik_entity_get_const(binding->pool, binding->self);
    const ik_entity_handle_t redirected =
        ik_entity_redirect(binding->pool, binding->self, redirect);
    const ik_entity_t* entity =
        ik_entity_get_const(binding->pool, redirected);
    if (!self || !entity) return 0;

    switch ((ik_expr_field_t)field) {
        case IK_EXPR_FIELD_STATE_NO:
            *out_value = entity->state_no;
            return 1;
        case IK_EXPR_FIELD_STATE_TIME:
            *out_value = entity->state_time;
            return 1;
        case IK_EXPR_FIELD_STATE_TYPE:
            *out_value = entity->state_type;
            return 1;
        case IK_EXPR_FIELD_MOVE_TYPE:
            *out_value = entity->move_type;
            return 1;
        case IK_EXPR_FIELD_CTRL:
            *out_value = entity->ctrl != 0u;
            return 1;
        case IK_EXPR_FIELD_MOVE_CONTACT:
            *out_value = entity->move_contact != 0u;
            return 1;
        case IK_EXPR_FIELD_POWER:
            *out_value = entity->power;
            return 1;
        case IK_EXPR_FIELD_ACTIVE_HIT_ATTR:
            *out_value = entity->active_hit_attr_mask;
            return 1;
        case IK_EXPR_FIELD_LIFE:
            *out_value = entity->life;
            return 1;
        case IK_EXPR_FIELD_BODY_DIST_X:
            *out_value =
                (q8_to_int(entity->x_q8) - q8_to_int(self->x_q8)) *
                    self->facing -
                self->push_front - entity->push_front;
            return 1;
        case IK_EXPR_FIELD_POS_X_Q8:
            *out_value = entity->x_q8;
            return 1;
        case IK_EXPR_FIELD_POS_Y_Q8:
            *out_value = entity->y_q8;
            return 1;
        case IK_EXPR_FIELD_VEL_X_Q8:
            *out_value = entity->vx_q8;
            return 1;
        case IK_EXPR_FIELD_VEL_Y_Q8:
            *out_value = entity->vy_q8;
            return 1;
        case IK_EXPR_FIELD_FACING:
            *out_value = entity->facing;
            return 1;
        case IK_EXPR_FIELD_ENTITY_ID:
            *out_value = entity->id;
            return 1;
        case IK_EXPR_FIELD_ENTITY_TYPE:
            *out_value = entity->type;
            return 1;
        case IK_EXPR_FIELD_VAR:
            if (index < 0 || index >= (int16_t)IK_ENTITY_VAR_COUNT) return 0;
            *out_value = entity->vars[index];
            return 1;
        case IK_EXPR_FIELD_FVAR_Q16:
            if (index < 0 || index >= (int16_t)IK_ENTITY_FVAR_COUNT) return 0;
            *out_value = entity->fvars_q16[index];
            return 1;
        case IK_EXPR_FIELD_NUM_PROJECTILES: {
            uint8_t count = 0u;
            for (uint8_t slot = 0u; slot < IK_ENTITY_CAPACITY; ++slot) {
                const ik_entity_t* candidate = &binding->pool->entities[slot];
                if (candidate->type == IK_ENTITY_PROJECTILE &&
                    candidate->owner_player == entity->owner_player) {
                    ++count;
                }
            }
            *out_value = count;
            return 1;
        }
        case IK_EXPR_FIELD_PROJ_CONTACT:
            *out_value = entity->proj_query_contact;
            return 1;
        case IK_EXPR_FIELD_PROJ_HIT:
            *out_value = entity->proj_query_hit;
            return 1;
        case IK_EXPR_FIELD_PROJ_GUARDED:
            *out_value = entity->proj_query_guarded;
            return 1;
        case IK_EXPR_FIELD_PROJ_CONTACT_TIME:
            *out_value = entity->proj_query_contact_time;
            return 1;
        case IK_EXPR_FIELD_PROJ_HIT_TIME:
            *out_value = entity->proj_query_hit_time;
            return 1;
        case IK_EXPR_FIELD_PROJ_GUARDED_TIME:
            *out_value = entity->proj_query_guarded_time;
            return 1;

        case IK_EXPR_FIELD_SYSVAR:
            if (index < 0 || index >= (int16_t)IK_ENTITY_SYSVAR_COUNT) return 0;
            *out_value = entity->sysvars[index];
            return 1;
        default:
            return 0;
    }
}
