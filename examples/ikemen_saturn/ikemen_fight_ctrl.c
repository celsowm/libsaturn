/* CNS controller executor: builds the trigger context for one fighter state
 * and runs each triggered controller through its family handler. */
#include "ikemen_fight_internal.h"

static uint16_t anim_ticks_remaining(
    const ik_frame_table_t* frames,
    const ik_fighter_t* f
) {
    const uint32_t duration = ik_action_duration_ticks(frames, f->anim);
    if (duration <= f->anim_time) return 0u;
    const uint32_t left = duration - f->anim_time;
    return (uint16_t)(left > 65535u ? 65535u : left);
}

/* Ticks left in the current element, this one included. */
static uint16_t elem_ticks_left(
    const ik_frame_table_t* frames,
    const ik_fighter_t* f,
    uint16_t elem,
    uint16_t elem_time
) {
    uint32_t first = 0u;
    uint32_t count = 0u;
    if (!frames || !ik_frames_bounds(frames, f->anim, &first, &count) ||
        elem < 1u || elem > count) {
        return 0u;
    }
    const uint16_t ticks = ik_frame_ticks(&frames->frames[first + elem - 1u]);
    return (uint16_t)(ticks > elem_time ? ticks - elem_time : 0u);
}

static void queue_state_playsnds(
    ik_fight_t* fight,
    const ik_fighter_t* f,
    const ik_cns_asset_t* state_cns,
    const ik_cns_state_t* state,
    uint16_t elem,
    uint16_t elem_time,
    int anim_ended
) {
    if (!state_cns->playsnds) return;
    for (uint8_t i = 0u; i < state->playsnd_count; ++i) {
        const uint16_t index = (uint16_t)(state->playsnd_ofs + i);
        if (index >= state_cns->playsnd_count) break;
        const ik_cns_playsnd_t* sound = &state_cns->playsnds[index];
        if (ik_cns_trigger_now(
                sound->trigger_kind, sound->trigger_value,
                f->state_time, elem, elem_time, anim_ended)) {
            ikf_queue_sound_event(fight, sound->group, sound->item);
        }
    }
}

/* Root entity of the fighter's player slot and how many projectiles it owns,
 * for the NumProj/ProjContact trigger family. */
static const ik_entity_t* projectile_query_root(
    const ik_fight_t* fight,
    uint8_t player_index,
    uint8_t* num_projectiles
) {
    *num_projectiles = 0u;
    if (!fight->entities || player_index >= 2u) return 0;
    for (uint8_t slot = 0u; slot < IK_ENTITY_CAPACITY; ++slot) {
        const ik_entity_t* candidate = &fight->entities->entities[slot];
        if (candidate->type == IK_ENTITY_PROJECTILE &&
            candidate->owner_player == player_index) {
            ++*num_projectiles;
        }
    }
    return ik_entity_get_const(
        fight->entities, fight->player_entities[player_index]);
}

static ik_cns_controller_context_t make_controller_context(
    const ik_fight_t* fight,
    const ik_fighter_t* f,
    const ik_frame_table_t* frames,
    uint16_t command_mask,
    uint16_t elem,
    uint16_t elem_time,
    int anim_ended
) {
    /* Screen-edge distances: from the origin, minus the Width controller's
     * edge width and, for classic characters, 0.5 px in the air / 1 px
     * lying down (upstream's undocumented Mugen offset). */
    const int32_t back_dist =
        f->facing > 0 ? f->x_q8 - f->xmin_q8 : f->xmax_q8 - f->x_q8;
    const int32_t front_dist =
        f->facing > 0 ? f->xmax_q8 - f->x_q8 : f->x_q8 - f->xmin_q8;
    const int32_t type_offset =
        f->cur_state_type == IK_CNS_STATE_AIR ? IK_CNS_Q8_ONE / 2
        : f->cur_state_type == IK_CNS_STATE_LIEDOWN ? IK_CNS_Q8_ONE : 0;
    const int32_t back_body_dist =
        back_dist - f->edge_back * IK_CNS_Q8_ONE - type_offset;
    const int32_t front_body_dist =
        front_dist - f->edge_front * IK_CNS_Q8_ONE - type_offset;
    const uint8_t player_index = ikf_fighter_player_index(fight, f);
    const ik_fighter_t* p2 =
        player_index < 2u ? &fight->fighters[player_index ^ 1u] : 0;
    const int32_t p2_dist_x_q8 =
        p2 ? (p2->x_q8 - f->x_q8) * (int32_t)f->facing : 0;
    uint8_t num_projectiles = 0u;
    const ik_entity_t* query_root =
        projectile_query_root(fight, player_index, &num_projectiles);

    const ik_cns_controller_context_t context = {
        .state_time = f->state_time,
        .anim_element = elem,
        .anim_element_time = elem_time,
        .anim_ticks_remaining = anim_ticks_remaining(frames, f),
        .anim_elem_ticks_left = elem_ticks_left(frames, f, elem, elem_time),
        .anim = f->anim,
        .vx_q8 = f->vx_q8,
        .vy_q8 = f->vy_q8,
        .y_q8 = f->y_q8,
        .floor_y_q8 = (int32_t)IK_FLOOR_Y * IK_CNS_Q8_ONE,
        .command_mask = command_mask,
        .hitstun = f->hitstun,
        .hit_pause = f->hit_pause,
        .hit_shake_time = f->hit_shake_time,
        .hit_slide_time = f->hit_slide_time,
        .hit_ctrl_time = f->hit_ctrl_time,
        .fall_time = f->fall_time,
        .back_edge_body_dist_q8 = back_body_dist,
        .front_edge_body_dist_q8 = front_body_dist,
        .back_edge_dist_q8 = back_dist,
        .p2_dist_x_q8 = p2_dist_x_q8,
        .state_axis = f->state_axis,
        .hit_launch = (uint8_t)(
            f->gethit_fall || f->gethit_vy_q8 != 0 || !f->on_ground),
        .alive = (uint8_t)(f->hp > 0),
        .can_recover = (uint8_t)(
            f->gethit_fall_recover &&
            f->fall_time >= f->gethit_fall_recover_time),
        .is_bound = (uint8_t)(f->bound_to >= 0),
        .anim_ended = (uint8_t)(anim_ended != 0),
        .move_contact = f->move_contact,
        .move_hit = f->move_hit,
        .round_state = fight->round_state,
        .num_projectiles = num_projectiles,
        .proj_contact = query_root ? query_root->proj_query_contact : 0u,
        .proj_hit = query_root ? query_root->proj_query_hit : 0u,
        .proj_guarded = query_root ? query_root->proj_query_guarded : 0u,
        .proj_contact_time =
            query_root ? query_root->proj_query_contact_time : -1,
        .proj_hit_time =
            query_root ? query_root->proj_query_hit_time : -1,
        .proj_guarded_time =
            query_root ? query_root->proj_query_guarded_time : -1,
        .in_guard_dist = f->in_guard_dist
    };
    return context;
}

