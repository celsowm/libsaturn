/* Body/Clsn geometry, animation position and HitDef distance checks. */
#include "ikemen_fight_internal.h"

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

void ikf_anim_position_at(const ik_frame_table_t* frames,
                          int16_t action,
                          uint16_t anim_time,
                          uint16_t* out_element,
                          uint16_t* out_element_time,
                          int* out_ended) {
    uint32_t first = 0u;
    uint32_t count = 0u;
    uint16_t element = 1u;
    uint16_t element_time = 0u;
    int ended = 0;

    if (!frames ||
        !ik_frames_bounds(frames, action, &first, &count) ||
        count == 0u) {
        if (out_element) *out_element = element;
        if (out_element_time) *out_element_time = element_time;
        if (out_ended) *out_ended = 0;
        return;
    }

    const uint32_t duration = ik_action_duration_ticks(frames, action);
    if (duration > 0u && anim_time >= duration) ended = 1;

    /* Past the end the last element keeps counting (AnimElem = last is no
     * longer an event: its element time is already above 0). */
    uint32_t remaining = anim_time;
    for (uint32_t i = 0u; i < count; ++i) {
        const uint16_t ticks = ik_frame_ticks(&frames->frames[first + i]);
        if (ticks == 0u || remaining < ticks || i + 1u == count) {
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

void ikf_anim_position(const ik_frame_table_t* frames,
                       const ik_fighter_t* fighter,
                       uint16_t* out_element,
                       uint16_t* out_element_time,
                       int* out_ended) {
    if (!fighter) {
        if (out_element) *out_element = 1u;
        if (out_element_time) *out_element_time = 0u;
        if (out_ended) *out_ended = 0;
        return;
    }
    ikf_anim_position_at(
        frames, fighter->anim, fighter->anim_time,
        out_element, out_element_time, out_ended);
}

uint16_t ikf_anim_element_start_tick(const ik_frame_table_t* frames,
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

int ikf_hitdef_trigger_now(
    const ik_frame_table_t* frames,
    int16_t action,
    uint16_t state_time,
    uint16_t anim_time,
    uint16_t anim_element,
    uint16_t anim_element_time,
    int anim_ended,
    uint8_t trigger_kind,
    int16_t trigger_value
) {
    if (trigger_kind == IK_CNS_TRIGGER_ANIM_ELEM_TIME_EQ_PACKED) {
        const uint16_t packed = (uint16_t)trigger_value;
        const uint16_t elem = (uint16_t)(packed >> 8);
        const int8_t offset = (int8_t)(packed & 0xffu);
        const uint16_t start =
            ikf_anim_element_start_tick(frames, action, elem);
        const int32_t target = (int32_t)start + (int32_t)offset;
        return target >= 0 && anim_time == (uint16_t)target;
    }
    return ik_cns_trigger_now(
        trigger_kind, trigger_value, state_time,
        anim_element, anim_element_time, anim_ended);
}

/* Strict box overlap, as upstream's ClsnOverlap, in Q8.8 world units. */
static int ik_boxes_overlap_q8(int32_t l0, int32_t t0, int32_t r0, int32_t b0,
                               int32_t l1, int32_t t1, int32_t r1, int32_t b1) {
    return (l0 < r1) && (l1 < r0) && (t0 < b1) && (t1 < b0);
}

int ikf_fighter_clsn_overlap(
    const ik_frame_table_t* attacker_frames,
    const ik_frame_table_t* victim_frames,
    const ik_fighter_t* attacker,
    const ik_fighter_t* victim
) {
    const ik_frame_t* af = fighter_frame(attacker_frames, attacker);
    const ik_frame_t* vf = fighter_frame(victim_frames, victim);
    if (!af || !vf || af->clsn1_count == 0u || vf->clsn2_count == 0u) return 0;

    for (uint16_t ai = 0u; ai < af->clsn1_count; ++ai) {
        int32_t al, at, ar, ab;
        if (!ik_frame_clsn_world_q8(
                attacker_frames, af, IK_CLSN_ATTACK, ai,
                attacker->x_q8, attacker->y_q8, attacker->facing,
                &al, &at, &ar, &ab)) continue;
        for (uint16_t vi = 0u; vi < vf->clsn2_count; ++vi) {
            int32_t vl, vt, vr, vb;
            if (!ik_frame_clsn_world_q8(
                    victim_frames, vf, IK_CLSN_HURT, vi,
                    victim->x_q8, victim->y_q8, victim->facing,
                    &vl, &vt, &vr, &vb)) continue;
            if (ik_boxes_overlap_q8(al, at, ar, ab, vl, vt, vr, vb)) return 1;
        }
    }
    return 0;
}

int ikf_fighter_hurt_overlap(
    const ik_frame_table_t* a_frames,
    const ik_frame_table_t* b_frames,
    const ik_fighter_t* a,
    const ik_fighter_t* b
) {
    const ik_frame_t* af = fighter_frame(a_frames, a);
    const ik_frame_t* bf = fighter_frame(b_frames, b);
    if (!af || !bf) return 0;
    for (uint16_t ai = 0u; ai < af->clsn2_count; ++ai) {
        int32_t al, at, ar, ab;
        if (!ik_frame_clsn_world_q8(
                a_frames, af, IK_CLSN_HURT, ai, a->x_q8, a->y_q8, a->facing,
                &al, &at, &ar, &ab)) continue;
        for (uint16_t bi = 0u; bi < bf->clsn2_count; ++bi) {
            int32_t bl, bt, br, bb;
            if (!ik_frame_clsn_world_q8(
                    b_frames, bf, IK_CLSN_HURT, bi, b->x_q8, b->y_q8,
                    b->facing, &bl, &bt, &br, &bb)) continue;
            if (ik_boxes_overlap_q8(al, at, ar, ab, bl, bt, br, bb)) return 1;
        }
    }
    return 0;
}

int ikf_fighter_reversal_clsn_overlap(
    const ik_frame_table_t* defender_frames,
    const ik_frame_table_t* attacker_frames,
    const ik_fighter_t* defender,
    const ik_fighter_t* attacker
) {
    const ik_frame_t* df = fighter_frame(defender_frames, defender);
    const ik_frame_t* af = fighter_frame(attacker_frames, attacker);
    if (!df || !af || df->clsn1_count == 0u || af->clsn1_count == 0u) {
        return 0;
    }

    for (uint16_t di = 0u; di < df->clsn1_count; ++di) {
        int32_t dl, dt, dr, db;
        if (!ik_frame_clsn_world_q8(
                defender_frames, df, IK_CLSN_ATTACK, di,
                defender->x_q8, defender->y_q8, defender->facing,
                &dl, &dt, &dr, &db)) {
            continue;
        }
        for (uint16_t ai = 0u; ai < af->clsn1_count; ++ai) {
            int32_t al, at, ar, ab;
            if (!ik_frame_clsn_world_q8(
                    attacker_frames, af, IK_CLSN_ATTACK, ai,
                    attacker->x_q8, attacker->y_q8, attacker->facing,
                    &al, &at, &ar, &ab)) {
                continue;
            }
            if (ik_boxes_overlap_q8(dl, dt, dr, db, al, at, ar, ab)) {
                return 1;
            }
        }
    }
    return 0;
}

void ikf_entity_anim_position(
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
    for (uint32_t n = 0u; n < count; ++n) {
        const uint16_t ticks =
            ik_frame_ticks(&frames->frames[first + n]);
        if (ticks == 0u || remaining < ticks) {
            element = (uint16_t)(n + 1u);
            element_time = (uint16_t)remaining;
            break;
        }
        remaining -= ticks;
    }

    if (out_element) *out_element = element;
    if (out_element_time) *out_element_time = element_time;
    if (out_ended) *out_ended = ended;
}

int ikf_entity_reversal_clsn_overlap(
    const ik_frame_table_t* defender_frames,
    const ik_frame_table_t* attacker_frames,
    const ik_fighter_t* defender,
    const ik_entity_t* attacker
) {
    if (!defender_frames || !attacker_frames ||
        !defender || !attacker) {
        return 0;
    }
    const ik_frame_t* df = fighter_frame(defender_frames, defender);
    const ik_frame_t* af = ik_frame_at_time(
        attacker_frames, attacker->anim_no, attacker->anim_time);
    if (!df || !af || df->clsn1_count == 0u ||
        af->clsn1_count == 0u) {
        return 0;
    }

    const int32_t ax = attacker->x_q8;
    const int32_t ay = attacker->y_q8;
    for (uint16_t di = 0u; di < df->clsn1_count; ++di) {
        int32_t dl, dt, dr, db;
        if (!ik_frame_clsn_world_q8(
                defender_frames, df, IK_CLSN_ATTACK, di,
                defender->x_q8, defender->y_q8, defender->facing,
                &dl, &dt, &dr, &db)) {
            continue;
        }
        for (uint16_t ai = 0u; ai < af->clsn1_count; ++ai) {
            int32_t al, at, ar, ab;
            if (!ik_frame_clsn_world_q8(
                    attacker_frames, af, IK_CLSN_ATTACK, ai,
                    ax, ay, attacker->facing,
                    &al, &at, &ar, &ab)) {
                continue;
            }
            if (ik_boxes_overlap_q8(dl, dt, dr, db, al, at, ar, ab)) {
                return 1;
            }
        }
    }
    return 0;
}

int ikf_entity_clsn_overlap(
    const ik_frame_table_t* attacker_frames,
    const ik_frame_table_t* victim_frames,
    const ik_entity_t* attacker,
    const ik_fighter_t* victim
) {
    if (!attacker_frames || !victim_frames || !attacker || !victim) {
        return 0;
    }
    const ik_frame_t* af = ik_frame_at_time(
        attacker_frames, attacker->anim_no, attacker->anim_time);
    const ik_frame_t* vf = fighter_frame(victim_frames, victim);
    if (!af || !vf || af->clsn1_count == 0u ||
        vf->clsn2_count == 0u) {
        return 0;
    }

    const int32_t ax = attacker->x_q8;
    const int32_t ay = attacker->y_q8;
    for (uint16_t ai = 0u; ai < af->clsn1_count; ++ai) {
        int32_t al, at, ar, ab;
        if (!ik_frame_clsn_world_q8(
                attacker_frames, af, IK_CLSN_ATTACK, ai,
                ax, ay, attacker->facing,
                &al, &at, &ar, &ab)) {
            continue;
        }
        for (uint16_t vi = 0u; vi < vf->clsn2_count; ++vi) {
            int32_t vl, vt, vr, vb;
            if (!ik_frame_clsn_world_q8(
                    victim_frames, vf, IK_CLSN_HURT, vi,
                    victim->x_q8, victim->y_q8, victim->facing,
                    &vl, &vt, &vr, &vb)) {
                continue;
            }
            if (ik_boxes_overlap_q8(al, at, ar, ab, vl, vt, vr, vb)) {
                return 1;
            }
        }
    }
    return 0;
}

int ikf_entity_attack_clsn_overlap(
    const ik_frame_table_t* a_frames,
    const ik_frame_table_t* b_frames,
    const ik_entity_t* a,
    const ik_entity_t* b
) {
    if (!a_frames || !b_frames || !a || !b) return 0;
    const ik_frame_t* af = ik_frame_at_time(
        a_frames, a->anim_no, a->anim_time);
    const ik_frame_t* bf = ik_frame_at_time(
        b_frames, b->anim_no, b->anim_time);
    if (!af || !bf || af->clsn1_count == 0u ||
        bf->clsn1_count == 0u) {
        return 0;
    }

    const int32_t ax = a->x_q8;
    const int32_t ay = a->y_q8;
    const int32_t bx = b->x_q8;
    const int32_t by = b->y_q8;
    for (uint16_t ai = 0u; ai < af->clsn1_count; ++ai) {
        int32_t al, at, ar, ab;
        if (!ik_frame_clsn_world_q8(
                a_frames, af, IK_CLSN_ATTACK, ai,
                ax, ay, a->facing, &al, &at, &ar, &ab)) {
            continue;
        }
        for (uint16_t bi = 0u; bi < bf->clsn1_count; ++bi) {
            int32_t bl, bt, br, bb;
            if (!ik_frame_clsn_world_q8(
                    b_frames, bf, IK_CLSN_ATTACK, bi,
                    bx, by, b->facing, &bl, &bt, &br, &bb)) {
                continue;
            }
            if (ik_boxes_overlap_q8(al, at, ar, ab, bl, bt, br, bb)) {
                return 1;
            }
        }
    }
    return 0;
}

int ikf_entity_body_dist_x(
    const ik_entity_t* attacker,
    const ik_fighter_t* victim
) {
    if (!attacker || !victim) return 32767;
    const int x = ik_cns_q8_to_int(attacker->x_q8);
    int value =
        ((int)victim->x - x) * (int)attacker->facing -
        attacker->push_front - victim->push_front;
    if (value < -32768) value = -32768;
    if (value > 32767) value = 32767;
    return value;
}

int ikf_body_dist_x(const ik_fighter_t* attacker,
                       const ik_fighter_t* victim) {
    if (!attacker || !victim) return 32767;
    int value =
        ((int)victim->x - (int)attacker->x) * (int)attacker->facing -
        attacker->push_front - victim->push_front;
    if (value < -32768) value = -32768;
    if (value > 32767) value = 32767;
    return value;
}

int ikf_hitdef_p2_dist_allows(const ik_cns_hitdef_t* hitdef,
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
