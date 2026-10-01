#include "ikemen_entity_runtime.h"

static const ik_frame_table_t* frames_for(
    const ik_entity_runtime_t* runtime,
    const ik_entity_t* entity
) {
    if (!runtime || !entity || entity->owner_player >= 2u) return 0;
    return runtime->frames[entity->owner_player];
}

static uint16_t anim_element_start_tick(
    const ik_frame_table_t* frames,
    int16_t action,
    uint16_t element
) {
    uint32_t first = 0u;
    uint32_t count = 0u;
    uint32_t total = 0u;
    if (!frames || element < 1u ||
        !ik_frames_bounds(frames, action, &first, &count) ||
        element > count) {
        return 0u;
    }
    for (uint16_t i = 1u; i < element; ++i) {
        const uint16_t ticks =
            ik_frame_ticks(&frames->frames[first + (uint32_t)(i - 1u)]);
        if (ticks == 0u) break;
        total += ticks;
    }
    return (uint16_t)(total > 65535u ? 65535u : total);
}

static void anim_position(
    const ik_frame_table_t* frames,
    const ik_entity_t* entity,
    uint16_t* out_element,
    uint16_t* out_element_time,
    int* out_ended
) {
    uint32_t first = 0u;
    uint32_t count = 0u;
    uint16_t element = 1u;
    uint16_t element_time = 0u;
    int ended = 0;

    if (!frames || !entity ||
        !ik_frames_bounds(frames, entity->anim_no, &first, &count) ||
        count == 0u) {
        if (out_element) *out_element = element;
        if (out_element_time) *out_element_time = element_time;
        if (out_ended) *out_ended = 0;
        return;
    }

    const uint32_t duration =
        ik_action_duration_ticks(frames, entity->anim_no);
    if (duration > 0u && entity->anim_time >= duration) ended = 1;

    uint32_t remaining = entity->anim_time;
    if (duration > 0u && remaining >= duration) remaining = duration - 1u;

    for (uint32_t i = 0u; i < count; ++i) {
        const uint16_t ticks =
            ik_frame_ticks(&frames->frames[first + i]);
        if (ticks == 0u || remaining < ticks) {
            element = (uint16_t)(i + 1u);
            element_time = (uint16_t)remaining;
            break;
        }
        remaining -= ticks;
    }

    if (out_element) *out_element = element;
    if (out_element_time) *out_element_time = element_time;
    if (out_ended) *out_ended = ended;
}

void ik_entity_runtime_init(
    ik_entity_runtime_t* runtime,
    ik_entity_pool_t* pool,
    const ik_cns_asset_t* cns,
    const ik_frame_table_t* p1_frames,
    const ik_frame_table_t* p2_frames
) {
    if (!runtime) return;
    runtime->pool = pool;
    runtime->cns = cns;
    runtime->frames[0] = p1_frames;
    runtime->frames[1] = p2_frames ? p2_frames : p1_frames;
    runtime->command_masks[0] = 0u;
    runtime->command_masks[1] = 0u;
    runtime->external_user = 0;
    runtime->target_controller = 0;
}

void ik_entity_runtime_set_target_controller(
    ik_entity_runtime_t* runtime,
    void* user,
    ik_entity_target_controller_fn callback
) {
    if (!runtime) return;
    runtime->external_user = user;
    runtime->target_controller = callback;
}

void ik_entity_runtime_set_command_mask(
    ik_entity_runtime_t* runtime,
    uint8_t player,
    uint16_t command_mask
) {
    if (!runtime || player >= 2u) return;
    runtime->command_masks[player] = command_mask;
}

