/* Per-fighter tick. Same order as Ikemen GO: housekeeping, controllers and
 * input dispatch, physics, then state/anim time advance. A ChangeState re-runs
 * the freshly entered state in the same tick (its Time = 0 controllers and its
 * physics), exactly like upstream. */
#include "ikemen_fight_internal.h"

/* Upstream has no cap; this only stops a ChangeState ping-pong from spinning. */
#define IKF_MAX_STATE_CHANGES_PER_TICK 8

enum {
    IKF_STEP_DONE = 0,
    IKF_STEP_RERUN = 1       /* state changed: run the new state this tick */
};

typedef struct ik_step {
    ik_fight_t* fight;
    ik_fighter_t* f;
    const ik_fighter_t* foe;
    int index;
    const ik_fight_controls_t* controls;
    int is_dummy;
    const ik_frame_table_t* frames;
    const ik_frame_table_t* foe_frames;
    const ik_cns_constants_t* c;
    const ik_cns_asset_t* native_cns;
    uint16_t entries_at_physics;  /* state_entries when physics began */
} ik_step_t;

static void tick_effect_timers(
    ik_fighter_t* f,
    const ik_fight_controls_t* controls
) {
    f->pos_freeze_x = 0u;
    f->pos_freeze_y = 0u;
    if (f->afterimage_time > 0u) {
        --f->afterimage_time;
    }
    if (f->not_hit_by_time > 0u) {
        --f->not_hit_by_time;
        if (f->not_hit_by_time == 0u) {
            f->not_hit_by_mask = 0u;
            f->not_hit_by_attr_mask = 0u;
        }
    }
    if (controls && !controls->up) {
        f->up_latched = 0u;
    }
}

/* PalFX counts down at the end of the tick, after the controllers that set
 * it ran, so a PalFX of N ticks is visible for N ticks. */
static void step_palfx(ik_fighter_t* f) {
    if (f->palfx_time == 0u) return;
    --f->palfx_time;
    ++f->palfx_phase;
    ++f->palfx_sinmul_phase;
}

/* Width is a one-tick controller in MUGEN/Ikemen. Reset to the character
 * constants before evaluating the current tick's controllers. */
static void reset_body_size(
    ik_fighter_t* f,
    const ik_cns_constants_t* c
) {
    if (!c) return;
    const int air = f->cur_state_type == IK_CNS_STATE_AIR;
    f->body_air = (uint8_t)air;
    f->push_back = air ? c->air_back : c->ground_back;
    f->push_front = air ? c->air_front : c->ground_front;
    f->body_height = c->height;
}

/* Upstream resets the edge widths every tick, and the ScreenBound /
 * movecamera flags at the start of every tick outside hit pause. */
static void reset_screen_flags(ik_fighter_t* f) {
    f->edge_front = 0;
    f->edge_back = 0;
    if (f->hit_pause == 0u) {
        f->screen_bound = 1u;
        f->move_camera_x = 1u;
        f->move_camera_y = 1u;
    }
}

static void update_facing_and_fall_time(
    ik_fight_t* fight,
    ik_fighter_t* f,
    const ik_fighter_t* foe
) {
    const ik_cns_state_t* spec = fighter_state_spec(fight, f);
    if (f->bound_to < 0 && !ik_entity_handle_is_valid(f->bound_entity)) {
        if (!spec) {
            if (f->ctrl) f->facing = (foe->x >= f->x) ? 1 : -1;
        } else if (f->ctrl || fight->round_state > 2u) {
            /* Upstream turns the root only from idle, crouch, walk and the
             * landing recovery (52) once its animation has ended. */
            uint16_t elem = 1u, elem_time = 0u;
            int anim_ended = 0;
            ikf_anim_position(frames_for_fighter(fight, f), f,
                              &elem, &elem_time, &anim_ended);
            if (f->state == 0 || f->state == 11 || f->state == 20 ||
                (f->state == 52 && anim_ended)) {
                ikf_auto_turn(fight, f);
            }
        }
    }
    if (f->gethit_fall && f->fall_time < 65535u) {
        f->fall_time++;
    }
}

/* Holding back against an incoming attack enters the guard stance (leaving it
 * is the guard states' own StopGuarding controller). Returns 1 when a state
 * was entered. */
static int enter_guard_stance(const ik_step_t* s) {
    ik_fight_t* fight = s->fight;
    ik_fighter_t* f = s->f;
    const ik_fight_controls_t* controls = s->controls;
    if (s->is_dummy || !controls || f->hitstun != 0u) return 0;

    const int threat = f->in_guard_dist;
    if (controls->back && threat && f->ctrl &&
        !is_attack_fighter(fight, f) &&
        !ikf_in_guard_state(f->state) &&
        ik_cns_find_state(s->native_cns, 120)) {
        f->guard_type = ikf_guard_type_for(fight, f, controls);
        ikf_enter_state(fight, f, 120);
        return 1;
    }
    return 0;
}

