#ifndef IKEMEN_FIGHT_INTERNAL_H
#define IKEMEN_FIGHT_INTERNAL_H

/* Shared by the ikemen_fight_*.c translation units only. The public API lives
 * in ikemen_fight.h. Small lookups are static inline so the split costs no
 * call overhead on the SH-2; everything that crosses a file is ikf_-prefixed. */

#include "ikemen_fight.h"
#include "ikemen_entity_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Result of one controller handler (see ikemen_fight_ctrl.c). */
enum {
    IKF_CTRL_NEXT = 0,       /* keep evaluating the remaining controllers */
    IKF_CTRL_END_TICK = 1,   /* stop: the state changed, run the new one */
    IKF_CTRL_END_PASS = 2,   /* stop, but report 0 (ChangeAnim) */
    IKF_CTRL_UNHANDLED = 3   /* this family does not own the type */
};

typedef struct ik_ctrl_exec {
    ik_fight_t* fight;
    ik_fighter_t* fighter;
    const ik_cns_controller_t* ctrl;
    const ik_cns_controller_context_t* context;
    const ik_cns_asset_t* state_cns;
    const ik_cns_asset_t* native_cns;
    const ik_frame_table_t* frames;
    uint8_t index;
    uint16_t anim_elem;
    uint8_t anim_ended;
} ik_ctrl_exec_t;

