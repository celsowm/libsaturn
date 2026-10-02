/* Frame orchestration: pause, round timer, KO and fight update. */
#include "ikemen_fight_internal.h"

typedef struct ik_frame_inputs {
    const ik_fight_controls_t* controls[2];
    const ik_frame_table_t* frames[2];
} ik_frame_inputs_t;

static void select_match_over_defeat_anim(
    ik_fighter_t* fighter,
    const ik_frame_table_t* frames
) {
    if (!fighter || !frames || fighter->state != 5150 ||
        fighter->anim < 5140 || fighter->anim > 5149) {
        return;
    }

    const int16_t match_anim = (int16_t)(fighter->anim + 10);
    uint32_t first = 0u;
    uint32_t count = 0u;
    if (!ik_frames_bounds(frames, match_anim, &first, &count) ||
        count == 0u) {
        return;
    }

    fighter->anim = match_anim;
    fighter->anim_time = 0u;
}

static void begin_frame(
    ik_fight_t* fight,
    const ik_frame_inputs_t* in
) {
    fight->player_frames[0] = in->frames[0];
    fight->player_frames[1] = in->frames[1];
    fight->effect_count = 0u;
    fight->sound_count = 0u;
    fight->events = IK_EVENT_NONE;
    fight->intro_asserted = 0u;
    ikf_advance_projectile_query_times(fight);
}

/* One entity-runtime pass with this frame's command masks. */
static void step_entities(
    ik_fight_t* fight,
    const ik_frame_inputs_t* in,
    int paused
) {
    if (!fight->entities) return;
    ikf_sync_player_entities(fight);
    ik_entity_runtime_t runtime;
    ik_entity_runtime_init(
        &runtime, fight->entities, fight->cns,
        in->frames[0], in->frames[1]);
    ikf_configure_fight_entity_runtime(fight, &runtime);
    ik_entity_runtime_set_command_mask(
        &runtime, 0u, controls_command_mask(in->controls[0]));
    ik_entity_runtime_set_command_mask(
        &runtime, 1u, controls_command_mask(in->controls[1]));

    if (!paused) {
        ik_entity_runtime_step(&runtime);
        return;
    }
    /* Entity pause/super-move budgets are independent from the fighter's
     * movetime. Both players' dynamic entities get a chance to consume their
     * authored budget during any pause. */
    ik_entity_runtime_step_paused(
        &runtime, 0u, fight->pause_is_super != 0u);
    ik_entity_runtime_step_paused(
        &runtime, 1u, fight->pause_is_super != 0u);
}

/* Round-over screen: hold until START (after the KO freeze) resets. */
static void update_round_over(
    ik_fight_t* fight,
    const ik_fight_controls_t* p1
) {
    if (p1 && p1->start && fight->ko_freeze == 0u) {
        ik_fight_reset(fight);
        return;
    }
    if (fight->ko_freeze > 0u) fight->ko_freeze--;
    fight->frame++;
}

/* After hit detection, paused or not: upstream drops a fighter that is not
 * in MoveType H (or is KO'd) from its attackers' target lists. A reversed
 * attacker is spared for one tick (hittmp = -1). */
static void exit_targets(ik_fight_t* fight) {
    for (int i = 0; i < 2; ++i) {
        ik_fighter_t* f = &fight->fighters[i];
        if (!f->reversed &&
            (f->cur_move_type != IK_CNS_MOVE_HIT || f->state == 5150)) {
            ikf_exit_target(fight, f);
        }
        /* The attacker's own list (upstream dropTargets). */
        if (f->drop_target_skip > 0u) {
            --f->drop_target_skip;
        } else if (f->target_index >= 0 && f->target_index < 2 &&
                   fight->fighters[(int)f->target_index].cur_move_type !=
                       IK_CNS_MOVE_HIT) {
            f->target_index = -1;
            f->target_id = -1;
        }
    }
}

static void release_pause_if_done(ik_fight_t* fight) {
    --fight->pause_time;
    if (fight->pause_time != 0u) return;
    fight->pause_owner = -1;
    fight->pause_move_time = 0u;
    fight->pause_end_cmd_buffer_time = 0u;
    fight->pause_is_super = 0u;
}

/* Pause/SuperPause: only the pause owner (while its movetime lasts) and the
 * entities with their own budgets act. */