int ik_entity_runtime_enter_state(
    ik_entity_runtime_t* runtime,
    ik_entity_handle_t handle,
    int16_t state_no
) {
    if (!runtime || !runtime->pool || !runtime->cns) return 0;
    ik_entity_t* entity = ik_entity_get(runtime->pool, handle);
    if (!entity) return 0;

    const ik_cns_state_t* spec =
        ik_cns_find_state(runtime->cns, state_no);
    const int16_t previous_anim = entity->anim_no;
    entity->prev_state_no = entity->state_no;
    entity->state_no = state_no;
    /* A helper is created after its owner's controller pass. The dynamic
     * pass runs later in the same fight tick: wrap to 0 there so HitDef
     * Time=0 is visible immediately, while compiled StateController Time=0
     * (lowered to internal tick 1) runs on the next dynamic tick. */
    entity->state_time = 65535u;
    entity->anim_time = 65535u;
    entity->move_contact = 0u;
    entity->move_hit = 0u;
    if (!(spec && spec->hitdef_persist)) {
        entity->hitdef_hit_mask = 0u;
        entity->active_hitdef_global = -1;
        entity->active_hitdef_local = -1;
        entity->active_hitdef_secondary = 0u;
    }

    if (spec) {
        entity->anim_no =
            spec->anim < 0 ? previous_anim : spec->anim;
        entity->ctrl = (uint8_t)(spec->ctrl != 0);
        entity->spr_priority = spec->spr_priority;
        if (spec->state_type != IK_CNS_STATE_UNCHANGED) {
            entity->state_type = (uint8_t)spec->state_type;
        }
        if (spec->move_type != IK_CNS_MOVE_UNCHANGED) {
            entity->move_type = (uint8_t)spec->move_type;
        }
        if (spec->has_velset) {
            entity->vx_q8 = spec->velset_x_q8;
            entity->vy_q8 = spec->velset_y_q8;
        }
    } else {
        entity->anim_no = state_no;
    }
    return 1;
}

int ik_entity_runtime_spawn_helper(
    ik_entity_runtime_t* runtime,
    ik_entity_handle_t parent_handle,
    const ik_cns_helper_t* helper,
    ik_entity_handle_t* out_handle
) {
    if (!runtime || !runtime->pool || !runtime->cns ||
        !helper || !out_handle) {
        return 0;
    }
    const ik_entity_t* parent =
        ik_entity_get_const(runtime->pool, parent_handle);
    if (!parent || parent->owner_player >= 2u) return 0;

    ik_entity_handle_t base_handle = parent_handle;
    if (helper->postype == IK_CNS_HELPER_POS_P2) {
        base_handle = runtime->pool->players[parent->owner_player ^ 1u];
    }
    const ik_entity_t* base =
        ik_entity_get_const(runtime->pool, base_handle);
    if (!base) return 0;

    ik_entity_handle_t spawned = ik_entity_invalid_handle();
    if (!ik_entity_spawn(
            runtime->pool, IK_ENTITY_HELPER, helper->id,
            parent->owner_player, parent_handle, &spawned)) {
        return 0;
    }

    ik_entity_t* entity = ik_entity_get(runtime->pool, spawned);
    if (!entity) {
        (void)ik_entity_destroy(runtime->pool, spawned);
        return 0;
    }

    entity->x_q8 =
        base->x_q8 + (int32_t)base->facing * helper->pos_x_q8;
    entity->y_q8 = base->y_q8 + helper->pos_y_q8;
    entity->facing =
        (int8_t)(parent->facing * (helper->facing < 0 ? -1 : 1));
    entity->life = runtime->cns->constants.life;
    entity->power = parent->power;
    entity->push_back = runtime->cns->constants.ground_back;
    entity->push_front = runtime->cns->constants.ground_front;
    entity->keyctrl = (uint8_t)(helper->keyctrl != 0u);
    entity->ownpal = (uint8_t)(helper->ownpal != 0u);

    if (!ik_entity_runtime_enter_state(
            runtime, spawned, helper->state_no)) {
        (void)ik_entity_destroy(runtime->pool, spawned);
        return 0;
    }

    *out_handle = spawned;
    return 1;
}