static void drop_stale_binder(const ik_fight_t* fight, ik_fighter_t* f) {
    if (!ik_entity_handle_is_valid(f->bound_entity)) return;
    const ik_entity_t* binder =
        fight->entities
            ? ik_entity_get_const(fight->entities, f->bound_entity)
            : 0;
    if (!binder) {
        f->bound_entity = ik_entity_invalid_handle();
    }
}

static int step_hitstun(const ik_step_t* s) {
    ik_fighter_t* f = s->f;
    /* The hit time only starts counting once the shake is over. */
    if (f->hit_shake_time > 0u) return IKF_STEP_DONE;
    f->hitstun--;
    if (!f->on_ground) {
        ikf_step_air(s->fight, f, 0, -1);
    } else if (s->c) {
        const ik_cns_state_t* spec = fighter_state_spec(s->fight, f);
        const int physics = spec ? spec->physics : IK_CNS_PHYS_STAND;
        ikf_apply_ground_velocity(
            f, s->c,
            physics == IK_CNS_PHYS_CROUCH
                ? IK_CNS_PHYS_CROUCH
                : IK_CNS_PHYS_STAND);
    }
    if (f->hitstun == 0u && f->state == IK_STATE_HIT && f->on_ground) {
        ikf_enter_state(s->fight, f, IK_STATE_IDLE);
        f->vx_q8 = 0;
    }
    return IKF_STEP_DONE;
}

static int step_attack(const ik_step_t* s) {
    ik_fight_t* fight = s->fight;
    ik_fighter_t* f = s->f;
    if (is_air_attack_fighter(fight, f)) {
        /* Physics=A continues while the attack animation runs. If the
         * common1 landing state is compiled, landing transitions to it. */
        ikf_step_air(fight, f, 1, -1);
        return IKF_STEP_DONE;
    }

    const ik_cns_state_t* spec = fighter_state_spec(fight, f);
    const int physics = state_physics(f, spec);
    if (spec &&
        (physics == IK_CNS_PHYS_STAND || physics == IK_CNS_PHYS_CROUCH)) {
        if (s->c) ikf_apply_ground_velocity(f, s->c, physics);
    } else if (spec && physics == IK_CNS_PHYS_NONE) {
        /* Physics=N keeps explicit velocity but applies no automatic
         * friction/gravity. Position still integrates velocity. */
        f->x_q8 += f->vx_q8;
        sync_position(f);
    }

    const uint32_t duration = ik_action_duration_ticks(s->frames, f->anim);
    if (duration > 0u && f->anim_time >= duration) {
        ikf_enter_state(
            fight, f,
            is_crouch_attack_fighter(fight, f)
                ? IK_STATE_CROUCH
                : IK_STATE_IDLE);
    }
    return IKF_STEP_DONE;
}

static int step_airborne(const ik_step_t* s) {
    ik_fight_t* fight = s->fight;
    ik_fighter_t* f = s->f;
    const ik_cns_state_t* spec = fighter_state_spec(fight, f);
    const int custom_landing =
        spec && spec->physics == IK_CNS_PHYS_NONE && spec->owns_air_accel;
    int16_t guard_land_state = -1;
    if (f->state == 132 || f->state == 155) {
        guard_land_state =
            s->controls && s->controls->back && f->in_guard_dist ? 130 : 52;
    }
    ikf_step_air(fight, f, custom_landing ? 0 : 1, guard_land_state);
    return IKF_STEP_DONE;
}

/* Legacy movement for characters without compiled common states. */
static void step_uncompiled_ground(const ik_step_t* s) {
    ik_fight_t* fight = s->fight;
    ik_fighter_t* f = s->f;
    const ik_fight_controls_t* controls = s->controls;
    int moved = 0;
    if (controls->back) {
        const int32_t speed =
            s->c ? s->c->walk_back_q8 : -2 * IK_CNS_Q8_ONE;
        f->x_q8 += (int32_t)f->facing * speed;
        moved = 1;
    }
    if (controls->forward) {
        const int32_t speed =
            s->c ? s->c->walk_fwd_q8 : 2 * IK_CNS_Q8_ONE;
        f->x_q8 += (int32_t)f->facing * speed;
        moved = 1;
    }
    sync_position(f);
    if (moved) {
        if (f->state != IK_STATE_WALK) {
            ikf_enter_state(fight, f, IK_STATE_WALK);
        }
    } else if (f->state != IK_STATE_IDLE) {
        ikf_enter_state(fight, f, IK_STATE_IDLE);
    }
}

