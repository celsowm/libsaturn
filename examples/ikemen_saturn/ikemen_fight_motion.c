/* Ground and air physics integration. */
#include "ikemen_fight_internal.h"

void ikf_apply_ground_velocity(ik_fighter_t* f,
                                  const ik_cns_constants_t* c,
                                  int physics) {
    if (!f || !c) return;
    const int crouch = physics == IK_CNS_PHYS_CROUCH;
    const int32_t friction = crouch
        ? ikf_q16_or_q8(c->crouch_friction_q16, c->crouch_friction_q8)
        : ikf_q16_or_q8(c->stand_friction_q16, c->stand_friction_q8);

    /* Q8.16 internally: a Q8.8 friction (0.85 -> 0.8516) would drift the
     * slide by a few percent over a knockback. */
    int32_t x = ikf_fine_get(f->x_q8, &f->fine_x);
    int32_t vx = ikf_fine_get(f->vx_q8, &f->fine_vx);
    x += vx;
    vx = (int32_t)(((int64_t)vx * friction) / 65536);
    /* Upstream's stand physics also drops any speed under 1 px per tick. */
    if (!crouch && vx > -65536 && vx < 65536) vx = 0;
    ikf_fine_set(x, &f->x_q8, &f->fine_x);
    ikf_fine_set(vx, &f->vx_q8, &f->fine_vx);
    sync_position(f);
}

int32_t ikf_pre_move_gravity(const ik_fight_t* fight, const ik_fighter_t* f) {
    const ik_cns_state_t* spec = fighter_state_spec(fight, f);
    if (!spec) return 0;
    if (ikf_is_hit_flight_state(spec)) {
        return f->state_time > 0u
            ? ikf_q16_or_q8(f->gethit_yaccel_q16, f->gethit_yaccel_q8) : 0;
    }
    if (spec->air_motion_start > 0u) {
        return f->state_time >= spec->air_motion_start
            ? ikf_q16_or_q8(spec->air_accel_q16, spec->air_accel_q8) : 0;
    }
    if (spec->physics == IK_CNS_PHYS_NONE && spec->air_accel_q8 != 0 &&
        !spec->owns_air_accel) {
        return ikf_q16_or_q8(spec->air_accel_q16, spec->air_accel_q8);
    }
    return 0;
}

