#include <cstdio>
#include <cstdlib>

#include "examples/ikemen_saturn/ikemen_entity_runtime.h"
#include "examples/ikemen_saturn/ikemen_fight.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); std::exit(1); } } while (0)
#define EQ(a,b) OK((a)==(b))

static const ik_clsn_box_t k_boxes[] = {
    {-24,-40,24,0},
    {-20,-60,20,0},
};

static const ik_frame_t k_frames[] = {
    {0,0,32,64,16,64,0,0,0,0,0,1,1},
    {200,0,48,64,24,64,0,0,0,0,1,0,0},
};

static const ik_frame_table_t k_table = {
    k_frames,2u,k_boxes,2u
};

static ik_cns_state_t make_state(
    int16_t number,
    int16_t anim,
    uint8_t move_type,
    uint16_t hitdef_ofs,
    uint8_t hitdef_count
) {
    ik_cns_state_t state{};
    state.number=number;
    state.anim=anim;
    state.state_type=IK_CNS_STATE_STAND;
    state.move_type=move_type;
    state.physics=IK_CNS_PHYS_NONE;
    state.ctrl=(uint8_t)(move_type==IK_CNS_MOVE_IDLE);
    state.hitdef_ofs=hitdef_ofs;
    state.hitdef_count=hitdef_count;
    return state;
}

static ik_cns_hitdef_t make_hitdef() {
    ik_cns_hitdef_t hit{};
    hit.state_number=200;
    hit.trigger_kind=IK_CNS_TRIGGER_TIME_EQ;
    hit.trigger_value=0;
    hit.damage=25;
    hit.guard_damage=5;
    hit.priority=4u;
    hit.pause_p1=3u;
    hit.pause_p2=2u;
    hit.ground_type=IK_CNS_GROUND_HIGH;
    hit.ground_hit_time=6u;
    hit.air_hit_time=6u;
    hit.guard_flags=IK_CNS_GUARD_STAND;
    hit.guard_kill=1u;
    hit.guard_hit_time=5u;
    hit.guard_slide_time=4u;
    hit.guard_ctrl_time=5u;
    hit.guard_velocity_x_q8=-2*IK_CNS_Q8_ONE;
    hit.hit_flags=IK_CNS_HIT_DEFAULT;
    hit.priority_type=IK_CNS_PRIORITY_HIT;
    hit.spark_no=-1;
    hit.guard_spark_no=-1;
    hit.p1_state_no=-1;
    hit.p2_state_no=-1;
    hit.alt_damage=-1;
    hit.alt_damage_prev_state=-32768;
    hit.fall_recover=1u;
    return hit;
}

static ik_cns_asset_t make_asset(
    const ik_cns_state_t* states,
    const ik_cns_hitdef_t* hitdefs
) {
    ik_cns_asset_t asset{};
    asset.constants.life=1000;
    asset.constants.ground_back=15;
    asset.constants.ground_front=16;
    asset.constants.air_back=12;
    asset.constants.air_front=12;
    asset.constants.height=60;
    asset.constants.attack_dist=160;
    asset.constants.air_juggle=15;
    asset.constants.yaccel_q8=113;
    asset.constants.stand_friction_q8=218;
    asset.constants.crouch_friction_q8=210;
    asset.states=states;
    asset.state_count=2u;
    asset.hitdefs=hitdefs;
    asset.hitdef_count=1u;
    return asset;
}

static void bind_players(
    ik_fight_t* fight,
    ik_entity_pool_t* pool,
    ik_entity_handle_t* p1,
    ik_entity_handle_t* p2
) {
    ik_entity_pool_init(pool);
    OK(ik_entity_spawn(
        pool,IK_ENTITY_PLAYER,1,0u,
        ik_entity_invalid_handle(),p1));
    OK(ik_entity_spawn(
        pool,IK_ENTITY_PLAYER,2,1u,
        ik_entity_invalid_handle(),p2));
    ik_fight_bind_entities(fight,pool,*p1,*p2);

    ik_entity_t* e1=ik_entity_get(pool,*p1);
    ik_entity_t* e2=ik_entity_get(pool,*p2);
    OK(e1 && e2);
    e1->x_q8=fight->fighters[0].x_q8;
    e1->y_q8=fight->fighters[0].y_q8;
    e1->facing=fight->fighters[0].facing;
    e1->power=fight->fighters[0].power;
    e2->x_q8=fight->fighters[1].x_q8;
    e2->y_q8=fight->fighters[1].y_q8;
    e2->facing=fight->fighters[1].facing;
}

