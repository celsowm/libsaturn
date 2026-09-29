#include "ikemen_fight.h"

static int16_t clamp16(int16_t v, int16_t lo, int16_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
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
    uint32_t h1 = fight->hits_p1;
    uint32_t h2 = fight->hits_p2;
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
    int hw = ik_body_half_w(f);
    int h = ik_body_h(f);
    int left = (int)f->x - hw;
    int top = (int)f->y - h;
    int right = (int)f->x + hw;
    int bottom = (int)f->y;
    if (l) *l = left;
    if (t) *t = top;
    if (r) *r = right;
    if (b) *b = bottom;
}

int ik_attack_box(const ik_fighter_t* f, int* l, int* t, int* r, int* b) {
    if (!f) return 0;
    if (f->state != IK_STATE_PUNCH && f->state != IK_STATE_KICK) return 0;
    /* Active only in the middle of the swing. */
    if (f->state_time < 4 || f->state_time >= 4 + IK_ATTACK_ACTIVE_FRAMES) return 0;
    int range = (f->state == IK_STATE_PUNCH) ? IK_PUNCH_RANGE : IK_KICK_RANGE;
    int top = (int)f->y - ((f->state == IK_STATE_PUNCH) ? 40 : 30);
    int bottom = top + 14;
    int left, right;
    if (f->facing >= 0) {
        left = (int)f->x + 6;
        right = (int)f->x + range;
    } else {
        left = (int)f->x - range;
        right = (int)f->x - 6;
    }
    if (l) *l = left;
    if (t) *t = top;
    if (r) *r = right;
    if (b) *b = bottom;
    return 1;
}

int ik_boxes_overlap(int l0, int t0, int r0, int b0,
                     int l1, int t1, int r1, int b1) {
    /* Strict: touching edges are not a hit. */
    return (l0 < r1) && (l1 < r0) && (t0 < b1) && (t1 < b0);
}

static void apply_damage(ik_fight_t* fight, int victim, int damage) {
    ik_fighter_t* v = &fight->fighters[victim];
    int attacker = victim ^ 1;
    /* Training guard: dummy blocks with no damage every 4th second. */
    if (victim == 1 && ((fight->frame / 240u) & 1u) != 0u) {
        v->state = IK_STATE_GUARD;
        v->state_time = 0;
        v->hitstun = 6;
        return;
    }
    v->hp = (int16_t)(v->hp - damage);
    if (v->hp <= 0) {
        v->hp = 0;
        v->state = IK_STATE_KO;
        v->state_time = 0;
        fight->round_over = 1;
        fight->winner = (uint8_t)(attacker + 1);
        fight->events |= (uint16_t)(IK_EVENT_HIT | IK_EVENT_KO | IK_EVENT_ROUND_OVER);
        fight->ko_freeze = IK_KO_FREEZE_FRAMES;
    } else {
        v->state = IK_STATE_HIT;
        v->state_time = 0;
        v->hitstun = IK_HITSTUN_FRAMES;
        fight->events |= IK_EVENT_HIT;
    }
    if (attacker == 0) fight->hits_p1++;
    else fight->hits_p2++;
}

