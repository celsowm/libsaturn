#include <cstdio>
#include <cstdlib>

#include "examples/ikemen_saturn/ikemen_fight.h"

#define EQ(a,b) do { if ((a)!=(b)) { std::fprintf(stderr,"FAIL %d: %s=%ld %s=%ld\n",__LINE__,#a,(long)(a),#b,(long)(b)); std::exit(1); } } while(0)
#define OK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while(0)

static const ik_clsn_box_t k_boxes[] = {
    {-15,-95,16,0},       /* 0 generic hurt */
    {16,-80,90,-65},      /* 1 generic attack */
    {10,-65,110,-20},     /* 2 crouch attack */
};

#define F(action,index,ticks,c1ofs,c1cnt)     {action,index,64u,96u,16,93,ticks,0u,0u,c1ofs,c1cnt,0u,1u}

static const ik_frame_t k_frames[] = {
    F(0,0,0,0,0),
    F(11,0,0,0,0),
    F(41,0,0,0,0),
    F(105,0,0,0,0),

    F(200,0,2,0,0), F(200,1,1,0,0), F(200,2,4,1,1),
    F(200,3,3,0,0), F(200,4,2,0,0),

    F(210,0,2,0,0), F(210,1,1,0,0), F(210,2,5,1,1),
    F(210,3,5,0,0),

    F(230,0,2,0,0), F(230,1,2,0,0), F(230,2,3,1,1),
    F(230,3,3,0,0), F(230,4,3,0,0), F(230,5,2,0,0),

    F(240,0,2,0,0), F(240,1,2,0,0), F(240,2,3,1,1),
    F(240,3,2,0,0), F(240,4,2,0,0), F(240,5,2,0,0),
    F(240,6,2,0,0),

    F(400,0,3,0,0), F(400,1,3,2,1), F(400,2,3,0,0),

    F(410,0,2,0,0), F(410,1,1,0,0), F(410,2,2,2,1),
    F(410,3,2,2,1), F(410,4,2,0,0),

    F(430,0,2,0,0), F(430,1,3,2,1), F(430,2,3,0,0),

    F(440,0,2,0,0), F(440,1,2,0,0), F(440,2,4,2,1),
    F(440,3,3,0,0),

    F(600,0,4,1,1), F(600,1,4,0,0),
    F(610,0,4,1,1), F(610,1,4,0,0),
    F(630,0,4,1,1), F(630,1,4,0,0),
    F(640,0,4,1,1), F(640,1,4,0,0),
};
#undef F

static const ik_frame_table_t k_table = {
    k_frames, sizeof(k_frames)/sizeof(k_frames[0]),
    k_boxes, sizeof(k_boxes)/sizeof(k_boxes[0])
};