static ik_entity_handle_t spawn_attacker(
    ik_fight_t* fight,
    ik_entity_pool_t* pool,
    ik_entity_handle_t p1
) {
    ik_entity_runtime_t runtime{};
    ik_entity_runtime_init(
        &runtime,pool,fight->cns,&k_table,&k_table);
    const ik_cns_helper_t helper={
        77,200,
        100*IK_ENTITY_Q8_ONE,0,
        1,IK_CNS_HELPER_POS_P1,0u,0u
    };
    ik_entity_handle_t handle{};
    OK(ik_entity_runtime_spawn_helper(
        &runtime,p1,&helper,&handle));
    return handle;
}

int main() {
    ik_cns_state_t states[2] = {
        make_state(0,0,IK_CNS_MOVE_IDLE,0u,0u),
        make_state(200,200,IK_CNS_MOVE_ATTACK,0u,1u),
    };
    ik_cns_hitdef_t hitdef=make_hitdef();
    ik_cns_asset_t asset=make_asset(states,&hitdef);

    {
        ik_fight_t fight{};
        ik_fight_init(&fight,&asset);
        ik_entity_pool_t pool{};
        ik_entity_handle_t p1{},p2{};
        bind_players(&fight,&pool,&p1,&p2);
        const ik_entity_handle_t helper=
            spawn_attacker(&fight,&pool,p1);

        ik_fight_controls_t idle{};
        ik_fight_update(
            &fight,&idle,&idle,&k_table,&k_table);

        const ik_entity_t* entity=
            ik_entity_get_const(&pool,helper);
        OK(entity!=nullptr);
        EQ(fight.fighters[1].hp-fight.fighters[1].pending_damage,975);
        EQ(fight.hits_p1,1u);
        EQ(entity->move_contact,1u);
        EQ(entity->hit_pause,3u);
        EQ(entity->hitdef_hit_mask,1u);
        EQ(fight.fighters[1].hit_shake_time,2u);

        ik_fight_update(
            &fight,&idle,&idle,&k_table,&k_table);
        entity=ik_entity_get_const(&pool,helper);
        OK(entity!=nullptr);
        EQ(fight.fighters[1].hp-fight.fighters[1].pending_damage,975);
        EQ(entity->hit_pause,2u);
        EQ(entity->state_time,1u); /* paused: the clock stays after tick 1 */
    }

    {
        ik_fight_t fight{};
        ik_fight_init(&fight,&asset);
        ik_entity_pool_t pool{};
        ik_entity_handle_t p1{},p2{};
        bind_players(&fight,&pool,&p1,&p2);
        const ik_entity_handle_t helper=
            spawn_attacker(&fight,&pool,p1);

        ik_fight_controls_t p1_controls{};
        ik_fight_controls_t p2_controls{};
        p2_controls.back=1u;
        ik_fight_update(
            &fight,&p1_controls,&p2_controls,
            &k_table,&k_table);

        const ik_entity_t* entity=
            ik_entity_get_const(&pool,helper);
        OK(entity!=nullptr);
        EQ(fight.fighters[1].hp,995);
        EQ(fight.hits_p1,0u);
        OK((fight.events & IK_EVENT_GUARD)!=0u);
        EQ(entity->move_contact,1u);
        EQ(entity->hit_pause,3u);
        EQ(entity->hitdef_hit_mask,1u);
    }

    std::puts("[test] ikemen_helper_combat OK");
    return 0;
}