int ik_entity_runtime_spawn_explod(
    ik_entity_runtime_t* runtime,
    ik_entity_handle_t parent_handle,
    const ik_cns_explod_t* explod,
    ik_entity_handle_t* out_handle
) {
    if (!runtime || !runtime->pool || !explod || !out_handle) {
        return 0;
    }

    const ik_entity_t* parent =
        ik_entity_get_const(runtime->pool, parent_handle);
    if (!parent || parent->owner_player >= 2u) return 0;

    ik_entity_handle_t spawned = ik_entity_invalid_handle();
    if (!ik_entity_spawn(
            runtime->pool, IK_ENTITY_EXPLOD, 0,
            parent->owner_player, parent_handle, &spawned)) {
        return 0;
    }

    ik_entity_t* entity = ik_entity_get(runtime->pool, spawned);
    if (!entity) {
        (void)ik_entity_destroy(runtime->pool, spawned);
        return 0;
    }

    entity->x_q8 =
        parent->x_q8 + (int32_t)parent->facing * explod->pos_x_q8;
    entity->y_q8 = parent->y_q8 + explod->pos_y_q8;
    entity->vx_q8 = (int32_t)parent->facing * explod->vel_x_q8;
    entity->vy_q8 = explod->vel_y_q8;
    entity->ax_q8 = (int32_t)parent->facing * explod->accel_x_q8;
    entity->ay_q8 = explod->accel_y_q8;
    entity->facing = parent->facing;
    entity->anim_no = explod->anim_no;
    entity->anim_time = 0u;
    entity->state_time = 0u;
    entity->remove_time = explod->remove_time;
    entity->spr_priority = explod->spr_priority;
    entity->ownpal = explod->ownpal;
    entity->pause_move_time = explod->pause_move_time;
    entity->super_move_time = explod->super_move_time;
    entity->explod_bind_time = explod->bind_time;
    entity->explod_parent_state_no = parent->state_no;
    entity->explod_bind_x_q8 = explod->pos_x_q8;
    entity->explod_bind_y_q8 = explod->pos_y_q8;
    entity->explod_remove_on_gethit =
        explod->remove_on_gethit != 0u;
    entity->explod_remove_on_state_change =
        explod->remove_on_state_change != 0u;
    entity->life = 1;

    *out_handle = spawned;
    return 1;
}

int ik_entity_runtime_spawn_projectile_spec(
    ik_entity_runtime_t* runtime,
    ik_entity_handle_t parent_handle,
    const ik_cns_projectile_t* projectile,
    ik_entity_handle_t* out_handle
) {
    if (!runtime || !runtime->pool || !runtime->cns ||
        !projectile || !out_handle) {
        return 0;
    }

    const ik_entity_t* parent =
        ik_entity_get_const(runtime->pool, parent_handle);
    if (!parent || parent->owner_player >= 2u) return 0;

    ik_entity_handle_t spawned = ik_entity_invalid_handle();
    if (!ik_entity_spawn(
            runtime->pool, IK_ENTITY_PROJECTILE, projectile->id,
            parent->owner_player, parent_handle, &spawned)) {
        return 0;
    }

    ik_entity_t* entity = ik_entity_get(runtime->pool, spawned);
    if (!entity) {
        (void)ik_entity_destroy(runtime->pool, spawned);
        return 0;
    }

    entity->x_q8 =
        parent->x_q8 +
        (int32_t)parent->facing * projectile->pos_x_q8;
    entity->y_q8 = parent->y_q8 + projectile->pos_y_q8;
    entity->vx_q8 =
        (int32_t)parent->facing * projectile->vel_x_q8;
    entity->vy_q8 = projectile->vel_y_q8;
    entity->ax_q8 =
        (int32_t)parent->facing * projectile->accel_x_q8;
    entity->ay_q8 = projectile->accel_y_q8;
    entity->facing = parent->facing;
    entity->life = 1;
    entity->power = parent->power;
    entity->push_back = runtime->cns->constants.air_back;
    entity->push_front = runtime->cns->constants.air_front;
    entity->anim_no = projectile->anim_no;
    entity->projectile_main_anim_no = projectile->anim_no;
    entity->anim_time = 0u;
    entity->state_no = -1;
    entity->prev_state_no = -1;
    entity->state_type = IK_CNS_STATE_STAND;
    entity->move_type = IK_CNS_MOVE_ATTACK;
    entity->remove_time = projectile->remove_time;
    entity->active_hitdef_global = projectile->hitdef_global;
    entity->active_hitdef_local = 0;
    entity->projectile_hit_anim_no = projectile->hit_anim_no;
    entity->projectile_remove_anim_no = projectile->remove_anim_no;
    entity->projectile_cancel_anim_no = projectile->cancel_anim_no;
    entity->projectile_edge_bound = projectile->edge_bound;
    entity->projectile_stage_bound = projectile->stage_bound;
    entity->projectile_hits_left =
        projectile->hits ? projectile->hits : 1u;
    entity->projectile_miss_time = projectile->miss_time;
    entity->projectile_hit_cooldown = 0u;
    entity->projectile_priority = projectile->priority;
    entity->projectile_remove_on_hit = projectile->remove_on_hit;
    entity->projectile_velmul_x_q8 = (int16_t)(
        projectile->velmul_x_q8 == 0
            ? IK_ENTITY_Q8_ONE
            : projectile->velmul_x_q8);
    entity->projectile_velmul_y_q8 = (int16_t)(
        projectile->velmul_y_q8 == 0
            ? IK_ENTITY_Q8_ONE
            : projectile->velmul_y_q8);
    entity->spr_priority = projectile->spr_priority;
    entity->ownpal = projectile->ownpal;
    entity->pause_move_time = projectile->pause_move_time;
    entity->super_move_time = projectile->super_move_time;
    entity->projectile_bind_time = projectile->bind_time;
    entity->projectile_parent_state_no = parent->state_no;
    entity->projectile_bind_x_q8 = projectile->pos_x_q8;
    entity->projectile_bind_y_q8 = projectile->pos_y_q8;
    entity->projectile_remove_on_gethit =
        projectile->remove_on_gethit != 0u;
    entity->projectile_remove_on_state_change =
        projectile->remove_on_state_change != 0u;

    *out_handle = spawned;
    return 1;
}