static int step_grounded(const ik_step_t* s) {
    ik_fight_t* fight = s->fight;
    ik_fighter_t* f = s->f;
    const ik_fight_controls_t* controls = s->controls;
    const ik_cns_state_t* spec = fighter_state_spec(fight, f);

    const int physics = state_physics(f, spec);
    if (spec &&
        (physics == IK_CNS_PHYS_STAND || physics == IK_CNS_PHYS_CROUCH)) {
        if (s->c) ikf_apply_ground_velocity(f, s->c, physics);
    } else if (!spec && !s->is_dummy && controls) {
        step_uncompiled_ground(s);
    } else if (!spec && f->state != IK_STATE_IDLE) {
        ikf_enter_state(fight, f, IK_STATE_IDLE);
    }
    return IKF_STEP_DONE;
}

/* Upstream's hard-coded keys run before any state controller: guard, jump,
 * crouch and the walk/brake pair. Walking forward while holding back against
 * an attacker in guard distance cancels out (back is for guarding then).
 * Returns 1 when a state was entered. */
static int dispatch_hardcoded_keys(const ik_step_t* s) {
    ik_fight_t* fight = s->fight;
    ik_fighter_t* f = s->f;
    const ik_fight_controls_t* controls = s->controls;
    if (!controls || f->hitstun != 0u) return 0;
    if (ikf_dispatch_controlled_input(fight, f, controls)) return 1;
    if (s->is_dummy || !f->on_ground || is_attack_fighter(fight, f)) return 0;

    /* Any standing state with ctrl may start walking, as upstream's hardcoded
     * keys do. Once inside a compiled common state its controllers/physics
     * own the state lifetime; the legacy fallback is only for missing data. */
    const ik_cns_state_t* spec = fighter_state_spec(fight, f);
    if (!spec) return 0;
    const int forward = controls->forward != 0;
    const int back = controls->back != 0;
    if (spec->assert_special_flags & IK_CNS_STATE_ASSERT_NO_WALK) return 0;
    if (f->state != IK_STATE_WALK && f->ctrl &&
        f->cur_state_type == IK_CNS_STATE_STAND &&
        (forward != ((!f->in_guard_dist) && back)) &&
        ik_cns_find_state(s->native_cns, IK_STATE_WALK)) {
        ikf_enter_state(fight, f, IK_STATE_WALK);
        return 1;
    }
    if (f->state == IK_STATE_WALK && back == forward) {
        ikf_enter_state(fight, f, IK_STATE_IDLE);
        return 1;
    }
    return 0;
}

/* Controllers, input and physics of the state the fighter is in right now. */
static int run_state_tick(ik_step_t* s, int first_pass) {
    ik_fight_t* fight = s->fight;
    ik_fighter_t* f = s->f;

    if (f->state == IK_STATE_KO) return IKF_STEP_DONE;
    if (first_pass) {
        drop_stale_binder(fight, f);
        if (dispatch_hardcoded_keys(s)) return IKF_STEP_RERUN;
        /* Upstream starts guarding right after state -1, before the
         * current state's own controllers run. */
        if (enter_guard_stance(s)) return IKF_STEP_RERUN;
    }
    if (ikf_process_cns_controllers(
            fight, f, s->controls, s->frames, 0)) {
        return IKF_STEP_RERUN;
    }

    /* TargetBind owns the bound player's transform. Physics=N thrown states
     * must not drift after either a root or Helper bind positioned them. */
    if (f->bound_to >= 0 || ik_entity_handle_is_valid(f->bound_entity)) {
        return IKF_STEP_DONE;
    }

    s->entries_at_physics = f->state_entries;
    if (f->hitstun > 0u) return step_hitstun(s);
    if (is_attack_fighter(fight, f)) return step_attack(s);
    if (!f->on_ground) return step_airborne(s);
    return step_grounded(s);
}

/* A state the physics entered (landing, hitstun end, animation end) is
 * initialised at once and its controllers run this tick, but it does not get
 * physics of its own until the next tick. */
static void run_controllers_of_physics_entries(ik_step_t* s) {
    for (int nest = 0; nest < IKF_MAX_STATE_CHANGES_PER_TICK &&
                       s->f->state_entries != s->entries_at_physics; ++nest) {
        s->entries_at_physics = s->f->state_entries;
        (void)ikf_process_cns_controllers(
            s->fight, s->f, s->controls, s->frames, 0);
    }
}