void ikf_step_air(ik_fight_t* fight, ik_fighter_t* f,
                     int allow_land_transition,
                     int16_t guard_land_state) {
    const ik_cns_constants_t* c =
        constants_for_fighter(fight, f);
    const ik_cns_state_t* spec = fighter_state_spec(fight, f);
    /* Only air physics and states that name their own acceleration fall;
     * physics N (guard shake, air jump start, the wall bounce 1027) does not. */
    const int engine_gravity =
        !spec || state_physics(f, spec) == IK_CNS_PHYS_AIR;
    /* Get-hit flight states (5030, 5035, 5040, 5050, 5071, 5200) set their
     * launch velocity on the first tick and run VelAdd from the second. A
     * ChangeState between two of them keeps the VelAdd the old one ran. */
    const int flight = ikf_is_hit_flight_state(spec);
    const int flight_accelerates =
        f->state_time > 0u || f->gravity_carry != 0u;
    const int32_t carried_q16 = f->gravity_carry_q16;
    f->gravity_carry = 0u;
    f->gravity_carry_q16 = 0;
    /* Accelerations are Q16.16: a Q8.8 gravity (0.35 -> 0.3516) piles up to
     * more than a pixel over one fall. */
    const int32_t default_gravity = c
        ? ikf_q16_or_q8(c->yaccel_q16, c->yaccel_q8)
        : (int32_t)(65536 / 2);
    /* Air recovery (5210) runs `VelAdd y = yAccel` from its fourth tick on,
     * as a controller: before the move, while position is frozen earlier. */
    const int timed_gravity = spec && spec->air_motion_start > 0u;
    /* A physics-N state that names its own acceleration (air guard, 132)
     * applies it as a VelAdd controller: before the move. */
    const int state_accel = spec && spec->physics == IK_CNS_PHYS_NONE &&
                            spec->air_accel_q8 != 0 && !spec->owns_air_accel;
    const int32_t gravity =
        timed_gravity
            ? (f->state_time >= spec->air_motion_start
                   ? ikf_q16_or_q8(spec->air_accel_q16, spec->air_accel_q8)
                   : 0)
        : (spec && spec->owns_air_accel)
            ? 0
            : (flight && !flight_accelerates)
                ? 0
            : (state_accel && !flight && !engine_gravity)
                ? ikf_q16_or_q8(spec->air_accel_q16, spec->air_accel_q8)
            : !(flight || engine_gravity)
                ? 0
            : (spec && spec->move_type == IK_CNS_MOVE_HIT &&
               f->gethit_yaccel_q8 != 0)
                ? ikf_q16_or_q8(f->gethit_yaccel_q16, f->gethit_yaccel_q8)
                : (spec && spec->air_accel_q8 != 0)
                    ? ikf_q16_or_q8(spec->air_accel_q16, spec->air_accel_q8)
                    : default_gravity;
    const int32_t floor_q8 = (int32_t)IK_FLOOR_Y * IK_CNS_Q8_ONE;
    int32_t land_level_q8 = spec ? spec->land_level_q8 : 0;
    /* Common state 5050: launch anims (Up/DiagUp) land on the floor itself,
     * the others at the air-gethit ground level. */
    if (f->state == 5050 &&
        ((f->anim >= 5051 && f->anim <= 5059) ||
         (f->anim >= 5061 && f->anim <= 5069))) {
        land_level_q8 = 0;
    }

    /* The flight state this one was entered from ran its VelAdd before it
     * changed state; a non-flight successor (air recovery) keeps it. */
    if (carried_q16 != 0 && !flight) {
        ikf_fine_set(ikf_fine_get(f->vy_q8, &f->fine_vy) + carried_q16,
                     &f->vy_q8, &f->fine_vy);
    }

    if (spec && f->state_time < spec->air_motion_start) {
        sync_position(f);
        return;
    }

    /* Position integrates the velocity this tick's controllers left. Physics
     * A applies gravity after the move; get-hit states with physics N run
     * VelAdd as a controller, so their gravity lands before it. */
    const int controller_gravity =
        flight || timed_gravity || (state_accel && !engine_gravity);
    int32_t x16 = ikf_fine_get(f->x_q8, &f->fine_x);
    int32_t y16 = ikf_fine_get(f->y_q8, &f->fine_y);
    const int32_t vx16 = ikf_fine_get(f->vx_q8, &f->fine_vx);
    int32_t vy16 = ikf_fine_get(f->vy_q8, &f->fine_vy);
    const int32_t land16 = (int32_t)((uint32_t)(floor_q8 + land_level_q8) << 8);
    if (controller_gravity) vy16 += gravity;

    /* A flight state's landing is a controller test: it sees the position
     * from before this tick's move. */
    const int pre_land =
        controller_gravity || (spec && spec->land_before_move);
    int lands = pre_land && vy16 > 0 && y16 >= land16;
    if (!lands) {
        if (!f->pos_freeze_x) x16 += vx16;
        if (!f->pos_freeze_y) y16 += vy16;
        if (!controller_gravity) vy16 += gravity;
        lands = !pre_land && vy16 > 0 && y16 >= land16;
    }
    ikf_fine_set(x16, &f->x_q8, &f->fine_x);
    ikf_fine_set(y16, &f->y_q8, &f->fine_y);
    ikf_fine_set(vy16, &f->vy_q8, &f->fine_vy);

    if (lands) {
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

            const ik_cns_asset_t* native_cns =
                cns_for_owner(fight, f->owner_player);
            if ((f->state == 5030 || f->state == 5035) &&
                ik_cns_find_state(
                    native_cns, f->gethit_fall ? 5050 : 5040)) {
                target = f->gethit_fall ? 5050 : 5040;
            } else if (guard_land_state >= 0 &&
                       (f->state == 132 || f->state == 155)) {
                target = guard_land_state;
            } else if (spec && spec->land_state != 0) {
                target = spec->land_state;
            } else if (ik_cns_find_state(native_cns, 52)) {
                target = 52;
            }
            const uint8_t landing_ctrl = spec ? spec->land_ctrl : 0u;
            /* ChangeState's ctrl applies first; the new StateDef's own
             * ctrl (state 52 has ctrl = 0) then overrides it. */
            if (target == 5040 || landing_ctrl) f->ctrl = 1;
            ikf_enter_state(fight, f, target);
            if (pre_land && !controller_gravity && c) {
                /* The landing controller changed state before the move, so
                 * the new state's ground physics runs this same tick. */
                const ik_cns_state_t* landed = fighter_state_spec(fight, f);
                const int physics = state_physics(f, landed);
                if (physics == IK_CNS_PHYS_STAND ||
                    physics == IK_CNS_PHYS_CROUCH) {
                    ikf_apply_ground_velocity(f, c, physics);
                }
            }
        }
    }
    sync_position(f);
}