/* ---- small lookups (inline) ---- */
static inline int16_t clamp16(int16_t v, int16_t lo, int16_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline int32_t clamp_q8(int32_t v, int16_t lo, int16_t hi) {
    const int32_t qlo = (int32_t)lo * IK_CNS_Q8_ONE;
    const int32_t qhi = (int32_t)hi * IK_CNS_Q8_ONE;
    if (v < qlo) return qlo;
    if (v > qhi) return qhi;
    return v;
}

static inline const ik_cns_asset_t* cns_for_owner(
    const ik_fight_t* fight,
    uint8_t owner
) {
    if (!fight) return 0;
    if (owner < 2u && fight->player_cns[owner]) {
        return fight->player_cns[owner];
    }
    return fight->cns;
}

static inline const ik_cns_asset_t* cns_for_fighter(
    const ik_fight_t* fight,
    const ik_fighter_t* fighter
) {
    if (!fighter) return fight ? fight->cns : 0;
    return cns_for_owner(fight, fighter->state_owner);
}

static inline const ik_cns_constants_t* constants_for_fighter(
    const ik_fight_t* fight,
    const ik_fighter_t* fighter
) {
    const ik_cns_asset_t* cns = fighter
        ? cns_for_owner(fight, fighter->owner_player)
        : 0;
    return cns ? &cns->constants : 0;
}

static inline const ik_frame_table_t* frames_for_owner(
    const ik_fight_t* fight,
    uint8_t owner
) {
    if (!fight || owner >= 2u) return 0;
    return fight->player_frames[owner];
}

static inline const ik_frame_table_t* frames_for_fighter(
    const ik_fight_t* fight,
    const ik_fighter_t* fighter
) {
    return fighter
        ? frames_for_owner(fight, fighter->anim_owner)
        : 0;
}

static inline const ik_frame_table_t* frames_for_entity(
    const ik_fight_t* fight,
    const ik_entity_t* entity
) {
    return entity
        ? frames_for_owner(fight, entity->anim_owner)
        : 0;
}

static inline void sync_position(ik_fighter_t* f) {
    if (!f) return;
    f->x = ik_cns_q8_to_int(f->x_q8);
    f->y = ik_cns_q8_to_int(f->y_q8);
}

static inline void set_position(ik_fighter_t* f, int16_t x, int16_t y) {
    f->x = x;
    f->y = y;
    f->x_q8 = (int32_t)x * IK_CNS_Q8_ONE;
    f->y_q8 = (int32_t)y * IK_CNS_Q8_ONE;
}

static inline const ik_cns_state_t* fighter_state_spec(
    const ik_fight_t* fight,
    const ik_fighter_t* fighter
) {
    return fighter
        ? ik_cns_find_state(cns_for_fighter(fight, fighter), fighter->state)
        : 0;
}

static inline const ik_cns_state_t* entity_state_spec(
    const ik_fight_t* fight,
    const ik_entity_t* entity
) {
    return entity
        ? ik_cns_find_state(
            cns_for_owner(fight, entity->state_owner), entity->state_no)
        : 0;
}

static inline int is_attack_fighter(
    const ik_fight_t* fight,
    const ik_fighter_t* fighter
) {
    const ik_cns_state_t* spec = fighter_state_spec(fight, fighter);
    return spec && spec->move_type == IK_CNS_MOVE_ATTACK;
}

static inline int is_crouch_attack_fighter(
    const ik_fight_t* fight,
    const ik_fighter_t* fighter
) {
    const ik_cns_state_t* spec = fighter_state_spec(fight, fighter);
    return spec && spec->move_type == IK_CNS_MOVE_ATTACK &&
           spec->state_type == IK_CNS_STATE_CROUCH;
}

static inline int is_air_attack_fighter(
    const ik_fight_t* fight,
    const ik_fighter_t* fighter
) {
    const ik_cns_state_t* spec = fighter_state_spec(fight, fighter);
    return spec && spec->move_type == IK_CNS_MOVE_ATTACK &&
           spec->state_type == IK_CNS_STATE_AIR;
}

static inline uint16_t controls_command_mask(
    const ik_fight_controls_t* controls
) {
    uint16_t command_mask = 0u;
    if (!controls) return 0u;
    if (controls->forward) command_mask |= IK_CNS_COMMAND_HOLD_FWD;
    if (controls->back) command_mask |= IK_CNS_COMMAND_HOLD_BACK;
    if (controls->up) command_mask |= IK_CNS_COMMAND_HOLD_UP;
    if (controls->down) command_mask |= IK_CNS_COMMAND_HOLD_DOWN;
    if (controls->recovery) command_mask |= IK_CNS_COMMAND_RECOVERY;
    if (controls->a) command_mask |= IK_CNS_COMMAND_A;
    if (controls->b) command_mask |= IK_CNS_COMMAND_B;
    return command_mask;
}

/* Physics of the current state; physics U (guard states) follows the type. */
static inline int state_physics(
    const ik_fighter_t* f,
    const ik_cns_state_t* spec
) {
    if (!spec) return IK_CNS_PHYS_NONE;
    if (spec->physics != IK_CNS_PHYS_BY_TYPE) return spec->physics;
    switch (f->cur_state_type) {
        case IK_CNS_STATE_STAND: return IK_CNS_PHYS_STAND;
        case IK_CNS_STATE_CROUCH: return IK_CNS_PHYS_CROUCH;
        case IK_CNS_STATE_AIR: return IK_CNS_PHYS_AIR;
        default: return IK_CNS_PHYS_NONE;
    }
}

/* A fighter's state_time advances at the end of its own tick, but contact
 * resolution (which runs after every fighter acted) must see the Time the
 * controllers evaluated with. anim_time only advances after contacts, so it is
 * already the evaluation value there. */
static inline uint16_t tick_state_time(const ik_fighter_t* f) {
    return f->state_time > 0u ? (uint16_t)(f->state_time - 1u) : 0u;
}

/* ---- camera ---- */
void ikf_camera_reset(ik_fight_t* fight);
void ikf_camera_step(ik_fight_t* fight);

/* ---- controller families ---- */
int ikf_ctrl_apply_fx(const ik_ctrl_exec_t* x);
int ikf_ctrl_apply_spawn(const ik_ctrl_exec_t* x);
int ikf_ctrl_apply_state(const ik_ctrl_exec_t* x);
int ikf_ctrl_apply_anim(const ik_ctrl_exec_t* x);
int ikf_ctrl_apply_motion(const ik_ctrl_exec_t* x);
int ikf_ctrl_apply_target(const ik_ctrl_exec_t* x);

/* ---- State transitions: ChangeState bookkeeping and controlled input ---- */
void ikf_enter_state(ik_fight_t* fight, ik_fighter_t* f, int16_t state);
void ikf_enter_state_deferred(
    ik_fight_t* fight,
    ik_fighter_t* f,
    int16_t state);
void ikf_init_statedef(ik_fight_t* fight, ik_fighter_t* f);
int ikf_dispatch_controlled_input(
    ik_fight_t* fight,
    ik_fighter_t* f,
    const ik_fight_controls_t* controls);

/* ---- Body/Clsn geometry, animation position and HitDef distance checks ---- */
void ikf_anim_position_at(
    const ik_frame_table_t* frames,
    int16_t action,
    uint16_t anim_time,
    uint16_t* out_element,
    uint16_t* out_element_time,
    int* out_ended);
void ikf_anim_position(
    const ik_frame_table_t* frames,
    const ik_fighter_t* fighter,
    uint16_t* out_element,
    uint16_t* out_element_time,
    int* out_ended);
uint16_t ikf_anim_element_start_tick(
    const ik_frame_table_t* frames,
    int16_t action,
    uint16_t element);
int ikf_hitdef_trigger_now(
    const ik_frame_table_t* frames,
    int16_t action,
    uint16_t state_time,
    uint16_t anim_time,
    uint16_t anim_element,
    uint16_t anim_element_time,
    int anim_ended,
    uint8_t trigger_kind,
    int16_t trigger_value);
int ikf_fighter_clsn_overlap(
    const ik_frame_table_t* attacker_frames,
    const ik_frame_table_t* victim_frames,
    const ik_fighter_t* attacker,
    const ik_fighter_t* victim);
/* Any hurt box (Clsn2) of `a` against any of `b`: the Mugen-style extra
 * condition for player push. */
int ikf_fighter_hurt_overlap(
    const ik_frame_table_t* a_frames,
    const ik_frame_table_t* b_frames,
    const ik_fighter_t* a,
    const ik_fighter_t* b);
int ikf_fighter_reversal_clsn_overlap(
    const ik_frame_table_t* defender_frames,
    const ik_frame_table_t* attacker_frames,
    const ik_fighter_t* defender,
    const ik_fighter_t* attacker);
void ikf_entity_anim_position(
    const ik_frame_table_t* frames,
    const ik_entity_t* entity,
    uint16_t* out_element,
    uint16_t* out_element_time,
    int* out_ended);
int ikf_entity_reversal_clsn_overlap(
    const ik_frame_table_t* defender_frames,
    const ik_frame_table_t* attacker_frames,
    const ik_fighter_t* defender,
    const ik_entity_t* attacker);
int ikf_entity_clsn_overlap(
    const ik_frame_table_t* attacker_frames,
    const ik_frame_table_t* victim_frames,
    const ik_entity_t* attacker,
    const ik_fighter_t* victim);
int ikf_entity_attack_clsn_overlap(
    const ik_frame_table_t* a_frames,
    const ik_frame_table_t* b_frames,
    const ik_entity_t* a,
    const ik_entity_t* b);
int ikf_entity_body_dist_x(
    const ik_entity_t* attacker,
    const ik_fighter_t* victim);
int ikf_body_dist_x(const ik_fighter_t* attacker, const ik_fighter_t* victim);
int ikf_hitdef_p2_dist_allows(
    const ik_cns_hitdef_t* hitdef,
    int p2_body_dist_x);

/* ---- Which HitDef/ReversalDef/HitOverride is currently active ---- */
uint8_t ikf_reversal_state_bit(
    const ik_fight_t* fight,
    const ik_fighter_t* attacker);
int ikf_fighter_not_hit_by_blocks(
    const ik_fight_t* fight,
    const ik_fighter_t* victim,
    uint8_t attacker_state_bit,
    const ik_cns_hitdef_t* incoming);
const ik_cns_reversaldef_t* ikf_active_reversaldef(
    const ik_fight_t* fight,
    const ik_fighter_t* defender,
    const ik_fighter_t* attacker,
    const ik_cns_hitdef_t* incoming);
uint8_t ikf_reversal_entity_state_bit(const ik_entity_t* attacker );
const ik_cns_reversaldef_t* ikf_active_reversaldef_entity(
    const ik_fight_t* fight,
    const ik_fighter_t* defender,
    const ik_entity_t* attacker,
    const ik_cns_hitdef_t* incoming);
const ik_cns_hitoverride_t* ikf_active_hitoverride(
    const ik_fight_t* fight,
    const ik_fighter_t* victim,
    const ik_cns_hitdef_t* incoming);
const ik_cns_hitdef_t* ikf_active_hitdef(
    ik_fight_t* fight,
    const ik_frame_table_t* frames,
    ik_fighter_t* fighter,
    const ik_fighter_t* victim,
    uint8_t* out_local_index);
const ik_cns_hitdef_t* ikf_active_entity_hitdef(
    ik_fight_t* fight,
    const ik_frame_table_t* frames,
    ik_entity_t* entity,
    const ik_fighter_t* victim,
    uint8_t* out_local_index);

/* ---- Hit eligibility: target attributes, chains, juggle, guard threat ---- */
void ikf_advance_projectile_query_times(ik_fight_t* fight );
void ikf_mark_projectile_contact(
    ik_fight_t* fight,
    ik_entity_handle_t projectile_handle,
    int guarded);
int ikf_projectile_contact_consumed(
    ik_fight_t* fight,
    ik_entity_handle_t handle,
    int canceled);
int ikf_hitdef_allows_target(
    const ik_fight_t* fight,
    const ik_fighter_t* victim,
    const ik_cns_hitdef_t* hitdef);
int ikf_hitdef_chain_allows_target(
    const ik_fighter_t* victim,
    uint8_t attacker_owner,
    const ik_cns_hitdef_t* hitdef);
int ikf_juggle_cost(
    const ik_fight_t* fight,
    const ik_fighter_t* attacker,
    const ik_cns_hitdef_t* hitdef);
int ikf_is_juggle_target(const ik_fight_t* fight, const ik_fighter_t* victim);
int ikf_juggle_allows_target(
    const ik_fight_t* fight,
    const ik_fighter_t* attacker,
    const ik_fighter_t* victim,
    const ik_cns_hitdef_t* hitdef);
int ikf_entity_juggle_cost(
    const ik_fight_t* fight,
    const ik_entity_t* attacker,
    const ik_cns_hitdef_t* hitdef);
int ikf_entity_juggle_allows_target(
    const ik_fight_t* fight,
    const ik_entity_t* attacker,
    const ik_fighter_t* victim,
    const ik_cns_hitdef_t* hitdef);
int ikf_is_active_guard_state(int16_t state);
int ikf_in_guard_state(int16_t state);
uint8_t ikf_guard_type_for(
    const ik_fight_t* fight,
    const ik_fighter_t* f,
    const ik_fight_controls_t* controls);
void ikf_update_guard_dist(ik_fight_t* fight);
/* The acceleration the fighter's current state applies as a VelAdd controller
 * this tick (0 when it applies none, or has not started yet). */
int32_t ikf_pre_move_gravity(const ik_fight_t* fight, const ik_fighter_t* f);
/* Q8.16 view of a Q8.8 field plus its fraction byte. */
static inline int32_t ikf_fine_get(int32_t q8, const ik_fine_t* e) {
    return (int32_t)((uint32_t)q8 << 8) + (e->q8_ref == q8 ? e->lo : 0);
}
static inline void ikf_fine_set(int32_t fine, int32_t* q8, ik_fine_t* e) {
    *q8 = fine / 256;                       /* toward zero, like a float cast */
    e->q8_ref = *q8;
    e->lo = (int16_t)(fine - *q8 * 256);
}
static inline int32_t ikf_q16_or_q8(int32_t q16, int32_t q8) {
    return q16 != 0 ? q16 : (int32_t)((uint32_t)q8 << 8);
}

/* The common get-hit states that fly under a VelAdd of GetHitVar(yaccel):
 * air, physics N, no velset (the shake states carry velset 0,0). Custom
 * states run only the acceleration their own controllers name. */
static inline int ikf_is_hit_flight_state(const ik_cns_state_t* spec) {
    if (!spec || spec->move_type != IK_CNS_MOVE_HIT ||
        spec->state_type != IK_CNS_STATE_AIR ||
        spec->physics != IK_CNS_PHYS_NONE || spec->has_velset) {
        return 0;
    }
    switch (spec->number) {
        case 5030: case 5035: case 5040: case 5050: case 5071: case 5200:
            return 1;
        default:
            return 0;
    }
}

/* Upstream's gethitAnimtype(): fall > air > ground, where a Back/Up/DiagUp
 * ground hit that does not launch plays as Hard. */
static inline uint8_t ikf_gethit_anim_type(
    const ik_cns_hitdef_t* hitdef, int fall, int airborne, int16_t velocity_y
) {
    if (fall) return hitdef->fall_anim_type;
    if (airborne) return hitdef->air_anim_type;
    return (hitdef->anim_type >= 3u && velocity_y == 0)
        ? 2u : hitdef->anim_type;
}

/* Char.autoTurn: face the foe when it is behind, playing the turn anim. */
void ikf_auto_turn(ik_fight_t* fight, ik_fighter_t* f);
/* The turn a ChangeState from the fighter's own controllers does first. */
void ikf_turn_before_change(ik_fight_t* fight, ik_fighter_t* f);

/* The victim offset HitDef mindist/maxdist/snap asks for. */
void ikf_hit_snap(const ik_fighter_t* attacker, ik_fighter_t* victim,
                  const ik_cns_hitdef_t* hitdef, int guarded);
void ikf_credit_hit_power(
    ik_fighter_t* attacker,
    ik_fighter_t* victim,
    const ik_cns_hitdef_t* hitdef,
    int guarded);
int ikf_can_guard_hit(
    const ik_fight_t* fight,
    const ik_fighter_t* victim,
    const ik_fight_controls_t* controls,
    const ik_cns_hitdef_t* hitdef);

/* ---- Sound and effect events queued for the presentation layer ---- */
void ikf_queue_sound_event(ik_fight_t* fight, int16_t group, int16_t item );
void ikf_queue_reversal_effect(
    ik_fight_t* fight,
    const ik_fighter_t* defender,
    const ik_cns_reversaldef_t* reversal);
void ikf_queue_hit_effect(
    ik_fight_t* fight,
    const ik_fighter_t* attacker,
    const ik_fighter_t* victim,
    const ik_cns_hitdef_t* hitdef,
    int16_t action,
    int use_trigger2_y);
void ikf_queue_entity_hit_effect(
    ik_fight_t* fight,
    const ik_entity_t* attacker,
    const ik_fighter_t* victim,
    const ik_cns_hitdef_t* hitdef,
    int16_t action,
    int use_trigger2_y);

/* ---- Applying a resolved hit from a root fighter: guard, throw, damage ---- */
void ikf_release_bound_target(ik_fight_t* fight, int owner);
void ikf_exit_target(ik_fight_t* fight, ik_fighter_t* f);
void ikf_release_entity_bound_fighter(
    ik_fight_t* fight,
    ik_fighter_t* fighter);
void ikf_apply_guard(
    ik_fight_t* fight,
    int victim,
    const ik_fight_controls_t* controls,
    const ik_cns_hitdef_t* hitdef);
void ikf_apply_throw(
    ik_fight_t* fight,
    int attacker,
    const ik_fight_controls_t* attacker_controls,
    const ik_cns_hitdef_t* hitdef);
void ikf_apply_damage(
    ik_fight_t* fight,
    int victim,
    const ik_cns_hitdef_t* hitdef);

/* ---- Applying a resolved hit from a helper/projectile owner ---- */
void ikf_apply_guard_from_entity(
    ik_fight_t* fight,
    ik_entity_handle_t attacker_handle,
    int victim,
    const ik_fight_controls_t* controls,
    const ik_cns_hitdef_t* hitdef);
void ikf_apply_damage_from_entity(
    ik_fight_t* fight,
    ik_entity_handle_t attacker_handle,
    int victim,
    const ik_cns_hitdef_t* hitdef,
    const ik_frame_table_t* p1_frames,
    const ik_frame_table_t* p2_frames);
void ikf_apply_throw_from_entity(
    ik_fight_t* fight,
    ik_entity_handle_t attacker_handle,
    int victim,
    const ik_cns_hitdef_t* hitdef,
    const ik_frame_table_t* p1_frames,
    const ik_frame_table_t* p2_frames);

/* ---- Fighter <-> entity pool mirroring and the entity runtime bridge ---- */
uint8_t ikf_fighter_player_index(
    const ik_fight_t* fight,
    const ik_fighter_t* fighter);
ik_entity_handle_t ikf_fighter_entity_handle(
    const ik_fight_t* fight,
    const ik_fighter_t* fighter);
ik_entity_t* ikf_fighter_entity(
    ik_fight_t* fight,
    const ik_fighter_t* fighter);
void ikf_sync_fighter_entity(ik_fight_t* fight, const ik_fighter_t* fighter );
void ikf_sync_player_entities(ik_fight_t* fight);
void ikf_configure_fight_entity_runtime(
    ik_fight_t* fight,
    ik_entity_runtime_t* runtime);

/* ---- Ground and air physics integration ---- */
void ikf_push_fighters(ik_fight_t* fight, const ik_frame_table_t* p1_frames,
                       const ik_frame_table_t* p2_frames);
void ikf_finish_tick(ik_fight_t* fight);
void ikf_apply_ground_velocity(
    ik_fighter_t* f,
    const ik_cns_constants_t* c,
    int physics);
void ikf_step_air(
    ik_fight_t* fight,
    ik_fighter_t* f,
    int allow_land_transition,
    int16_t guard_land_state);

/* ---- Per-fighter tick: timers, guard stance, controllers, physics ---- */
void ikf_step_fighter(
    ik_fight_t* fight,
    int index,
    const ik_fight_controls_t* controls,
    int is_dummy,
    const ik_frame_table_t* frames,
    const ik_frame_table_t* foe_frames);

/* ---- Paused-owner, projectile-trade and entity contact resolution ---- */
void ikf_resolve_paused_owner_contact(
    ik_fight_t* fight,
    int attacker_index,
    const ik_fight_controls_t* p1,
    const ik_fight_controls_t* p2,
    const ik_frame_table_t* p1_frames,
    const ik_frame_table_t* p2_frames);
void ikf_resolve_projectile_trades(
    ik_fight_t* fight,
    const ik_frame_table_t* p1_frames,
    const ik_frame_table_t* p2_frames);
void ikf_resolve_entity_contacts(
    ik_fight_t* fight,
    const ik_fight_controls_t* p1,
    const ik_fight_controls_t* p2,
    const ik_frame_table_t* p1_frames,
    const ik_frame_table_t* p2_frames,
    int owner_filter);

/* ---- Global contact queue: gather, arbitrate, apply ---- */
void ikf_resolve_global_contacts(
    ik_fight_t* fight,
    const ik_fight_controls_t* p1,
    const ik_fight_controls_t* p2,
    const ik_frame_table_t* p1_frames,
    const ik_frame_table_t* p2_frames);

/* ---- Controller executor ---- */
int ikf_process_cns_controllers(
    ik_fight_t* fight,
    ik_fighter_t* f,
    const ik_fight_controls_t* controls,
    const ik_frame_table_t* frames,
    int hit_pause_only);

#ifdef __cplusplus
}
#endif

#endif
