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

static int is_attack_state(int16_t state) {
    return state == IK_STATE_PUNCH || state == IK_STATE_STRONG_PUNCH ||
           state == IK_STATE_KICK || state == IK_STATE_STRONG_KICK;
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

static void enter_state(ik_fight_t* fight, ik_fighter_t* f, int16_t state) {
    const ik_cns_state_t* spec = ik_cns_find_state(fight ? fight->cns : 0, state);
    f->state = state;
    f->state_time = 0u;
    f->move_contact = 0u;

    if (is_attack_state(state)) {
        f->attack_has_hit = 0u;
        ++f->attack_id;
    }

    if (spec && spec->has_velset) {
        f->vx_q8 = spec->velset_x_q8;
        f->vy_q8 = spec->velset_y_q8;
    }
}

static void fighter_spawn(ik_fighter_t* f, int16_t x, int8_t facing, int hp) {
    set_position(f, x, IK_FLOOR_Y);
    f->vx_q8 = 0;
    f->vy_q8 = 0;
    f->facing = facing;
    f->on_ground = 1;
    f->state = IK_STATE_IDLE;
    f->state_time = 0;
    f->hp = (int16_t)hp;
    f->hitstun = 0;
    f->hit_pause = 0;
    f->attack_has_hit = 0;
    f->attack_id = 0;
    f->move_contact = 0;
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
    fighter_spawn(&fight->fighters[0], 110, 1, hp);
    fighter_spawn(&fight->fighters[1], 210, -1, hp);
    fight->frame = 0;
    fight->timer_frames = IK_ROUND_TIME_FRAMES;
    fight->events = IK_EVENT_NONE;
    fight->round_over = 0;
    fight->winner = 0;
    fight->hits_p1 = 0;
    fight->hits_p2 = 0;
    fight->ko_freeze = 0;
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
    if (!f) return 10;
    if (f->state == IK_STATE_CROUCH) return 12;
    return 10;
}

int ik_body_h(const ik_fighter_t* f) {
    if (!f) return 48;
    if (f->state == IK_STATE_CROUCH) return 32;
    if (!f->on_ground) return 44;
    return 48;
}

void ik_body_box(const ik_fighter_t* f, int* l, int* t, int* r, int* b) {
    const int hw = ik_body_half_w(f);
    const int h = ik_body_h(f);
    if (l) *l = (int)f->x - hw;
    if (t) *t = (int)f->y - h;
    if (r) *r = (int)f->x + hw;
    if (b) *b = (int)f->y;
}

int ik_boxes_overlap(int l0, int t0, int r0, int b0,
                     int l1, int t1, int r1, int b1) {
    return (l0 < r1) && (l1 < r0) && (t0 < b1) && (t1 < b0);
}

static const ik_frame_t* fighter_frame(const ik_fight_t* fight,
                                       const ik_frame_table_t* frames,
                                       const ik_fighter_t* fighter) {
    if (!frames || !fighter) return 0;
    return ik_frame_at_time(
        frames, ik_action_for_state(fight ? fight->cns : 0, fighter->state),
        fighter->state_time);
}

static uint16_t fighter_anim_element(const ik_fight_t* fight,
                                     const ik_frame_table_t* frames,
                                     const ik_fighter_t* fighter) {
    const ik_frame_t* frame = fighter_frame(fight, frames, fighter);
    return frame ? (uint16_t)(frame->index + 1u) : 1u;
}

static int fighter_clsn_overlap(const ik_fight_t* fight,
                                const ik_frame_table_t* frames,
                                const ik_fighter_t* attacker,
                                const ik_fighter_t* victim) {
    const ik_frame_t* af = fighter_frame(fight, frames, attacker);
    const ik_frame_t* vf = fighter_frame(fight, frames, victim);
    if (!af || !vf || af->clsn1_count == 0u || vf->clsn2_count == 0u) return 0;

    for (uint16_t ai = 0u; ai < af->clsn1_count; ++ai) {
        int al, at, ar, ab;
        if (!ik_frame_clsn_world(frames, af, IK_CLSN_ATTACK, ai,
                                 attacker->x, attacker->y, attacker->facing,
                                 &al, &at, &ar, &ab)) {
            continue;
        }
        for (uint16_t vi = 0u; vi < vf->clsn2_count; ++vi) {
            int vl, vt, vr, vb;
            if (!ik_frame_clsn_world(frames, vf, IK_CLSN_HURT, vi,
                                     victim->x, victim->y, victim->facing,
                                     &vl, &vt, &vr, &vb)) {
                continue;
            }
            if (ik_boxes_overlap(al, at, ar, ab, vl, vt, vr, vb)) return 1;
        }
    }
    return 0;
}

static const ik_cns_hitdef_t* active_hitdef(const ik_fight_t* fight,
                                            const ik_frame_table_t* frames,
                                            const ik_fighter_t* fighter) {
    if (!fight || !fight->cns || !fighter || !is_attack_state(fighter->state)) {
        return 0;
    }
    return ik_cns_active_hitdef(
        fight->cns, fighter->state, fighter->state_time,
        fighter_anim_element(fight, frames, fighter));
}

static void apply_damage(ik_fight_t* fight, int victim,
                         const ik_cns_hitdef_t* hitdef) {
    ik_fighter_t* v = &fight->fighters[victim];
    ik_fighter_t* a = &fight->fighters[victim ^ 1];

    v->hp = (int16_t)(v->hp - hitdef->damage);
    v->hitstun = hitdef->ground_hit_time;
    v->hit_pause = hitdef->pause_p2;
    a->hit_pause = hitdef->pause_p1;

    /* HitDef velocities are in the target's local facing coordinates.
     * A negative X therefore always knocks the target away from the attacker. */
    v->vx_q8 = (int32_t)v->facing * hitdef->ground_velocity_x_q8;
    v->vy_q8 = hitdef->ground_velocity_y_q8;

    a->move_contact = 1u;

    if (v->hp <= 0) {
        v->hp = 0;
        enter_state(fight, v, IK_STATE_KO);
        fight->round_over = 1;
        fight->winner = (uint8_t)((victim ^ 1) + 1);
        fight->events |= (uint16_t)(IK_EVENT_HIT | IK_EVENT_KO | IK_EVENT_ROUND_OVER);
        fight->ko_freeze = IK_KO_FREEZE_FRAMES;
    } else {
        enter_state(fight, v, IK_STATE_HIT);
        fight->events |= IK_EVENT_HIT;
    }
    if ((victim ^ 1) == 0) fight->hits_p1++;
    else fight->hits_p2++;
}

static uint32_t attack_duration(const ik_fight_t* fight,
                                const ik_frame_table_t* frames,
                                int16_t state) {
    const uint32_t exact = ik_action_duration_ticks(
        frames, ik_action_for_state(fight ? fight->cns : 0, state));
    return exact != 0u ? exact : 1u;
}

static void apply_ground_velocity(ik_fighter_t* f,
                                  const ik_cns_constants_t* c) {
    if (!f || !c) return;
    f->x_q8 += f->vx_q8;
    f->vx_q8 = (f->vx_q8 * c->stand_friction_q8) / IK_CNS_Q8_ONE;
    if (f->vx_q8 < c->stand_friction_threshold_q8 &&
        f->vx_q8 > -c->stand_friction_threshold_q8) {
        f->vx_q8 = 0;
    }
    f->x_q8 = clamp_q8(f->x_q8, IK_STAGE_MIN_X, IK_STAGE_MAX_X);
    sync_position(f);
}

static void start_jump(ik_fight_t* fight, ik_fighter_t* f,
                       const ik_fight_controls_t* controls) {
    const ik_cns_constants_t* c = constants_for(fight);
    enter_state(fight, f, IK_STATE_JUMP);
    f->on_ground = 0;

    if (c) {
        if (controls && controls->forward) {
            f->vx_q8 = (int32_t)f->facing * c->jump_fwd_q8;
        } else if (controls && controls->back) {
            f->vx_q8 = (int32_t)f->facing * c->jump_back_q8;
        } else {
            f->vx_q8 = (int32_t)f->facing * c->jump_neu_x_q8;
        }
        f->vy_q8 = c->jump_neu_y_q8;
    } else {
        f->vx_q8 = 0;
        f->vy_q8 = -8 * IK_CNS_Q8_ONE;
    }
}

static void step_air(ik_fight_t* fight, ik_fighter_t* f) {
    const ik_cns_constants_t* c = constants_for(fight);
    const int32_t gravity = c ? c->yaccel_q8 : (IK_CNS_Q8_ONE / 2);

    f->vy_q8 += gravity;
    f->x_q8 += f->vx_q8;
    f->y_q8 += f->vy_q8;
    f->x_q8 = clamp_q8(f->x_q8, IK_STAGE_MIN_X, IK_STAGE_MAX_X);

    if (f->y_q8 >= (int32_t)IK_FLOOR_Y * IK_CNS_Q8_ONE) {
        f->y_q8 = (int32_t)IK_FLOOR_Y * IK_CNS_Q8_ONE;
        f->vy_q8 = 0;
        f->vx_q8 = 0;
        f->on_ground = 1;
        enter_state(fight, f, IK_STATE_IDLE);
    }
    sync_position(f);
}

static int try_attack_cancel(ik_fight_t* fight, ik_fighter_t* f,
                             const ik_fight_controls_t* controls) {
    if (!controls) return 0;

    if (f->state == IK_STATE_PUNCH && f->state_time > 5u) {
        if (controls->y) {
            enter_state(fight, f, IK_STATE_STRONG_PUNCH);
            return 1;
        }
        if (controls->b) {
            enter_state(fight, f, IK_STATE_STRONG_KICK);
            return 1;
        }
    }
    if (f->state == IK_STATE_KICK && f->state_time > 6u) {
        if (controls->y) {
            enter_state(fight, f, IK_STATE_STRONG_PUNCH);
            return 1;
        }
        if (controls->b) {
            enter_state(fight, f, IK_STATE_STRONG_KICK);
            return 1;
        }
    }
    return 0;
}

static void step_fighter(ik_fight_t* fight, int index,
                         const ik_fight_controls_t* controls,
                         int is_dummy, const ik_frame_table_t* frames) {
    ik_fighter_t* f = &fight->fighters[index];
    ik_fighter_t* foe = &fight->fighters[index ^ 1];
    const ik_cns_constants_t* c = constants_for(fight);

    if (f->hit_pause > 0u) {
        f->hit_pause--;
        return;
    }

    f->state_time++;
    f->facing = (foe->x >= f->x) ? 1 : -1;

    if (f->state == IK_STATE_KO) return;

    if (f->hitstun > 0u) {
        f->hitstun--;
        if (c) apply_ground_velocity(f, c);
        if (f->hitstun == 0u && f->state == IK_STATE_HIT) {
            enter_state(fight, f, IK_STATE_IDLE);
            f->vx_q8 = 0;
        }
    } else if (is_attack_state(f->state)) {
        if (!try_attack_cancel(fight, f, controls) &&
            f->state_time >= attack_duration(fight, frames, f->state)) {
            enter_state(fight, f, IK_STATE_IDLE);
        }
    } else if (!f->on_ground) {
        step_air(fight, f);
    } else if (!is_dummy && controls) {
        int moved = 0;

        if (controls->back) {
            const int32_t speed = c ? c->walk_back_q8 : -2 * IK_CNS_Q8_ONE;
            f->x_q8 += (int32_t)f->facing * speed;
            moved = 1;
        }
        if (controls->forward) {
            const int32_t speed = c ? c->walk_fwd_q8 : 2 * IK_CNS_Q8_ONE;
            f->x_q8 += (int32_t)f->facing * speed;
            moved = 1;
        }
        f->x_q8 = clamp_q8(f->x_q8, IK_STAGE_MIN_X, IK_STAGE_MAX_X);
        sync_position(f);

        if (controls->up) {
            start_jump(fight, f, controls);
        } else if (controls->down) {
            if (f->state != IK_STATE_CROUCH) enter_state(fight, f, IK_STATE_CROUCH);
        } else if (controls->y) {
            enter_state(fight, f, IK_STATE_STRONG_PUNCH);
        } else if (controls->b) {
            enter_state(fight, f, IK_STATE_STRONG_KICK);
        } else if (controls->x) {
            enter_state(fight, f, IK_STATE_PUNCH);
        } else if (controls->a) {
            enter_state(fight, f, IK_STATE_KICK);
        } else if (moved) {
            if (f->state != IK_STATE_WALK) enter_state(fight, f, IK_STATE_WALK);
        } else if (f->state != IK_STATE_IDLE) {
            enter_state(fight, f, IK_STATE_IDLE);
        }
    } else {
        if (f->state != IK_STATE_IDLE) enter_state(fight, f, IK_STATE_IDLE);
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
                     const ik_frame_table_t* frames) {
    if (!fight) return;
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
            fight->winner = (fight->fighters[0].hp >= fight->fighters[1].hp) ? 1u : 2u;
            fight->events |= IK_EVENT_ROUND_OVER;
            return;
        }
    }

    const int dummy = (p2 == 0);
    step_fighter(fight, 0, p1, 0, frames);
    step_fighter(fight, 1, p2, dummy, frames);

    for (int atk = 0; atk < 2; ++atk) {
        ik_fighter_t* a = &fight->fighters[atk];
        ik_fighter_t* v = &fight->fighters[atk ^ 1];
        const ik_cns_hitdef_t* hitdef = active_hitdef(fight, frames, a);
        if (!hitdef || a->attack_has_hit) continue;
        if (!fighter_clsn_overlap(fight, frames, a, v)) continue;

        a->attack_has_hit = 1u;
        apply_damage(fight, atk ^ 1, hitdef);
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