int ik_entity_runtime_spawn_projectile(
    ik_entity_runtime_t* runtime,
    ik_entity_handle_t parent_handle,
    int32_t id,
    int16_t state_no,
    int32_t pos_x_q8,
    int32_t pos_y_q8,
    int32_t vel_x_q8,
    int32_t vel_y_q8,
    ik_entity_handle_t* out_handle
) {
    if (!runtime || !runtime->pool || !runtime->cns || !out_handle) {
        return 0;
    }

    const ik_entity_t* parent =
        ik_entity_get_const(runtime->pool, parent_handle);
    if (!parent || parent->owner_player >= 2u) return 0;

    ik_entity_handle_t spawned = ik_entity_invalid_handle();
    if (!ik_entity_spawn(
            runtime->pool, IK_ENTITY_PROJECTILE, id,
            parent->owner_player, parent_handle, &spawned)) {
        return 0;
    }

    ik_entity_t* entity = ik_entity_get(runtime->pool, spawned);
    if (!entity) {
        (void)ik_entity_destroy(runtime->pool, spawned);
        return 0;
    }

    entity->x_q8 =
        parent->x_q8 + (int32_t)parent->facing * pos_x_q8;
    entity->y_q8 = parent->y_q8 + pos_y_q8;
    entity->facing = parent->facing;
    entity->life = 1;
    entity->power = parent->power;
    entity->push_back = runtime->cns->constants.air_back;
    entity->push_front = runtime->cns->constants.air_front;

    if (!ik_entity_runtime_enter_state(
            runtime, spawned, state_no)) {
        (void)ik_entity_destroy(runtime->pool, spawned);
        return 0;
    }

    entity = ik_entity_get(runtime->pool, spawned);
    if (!entity) return 0;
    entity->vx_q8 = (int32_t)entity->facing * vel_x_q8;
    entity->vy_q8 = vel_y_q8;

    *out_handle = spawned;
    return 1;
}