static void update_paused(
    ik_fight_t* fight,
    const ik_frame_inputs_t* in
) {
    const int owner =
        fight->pause_owner >= 0 && fight->pause_owner < 2
            ? fight->pause_owner : -1;

    if (fight->pause_move_time > 0u && owner >= 0) {
        ikf_step_fighter(
            fight, owner, in->controls[owner],
            owner == 1 && in->controls[1] == 0,
            in->frames[owner], in->frames[owner ^ 1]);
        ikf_resolve_paused_owner_contact(
            fight, owner, in->controls[0], in->controls[1],
            in->frames[0], in->frames[1]);
        ikf_camera_step(fight);
        ikf_finish_tick(fight);
        --fight->pause_move_time;
    }

    if (fight->entities) {
        step_entities(fight, in, 1);
        if (!fight->round_over) {
            ikf_resolve_projectile_trades(fight, in->frames[0], in->frames[1]);
            ikf_resolve_entity_contacts(
                fight, in->controls[0], in->controls[1],
                in->frames[0], in->frames[1], -1);
        }
    }

    exit_targets(fight);
    release_pause_if_done(fight);
    fight->frame++;
}

/* Returns 1 when the round clock ran out this tick. */
static int tick_round_timer(ik_fight_t* fight) {
    if (fight->round_state < 2u || fight->timer_frames == 0u) return 0;
    if (--fight->timer_frames != 0u) return 0;
    fight->round_over = 1;
    fight->winner =
        (fight->fighters[0].hp >= fight->fighters[1].hp) ? 1u : 2u;
    fight->events |= IK_EVENT_ROUND_OVER;
    return 1;
}

static void advance_round_state(ik_fight_t* fight) {
    if (fight->round_state == 0u) {
        /* Give RoundState=0 one authored pre-intro tick, then let the
         * character's AssertSpecial Intro controller hold the intro. */
        fight->round_state = 1u;
    } else if (fight->round_state == 1u && !fight->intro_asserted) {
        if (++fight->round_wait >= IK_ROUND_FIGHT_WAIT_TICKS) {
            fight->round_state = 2u;
        }
    }
}

/* Projectile-vs-projectile cancellation happens first. Surviving
 * player/helper/projectile attacks then enter one deterministic
 * gather/arbitrate/apply queue, so priority and Hit/Miss/Dodge semantics are
 * not split by attacker representation. */
static void resolve_contacts(
    ik_fight_t* fight,
    const ik_frame_inputs_t* in
) {
    if (fight->round_over) return;
    ikf_resolve_projectile_trades(fight, in->frames[0], in->frames[1]);
    ikf_resolve_global_contacts(
        fight, in->controls[0], in->controls[1],
        in->frames[0], in->frames[1]);
}

static void check_knockout(
    ik_fight_t* fight,
    const ik_frame_inputs_t* in
) {
    if (fight->round_over) return;
    for (int i = 0; i < 2; ++i) {
        if (fight->fighters[i].hp > 0 || fight->fighters[i].state != 5150) {
            continue;
        }
        select_match_over_defeat_anim(&fight->fighters[i], in->frames[i]);
        fight->round_over = 1;
        fight->winner = (uint8_t)((i ^ 1) + 1);
        fight->events |= IK_EVENT_ROUND_OVER;
        return;
    }
}

static int run_priority(const ik_fighter_t* f) {
    if (f->cur_move_type == IK_CNS_MOVE_ATTACK) return 5;
    return f->cur_move_type == IK_CNS_MOVE_IDLE ? 4 : 3;
}

void ik_fight_update(ik_fight_t* fight,
                     const ik_fight_controls_t* p1,
                     const ik_fight_controls_t* p2,
                     const ik_frame_table_t* p1_frames,
                     const ik_frame_table_t* p2_frames) {
    if (!fight || !p1_frames) return;
    if (!p2_frames) p2_frames = p1_frames;
    const ik_frame_inputs_t in = {{p1, p2}, {p1_frames, p2_frames}};

    begin_frame(fight, &in);
    if (fight->round_over) {
        update_round_over(fight, p1);
        return;
    }
    if (fight->pause_time > 0u) {
        update_paused(fight, &in);
        return;
    }

    fight->frame++;
    if (tick_round_timer(fight)) return;

    /* Upstream runs attackers first, then idle players, then the rest (a
     * fighter in a get-hit state); equal priority runs P1 first. */
    const int first = run_priority(&fight->fighters[1]) >
                              run_priority(&fight->fighters[0]) ? 1 : 0;
    for (int n = 0; n < 2; ++n) {
        const int i = first ^ n;
        ikf_step_fighter(
            fight, i, i == 0 ? p1 : p2, i == 1 && p2 == 0,
            i == 0 ? p1_frames : p2_frames, i == 0 ? p2_frames : p1_frames);
    }
    ikf_push_fighters(fight, p1_frames, p2_frames);
    advance_round_state(fight);
    step_entities(fight, &in, 0);
    resolve_contacts(fight, &in);
    exit_targets(fight);
    ikf_camera_step(fight);
    ikf_finish_tick(fight);
    ikf_update_guard_dist(fight);
    check_knockout(fight, &in);
}