static const ik_cns_hitdef_t k_hitdefs[] = {
    {200,IK_CNS_TRIGGER_ANIM_ELEM_EQ,3,23,0,3,8,8,IK_CNS_GROUND_HIGH,5,11,15,-1024,0,-358,-768,0,-10,-76,5,0,6,0,0},
    {210,IK_CNS_TRIGGER_ANIM_ELEM_EQ,3,57,0,4,12,12,IK_CNS_GROUND_HIGH,12,16,16,-1408,0,-640,-1024,1,-10,-70,5,2,6,0,IK_CNS_HITDEF_FORCE_NO_FALL},
    {230,IK_CNS_TRIGGER_TIME_EQ,0,26,0,4,12,12,IK_CNS_GROUND_LOW,10,14,14,-1280,0,-640,-896,0,-10,-37,5,1,6,0,0},
    {240,IK_CNS_TRIGGER_TIME_EQ,0,63,0,4,12,12,IK_CNS_GROUND_LOW,12,17,17,-1536,0,-563,-819,1,-10,-60,5,2,6,0,0},
    {400,IK_CNS_TRIGGER_TIME_EQ,0,23,0,3,10,11,IK_CNS_GROUND_LOW,4,9,9,-1024,0,-384,-768,0,-10,-42,5,0,6,0,0},
    {410,IK_CNS_TRIGGER_ANIM_ELEM_EQ,3,37,0,4,12,12,IK_CNS_GROUND_LOW,12,17,17,-1024,0,-768,-1024,1,-10,-55,5,2,6,0,0},
    {410,IK_CNS_TRIGGER_ANIM_ELEM_EQ,4,36,0,4,12,12,IK_CNS_GROUND_HIGH,12,17,17,-1792,0,-768,-1024,-1,-10,-83,5,2,6,0,0},
    {430,IK_CNS_TRIGGER_TIME_EQ,0,28,0,4,12,12,IK_CNS_GROUND_LOW,6,10,10,-1280,0,-512,-768,0,-10,-8,5,1,6,0,0},
    {440,IK_CNS_TRIGGER_TIME_EQ,0,72,0,4,12,12,IK_CNS_GROUND_TRIP,10,17,17,-384,-512,-307,-768,1,-5,-10,5,2,6,0,IK_CNS_HITDEF_FALL},
    {600,IK_CNS_TRIGGER_TIME_EQ,0,20,0,3,7,8,IK_CNS_GROUND_HIGH,5,8,14,-1024,0,-333,-768,0,-10,-58,5,0,6,0,0},
    {610,IK_CNS_TRIGGER_TIME_EQ,0,72,0,4,12,12,IK_CNS_GROUND_HIGH,12,14,14,-1536,0,-768,-1024,1,-10,-55,5,3,6,0,0},
    {630,IK_CNS_TRIGGER_TIME_EQ,0,26,0,3,8,8,IK_CNS_GROUND_HIGH,6,10,14,-1024,0,-512,-768,1,-5,-35,5,0,6,0,0},
    {640,IK_CNS_TRIGGER_TIME_EQ,0,70,0,4,12,12,IK_CNS_GROUND_HIGH,12,15,15,-1792,0,-768,-1024,1,-10,-40,5,3,6,0,0},
};

static const ik_cns_controller_t k_ctrls[] = {
    {200,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_ANIM_END,
     0,0,0,1,IK_CNS_CTRL_HAS_CTRL},

    {210,IK_CNS_CTRL_WIDTH,IK_CNS_TRIGGER_ANIM_ELEM_RANGE,
     2,7,15,0,0u},
    {210,IK_CNS_CTRL_CHANGE_ANIM,
     IK_CNS_TRIGGER_MOVE_CONTACT_ELEM_WINDOW,
     5,6,210,6,IK_CNS_CTRL_IGNORE_HIT_PAUSE},
    {210,IK_CNS_CTRL_SPR_PRIORITY,IK_CNS_TRIGGER_ANIM_ELEM_EQ,
     5,0,2,0,0u},
    {210,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_ANIM_END,
     0,0,0,1,IK_CNS_CTRL_HAS_CTRL},

    {230,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_ANIM_END,
     0,0,0,1,IK_CNS_CTRL_HAS_CTRL},

    {240,IK_CNS_CTRL_POS_ADD,IK_CNS_TRIGGER_ANIM_ELEM_EQ,
     7,0,12*256,0,0u},
    {240,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_ANIM_END,
     0,0,0,1,IK_CNS_CTRL_HAS_CTRL},

    {400,IK_CNS_CTRL_CTRL_SET,IK_CNS_TRIGGER_TIME_EQ,
     6,0,1,0,0u},
    {400,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_ANIM_END,
     0,0,11,0,0u},

    {410,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_ANIM_END,
     0,0,11,1,IK_CNS_CTRL_HAS_CTRL},
    {430,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_ANIM_END,
     0,0,11,1,IK_CNS_CTRL_HAS_CTRL},
    {440,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_ANIM_END,
     0,0,11,1,IK_CNS_CTRL_HAS_CTRL},

    {600,IK_CNS_CTRL_CTRL_SET,IK_CNS_TRIGGER_TIME_EQ,
     17,0,1,0,0u},
};