static int process_controllers(
    ik_entity_runtime_t* runtime,
    ik_entity_handle_t handle,
    uint8_t* freeze_x,
    uint8_t* freeze_y,
    int hit_pause_only
) {
    ik_entity_t* entity = ik_entity_get(runtime->pool, handle);
    if (!entity) return 2;
    const ik_cns_state_t* state =
        ik_cns_find_state(runtime->cns, entity->state_no);
    if (!state || !runtime->cns->controllers) return 0;

    const ik_frame_table_t* frames = frames_for(runtime, entity);
    uint16_t elem = 1u;
    uint16_t elem_time = 0u;
    int anim_ended = 0;
    anim_position(frames, entity, &elem, &elem_time, &anim_ended);

    const ik_cns_controller_context_t context = {
        .state_time = entity->state_time,
        .anim_element = elem,
        .anim_element_time = elem_time,
        .anim_ticks_remaining = (uint16_t)(
            ik_action_duration_ticks(frames, entity->anim_no) > entity->anim_time
                ? (ik_action_duration_ticks(frames, entity->anim_no) - entity->anim_time > 65535u
                    ? 65535u
                    : ik_action_duration_ticks(frames, entity->anim_no) - entity->anim_time)
                : 0u),
        .anim = entity->anim_no,
        .vx_q8 = entity->vx_q8,
        .vy_q8 = entity->vy_q8,
        .y_q8 = entity->y_q8,
        .floor_y_q8 = 0,
        .command_mask =
            entity->keyctrl && entity->owner_player < 2u
                ? runtime->command_masks[entity->owner_player]
                : 0u,
        .hit_pause = entity->hit_pause,
        .alive = (uint8_t)(entity->life > 0),
        .anim_ended = (uint8_t)(anim_ended != 0),
        .move_contact = entity->move_contact,
        .move_hit = entity->move_hit,
        .round_state = 2u
    };

    for (uint8_t i = 0u; i < state->controller_count; ++i) {
        const uint16_t index =
            (uint16_t)(state->controller_ofs + i);
        if (index >= runtime->cns->controller_count) break;

        const ik_cns_controller_t* ctrl =
            &runtime->cns->controllers[index];
        if (hit_pause_only &&
            (ctrl->flags & IK_CNS_CTRL_IGNORE_HIT_PAUSE) == 0u) {
            continue;
        }
        if (!ik_cns_controller_trigger_context_now(ctrl, &context)) {
            continue;
        }

        entity = ik_entity_get(runtime->pool, handle);
        if (!entity) return 2;

        switch ((ik_cns_controller_type_t)ctrl->type) {
            case IK_CNS_CTRL_CHANGE_STATE:
            case IK_CNS_CTRL_SELF_STATE:
                if (ik_entity_runtime_enter_state(
                        runtime, handle, (int16_t)ctrl->value0)) {
                    entity = ik_entity_get(runtime->pool, handle);
                    if (entity &&
                        (ctrl->flags & IK_CNS_CTRL_HAS_CTRL) != 0u) {
                        entity->ctrl =
                            (uint8_t)(ctrl->value1 != 0);
                    }
                    return 1;
                }
                break;

            case IK_CNS_CTRL_CTRL_SET:
                entity->ctrl = (uint8_t)(ctrl->value0 != 0);
                break;

            case IK_CNS_CTRL_VAR_SET:
                if (ctrl->value0 >= 0 &&
                    ctrl->value0 < (int32_t)IK_ENTITY_VAR_COUNT) {
                    entity->vars[ctrl->value0] = ctrl->value1;
                }
                break;

            case IK_CNS_CTRL_VAR_ADD:
                if (ctrl->value0 >= 0 &&
                    ctrl->value0 < (int32_t)IK_ENTITY_VAR_COUNT) {
                    entity->vars[ctrl->value0] += ctrl->value1;
                }
                break;

            case IK_CNS_CTRL_POS_ADD:
                entity->x_q8 +=
                    (int32_t)entity->facing * ctrl->value0;
                entity->y_q8 += ctrl->value1;
                break;

            case IK_CNS_CTRL_POS_SET:
                if ((ctrl->flags & IK_CNS_CTRL_AXIS_X) != 0u) {
                    entity->x_q8 = ctrl->value0;
                }
                if ((ctrl->flags & IK_CNS_CTRL_AXIS_Y) != 0u) {
                    entity->y_q8 = ctrl->value1;
                }
                break;

            case IK_CNS_CTRL_VEL_SET:
                if ((ctrl->flags & IK_CNS_CTRL_AXIS_X) != 0u) {
                    int32_t vx = ctrl->value0;
                    if ((ctrl->flags & IK_CNS_CTRL_LOCAL_X) != 0u) {
                        vx *= entity->facing;
                    }
                    entity->vx_q8 = vx;
                }
                if ((ctrl->flags & IK_CNS_CTRL_AXIS_Y) != 0u) {
                    entity->vy_q8 = ctrl->value1;
                }
                break;

            case IK_CNS_CTRL_VEL_ADD:
                if ((ctrl->flags & IK_CNS_CTRL_AXIS_X) != 0u) {
                    int32_t vx = ctrl->value0;
                    if ((ctrl->flags & IK_CNS_CTRL_LOCAL_X) != 0u) {
                        vx *= entity->facing;
                    }
                    entity->vx_q8 += vx;
                }
                if ((ctrl->flags & IK_CNS_CTRL_AXIS_Y) != 0u) {
                    entity->vy_q8 += ctrl->value1;
                }
                break;

            case IK_CNS_CTRL_VEL_MUL:
                if ((ctrl->flags & IK_CNS_CTRL_AXIS_X) != 0u) {
                    entity->vx_q8 =
                        (entity->vx_q8 * ctrl->value0) / IK_CNS_Q8_ONE;
                }
                if ((ctrl->flags & IK_CNS_CTRL_AXIS_Y) != 0u) {
                    entity->vy_q8 =
                        (entity->vy_q8 * ctrl->value1) / IK_CNS_Q8_ONE;
                }
                break;

            case IK_CNS_CTRL_VEL_MUL_X_BY_ANIM_ELEM: {
                const int32_t mul =
                    elem < (uint16_t)(
                        ctrl->trigger_value < 1
                            ? 1 : ctrl->trigger_value)
                        ? ctrl->value0 : ctrl->value1;
                entity->vx_q8 =
                    (entity->vx_q8 * mul) / IK_CNS_Q8_ONE;
                break;
            }

            case IK_CNS_CTRL_CHANGE_ANIM:
            case IK_CNS_CTRL_CHANGE_ANIM2:
                entity->anim_no = (int16_t)ctrl->value0;
                entity->anim_time = anim_element_start_tick(
                    frames, entity->anim_no,
                    (uint16_t)(ctrl->value1 < 1 ? 1 : ctrl->value1));
                break;

            case IK_CNS_CTRL_SPR_PRIORITY:
                entity->spr_priority = (int8_t)ctrl->value0;
                break;

            case IK_CNS_CTRL_TURN:
                entity->facing = (int8_t)-entity->facing;
                break;

            case IK_CNS_CTRL_WIDTH:
                entity->push_front = (int16_t)(
                    runtime->cns->constants.ground_front + ctrl->value0);
                entity->push_back = (int16_t)(
                    runtime->cns->constants.ground_back + ctrl->value1);
                break;

            case IK_CNS_CTRL_POS_FREEZE:
                if (freeze_x &&
                    (ctrl->flags & IK_CNS_CTRL_AXIS_X) != 0u) {
                    *freeze_x = 1u;
                }
                if (freeze_y &&
                    (ctrl->flags & IK_CNS_CTRL_AXIS_Y) != 0u) {
                    *freeze_y = 1u;
                }
                break;

            case IK_CNS_CTRL_HELPER:
                if (ctrl->value0 >= 0 &&
                    ctrl->value0 < runtime->cns->helper_count &&
                    runtime->cns->helpers) {
                    ik_entity_handle_t child =
                        ik_entity_invalid_handle();
                    (void)ik_entity_runtime_spawn_helper(
                        runtime, handle,
                        &runtime->cns->helpers[ctrl->value0],
                        &child);
                }
                break;

            case IK_CNS_CTRL_TARGET_BIND:
            case IK_CNS_CTRL_TARGET_FACING:
            case IK_CNS_CTRL_TARGET_LIFE_ADD:
            case IK_CNS_CTRL_TARGET_STATE:
                if (runtime->target_controller) {
                    const int result = runtime->target_controller(
                        runtime->external_user, runtime, handle, ctrl);
                    if (result > 0) return 1;
                    if (result < 0) return 2;
                }
                break;

            case IK_CNS_CTRL_DESTROY_SELF:
                (void)ik_entity_destroy(runtime->pool, handle);
                return 2;

            default:
                break;
        }
    }

    return 0;
}

