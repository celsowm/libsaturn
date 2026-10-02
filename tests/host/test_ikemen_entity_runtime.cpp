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
     1,0,3,10,0u},
    {200,IK_CNS_CTRL_VEL_SET,IK_CNS_TRIGGER_TIME_EQ,
     1,0,256,0,IK_CNS_CTRL_AXIS_X|IK_CNS_CTRL_LOCAL_X},
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
        -1,IK_CNS_HELPER_POS_P1,1u,1u,2u,1u
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
    EQ(spawned->state_time,1u); /* Time 0 ran; the clock advances at tick end */
    EQ(spawned->vars[3],0);
    EQ(spawned->vx_q8,0);

    ik_entity_runtime_step(&runtime);
    spawned=ik_entity_get_const(&pool,helper);
    OK(spawned!=nullptr);
    EQ(spawned->state_time,2u);
    EQ(spawned->vars[3],10);
    EQ(spawned->vx_q8,-256);
    EQ(spawned->x_q8,110*IK_ENTITY_Q8_ONE-256);

    ik_entity_runtime_step(&runtime);
    OK(ik_entity_get_const(&pool,helper)==nullptr);
    EQ(ik_entity_count_type(&pool,IK_ENTITY_HELPER),0u);

    ik_entity_handle_t paused_helper{};
    OK(ik_entity_runtime_spawn_helper(
        &runtime,p1,&spec,&paused_helper));
    const ik_entity_t* paused_h=
        ik_entity_get_const(&pool,paused_helper);
    OK(paused_h!=nullptr);
    EQ(paused_h->pause_move_time,2u);
    EQ(paused_h->super_move_time,1u);

    ik_entity_runtime_step_paused(&runtime,0u,0);
    paused_h=ik_entity_get_const(&pool,paused_helper);
    OK(paused_h!=nullptr);
    EQ(paused_h->pause_move_time,1u);

    ik_entity_runtime_step_paused(&runtime,0u,1);
    paused_h=ik_entity_get_const(&pool,paused_helper);
    OK(paused_h!=nullptr);
    EQ(paused_h->super_move_time,0u);

    (void)ik_entity_destroy(&pool,paused_helper);

    const ik_cns_explod_t explod_spec={
        200,
        20*IK_ENTITY_Q8_ONE,-10*IK_ENTITY_Q8_ONE,
        2*IK_ENTITY_Q8_ONE,-4*IK_ENTITY_Q8_ONE,
        0,82,
        3,1,IK_CNS_HELPER_POS_P1
    };
    ik_entity_handle_t explod{};
    OK(ik_entity_runtime_spawn_explod(
        &runtime,p1,&explod_spec,&explod));
    const ik_entity_t* visual=ik_entity_get_const(&pool,explod);
    OK(visual!=nullptr);
    EQ(visual->type,IK_ENTITY_EXPLOD);
    EQ(visual->x_q8,120*IK_ENTITY_Q8_ONE);
    EQ(visual->y_q8,40*IK_ENTITY_Q8_ONE);
    EQ(visual->vx_q8,2*IK_ENTITY_Q8_ONE);
    EQ(visual->vy_q8,-4*IK_ENTITY_Q8_ONE);
    EQ(visual->ay_q8,82);
    EQ(visual->remove_time,3);
    EQ(visual->spr_priority,1);

    ik_entity_runtime_step(&runtime);
    visual=ik_entity_get_const(&pool,explod);
    OK(visual!=nullptr);
    EQ(visual->x_q8,122*IK_ENTITY_Q8_ONE);
    EQ(visual->y_q8,36*IK_ENTITY_Q8_ONE);
    EQ(visual->vy_q8,-4*IK_ENTITY_Q8_ONE+82);
    EQ(visual->remove_time,2);

    ik_entity_runtime_step(&runtime);
    visual=ik_entity_get_const(&pool,explod);
    OK(visual!=nullptr);
    EQ(visual->remove_time,1);

    ik_entity_runtime_step(&runtime);
    OK(ik_entity_get_const(&pool,explod)==nullptr);
    EQ(ik_entity_count_type(&pool,IK_ENTITY_EXPLOD),0u);

    ik_cns_projectile_t projectile_spec{};
    projectile_spec.id=42;
    projectile_spec.anim_no=200;
    projectile_spec.hit_anim_no=200;
    projectile_spec.remove_anim_no=-1;
    projectile_spec.cancel_anim_no=-1;
    projectile_spec.hitdef_global=0;
    projectile_spec.pos_x_q8=30*IK_ENTITY_Q8_ONE;
    projectile_spec.pos_y_q8=-20*IK_ENTITY_Q8_ONE;
    projectile_spec.vel_x_q8=4*IK_ENTITY_Q8_ONE;
    projectile_spec.vel_y_q8=-1*IK_ENTITY_Q8_ONE;
    projectile_spec.velmul_x_q8=IK_ENTITY_Q8_ONE;
    projectile_spec.velmul_y_q8=251;
    projectile_spec.accel_x_q8=0;
    projectile_spec.accel_y_q8=26;
    projectile_spec.remove_time=5;
    projectile_spec.edge_bound=50;
    projectile_spec.stage_bound=60;
    projectile_spec.hits=3u;
    projectile_spec.miss_time=8u;
    projectile_spec.priority=2u;
    projectile_spec.remove_on_hit=0u;
    projectile_spec.spr_priority=4;
    projectile_spec.ownpal=1u;
    projectile_spec.pause_move_time=5u;
    projectile_spec.super_move_time=7u;

    ik_entity_handle_t projectile{};
    OK(ik_entity_runtime_spawn_projectile_spec(
        &runtime,p1,&projectile_spec,&projectile));
    const ik_entity_t* shot=ik_entity_get_const(&pool,projectile);
    OK(shot!=nullptr);
    EQ(shot->type,IK_ENTITY_PROJECTILE);
    EQ(shot->id,42);
    EQ(shot->state_no,-1);
    EQ(shot->active_hitdef_global,0);
    EQ(shot->projectile_hits_left,3u);
    EQ(shot->projectile_miss_time,8u);
    EQ(shot->projectile_priority,2u);
    EQ(shot->pause_move_time,5u);
    EQ(shot->super_move_time,7u);
    EQ(shot->x_q8,130*IK_ENTITY_Q8_ONE);
    EQ(shot->y_q8,30*IK_ENTITY_Q8_ONE);

    ik_entity_runtime_step(&runtime);
    shot=ik_entity_get_const(&pool,projectile);
    OK(shot!=nullptr);
    EQ(shot->x_q8,134*IK_ENTITY_Q8_ONE);
    EQ(shot->remove_time,4);

    ik_cns_projectile_t bound_projectile_spec=projectile_spec;
    bound_projectile_spec.id=44;
    bound_projectile_spec.remove_time=20;
    bound_projectile_spec.bind_time=2;
    bound_projectile_spec.remove_on_state_change=1u;
    bound_projectile_spec.remove_on_gethit=1u;
    bound_projectile_spec.pos_x_q8=12*IK_ENTITY_Q8_ONE;
    bound_projectile_spec.pos_y_q8=-6*IK_ENTITY_Q8_ONE;

    parent=ik_entity_get(&pool,p1);
    OK(parent!=nullptr);
    parent->state_no=200;
    parent->move_type=IK_CNS_MOVE_IDLE;
    parent->x_q8=100*IK_ENTITY_Q8_ONE;
    parent->y_q8=50*IK_ENTITY_Q8_ONE;

    ik_entity_handle_t bound_projectile{};
    OK(ik_entity_runtime_spawn_projectile_spec(
        &runtime,p1,&bound_projectile_spec,&bound_projectile));
    parent->x_q8=115*IK_ENTITY_Q8_ONE;
    ik_entity_runtime_step(&runtime);
    const ik_entity_t* bound_shot=
        ik_entity_get_const(&pool,bound_projectile);
    OK(bound_shot!=nullptr);
    EQ(bound_shot->x_q8,127*IK_ENTITY_Q8_ONE);
    EQ(bound_shot->y_q8,44*IK_ENTITY_Q8_ONE);
    EQ(bound_shot->projectile_bind_time,1);

    parent=ik_entity_get(&pool,p1);
    OK(parent!=nullptr);
    parent->move_type=IK_CNS_MOVE_HIT;
    ik_entity_runtime_step(&runtime);
    OK(ik_entity_get_const(&pool,bound_projectile)==nullptr);

    ik_cns_projectile_t remove_spec=projectile_spec;
    remove_spec.id=45;
    remove_spec.anim_no=200;
    remove_spec.remove_anim_no=200;
    remove_spec.remove_time=1;
    remove_spec.edge_bound=0;
    remove_spec.stage_bound=4;
    remove_spec.pos_x_q8=-10*IK_ENTITY_Q8_ONE;
    remove_spec.vel_x_q8=0;
    ik_entity_runtime_set_stage_bounds(&runtime,24,296);

    ik_entity_handle_t removing_projectile{};
    OK(ik_entity_runtime_spawn_projectile_spec(
        &runtime,p1,&remove_spec,&removing_projectile));
    ik_entity_runtime_step(&runtime);
    const ik_entity_t* removing=
        ik_entity_get_const(&pool,removing_projectile);
    OK(removing!=nullptr);
    EQ(removing->active_hitdef_global,-1);
    EQ(removing->anim_no,200);
    EQ(removing->remove_time,0);

    for(int i=0;i<7;++i){
        ik_entity_runtime_step(&runtime);
        removing=ik_entity_get_const(&pool,removing_projectile);
        OK(removing!=nullptr);
        EQ(removing->active_hitdef_global,-1);
    }
    ik_entity_runtime_step(&runtime);
    OK(ik_entity_get_const(&pool,removing_projectile)==nullptr);

    ik_cns_projectile_t pause_spec=projectile_spec;
    pause_spec.id=43;
    pause_spec.remove_time=10;
    pause_spec.pause_move_time=2u;
    pause_spec.super_move_time=1u;
    ik_entity_handle_t paused_projectile{};
    OK(ik_entity_runtime_spawn_projectile_spec(
        &runtime,p1,&pause_spec,&paused_projectile));
    const ik_entity_t* paused=
        ik_entity_get_const(&pool,paused_projectile);
    OK(paused!=nullptr);
    const int32_t paused_x0=paused->x_q8;

    ik_entity_runtime_step_paused(&runtime,0u,0);
    paused=ik_entity_get_const(&pool,paused_projectile);
    OK(paused!=nullptr);
    OK(paused->x_q8>paused_x0);
    EQ(paused->pause_move_time,1u);
    const int32_t paused_x1=paused->x_q8;

    ik_entity_runtime_step_paused(&runtime,0u,0);
    paused=ik_entity_get_const(&pool,paused_projectile);
    OK(paused!=nullptr);
    OK(paused->x_q8>paused_x1);
    EQ(paused->pause_move_time,0u);
    const int32_t paused_x2=paused->x_q8;

    ik_entity_runtime_step_paused(&runtime,0u,0);
    paused=ik_entity_get_const(&pool,paused_projectile);
    OK(paused!=nullptr);
    EQ(paused->x_q8,paused_x2);

    ik_entity_runtime_step_paused(&runtime,0u,1);
    paused=ik_entity_get_const(&pool,paused_projectile);
    OK(paused!=nullptr);
    OK(paused->x_q8>paused_x2);
    EQ(paused->super_move_time,0u);

    ik_cns_explod_t bound_spec{};
    bound_spec.anim_no=200;
    bound_spec.pos_x_q8=10*IK_ENTITY_Q8_ONE;
    bound_spec.pos_y_q8=-5*IK_ENTITY_Q8_ONE;
    bound_spec.remove_time=-1;
    bound_spec.bind_time=2;
    bound_spec.remove_on_state_change=1u;

    parent=ik_entity_get(&pool,p1);
    OK(parent!=nullptr);
    parent->state_no=100;
    parent->x_q8=100*IK_ENTITY_Q8_ONE;
    parent->y_q8=50*IK_ENTITY_Q8_ONE;
    parent->facing=1;

    ik_entity_handle_t bound_explod{};
    OK(ik_entity_runtime_spawn_explod(
        &runtime,p1,&bound_spec,&bound_explod));
    parent->x_q8=120*IK_ENTITY_Q8_ONE;
    ik_entity_runtime_step(&runtime);
    const ik_entity_t* bound=
        ik_entity_get_const(&pool,bound_explod);
    OK(bound!=nullptr);
    EQ(bound->x_q8,130*IK_ENTITY_Q8_ONE);
    EQ(bound->y_q8,45*IK_ENTITY_Q8_ONE);
    EQ(bound->explod_bind_time,1);

    parent=ik_entity_get(&pool,p1);
    OK(parent!=nullptr);
    parent->state_no=101;
    ik_entity_runtime_step(&runtime);
    OK(ik_entity_get_const(&pool,bound_explod)==nullptr);

    ik_cns_projectile_t mod_target_spec=projectile_spec;
    mod_target_spec.id=77;
    mod_target_spec.remove_time=30;
    mod_target_spec.hits=3u;
    mod_target_spec.priority=2u;
    mod_target_spec.anim_no=200;
    mod_target_spec.vel_x_q8=1*IK_ENTITY_Q8_ONE;
    mod_target_spec.vel_y_q8=0;

    ik_cns_projectile_t mod_second_spec=mod_target_spec;
    ik_cns_projectile_t mod_other_spec=mod_target_spec;
    mod_other_spec.id=78;
    ik_entity_handle_t mod_target{};
    ik_entity_handle_t mod_second{};
    ik_entity_handle_t mod_other{};
    OK(ik_entity_runtime_spawn_projectile_spec(
        &runtime,p1,&mod_target_spec,&mod_target));
    OK(ik_entity_runtime_spawn_projectile_spec(
        &runtime,p1,&mod_second_spec,&mod_second));
    OK(ik_entity_runtime_spawn_projectile_spec(
        &runtime,p1,&mod_other_spec,&mod_other));

    ik_cns_projectile_mod_t modification{};
    modification.id=77;
    modification.index=1;
    modification.mask=
        IK_CNS_PROJ_MOD_VELOCITY |
        IK_CNS_PROJ_MOD_PRIORITY;
    modification.vel_x_q8=5*IK_ENTITY_Q8_ONE;
    modification.vel_y_q8=-1*IK_ENTITY_Q8_ONE;
    modification.priority=6u;

    EQ(ik_entity_runtime_modify_projectiles(
        &runtime,0u,&modification),1u);
    const ik_entity_t* first_same_id=
        ik_entity_get_const(&pool,mod_target);
    const ik_entity_t* modified=
        ik_entity_get_const(&pool,mod_second);
    const ik_entity_t* untouched=
        ik_entity_get_const(&pool,mod_other);
    OK(first_same_id!=nullptr);
    OK(modified!=nullptr);
    OK(untouched!=nullptr);
    EQ(first_same_id->vx_q8,1*IK_ENTITY_Q8_ONE);
    EQ(first_same_id->projectile_priority,2u);
    EQ(modified->vx_q8,5*IK_ENTITY_Q8_ONE);
    EQ(modified->vy_q8,-1*IK_ENTITY_Q8_ONE);
    EQ(modified->projectile_priority,6u);
    EQ(modified->projectile_hits_left,3u);
    EQ(modified->anim_no,200);
    EQ(untouched->vx_q8,1*IK_ENTITY_Q8_ONE);
    EQ(untouched->projectile_priority,2u);

    ik_entity_t* opponent=ik_entity_get(&pool,p2);
    OK(opponent!=nullptr);
    opponent->x_q8=220*IK_ENTITY_Q8_ONE;
    opponent->y_q8=50*IK_ENTITY_Q8_ONE;
    opponent->facing=-1;

    ik_cns_explod_t p2_explod_spec{};
    p2_explod_spec.anim_no=200;
    p2_explod_spec.postype=IK_CNS_HELPER_POS_P2;
    p2_explod_spec.pos_x_q8=10*IK_ENTITY_Q8_ONE;
    p2_explod_spec.pos_y_q8=-5*IK_ENTITY_Q8_ONE;
    p2_explod_spec.remove_time=5;
    p2_explod_spec.scale_x_q8=IK_ENTITY_Q8_ONE;
    p2_explod_spec.scale_y_q8=IK_ENTITY_Q8_ONE;
    p2_explod_spec.alpha=255u;

    ik_entity_handle_t p2_explod{};
    OK(ik_entity_runtime_spawn_explod(
        &runtime,p1,&p2_explod_spec,&p2_explod));
    const ik_entity_t* p2_visual=
        ik_entity_get_const(&pool,p2_explod);
    OK(p2_visual!=nullptr);
    EQ(p2_visual->x_q8,210*IK_ENTITY_Q8_ONE);
    EQ(p2_visual->y_q8,45*IK_ENTITY_Q8_ONE);

    ik_cns_projectile_t p2_projectile_spec=projectile_spec;
    p2_projectile_spec.id=46;
    p2_projectile_spec.postype=IK_CNS_HELPER_POS_P2;
    p2_projectile_spec.pos_x_q8=12*IK_ENTITY_Q8_ONE;
    p2_projectile_spec.pos_y_q8=-6*IK_ENTITY_Q8_ONE;
    p2_projectile_spec.vel_x_q8=0;
    p2_projectile_spec.vel_y_q8=0;
    p2_projectile_spec.remove_time=5;
    p2_projectile_spec.bind_time=2;

    ik_entity_handle_t p2_projectile{};
    OK(ik_entity_runtime_spawn_projectile_spec(
        &runtime,p1,&p2_projectile_spec,&p2_projectile));
    const ik_entity_t* p2_shot=
        ik_entity_get_const(&pool,p2_projectile);
    OK(p2_shot!=nullptr);
    EQ(p2_shot->x_q8,208*IK_ENTITY_Q8_ONE);
    EQ(p2_shot->y_q8,44*IK_ENTITY_Q8_ONE);

    opponent=ik_entity_get(&pool,p2);
    OK(opponent!=nullptr);
    opponent->x_q8=200*IK_ENTITY_Q8_ONE;
    ik_entity_runtime_step(&runtime);
    p2_shot=ik_entity_get_const(&pool,p2_projectile);
    OK(p2_shot!=nullptr);
    EQ(p2_shot->x_q8,188*IK_ENTITY_Q8_ONE);

    {
        ik_entity_pool_t nested_pool{};
        ik_entity_pool_init(&nested_pool);
        ik_entity_handle_t root0{}, root1{};
        OK(ik_entity_spawn(
            &nested_pool,IK_ENTITY_PLAYER,1,0u,
            ik_entity_invalid_handle(),&root0));
        OK(ik_entity_spawn(
            &nested_pool,IK_ENTITY_PLAYER,2,1u,
            ik_entity_invalid_handle(),&root1));
        ik_entity_t* nested_parent=ik_entity_get(&nested_pool,root0);
        OK(nested_parent!=nullptr);
        nested_parent->x_q8=100*IK_ENTITY_Q8_ONE;
        nested_parent->y_q8=50*IK_ENTITY_Q8_ONE;
        nested_parent->facing=1;

        const ik_cns_controller_t nested_ctrls[]={
            {300,IK_CNS_CTRL_EXPLOD,IK_CNS_TRIGGER_ALWAYS,
             0,0,0,1,0u},
            {300,IK_CNS_CTRL_PROJECTILE,IK_CNS_TRIGGER_ALWAYS,
             0,0,0,1,0u},
        };
        ik_cns_state_t nested_state{};
        nested_state.number=300;
        nested_state.anim=200;
        nested_state.state_type=IK_CNS_STATE_STAND;
        nested_state.move_type=IK_CNS_MOVE_IDLE;
        nested_state.physics=IK_CNS_PHYS_NONE;
        nested_state.controller_count=2u;

        ik_cns_explod_t nested_explod{};
        nested_explod.anim_no=200;
        nested_explod.remove_time=20;
        nested_explod.scale_x_q8=IK_ENTITY_Q8_ONE;
        nested_explod.scale_y_q8=IK_ENTITY_Q8_ONE;
        nested_explod.alpha=255u;

        ik_cns_projectile_t nested_projectile{};
        nested_projectile.id=99;
        nested_projectile.anim_no=200;
        nested_projectile.hitdef_global=-1;
        nested_projectile.remove_time=20;
        nested_projectile.hits=1u;
        nested_projectile.priority=1u;
        nested_projectile.velmul_x_q8=IK_ENTITY_Q8_ONE;
        nested_projectile.velmul_y_q8=IK_ENTITY_Q8_ONE;

        ik_cns_asset_t nested_cns{};
        nested_cns.constants.life=1000;
        nested_cns.constants.ground_back=15;
        nested_cns.constants.ground_front=16;
        nested_cns.constants.air_back=12;
        nested_cns.constants.air_front=12;
        nested_cns.states=&nested_state;
        nested_cns.state_count=1u;
        nested_cns.controllers=nested_ctrls;
        nested_cns.controller_count=2u;
        nested_cns.explods=&nested_explod;
        nested_cns.explod_count=1u;
        nested_cns.projectiles=&nested_projectile;
        nested_cns.projectile_count=1u;

        ik_entity_runtime_t nested_runtime{};
        ik_entity_runtime_init(
            &nested_runtime,&nested_pool,&nested_cns,&k_table,&k_table);

        ik_cns_helper_t nested_helper{};
        nested_helper.id=55;
        nested_helper.state_no=300;
        ik_entity_handle_t helper_parent{};
        OK(ik_entity_runtime_spawn_helper(
            &nested_runtime,root0,&nested_helper,&helper_parent));

        ik_entity_runtime_step(&nested_runtime);
        EQ(ik_entity_count_type(&nested_pool,IK_ENTITY_EXPLOD),1u);
        EQ(ik_entity_count_type(&nested_pool,IK_ENTITY_PROJECTILE),1u);

        ik_entity_runtime_step(&nested_runtime);
        EQ(ik_entity_count_type(&nested_pool,IK_ENTITY_EXPLOD),1u);
        EQ(ik_entity_count_type(&nested_pool,IK_ENTITY_PROJECTILE),1u);
    }

    std::puts("[test] ikemen_entity_runtime OK");
    return 0;
}