/* Players never overlap: when their body boxes cross, both are pushed back by
 * half the overlap (all of it onto the free one against a stage wall), like
 * Ikemen's push detection. Runs once per tick after both fighters moved. */
/* Body widths in pixels. A state change this tick (landing) switches the
 * base widths between the air and ground sizes; Width additions stay. */
static void body_widths(const ik_fight_t* fight, const ik_fighter_t* f,
                        int32_t* back, int32_t* front) {
    int32_t push_back = f->push_back;
    int32_t push_front = f->push_front;
    const ik_cns_constants_t* c = constants_for_fighter(fight, f);
    const int air = f->cur_state_type == IK_CNS_STATE_AIR;
    if (c && air != (f->body_air != 0u)) {
        push_back += (air ? c->air_back : c->ground_back) -
                     (air ? c->ground_back : c->air_back);
        push_front += (air ? c->air_front : c->ground_front) -
                      (air ? c->ground_front : c->air_front);
    }
    *back = push_back;
    *front = push_front;
}

static void body_box_q8(const ik_fight_t* fight, const ik_fighter_t* f,
                        int32_t* l, int32_t* t, int32_t* r, int32_t* b) {
    int32_t push_back, push_front;
    body_widths(fight, f, &push_back, &push_front);
    const int32_t back = push_back * IK_CNS_Q8_ONE;
    const int32_t front = push_front * IK_CNS_Q8_ONE;
    *l = f->x_q8 - (f->facing >= 0 ? back : front);
    *r = f->x_q8 + (f->facing >= 0 ? front : back);
    *t = f->y_q8 - (int32_t)f->body_height * IK_CNS_Q8_ONE;
    *b = f->y_q8;
}

void ikf_push_fighters(ik_fight_t* fight, const ik_frame_table_t* p1_frames,
                       const ik_frame_table_t* p2_frames) {
    ik_fighter_t* a = &fight->fighters[0];
    ik_fighter_t* b = &fight->fighters[1];
    if (a->bound_to >= 0 || b->bound_to >= 0 ||
        ik_entity_handle_is_valid(a->bound_entity) ||
        ik_entity_handle_is_valid(b->bound_entity)) {
        return;
    }
    /* Body (size) boxes in Q8.8 world units, like upstream's floats. */
    int32_t l0, t0, r0, b0, l1, t1, r1, b1;
    body_box_q8(fight, a, &l0, &t0, &r0, &b0);
    body_box_q8(fight, b, &l1, &t1, &r1, &b1);
    if (!(l0 < r1 && l1 < r0 && t0 < b1 && t1 < b0)) return;
    /* Upstream also wants the hurt boxes to touch (unless the character
     * asserts SizePushOnly): a body that flies over the other does not push. */
    const ik_frame_table_t* fa = frames_for_fighter(fight, a);
    const ik_frame_table_t* fb = frames_for_fighter(fight, b);
    if (!ikf_fighter_hurt_overlap(fa ? fa : p1_frames, fb ? fb : p2_frames,
                                  a, b)) {
        return;
    }

    ik_fighter_t* left = a->x_q8 <= b->x_q8 ? a : b;
    ik_fighter_t* right = left == a ? b : a;
    int32_t lback, lfront, rback, rfront;
    body_widths(fight, left, &lback, &lfront);
    body_widths(fight, right, &rback, &rfront);
    const int32_t left_edge_q8 = left->x_q8 +
        (left->facing >= 0 ? lfront : lback) * IK_CNS_Q8_ONE;
    const int32_t right_edge_q8 = right->x_q8 -
        (right->facing >= 0 ? rback : rfront) * IK_CNS_Q8_ONE;
    const int32_t overlap_q8 = left_edge_q8 - right_edge_q8;
    if (overlap_q8 <= 0) return;

    const int32_t min_q8 = a->xmin_q8;
    const int32_t max_q8 = a->xmax_q8;
    int32_t push_left = overlap_q8 / 2;
    int32_t push_right = overlap_q8 - push_left;
    if (left->x_q8 - push_left < min_q8) {
        push_left = left->x_q8 - min_q8;
        push_right = overlap_q8 - push_left;
    } else if (right->x_q8 + push_right > max_q8) {
        push_right = max_q8 - right->x_q8;
        push_left = overlap_q8 - push_right;
    }
    left->x_q8 -= push_left;
    right->x_q8 += push_right;
    sync_position(left);
    sync_position(right);
}
