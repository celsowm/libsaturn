#include "ikemen_fight.h"

static int16_t clamp16(int16_t v, int16_t lo, int16_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int32_t clamp_q8(int32_t v, int16_t lo, int16_t hi) {
    const int32_t qlo = (int32_t)lo * IK_CNS_Q8_ONE;
    const int32_t qhi = (int32_t)hi * IK_CNS_Q8_ONE;
    if (v < qlo) return qlo;
    if (v > qhi) return qhi;
    return v;
}

static const ik_cns_constants_t* constants_for(const ik_fight_t* fight) {
    return (fight && fight->cns) ? &fight->cns->constants : 0;
}

static void sync_position(ik_fighter_t* f) {
    if (!f) return;
    f->x = ik_cns_q8_to_int(f->x_q8);
    f->y = ik_cns_q8_to_int(f->y_q8);
}

static void set_position(ik_fighter_t* f, int16_t x, int16_t y) {
    f->x = x;
    f->y = y;
    f->x_q8 = (int32_t)x * IK_CNS_Q8_ONE;
    f->y_q8 = (int32_t)y * IK_CNS_Q8_ONE;
}

static const ik_cns_state_t* state_spec(const ik_fight_t* fight,
                                        int16_t state) {
    return ik_cns_find_state(fight ? fight->cns : 0, state);
}

static int is_attack_state(const ik_fight_t* fight, int16_t state) {
    const ik_cns_state_t* spec = state_spec(fight, state);
    return spec && spec->move_type == IK_CNS_MOVE_ATTACK;
}

static int is_crouch_attack(const ik_fight_t* fight, int16_t state) {
    const ik_cns_state_t* spec = state_spec(fight, state);
    return spec && spec->move_type == IK_CNS_MOVE_ATTACK &&
           spec->state_type == IK_CNS_STATE_CROUCH;
}

static int is_air_attack(const ik_fight_t* fight, int16_t state) {
    const ik_cns_state_t* spec = state_spec(fight, state);
    return spec && spec->move_type == IK_CNS_MOVE_ATTACK &&
           spec->state_type == IK_CNS_STATE_AIR;
}

static int is_active_guard_state(int16_t state) {
    return state == 120 || state == 130 || state == 131 || state == 132;
}

static uint8_t guard_type_for(const ik_fight_t* fight,
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

uint8_t ik_fight_state_type(const ik_fight_t* fight,
                            const ik_fighter_t* fighter) {
    if (!fighter) return IK_CNS_STATE_UNCHANGED;
    const ik_cns_state_t* spec = state_spec(fight, fighter->state);
    if (spec && spec->state_type != IK_CNS_STATE_UNCHANGED) {
        return (uint8_t)spec->state_type;
    }
    if (!fighter->on_ground) return IK_CNS_STATE_AIR;
    if (fighter->state == IK_STATE_CROUCH) return IK_CNS_STATE_CROUCH;
    return IK_CNS_STATE_STAND;
}

int ik_action_for_state(const ik_cns_asset_t* cns, int16_t state) {
    const ik_cns_state_t* spec = ik_cns_find_state(cns, state);
    if (spec) return spec->anim;

    switch (state) {
        case IK_STATE_WALK: return 20;
        case IK_STATE_CROUCH: return 11;
        case IK_STATE_JUMP: return 41;
        case IK_STATE_HIT: return 105;
        case IK_STATE_KO: return 120;
        case IK_STATE_GUARD: return 130;
        default: return 0;
    }
}

static int default_ctrl_for_state(int16_t state) {
    return state == IK_STATE_IDLE || state == IK_STATE_WALK ||
           state == IK_STATE_CROUCH || state == IK_STATE_JUMP;
}

static void enter_state(ik_fight_t* fight, ik_fighter_t* f, int16_t state) {
    const ik_cns_state_t* spec = ik_cns_find_state(fight ? fight->cns : 0, state);
    const int16_t previous_anim = f->anim;
    const int16_t previous_state = f->state;

    f->prev_state = previous_state;
    f->state = state;
    f->state_time = 0u;
    f->anim = (spec && spec->anim < 0)
        ? previous_anim
        : (int16_t)ik_action_for_state(fight ? fight->cns : 0, state);
    f->anim_time = 0u;
    f->move_contact = 0u;
    if (!(spec && spec->hitdef_persist)) {
        f->hitdef_hit_mask = 0u;
        f->active_hitdef_local = -1;
        f->active_hitdef_global = -1;
    }
    f->state_axis = 0;

    if (is_attack_state(fight, state)) ++f->attack_id;

    if (spec) {
        int power = (int)f->power + spec->power_add;
        if (power < 0) power = 0;
        if (power > IK_MAX_POWER) power = IK_MAX_POWER;
        f->power = (int16_t)power;
        f->ctrl = spec->ctrl;
        f->spr_priority = spec->spr_priority;
        if (spec->state_type == IK_CNS_STATE_AIR) {
            f->on_ground = 0;
        } else if (spec->state_type == IK_CNS_STATE_STAND ||
                   spec->state_type == IK_CNS_STATE_CROUCH ||
                   spec->state_type == IK_CNS_STATE_LIEDOWN) {
            f->on_ground = 1;
        }
        if (spec->has_velset) {
            f->vx_q8 = spec->velset_x_q8;
            f->vy_q8 = spec->velset_y_q8;
        }
    } else {
        f->ctrl = (int8_t)default_ctrl_for_state(state);
        f->spr_priority = 0;
    }
}

static void fighter_spawn(ik_fight_t* fight, ik_fighter_t* f,
                          int16_t x, int8_t facing, int hp) {
    const ik_cns_constants_t* c = constants_for(fight);
    set_position(f, x, IK_FLOOR_Y);
    f->vx_q8 = 0;
    f->vy_q8 = 0;
    f->facing = facing;
    f->on_ground = 1;
    f->state = IK_STATE_IDLE;
    f->prev_state = IK_STATE_IDLE;
    f->state_time = 0;
    f->anim = 0;
    f->anim_time = 0;
    f->state_axis = 0;
    f->air_jumps_used = 0u;
    f->up_latched = 0u;
    f->ctrl = 1;
    f->spr_priority = 0;
    f->hp = (int16_t)hp;
    f->power = 0;
    f->hitstun = 0;
    f->hit_pause = 0;
    f->hit_slide_time = 0;
    f->hit_ctrl_time = 0;
    f->gethit_vx_q8 = 0;
    f->gethit_vy_q8 = 0;
    f->gethit_yaccel_q8 = 0;
    f->gethit_ground_type = IK_CNS_GROUND_NORMAL;
    f->gethit_anim_type = 0u;
    f->gethit_fall = 0u;
    f->gethit_fall_x_q8 = 0;
    f->gethit_fall_y_q8 = -1152;
    f->gethit_fall_x_set = 0u;
    f->gethit_fall_recover = 1u;
    f->gethit_fall_recover_time = 4u;
    f->fall_time = 0u;
    f->juggle_points =
        (int16_t)((c && c->air_juggle > 0) ? c->air_juggle : 15);
    f->guard_type = IK_CNS_STATE_STAND;
    f->push_back = c ? c->ground_back : 15;
    f->push_front = c ? c->ground_front : 16;
    f->body_height = c ? c->height : 60;
    f->hitdef_hit_mask = 0u;
    f->attack_id = 0;
    f->move_contact = 0;
    f->active_hitdef_local = -1;
    f->active_hitdef_global = -1;
    f->pos_freeze_x = 0u;
    f->pos_freeze_y = 0u;
    f->target_index = -1;
    f->bound_to = -1;
}

int ik_fight_max_hp(const ik_fight_t* fight) {
    if (fight && fight->cns && fight->cns->constants.life > 0) {
        return fight->cns->constants.life;
    }
    return IK_MAX_HP;
}

void ik_fight_init(ik_fight_t* fight, const ik_cns_asset_t* cns) {
    if (!fight) return;
    fight->cns = cns;
    const int hp = ik_fight_max_hp(fight);
    fighter_spawn(fight, &fight->fighters[0], 110, 1, hp);
    fighter_spawn(fight, &fight->fighters[1], 210, -1, hp);
    fight->frame = 0;
    fight->timer_frames = IK_ROUND_TIME_FRAMES;
    fight->events = IK_EVENT_NONE;
    fight->round_over = 0;
    fight->winner = 0;
    fight->hits_p1 = 0;
    fight->hits_p2 = 0;
    fight->ko_freeze = 0;
    fight->effect_count = 0u;
}

void ik_fight_reset(ik_fight_t* fight) {
    if (!fight) return;
    const uint32_t h1 = fight->hits_p1;
    const uint32_t h2 = fight->hits_p2;
    const ik_cns_asset_t* cns = fight->cns;
    ik_fight_init(fight, cns);
    fight->hits_p1 = h1;
    fight->hits_p2 = h2;
    fight->events = IK_EVENT_RESET;
}

int ik_body_half_w(const ik_fighter_t* f) {
    if (!f) return 16;
    return (f->push_front > f->push_back) ? f->push_front : f->push_back;
}

int ik_body_h(const ik_fighter_t* f) {
    return f ? f->body_height : 60;
}

void ik_body_box(const ik_fighter_t* f, int* l, int* t, int* r, int* b) {
    if (!f) return;
    int left;
    int right;
    if (f->facing >= 0) {
        left = (int)f->x - f->push_back;
        right = (int)f->x + f->push_front;
    } else {
        left = (int)f->x - f->push_front;
        right = (int)f->x + f->push_back;
    }
    if (l) *l = left;
    if (t) *t = (int)f->y - f->body_height;
    if (r) *r = right;
    if (b) *b = (int)f->y;
}

int ik_boxes_overlap(int l0, int t0, int r0, int b0,
                     int l1, int t1, int r1, int b1) {
    return (l0 < r1) && (l1 < r0) && (t0 < b1) && (t1 < b0);
}

static const ik_frame_t* fighter_frame(const ik_frame_table_t* frames,
                                       const ik_fighter_t* fighter) {
    if (!frames || !fighter) return 0;
    return ik_frame_at_time(frames, fighter->anim, fighter->anim_time);
}

static void anim_position(const ik_frame_table_t* frames,
                          const ik_fighter_t* fighter,
                          uint16_t* out_element,
                          uint16_t* out_element_time,
                          int* out_ended) {
    uint32_t first = 0u;
    uint32_t count = 0u;
    uint16_t element = 1u;
    uint16_t element_time = 0u;
    int ended = 0;

    if (!frames || !fighter ||
        !ik_frames_bounds(frames, fighter->anim, &first, &count) ||
        count == 0u) {
        if (out_element) *out_element = element;
        if (out_element_time) *out_element_time = element_time;
        if (out_ended) *out_ended = 0;
        return;
    }

    const uint32_t duration = ik_action_duration_ticks(frames, fighter->anim);
    if (duration > 0u && fighter->anim_time >= duration) ended = 1;

    uint32_t remaining = fighter->anim_time;
    if (duration > 0u && remaining >= duration) remaining = duration - 1u;

    for (uint32_t i = 0u; i < count; ++i) {
        const uint16_t ticks = ik_frame_ticks(&frames->frames[first + i]);
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

static uint16_t anim_element_start_tick(const ik_frame_table_t* frames,
                                        int16_t action,
                                        uint16_t element) {
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

static int fighter_clsn_overlap(
    const ik_frame_table_t* attacker_frames,
    const ik_frame_table_t* victim_frames,
    const ik_fighter_t* attacker,
    const ik_fighter_t* victim
) {
    const ik_frame_t* af = fighter_frame(attacker_frames, attacker);
    const ik_frame_t* vf = fighter_frame(victim_frames, victim);
    if (!af || !vf || af->clsn1_count == 0u || vf->clsn2_count == 0u) return 0;

    for (uint16_t ai = 0u; ai < af->clsn1_count; ++ai) {
        int al, at, ar, ab;
        if (!ik_frame_clsn_world(
                attacker_frames, af, IK_CLSN_ATTACK, ai,
                attacker->x, attacker->y, attacker->facing,
                &al, &at, &ar, &ab)) continue;
        for (uint16_t vi = 0u; vi < vf->clsn2_count; ++vi) {
            int vl, vt, vr, vb;
            if (!ik_frame_clsn_world(
                    victim_frames, vf, IK_CLSN_HURT, vi,
                    victim->x, victim->y, victim->facing,
                    &vl, &vt, &vr, &vb)) continue;
            if (ik_boxes_overlap(al, at, ar, ab, vl, vt, vr, vb)) return 1;
        }
    }
    return 0;
}

static int body_dist_x(const ik_fighter_t* attacker,
                       const ik_fighter_t* victim) {
    if (!attacker || !victim) return 32767;
    int value =
        ((int)victim->x - (int)attacker->x) * (int)attacker->facing -
        attacker->push_front - victim->push_front;
    if (value < -32768) value = -32768;
    if (value > 32767) value = 32767;
    return value;
}

static int hitdef_p2_dist_allows(const ik_cns_hitdef_t* hitdef,
                                 int p2_body_dist_x) {
    if (!hitdef) return 0;
    switch ((ik_cns_p2_dist_op_t)hitdef->p2_body_dist_op) {
        case IK_CNS_P2_DIST_LT:
            return p2_body_dist_x < hitdef->p2_body_dist_x;
        case IK_CNS_P2_DIST_LE:
            return p2_body_dist_x <= hitdef->p2_body_dist_x;
        case IK_CNS_P2_DIST_GT:
            return p2_body_dist_x > hitdef->p2_body_dist_x;
        case IK_CNS_P2_DIST_GE:
            return p2_body_dist_x >= hitdef->p2_body_dist_x;
        case IK_CNS_P2_DIST_NONE:
        default:
            return 1;
    }
}

static const ik_cns_hitdef_t* active_hitdef(ik_fight_t* fight,
                                            const ik_frame_table_t* frames,
                                            ik_fighter_t* fighter,
                                            const ik_fighter_t* victim,
                                            uint8_t* out_local_index) {
    if (!fight || !fight->cns || !fighter ||
        !is_attack_state(fight, fighter->state)) {
        return 0;
    }

    const ik_cns_state_t* state =
        ik_cns_find_state(fight->cns, fighter->state);
    if (!state || !fight->cns->hitdefs) return 0;

    if (state->hitdef_count == 0u) {
        if (state->hitdef_persist &&
            fighter->active_hitdef_global >= 0 &&
            fighter->active_hitdef_global < (int16_t)fight->cns->hitdef_count) {
            if (out_local_index) {
                *out_local_index = (uint8_t)(
                    fighter->active_hitdef_local < 0
                        ? 0
                        : fighter->active_hitdef_local);
            }
            return &fight->cns->hitdefs[
                (uint16_t)fighter->active_hitdef_global];
        }
        return 0;
    }

    uint16_t element = 1u;
    uint16_t element_time = 0u;
    int anim_ended = 0;
    anim_position(
        frames, fighter, &element, &element_time, &anim_ended);
    const int p2_dist = body_dist_x(fighter, victim);

    /* HitDef controllers execute in source order. A HitDef that triggers on
     * this tick replaces the current one and then remains active until another
     * HitDef fires or the state changes. */
    for (uint8_t i = 0u; i < state->hitdef_count; ++i) {
        const uint16_t global = (uint16_t)(state->hitdef_ofs + i);
        if (global >= fight->cns->hitdef_count) break;
        const ik_cns_hitdef_t* hitdef = &fight->cns->hitdefs[global];
        const int primary_now = ik_cns_trigger_now(
            hitdef->trigger_kind, hitdef->trigger_value,
            fighter->state_time, element, element_time, anim_ended);
        const int secondary_now =
            hitdef->has_trigger2 &&
            ik_cns_trigger_now(
                hitdef->trigger2_kind, hitdef->trigger2_value,
                fighter->state_time, element, element_time, anim_ended);
        if (!primary_now && !secondary_now) continue;
        if (!hitdef_p2_dist_allows(hitdef, p2_dist)) continue;

        /* A second trigger on the same HitDef controller is an intentional
         * re-activation (Fast Upper). Re-arm only this controller's hit bit. */
        if (secondary_now && i < 32u) {
            fighter->hitdef_hit_mask &= ~(1u << i);
        }
        fighter->active_hitdef_local = (int8_t)i;
        fighter->active_hitdef_global = (int16_t)global;
    }

    if (fighter->active_hitdef_global < 0 ||
        fighter->active_hitdef_global >= (int16_t)fight->cns->hitdef_count) {
        return 0;
    }
    if (out_local_index) {
        *out_local_index = (uint8_t)(
            fighter->active_hitdef_local < 0
                ? 0
                : fighter->active_hitdef_local);
    }
    return &fight->cns->hitdefs[
        (uint16_t)fighter->active_hitdef_global];
}

static int hitdef_allows_target(const ik_fight_t* fight,
                                const ik_fighter_t* victim,
                                const ik_cns_hitdef_t* hitdef) {
    if (!victim || !hitdef) return 0;

    const uint8_t flags = hitdef->hit_flags != 0u
        ? hitdef->hit_flags
        : IK_CNS_HIT_DEFAULT;
    const ik_cns_state_t* spec = state_spec(fight, victim->state);
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

static int juggle_cost(const ik_fight_t* fight,
                       const ik_fighter_t* attacker,
                       const ik_cns_hitdef_t* hitdef) {
    int cost = hitdef ? hitdef->air_juggle : 0;
    const ik_cns_state_t* state =
        attacker ? state_spec(fight, attacker->state) : 0;
    if (state && state->has_juggle && state->juggle > 0) {
        cost += state->juggle;
    }
    return cost < 0 ? 0 : cost;
}

static int is_juggle_target(const ik_fight_t* fight,
                            const ik_fighter_t* victim) {
    if (!victim) return 0;
    return victim->gethit_fall ||
           ik_fight_state_type(fight, victim) == IK_CNS_STATE_LIEDOWN;
}

static int juggle_allows_target(const ik_fight_t* fight,
                                const ik_fighter_t* attacker,
                                const ik_fighter_t* victim,
                                const ik_cns_hitdef_t* hitdef) {
    if (!is_juggle_target(fight, victim)) return 1;
    return juggle_cost(fight, attacker, hitdef) <= victim->juggle_points;
}

static int guard_threat(ik_fight_t* fight,
                        const ik_frame_table_t* attacker_frames,
                        int victim,
                        const ik_fight_controls_t* controls) {
    if (!fight || !fight->cns) return 0;
    const ik_fighter_t* v = &fight->fighters[victim];
    ik_fighter_t* a = &fight->fighters[victim ^ 1];
    const ik_cns_hitdef_t* hitdef =
        active_hitdef(fight, attacker_frames, a, v, 0);
    if (!hitdef || hitdef->guard_flags == 0u ||
        !hitdef_allows_target(fight, v, hitdef)) return 0;

    int dx = (int)a->x - (int)v->x;
    if (dx < 0) dx = -dx;
    if (dx > fight->cns->constants.attack_dist) return 0;

    const uint8_t type = guard_type_for(fight, v, controls);
    return (hitdef->guard_flags & guard_mask_for_type(type)) != 0u;
}

static int can_guard_hit(const ik_fight_t* fight,
                         const ik_fighter_t* victim,
                         const ik_fight_controls_t* controls,
                         const ik_cns_hitdef_t* hitdef) {
    if (!fight || !victim || !hitdef || hitdef->guard_flags == 0u) return 0;
    if ((!controls || !controls->back) &&
        !is_active_guard_state(victim->state)) {
        return 0;
    }
    const uint8_t type = guard_type_for(fight, victim, controls);
    return (hitdef->guard_flags & guard_mask_for_type(type)) != 0u;
}

static void queue_hit_effect(ik_fight_t* fight,
                             const ik_fighter_t* attacker,
                             const ik_fighter_t* victim,
                             const ik_cns_hitdef_t* hitdef,
                             int16_t action);

static void apply_guard(ik_fight_t* fight, int victim,
                        const ik_fight_controls_t* controls,
                        const ik_cns_hitdef_t* hitdef) {
    ik_fighter_t* v = &fight->fighters[victim];
    ik_fighter_t* a = &fight->fighters[victim ^ 1];
    const uint8_t type = guard_type_for(fight, v, controls);

    int guard_ko = 0;
    if (hitdef->guard_damage > 0) {
        v->hp = (int16_t)(v->hp - hitdef->guard_damage);
        if (v->hp <= 0) {
            if (hitdef->guard_kill) {
                v->hp = 0;
                guard_ko = 1;
            } else {
                v->hp = 1;
            }
        }
    }

    v->hit_pause = hitdef->pause_p2;
    a->hit_pause = hitdef->pause_p1;
    v->hitstun = hitdef->guard_hit_time;
    v->hit_slide_time = hitdef->guard_slide_time;
    v->hit_ctrl_time = hitdef->guard_ctrl_time;
    v->guard_type = type;

    if (type == IK_CNS_STATE_AIR) {
        v->gethit_vx_q8 = hitdef->air_guard_velocity_x_q8;
        v->gethit_vy_q8 = hitdef->air_guard_velocity_y_q8;
    } else {
        v->gethit_vx_q8 = hitdef->guard_velocity_x_q8;
        v->gethit_vy_q8 = 0;
    }

    a->move_contact = 1u;

    int16_t state = 150;
    if (type == IK_CNS_STATE_CROUCH) state = 152;
    else if (type == IK_CNS_STATE_AIR) state = 154;

    if (ik_cns_find_state(fight->cns, state)) {
        enter_state(fight, v, state);
    } else {
        enter_state(fight, v,
                    type == IK_CNS_STATE_CROUCH ? 131 :
                    type == IK_CNS_STATE_AIR ? 132 : 130);
    }

    queue_hit_effect(fight, a, v, hitdef, 40);
    fight->events |= IK_EVENT_GUARD;
    if (guard_ko) {
        fight->winner = (uint8_t)((victim ^ 1) + 1);
        fight->events |= IK_EVENT_KO;
        if (!ik_cns_find_state(fight->cns, 5050)) {
            enter_state(fight, v, IK_STATE_KO);
            fight->round_over = 1;
            fight->events |= IK_EVENT_ROUND_OVER;
            fight->ko_freeze = IK_KO_FREEZE_FRAMES;
        }
    }
}

static void apply_throw(ik_fight_t* fight, int attacker,
                        const ik_fight_controls_t* attacker_controls,
                        const ik_cns_hitdef_t* hitdef) {
    if (!fight || !hitdef || attacker < 0 || attacker > 1) return;
    const int victim = attacker ^ 1;
    ik_fighter_t* a = &fight->fighters[attacker];
    ik_fighter_t* v = &fight->fighters[victim];

    a->target_index = (int8_t)victim;
    v->bound_to = (int8_t)attacker;
    a->move_contact = 1u;

    if (hitdef->p1_facing != 0) {
        const int8_t toward = v->x >= a->x ? 1 : -1;
        a->facing = hitdef->p1_facing > 0 ? toward : (int8_t)-toward;
    }
    if (hitdef->p2_facing != 0) {
        const int8_t toward = a->x >= v->x ? 1 : -1;
        v->facing = hitdef->p2_facing > 0 ? toward : (int8_t)-toward;
    }
    v->gethit_fall =
        (uint8_t)((hitdef->flags & IK_CNS_HITDEF_FALL) != 0u);
    v->gethit_fall_x_q8 = hitdef->fall_x_velocity_q8;
    v->gethit_fall_y_q8 = hitdef->fall_y_velocity_q8;
    v->gethit_fall_x_set = hitdef->fall_x_velocity_set;
    v->gethit_fall_recover = hitdef->fall_recover;
    v->gethit_fall_recover_time = hitdef->fall_recover_time;

    if (hitdef->p2_state_no >= 0) {
        enter_state(fight, v, hitdef->p2_state_no);
    }
    if (hitdef->p1_state_no >= 0) {
        enter_state(fight, a, hitdef->p1_state_no);
        if (hitdef->p1_spr_priority != -128) {
            a->spr_priority = hitdef->p1_spr_priority;
        }
        /* KFM state 810 snapshots command="holdfwd" at Time=0. The throw
         * changes state during collision resolution, so preserve that entry
         * input in the generic state-axis scratch immediately. */
        if (attacker_controls) {
            if (attacker_controls->forward) a->state_axis = 1;
            else if (attacker_controls->back) a->state_axis = -1;
        }
    }

    queue_hit_effect(fight, a, v, hitdef, hitdef->spark_no);
    fight->events |= IK_EVENT_HIT;
    if (attacker == 0) ++fight->hits_p1;
    else ++fight->hits_p2;
}

static void queue_hit_effect(ik_fight_t* fight,
                             const ik_fighter_t* attacker,
                             const ik_fighter_t* victim,
                             const ik_cns_hitdef_t* hitdef,
                             int16_t action) {
    if (!fight || !attacker || !victim || !hitdef || action < 0 ||
        fight->effect_count >= IK_MAX_EFFECT_EVENTS) return;
    ik_effect_event_t* effect =
        &fight->effect_events[fight->effect_count++];
    effect->action = action;
    effect->x = (int16_t)(
        victim->x + (int16_t)attacker->facing * hitdef->spark_x);
    /* MUGEN sparkxy: X is relative to P2, Y is relative to P1. */
    effect->y = (int16_t)(attacker->y + hitdef->spark_y);
}

static void release_bound_target(ik_fight_t* fight, int owner) {
    if (!fight || owner < 0 || owner > 1) return;
    ik_fighter_t* f = &fight->fighters[owner];
    if (f->target_index < 0 || f->target_index > 1) return;
    ik_fighter_t* target = &fight->fighters[(int)f->target_index];
    if (target->bound_to == owner) target->bound_to = -1;
    f->target_index = -1;
}

static void apply_damage(ik_fight_t* fight, int victim,
                         const ik_cns_hitdef_t* hitdef) {
    ik_fighter_t* v = &fight->fighters[victim];
    /* Losing a throw owner releases its bound target. State 820's compiled
     * !isbound SelfState then returns the target to its own fall graph. */
    release_bound_target(fight, victim);
    ik_fighter_t* a = &fight->fighters[victim ^ 1];

    const uint8_t victim_type = ik_fight_state_type(fight, v);
    const int downed = victim_type == IK_CNS_STATE_LIEDOWN;
    const int airborne = !v->on_ground || victim_type == IK_CNS_STATE_AIR;
    const int16_t velocity_x = downed
        ? hitdef->down_velocity_x_q8
        : airborne
            ? hitdef->air_velocity_x_q8
            : hitdef->ground_velocity_x_q8;
    const int16_t velocity_y = downed
        ? hitdef->down_velocity_y_q8
        : airborne
            ? hitdef->air_velocity_y_q8
            : hitdef->ground_velocity_y_q8;
    const int16_t hit_time = downed
        ? (velocity_y == 0
            ? (int16_t)hitdef->down_hit_time
            : (int16_t)hitdef->air_hit_time)
        : airborne
            ? (int16_t)hitdef->air_hit_time
            : (int16_t)hitdef->ground_hit_time;
    const int was_juggle_target = is_juggle_target(fight, v);
    const int attack_juggle = juggle_cost(fight, a, hitdef);
    const int downed_launch = downed && velocity_y != 0;
    const int launch = airborne || downed_launch ||
        (hitdef->flags & IK_CNS_HITDEF_FALL) != 0u ||
        velocity_y != 0;

    int damage = hitdef->damage;
    if (hitdef->has_alt_damage &&
        a->prev_state == hitdef->alt_damage_prev_state) {
        damage = hitdef->alt_damage;
    }
    v->hp = (int16_t)(v->hp - damage);
    v->hitstun = (uint16_t)(hit_time < 0 ? 0 : hit_time);
    v->hit_pause = hitdef->pause_p2;
    v->hit_slide_time = downed && velocity_y == 0
        ? hitdef->down_hit_time
        : hitdef->ground_slide_time;
    v->hit_ctrl_time = (uint16_t)(hit_time < 0 ? 0 : hit_time);
    v->gethit_vx_q8 = velocity_x;
    v->gethit_vy_q8 = velocity_y;
    v->gethit_yaccel_q8 = hitdef->yaccel_q8;
    v->gethit_ground_type = hitdef->ground_type;
    v->gethit_anim_type = airborne
        ? hitdef->air_anim_type
        : hitdef->anim_type;
    /* A liedown victim launched by down.velocity always enters the fall
     * graph so it returns to a downed state on landing. down.bounce only
     * controls whether state 5100 receives a non-zero fall Y velocity and
     * therefore proceeds through the single 5101 ground bounce. */
    v->gethit_fall = (uint8_t)(
        ((hitdef->flags & IK_CNS_HITDEF_FALL) != 0u) ||
        (airborne &&
         (hitdef->flags & IK_CNS_HITDEF_AIR_FALL) != 0u) ||
        downed_launch);
    v->gethit_fall_x_q8 = hitdef->fall_x_velocity_q8;
    v->gethit_fall_y_q8 =
        (downed_launch && !hitdef->down_bounce)
            ? 0
            : hitdef->fall_y_velocity_q8;
    v->gethit_fall_x_set =
        (uint8_t)(hitdef->fall_x_velocity_set &&
                  (!downed_launch || hitdef->down_bounce));
    v->gethit_fall_recover = hitdef->fall_recover;
    v->gethit_fall_recover_time = hitdef->fall_recover_time;
    v->fall_time = 0u;

    if (was_juggle_target) {
        v->juggle_points = (int16_t)(
            v->juggle_points > attack_juggle
                ? v->juggle_points - attack_juggle
                : 0);
    } else if ((hitdef->flags & IK_CNS_HITDEF_FALL) != 0u) {
        const ik_cns_constants_t* c = constants_for(fight);
        const int initial = (c && c->air_juggle > 0) ? c->air_juggle : 15;
        v->juggle_points =
            (int16_t)(initial > attack_juggle ? initial - attack_juggle : 0);
    }

    a->hit_pause = hitdef->pause_p1;

    if (launch) v->on_ground = 0;

    a->move_contact = 1u;

    if (!airborne && !downed &&
        hitdef->ground_cornerpush_veloff_q8 != 0) {
        int left=0, top=0, right=0, bottom=0;
        ik_body_box(v, &left, &top, &right, &bottom);
        if (left <= IK_STAGE_MIN_X || right >= IK_STAGE_MAX_X) {
            a->vx_q8 =
                (int32_t)a->facing * hitdef->ground_cornerpush_veloff_q8;
        }
    }

    {
        int16_t target = IK_STATE_HIT;
        if (hitdef->p2_state_no >= 0 &&
            ik_cns_find_state(fight->cns, hitdef->p2_state_no)) {
            target = hitdef->p2_state_no;
        } else if (victim_type == IK_CNS_STATE_LIEDOWN &&
            ik_cns_find_state(fight->cns, 5080)) {
            target = 5080;
        } else if (!airborne && hitdef->ground_type == IK_CNS_GROUND_TRIP &&
                   ik_cns_find_state(fight->cns, 5070)) {
            target = 5070;
        } else if (airborne && ik_cns_find_state(fight->cns, 5020)) {
            target = 5020;
        } else if (victim_type == IK_CNS_STATE_CROUCH &&
                   (hitdef->flags & IK_CNS_HITDEF_FORCE_STAND) == 0u &&
                   ik_cns_find_state(fight->cns, 5010)) {
            target = 5010;
        } else if (ik_cns_find_state(fight->cns, 5000)) {
            target = 5000;
        }

        if (hitdef->p2_facing != 0) {
            const int8_t toward = a->x >= v->x ? 1 : -1;
            v->facing =
                hitdef->p2_facing > 0 ? toward : (int8_t)-toward;
        }

        if (target != IK_STATE_HIT) {
            v->vx_q8 = 0;
            v->vy_q8 = 0;
        } else {
            v->vx_q8 = (int32_t)v->facing * velocity_x;
            v->vy_q8 = velocity_y;
        }

        queue_hit_effect(fight, a, v, hitdef, hitdef->spark_no);
        if (v->hp <= 0) {
            v->hp = 0;
            fight->winner = (uint8_t)((victim ^ 1) + 1);
            fight->events |= (uint16_t)(IK_EVENT_HIT | IK_EVENT_KO);
        } else {
            fight->events |= IK_EVENT_HIT;
        }

        if (target == IK_STATE_HIT && v->hp <= 0) {
            enter_state(fight, v, IK_STATE_KO);
            fight->round_over = 1;
            fight->events |= IK_EVENT_ROUND_OVER;
            fight->ko_freeze = IK_KO_FREEZE_FRAMES;
        } else {
            enter_state(fight, v, target);
        }
    }

    if (hitdef->p1_state_no >= 0 &&
        ik_cns_find_state(fight->cns, hitdef->p1_state_no)) {
        enter_state(fight, a, hitdef->p1_state_no);
    }

    if ((victim ^ 1) == 0) fight->hits_p1++;
    else fight->hits_p2++;
}

static void apply_ground_velocity(ik_fighter_t* f,
                                  const ik_cns_constants_t* c,
                                  int physics) {
    if (!f || !c) return;
    const int crouch = physics == IK_CNS_PHYS_CROUCH;
    const int16_t friction = crouch
        ? c->crouch_friction_q8
        : c->stand_friction_q8;
    const int16_t threshold = crouch
        ? c->crouch_friction_threshold_q8
        : c->stand_friction_threshold_q8;

    f->x_q8 += f->vx_q8;
    f->vx_q8 = (f->vx_q8 * friction) / IK_CNS_Q8_ONE;
    if (f->vx_q8 < threshold && f->vx_q8 > -threshold) {
        f->vx_q8 = 0;
    }
    f->x_q8 = clamp_q8(f->x_q8, IK_STAGE_MIN_X, IK_STAGE_MAX_X);
    sync_position(f);
}

static void step_air(ik_fight_t* fight, ik_fighter_t* f,
                     int allow_land_transition) {
    const ik_cns_constants_t* c = constants_for(fight);
    const ik_cns_state_t* spec = state_spec(fight, f->state);
    const int32_t gravity =
        (spec && spec->owns_air_accel)
            ? 0
            : (spec && spec->move_type == IK_CNS_MOVE_HIT &&
               f->gethit_yaccel_q8 != 0)
                ? f->gethit_yaccel_q8
                : (spec && spec->air_accel_q8 != 0)
                    ? spec->air_accel_q8
                    : (c ? c->yaccel_q8 : (IK_CNS_Q8_ONE / 2));
    const int32_t floor_q8 = (int32_t)IK_FLOOR_Y * IK_CNS_Q8_ONE;
    const int32_t land_level_q8 = spec ? spec->land_level_q8 : 0;

    if (spec && f->state_time < spec->air_motion_start) {
        sync_position(f);
        return;
    }

    f->vy_q8 += gravity;
    if (!f->pos_freeze_x) f->x_q8 += f->vx_q8;
    if (!f->pos_freeze_y) f->y_q8 += f->vy_q8;
    f->x_q8 = clamp_q8(f->x_q8, IK_STAGE_MIN_X, IK_STAGE_MAX_X);

    if (f->vy_q8 > 0 && f->y_q8 >= floor_q8 + land_level_q8) {
        const int hit_owned_landing =
            spec && spec->move_type == IK_CNS_MOVE_HIT &&
            (spec->land_state != 0 || f->state == 5030 ||
             f->state == 5035);
        if (allow_land_transition || hit_owned_landing) {
            int16_t target = IK_STATE_IDLE;
            f->y_q8 = floor_q8;
            f->vy_q8 = 0;
            f->on_ground = 1;
            f->air_jumps_used = 0u;

            if ((f->state == 5030 || f->state == 5035) &&
                ik_cns_find_state(fight ? fight->cns : 0,
                                  f->gethit_fall ? 5050 : 5040)) {
                target = f->gethit_fall ? 5050 : 5040;
            } else if (spec && spec->land_state != 0) {
                target = spec->land_state;
            } else if (ik_cns_find_state(fight ? fight->cns : 0, 52)) {
                target = 52;
            }
            const uint8_t landing_ctrl = spec ? spec->land_ctrl : 0u;
            enter_state(fight, f, target);
            if (target == 5040 || landing_ctrl) f->ctrl = 1;
        }
    }
    sync_position(f);
}

static int process_cns_controllers(ik_fight_t* fight, ik_fighter_t* f,
                                   const ik_fight_controls_t* controls,
                                   const ik_frame_table_t* frames,
                                   int hit_pause_only) {
    if (!fight || !fight->cns || !f) return 0;
    const ik_cns_state_t* state = ik_cns_find_state(fight->cns, f->state);
    if (!state || !fight->cns->controllers) return 0;

    uint16_t elem = 1u;
    uint16_t elem_time = 0u;
    int anim_ended = 0;
    anim_position(frames, f, &elem, &elem_time, &anim_ended);

    uint16_t command_mask = 0u;
    if (controls) {
        if (controls->forward) command_mask |= IK_CNS_COMMAND_HOLD_FWD;
        if (controls->back) command_mask |= IK_CNS_COMMAND_HOLD_BACK;
        if (controls->up) command_mask |= IK_CNS_COMMAND_HOLD_UP;
        if (controls->down) command_mask |= IK_CNS_COMMAND_HOLD_DOWN;
        if (controls->recovery) command_mask |= IK_CNS_COMMAND_RECOVERY;
        if (controls->a) command_mask |= IK_CNS_COMMAND_A;
        if (controls->b) command_mask |= IK_CNS_COMMAND_B;
    }
    const int back_body_dist =
        f->facing > 0
            ? (int)f->x - f->push_back - IK_STAGE_MIN_X
            : IK_STAGE_MAX_X - ((int)f->x + f->push_back);
    const int front_body_dist =
        f->facing > 0
            ? IK_STAGE_MAX_X - ((int)f->x + f->push_front)
            : (int)f->x - f->push_front - IK_STAGE_MIN_X;
    const int back_dist =
        f->facing > 0
            ? (int)f->x - IK_STAGE_MIN_X
            : IK_STAGE_MAX_X - (int)f->x;

    const ik_cns_controller_context_t context = {
        .state_time = f->state_time,
        .anim_element = elem,
        .anim_element_time = elem_time,
        .anim = f->anim,
        .vx_q8 = f->vx_q8,
        .vy_q8 = f->vy_q8,
        .y_q8 = f->y_q8,
        .floor_y_q8 = (int32_t)IK_FLOOR_Y * IK_CNS_Q8_ONE,
        .command_mask = command_mask,
        .hitstun = f->hitstun,
        .hit_pause = f->hit_pause,
        .hit_slide_time = f->hit_slide_time,
        .hit_ctrl_time = f->hit_ctrl_time,
        .fall_time = f->fall_time,
        .back_edge_body_dist = (int16_t)back_body_dist,
        .front_edge_body_dist = (int16_t)front_body_dist,
        .back_edge_dist = (int16_t)back_dist,
        .state_axis = f->state_axis,
        .hit_launch = (uint8_t)(
            f->gethit_fall || f->gethit_vy_q8 != 0 || !f->on_ground),
        .alive = (uint8_t)(f->hp > 0),
        .can_recover = (uint8_t)(
            f->gethit_fall_recover &&
            f->fall_time >= f->gethit_fall_recover_time),
        .is_bound = (uint8_t)(f->bound_to >= 0),
        .anim_ended = (uint8_t)(anim_ended != 0),
        .move_contact = f->move_contact
    };
    for (uint8_t i = 0u; i < state->controller_count; ++i) {
        const uint16_t index = (uint16_t)(state->controller_ofs + i);
        if (index >= fight->cns->controller_count) break;
        const ik_cns_controller_t* ctrl = &fight->cns->controllers[index];
        if (hit_pause_only &&
            (ctrl->flags & IK_CNS_CTRL_IGNORE_HIT_PAUSE) == 0u) {
            continue;
        }
        if (!ik_cns_controller_trigger_context_now(ctrl, &context)) {
            continue;
        }

        switch ((ik_cns_controller_type_t)ctrl->type) {
            case IK_CNS_CTRL_CHANGE_STATE: {
                const int has_ctrl = (ctrl->flags & IK_CNS_CTRL_HAS_CTRL) != 0u;
                const int16_t target = ctrl->value0;
                enter_state(fight, f, target);
                if (has_ctrl) f->ctrl = (int8_t)(ctrl->value1 != 0);
                return 1;
            }
            case IK_CNS_CTRL_CTRL_SET:
                f->ctrl = (int8_t)(ctrl->value0 != 0);
                break;
            case IK_CNS_CTRL_POS_ADD:
                f->x_q8 += (int32_t)f->facing * ctrl->value0;
                f->y_q8 += ctrl->value1;
                f->x_q8 = clamp_q8(f->x_q8, IK_STAGE_MIN_X, IK_STAGE_MAX_X);
                sync_position(f);
                break;
            case IK_CNS_CTRL_SPR_PRIORITY:
                f->spr_priority = (int8_t)ctrl->value0;
                break;

            case IK_CNS_CTRL_CHANGE_ANIM:
                f->anim = ctrl->value0;
                f->anim_time = anim_element_start_tick(
                    frames, f->anim,
                    (uint16_t)(ctrl->value1 < 1 ? 1 : ctrl->value1));
                return 0;

            case IK_CNS_CTRL_WIDTH: {
                const ik_cns_constants_t* c = constants_for(fight);
                const int16_t base_front = c ? c->ground_front : 16;
                const int16_t base_back = c ? c->ground_back : 15;
                f->push_front = (int16_t)(base_front + ctrl->value0);
                f->push_back = (int16_t)(base_back + ctrl->value1);
                break;
            }

            case IK_CNS_CTRL_VEL_SET:
                if ((ctrl->flags & IK_CNS_CTRL_AXIS_X) != 0u) {
                    int32_t vx = ctrl->value0;
                    if ((ctrl->flags & IK_CNS_CTRL_LOCAL_X) != 0u) {
                        vx *= f->facing;
                    }
                    f->vx_q8 = vx;
                }
                if ((ctrl->flags & IK_CNS_CTRL_AXIS_Y) != 0u) {
                    f->vy_q8 = ctrl->value1;
                }
                break;

            case IK_CNS_CTRL_VEL_MUL:
                if ((ctrl->flags & IK_CNS_CTRL_AXIS_X) != 0u) {
                    f->vx_q8 =
                        (f->vx_q8 * (int32_t)ctrl->value0) / IK_CNS_Q8_ONE;
                }
                if ((ctrl->flags & IK_CNS_CTRL_AXIS_Y) != 0u) {
                    f->vy_q8 =
                        (f->vy_q8 * (int32_t)ctrl->value1) / IK_CNS_Q8_ONE;
                }
                break;

            case IK_CNS_CTRL_POS_SET:
                if ((ctrl->flags & IK_CNS_CTRL_AXIS_X) != 0u) {
                    f->x_q8 = ctrl->value0;
                }
                if ((ctrl->flags & IK_CNS_CTRL_AXIS_Y) != 0u) {
                    f->y_q8 =
                        (int32_t)IK_FLOOR_Y * IK_CNS_Q8_ONE + ctrl->value1;
                }
                sync_position(f);
                break;

            case IK_CNS_CTRL_CHANGE_ANIM_BY_VX: {
                const int32_t local_vx = f->vx_q8 * f->facing;
                int16_t action = ctrl->value0;
                if (local_vx > 0) action = ctrl->value1;
                else if (local_vx < 0) action = (int16_t)(ctrl->value1 + 1);
                if (action >= 0 && action != f->anim) {
                    f->anim = action;
                    f->anim_time = 0u;
                }
                break;
            }

            case IK_CNS_CTRL_CHANGE_ANIM_IF_END_FROM:
                if (f->anim == ctrl->value0 && anim_ended) {
                    f->anim = ctrl->value1;
                    f->anim_time = 0u;
                }
                break;

            case IK_CNS_CTRL_CAPTURE_COMMAND_AXIS:
                if ((command_mask & IK_CNS_COMMAND_HOLD_BACK) != 0u) {
                    f->state_axis = -1;
                } else if ((command_mask & IK_CNS_COMMAND_HOLD_FWD) != 0u) {
                    f->state_axis = 1;
                }
                break;

            case IK_CNS_CTRL_JUMP_LAUNCH: {
                const ik_cns_constants_t* c = constants_for(fight);
                if (!c) break;
                int32_t vx = c->jump_neu_x_q8;
                if (f->state_axis < 0) {
                    vx = c->jump_back_q8;
                } else if (f->state_axis > 0) {
                    vx = (f->prev_state == 100)
                        ? c->run_jump_fwd_x_q8
                        : c->jump_fwd_q8;
                }
                f->vx_q8 = (int32_t)f->facing * vx;
                f->vy_q8 = c->jump_neu_y_q8;
                break;
            }

            case IK_CNS_CTRL_AIR_JUMP_LAUNCH: {
                const ik_cns_constants_t* c = constants_for(fight);
                if (!c) break;
                int32_t vx = c->air_jump_neu_x_q8;
                if (f->state_axis < 0) vx = c->air_jump_back_q8;
                else if (f->state_axis > 0) vx = c->air_jump_fwd_q8;
                f->vx_q8 = (int32_t)f->facing * vx;
                f->vy_q8 = c->air_jump_neu_y_q8;
                break;
            }

            case IK_CNS_CTRL_CHANGE_ANIM_IF_EXISTS: {
                uint32_t first = 0u;
                uint32_t count = 0u;
                int16_t action = ctrl->value1;
                if (frames && ik_frames_bounds(
                        frames, ctrl->value0, &first, &count) && count > 0u) {
                    action = ctrl->value0;
                }
                if (f->anim != action) {
                    f->anim = action;
                    f->anim_time = 0u;
                }
                break;
            }

            case IK_CNS_CTRL_CHANGE_ANIM_DESCENT_IF_EXISTS: {
                const int16_t first_action = ctrl->value1;
                if (f->vy_q8 > ctrl->value0 &&
                    f->anim >= first_action &&
                    f->anim <= (int16_t)(first_action + 2)) {
                    uint32_t first = 0u;
                    uint32_t count = 0u;
                    const int16_t action = (int16_t)(f->anim + 3);
                    if (frames && ik_frames_bounds(
                            frames, action, &first, &count) && count > 0u) {
                        f->anim = action;
                        f->anim_time = 0u;
                    }
                }
                break;
            }

            case IK_CNS_CTRL_GUARD_ANIM_BY_TYPE: {
                int16_t action = ctrl->value0;
                if (f->guard_type == IK_CNS_STATE_CROUCH) action++;
                else if (f->guard_type == IK_CNS_STATE_AIR) action += 2;
                if (f->anim != action) {
                    f->anim = action;
                    f->anim_time = 0u;
                }
                break;
            }

            case IK_CNS_CTRL_GUARD_STATE_BY_TYPE: {
                int16_t target = ctrl->value0;
                if (f->guard_type == IK_CNS_STATE_CROUCH) target++;
                else if (f->guard_type == IK_CNS_STATE_AIR) target += 2;
                enter_state(fight, f, target);
                return 1;
            }

            case IK_CNS_CTRL_GUARD_END: {
                int16_t target = 0;
                if (f->guard_type == IK_CNS_STATE_CROUCH) target = 11;
                else if (f->guard_type == IK_CNS_STATE_AIR) target = 50;
                enter_state(fight, f, target);
                f->ctrl = 1;
                return 1;
            }

            case IK_CNS_CTRL_HIT_VEL_SET:
                if ((ctrl->flags & IK_CNS_CTRL_AXIS_X) != 0u) {
                    f->vx_q8 = (int32_t)f->facing * f->gethit_vx_q8;
                }
                if ((ctrl->flags & IK_CNS_CTRL_AXIS_Y) != 0u) {
                    f->vy_q8 = f->gethit_vy_q8;
                }
                break;

            case IK_CNS_CTRL_GET_HIT_ANIM: {
                int16_t base;
                if (ctrl->value0 == 1) {
                    base = 5020;
                } else {
                    base = f->gethit_ground_type == IK_CNS_GROUND_HIGH
                        ? 5000
                        : 5010;
                }
                int16_t action =
                    (int16_t)(base + (int16_t)f->gethit_anim_type);
                uint32_t first = 0u;
                uint32_t count = 0u;
                if (!frames || !ik_frames_bounds(
                        frames, action, &first, &count) || count == 0u) {
                    action = base;
                }
                if (f->anim != action) {
                    f->anim = action;
                    f->anim_time = 0u;
                }
                break;
            }

            case IK_CNS_CTRL_HIT_RECOVER_STATE: {
                const int16_t target = f->gethit_fall ? 5050 : 5040;
                enter_state(fight, f, target);
                if (!f->gethit_fall) f->ctrl = 1;
                return 1;
            }

            case IK_CNS_CTRL_FALL_BOUNCE_VEL:
                if (f->gethit_fall_x_set) {
                    f->vx_q8 =
                        (int32_t)f->facing * f->gethit_fall_x_q8;
                } else {
                    f->vx_q8 = (f->vx_q8 * 3) / 4;
                }
                f->vy_q8 = f->gethit_fall_y_q8;
                f->on_ground = 0;
                break;

            case IK_CNS_CTRL_FALL_GROUND_BRANCH:
                if (f->gethit_fall_y_q8 == 0) {
                    enter_state(fight, f, ctrl->value0);
                    return 1;
                }
                break;

            case IK_CNS_CTRL_POS_ADD_VEL:
                if ((ctrl->flags & IK_CNS_CTRL_AXIS_X) != 0u) {
                    f->x_q8 += f->vx_q8;
                    f->x_q8 = clamp_q8(
                        f->x_q8, IK_STAGE_MIN_X, IK_STAGE_MAX_X);
                }
                if ((ctrl->flags & IK_CNS_CTRL_AXIS_Y) != 0u) {
                    f->y_q8 += f->vy_q8;
                }
                sync_position(f);
                break;

            case IK_CNS_CTRL_VEL_ADD:
                if ((ctrl->flags & IK_CNS_CTRL_AXIS_X) != 0u) {
                    int32_t vx = ctrl->value0;
                    if ((ctrl->flags & IK_CNS_CTRL_LOCAL_X) != 0u) {
                        vx *= f->facing;
                    }
                    f->vx_q8 += vx;
                }
                if ((ctrl->flags & IK_CNS_CTRL_AXIS_Y) != 0u) {
                    f->vy_q8 += ctrl->value1;
                }
                break;

            case IK_CNS_CTRL_VEL_MUL_X_BY_ANIM_ELEM: {
                const int16_t mul =
                    elem < (uint16_t)(ctrl->trigger_value < 1
                                          ? 1
                                          : ctrl->trigger_value)
                        ? ctrl->value0
                        : ctrl->value1;
                f->vx_q8 =
                    (f->vx_q8 * (int32_t)mul) / IK_CNS_Q8_ONE;
                break;
            }

            case IK_CNS_CTRL_POS_ADD_FROM_BACK_EDGE: {
                const int32_t delta =
                    (int32_t)ctrl->value0 -
                    (int32_t)context.back_edge_body_dist * IK_CNS_Q8_ONE;
                f->x_q8 += (int32_t)f->facing * delta;
                f->x_q8 = clamp_q8(
                    f->x_q8, IK_STAGE_MIN_X, IK_STAGE_MAX_X);
                sync_position(f);
                break;
            }

            case IK_CNS_CTRL_POS_FREEZE:
                if ((ctrl->flags & IK_CNS_CTRL_AXIS_X) != 0u) {
                    f->pos_freeze_x = 1u;
                }
                if ((ctrl->flags & IK_CNS_CTRL_AXIS_Y) != 0u) {
                    f->pos_freeze_y = 1u;
                }
                break;

            case IK_CNS_CTRL_DOWNED_HIT_BRANCH:
                if (f->gethit_vy_q8 != 0 &&
                    ik_cns_find_state(fight->cns, 5030)) {
                    f->anim = 5090;
                    f->anim_time = 0u;
                    enter_state(fight, f, 5030);
                } else if (ik_cns_find_state(fight->cns, 5081)) {
                    f->anim = 5080;
                    f->anim_time = 0u;
                    enter_state(fight, f, 5081);
                }
                return 1;

            case IK_CNS_CTRL_TARGET_BIND:
                if (f->target_index >= 0 && f->target_index < 2) {
                    ik_fighter_t* target =
                        &fight->fighters[(int)f->target_index];
                    target->bound_to =
                        (int8_t)(f == &fight->fighters[0] ? 0 : 1);
                    target->x_q8 =
                        f->x_q8 + (int32_t)f->facing * ctrl->value0;
                    target->y_q8 = f->y_q8 + ctrl->value1;
                    target->vx_q8 = 0;
                    target->vy_q8 = 0;
                    sync_position(target);
                }
                break;

            case IK_CNS_CTRL_TARGET_FACING:
                if (f->target_index >= 0 && f->target_index < 2) {
                    ik_fighter_t* target =
                        &fight->fighters[(int)f->target_index];
                    target->facing = (int8_t)(
                        f->facing * (ctrl->value0 < 0 ? -1 : 1));
                }
                break;

            case IK_CNS_CTRL_TARGET_LIFE_ADD:
                if (f->target_index >= 0 && f->target_index < 2) {
                    const int target_index = f->target_index;
                    ik_fighter_t* target = &fight->fighters[target_index];
                    int hp = (int)target->hp + ctrl->value0;
                    const int max_hp = ik_fight_max_hp(fight);
                    if (hp > max_hp) hp = max_hp;
                    if (hp <= 0) {
                        hp = 0;
                        fight->winner =
                            (uint8_t)((target_index ^ 1) + 1);
                        fight->events |= IK_EVENT_KO;
                    }
                    target->hp = (int16_t)hp;
                }
                break;

            case IK_CNS_CTRL_TARGET_STATE:
                if (f->target_index >= 0 && f->target_index < 2) {
                    const int target_index = f->target_index;
                    ik_fighter_t* target = &fight->fighters[target_index];
                    target->bound_to = -1;
                    enter_state(fight, target, ctrl->value0);
                    f->target_index = -1;
                }
                break;

            case IK_CNS_CTRL_TURN:
                f->facing = (int8_t)-f->facing;
                break;

            case IK_CNS_CTRL_CHANGE_ANIM2:
                f->anim = ctrl->value0;
                f->anim_time = anim_element_start_tick(
                    frames, f->anim,
                    (uint16_t)(ctrl->value1 < 1 ? 1 : ctrl->value1));
                break;

            case IK_CNS_CTRL_SELF_STATE:
                if (f->bound_to >= 0 && f->bound_to < 2) {
                    fight->fighters[(int)f->bound_to].target_index = -1;
                }
                f->bound_to = -1;
                enter_state(fight, f, ctrl->value0);
                return 1;

            case IK_CNS_CTRL_FALL_RECOVERY: {
                const ik_cns_constants_t* c = constants_for(fight);
                if (!c ||
                    (command_mask & IK_CNS_COMMAND_RECOVERY) == 0u ||
                    !f->gethit_fall_recover ||
                    f->fall_time < f->gethit_fall_recover_time) {
                    break;
                }

                const int32_t rel_y_q8 =
                    f->y_q8 -
                    (int32_t)IK_FLOOR_Y * IK_CNS_Q8_ONE;
                if (f->vy_q8 > 0 &&
                    rel_y_q8 >=
                        c->air_gethit_groundrecover_threshold_q8 &&
                    ik_cns_find_state(fight->cns, 5200)) {
                    enter_state(fight, f, 5200);
                    return 1;
                }
                if (f->vy_q8 >
                        c->air_gethit_airrecover_threshold_q8 &&
                    ik_cns_find_state(fight->cns, 5210)) {
                    enter_state(fight, f, 5210);
                    return 1;
                }
                break;
            }

            default:
                break;
        }
    }
    return 0;
}

static int dispatch_controlled_input(ik_fight_t* fight, ik_fighter_t* f,
                                     const ik_fight_controls_t* controls) {
    if (!controls) return 0;

    /* State -1 gates execute before ctrl: legal cancels can intentionally
     * ChangeState while the current attack still has ctrl = 0. */
    if (controls->has_state_request) {
        enter_state(fight, f, controls->requested_state);
        return 1;
    }

    if (!f->on_ground) {
        const ik_cns_constants_t* c = constants_for(fight);
        if (f->ctrl && controls->up && !f->up_latched && c &&
            c->air_jump_num > 0 &&
            f->air_jumps_used < (uint8_t)c->air_jump_num &&
            ((int32_t)IK_FLOOR_Y * IK_CNS_Q8_ONE - f->y_q8) >=
                (int32_t)c->air_jump_height * IK_CNS_Q8_ONE &&
            ik_cns_find_state(fight ? fight->cns : 0, 45)) {
            enter_state(fight, f, 45);
            f->air_jumps_used++;
            f->up_latched = 1u;
            return 1;
        }
        return 0;
    }

    if (!f->ctrl) return 0;
    if (controls->down) {
        const int16_t crouch_state =
            ik_cns_find_state(fight ? fight->cns : 0, 10)
                ? 10
                : IK_STATE_CROUCH;
        if (f->state != IK_STATE_CROUCH && f->state != crouch_state) {
            enter_state(fight, f, crouch_state);
        }
        return 1;
    }
    if (!controls->down && f->state == IK_STATE_CROUCH &&
        ik_cns_find_state(fight ? fight->cns : 0, 12)) {
        enter_state(fight, f, 12);
        return 1;
    }
    if (controls->up &&
        ik_cns_find_state(fight ? fight->cns : 0, IK_STATE_JUMP)) {
        enter_state(fight, f, IK_STATE_JUMP);
        f->up_latched = 1u;
        return 1;
    }
    return 0;
}

static void step_fighter(ik_fight_t* fight, int index,
                         const ik_fight_controls_t* controls,
                         int is_dummy,
                         const ik_frame_table_t* frames,
                         const ik_frame_table_t* foe_frames) {
    ik_fighter_t* f = &fight->fighters[index];
    ik_fighter_t* foe = &fight->fighters[index ^ 1];
    const ik_cns_constants_t* c = constants_for(fight);

    f->pos_freeze_x = 0u;
    f->pos_freeze_y = 0u;
    if (controls && !controls->up) {
        f->up_latched = 0u;
    }

    /* Width is a one-tick controller in MUGEN/Ikemen. Reset to the
     * character constants before evaluating the current tick's controllers. */
    if (c) {
        f->push_back = f->on_ground ? c->ground_back : c->air_back;
        f->push_front = f->on_ground ? c->ground_front : c->air_front;
        f->body_height = c->height;
    }

    if (f->hit_pause > 0u) {
        (void)process_cns_controllers(fight, f, controls, frames, 1);
        f->hit_pause--;
        return;
    }

    f->state_time++;
    f->anim_time++;
    if (f->ctrl && f->bound_to < 0) {
        f->facing = (foe->x >= f->x) ? 1 : -1;
    }
    if (f->gethit_fall && f->fall_time < 65535u) {
        f->fall_time++;
    }

    if (f->state == IK_STATE_KO) return;

    if (!is_dummy && controls && f->hitstun == 0u) {
        const int threat =
            guard_threat(fight, foe_frames, index, controls);
        if (controls->back && threat && f->ctrl &&
            !is_attack_state(fight, f->state) &&
            !is_active_guard_state(f->state) && f->state != 140 &&
            ik_cns_find_state(fight->cns, 120)) {
            f->guard_type = guard_type_for(fight, f, controls);
            enter_state(fight, f, 120);
            return;
        }
        if (is_active_guard_state(f->state) &&
            (!controls->back || !threat) &&
            ik_cns_find_state(fight->cns, 140)) {
            enter_state(fight, f, 140);
            return;
        }
    }

    if (process_cns_controllers(fight, f, controls, frames, 0)) return;

    /* TargetBind owns the bound player's transform. Physics=N thrown states
     * must not drift after the attacker's bind controller positioned them. */
    if (f->bound_to >= 0) return;

    if (f->hitstun > 0u) {
        f->hitstun--;
        if (!f->on_ground) {
            step_air(fight, f, 0);
        } else if (c) {
            const ik_cns_state_t* hit_spec = state_spec(fight, f->state);
            const int physics = hit_spec ? hit_spec->physics : IK_CNS_PHYS_STAND;
            apply_ground_velocity(
                f, c,
                physics == IK_CNS_PHYS_CROUCH
                    ? IK_CNS_PHYS_CROUCH
                    : IK_CNS_PHYS_STAND);
        }
        if (f->hitstun == 0u && f->state == IK_STATE_HIT) {
            if (f->on_ground) {
                enter_state(fight, f, IK_STATE_IDLE);
                f->vx_q8 = 0;
            }
        }
    } else if (is_attack_state(fight, f->state)) {
        if (dispatch_controlled_input(fight, f, controls)) return;

        if (is_air_attack(fight, f->state)) {
            /* Physics=A continues while the attack animation runs. If the
             * common1 landing state is compiled, landing transitions to it. */
            step_air(fight, f, 1);
            return;
        }

        {
            const ik_cns_state_t* attack_spec = state_spec(fight, f->state);
            if (attack_spec &&
                (attack_spec->physics == IK_CNS_PHYS_STAND ||
                 attack_spec->physics == IK_CNS_PHYS_CROUCH)) {
                if (c) apply_ground_velocity(f, c, attack_spec->physics);
            } else if (attack_spec &&
                       attack_spec->physics == IK_CNS_PHYS_NONE) {
                /* Physics=N keeps explicit velocity but applies no automatic
                 * friction/gravity. Position still integrates velocity. */
                f->x_q8 += f->vx_q8;
                f->x_q8 = clamp_q8(
                    f->x_q8, IK_STAGE_MIN_X, IK_STAGE_MAX_X);
                sync_position(f);
            }
        }

        const uint32_t duration = ik_action_duration_ticks(frames, f->anim);
        if (duration > 0u && f->anim_time >= duration) {
            enter_state(
                fight, f,
                is_crouch_attack(fight, f->state)
                    ? IK_STATE_CROUCH
                    : IK_STATE_IDLE);
        }
    } else if (!f->on_ground) {
        if (!is_dummy && controls &&
            dispatch_controlled_input(fight, f, controls)) {
            return;
        }
        const ik_cns_state_t* air_spec = state_spec(fight, f->state);
        const int custom_landing =
            air_spec && air_spec->physics == IK_CNS_PHYS_NONE &&
            air_spec->owns_air_accel;
        step_air(fight, f, custom_landing ? 0 : 1);
    } else if (f->on_ground) {
        const ik_cns_state_t* spec = state_spec(fight, f->state);

        if (!is_dummy && controls) {
            if (dispatch_controlled_input(fight, f, controls)) return;

            /* Common state 0 owns the transition into walk state 20. Once
             * inside a compiled common state its controllers/physics own the
             * state lifetime; the legacy fallback is only for missing data. */
            if (spec && f->state == IK_STATE_IDLE &&
                (controls->forward || controls->back) &&
                ik_cns_find_state(fight ? fight->cns : 0, IK_STATE_WALK)) {
                enter_state(fight, f, IK_STATE_WALK);
                return;
            }

            if (spec && f->state == IK_STATE_WALK &&
                !controls->forward && !controls->back) {
                enter_state(fight, f, IK_STATE_IDLE);
                return;
            }
        }

        if (spec && (spec->physics == IK_CNS_PHYS_STAND ||
                     spec->physics == IK_CNS_PHYS_CROUCH)) {
            if (c) apply_ground_velocity(f, c, spec->physics);
        } else if (!spec && !is_dummy && controls) {
            int moved = 0;
            if (controls->back) {
                const int32_t speed =
                    c ? c->walk_back_q8 : -2 * IK_CNS_Q8_ONE;
                f->x_q8 += (int32_t)f->facing * speed;
                moved = 1;
            }
            if (controls->forward) {
                const int32_t speed =
                    c ? c->walk_fwd_q8 : 2 * IK_CNS_Q8_ONE;
                f->x_q8 += (int32_t)f->facing * speed;
                moved = 1;
            }
            f->x_q8 = clamp_q8(
                f->x_q8, IK_STAGE_MIN_X, IK_STAGE_MAX_X);
            sync_position(f);
            if (moved) {
                if (f->state != IK_STATE_WALK) {
                    enter_state(fight, f, IK_STATE_WALK);
                }
            } else if (f->state != IK_STATE_IDLE) {
                enter_state(fight, f, IK_STATE_IDLE);
            }
        } else if (!spec && f->state != IK_STATE_IDLE) {
            enter_state(fight, f, IK_STATE_IDLE);
        }
    }

    {
        int l0, t0, r0, b0, l1, t1, r1, b1;
        ik_body_box(f, &l0, &t0, &r0, &b0);
        ik_body_box(foe, &l1, &t1, &r1, &b1);
        if (ik_boxes_overlap(l0, t0, r0, b0, l1, t1, r1, b1)) {
            const int16_t mid = (int16_t)((f->x + foe->x) / 2);
            if (f->x <= foe->x) {
                f->x = clamp16((int16_t)(f->x - 1), IK_STAGE_MIN_X, mid);
            } else {
                f->x = clamp16((int16_t)(f->x + 1), mid, IK_STAGE_MAX_X);
            }
            f->x_q8 = (int32_t)f->x * IK_CNS_Q8_ONE;
        }
    }
}

void ik_fight_update(ik_fight_t* fight,
                     const ik_fight_controls_t* p1,
                     const ik_fight_controls_t* p2,
                     const ik_frame_table_t* p1_frames,
                     const ik_frame_table_t* p2_frames) {
    if (!fight || !p1_frames) return;
    if (!p2_frames) p2_frames = p1_frames;
    fight->effect_count = 0u;
    if (p1 && p1->start) {
        ik_fight_reset(fight);
        return;
    }

    fight->events = IK_EVENT_NONE;
    if (fight->round_over) {
        if (fight->ko_freeze > 0u) fight->ko_freeze--;
        fight->frame++;
        return;
    }

    fight->frame++;
    if (fight->timer_frames > 0u) {
        fight->timer_frames--;
        if (fight->timer_frames == 0u) {
            fight->round_over = 1;
            fight->winner =
                (fight->fighters[0].hp >= fight->fighters[1].hp) ? 1u : 2u;
            fight->events |= IK_EVENT_ROUND_OVER;
            return;
        }
    }

    const int dummy = (p2 == 0);
    step_fighter(fight, 0, p1, 0, p1_frames, p2_frames);
    step_fighter(fight, 1, p2, dummy, p2_frames, p1_frames);

    const ik_cns_hitdef_t* candidates[2] = {0, 0};
    uint32_t candidate_bits[2] = {0u, 0u};
    uint8_t lands[2] = {0u, 0u};
    uint8_t consume[2] = {0u, 0u};

    /* Gather both contacts before changing either fighter's state. This is
     * required for MUGEN priority/trade semantics: the old loop applied P1
     * first, which could erase P2's simultaneous active HitDef. */
    for (int atk = 0; atk < 2; ++atk) {
        ik_fighter_t* a = &fight->fighters[atk];
        ik_fighter_t* v = &fight->fighters[atk ^ 1];
        const ik_frame_table_t* attacker_frames =
            atk == 0 ? p1_frames : p2_frames;
        const ik_frame_table_t* victim_frames =
            atk == 0 ? p2_frames : p1_frames;
        uint8_t local_hitdef = 0u;
        const ik_cns_hitdef_t* hitdef =
            active_hitdef(fight, attacker_frames, a, v, &local_hitdef);
        if (!hitdef || local_hitdef >= 32u) continue;
        if (!hitdef_allows_target(fight, v, hitdef)) continue;
        if (!juggle_allows_target(fight, a, v, hitdef)) continue;
        const uint32_t bit = (uint32_t)1u << local_hitdef;
        if ((a->hitdef_hit_mask & bit) != 0u) continue;
        if (!fighter_clsn_overlap(
                attacker_frames, victim_frames, a, v)) continue;

        candidates[atk] = hitdef;
        candidate_bits[atk] = bit;
        lands[atk] = 1u;
    }

    if (candidates[0] && candidates[1]) {
        const uint8_t p0 =
            candidates[0]->priority ? candidates[0]->priority : 4u;
        const uint8_t p1v =
            candidates[1]->priority ? candidates[1]->priority : 4u;

        if (p0 > p1v) {
            lands[1] = 0u;
            consume[1] = 1u;
        } else if (p1v > p0) {
            lands[0] = 0u;
            consume[0] = 1u;
        } else {
            const uint8_t t0 = candidates[0]->priority_type;
            const uint8_t t1 = candidates[1]->priority_type;

            if (t0 == IK_CNS_PRIORITY_DODGE ||
                t1 == IK_CNS_PRIORITY_DODGE) {
                /* Equal-priority no-hit tie: both HitDefs stay enabled. */
                lands[0] = lands[1] = 0u;
            } else if (t0 == IK_CNS_PRIORITY_HIT &&
                       t1 == IK_CNS_PRIORITY_HIT) {
                /* True trade. */
            } else if (t0 == IK_CNS_PRIORITY_HIT &&
                       t1 == IK_CNS_PRIORITY_MISS) {
                lands[1] = 0u;
                consume[1] = 1u;
            } else if (t1 == IK_CNS_PRIORITY_HIT &&
                       t0 == IK_CNS_PRIORITY_MISS) {
                lands[0] = 0u;
                consume[0] = 1u;
            } else {
                /* Miss/Miss is also a no-hit tie; leave both active. */
                lands[0] = lands[1] = 0u;
            }
        }
    }

    for (int atk = 0; atk < 2; ++atk) {
        if (consume[atk] && candidate_bits[atk] != 0u) {
            fight->fighters[atk].hitdef_hit_mask |= candidate_bits[atk];
        }
    }

    for (int atk = 0; atk < 2; ++atk) {
        const ik_cns_hitdef_t* hitdef = candidates[atk];
        if (!lands[atk] || !hitdef) continue;

        fight->fighters[atk].hitdef_hit_mask |= candidate_bits[atk];
        const int victim = atk ^ 1;
        ik_fighter_t* v = &fight->fighters[victim];
        const ik_fight_controls_t* victim_controls =
            victim == 0 ? p1 : p2;
        if ((hitdef->flags & IK_CNS_HITDEF_THROW) != 0u) {
            const ik_fight_controls_t* attacker_controls =
                atk == 0 ? p1 : p2;
            apply_throw(fight, atk, attacker_controls, hitdef);
        } else if (can_guard_hit(fight, v, victim_controls, hitdef)) {
            apply_guard(fight, victim, victim_controls, hitdef);
        } else {
            apply_damage(fight, victim, hitdef);
        }
    }

    if (!fight->round_over) {
        for (int i = 0; i < 2; ++i) {
            if (fight->fighters[i].hp <= 0 &&
                fight->fighters[i].state == 5150) {
                fight->round_over = 1;
                fight->winner = (uint8_t)((i ^ 1) + 1);
                fight->events |= IK_EVENT_ROUND_OVER;
                break;
            }
        }
    }
}

const char* ik_fight_status_text(const ik_fight_t* fight) {
    if (!fight) return 0;
    if (fight->round_over) {
        if (fight->winner == 1) return "P1 WINS - START RESETS";
        if (fight->winner == 2) return "P2 WINS - START RESETS";
        return "TIME OVER - START RESETS";
    }
    return 0;
}

int ik_fight_status_needs_start(const ik_fight_t* fight) {
    return fight != 0 && fight->round_over != 0;
}
