#include <cstdio>
#include <cstdlib>

#include "examples/ikemen_saturn/ikemen_entity_runtime.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)
#define EQ(a,b) OK((a)==(b))

static const ik_frame_t k_frames[] = {
    {200,0,8,8,0,0,8,0,0,0,0,0,0},
};
static const ik_frame_table_t k_table = {
    k_frames,1u,nullptr,0u
};

static const ik_cns_controller_t k_ctrls[] = {
    {200,IK_CNS_CTRL_VAR_SET,IK_CNS_TRIGGER_TIME_EQ,
     0,0,3,10,0u},
    {200,IK_CNS_CTRL_VEL_SET,IK_CNS_TRIGGER_TIME_EQ,
     0,0,256,0,IK_CNS_CTRL_AXIS_X|IK_CNS_CTRL_LOCAL_X},
    {200,IK_CNS_CTRL_DESTROY_SELF,IK_CNS_TRIGGER_TIME_EQ,
     2,0,0,0,0u},
};

static const ik_cns_state_t k_states[] = {
    {200,200,0,0,0,IK_CNS_STATE_STAND,IK_CNS_MOVE_IDLE,IK_CNS_PHYS_NONE,
     0,2,0u,0u,0u,0u,0u,0u,3u,0,0,0u,0u,0,0u,0u,0u},
};

static ik_cns_asset_t make_cns() {
    ik_cns_asset_t asset{};
    asset.constants.life=1000;
    asset.constants.ground_back=15;
    asset.constants.ground_front=16;
    asset.constants.yaccel_q8=113;
    asset.constants.stand_friction_q8=218;
    asset.constants.crouch_friction_q8=210;
    asset.states=k_states;
    asset.state_count=1u;
    asset.controllers=k_ctrls;
    asset.controller_count=3u;
    return asset;
}

static const ik_cns_asset_t k_cns=make_cns();

int main() {
    ik_entity_pool_t pool{};
    ik_entity_pool_init(&pool);

    ik_entity_handle_t p1{}, p2{};
    OK(ik_entity_spawn(
        &pool,IK_ENTITY_PLAYER,1,0u,
        ik_entity_invalid_handle(),&p1));
    OK(ik_entity_spawn(
        &pool,IK_ENTITY_PLAYER,2,1u,
        ik_entity_invalid_handle(),&p2));

    ik_entity_t* parent=ik_entity_get(&pool,p1);
    OK(parent!=nullptr);
    parent->x_q8=100*IK_ENTITY_Q8_ONE;
    parent->y_q8=50*IK_ENTITY_Q8_ONE;
    parent->facing=1;
    parent->power=321;

    ik_entity_runtime_t runtime{};
    ik_entity_runtime_init(&runtime,&pool,&k_cns,&k_table,&k_table);

    const ik_cns_helper_t spec={
        77,200,
        10*IK_ENTITY_Q8_ONE,-5*IK_ENTITY_Q8_ONE,
        -1,IK_CNS_HELPER_POS_P1,1u,1u
    };
    ik_entity_handle_t helper{};
    OK(ik_entity_runtime_spawn_helper(&runtime,p1,&spec,&helper));
    const ik_entity_t* spawned=ik_entity_get_const(&pool,helper);
    OK(spawned!=nullptr);
    EQ(spawned->type,IK_ENTITY_HELPER);
    EQ(spawned->id,77);
    EQ(spawned->x_q8,110*IK_ENTITY_Q8_ONE);
    EQ(spawned->y_q8,45*IK_ENTITY_Q8_ONE);
    EQ(spawned->facing,-1);
    EQ(spawned->power,321);
    OK(ik_entity_handle_equal(spawned->parent,p1));
    OK(ik_entity_handle_equal(spawned->root,p1));

    ik_entity_runtime_step(&runtime);
    spawned=ik_entity_get_const(&pool,helper);
    OK(spawned!=nullptr);
    EQ(spawned->state_time,0u);
    EQ(spawned->vars[3],10);
    EQ(spawned->vx_q8,-256);
    EQ(spawned->x_q8,110*IK_ENTITY_Q8_ONE-256);

    ik_entity_runtime_step(&runtime);
    spawned=ik_entity_get_const(&pool,helper);
    OK(spawned!=nullptr);
    EQ(spawned->state_time,1u);

    ik_entity_runtime_step(&runtime);
    OK(ik_entity_get_const(&pool,helper)==nullptr);
    EQ(ik_entity_count_type(&pool,IK_ENTITY_HELPER),0u);

    std::puts("[test] ikemen_entity_runtime OK");
    return 0;
}