#define S(no,anim,hoff,hcnt,coff,ccnt)     {no,anim,0,0,0,IK_CNS_STATE_STAND,IK_CNS_MOVE_ATTACK,IK_CNS_PHYS_STAND,0,2,0u,hoff,hcnt,0u,0u,coff,ccnt,0}
#define C(no,anim,hoff,hcnt,coff,ccnt)     {no,anim,0,0,0,IK_CNS_STATE_CROUCH,IK_CNS_MOVE_ATTACK,IK_CNS_PHYS_CROUCH,0,2,0u,hoff,hcnt,0u,0u,coff,ccnt,0}
#define A(no,anim,hoff,hcnt,coff,ccnt)     {no,anim,0,0,0,IK_CNS_STATE_AIR,IK_CNS_MOVE_ATTACK,IK_CNS_PHYS_AIR,0,2,0u,hoff,hcnt,0u,0u,coff,ccnt,0}

static const ik_cns_state_t k_states[] = {
    S(200,200,0,1,0,1),
    S(210,210,1,1,1,4),
    S(230,230,2,1,5,1),
    S(240,240,3,1,6,2),
    C(400,400,4,1,8,2),
    C(410,410,5,2,10,1),
    C(430,430,7,1,11,1),
    C(440,440,8,1,12,1),
    A(600,600,9,1,13,1),
    A(610,610,10,1,14,0),
    A(630,630,11,1,14,0),
    A(640,640,12,1,14,0),
};
#undef S
#undef C
#undef A

static const ik_cns_asset_t k_cns = {
    {1000,15,16,12,12,60,614,-563,1178,0,-1152,-973,0,-2150,-653,640,1024,-2074,113,218,210,512,13},
    k_states,12u,
    k_hitdefs,13u,
    nullptr,0u,
    k_ctrls,14u
};

static void tick(ik_fight_t* g, const ik_fight_controls_t* p) {
    ik_fight_update(g,p,nullptr,&k_table);
}

static void request(ik_fight_controls_t* p, int16_t state) {
    p->requested_state=state;
    p->has_state_request=1u;
}

static void idle(ik_fight_t* g,int n) {
    ik_fight_controls_t p{};
    for(int i=0;i<n;++i) tick(g,&p);
}

static void place(ik_fight_t* g,int x0,int x1) {
    g->fighters[0].x=(int16_t)x0;
    g->fighters[0].x_q8=x0*256;
    g->fighters[1].x=(int16_t)x1;
    g->fighters[1].x_q8=x1*256;
}

