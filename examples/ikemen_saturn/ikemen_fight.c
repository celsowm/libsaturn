#include "ikemen_fight.h"

typedef struct ik_move_hitdef {
    int16_t state;
    int16_t damage;
    uint16_t hitstun;
    uint16_t hitpause;
    int16_t ground_velocity;
} ik_move_hitdef_t;

static const ik_move_hitdef_t k_punch_hitdef = {
    IK_STATE_PUNCH, IK_PUNCH_DAMAGE, IK_PUNCH_HITSTUN,
    IK_PUNCH_HITPAUSE, IK_PUNCH_GROUND_VELOCITY
};
static const ik_move_hitdef_t k_kick_hitdef = {
    IK_STATE_KICK, IK_KICK_DAMAGE, IK_KICK_HITSTUN,
    IK_KICK_HITPAUSE, IK_KICK_GROUND_VELOCITY
};

static int16_t clamp16(int16_t v, int16_t lo, int16_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static const ik_move_hitdef_t* hitdef_for_state(int16_t state) {
    if (state == IK_STATE_PUNCH) return &k_punch_hitdef;
    if (state == IK_STATE_KICK) return &k_kick_hitdef;
    return 0;
}

int ik_action_for_state(int16_t state) {
    switch (state) {
        case IK_STATE_WALK: return 20;
        case IK_STATE_CROUCH: return 11;
        case IK_STATE_JUMP: return 41;
        case IK_STATE_PUNCH: return 200;
        case IK_STATE_KICK: return 230;
        case IK_STATE_HIT: return 105;
        case IK_STATE_KO: return 120;
        case IK_STATE_GUARD: return 130;
        default: return 0;
    }
}

static void fighter_spawn(ik_fighter_t* f, int16_t x, int8_t facing) {
    f->x = x;
    f->y = IK_FLOOR_Y;
    f->vx = 0;
    f->vy = 0;
    f->facing = facing;
    f->on_ground = 1;
    f->state = IK_STATE_IDLE;
    f->state_time = 0;
    f->hp = IK_MAX_HP;
    f->hitstun = 0;
    f->hit_pause = 0;
    f->attack_has_hit = 0;
    f->attack_id = 0;
}

void ik_fight_init(ik_fight_t* fight) {
    if (!fight) return;
    fighter_spawn(&fight->fighters[0], 110, 1);
    fighter_spawn(&fight->fighters[1], 210, -1);
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
    ik_fight_init(fight);
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

static const ik_frame_t* fighter_frame(const ik_frame_table_t* frames,
                                       const ik_fighter_t* fighter) {
    if (!frames || !fighter) return 0;
    return ik_frame_at_time(
        frames, ik_action_for_state(fighter->state), fighter->state_time);
}

static int fighter_clsn_overlap(const ik_frame_table_t* frames,
                                const ik_fighter_t* attacker,
                                const ik_fighter_t* victim) {
    const ik_frame_t* af = fighter_frame(frames, attacker);
    const ik_frame_t* vf = fighter_frame(frames, victim);
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

static void apply_damage(ik_fight_t* fight, int victim,
                         const ik_move_hitdef_t* hitdef) {
    ik_fighter_t* v = &fight->fighters[victim];
    ik_fighter_t* a = &fight->fighters[victim ^ 1];

    v->hp = (int16_t)(v->hp - hitdef->damage);
    v->hitstun = hitdef->hitstun;
    v->hit_pause = hitdef->hitpause;
    a->hit_pause = hitdef->hitpause;
    v->vx = (int16_t)(a->facing * hitdef->ground_velocity);

    if (v->hp <= 0) {
        v->hp = 0;
        v->state = IK_STATE_KO;
        v->state_time = 0;
        fight->round_over = 1;
        fight->winner = (uint8_t)((victim ^ 1) + 1);
        fight->events |= (uint16_t)(IK_EVENT_HIT | IK_EVENT_KO | IK_EVENT_ROUND_OVER);
        fight->ko_freeze = IK_KO_FREEZE_FRAMES;
    } else {
        v->state = IK_STATE_HIT;
        v->state_time = 0;
        fight->events |= IK_EVENT_HIT;
    }
    if ((victim ^ 1) == 0) fight->hits_p1++;
    else fight->hits_p2++;
}

static uint32_t attack_duration(const ik_frame_table_t* frames, int16_t state) {
    const uint32_t exact = ik_action_duration_ticks(frames, ik_action_for_state(state));
    if (exact != 0u) return exact;
    return state == IK_STATE_PUNCH ? 12u : 15u;
}

static void step_fighter(ik_fight_t* fight, int index,
                         const ik_fight_controls_t* controls,
                         int is_dummy, const ik_frame_table_t* frames) {
    ik_fighter_t* f = &fight->fighters[index];
    ik_fighter_t* foe = &fight->fighters[index ^ 1];

    if (f->hit_pause > 0u) {
        f->hit_pause--;
        return;
    }

    f->state_time++;
    f->facing = (foe->x >= f->x) ? 1 : -1;

    if (f->state == IK_STATE_KO) return;

    if (f->hitstun > 0u) {
        f->hitstun--;
        f->x = clamp16((int16_t)(f->x + f->vx), IK_STAGE_MIN_X, IK_STAGE_MAX_X);
        if (f->vx > 0) {
            f->vx = (int16_t)((f->vx * 85) / 100);
            if (f->vx < 2) f->vx = 0;
        } else if (f->vx < 0) {
            f->vx = (int16_t)((f->vx * 85) / 100);
            if (f->vx > -2) f->vx = 0;
        }
        if (f->hitstun == 0u && f->state == IK_STATE_HIT) {
            f->state = IK_STATE_IDLE;
            f->state_time = 0;
            f->vx = 0;
        }
    } else if (f->state == IK_STATE_PUNCH || f->state == IK_STATE_KICK) {
        if (f->state_time >= attack_duration(frames, f->state)) {
            f->state = IK_STATE_IDLE;
            f->state_time = 0;
        }
    } else if (!f->on_ground) {
        f->vy = (int16_t)(f->vy + IK_GRAVITY);
        f->y = (int16_t)(f->y + f->vy);
        if (!is_dummy && controls) {
            if (controls->back) f->x = (int16_t)(f->x - f->facing);
            if (controls->forward) f->x = (int16_t)(f->x + f->facing);
        }
        if (f->y >= IK_FLOOR_Y) {
            f->y = IK_FLOOR_Y;
            f->vy = 0;
            f->on_ground = 1;
            f->state = IK_STATE_IDLE;
            f->state_time = 0;
        }
        f->x = clamp16(f->x, IK_STAGE_MIN_X, IK_STAGE_MAX_X);
    } else if (!is_dummy && controls) {
        int moved = 0;
        if (controls->back) {
            f->x = clamp16(
                (int16_t)(f->x - f->facing * IK_WALK_SPEED),
                IK_STAGE_MIN_X, IK_STAGE_MAX_X);
            moved = 1;
        }
        if (controls->forward) {
            f->x = clamp16(
                (int16_t)(f->x + f->facing * IK_WALK_SPEED),
                IK_STAGE_MIN_X, IK_STAGE_MAX_X);
            moved = 1;
        }
        if (controls->up) {
            f->vy = IK_JUMP_VELOCITY;
            f->on_ground = 0;
            f->state = IK_STATE_JUMP;
            f->state_time = 0;
        } else if (controls->down) {
            if (f->state != IK_STATE_CROUCH) f->state_time = 0;
            f->state = IK_STATE_CROUCH;
        } else if (controls->x) {
            /* KFM CMD: standing light punch is button x -> state 200. */
            f->state = IK_STATE_PUNCH;
            f->state_time = 0;
            f->attack_has_hit = 0;
            f->attack_id++;
        } else if (controls->a) {
            /* KFM CMD: standing light kick is button a -> state 230. */
            f->state = IK_STATE_KICK;
            f->state_time = 0;
            f->attack_has_hit = 0;
            f->attack_id++;
        } else if (moved) {
            if (f->state != IK_STATE_WALK) {
                f->state = IK_STATE_WALK;
                f->state_time = 0;
            }
        } else if (f->state != IK_STATE_IDLE) {
            f->state = IK_STATE_IDLE;
            f->state_time = 0;
        }
    } else {
        if (f->state != IK_STATE_IDLE) {
            f->state = IK_STATE_IDLE;
            f->state_time = 0;
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
        const ik_move_hitdef_t* hitdef = hitdef_for_state(a->state);
        if (!hitdef || a->attack_has_hit) continue;
        if (!fighter_clsn_overlap(frames, a, v)) continue;

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