static void step_fighter(ik_fight_t* fight, int index, const sat_pad_state_t* pad,
                         int is_dummy) {
    ik_fighter_t* f = &fight->fighters[index];
    ik_fighter_t* foe = &fight->fighters[index ^ 1];
    uint16_t held = pad ? pad->held : 0u;
    uint16_t pressed = pad ? pad->pressed : 0u;

    f->state_time++;

    /* Face the opponent. */
    f->facing = (foe->x >= f->x) ? 1 : -1;

    if (f->state == IK_STATE_KO) return;

    if (f->hitstun > 0) {
        f->hitstun--;
        if (f->hitstun == 0 && f->state == IK_STATE_HIT) {
            f->state = IK_STATE_IDLE;
            f->state_time = 0;
        }
        /* Knockback drift while stunned. */
        f->x = clamp16((int16_t)(f->x - f->facing * 1), IK_STAGE_MIN_X, IK_STAGE_MAX_X);
    } else if (f->state == IK_STATE_PUNCH || f->state == IK_STATE_KICK) {
        uint16_t dur = (f->state == IK_STATE_PUNCH) ? 16u : 20u;
        if (f->state_time >= dur) {
            f->state = IK_STATE_IDLE;
            f->state_time = 0;
        }
    } else if (!f->on_ground) {
        /* Airborne: gravity + drift. */
        f->vy = (int16_t)(f->vy + IK_GRAVITY);
        f->y = (int16_t)(f->y + f->vy);
        if (!is_dummy) {
            if ((held & SAT_PAD_LEFT) != 0u) f->x--;
            if ((held & SAT_PAD_RIGHT) != 0u) f->x++;
        }
        if (f->y >= IK_FLOOR_Y) {
            f->y = IK_FLOOR_Y;
            f->vy = 0;
            f->on_ground = 1;
            f->state = IK_STATE_IDLE;
            f->state_time = 0;
        }
        f->x = clamp16(f->x, IK_STAGE_MIN_X, IK_STAGE_MAX_X);
    } else if (!is_dummy) {
        int moved = 0;
        if ((held & SAT_PAD_LEFT) != 0u) {
            f->x = clamp16((int16_t)(f->x - IK_WALK_SPEED), IK_STAGE_MIN_X, IK_STAGE_MAX_X);
            moved = 1;
        }
        if ((held & SAT_PAD_RIGHT) != 0u) {
            f->x = clamp16((int16_t)(f->x + IK_WALK_SPEED), IK_STAGE_MIN_X, IK_STAGE_MAX_X);
            moved = 1;
        }
        if ((pressed & SAT_PAD_UP) != 0u) {
            f->vy = IK_JUMP_VELOCITY;
            f->on_ground = 0;
            f->state = IK_STATE_JUMP;
            f->state_time = 0;
        } else if ((held & SAT_PAD_DOWN) != 0u) {
            f->state = IK_STATE_CROUCH;
        } else if (((pressed & SAT_PAD_A) != 0u) || ((pressed & SAT_PAD_X) != 0u)) {
            f->state = IK_STATE_PUNCH;
            f->state_time = 0;
            f->attack_has_hit = 0;
            f->attack_id++;
        } else if (((pressed & SAT_PAD_B) != 0u) || ((pressed & SAT_PAD_Y) != 0u)) {
            f->state = IK_STATE_KICK;
            f->state_time = 0;
            f->attack_has_hit = 0;
            f->attack_id++;
        } else if (moved) {
            if (f->state != IK_STATE_WALK) {
                f->state = IK_STATE_WALK;
                f->state_time = 0;
            }
        } else {
            if (f->state != IK_STATE_IDLE && f->state != IK_STATE_GUARD) {
                f->state = IK_STATE_IDLE;
                f->state_time = 0;
            }
        }
    } else {
        /* Training dummy: idle breathing, guard window handled in damage. */
        if (f->state != IK_STATE_GUARD) {
            f->state = IK_STATE_IDLE;
        } else if (f->state_time > 30) {
            f->state = IK_STATE_IDLE;
            f->state_time = 0;
        }
    }

    /* Pushboxes: never let bodies interpenetrate. */
    {
        int l0, t0, r0, b0, l1, t1, r1, b1;
        ik_body_box(f, &l0, &t0, &r0, &b0);
        ik_body_box(foe, &l1, &t1, &r1, &b1);
        if (ik_boxes_overlap(l0, t0, r0, b0, l1, t1, r1, b1)) {
            int16_t mid = (int16_t)((f->x + foe->x) / 2);
            if (f->x <= foe->x) {
                f->x = clamp16((int16_t)(f->x - 1), IK_STAGE_MIN_X, mid);
            } else {
                f->x = clamp16((int16_t)(f->x + 1), mid, IK_STAGE_MAX_X);
            }
        }
    }
}

void ik_fight_update(ik_fight_t* fight, const sat_pad_state_t* p1_pad,
                     const sat_pad_state_t* p2_pad) {
    if (!fight) return;
    /* START on P1 resets (training convenience). */
    if (p1_pad && ((p1_pad->pressed & SAT_PAD_START) != 0u)) {
        ik_fight_reset(fight);
        return;
    }
    fight->events = IK_EVENT_NONE;
    if (fight->round_over) {
        if (fight->ko_freeze > 0) fight->ko_freeze--;
        fight->frame++;
        return;
    }
    fight->frame++;
    if (fight->timer_frames > 0) {
        fight->timer_frames--;
        if (fight->timer_frames == 0) {
            fight->round_over = 1;
            if (fight->fighters[0].hp >= fight->fighters[1].hp) fight->winner = 1;
            else fight->winner = 2;
            fight->events |= IK_EVENT_ROUND_OVER;
            return;
        }
    }

    int dummy = (p2_pad == NULL);
    step_fighter(fight, 0, p1_pad, 0);
    step_fighter(fight, 1, p2_pad, dummy);

    /* Resolve active attack boxes vs body boxes (both directions). */
    for (int atk = 0; atk < 2; atk++) {
        ik_fighter_t* a = &fight->fighters[atk];
        ik_fighter_t* v = &fight->fighters[atk ^ 1];
        if (a->attack_has_hit) continue;
        int al, at, ar, ab, vl, vt, vr, vb;
        if (!ik_attack_box(a, &al, &at, &ar, &ab)) continue;
        ik_body_box(v, &vl, &vt, &vr, &vb);
        if (ik_boxes_overlap(al, at, ar, ab, vl, vt, vr, vb)) {
            int dmg = (a->state == IK_STATE_PUNCH) ? IK_PUNCH_DAMAGE : IK_KICK_DAMAGE;
            a->attack_has_hit = 1;
            apply_damage(fight, atk ^ 1, dmg);
        }
    }
}

const char* ik_fight_status_text(const ik_fight_t* fight) {
    if (!fight) return NULL;
    if (fight->round_over) {
        if (fight->winner == 1) return "P1 WINS - START RESETS";
        if (fight->winner == 2) return "P2 WINS - START RESETS";
        return "TIME OVER - START RESETS";
    }
    return NULL;
}

int ik_fight_status_needs_start(const ik_fight_t* fight) {
    if (!fight) return 0;
    return fight->round_over != 0;
}