static void step_fighter_body(
    ik_fight_t* fight,
    int index,
    const ik_fight_controls_t* controls,
    int is_dummy,
    const ik_frame_table_t* frames,
    const ik_frame_table_t* foe_frames
) {
    ik_fighter_t* f = &fight->fighters[index];
    const ik_fighter_t* foe = &fight->fighters[index ^ 1];
    const ik_frame_table_t* owned_frames = frames_for_fighter(fight, f);
    const ik_frame_table_t* owned_foe_frames = frames_for_fighter(fight, foe);
    if (owned_frames) frames = owned_frames;
    if (owned_foe_frames) foe_frames = owned_foe_frames;

    f->gravity_carry = 0u;
    f->gravity_carry_q16 = 0;
    ik_step_t step = {
        fight, f, foe, index, controls, is_dummy, frames, foe_frames,
        constants_for_fighter(fight, f),
        cns_for_owner(fight, f->owner_player),
        f->state_entries
    };
    /* State -1 runs once per tick: after a ChangeState only the new state's
     * own controllers and input handling run, so its request is consumed. */
    ik_fight_controls_t follow_up = {0};
    if (controls) {
        follow_up = *controls;
        follow_up.has_state_request = 0u;
    }

    if (f->pending_power != 0) {
        int power = (int)f->power + f->pending_power;
        f->pending_power = 0;
        f->power = (int16_t)(power < 0 ? 0 : power > IK_MAX_POWER
                                                  ? IK_MAX_POWER : power);
    }
    tick_effect_timers(f, controls);
    reset_body_size(f, step.c);
    reset_screen_flags(f);
    if (f->snap_flags != 0u && f->cur_move_type == IK_CNS_MOVE_HIT) {
        if (f->snap_flags & 1u) f->x_q8 += f->snap_x_q8;
        if (f->snap_flags & 2u) f->y_q8 += f->snap_y_q8;
        f->snap_flags = 0u;
        sync_position(f);
    }
    if (f->pending_damage != 0) {
        f->hp = (int16_t)(f->hp > f->pending_damage
                              ? f->hp - f->pending_damage : 0);
        f->pending_damage = 0;
    }

    f->frozen_tick = f->hit_pause > 0u;
    if (f->hit_pause > 0u) {
        (void)ikf_process_cns_controllers(fight, f, controls, frames, 1);
        f->hit_pause--;
        if (f->hit_shake_time > 0u) --f->hit_shake_time;
        step_palfx(f);
        return;
    }

    if (f->statedef_pending) ikf_init_statedef(fight, f);
    update_facing_and_fall_time(fight, f, foe);

    for (int pass = 0; pass < IKF_MAX_STATE_CHANGES_PER_TICK; ++pass) {
        if ((run_state_tick(&step, pass == 0) & IKF_STEP_RERUN) == 0) break;
        step.controls = controls ? &follow_up : 0;
    }

    run_controllers_of_physics_entries(&step);
    f->state_time++;
    if (f->move_contact_time > 0u) f->move_contact_time++;
    if (f->hit_shake_time > 0u) --f->hit_shake_time;
    step_palfx(f);
    /* anim_time advances after contacts (ikf_finish_tick). */
    f->anim_clock_pending = 1u;
}

void ikf_step_fighter(
    ik_fight_t* fight,
    int index,
    const ik_fight_controls_t* controls,
    int is_dummy,
    const ik_frame_table_t* frames,
    const ik_frame_table_t* foe_frames
) {
    if (!fight) return;
    step_fighter_body(fight, index, controls, is_dummy, frames, foe_frames);
    /* A TargetBind lasts one tick of the bound fighter's own: it counts down
     * when that fighter's tick ends, so a victim that acts before its binder
     * still sees the bind the binder set last tick. */
    ik_fighter_t* f = &fight->fighters[index];
    if (f->bind_ticks > 0u && --f->bind_ticks == 0u && f->bound_to >= 0) {
        f->bound_to = -1;
    }
}

/* End of tick, after contacts: upstream steps animations after hit detection
 * (contacts see the frame the controllers evaluated with) and stamps a fresh
 * contact as MoveContactTime 1. Fighters that sat out the tick (hit pause)
 * keep their frame. */
void ikf_finish_tick(ik_fight_t* fight) {
    if (fight->entities) {
        ik_entity_runtime_t runtime;
        ik_entity_runtime_init(
            &runtime, fight->entities, fight->cns,
            fight->player_frames[0], fight->player_frames[1]);
        ik_entity_runtime_finish_tick(&runtime);
    }
    for (int i = 0; i < 2; ++i) {
        ik_fighter_t* f = &fight->fighters[i];
        if (f->move_contact && f->move_contact_time == 0u) {
            f->move_contact_time = 1u;
        }
        if (!f->anim_clock_pending) continue;
        f->anim_clock_pending = 0u;
        f->anim_time++;
    }
}