static void step_one(
    ik_entity_runtime_t* runtime,
    ik_entity_handle_t handle
) {
    ik_entity_t* entity = ik_entity_get(runtime->pool, handle);
    if (!entity) return;

    if (entity->type == IK_ENTITY_EXPLOD ||
        (entity->type == IK_ENTITY_PROJECTILE &&
         entity->state_no < 0)) {
        int explod_bound = 0;
        if (entity->type == IK_ENTITY_EXPLOD) {
            const ik_entity_t* parent =
                ik_entity_get_const(runtime->pool, entity->parent);
            if (entity->explod_remove_on_state_change && parent &&
                parent->state_no != entity->explod_parent_state_no) {
                (void)ik_entity_destroy(runtime->pool, handle);
                return;
            }
            if (entity->explod_remove_on_gethit && parent &&
                parent->move_type == IK_CNS_MOVE_HIT) {
                (void)ik_entity_destroy(runtime->pool, handle);
                return;
            }
            if (parent && entity->explod_bind_time != 0) {
                explod_bound = 1;
                entity->x_q8 =
                    parent->x_q8 +
                    (int32_t)parent->facing *
                        entity->explod_bind_x_q8;
                entity->y_q8 =
                    parent->y_q8 + entity->explod_bind_y_q8;
                entity->facing = parent->facing;
                if (entity->explod_bind_time > 0) {
                    --entity->explod_bind_time;
                }
            }
        }
        ++entity->state_time;
        ++entity->anim_time;
        if (entity->type != IK_ENTITY_EXPLOD || !explod_bound) {
            entity->x_q8 += entity->vx_q8;
            entity->y_q8 += entity->vy_q8;
        }
        entity->vx_q8 += entity->ax_q8;
        entity->vy_q8 += entity->ay_q8;

        if (entity->type == IK_ENTITY_PROJECTILE) {
            const ik_entity_t* parent =
                ik_entity_get_const(runtime->pool, entity->parent);
            if (entity->projectile_remove_on_state_change &&
                parent &&
                parent->state_no != entity->projectile_parent_state_no) {
                (void)ik_entity_destroy(runtime->pool, handle);
                return;
            }
            if (entity->projectile_remove_on_gethit && parent &&
                parent->move_type == IK_CNS_MOVE_HIT) {
                (void)ik_entity_destroy(runtime->pool, handle);
                return;
            }
            if (parent && entity->projectile_bind_time != 0) {
                entity->x_q8 =
                    parent->x_q8 +
                    (int32_t)parent->facing *
                        entity->projectile_bind_x_q8;
                entity->y_q8 =
                    parent->y_q8 + entity->projectile_bind_y_q8;
                entity->facing = parent->facing;
                if (entity->projectile_bind_time > 0) {
                    --entity->projectile_bind_time;
                }
            }

            entity->vx_q8 =
                (entity->vx_q8 * entity->projectile_velmul_x_q8) /
                IK_ENTITY_Q8_ONE;
            entity->vy_q8 =
                (entity->vy_q8 * entity->projectile_velmul_y_q8) /
                IK_ENTITY_Q8_ONE;
            if (entity->projectile_hit_cooldown > 0u) {
                --entity->projectile_hit_cooldown;
                if (entity->projectile_hit_cooldown == 0u) {
                    entity->hitdef_hit_mask = 0u;
                    if (entity->active_hitdef_global >= 0 &&
                        entity->projectile_main_anim_no >= 0 &&
                        entity->anim_no != entity->projectile_main_anim_no) {
                        entity->anim_no = entity->projectile_main_anim_no;
                        entity->anim_time = 0u;
                    }
                }
            }
            if (entity->projectile_edge_bound > 0) {
                const int32_t x = entity->x_q8 / IK_ENTITY_Q8_ONE;
                if (x < -entity->projectile_edge_bound ||
                    x > 320 + entity->projectile_edge_bound) {
                    (void)ik_entity_destroy(runtime->pool, handle);
                    return;
                }
            }
            if (entity->projectile_stage_bound > 0) {
                const int32_t x = entity->x_q8 / IK_ENTITY_Q8_ONE;
                if (x < -entity->projectile_stage_bound ||
                    x > 320 + entity->projectile_stage_bound) {
                    (void)ik_entity_destroy(runtime->pool, handle);
                    return;
                }
            }
        }

        if (entity->remove_time > 0) {
            --entity->remove_time;
            if (entity->remove_time == 0) {
                (void)ik_entity_destroy(runtime->pool, handle);
                return;
            }
        } else if (entity->remove_time == 0) {
            const ik_frame_table_t* frames = frames_for(runtime, entity);
            const uint32_t duration =
                ik_action_duration_ticks(frames, entity->anim_no);
            if ((entity->type == IK_ENTITY_EXPLOD ||
                 (entity->type == IK_ENTITY_PROJECTILE &&
                  entity->active_hitdef_global < 0)) &&
                duration > 0u && entity->anim_time >= duration) {
                (void)ik_entity_destroy(runtime->pool, handle);
            }
        }
        return;
    }

    if (entity->type != IK_ENTITY_HELPER &&
        entity->type != IK_ENTITY_PROJECTILE) {
        return;
    }

    uint8_t freeze_x = 0u;
    uint8_t freeze_y = 0u;
    if (entity->hit_pause > 0u) {
        (void)process_controllers(
            runtime, handle, &freeze_x, &freeze_y, 1);
        entity = ik_entity_get(runtime->pool, handle);
        if (entity && entity->hit_pause > 0u) {
            --entity->hit_pause;
        }
        return;
    }

    ++entity->state_time;
    ++entity->anim_time;

    const int result =
        process_controllers(
            runtime, handle, &freeze_x, &freeze_y, 0);
    if (result != 0) return;

    entity = ik_entity_get(runtime->pool, handle);
    if (!entity) return;
    const ik_cns_state_t* state =
        ik_cns_find_state(runtime->cns, entity->state_no);

    if (state) {
        entity->state_type =
            state->state_type == IK_CNS_STATE_UNCHANGED
                ? entity->state_type
                : (uint8_t)state->state_type;
        entity->move_type =
            state->move_type == IK_CNS_MOVE_UNCHANGED
                ? entity->move_type
                : (uint8_t)state->move_type;

        if (state->physics == IK_CNS_PHYS_AIR && !freeze_y) {
            entity->vy_q8 += runtime->cns->constants.yaccel_q8;
        } else if (state->physics == IK_CNS_PHYS_STAND) {
            entity->vx_q8 =
                (entity->vx_q8 *
                 runtime->cns->constants.stand_friction_q8) /
                IK_CNS_Q8_ONE;
        } else if (state->physics == IK_CNS_PHYS_CROUCH) {
            entity->vx_q8 =
                (entity->vx_q8 *
                 runtime->cns->constants.crouch_friction_q8) /
                IK_CNS_Q8_ONE;
        }
    }

    if (!freeze_x) entity->x_q8 += entity->vx_q8;
    if (!freeze_y) entity->y_q8 += entity->vy_q8;
}