static int dispatch_controller(const ik_ctrl_exec_t* x) {
    int result = ikf_ctrl_apply_motion(x);
    if (result != IKF_CTRL_UNHANDLED) return result;
    result = ikf_ctrl_apply_anim(x);
    if (result != IKF_CTRL_UNHANDLED) return result;
    result = ikf_ctrl_apply_state(x);
    if (result != IKF_CTRL_UNHANDLED) return result;
    result = ikf_ctrl_apply_fx(x);
    if (result != IKF_CTRL_UNHANDLED) return result;
    result = ikf_ctrl_apply_spawn(x);
    if (result != IKF_CTRL_UNHANDLED) return result;
    result = ikf_ctrl_apply_target(x);
    return result == IKF_CTRL_UNHANDLED ? IKF_CTRL_NEXT : result;
}

int ikf_process_cns_controllers(
    ik_fight_t* fight,
    ik_fighter_t* f,
    const ik_fight_controls_t* controls,
    const ik_frame_table_t* frames,
    int hit_pause_only
) {
    if (!fight || !f) return 0;
    {
        const ik_frame_table_t* owned_frames = frames_for_fighter(fight, f);
        if (owned_frames) frames = owned_frames;
    }
    const ik_cns_asset_t* state_cns = cns_for_fighter(fight, f);
    const ik_cns_asset_t* native_cns = cns_for_owner(fight, f->owner_player);
    if (!state_cns) return 0;
    const ik_cns_state_t* state = ik_cns_find_state(state_cns, f->state);
    if (!state) return 0;

    uint16_t elem = 1u;
    uint16_t elem_time = 0u;
    int anim_ended = 0;
    ikf_anim_position(frames, f, &elem, &elem_time, &anim_ended);

    ik_cns_controller_context_t context = make_controller_context(
        fight, f, frames, controls_command_mask(controls),
        elem, elem_time, anim_ended);
    if (!hit_pause_only) {
        queue_state_playsnds(
            fight, f, state_cns, state, elem, elem_time, anim_ended);
    }

    if (!state_cns->controllers) return 0;
    for (uint8_t i = 0u; i < state->controller_count; ++i) {
        const uint16_t index = (uint16_t)(state->controller_ofs + i);
        if (index >= state_cns->controller_count) break;
        const ik_cns_controller_t* ctrl = &state_cns->controllers[index];
        if (hit_pause_only &&
            (ctrl->flags & IK_CNS_CTRL_IGNORE_HIT_PAUSE) == 0u) {
            continue;
        }
        ik_cns_controller_context_t query_context = context;
        if (ctrl->trigger_kind == IK_CNS_TRIGGER_NUM_TARGET_QUERY) {
            const int id_matches =
                f->target_index >= 0 && f->target_index < 2 &&
                (ctrl->trigger_aux < 0 ||
                 f->target_id == ctrl->trigger_aux);
            query_context.num_targets = (uint8_t)(id_matches ? 1u : 0u);
        }
        if (!ik_cns_controller_trigger_context_now(ctrl, &query_context)) {
            continue;
        }

        const ik_ctrl_exec_t exec = {
            fight, f, ctrl, &context, state_cns, native_cns, frames,
            i, elem, (uint8_t)(anim_ended != 0)
        };
        const int16_t anim_before = f->anim;
        const uint32_t anim_time_before = f->anim_time;
        const int result = dispatch_controller(&exec);
        if (result == IKF_CTRL_END_TICK) return 1;
        if (result == IKF_CTRL_END_PASS) return 0;
        /* Later controllers see the velocity and height this one left. */
        context.vx_q8 = f->vx_q8;
        context.vy_q8 = f->vy_q8;
        context.y_q8 = f->y_q8;
        if (f->anim != anim_before || f->anim_time != anim_time_before) {
            /* A ChangeAnim-like controller ran: the controllers after it
             * see the new animation, as upstream's do. */
            ikf_anim_position(frames, f, &elem, &elem_time, &anim_ended);
            context.anim = f->anim;
            context.anim_element = elem;
            context.anim_element_time = elem_time;
            context.anim_ticks_remaining = anim_ticks_remaining(frames, f);
            context.anim_elem_ticks_left =
                elem_ticks_left(frames, f, elem, elem_time);
            context.anim_ended = (uint8_t)(anim_ended != 0);
        }
    }
    return 0;
}