int main() {
    ik_fight_t g;
    ik_fight_init(&g,&k_cns);
    EQ(g.fighters[0].hp,1000);
    EQ(g.fighters[0].push_front,16);
    EQ(g.fighters[0].push_back,15);

    {
        const int x=g.fighters[0].x;
        ik_fight_controls_t p{}; p.forward=1;
        tick(&g,&p);
        OK(g.fighters[0].x>x);
        EQ(g.fighters[0].x_q8,110*256+614);
    }

    {
        ik_fight_init(&g,&k_cns); place(&g,100,150);
        const int hp=g.fighters[1].hp;
        ik_fight_controls_t p{}; request(&p,IK_STATE_PUNCH);
        tick(&g,&p); idle(&g,3);
        EQ(hp-g.fighters[1].hp,23);
        EQ(g.hits_p1,1u);
    }

    /* Down+X enters real crouching state 400. Its Time=6 CtrlSet is executed,
     * and AnimTime=0 returns to common crouch state 11. */
    {
        ik_fight_init(&g,&k_cns); place(&g,100,170);
        ik_fight_controls_t p{}; p.down=1; request(&p,IK_STATE_CROUCH_PUNCH);
        tick(&g,&p);
        EQ(g.fighters[0].state,IK_STATE_CROUCH_PUNCH);
        idle(&g,6);
        EQ(g.fighters[0].ctrl,1);
        idle(&g,3);
        EQ(g.fighters[0].state,IK_STATE_CROUCH);
        EQ(g.fighters[0].anim,11);
    }

    /* State 410 owns two HitDefs. Each one may connect once; the old single
     * attack_has_hit latch incorrectly suppressed the second hit. */
    {
        ik_fight_init(&g,&k_cns); place(&g,100,145);
        const int hp=g.fighters[1].hp;
        ik_fight_controls_t p{}; p.down=1; request(&p,IK_STATE_CROUCH_STRONG_PUNCH);
        tick(&g,&p);
        for(int i=0;i<40 && g.hits_p1<2u;++i) idle(&g,1);
        EQ(g.hits_p1,2u);
        EQ(hp-g.fighters[1].hp,73);
    }

    /* State 240 PosAdd at AnimElem 7 moves forward relative to facing. */
    {
        ik_fight_init(&g,&k_cns); place(&g,80,220);
        ik_fight_controls_t p{}; request(&p,IK_STATE_STRONG_KICK);
        tick(&g,&p);
        const int32_t before=g.fighters[0].x_q8;
        for(int i=0;i<14;++i) idle(&g,1);
        OK(g.fighters[0].x_q8>=before+12*256);
    }

    /* Sweep uses fall/vertical ground velocity instead of flattening the
     * HitDef into horizontal-only knockback. */
    {
        ik_fight_init(&g,&k_cns); place(&g,100,145);
        ik_fight_controls_t p{}; p.down=1; request(&p,IK_STATE_CROUCH_STRONG_KICK);
        tick(&g,&p);
        for(int i=0;i<5 && g.hits_p1==0u;++i) idle(&g,1);
        EQ(g.hits_p1,1u);
        EQ(g.fighters[1].on_ground,0);
        OK(g.fighters[1].vy_q8<0);
    }

    /* Jump normals are compiled CNS states with Physics=A. They remain
     * airborne while attacking and use their original Time=0 HitDefs. */
    {
        ik_fight_init(&g,&k_cns); place(&g,100,145);
        ik_fight_controls_t p{}; p.up=1;
        tick(&g,&p);
        EQ(g.fighters[0].state,IK_STATE_JUMP);
        EQ(g.fighters[0].on_ground,0);

        p={}; request(&p,IK_STATE_JUMP_PUNCH);
        const int hp=g.fighters[1].hp;
        tick(&g,&p);
        EQ(g.fighters[0].state,IK_STATE_JUMP_PUNCH);
        EQ(hp-g.fighters[1].hp,20);
        EQ(g.fighters[0].move_contact,1);

        idle(&g,8);
        p={}; request(&p,IK_STATE_JUMP_STRONG_PUNCH);
        tick(&g,&p);
        EQ(g.fighters[0].state,IK_STATE_JUMP_STRONG_PUNCH);
        EQ(g.fighters[0].on_ground,0);
    }

    /* Airborne victims consume air.hittime / air.velocity rather than the
     * ground HitDef branch. */
    {
        ik_fight_init(&g,&k_cns); place(&g,100,145);
        g.fighters[1].on_ground=0;
        g.fighters[1].y=150;
        g.fighters[1].y_q8=150*256;

        ik_fight_controls_t p{}; p.up=1;
        tick(&g,&p);
        p={}; request(&p,IK_STATE_JUMP_PUNCH);
        tick(&g,&p);

        EQ(g.hits_p1,1u);
        EQ(g.fighters[1].hitstun,14u);
        EQ(g.fighters[1].vx_q8,333);
        EQ(g.fighters[1].vy_q8,-768);
        EQ(g.fighters[1].on_ground,0);
    }

    /* State 600's original CtrlSet at Time=17 is executed by the generic
     * controller runtime, allowing new controlled air input afterwards. */
    {
        ik_fight_init(&g,&k_cns); place(&g,60,260);
        ik_fight_controls_t p{}; p.up=1;
        tick(&g,&p);
        p={}; request(&p,IK_STATE_JUMP_PUNCH);
        tick(&g,&p);
        EQ(g.fighters[0].ctrl,0);
        idle(&g,17);
        EQ(g.fighters[0].ctrl,1);
        EQ(g.fighters[0].state,IK_STATE_JUMP_PUNCH);
    }

    std::puts("[test] ikemen_fight OK");
    return 0;
}