void ik_entity_runtime_step_paused(
    ik_entity_runtime_t* runtime,
    uint8_t owner_player,
    int super_pause
) {
    if (!runtime || !runtime->pool || !runtime->cns ||
        owner_player >= 2u) {
        return;
    }

    ik_entity_handle_t snapshot[IK_ENTITY_CAPACITY];
    uint8_t count = 0u;
    for (uint8_t slot = 0u; slot < IK_ENTITY_CAPACITY; ++slot) {
        const ik_entity_t* entity = &runtime->pool->entities[slot];
        if ((entity->type != IK_ENTITY_HELPER &&
             entity->type != IK_ENTITY_PROJECTILE &&
             entity->type != IK_ENTITY_EXPLOD) ||
            entity->owner_player != owner_player) {
            continue;
        }

        const uint8_t budget = super_pause
            ? entity->super_move_time
            : entity->pause_move_time;
        if (budget == 0u) continue;

        snapshot[count].slot = slot;
        snapshot[count].generation =
            runtime->pool->generations[slot];
        ++count;
    }

    for (uint8_t i = 0u; i < count; ++i) {
        ik_entity_t* entity =
            ik_entity_get(runtime->pool, snapshot[i]);
        if (!entity) continue;
        uint8_t* budget = super_pause
            ? &entity->super_move_time
            : &entity->pause_move_time;
        if (*budget == 0u) continue;
        --(*budget);
        step_one(runtime, snapshot[i]);
    }
}

void ik_entity_runtime_step(ik_entity_runtime_t* runtime) {
    if (!runtime || !runtime->pool || !runtime->cns) return;

    ik_entity_handle_t snapshot[IK_ENTITY_CAPACITY];
    uint8_t count = 0u;
    for (uint8_t slot = 0u; slot < IK_ENTITY_CAPACITY; ++slot) {
        const ik_entity_t* entity = &runtime->pool->entities[slot];
        if (entity->type != IK_ENTITY_HELPER &&
            entity->type != IK_ENTITY_PROJECTILE &&
            entity->type != IK_ENTITY_EXPLOD) {
            continue;
        }
        snapshot[count].slot = slot;
        snapshot[count].generation =
            runtime->pool->generations[slot];
        ++count;
    }

    for (uint8_t i = 0u; i < count; ++i) {
        step_one(runtime, snapshot[i]);
    }
}
