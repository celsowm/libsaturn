#include <cstdio>
#include <cstdlib>

#include "examples/ikemen_saturn/ikemen_fight.h"
#include "examples/ikemen_saturn/ikemen_entity_runtime.h"

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
    F(10,0,2,0,0),
    F(11,0,0,0,0),
    F(12,0,2,0,0),
    F(20,0,4,0,0),
    F(21,0,4,0,0),
    F(40,0,2,0,0),
    F(41,0,0,0,0),
    F(42,0,0,0,0),
    F(43,0,0,0,0),
    F(47,0,3,0,0),
    F(100,0,4,0,0),
    F(105,0,4,0,0),
    F(120,0,2,0,0),
    F(121,0,2,0,0),
    F(122,0,2,0,0),
    F(130,0,0,0,0),
    F(131,0,0,0,0),
    F(132,0,0,0,0),
    F(140,0,2,0,0),
    F(141,0,2,0,0),
    F(142,0,2,0,0),
    F(150,0,2,0,0),
    F(151,0,2,0,0),
    F(152,0,2,0,0),
    F(5000,0,2,0,0),
    F(5001,0,2,0,0),
    F(5010,0,2,0,0),
    F(5011,0,2,0,0),
    F(5020,0,2,0,0),
    F(5030,0,2,0,0),
    F(5035,0,2,0,0),
    F(5040,0,2,0,0),
    F(5050,0,2,0,0),
    F(5070,0,2,0,0),
    F(5080,0,2,0,0),
    F(5090,0,2,0,0),
    F(5100,0,2,0,0),
    F(5160,0,4,0,0),
    F(5110,0,0,0,0),
    F(5120,0,3,0,0),
    F(5150,0,0,0,0),
    F(5200,0,3,0,0),
    F(5210,0,0,0,0),

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

    F(800,0,2,1,1),
    F(810,0,1,0,0), F(810,1,1,0,0), F(810,2,1,0,0),
    F(810,3,1,0,0), F(810,4,1,0,0), F(810,5,1,0,0),
    F(810,6,1,0,0), F(810,7,1,0,0), F(810,8,1,0,0),
    F(810,9,1,0,0), F(810,10,1,0,0), F(810,11,1,0,0),
    F(810,12,1,0,0), F(810,13,1,0,0), F(810,14,1,0,0),
    F(820,0,0,0,0),
    F(821,0,0,0,0),

    /* Palm HitDef activates on element 3; attack Clsn1 starts on element 4. */
    F(903,0,1,0,0), F(903,1,1,0,0), F(903,2,1,0,0),
    F(903,3,4,1,1),

    /* Fast Palm and its custom victim animations. */
    F(1020,0,1,0,0), F(1020,1,1,0,0), F(1020,2,1,0,0),
    F(1020,3,4,1,1),
    F(1025,0,4,0,0),
    F(1027,0,3,0,0),

    F(910,0,2,1,1),
    F(911,0,4,1,1),
    F(915,0,4,1,1),

    /* Upper runtime fixtures. */
    F(920,0,1,1,1), F(920,1,1,1,1), F(920,2,1,1,1),
    F(920,3,1,1,1), F(920,4,2,1,1),
    F(921,0,3,1,1),
    F(922,0,3,1,1),
    F(923,0,3,1,1),
};
#undef F

static const ik_frame_table_t k_table = {
    k_frames, sizeof(k_frames)/sizeof(k_frames[0]),
    k_boxes, sizeof(k_boxes)/sizeof(k_boxes[0])
};

static const ik_cns_hitdef_t k_hitdefs[] = {
    {200,IK_CNS_TRIGGER_ANIM_ELEM_EQ,3,23,5,3,8,8,IK_CNS_GROUND_HIGH,5,11,15,-1024,0,-358,-768,0,-10,-76,5,0,6,0,0,
     IK_CNS_GUARD_STAND|IK_CNS_GUARD_CROUCH|IK_CNS_GUARD_AIR,1,5,11,11,-1024,-537,-384,1,1},
    {210,IK_CNS_TRIGGER_ANIM_ELEM_EQ,3,57,0,4,12,12,IK_CNS_GROUND_HIGH,12,16,16,-1408,0,-640,-1024,1,-10,-70,5,2,6,0,IK_CNS_HITDEF_FORCE_NO_FALL},
    {230,IK_CNS_TRIGGER_TIME_EQ,0,26,0,4,12,12,IK_CNS_GROUND_LOW,10,14,14,-1280,0,-640,-896,0,-10,-37,5,1,6,0,0},
    {240,IK_CNS_TRIGGER_TIME_EQ,0,63,0,4,12,12,IK_CNS_GROUND_LOW,12,17,17,-1536,0,-563,-819,1,-10,-60,5,2,6,0,0},
    {400,IK_CNS_TRIGGER_TIME_EQ,0,23,5,3,10,11,IK_CNS_GROUND_LOW,4,9,9,-1024,0,-384,-768,0,-10,-42,5,0,6,0,0,
     IK_CNS_GUARD_CROUCH,0,4,9,9,-1024,-576,-384},
    {410,IK_CNS_TRIGGER_ANIM_ELEM_EQ,3,37,0,4,12,12,IK_CNS_GROUND_LOW,12,17,17,-1024,0,-768,-1024,1,-10,-55,5,2,6,0,0},
    {410,IK_CNS_TRIGGER_ANIM_ELEM_EQ,4,36,0,4,12,12,IK_CNS_GROUND_HIGH,12,17,17,-1792,0,-768,-1024,-1,-10,-83,5,2,6,0,0},
    {430,IK_CNS_TRIGGER_TIME_EQ,0,28,0,4,12,12,IK_CNS_GROUND_LOW,6,10,10,-1280,0,-512,-768,0,-10,-8,5,1,6,0,0},
    {440,IK_CNS_TRIGGER_TIME_EQ,0,72,0,4,12,12,IK_CNS_GROUND_TRIP,10,17,17,-384,-512,-307,-768,1,-5,-10,5,2,6,0,IK_CNS_HITDEF_FALL,
     IK_CNS_GUARD_CROUCH,1,10,17,17,-1280,-461,-384,2,2,0,-1152,0,1,4},
    {600,IK_CNS_TRIGGER_TIME_EQ,0,20,0,3,7,8,IK_CNS_GROUND_HIGH,5,8,14,-1024,0,-333,-768,0,-10,-58,5,0,6,0,0},
    {610,IK_CNS_TRIGGER_TIME_EQ,0,72,0,4,12,12,IK_CNS_GROUND_HIGH,12,14,14,-1536,0,-768,-1024,1,-10,-55,5,3,6,0,0},
    {630,IK_CNS_TRIGGER_TIME_EQ,0,26,0,3,8,8,IK_CNS_GROUND_HIGH,6,10,14,-1024,0,-512,-768,1,-5,-35,5,0,6,0,0},
    {640,IK_CNS_TRIGGER_TIME_EQ,0,70,0,4,12,12,IK_CNS_GROUND_HIGH,12,15,15,-1792,0,-768,-1024,1,-10,-40,5,3,6,0,0},

    /* Host-only downed launch fixtures: same launch, bounce disabled/enabled. */
    {201,IK_CNS_TRIGGER_TIME_EQ,0,0,0,4,0,0,IK_CNS_GROUND_HIGH,0,10,10,
     0,0,-512,-768,-1,0,0,-1,-1,-1,-1,0u,
     0u,1u,0u,0u,0u,0,0,0,0u,0u,
     -256,-1152,1u,1u,4u,0u,-512,-768,0u,IK_CNS_HIT_DEFAULT|IK_CNS_HIT_DOWN},
    {202,IK_CNS_TRIGGER_TIME_EQ,0,0,0,4,0,0,IK_CNS_GROUND_HIGH,0,10,10,
     0,0,-512,-768,-1,0,0,-1,-1,-1,-1,0u,
     0u,1u,0u,0u,0u,0,0,0,0u,0u,
     -256,-1152,1u,1u,4u,0u,-512,-768,1u,IK_CNS_HIT_DEFAULT|IK_CNS_HIT_DOWN},
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

    {120,IK_CNS_CTRL_GUARD_ANIM_BY_TYPE,IK_CNS_TRIGGER_TIME_EQ,
     1,0,120,0,0u},
    {120,IK_CNS_CTRL_GUARD_STATE_BY_TYPE,IK_CNS_TRIGGER_ANIM_END,
     0,0,130,0,0u},
    {130,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_COMMAND_ACTIVE,
     IK_CNS_COMMAND_HOLD_DOWN,0,131,0,0u},
    {131,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_COMMAND_INACTIVE,
     IK_CNS_COMMAND_HOLD_DOWN,0,130,0,0u},
    {140,IK_CNS_CTRL_GUARD_ANIM_BY_TYPE,IK_CNS_TRIGGER_TIME_EQ,
     1,0,140,0,0u},
    {140,IK_CNS_CTRL_GUARD_END,IK_CNS_TRIGGER_ANIM_END,
     0,0,0,0,0u},
    {150,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_TIME_EQ,
     1,0,151,0,0u},
    {151,IK_CNS_CTRL_HIT_VEL_SET,IK_CNS_TRIGGER_TIME_EQ,
     1,0,0,0,IK_CNS_CTRL_AXIS_X},
    {151,IK_CNS_CTRL_VEL_SET,IK_CNS_TRIGGER_HIT_SLIDE_TIME,
     0,0,0,0,IK_CNS_CTRL_AXIS_X},
    {151,IK_CNS_CTRL_CTRL_SET,IK_CNS_TRIGGER_HIT_CTRL_TIME,
     0,0,1,0,0u},
    {151,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_HIT_OVER,
     0,0,130,1,IK_CNS_CTRL_HAS_CTRL},
    {152,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_TIME_EQ,
     1,0,153,0,0u},
    {153,IK_CNS_CTRL_HIT_VEL_SET,IK_CNS_TRIGGER_TIME_EQ,
     1,0,0,0,IK_CNS_CTRL_AXIS_X},
    {153,IK_CNS_CTRL_VEL_SET,IK_CNS_TRIGGER_HIT_SLIDE_TIME,
     0,0,0,0,IK_CNS_CTRL_AXIS_X},
    {153,IK_CNS_CTRL_CTRL_SET,IK_CNS_TRIGGER_HIT_CTRL_TIME,
     0,0,1,0,0u},
    {153,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_HIT_OVER,
     0,0,131,1,IK_CNS_CTRL_HAS_CTRL},
    {154,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_TIME_EQ,
     1,0,155,0,0u},
    {155,IK_CNS_CTRL_HIT_VEL_SET,IK_CNS_TRIGGER_TIME_EQ,
     1,0,0,0,IK_CNS_CTRL_AXIS_X|IK_CNS_CTRL_AXIS_Y},
    {155,IK_CNS_CTRL_CTRL_SET,IK_CNS_TRIGGER_HIT_CTRL_TIME,
     0,0,1,0,0u},

    {5000,IK_CNS_CTRL_GET_HIT_ANIM,IK_CNS_TRIGGER_TIME_EQ,
     1,0,0,0,0u},
    {5000,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_HIT_LAUNCH,
     0,0,5030,0,0u},
    {5000,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_HIT_NO_LAUNCH,
     0,0,5001,0,0u},
    {5001,IK_CNS_CTRL_HIT_VEL_SET,IK_CNS_TRIGGER_TIME_EQ,
     1,0,0,0,IK_CNS_CTRL_AXIS_X},
    {5001,IK_CNS_CTRL_VEL_MUL,IK_CNS_TRIGGER_HIT_SLIDE_TIME,
     0,0,154,0,IK_CNS_CTRL_AXIS_X},
    {5001,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_HIT_OVER,
     0,0,0,1,IK_CNS_CTRL_HAS_CTRL},

    {5010,IK_CNS_CTRL_GET_HIT_ANIM,IK_CNS_TRIGGER_TIME_EQ,
     1,0,1,0,0u},
    {5010,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_HIT_LAUNCH,
     0,0,5030,0,0u},
    {5010,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_HIT_NO_LAUNCH,
     0,0,5011,0,0u},
    {5011,IK_CNS_CTRL_HIT_VEL_SET,IK_CNS_TRIGGER_TIME_EQ,
     1,0,0,0,IK_CNS_CTRL_AXIS_X},
    {5011,IK_CNS_CTRL_VEL_MUL,IK_CNS_TRIGGER_HIT_SLIDE_TIME,
     0,0,154,0,IK_CNS_CTRL_AXIS_X},
    {5011,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_HIT_OVER,
     0,0,11,1,IK_CNS_CTRL_HAS_CTRL},

    {5020,IK_CNS_CTRL_GET_HIT_ANIM,IK_CNS_TRIGGER_TIME_EQ,
     1,0,2,0,0u},
    {5020,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_TIME_EQ,
     1,0,5030,0,0u},
    {5030,IK_CNS_CTRL_HIT_VEL_SET,IK_CNS_TRIGGER_TIME_EQ,
     1,0,0,0,IK_CNS_CTRL_AXIS_X|IK_CNS_CTRL_AXIS_Y},
    {5030,IK_CNS_CTRL_HIT_RECOVER_STATE,IK_CNS_TRIGGER_HIT_OVER,
     0,0,0,0,0u},
    {5030,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_ANIM_END,
     0,0,5035,0,0u},
    {5035,IK_CNS_CTRL_HIT_RECOVER_STATE,IK_CNS_TRIGGER_HIT_OVER,
     0,0,0,0,0u},
    {5035,IK_CNS_CTRL_HIT_RECOVER_STATE,IK_CNS_TRIGGER_ANIM_END,
     0,0,0,0,0u},
    {5070,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_TIME_EQ,
     1,0,5071,0,0u},
    {5071,IK_CNS_CTRL_HIT_VEL_SET,IK_CNS_TRIGGER_TIME_EQ,
     1,0,0,0,IK_CNS_CTRL_AXIS_X|IK_CNS_CTRL_AXIS_Y},
    {5100,IK_CNS_CTRL_POS_SET,IK_CNS_TRIGGER_TIME_EQ,
     1,0,0,0,IK_CNS_CTRL_AXIS_Y},
    {5100,IK_CNS_CTRL_VEL_SET,IK_CNS_TRIGGER_TIME_EQ,
     1,0,0,0,IK_CNS_CTRL_AXIS_Y},
    {5100,IK_CNS_CTRL_VEL_MUL,IK_CNS_TRIGGER_TIME_EQ,
     1,0,192,0,IK_CNS_CTRL_AXIS_X},
    {5100,IK_CNS_CTRL_FALL_GROUND_BRANCH,IK_CNS_TRIGGER_TIME_EQ,
     1,0,5110,0,0u},
    {5100,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_ANIM_END,
     0,0,5101,0,0u},
    {5101,IK_CNS_CTRL_FALL_BOUNCE_VEL,IK_CNS_TRIGGER_TIME_EQ,
     1,0,0,0,0u},
    {5101,IK_CNS_CTRL_POS_SET,IK_CNS_TRIGGER_TIME_EQ,
     1,0,0,5120,IK_CNS_CTRL_AXIS_Y},
    {5101,IK_CNS_CTRL_POS_ADD,IK_CNS_TRIGGER_TIME_EQ,
     1,0,0,0,0u},
    {5110,IK_CNS_CTRL_POS_SET,IK_CNS_TRIGGER_TIME_EQ,
     1,0,0,0,IK_CNS_CTRL_AXIS_Y},
    {5110,IK_CNS_CTRL_VEL_SET,IK_CNS_TRIGGER_TIME_EQ,
     1,0,0,0,IK_CNS_CTRL_AXIS_Y},
    {5110,IK_CNS_CTRL_VEL_MUL,IK_CNS_TRIGGER_ALWAYS,
     0,0,218,0,IK_CNS_CTRL_AXIS_X},
    {5110,IK_CNS_CTRL_VEL_SET,IK_CNS_TRIGGER_ABS_VX_LT_Q8,
     13,0,0,0,IK_CNS_CTRL_AXIS_X},
    {5110,IK_CNS_CTRL_POS_ADD_VEL,IK_CNS_TRIGGER_ALWAYS,
     0,0,0,0,IK_CNS_CTRL_AXIS_X},
    {5110,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_TIME_EQ,
     60,0,5120,0,0u},
    {5120,IK_CNS_CTRL_VEL_SET,IK_CNS_TRIGGER_TIME_EQ,
     1,0,0,0,IK_CNS_CTRL_AXIS_X},
    {5120,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_ANIM_END,
     0,0,0,1,IK_CNS_CTRL_HAS_CTRL},
    {5080,IK_CNS_CTRL_DOWNED_HIT_BRANCH,IK_CNS_TRIGGER_TIME_EQ,
     1,0,0,0,0u},
    {5081,IK_CNS_CTRL_HIT_VEL_SET,IK_CNS_TRIGGER_TIME_EQ,
     1,0,0,0,IK_CNS_CTRL_AXIS_X},
    {5081,IK_CNS_CTRL_VEL_SET,IK_CNS_TRIGGER_HIT_OVER,
     0,0,0,0,IK_CNS_CTRL_AXIS_X},
    {5081,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_HIT_OVER,
     0,0,5110,0,0u},
};

#define S(no,anim,hoff,hcnt,coff,ccnt)     {no,anim,0,0,0,IK_CNS_STATE_STAND,IK_CNS_MOVE_ATTACK,IK_CNS_PHYS_STAND,0,2,0u,hoff,hcnt,0u,0u,coff,ccnt,0}
#define C(no,anim,hoff,hcnt,coff,ccnt)     {no,anim,0,0,0,IK_CNS_STATE_CROUCH,IK_CNS_MOVE_ATTACK,IK_CNS_PHYS_CROUCH,0,2,0u,hoff,hcnt,0u,0u,coff,ccnt,0}
#define A(no,anim,hoff,hcnt,coff,ccnt)     {no,anim,0,0,0,IK_CNS_STATE_AIR,IK_CNS_MOVE_ATTACK,IK_CNS_PHYS_AIR,0,2,0u,hoff,hcnt,0u,0u,coff,ccnt,0}

static const ik_cns_state_t k_states[] = {
    S(200,200,0,1,0,1),
    S(201,200,13,1,70,0),
    S(202,200,14,1,70,0),
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
    {120,-1,0,0,0,IK_CNS_STATE_UNCHANGED,IK_CNS_MOVE_IDLE,IK_CNS_PHYS_NONE,
     0,0,0u,0u,0u,0u,0u,14u,2u,0},
    {130,130,0,0,0,IK_CNS_STATE_STAND,IK_CNS_MOVE_IDLE,IK_CNS_PHYS_STAND,
     0,0,0u,0u,0u,0u,0u,16u,1u,0},
    {131,131,0,0,0,IK_CNS_STATE_CROUCH,IK_CNS_MOVE_IDLE,IK_CNS_PHYS_CROUCH,
     0,0,0u,0u,0u,0u,0u,17u,1u,0},
    {132,132,0,0,0,IK_CNS_STATE_AIR,IK_CNS_MOVE_IDLE,IK_CNS_PHYS_NONE,
     0,0,0u,0u,0u,0u,0u,18u,0u,130},
    {140,-1,0,0,0,IK_CNS_STATE_UNCHANGED,IK_CNS_MOVE_IDLE,IK_CNS_PHYS_NONE,
     1,0,0u,0u,0u,0u,0u,18u,2u,0},
    {150,150,0,0,0,IK_CNS_STATE_STAND,IK_CNS_MOVE_HIT,IK_CNS_PHYS_NONE,
     0,0,1u,0u,0u,0u,0u,20u,1u,0},
    {151,150,0,0,0,IK_CNS_STATE_STAND,IK_CNS_MOVE_HIT,IK_CNS_PHYS_STAND,
     0,0,0u,0u,0u,0u,0u,21u,4u,0},
    {152,151,0,0,0,IK_CNS_STATE_CROUCH,IK_CNS_MOVE_HIT,IK_CNS_PHYS_NONE,
     0,0,1u,0u,0u,0u,0u,25u,1u,0},
    {153,151,0,0,0,IK_CNS_STATE_CROUCH,IK_CNS_MOVE_HIT,IK_CNS_PHYS_CROUCH,
     0,0,0u,0u,0u,0u,0u,26u,4u,0},
    {154,152,0,0,0,IK_CNS_STATE_AIR,IK_CNS_MOVE_HIT,IK_CNS_PHYS_NONE,
     0,0,1u,0u,0u,0u,0u,30u,1u,0},
    {155,152,0,0,0,IK_CNS_STATE_AIR,IK_CNS_MOVE_HIT,IK_CNS_PHYS_NONE,
     0,0,0u,0u,0u,0u,0u,31u,2u,52},
    {5000,-1,0,0,0,IK_CNS_STATE_STAND,IK_CNS_MOVE_HIT,IK_CNS_PHYS_NONE,
     0,0,1u,0u,0u,0u,0u,33u,3u,0},
    {5001,-1,0,0,0,IK_CNS_STATE_STAND,IK_CNS_MOVE_HIT,IK_CNS_PHYS_STAND,
     0,0,0u,0u,0u,0u,0u,36u,3u,0},
    {5010,-1,0,0,0,IK_CNS_STATE_CROUCH,IK_CNS_MOVE_HIT,IK_CNS_PHYS_NONE,
     0,0,1u,0u,0u,0u,0u,39u,3u,0},
    {5011,-1,0,0,0,IK_CNS_STATE_CROUCH,IK_CNS_MOVE_HIT,IK_CNS_PHYS_CROUCH,
     0,0,0u,0u,0u,0u,0u,42u,3u,0},
    {5020,-1,0,0,0,IK_CNS_STATE_AIR,IK_CNS_MOVE_HIT,IK_CNS_PHYS_NONE,
     0,0,1u,0u,0u,0u,0u,45u,2u,0},
    {5030,5030,0,0,0,IK_CNS_STATE_AIR,IK_CNS_MOVE_HIT,IK_CNS_PHYS_NONE,
     0,0,0u,0u,0u,0u,0u,47u,3u,0,0,6400},
    {5035,5035,0,0,0,IK_CNS_STATE_AIR,IK_CNS_MOVE_HIT,IK_CNS_PHYS_NONE,
     0,0,0u,0u,0u,0u,0u,50u,2u,0,0,6400},
    {5040,5040,0,0,0,IK_CNS_STATE_AIR,IK_CNS_MOVE_HIT,IK_CNS_PHYS_NONE,
     1,0,0u,0u,0u,0u,0u,52u,0u,52,0,0},
    {5050,5050,0,0,0,IK_CNS_STATE_AIR,IK_CNS_MOVE_HIT,IK_CNS_PHYS_NONE,
     0,0,0u,0u,0u,0u,0u,52u,0u,5100,0,6400},
    {5070,5070,0,0,0,IK_CNS_STATE_AIR,IK_CNS_MOVE_HIT,IK_CNS_PHYS_NONE,
     0,0,1u,0u,0u,0u,0u,52u,1u,0,0,0},
    {5071,-1,0,0,0,IK_CNS_STATE_AIR,IK_CNS_MOVE_HIT,IK_CNS_PHYS_NONE,
     0,0,0u,0u,0u,0u,0u,53u,1u,5110,0,3840},
    {5100,5100,0,0,0,IK_CNS_STATE_LIEDOWN,IK_CNS_MOVE_HIT,IK_CNS_PHYS_NONE,
     0,0,0u,0u,0u,0u,0u,54u,5u,0,0,0},
    {5101,5160,0,0,0,IK_CNS_STATE_LIEDOWN,IK_CNS_MOVE_HIT,IK_CNS_PHYS_NONE,
     0,0,0u,0u,0u,0u,0u,59u,3u,5110,102,3072},
    {5110,5110,0,0,0,IK_CNS_STATE_LIEDOWN,IK_CNS_MOVE_HIT,IK_CNS_PHYS_NONE,
     0,0,0u,0u,0u,0u,0u,62u,6u,0,0,0},
    {5120,5120,0,0,0,IK_CNS_STATE_LIEDOWN,IK_CNS_MOVE_IDLE,IK_CNS_PHYS_NONE,
     0,0,0u,0u,0u,0u,0u,68u,2u,0,0,0},
    {5080,-1,0,0,0,IK_CNS_STATE_LIEDOWN,IK_CNS_MOVE_HIT,IK_CNS_PHYS_NONE,
     0,0,1u,0u,0u,0u,0u,70u,1u,0,0,0},
    {5081,-1,0,0,0,IK_CNS_STATE_LIEDOWN,IK_CNS_MOVE_HIT,IK_CNS_PHYS_CROUCH,
     0,0,0u,0u,0u,0u,0u,71u,3u,0,0,0},
};
#undef S
#undef C
#undef A

static const ik_cns_asset_t k_cns = {
    {1000,15,16,12,12,60,160,614,-563,1178,0,-1152,-973,0,-2150,-653,640,1024,-2074,0,-2074,-653,640,1,35,113,218,210,512,13,
     60,6400,3840,0,5120,102,3072,13},
    k_states,(uint16_t)(sizeof(k_states)/sizeof(k_states[0])),
    k_hitdefs,15u,
    nullptr,0u,
    k_ctrls,(uint16_t)(sizeof(k_ctrls)/sizeof(k_ctrls[0]))
};


static const ik_cns_controller_t k_common_ctrls[] = {
    {0,IK_CNS_CTRL_VEL_SET,IK_CNS_TRIGGER_TIME_EQ,
     4,0,0,0,IK_CNS_CTRL_AXIS_X},
    {0,IK_CNS_CTRL_VEL_SET,IK_CNS_TRIGGER_ABS_VX_LT_Q8,
     512,0,0,0,IK_CNS_CTRL_AXIS_X},

    {20,IK_CNS_CTRL_VEL_SET,IK_CNS_TRIGGER_COMMAND_ACTIVE,
     IK_CNS_COMMAND_HOLD_BACK,0,-563,0,
     IK_CNS_CTRL_AXIS_X|IK_CNS_CTRL_LOCAL_X},
    {20,IK_CNS_CTRL_VEL_SET,IK_CNS_TRIGGER_COMMAND_ACTIVE,
     IK_CNS_COMMAND_HOLD_FWD,0,614,0,
     IK_CNS_CTRL_AXIS_X|IK_CNS_CTRL_LOCAL_X},
    {20,IK_CNS_CTRL_CHANGE_ANIM_BY_VX,IK_CNS_TRIGGER_ALWAYS,
     0,0,-1,20,0u},

    {52,IK_CNS_CTRL_VEL_SET,IK_CNS_TRIGGER_TIME_EQ,
     1,0,0,0,IK_CNS_CTRL_AXIS_Y},
    {52,IK_CNS_CTRL_POS_SET,IK_CNS_TRIGGER_TIME_EQ,
     1,0,0,0,IK_CNS_CTRL_AXIS_Y},
    {52,IK_CNS_CTRL_CTRL_SET,IK_CNS_TRIGGER_TIME_EQ,
     3,0,1,0,0u},
    {52,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_ANIM_END,
     0,0,0,1,IK_CNS_CTRL_HAS_CTRL},

    {100,IK_CNS_CTRL_VEL_SET,IK_CNS_TRIGGER_ALWAYS,
     0,0,1178,0,IK_CNS_CTRL_AXIS_X|IK_CNS_CTRL_LOCAL_X},
    {100,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_COMMAND_INACTIVE,
     IK_CNS_COMMAND_HOLD_FWD,0,0,0,0u},

    {105,IK_CNS_CTRL_VEL_SET,IK_CNS_TRIGGER_TIME_EQ,
     1,0,-1152,-973,
     IK_CNS_CTRL_AXIS_X|IK_CNS_CTRL_AXIS_Y|IK_CNS_CTRL_LOCAL_X},
    {105,IK_CNS_CTRL_CTRL_SET,IK_CNS_TRIGGER_TIME_EQ,
     2,0,1,0,0u},

    {106,IK_CNS_CTRL_VEL_SET,IK_CNS_TRIGGER_TIME_EQ,
     1,0,0,0,IK_CNS_CTRL_AXIS_Y},
    {106,IK_CNS_CTRL_POS_SET,IK_CNS_TRIGGER_TIME_EQ,
     1,0,0,0,IK_CNS_CTRL_AXIS_Y},
    {106,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_TIME_EQ,
     7,0,0,1,IK_CNS_CTRL_HAS_CTRL},

    {40,IK_CNS_CTRL_CAPTURE_COMMAND_AXIS,IK_CNS_TRIGGER_ALWAYS,
     0,0,0,0,0u},
    {40,IK_CNS_CTRL_JUMP_LAUNCH,IK_CNS_TRIGGER_ANIM_END,
     0,0,0,0,0u},
    {40,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_ANIM_END,
     0,0,50,1,IK_CNS_CTRL_HAS_CTRL},

    {45,IK_CNS_CTRL_CHANGE_ANIM_IF_EXISTS,IK_CNS_TRIGGER_TIME_EQ,
     1,0,44,41,0u},
    {45,IK_CNS_CTRL_CAPTURE_COMMAND_AXIS,IK_CNS_TRIGGER_ALWAYS,
     0,0,0,0,0u},
    {45,IK_CNS_CTRL_AIR_JUMP_LAUNCH,IK_CNS_TRIGGER_TIME_EQ,
     2,0,0,0,0u},
    {45,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_TIME_EQ,
     2,0,50,1,IK_CNS_CTRL_HAS_CTRL},

    {50,IK_CNS_CTRL_CHANGE_ANIM_BY_VX,IK_CNS_TRIGGER_TIME_EQ,
     1,0,41,42,0u},
    {50,IK_CNS_CTRL_CHANGE_ANIM_DESCENT_IF_EXISTS,IK_CNS_TRIGGER_ALWAYS,
     0,0,-512,41,0u},
};

static const ik_cns_state_t k_common_states[] = {
    {0,0,0,0,0,IK_CNS_STATE_STAND,IK_CNS_MOVE_IDLE,IK_CNS_PHYS_STAND,
     1,0,0u,0u,0u,0u,0u,0u,2u,0},
    {20,-1,0,0,0,IK_CNS_STATE_STAND,IK_CNS_MOVE_IDLE,IK_CNS_PHYS_STAND,
     1,0,0u,0u,0u,0u,0u,2u,3u,0},
    {40,40,0,0,0,IK_CNS_STATE_STAND,IK_CNS_MOVE_IDLE,IK_CNS_PHYS_STAND,
     0,1,0u,0u,0u,0u,0u,16u,3u,0},
    {45,41,0,0,0,IK_CNS_STATE_AIR,IK_CNS_MOVE_IDLE,IK_CNS_PHYS_NONE,
     0,0,1u,0u,0u,0u,0u,19u,4u,0},
    {50,-1,0,0,0,IK_CNS_STATE_AIR,IK_CNS_MOVE_IDLE,IK_CNS_PHYS_AIR,
     0,0,0u,0u,0u,0u,0u,23u,2u,52},
    {51,-1,0,0,0,IK_CNS_STATE_AIR,IK_CNS_MOVE_IDLE,IK_CNS_PHYS_AIR,
     0,0,0u,0u,0u,0u,0u,25u,0u,52},
    {52,47,0,0,0,IK_CNS_STATE_STAND,IK_CNS_MOVE_IDLE,IK_CNS_PHYS_STAND,
     0,0,0u,0u,0u,0u,0u,5u,4u,0},
    {100,100,0,0,0,IK_CNS_STATE_STAND,IK_CNS_MOVE_IDLE,IK_CNS_PHYS_STAND,
     1,0,0u,0u,0u,0u,0u,9u,2u,0},
    {105,105,0,0,0,IK_CNS_STATE_AIR,IK_CNS_MOVE_IDLE,IK_CNS_PHYS_AIR,
     0,0,0u,0u,0u,0u,0u,11u,2u,106},
    {106,47,0,0,0,IK_CNS_STATE_STAND,IK_CNS_MOVE_IDLE,IK_CNS_PHYS_STAND,
     0,0,0u,0u,0u,0u,0u,13u,3u,0},
};

static const ik_cns_asset_t k_common_cns = {
    {1000,15,16,12,12,60,160,614,-563,1178,0,-1152,-973,
     0,-2150,-653,640,1024,-2074,
     0,-2074,-653,640,1,35,
     113,218,210,512,13},
    k_common_states,
    (uint16_t)(sizeof(k_common_states)/sizeof(k_common_states[0])),
    nullptr,0u,
    nullptr,0u,
    k_common_ctrls,
    (uint16_t)(sizeof(k_common_ctrls)/sizeof(k_common_ctrls[0]))
};

static const ik_cns_controller_t k_recovery_ctrls[] = {
    {5050,IK_CNS_CTRL_FALL_RECOVERY,IK_CNS_TRIGGER_ALWAYS,
     0,0,0,0,0u},
    {5200,IK_CNS_CTRL_CHANGE_ANIM_IF_END_FROM,IK_CNS_TRIGGER_ALWAYS,
     0,0,5035,5050,0u},
    {5201,IK_CNS_CTRL_VEL_SET,IK_CNS_TRIGGER_TIME_EQ,
     1,0,-38,-896,
     IK_CNS_CTRL_AXIS_X|IK_CNS_CTRL_AXIS_Y|IK_CNS_CTRL_LOCAL_X},
    {5201,IK_CNS_CTRL_POS_SET,IK_CNS_TRIGGER_TIME_EQ,
     1,0,0,0,IK_CNS_CTRL_AXIS_Y},
    {5210,IK_CNS_CTRL_VEL_MUL,IK_CNS_TRIGGER_TIME_EQ,
     4,0,128,51,IK_CNS_CTRL_AXIS_X|IK_CNS_CTRL_AXIS_Y},
    {5210,IK_CNS_CTRL_VEL_ADD,IK_CNS_TRIGGER_TIME_EQ,
     4,0,0,-1152,IK_CNS_CTRL_AXIS_X|IK_CNS_CTRL_AXIS_Y},
    {5210,IK_CNS_CTRL_VEL_ADD,IK_CNS_TRIGGER_COMMAND_ACTIVE,
     IK_CNS_COMMAND_HOLD_UP,0,0,-512,IK_CNS_CTRL_AXIS_Y},
    {5210,IK_CNS_CTRL_VEL_ADD,IK_CNS_TRIGGER_COMMAND_ACTIVE,
     IK_CNS_COMMAND_HOLD_DOWN,0,0,384,IK_CNS_CTRL_AXIS_Y},
    {5210,IK_CNS_CTRL_VEL_ADD,IK_CNS_TRIGGER_COMMAND_ACTIVE,
     IK_CNS_COMMAND_HOLD_FWD,0,0,0,
     IK_CNS_CTRL_AXIS_X|IK_CNS_CTRL_LOCAL_X},
    {5210,IK_CNS_CTRL_VEL_ADD,IK_CNS_TRIGGER_COMMAND_ACTIVE,
     IK_CNS_COMMAND_HOLD_BACK,0,-256,0,
     IK_CNS_CTRL_AXIS_X|IK_CNS_CTRL_LOCAL_X},
    {5210,IK_CNS_CTRL_CTRL_SET,IK_CNS_TRIGGER_TIME_EQ,
     20,0,1,0,0u},
};

static const ik_cns_state_t k_recovery_states[] = {
    {5050,5050,0,0,0,IK_CNS_STATE_AIR,IK_CNS_MOVE_HIT,IK_CNS_PHYS_NONE,
     0,0,0u,0u,0u,0u,0u,0u,1u,5100,0,6400,0u},
    {5200,-1,0,0,0,IK_CNS_STATE_AIR,IK_CNS_MOVE_HIT,IK_CNS_PHYS_NONE,
     0,0,0u,0u,0u,0u,0u,1u,1u,5201,0,2560,0u},
    {5201,5200,0,0,0,IK_CNS_STATE_AIR,IK_CNS_MOVE_HIT,IK_CNS_PHYS_AIR,
     0,0,0u,0u,0u,0u,0u,2u,2u,52,0,0,0u},
    {5210,5210,0,0,0,IK_CNS_STATE_AIR,IK_CNS_MOVE_IDLE,IK_CNS_PHYS_NONE,
     0,0,0u,0u,0u,0u,0u,4u,7u,52,90,0,4u,1u},
};

static const ik_cns_asset_t k_recovery_cns = {
    {1000,15,16,12,12,60,160,614,-563,1178,0,-1152,-973,
     0,-2150,-653,640,1024,-2074,
     0,-2074,-653,640,1,35,
     113,218,210,512,13,
     60,6400,3840,0,5120,102,3072,13,
     -38,-896,-5120,2560,128,51,0,-1152,-256,0,-512,384,-256,90},
    k_recovery_states,
    (uint16_t)(sizeof(k_recovery_states)/sizeof(k_recovery_states[0])),
    nullptr,0u,
    nullptr,0u,
    k_recovery_ctrls,
    (uint16_t)(sizeof(k_recovery_ctrls)/sizeof(k_recovery_ctrls[0]))
};

static const ik_cns_hitdef_t k_downed_hitdefs[] = {
    {200,IK_CNS_TRIGGER_TIME_EQ,0,
     23,0,3u,0u,0u,
     IK_CNS_GROUND_HIGH,3u,3u,3u,
     -1024,0,-358,-768,
     0,-10,-76,5,0,6,0,0u,
     0u,1u,0u,0u,0u,0,0,0,0u,0u,
     0,-1152,0u,1u,4u,22u,-1280,0,0u,IK_CNS_HIT_DEFAULT|IK_CNS_HIT_DOWN}
};

static const ik_cns_controller_t k_downed_ctrls[] = {
    {5080,IK_CNS_CTRL_DOWNED_HIT_BRANCH,IK_CNS_TRIGGER_TIME_EQ,
     1,0,0,0,0u},
    {5081,IK_CNS_CTRL_HIT_VEL_SET,IK_CNS_TRIGGER_TIME_EQ,
     1,0,0,0,IK_CNS_CTRL_AXIS_X},
    {5081,IK_CNS_CTRL_VEL_SET,IK_CNS_TRIGGER_HIT_OVER,
     0,0,0,0,IK_CNS_CTRL_AXIS_X},
    {5081,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_HIT_OVER,
     0,0,5110,0,0u},
    {5110,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_NOT_ALIVE,
     0,0,5150,0,0u},
};

static const ik_cns_state_t k_downed_states[] = {
    {200,200,0,0,0,
     IK_CNS_STATE_STAND,IK_CNS_MOVE_ATTACK,IK_CNS_PHYS_STAND,
     0,2,0u,0u,1u,0u,0u,0u,0u,0,0,0,0u},
    {5080,-1,0,0,0,
     IK_CNS_STATE_LIEDOWN,IK_CNS_MOVE_HIT,IK_CNS_PHYS_NONE,
     0,0,1u,0u,0u,0u,0u,0u,1u,0,0,0,0u},
    {5081,-1,0,0,0,
     IK_CNS_STATE_LIEDOWN,IK_CNS_MOVE_HIT,IK_CNS_PHYS_CROUCH,
     0,0,0u,0u,0u,0u,0u,1u,3u,0,0,0,0u},
    {5110,5110,0,0,0,
     IK_CNS_STATE_LIEDOWN,IK_CNS_MOVE_HIT,IK_CNS_PHYS_NONE,
     0,0,0u,0u,0u,0u,0u,4u,1u,0,0,0,0u},
    {5150,5150,0,0,0,
     IK_CNS_STATE_LIEDOWN,IK_CNS_MOVE_HIT,IK_CNS_PHYS_NONE,
     0,-3,0u,0u,0u,0u,0u,5u,0u,0,0,0,0u},
};

static const ik_cns_asset_t k_downed_cns = {
    {1000,15,16,12,12,60,160},
    k_downed_states,
    (uint16_t)(sizeof(k_downed_states)/sizeof(k_downed_states[0])),
    k_downed_hitdefs,
    (uint16_t)(sizeof(k_downed_hitdefs)/sizeof(k_downed_hitdefs[0])),
    nullptr,0u,
    k_downed_ctrls,
    (uint16_t)(sizeof(k_downed_ctrls)/sizeof(k_downed_ctrls[0]))
};

static void tick(ik_fight_t* g, const ik_fight_controls_t* p) {
    ik_fight_update(g,p,nullptr,&k_table,&k_table);
}

static void tick2(ik_fight_t* g,
                  const ik_fight_controls_t* p1,
                  const ik_fight_controls_t* p2) {
    ik_fight_update(g,p1,p2,&k_table,&k_table);
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

    /* Generic VarSet/VarAdd write entity-local MUGEN vars rather than adding
     * another variable array to ik_fighter_t. Binding survives round reset. */
    {
        const ik_cns_controller_t var_ctrls[] = {
            {0,IK_CNS_CTRL_VAR_SET,IK_CNS_TRIGGER_ALWAYS,
             0,0,7,123456,0u},
            {0,IK_CNS_CTRL_VAR_ADD,IK_CNS_TRIGGER_TIME_EQ,
             1,0,7,44,0u},
        };
        ik_cns_state_t var_state=k_states[0];
        var_state.number=0;
        var_state.anim=0;
        var_state.hitdef_ofs=0u;
        var_state.hitdef_count=0u;
        var_state.playsnd_ofs=0u;
        var_state.playsnd_count=0u;
        var_state.controller_ofs=0u;
        var_state.controller_count=2u;

        ik_cns_asset_t asset=k_cns;
        asset.states=&var_state;
        asset.state_count=1u;
        asset.hitdefs=nullptr;
        asset.hitdef_count=0u;
        asset.playsnds=nullptr;
        asset.playsnd_count=0u;
        asset.controllers=var_ctrls;
        asset.controller_count=2u;

        ik_entity_pool_t pool{};
        ik_entity_pool_init(&pool);
        ik_entity_handle_t p1{};
        ik_entity_handle_t p2{};
        OK(ik_entity_spawn(
            &pool,IK_ENTITY_PLAYER,1,0u,
            ik_entity_invalid_handle(),&p1));
        OK(ik_entity_spawn(
            &pool,IK_ENTITY_PLAYER,2,1u,
            ik_entity_invalid_handle(),&p2));

        ik_fight_init(&g,&asset);
        ik_fight_bind_entities(&g,&pool,p1,p2);
        ik_fight_controls_t idle_controls{};
        tick(&g,&idle_controls);
        ik_entity_t* p1_entity=ik_entity_get(&pool,p1);
        OK(p1_entity!=nullptr);
        EQ(p1_entity->vars[7],123456);
        tick(&g,&idle_controls);
        EQ(p1_entity->vars[7],123500);

        ik_fight_reset(&g);
        OK(g.entities==&pool);
        OK(ik_entity_handle_equal(g.player_entities[0],p1));
        EQ(p1_entity->vars[7],123500);
    }

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
        EQ(g.fighters[1].state,5000);
        EQ(g.fighters[1].vx_q8,0);
        idle(&g,9);
        OK(g.fighters[1].state==5001 || g.fighters[1].state==0);
    }

    /* Equal-priority Hit/Hit attacks trade: contacts are gathered before
     * either fighter is moved into a get-hit state. */
    {
        ik_fight_init(&g,&k_cns); place(&g,100,145);
        ik_fight_controls_t p1{}; request(&p1,IK_STATE_PUNCH);
        ik_fight_controls_t p2{}; request(&p2,IK_STATE_PUNCH);
        for(int i=0;i<12 && g.hits_p1==0u && g.hits_p2==0u;++i) {
            tick2(&g,&p1,&p2);
            p1.has_state_request=0u;
            p2.has_state_request=0u;
        }
        EQ(g.hits_p1,1u);
        EQ(g.hits_p2,1u);
        EQ(g.effect_count,2u);
        EQ(g.effect_events[0].action,0);
        EQ(g.effect_events[1].action,0);
        EQ(g.fighters[0].hp,977);
        EQ(g.fighters[1].hp,977);
    }

    /* Numeric priority wins before the equal-priority class tiebreaker. */
    {
        ik_fight_init(&g,&k_cns); place(&g,100,145);
        ik_fight_controls_t p1{}; request(&p1,IK_STATE_PUNCH);
        ik_fight_controls_t p2{}; request(&p2,IK_STATE_STRONG_PUNCH);
        for(int i=0;i<16 && g.hits_p2==0u;++i) {
            tick2(&g,&p1,&p2);
            p1.has_state_request=0u;
            p2.has_state_request=0u;
        }
        EQ(g.hits_p1,0u);
        EQ(g.hits_p2,1u);
        EQ(g.fighters[0].hp,943);
        EQ(g.fighters[1].hp,1000);
    }

    /* Equal priority Hit vs Miss: Hit lands, Miss is deactivated. */
    {
        constexpr unsigned hitdef_count=
            (unsigned)(sizeof(k_hitdefs)/sizeof(k_hitdefs[0]));
        ik_cns_hitdef_t hitdefs[hitdef_count];
        for(unsigned i=0;i<hitdef_count;++i) hitdefs[i]=k_hitdefs[i];
        hitdefs[0].priority=3u;
        hitdefs[0].priority_type=IK_CNS_PRIORITY_HIT;
        hitdefs[1].priority=3u;
        hitdefs[1].priority_type=IK_CNS_PRIORITY_MISS;
        ik_cns_asset_t asset=k_cns;
        asset.hitdefs=hitdefs;

        ik_fight_init(&g,&asset); place(&g,100,145);
        ik_fight_controls_t p1{}; request(&p1,IK_STATE_PUNCH);
        ik_fight_controls_t p2{}; request(&p2,IK_STATE_STRONG_PUNCH);
        for(int i=0;i<16 && g.hits_p1==0u;++i) {
            tick2(&g,&p1,&p2);
            p1.has_state_request=0u;
            p2.has_state_request=0u;
        }
        EQ(g.hits_p1,1u);
        EQ(g.hits_p2,0u);
        EQ(g.fighters[0].hp,1000);
        EQ(g.fighters[1].hp,977);
    }

    /* Equal priority Hit vs Dodge: neither connects and the no-hit tie does
     * not consume either persistent HitDef. */
    {
        constexpr unsigned hitdef_count=
            (unsigned)(sizeof(k_hitdefs)/sizeof(k_hitdefs[0]));
        ik_cns_hitdef_t hitdefs[hitdef_count];
        for(unsigned i=0;i<hitdef_count;++i) hitdefs[i]=k_hitdefs[i];
        hitdefs[0].priority=3u;
        hitdefs[0].priority_type=IK_CNS_PRIORITY_HIT;
        hitdefs[1].priority=3u;
        hitdefs[1].priority_type=IK_CNS_PRIORITY_DODGE;
        ik_cns_asset_t asset=k_cns;
        asset.hitdefs=hitdefs;

        ik_fight_init(&g,&asset); place(&g,100,145);
        ik_fight_controls_t p1{}; request(&p1,IK_STATE_PUNCH);
        ik_fight_controls_t p2{}; request(&p2,IK_STATE_STRONG_PUNCH);
        for(int i=0;i<10;++i) {
            tick2(&g,&p1,&p2);
            p1.has_state_request=0u;
            p2.has_state_request=0u;
        }
        EQ(g.hits_p1,0u);
        EQ(g.hits_p2,0u);
        EQ(g.fighters[0].hp,1000);
        EQ(g.fighters[1].hp,1000);
        EQ(g.fighters[0].hitdef_hit_mask,0u);
        EQ(g.fighters[1].hitdef_hit_mask,0u);
    }

    /* MA guardflag blocks standing. Guard hit enters common 150/151
     * rather than the legacy damage state and does not count as a hit. */
    {
        ik_fight_init(&g,&k_cns); place(&g,100,145);
        g.fighters[1].hp=3;
        ik_fight_controls_t p1{}; request(&p1,IK_STATE_PUNCH);
        ik_fight_controls_t p2{}; p2.back=1;
        for(int i=0;i<12 && !(g.events&IK_EVENT_GUARD);++i) {
            tick2(&g,&p1,&p2);
            p1.has_state_request=0u;
        }
        OK((g.events&IK_EVENT_GUARD)!=0u);
        EQ(g.fighters[1].hp,0);
        OK((g.events&IK_EVENT_KO)!=0u);
        EQ(g.hits_p1,0u);
        EQ(g.fighters[1].guard_type,IK_CNS_STATE_STAND);
        OK(g.fighters[1].state==150 || g.fighters[1].state==151);
    }

    /* L guardflag cannot be blocked standing but can be blocked crouching. */
    {
        ik_fight_init(&g,&k_cns); place(&g,100,145);
        const int hp=g.fighters[1].hp;
        ik_fight_controls_t p1{}; request(&p1,IK_STATE_CROUCH_PUNCH);
        ik_fight_controls_t p2{}; p2.back=1;
        for(int i=0;i<8 && g.hits_p1==0u;++i) {
            tick2(&g,&p1,&p2);
            p1.has_state_request=0u;
        }
        EQ(hp-g.fighters[1].hp,23);
        EQ(g.hits_p1,1u);

        ik_fight_init(&g,&k_cns); place(&g,100,145);
        g.fighters[1].hp=3;
        p1={}; request(&p1,IK_STATE_CROUCH_PUNCH);
        p2={}; p2.back=1; p2.down=1;
        for(int i=0;i<8 && !(g.events&IK_EVENT_GUARD);++i) {
            tick2(&g,&p1,&p2);
            p1.has_state_request=0u;
        }
        OK((g.events&IK_EVENT_GUARD)!=0u);
        EQ(g.fighters[1].hp,1);
        OK((g.events&IK_EVENT_KO)==0u);
        EQ(g.fighters[1].guard_type,IK_CNS_STATE_CROUCH);
        EQ(g.fighters[1].state,152);
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
        EQ(g.fighters[1].state,5070);
        OK(g.fighters[1].gethit_vy_q8<0);
        EQ(g.fighters[1].gethit_fall_y_q8,-1152);
        EQ(g.fighters[1].vy_q8,0);

        for(int i=0;i<160 && g.fighters[1].state!=5110;++i) idle(&g,1);
        EQ(g.fighters[1].state,5110);
        EQ(g.fighters[1].on_ground,1);
    }

    /* Falling state 5050 reaches 5100, applies the default MUGEN
     * fall.yvelocity bounce, lies down, then enters get-up 5120. */
    {
        ik_fight_init(&g,&k_cns);
        ik_fighter_t* f=&g.fighters[1];
        f->state=5050;
        f->anim=5050;
        f->state_time=0u;
        f->anim_time=0u;
        f->on_ground=0;
        f->ctrl=0;
        f->y=170;
        f->y_q8=170*256;
        f->vx_q8=512;
        f->vy_q8=512;
        f->gethit_fall=1u;
        f->gethit_fall_y_q8=-1152;
        f->gethit_fall_x_set=0u;

        for(int i=0;i<80 && f->state!=5101;++i) idle(&g,1);
        EQ(f->state,5101);
        idle(&g,1);
        EQ(f->on_ground,0);
        EQ(f->vy_q8,-1050);
        OK(f->y_q8>IK_FLOOR_Y*256);

        for(int i=0;i<100 && f->state!=5110;++i) idle(&g,1);
        EQ(f->state,5110);
        EQ(f->on_ground,1);

        int saw_getup=0;
        for(int i=0;i<90 && f->state!=IK_STATE_IDLE;++i) {
            idle(&g,1);
            if(f->state==5120) saw_getup=1;
        }
        OK(saw_getup);
        EQ(f->state,IK_STATE_IDLE);
        EQ(f->ctrl,1);
    }

    /* Jump normals are compiled CNS states with Physics=A. They remain
     * airborne while attacking and use their original Time=0 HitDefs. */
    {
        ik_fight_init(&g,&k_cns); place(&g,100,145);
        g.fighters[0].state=IK_STATE_JUMP;
        g.fighters[0].on_ground=0;
        g.fighters[0].vy_q8=-2150;
        ik_fight_controls_t p{}; request(&p,IK_STATE_JUMP_PUNCH);
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

        g.fighters[0].state=IK_STATE_JUMP;
        g.fighters[0].on_ground=0;
        g.fighters[0].vy_q8=-2150;
        ik_fight_controls_t p{}; request(&p,IK_STATE_JUMP_PUNCH);
        tick(&g,&p);

        EQ(g.hits_p1,1u);
        EQ(g.fighters[1].hitstun,14u);
        EQ(g.fighters[1].state,5020);
        EQ(g.fighters[1].vx_q8,0);
        EQ(g.fighters[1].vy_q8,0);
        EQ(g.fighters[1].gethit_vx_q8,-333);
        EQ(g.fighters[1].gethit_vy_q8,-768);
        EQ(g.fighters[1].on_ground,0);
    }

    /* State 600's original CtrlSet at Time=17 is executed by the generic
     * controller runtime, allowing new controlled air input afterwards. */
    {
        ik_fight_init(&g,&k_cns); place(&g,60,260);
        g.fighters[0].state=IK_STATE_JUMP;
        g.fighters[0].on_ground=0;
        g.fighters[0].vy_q8=-2150;
        ik_fight_controls_t p{}; request(&p,IK_STATE_JUMP_PUNCH);
        tick(&g,&p);
        EQ(g.fighters[0].ctrl,0);
        idle(&g,17);
        EQ(g.fighters[0].ctrl,1);
        EQ(g.fighters[0].state,IK_STATE_JUMP_PUNCH);
    }

    /* Compiled common state 20 owns walking velocity and animation. */
    {
        ik_fight_init(&g,&k_common_cns);
        const int32_t x0=g.fighters[0].x_q8;
        ik_fight_controls_t p{}; p.forward=1;
        tick(&g,&p);
        EQ(g.fighters[0].state,IK_STATE_WALK);
        tick(&g,&p);
        EQ(g.fighters[0].anim,20);
        EQ(g.fighters[0].x_q8,x0+614);

        p={};
        tick(&g,&p);
        EQ(g.fighters[0].state,IK_STATE_IDLE);
    }

    /* FF/BB State -1 requests can now enter real common run/hop states. */
    {
        ik_fight_init(&g,&k_common_cns);
        ik_fight_controls_t p{}; p.forward=1; request(&p,100);
        tick(&g,&p);
        EQ(g.fighters[0].state,100);
        const int32_t x0=g.fighters[0].x_q8;

        p={}; p.forward=1;
        tick(&g,&p);
        EQ(g.fighters[0].state,100);
        EQ(g.fighters[0].x_q8,x0+1178);

        p={};
        tick(&g,&p);
        EQ(g.fighters[0].state,IK_STATE_IDLE);
    }

    {
        ik_fight_init(&g,&k_common_cns);
        ik_fight_controls_t p{}; request(&p,105);
        tick(&g,&p);
        EQ(g.fighters[0].state,105);
        EQ(g.fighters[0].on_ground,0);

        p={};
        tick(&g,&p);
        EQ(g.fighters[0].vx_q8,-1152);
        OK(g.fighters[0].vy_q8<0);

        for(int i=0;i<80 && g.fighters[0].state!=106;++i) tick(&g,&p);
        EQ(g.fighters[0].state,106);
        EQ(g.fighters[0].on_ground,1);
        idle(&g,7);
        EQ(g.fighters[0].state,IK_STATE_IDLE);
    }

    /* Common1 owns jump startup, remembers direction until AnimTime=0,
     * launches into state 50, and lands through state 52. */
    {
        ik_fight_init(&g,&k_common_cns);
        ik_fight_controls_t p{}; p.up=1; p.forward=1;
        tick(&g,&p);
        EQ(g.fighters[0].state,40);
        EQ(g.fighters[0].on_ground,1);

        p={}; p.forward=1;
        tick(&g,&p);
        p={};
        tick(&g,&p);
        EQ(g.fighters[0].state,50);
        EQ(g.fighters[0].on_ground,0);
        EQ(g.fighters[0].vx_q8,640);
        EQ(g.fighters[0].vy_q8,-2150);

        for(int i=0;i<80 && g.fighters[0].state!=52;++i) tick(&g,&p);
        EQ(g.fighters[0].state,52);
        EQ(g.fighters[0].on_ground,1);
        idle(&g,3);
        EQ(g.fighters[0].state,IK_STATE_IDLE);
    }

    /* Jumping directly out of run uses runjump.fwd.x via prevStateNo=100. */
    {
        ik_fight_init(&g,&k_common_cns);
        ik_fight_controls_t p{}; p.forward=1; request(&p,100);
        tick(&g,&p);
        p={}; p.forward=1; p.up=1;
        tick(&g,&p);
        EQ(g.fighters[0].state,40);
        EQ(g.fighters[0].prev_state,100);

        p={}; p.forward=1;
        tick(&g,&p);
        p={};
        tick(&g,&p);
        EQ(g.fighters[0].state,50);
        EQ(g.fighters[0].vx_q8,1024);
    }

    /* Air jump requires release/re-press, the configured height threshold,
     * and consumes the compiled airjump.num budget. */
    {
        ik_fight_init(&g,&k_common_cns);
        ik_fight_controls_t p{}; p.up=1;
        tick(&g,&p);
        p={};
        tick(&g,&p);
        tick(&g,&p);
        EQ(g.fighters[0].state,50);

        for(int i=0;i<20 &&
            ((int32_t)IK_FLOOR_Y*256-g.fighters[0].y_q8)<35*256;++i) {
            tick(&g,&p);
        }
        OK(((int32_t)IK_FLOOR_Y*256-g.fighters[0].y_q8)>=35*256);

        p={}; p.up=1; p.forward=1;
        tick(&g,&p);
        EQ(g.fighters[0].state,45);
        EQ(g.fighters[0].air_jumps_used,1u);
        EQ(g.fighters[0].vx_q8,0);
        EQ(g.fighters[0].vy_q8,0);

        p={}; p.forward=1;
        tick(&g,&p);
        tick(&g,&p);
        EQ(g.fighters[0].state,50);
        EQ(g.fighters[0].vx_q8,640);
        EQ(g.fighters[0].vy_q8,-2074);
    }

    /* StateDef juggle points are spent when a hit starts fall, then gate
     * subsequent hits against falling/downed targets. */
    {
        constexpr unsigned state_count=
            (unsigned)(sizeof(k_states)/sizeof(k_states[0]));
        constexpr unsigned hitdef_count=
            (unsigned)(sizeof(k_hitdefs)/sizeof(k_hitdefs[0]));
        ik_cns_state_t states[state_count+1];
        ik_cns_hitdef_t hitdefs[hitdef_count+1];
        for(unsigned i=0;i<state_count;++i) states[i]=k_states[i];
        for(unsigned i=0;i<hitdef_count;++i) hitdefs[i]=k_hitdefs[i];

        states[state_count]=k_states[0];
        states[state_count].number=902;
        states[state_count].anim=200;
        states[state_count].hitdef_ofs=(uint16_t)hitdef_count;
        states[state_count].hitdef_count=1u;
        states[state_count].controller_count=0u;
        states[state_count].juggle=5;
        states[state_count].has_juggle=1u;

        hitdefs[hitdef_count]=k_hitdefs[0];
        hitdefs[hitdef_count].state_number=902;
        hitdefs[hitdef_count].trigger_kind=IK_CNS_TRIGGER_TIME_EQ;
        hitdefs[hitdef_count].trigger_value=0;
        hitdefs[hitdef_count].damage=1;
        hitdefs[hitdef_count].pause_p1=0u;
        hitdefs[hitdef_count].pause_p2=0u;
        hitdefs[hitdef_count].guard_flags=0u;
        hitdefs[hitdef_count].flags=IK_CNS_HITDEF_FALL;
        hitdefs[hitdef_count].hit_flags=IK_CNS_HIT_DEFAULT;
        hitdefs[hitdef_count].air_juggle=0u;

        ik_cns_asset_t asset=k_cns;
        asset.states=states;
        asset.state_count=(uint16_t)(state_count+1);
        asset.hitdefs=hitdefs;
        asset.hitdef_count=(uint16_t)(hitdef_count+1);
        asset.constants.air_juggle=15;

        ik_fight_init(&g,&asset); place(&g,100,145);
        ik_fight_controls_t p1{}; request(&p1,902);
        ik_fight_controls_t p2{};
        for(int i=0;i<8 && g.hits_p1==0u;++i) {
            tick2(&g,&p1,&p2);
            p1.has_state_request=0u;
        }
        EQ(g.hits_p1,1u);
        EQ(g.fighters[1].gethit_fall,1u);
        EQ(g.fighters[1].juggle_points,10);

        /* Exactly enough points permits one more juggle and consumes them. */
        g.fighters[1].state=5050;
        g.fighters[1].anim=5050;
        g.fighters[1].on_ground=0;
        g.fighters[1].gethit_fall=1u;
        g.fighters[1].juggle_points=5;
        g.fighters[1].x=145;
        g.fighters[1].x_q8=145*256;
        g.fighters[1].y=150;
        g.fighters[1].y_q8=150*256;
        g.fighters[0].x=100;
        g.fighters[0].x_q8=100*256;
        p1={}; request(&p1,902); p2={};
        for(int i=0;i<8 && g.hits_p1<2u;++i) {
            tick2(&g,&p1,&p2);
            p1.has_state_request=0u;
        }
        EQ(g.hits_p1,2u);
        EQ(g.fighters[1].juggle_points,0);

        /* No remaining budget: the same persistent-F hitflag attack misses. */
        g.fighters[1].state=5050;
        g.fighters[1].anim=5050;
        g.fighters[1].on_ground=0;
        g.fighters[1].gethit_fall=1u;
        g.fighters[1].x=145;
        g.fighters[1].x_q8=145*256;
        g.fighters[1].y=150;
        g.fighters[1].y_q8=150*256;
        g.fighters[0].x=100;
        g.fighters[0].x_q8=100*256;
        p1={}; request(&p1,902); p2={};
        for(int i=0;i<8;++i) {
            tick2(&g,&p1,&p2);
            p1.has_state_request=0u;
        }
        EQ(g.hits_p1,2u);
        EQ(g.fighters[1].juggle_points,0);
    }

    /* Conditional HitDefs are activated on the exact trigger tick and
     * persist. Moving P2 across the p2bodydist threshold after activation
     * must not replace the selected definition. StateDef poweradd is also
     * applied exactly once when the Palm state is entered. */
    {
        constexpr unsigned state_count=
            (unsigned)(sizeof(k_states)/sizeof(k_states[0]));
        constexpr unsigned hitdef_count=
            (unsigned)(sizeof(k_hitdefs)/sizeof(k_hitdefs[0]));
        ik_cns_state_t states[state_count+1];
        ik_cns_hitdef_t hitdefs[hitdef_count+2];
        for(unsigned i=0;i<state_count;++i) states[i]=k_states[i];
        for(unsigned i=0;i<hitdef_count;++i) hitdefs[i]=k_hitdefs[i];

        states[state_count]=k_states[0];
        states[state_count].number=903;
        states[state_count].anim=903;
        states[state_count].power_add=55;
        states[state_count].hitdef_ofs=(uint16_t)hitdef_count;
        states[state_count].hitdef_count=2u;
        states[state_count].controller_count=0u;

        hitdefs[hitdef_count]=k_hitdefs[0];
        hitdefs[hitdef_count].state_number=903;
        hitdefs[hitdef_count].trigger_kind=IK_CNS_TRIGGER_ANIM_ELEM_EQ;
        hitdefs[hitdef_count].trigger_value=3;
        hitdefs[hitdef_count].damage=90;
        hitdefs[hitdef_count].guard_damage=0;
        hitdefs[hitdef_count].pause_p1=0u;
        hitdefs[hitdef_count].pause_p2=0u;
        hitdefs[hitdef_count].guard_flags=0u;
        hitdefs[hitdef_count].p2_body_dist_op=IK_CNS_P2_DIST_LT;
        hitdefs[hitdef_count].p2_body_dist_x=40;

        hitdefs[hitdef_count+1]=hitdefs[hitdef_count];
        hitdefs[hitdef_count+1].damage=85;
        hitdefs[hitdef_count+1].p2_body_dist_op=IK_CNS_P2_DIST_GE;

        ik_cns_asset_t asset=k_cns;
        asset.states=states;
        asset.state_count=(uint16_t)(state_count+1);
        asset.hitdefs=hitdefs;
        asset.hitdef_count=(uint16_t)(hitdef_count+2);

        ik_fight_init(&g,&asset);
        place(&g,100,145); /* body distance 13 => near */
        ik_fight_controls_t p1{}; request(&p1,903);
        ik_fight_controls_t p2{};

        tick2(&g,&p1,&p2);
        p1.has_state_request=0u;
        EQ(g.fighters[0].power,55);
        tick2(&g,&p1,&p2);
        tick2(&g,&p1,&p2); /* element 3 activates near HitDef */
        EQ(g.fighters[0].active_hitdef_local,0);
        EQ(g.hits_p1,0u);

        g.fighters[1].x=180;
        g.fighters[1].x_q8=180*256; /* body distance now >=40 */
        tick2(&g,&p1,&p2); /* element 4 gets Clsn1 */
        EQ(g.hits_p1,1u);
        EQ(g.fighters[1].hp,910);
        EQ(g.fighters[0].active_hitdef_local,0);
        EQ(g.fighters[0].power,55);

        ik_fight_init(&g,&asset);
        place(&g,100,180); /* body distance 48 => far */
        p1={}; request(&p1,903); p2={};
        tick2(&g,&p1,&p2);
        p1.has_state_request=0u;
        tick2(&g,&p1,&p2);
        tick2(&g,&p1,&p2);
        EQ(g.fighters[0].active_hitdef_local,1);
        tick2(&g,&p1,&p2);
        EQ(g.hits_p1,1u);
        EQ(g.fighters[1].hp,915);
    }

    /* Default hitflag=MAF must not hit a liedown opponent. D is what
     * opts an attack into OTG/downed hits. */
    {
        ik_fight_init(&g,&k_cns); place(&g,100,145);
        ik_fighter_t* v=&g.fighters[1];
        v->state=5110; v->anim=5110; v->on_ground=1; v->ctrl=0;
        const int hp=v->hp;

        ik_fight_controls_t p1{}; request(&p1,IK_STATE_PUNCH);
        ik_fight_controls_t p2{};
        for(int i=0;i<12;++i) {
            tick2(&g,&p1,&p2);
            p1.has_state_request=0u;
        }
        EQ(g.hits_p1,0u);
        EQ(v->hp,hp);
        EQ(v->state,5110);
    }

    /* A is not enough for an opponent already in the fall graph; F is
     * the explicit juggle permission. Build one host-only HitDef on top of
     * the real fixture so the only changing input is hitflag. */
    {
        constexpr unsigned state_count=
            (unsigned)(sizeof(k_states)/sizeof(k_states[0]));
        constexpr unsigned hitdef_count=
            (unsigned)(sizeof(k_hitdefs)/sizeof(k_hitdefs[0]));
        ik_cns_state_t states[state_count+1];
        ik_cns_hitdef_t hitdefs[hitdef_count+1];
        for(unsigned i=0;i<state_count;++i) states[i]=k_states[i];
        for(unsigned i=0;i<hitdef_count;++i) hitdefs[i]=k_hitdefs[i];

        states[state_count]=k_states[0];
        states[state_count].number=900;
        states[state_count].anim=200;
        states[state_count].hitdef_ofs=(uint16_t)hitdef_count;
        states[state_count].hitdef_count=1u;
        states[state_count].controller_count=0u;

        hitdefs[hitdef_count]=k_hitdefs[0];
        hitdefs[hitdef_count].state_number=900;
        hitdefs[hitdef_count].trigger_kind=IK_CNS_TRIGGER_TIME_EQ;
        hitdefs[hitdef_count].trigger_value=0;
        hitdefs[hitdef_count].damage=1;
        hitdefs[hitdef_count].pause_p1=0u;
        hitdefs[hitdef_count].pause_p2=0u;
        hitdefs[hitdef_count].guard_flags=0u;

        ik_cns_asset_t asset=k_cns;
        asset.states=states;
        asset.state_count=(uint16_t)(state_count+1);
        asset.hitdefs=hitdefs;
        asset.hitdef_count=(uint16_t)(hitdef_count+1);

        hitdefs[hitdef_count].hit_flags=IK_CNS_HIT_AIR;
        ik_fight_init(&g,&asset); place(&g,100,145);
        g.fighters[1].state=5050;
        g.fighters[1].anim=5050;
        g.fighters[1].on_ground=0;
        g.fighters[1].gethit_fall=1u;
        g.fighters[1].y=150;
        g.fighters[1].y_q8=150*256;
        ik_fight_controls_t p1{}; request(&p1,900);
        ik_fight_controls_t p2{};
        for(int i=0;i<8;++i) {
            tick2(&g,&p1,&p2);
            p1.has_state_request=0u;
        }
        EQ(g.hits_p1,0u);

        hitdefs[hitdef_count].hit_flags=IK_CNS_HIT_FALL;
        ik_fight_init(&g,&asset); place(&g,100,145);
        g.fighters[1].state=5050;
        g.fighters[1].anim=5050;
        g.fighters[1].on_ground=0;
        g.fighters[1].gethit_fall=1u;
        g.fighters[1].y=150;
        g.fighters[1].y_q8=150*256;
        p1={}; request(&p1,900);
        p2={};
        for(int i=0;i<8 && g.hits_p1==0u;++i) {
            tick2(&g,&p1,&p2);
            p1.has_state_request=0u;
        }
        EQ(g.hits_p1,1u);
    }

    /* hitflag '+' requires a get-hit state; '-' rejects it. */
    {
        constexpr unsigned state_count=
            (unsigned)(sizeof(k_states)/sizeof(k_states[0]));
        constexpr unsigned hitdef_count=
            (unsigned)(sizeof(k_hitdefs)/sizeof(k_hitdefs[0]));
        ik_cns_state_t states[state_count+1];
        ik_cns_hitdef_t hitdefs[hitdef_count+1];
        for(unsigned i=0;i<state_count;++i) states[i]=k_states[i];
        for(unsigned i=0;i<hitdef_count;++i) hitdefs[i]=k_hitdefs[i];
        states[state_count]=k_states[0];
        states[state_count].number=901;
        states[state_count].anim=200;
        states[state_count].hitdef_ofs=(uint16_t)hitdef_count;
        states[state_count].hitdef_count=1u;
        states[state_count].controller_count=0u;
        hitdefs[hitdef_count]=k_hitdefs[0];
        hitdefs[hitdef_count].state_number=901;
        hitdefs[hitdef_count].trigger_kind=IK_CNS_TRIGGER_TIME_EQ;
        hitdefs[hitdef_count].trigger_value=0;
        hitdefs[hitdef_count].damage=1;
        hitdefs[hitdef_count].pause_p1=0u;
        hitdefs[hitdef_count].pause_p2=0u;
        hitdefs[hitdef_count].guard_flags=0u;
        ik_cns_asset_t asset=k_cns;
        asset.states=states;
        asset.state_count=(uint16_t)(state_count+1);
        asset.hitdefs=hitdefs;
        asset.hitdef_count=(uint16_t)(hitdef_count+1);

        hitdefs[hitdef_count].hit_flags=
            IK_CNS_HIT_STAND|IK_CNS_HIT_ONLY_GETHIT;
        ik_fight_init(&g,&asset); place(&g,100,145);
        ik_fight_controls_t p1{}; request(&p1,901);
        ik_fight_controls_t p2{};
        for(int i=0;i<8;++i) {
            tick2(&g,&p1,&p2);
            p1.has_state_request=0u;
        }
        EQ(g.hits_p1,0u);

        ik_fight_init(&g,&asset); place(&g,100,145);
        g.fighters[1].state=5001;
        g.fighters[1].anim=5001;
        g.fighters[1].on_ground=1;
        p1={}; request(&p1,901); p2={};
        for(int i=0;i<8 && g.hits_p1==0u;++i) {
            tick2(&g,&p1,&p2);
            p1.has_state_request=0u;
        }
        EQ(g.hits_p1,1u);

        hitdefs[hitdef_count].hit_flags=
            IK_CNS_HIT_STAND|IK_CNS_HIT_NOT_GETHIT;
        ik_fight_init(&g,&asset); place(&g,100,145);
        g.fighters[1].state=5001;
        g.fighters[1].anim=5001;
        g.fighters[1].on_ground=1;
        p1={}; request(&p1,901); p2={};
        for(int i=0;i<8;++i) {
            tick2(&g,&p1,&p2);
            p1.has_state_request=0u;
        }
        EQ(g.hits_p1,0u);
    }

    /* A liedown launch with down.bounce=0 still enters the falling
     * graph, but state 5100 receives fall.yVel=0 and goes straight back to
     * 5110 without the 5101 ground bounce. */
    {
        ik_fight_init(&g,&k_cns); place(&g,100,145);
        ik_fighter_t* v=&g.fighters[1];
        v->state=5110; v->anim=5110; v->on_ground=1; v->ctrl=0;

        ik_fight_controls_t p1{}; request(&p1,201);
        ik_fight_controls_t p2{};
        for(int i=0;i<12 && g.hits_p1==0u;++i) {
            tick2(&g,&p1,&p2);
            p1.has_state_request=0u;
        }
        EQ(g.hits_p1,1u);
        EQ(v->state,5080);
        EQ(v->gethit_vy_q8,-768);
        EQ(v->gethit_fall,1u);
        EQ(v->gethit_fall_y_q8,0);

        int saw_bounce=0;
        for(int i=0;i<180 && v->state!=5110;++i) {
            tick2(&g,&p1,&p2);
            if(v->state==5101) saw_bounce=1;
        }
        EQ(v->state,5110);
        EQ(saw_bounce,0);
    }

    /* The identical liedown launch with down.bounce=1 preserves
     * fall.x/yvelocity and therefore executes exactly one 5101 bounce. */
    {
        ik_fight_init(&g,&k_cns); place(&g,100,145);
        ik_fighter_t* v=&g.fighters[1];
        v->state=5110; v->anim=5110; v->on_ground=1; v->ctrl=0;

        ik_fight_controls_t p1{}; request(&p1,202);
        ik_fight_controls_t p2{};
        for(int i=0;i<12 && g.hits_p1==0u;++i) {
            tick2(&g,&p1,&p2);
            p1.has_state_request=0u;
        }
        EQ(g.hits_p1,1u);
        EQ(v->gethit_fall,1u);
        EQ(v->gethit_fall_y_q8,-1152);
        EQ(v->gethit_fall_x_q8,-256);
        EQ(v->gethit_fall_x_set,1u);

        int bounce_count=0;
        int was_bounce=0;
        for(int i=0;i<220 && (v->state!=5110 || bounce_count==0);++i) {
            tick2(&g,&p1,&p2);
            const int now=v->state==5101;
            if(now && !was_bounce) ++bounce_count;
            was_bounce=now;
        }
        EQ(bounce_count,1);
        EQ(v->state,5110);
    }

    /* A hit against a liedown fighter enters 5080. With zero Y hit
     * velocity the compiled branch selects 5081, then returns to 5110. */
    {
        ik_fight_init(&g,&k_downed_cns);
        place(&g,100,145);
        ik_fighter_t* v=&g.fighters[1];
        v->state=5110;
        v->anim=5110;
        v->on_ground=1;
        v->ctrl=0;

        ik_fight_controls_t p1{}; request(&p1,200);
        ik_fight_controls_t p2{};
        for(int i=0;i<20 && g.hits_p1==0u;++i) {
            tick2(&g,&p1,&p2);
            p1.has_state_request=0u;
        }
        EQ(g.hits_p1,1u);
        EQ(v->state,5080);
        EQ(v->gethit_vx_q8,-1280);
        EQ(v->gethit_vy_q8,0);
        EQ(v->hitstun,22u);
        EQ(v->hit_slide_time,22u);

        for(int i=0;i<20 && v->state!=5081;++i) {
            tick2(&g,&p1,&p2);
        }
        EQ(v->state,5081);

        for(int i=0;i<40 && v->state!=5110;++i) {
            tick2(&g,&p1,&p2);
        }
        EQ(v->state,5110);
    }

    /* A real fatal HitDef against a liedown fighter stays in the common
     * graph: apply_damage enters 5080, then 5081/5110/5150. It must not
     * jump to the synthetic IK_STATE_KO fallback. */
    {
        ik_fight_init(&g,&k_downed_cns);
        place(&g,100,145);
        ik_fighter_t* v=&g.fighters[1];
        v->state=5110;
        v->anim=5110;
        v->on_ground=1;
        v->ctrl=0;
        v->hp=20;

        ik_fight_controls_t p1{}; request(&p1,200);
        ik_fight_controls_t p2{};
        for(int i=0;i<20 && v->hp>0;++i) {
            tick2(&g,&p1,&p2);
            p1.has_state_request=0u;
        }

        EQ(v->hp,0);
        EQ(v->state,5080);
        EQ(g.round_over,0u);
        OK(v->state!=IK_STATE_KO);
        OK((g.events&IK_EVENT_KO)!=0u);

        for(int i=0;i<40 && !g.round_over;++i) {
            tick2(&g,&p1,&p2);
        }
        EQ(v->state,5150);
        EQ(g.round_over,1u);
        EQ(g.winner,1u);
    }

    /* A defeated fighter already lying down follows the common
     * !alive branch into 5150 before the round is finalized. */
    {
        ik_fight_init(&g,&k_downed_cns);
        ik_fighter_t* v=&g.fighters[1];
        v->state=5110;
        v->anim=5110;
        v->on_ground=1;
        v->ctrl=0;
        v->hp=0;

        ik_fight_controls_t p{};
        tick(&g,&p);
        EQ(v->state,5150);
        EQ(v->spr_priority,-3);
        EQ(g.round_over,1u);
        EQ(g.winner,1u);
    }

    /* KFM throw capture: state 800's throw HitDef binds P2, changes both
     * states, 810 drives target offsets, element 11 removes 78 life and
     * releases P2 into 821. 821 owns its .4 VelAdd so gravity is not doubled. */
    {
        ik_cns_hitdef_t throw_hit{};
        throw_hit.state_number=800;
        throw_hit.trigger_kind=IK_CNS_TRIGGER_TIME_EQ;
        throw_hit.trigger_value=0;
        throw_hit.priority=1u;
        throw_hit.flags=IK_CNS_HITDEF_FALL|IK_CNS_HITDEF_THROW;
        throw_hit.hit_flags=
            IK_CNS_HIT_STAND|IK_CNS_HIT_CROUCH|IK_CNS_HIT_NOT_GETHIT;
        throw_hit.priority_type=IK_CNS_PRIORITY_MISS;
        throw_hit.p1_state_no=810;
        throw_hit.p2_state_no=820;
        throw_hit.guard_dist=0;
        throw_hit.p1_facing=1;
        throw_hit.p2_facing=1;
        throw_hit.p1_spr_priority=1;
        throw_hit.fall_recover=1u;
        throw_hit.fall_recover_time=4u;
        throw_hit.fall_y_velocity_q8=-1152;

        ik_cns_controller_t ctrls[] = {
            {810,IK_CNS_CTRL_TARGET_BIND,IK_CNS_TRIGGER_ANIM_ELEM_RANGE,
             2,5,58*256,0,0u},
            {810,IK_CNS_CTRL_TURN,
             IK_CNS_TRIGGER_STATE_AXIS_FWD_ANIM_ELEM_EQ,
             6,0,0,0,0u},
            {810,IK_CNS_CTRL_POS_ADD,
             IK_CNS_TRIGGER_STATE_AXIS_FWD_ANIM_ELEM_EQ,
             6,0,-37*256,0,0u},
            {810,IK_CNS_CTRL_TARGET_FACING,
             IK_CNS_TRIGGER_STATE_AXIS_FWD_ANIM_ELEM_EQ,
             6,0,-1,0,0u},
            {810,IK_CNS_CTRL_TARGET_BIND,IK_CNS_TRIGGER_ANIM_ELEM_RANGE,
             6,7,41*256,-60*256,0u},
            {810,IK_CNS_CTRL_TARGET_BIND,IK_CNS_TRIGGER_ANIM_ELEM_RANGE,
             7,8,25*256,-75*256,0u},
            {810,IK_CNS_CTRL_TARGET_BIND,IK_CNS_TRIGGER_ANIM_ELEM_RANGE,
             8,9,15*256,-90*256,0u},
            {810,IK_CNS_CTRL_TARGET_BIND,IK_CNS_TRIGGER_ANIM_ELEM_RANGE,
             9,10,-5*256,-96*256,0u},
            {810,IK_CNS_CTRL_TARGET_BIND,IK_CNS_TRIGGER_ANIM_ELEM_RANGE,
             10,11,-14*256,-90*256,0u},
            {810,IK_CNS_CTRL_TARGET_BIND,IK_CNS_TRIGGER_ANIM_ELEM_EQ,
             11,0,-50*256,-50*256,0u},
            {810,IK_CNS_CTRL_TARGET_LIFE_ADD,IK_CNS_TRIGGER_ANIM_ELEM_EQ,
             11,0,-78,0,0u},
            {810,IK_CNS_CTRL_TARGET_STATE,IK_CNS_TRIGGER_ANIM_ELEM_EQ,
             11,0,821,0,0u},
            {810,IK_CNS_CTRL_TURN,IK_CNS_TRIGGER_ANIM_ELEM_EQ,
             12,0,0,0,0u},
            {810,IK_CNS_CTRL_POS_ADD,IK_CNS_TRIGGER_ANIM_ELEM_EQ,
             15,0,-10*256,0,0u},
            {810,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_ANIM_END,
             0,0,0,1,IK_CNS_CTRL_HAS_CTRL},

            {820,IK_CNS_CTRL_CHANGE_ANIM2,IK_CNS_TRIGGER_TIME_EQ,
             0,0,820,1,0u},
            {820,IK_CNS_CTRL_SELF_STATE,IK_CNS_TRIGGER_NOT_BOUND,
             0,0,5050,0,0u},

            {821,IK_CNS_CTRL_VEL_ADD,IK_CNS_TRIGGER_ALWAYS,
             0,0,0,102,IK_CNS_CTRL_AXIS_Y},
            {821,IK_CNS_CTRL_CHANGE_STATE,
             IK_CNS_TRIGGER_THROW_GROUND_RECOVERY,
             -20*256,0,5200,0,0u},
            {821,IK_CNS_CTRL_SELF_STATE,
             IK_CNS_TRIGGER_THROW_AIR_RECOVERY,
             0,0,5210,0,0u},
            {821,IK_CNS_CTRL_SELF_STATE,
             IK_CNS_TRIGGER_VY_GT_Q8_AT_FLOOR,
             0,0,5100,0,0u},
        };

        ik_cns_state_t states[5]{};
        states[0].number=800;
        states[0].anim=800;
        states[0].state_type=IK_CNS_STATE_STAND;
        states[0].move_type=IK_CNS_MOVE_ATTACK;
        states[0].physics=IK_CNS_PHYS_STAND;
        states[0].spr_priority=2;
        states[0].hitdef_count=1u;

        states[1].number=810;
        states[1].anim=810;
        states[1].state_type=IK_CNS_STATE_STAND;
        states[1].move_type=IK_CNS_MOVE_ATTACK;
        states[1].physics=IK_CNS_PHYS_NONE;
        states[1].controller_ofs=0u;
        states[1].controller_count=15u;

        states[2].number=820;
        states[2].anim=820;
        states[2].state_type=IK_CNS_STATE_AIR;
        states[2].move_type=IK_CNS_MOVE_HIT;
        states[2].physics=IK_CNS_PHYS_NONE;
        states[2].has_velset=1u;
        states[2].controller_ofs=15u;
        states[2].controller_count=2u;

        states[3].number=821;
        states[3].anim=821;
        states[3].state_type=IK_CNS_STATE_AIR;
        states[3].move_type=IK_CNS_MOVE_HIT;
        states[3].physics=IK_CNS_PHYS_NONE;
        states[3].has_velset=1u;
        states[3].velset_x_q8=717;
        states[3].velset_y_q8=-1792;
        states[3].controller_ofs=17u;
        states[3].controller_count=4u;
        states[3].owns_air_accel=1u;

        states[4].number=5100;
        states[4].anim=5100;
        states[4].state_type=IK_CNS_STATE_LIEDOWN;
        states[4].move_type=IK_CNS_MOVE_HIT;
        states[4].physics=IK_CNS_PHYS_NONE;

        ik_cns_asset_t asset{};
        asset.constants.life=1000;
        asset.constants.ground_back=15;
        asset.constants.ground_front=16;
        asset.constants.air_back=12;
        asset.constants.air_front=12;
        asset.constants.height=60;
        asset.constants.attack_dist=160;
        asset.constants.yaccel_q8=113;
        asset.constants.air_juggle=15;
        asset.states=states;
        asset.state_count=5u;
        asset.hitdefs=&throw_hit;
        asset.hitdef_count=1u;
        asset.controllers=ctrls;
        asset.controller_count=
            (uint16_t)(sizeof(ctrls)/sizeof(ctrls[0]));

        ik_fight_init(&g,&asset);
        place(&g,100,132);
        ik_fight_controls_t p1{};
        p1.forward=1;
        request(&p1,800);
        ik_fight_controls_t p2{};

        tick2(&g,&p1,&p2);
        EQ(g.fighters[0].state,810);
        EQ(g.fighters[1].state,820);
        EQ(g.fighters[0].target_index,1);
        EQ(g.fighters[1].bound_to,0);
        EQ(g.fighters[0].state_axis,1);
        EQ(g.fighters[0].spr_priority,1);
        EQ(g.hits_p1,1u);

        p1={};
        int saw_release=0;
        for(int i=0;i<14;++i) {
            tick2(&g,&p1,&p2);
            if(g.fighters[1].state==821) {
                saw_release=1;
                break;
            }
        }
        OK(saw_release);
        EQ(g.fighters[1].hp,922);
        EQ(g.fighters[1].bound_to,-1);
        EQ(g.fighters[0].target_index,-1);
        EQ(g.fighters[1].state,821);
        EQ(g.fighters[1].vx_q8,717);
        /* 821 runs one .4 VelAdd (102 Q8) before integrating; no +yaccel. */
        EQ(g.fighters[1].vy_q8,-1690);

        for(int i=0;i<100 && g.fighters[1].state!=5100;++i) {
            tick2(&g,&p1,&p2);
        }
        EQ(g.fighters[1].state,5100);
    }

    /* Fast Kung Fu Palm: spending 330 power, custom hit state 1025,
     * HitShakeOver -> 1026, wall contact -> 1027 freeze, then 1028 bounce
     * and finally the victim returns to the common 5100 ground state. */
    {
        ik_cns_hitdef_t hit{};
        hit.state_number=1020;
        hit.trigger_kind=IK_CNS_TRIGGER_ANIM_ELEM_EQ;
        hit.trigger_value=4;
        hit.damage=95;
        hit.guard_damage=5;
        hit.priority=4u;
        hit.pause_p1=0u;
        hit.pause_p2=2u;
        hit.ground_type=IK_CNS_GROUND_LOW;
        hit.ground_slide_time=20u;
        hit.ground_hit_time=22u;
        hit.air_hit_time=22u;
        hit.ground_velocity_x_q8=-8*256;
        hit.ground_velocity_y_q8=-7*256;
        hit.air_velocity_x_q8=-8*256;
        hit.air_velocity_y_q8=-7*256;
        hit.flags=IK_CNS_HITDEF_FALL;
        hit.hit_flags=IK_CNS_HIT_DEFAULT;
        hit.p2_state_no=1025;
        hit.p2_facing=1;

        ik_cns_controller_t ctrls[] = {
            {1025,IK_CNS_CTRL_CHANGE_ANIM2,IK_CNS_TRIGGER_ALWAYS,
             0,0,1025,1,0u},
            {1025,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_HIT_SHAKE_OVER,
             0,0,1026,0,0u},

            {1026,IK_CNS_CTRL_HIT_VEL_SET,IK_CNS_TRIGGER_TIME_EQ,
             1,0,0,0,IK_CNS_CTRL_AXIS_X|IK_CNS_CTRL_AXIS_Y},
            {1026,IK_CNS_CTRL_VEL_ADD,IK_CNS_TRIGGER_ALWAYS,
             0,0,0,115,IK_CNS_CTRL_AXIS_Y},
            {1026,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_AIR_NEAR_BODY_EDGE,
             -15*256,20,1027,0,0u},
            {1026,IK_CNS_CTRL_SELF_STATE,IK_CNS_TRIGGER_VY_GT_Q8_AT_FLOOR,
             0,0,5100,0,0u},

            {1027,IK_CNS_CTRL_TURN,
             IK_CNS_TRIGGER_STATE_ENTRY_FRONT_EDGE_BODY_LE,
             30,0,0,0,0u},
            {1027,IK_CNS_CTRL_POS_ADD_FROM_BACK_EDGE,IK_CNS_TRIGGER_TIME_EQ,
             1,0,15*256,0,0u},
            {1027,IK_CNS_CTRL_POS_FREEZE,IK_CNS_TRIGGER_ALWAYS,
             0,0,0,0,IK_CNS_CTRL_AXIS_X|IK_CNS_CTRL_AXIS_Y},
            {1027,IK_CNS_CTRL_CHANGE_ANIM2,IK_CNS_TRIGGER_TIME_EQ,
             1,0,1027,1,0u},
            {1027,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_ANIM_END,
             0,0,1028,0,0u},

            {1028,IK_CNS_CTRL_VEL_SET,IK_CNS_TRIGGER_TIME_EQ,
             1,0,0,-6*256,IK_CNS_CTRL_AXIS_Y},
            {1028,IK_CNS_CTRL_VEL_SET,IK_CNS_TRIGGER_TIME_EQ,
             1,0,410,0,IK_CNS_CTRL_AXIS_X|IK_CNS_CTRL_LOCAL_X},
            {1028,IK_CNS_CTRL_TURN,IK_CNS_TRIGGER_STATE_ENTRY_BACK_EDGE_LT,
             30,0,0,0,0u},
            {1028,IK_CNS_CTRL_VEL_ADD,IK_CNS_TRIGGER_ALWAYS,
             0,0,0,90,IK_CNS_CTRL_AXIS_Y},
            {1028,IK_CNS_CTRL_SELF_STATE,IK_CNS_TRIGGER_VY_GT_Q8_AT_FLOOR,
             0,0,5100,0,0u},
        };

        ik_cns_state_t states[6]{};
        states[0].number=1020;
        states[0].anim=1020;
        states[0].power_add=-330;
        states[0].state_type=IK_CNS_STATE_STAND;
        states[0].move_type=IK_CNS_MOVE_ATTACK;
        states[0].physics=IK_CNS_PHYS_NONE;
        states[0].hitdef_count=1u;

        states[1].number=1025;
        states[1].anim=-1;
        states[1].state_type=IK_CNS_STATE_AIR;
        states[1].move_type=IK_CNS_MOVE_HIT;
        states[1].physics=IK_CNS_PHYS_NONE;
        states[1].has_velset=1u;
        states[1].controller_ofs=0u;
        states[1].controller_count=2u;

        states[2].number=1026;
        states[2].anim=-1;
        states[2].state_type=IK_CNS_STATE_AIR;
        states[2].move_type=IK_CNS_MOVE_HIT;
        states[2].physics=IK_CNS_PHYS_NONE;
        states[2].controller_ofs=2u;
        states[2].controller_count=4u;
        states[2].owns_air_accel=1u;

        states[3].number=1027;
        states[3].anim=-1;
        states[3].state_type=IK_CNS_STATE_AIR;
        states[3].move_type=IK_CNS_MOVE_HIT;
        states[3].physics=IK_CNS_PHYS_NONE;
        states[3].controller_ofs=6u;
        states[3].controller_count=5u;

        states[4].number=1028;
        states[4].anim=-1;
        states[4].state_type=IK_CNS_STATE_AIR;
        states[4].move_type=IK_CNS_MOVE_HIT;
        states[4].physics=IK_CNS_PHYS_NONE;
        states[4].controller_ofs=11u;
        states[4].controller_count=5u;
        states[4].owns_air_accel=1u;

        states[5].number=5100;
        states[5].anim=5100;
        states[5].state_type=IK_CNS_STATE_LIEDOWN;
        states[5].move_type=IK_CNS_MOVE_HIT;
        states[5].physics=IK_CNS_PHYS_NONE;

        ik_cns_asset_t asset{};
        asset.constants.life=1000;
        asset.constants.ground_back=15;
        asset.constants.ground_front=16;
        asset.constants.air_back=12;
        asset.constants.air_front=12;
        asset.constants.height=60;
        asset.constants.attack_dist=160;
        asset.constants.yaccel_q8=113;
        asset.constants.air_juggle=15;
        asset.states=states;
        asset.state_count=6u;
        asset.hitdefs=&hit;
        asset.hitdef_count=1u;
        asset.controllers=ctrls;
        asset.controller_count=
            (uint16_t)(sizeof(ctrls)/sizeof(ctrls[0]));

        ik_fight_init(&g,&asset);
        place(&g,230,270);
        g.fighters[0].power=330;

        ik_fight_controls_t p1{}; request(&p1,1020);
        ik_fight_controls_t p2{};
        tick2(&g,&p1,&p2);
        p1.has_state_request=0u;
        EQ(g.fighters[0].state,1020);
        EQ(g.fighters[0].power,0);

        for(int i=0;i<8 && g.hits_p1==0u;++i) {
            tick2(&g,&p1,&p2);
        }
        EQ(g.hits_p1,1u);
        EQ(g.fighters[1].hp,905);
        EQ(g.fighters[1].state,1025);
        EQ(g.fighters[1].facing,-1);

        for(int i=0;i<12 && g.fighters[1].state!=1026;++i) {
            tick2(&g,&p1,&p2);
        }
        EQ(g.fighters[1].state,1026);

        int saw_wall=0;
        for(int i=0;i<20 && !saw_wall;++i) {
            tick2(&g,&p1,&p2);
            saw_wall=(g.fighters[1].state==1027);
        }
        OK(saw_wall);
        OK(g.fighters[1].y < IK_FLOOR_Y-15);

        const int16_t wall_x=g.fighters[1].x;
        tick2(&g,&p1,&p2);
        EQ(g.fighters[1].state,1027);
        OK(g.fighters[1].pos_freeze_x);
        OK(g.fighters[1].pos_freeze_y);
        OK(g.fighters[1].x<=wall_x);

        for(int i=0;i<10 && g.fighters[1].state!=1028;++i) {
            tick2(&g,&p1,&p2);
        }
        EQ(g.fighters[1].state,1028);

        tick2(&g,&p1,&p2);
        EQ(g.fighters[1].facing,1);
        EQ(g.fighters[1].vx_q8,-410);
        EQ(g.fighters[1].vy_q8,-1446);

        for(int i=0;i<100 && g.fighters[1].state!=5100;++i) {
            tick2(&g,&p1,&p2);
        }
        EQ(g.fighters[1].state,5100);
    }

    /* hitdefpersist keeps both the active definition and its hit mask:
     * changing into an aerial continuation must not create a free second hit. */
    {
        ik_cns_hitdef_t hit{};
        hit.state_number=910;
        hit.trigger_kind=IK_CNS_TRIGGER_TIME_EQ;
        hit.trigger_value=0;
        hit.damage=20;
        hit.priority=4u;
        hit.hit_flags=IK_CNS_HIT_DEFAULT;

        ik_cns_controller_t ctrls[] = {
            {910,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_TIME_EQ,
             1,0,911,0,0u},
        };

        ik_cns_state_t states[2]{};
        states[0].number=910;
        states[0].anim=910;
        states[0].state_type=IK_CNS_STATE_STAND;
        states[0].move_type=IK_CNS_MOVE_ATTACK;
        states[0].physics=IK_CNS_PHYS_NONE;
        states[0].hitdef_count=1u;
        states[0].controller_count=1u;

        states[1].number=911;
        states[1].anim=911;
        states[1].state_type=IK_CNS_STATE_AIR;
        states[1].move_type=IK_CNS_MOVE_ATTACK;
        states[1].physics=IK_CNS_PHYS_NONE;
        states[1].hitdef_persist=1u;

        ik_cns_asset_t asset{};
        asset.constants.life=1000;
        asset.constants.ground_back=15;
        asset.constants.ground_front=16;
        asset.constants.air_back=12;
        asset.constants.air_front=12;
        asset.constants.height=60;
        asset.constants.yaccel_q8=113;
        asset.states=states;
        asset.state_count=2u;
        asset.hitdefs=&hit;
        asset.hitdef_count=1u;
        asset.controllers=ctrls;
        asset.controller_count=1u;

        ik_fight_init(&g,&asset); place(&g,100,145);
        ik_fight_controls_t p1{}; request(&p1,910);
        ik_fight_controls_t p2{};
        tick2(&g,&p1,&p2);
        p1.has_state_request=0u;
        EQ(g.hits_p1,1u);
        EQ(g.fighters[1].hp,980);
        EQ(g.fighters[0].active_hitdef_global,0);

        tick2(&g,&p1,&p2);
        EQ(g.fighters[0].state,911);
        EQ(g.fighters[0].active_hitdef_global,0);
        EQ(g.fighters[0].hitdef_hit_mask,1u);

        for(int i=0;i<4;++i) tick2(&g,&p1,&p2);
        EQ(g.hits_p1,1u);
        EQ(g.fighters[1].hp,980);
    }

    /* The shared Knee kick reads prevstateno: 1051 -> 35 damage,
     * 1061 -> 40 damage, matching KFM's CNS expression. */
    {
        ik_cns_hitdef_t hit{};
        hit.state_number=915;
        hit.trigger_kind=IK_CNS_TRIGGER_TIME_EQ;
        hit.trigger_value=0;
        hit.damage=35;
        hit.alt_damage=40;
        hit.alt_damage_prev_state=1061;
        hit.priority=4u;
        hit.hit_flags=IK_CNS_HIT_DEFAULT;

        ik_cns_state_t states[3]{};
        states[0].number=1051;
        states[0].anim=911;
        states[0].state_type=IK_CNS_STATE_AIR;
        states[0].move_type=IK_CNS_MOVE_ATTACK;
        states[0].physics=IK_CNS_PHYS_NONE;
        states[1]=states[0];
        states[1].number=1061;
        states[2].number=915;
        states[2].anim=915;
        states[2].state_type=IK_CNS_STATE_AIR;
        states[2].move_type=IK_CNS_MOVE_ATTACK;
        states[2].physics=IK_CNS_PHYS_NONE;
        states[2].hitdef_count=1u;

        ik_cns_asset_t asset{};
        asset.constants.life=1000;
        asset.constants.ground_back=15;
        asset.constants.ground_front=16;
        asset.constants.air_back=12;
        asset.constants.air_front=12;
        asset.constants.height=60;
        asset.constants.yaccel_q8=113;
        asset.states=states;
        asset.state_count=3u;
        asset.hitdefs=&hit;
        asset.hitdef_count=1u;

        ik_fight_init(&g,&asset); place(&g,100,145);
        g.fighters[0].state=1051;
        g.fighters[0].anim=911;
        g.fighters[0].on_ground=0;
        p1={}; request(&p1,915); p2={};
        tick2(&g,&p1,&p2);
        EQ(g.hits_p1,1u);
        EQ(g.fighters[1].hp,965);

        ik_fight_init(&g,&asset); place(&g,100,145);
        g.fighters[0].state=1061;
        g.fighters[0].anim=911;
        g.fighters[0].on_ground=0;
        p1={}; request(&p1,915); p2={};
        tick2(&g,&p1,&p2);
        EQ(g.hits_p1,1u);
        EQ(g.fighters[1].hp,960);
        EQ(g.fighters[0].prev_state,1061);
    }

    /* Fast Upper reuses one HitDef controller: Time=0 hits once, then
     * trigger2=AnimElem 4 rearms that same local HitDef for exactly one
     * additional contact. It must not become active every subsequent tick. */
    {
        constexpr unsigned state_count=
            (unsigned)(sizeof(k_states)/sizeof(k_states[0]));
        ik_cns_state_t states[state_count+1];
        for(unsigned i=0;i<state_count;++i) states[i]=k_states[i];

        states[state_count]=k_states[0];
        states[state_count].number=920;
        states[state_count].anim=920;
        states[state_count].power_add=-330;
        states[state_count].hitdef_ofs=0u;
        states[state_count].hitdef_count=1u;
        states[state_count].playsnd_count=0u;
        states[state_count].controller_count=0u;

        ik_cns_hitdef_t hit{};
        hit.state_number=920;
        hit.trigger_kind=IK_CNS_TRIGGER_TIME_EQ;
        hit.trigger_value=0;
        hit.trigger2_kind=IK_CNS_TRIGGER_ANIM_ELEM_EQ;
        hit.trigger2_value=4;
        hit.has_trigger2=1u;
        hit.damage=30;
        hit.priority=5u;
        hit.ground_type=IK_CNS_GROUND_LOW;
        hit.ground_hit_time=1u;
        hit.air_hit_time=1u;
        hit.hit_flags=IK_CNS_HIT_DEFAULT;
        hit.guard_kill=1u;
        hit.p1_state_no=-1;
        hit.p2_state_no=-1;
        hit.guard_dist=-1;
        hit.p1_spr_priority=-128;
        hit.fall_recover=1u;

        ik_cns_asset_t asset=k_cns;
        asset.states=states;
        asset.state_count=(uint16_t)(state_count+1);
        asset.hitdefs=&hit;
        asset.hitdef_count=1u;

        ik_fight_init(&g,&asset);
        place(&g,100,145);
        g.fighters[0].power=330;
        ik_fight_controls_t p1{}; request(&p1,920);
        ik_fight_controls_t p2{};

        tick2(&g,&p1,&p2);
        p1.has_state_request=0u;
        EQ(g.fighters[0].power,0);
        EQ(g.hits_p1,1u);
        EQ(g.fighters[1].hp,970);

        for(int i=0;i<8;++i) tick2(&g,&p1,&p2);
        EQ(g.hits_p1,2u);
        EQ(g.fighters[1].hp,940);
        EQ(g.fighters[0].hitdef_hit_mask,1u);
    }

    /* forcestand changes only the ground get-hit branch: the same crouching
     * victim normally enters 5010, while the Upper version enters 5000. */
    {
        constexpr unsigned state_count=
            (unsigned)(sizeof(k_states)/sizeof(k_states[0]));
        ik_cns_state_t states[state_count+1];
        for(unsigned i=0;i<state_count;++i) states[i]=k_states[i];
        states[state_count]=k_states[0];
        states[state_count].number=921;
        states[state_count].anim=921;
        states[state_count].hitdef_ofs=0u;
        states[state_count].hitdef_count=1u;
        states[state_count].playsnd_count=0u;
        states[state_count].controller_count=0u;

        ik_cns_hitdef_t hit{};
        hit.state_number=921;
        hit.trigger_kind=IK_CNS_TRIGGER_TIME_EQ;
        hit.trigger_value=0;
        hit.damage=1;
        hit.priority=5u;
        hit.ground_type=IK_CNS_GROUND_LOW;
        hit.ground_hit_time=10u;
        hit.air_hit_time=10u;
        hit.hit_flags=IK_CNS_HIT_DEFAULT;
        hit.guard_kill=1u;
        hit.p1_state_no=-1;
        hit.p2_state_no=-1;
        hit.guard_dist=-1;
        hit.p1_spr_priority=-128;
        hit.fall_recover=1u;

        ik_cns_asset_t asset=k_cns;
        asset.states=states;
        asset.state_count=(uint16_t)(state_count+1);
        asset.hitdefs=&hit;
        asset.hitdef_count=1u;

        ik_fight_init(&g,&asset); place(&g,100,145);
        g.fighters[1].state=11;
        g.fighters[1].anim=11;
        g.fighters[1].on_ground=1;
        g.fighters[1].ctrl=0;
        ik_fight_controls_t p1{}; request(&p1,921);
        ik_fight_controls_t p2{};
        tick2(&g,&p1,&p2);
        EQ(g.fighters[1].state,5010);

        hit.flags=IK_CNS_HITDEF_FORCE_STAND;
        ik_fight_init(&g,&asset); place(&g,100,145);
        g.fighters[1].state=11;
        g.fighters[1].anim=11;
        g.fighters[1].on_ground=1;
        g.fighters[1].ctrl=0;
        p1={}; request(&p1,921); p2={};
        tick2(&g,&p1,&p2);
        EQ(g.fighters[1].state,5000);
    }

    /* Upper yaccel=.4 follows the victim into the compiled air get-hit graph
     * and overrides the character's default .44 gravity for that hit. */
    {
        constexpr unsigned state_count=
            (unsigned)(sizeof(k_states)/sizeof(k_states[0]));
        ik_cns_state_t states[state_count+1];
        for(unsigned i=0;i<state_count;++i) states[i]=k_states[i];
        states[state_count]=k_states[0];
        states[state_count].number=922;
        states[state_count].anim=922;
        states[state_count].hitdef_ofs=0u;
        states[state_count].hitdef_count=1u;
        states[state_count].playsnd_count=0u;
        states[state_count].controller_count=0u;

        ik_cns_hitdef_t hit{};
        hit.state_number=922;
        hit.trigger_kind=IK_CNS_TRIGGER_TIME_EQ;
        hit.trigger_value=0;
        hit.damage=1;
        hit.priority=5u;
        hit.ground_type=IK_CNS_GROUND_LOW;
        hit.ground_hit_time=20u;
        hit.air_hit_time=20u;
        hit.ground_velocity_x_q8=-256;
        hit.ground_velocity_y_q8=-4*256;
        hit.air_velocity_x_q8=-256;
        hit.air_velocity_y_q8=-4*256;
        hit.flags=IK_CNS_HITDEF_FALL;
        hit.hit_flags=IK_CNS_HIT_DEFAULT;
        hit.guard_kill=1u;
        hit.p1_state_no=-1;
        hit.p2_state_no=-1;
        hit.guard_dist=-1;
        hit.p1_spr_priority=-128;
        hit.fall_recover=1u;
        hit.yaccel_q8=102;

        ik_cns_asset_t asset=k_cns;
        asset.states=states;
        asset.state_count=(uint16_t)(state_count+1);
        asset.hitdefs=&hit;
        asset.hitdef_count=1u;

        ik_fight_init(&g,&asset); place(&g,100,145);
        ik_fight_controls_t p1{}; request(&p1,922);
        ik_fight_controls_t p2{};
        tick2(&g,&p1,&p2);
        EQ(g.hits_p1,1u);
        EQ(g.fighters[1].gethit_yaccel_q8,102);

        ik_fighter_t* v=&g.fighters[1];
        v->state=5030;
        v->anim=5030;
        v->state_time=0u;
        v->anim_time=0u;
        v->on_ground=0;
        v->y=100;
        v->y_q8=100*256;
        v->vy_q8=0;
        v->hitstun=10u;
        p1={}; p2={};
        tick2(&g,&p1,&p2);
        EQ(v->vy_q8,-1024+102);
    }

    /* ground.cornerpush.veloff applies only when the grounded victim's
     * body reaches a stage edge. The value is local to P1's facing. */
    {
        constexpr unsigned state_count=
            (unsigned)(sizeof(k_states)/sizeof(k_states[0]));
        ik_cns_state_t states[state_count+1];
        for(unsigned i=0;i<state_count;++i) states[i]=k_states[i];
        states[state_count]=k_states[0];
        states[state_count].number=923;
        states[state_count].anim=923;
        states[state_count].hitdef_ofs=0u;
        states[state_count].hitdef_count=1u;
        states[state_count].playsnd_count=0u;
        states[state_count].controller_count=0u;

        ik_cns_hitdef_t hit{};
        hit.state_number=923;
        hit.trigger_kind=IK_CNS_TRIGGER_TIME_EQ;
        hit.trigger_value=0;
        hit.damage=1;
        hit.priority=5u;
        hit.ground_type=IK_CNS_GROUND_LOW;
        hit.ground_hit_time=10u;
        hit.air_hit_time=10u;
        hit.hit_flags=IK_CNS_HIT_DEFAULT;
        hit.guard_kill=1u;
        hit.p1_state_no=-1;
        hit.p2_state_no=-1;
        hit.guard_dist=-1;
        hit.p1_spr_priority=-128;
        hit.fall_recover=1u;
        hit.ground_cornerpush_veloff_q8=-12*256;

        ik_cns_asset_t asset=k_cns;
        asset.states=states;
        asset.state_count=(uint16_t)(state_count+1);
        asset.hitdefs=&hit;
        asset.hitdef_count=1u;

        ik_fight_init(&g,&asset); place(&g,100,145);
        ik_fight_controls_t p1{}; request(&p1,923);
        ik_fight_controls_t p2{};
        tick2(&g,&p1,&p2);
        EQ(g.hits_p1,1u);
        EQ(g.fighters[0].vx_q8,0);

        ik_fight_init(&g,&asset); place(&g,240,281);
        p1={}; request(&p1,923); p2={};
        tick2(&g,&p1,&p2);
        EQ(g.hits_p1,1u);
        EQ(g.fighters[0].facing,1);
        EQ(g.fighters[0].vx_q8,-12*256);
    }

    /* Recovery command uses the compiled common thresholds. Near the
     * ground it enters 5200 and subsequently reaches ground recovery 5201. */
    {
        ik_fight_init(&g,&k_recovery_cns);
        ik_fighter_t* f=&g.fighters[0];
        f->state=5050;
        f->anim=5050;
        f->on_ground=0;
        f->ctrl=0;
        f->y=165;
        f->y_q8=165*256;
        f->vy_q8=100;
        f->gethit_fall=1u;
        f->gethit_fall_recover=1u;
        f->gethit_fall_recover_time=4u;
        f->fall_time=4u;

        ik_fight_controls_t p{}; p.recovery=1;
        tick(&g,&p);
        EQ(f->state,5200);

        p={};
        for(int i=0;i<30 && f->state!=5201;++i) tick(&g,&p);
        EQ(f->state,5201);
    }

    /* Mid-air recovery enters 5210. Physics=N stays frozen for four frames,
     * then compiled recovery multipliers/additions and directional input run. */
    {
        ik_fight_init(&g,&k_recovery_cns);
        ik_fighter_t* f=&g.fighters[0];
        f->state=5050;
        f->anim=5050;
        f->on_ground=0;
        f->ctrl=0;
        f->y=140;
        f->y_q8=140*256;
        f->vy_q8=0;
        f->gethit_fall=1u;
        f->gethit_fall_recover=1u;
        f->gethit_fall_recover_time=4u;
        f->fall_time=4u;

        ik_fight_controls_t p{}; p.recovery=1;
        tick(&g,&p);
        EQ(f->state,5210);
        const int32_t x0=f->x_q8;
        const int32_t y0=f->y_q8;

        p={};
        tick(&g,&p);
        tick(&g,&p);
        tick(&g,&p);
        EQ(f->x_q8,x0);
        EQ(f->y_q8,y0);

        p={}; p.up=1;
        tick(&g,&p);
        EQ(f->vy_q8,-1574);
        OK(f->y_q8<y0);

        f->y=IK_FLOOR_Y;
        f->y_q8=IK_FLOOR_Y*256;
        f->vy_q8=100;
        p={};
        tick(&g,&p);
        EQ(f->state,52);
        EQ(f->ctrl,1);
    }

    /* Pause freezes fight simulation for the authored duration and rewinds
     * the internal Time=0 bookkeeping tick so Time=1 controllers run after
     * the pause instead of being skipped. */
    {
        const ik_cns_controller_t ctrls[] = {
            {930,IK_CNS_CTRL_PAUSE,IK_CNS_TRIGGER_TIME_EQ,
             1,0,3,0,0u},
            {930,IK_CNS_CTRL_CTRL_SET,IK_CNS_TRIGGER_TIME_EQ,
             1,0,1,0,0u},
        };
        ik_cns_state_t state{};
        state.number=930;
        state.anim=0;
        state.state_type=IK_CNS_STATE_STAND;
        state.move_type=IK_CNS_MOVE_IDLE;
        state.physics=IK_CNS_PHYS_STAND;
        state.controller_count=2u;

        ik_cns_asset_t asset{};
        asset.constants.life=1000;
        asset.constants.ground_back=15;
        asset.constants.ground_front=16;
        asset.constants.air_back=12;
        asset.constants.air_front=12;
        asset.constants.height=60;
        asset.states=&state;
        asset.state_count=1u;
        asset.controllers=ctrls;
        asset.controller_count=2u;

        ik_fight_init(&g,&asset);
        g.fighters[0].state=930;
        g.fighters[0].anim=0;
        g.fighters[0].ctrl=0;
        ik_fight_controls_t p1{};
        ik_fight_controls_t p2{};

        tick2(&g,&p1,&p2);
        EQ(g.pause_time,3u);
        EQ(g.fighters[0].state_time,0u);
        EQ(g.fighters[0].ctrl,0);

        const uint32_t frozen_frame=g.frame;
        tick2(&g,&p1,&p2);
        tick2(&g,&p1,&p2);
        tick2(&g,&p1,&p2);
        EQ(g.pause_time,0u);
        EQ(g.fighters[0].state_time,0u);
        EQ(g.frame,frozen_frame+3u);

        tick2(&g,&p1,&p2);
        EQ(g.fighters[0].state_time,1u);
        EQ(g.fighters[0].ctrl,1);
    }

    /* Pause.movetime advances only the owner while the opponent and round
     * timer remain frozen. Once movetime is exhausted the owner freezes too. */
    {
        const ik_cns_controller_t ctrls[] = {
            {936,IK_CNS_CTRL_PAUSE,IK_CNS_TRIGGER_TIME_EQ,
             1,0,3,2,0u},
        };
        ik_cns_state_t states[2]{};
        states[0].number=936;
        states[0].anim=0;
        states[0].state_type=IK_CNS_STATE_STAND;
        states[0].move_type=IK_CNS_MOVE_IDLE;
        states[0].physics=IK_CNS_PHYS_NONE;
        states[0].controller_count=1u;

        states[1].number=0;
        states[1].anim=0;
        states[1].state_type=IK_CNS_STATE_STAND;
        states[1].move_type=IK_CNS_MOVE_IDLE;
        states[1].physics=IK_CNS_PHYS_NONE;
        states[1].ctrl=1;

        ik_cns_asset_t asset{};
        asset.constants.life=1000;
        asset.constants.ground_back=15;
        asset.constants.ground_front=16;
        asset.constants.air_back=12;
        asset.constants.air_front=12;
        asset.constants.height=60;
        asset.states=states;
        asset.state_count=2u;
        asset.controllers=ctrls;
        asset.controller_count=1u;

        ik_fight_init(&g,&asset);
        g.fighters[0].state=936;
        g.fighters[0].anim=0;
        g.fighters[0].ctrl=0;
        g.fighters[1].state=0;
        g.fighters[1].anim=0;
        const uint32_t timer=g.timer_frames;

        ik_fight_controls_t p1{};
        ik_fight_controls_t p2{};
        tick2(&g,&p1,&p2);
        EQ(g.pause_time,3u);
        EQ(g.pause_move_time,2u);
        EQ(g.fighters[0].state_time,0u);
        const uint16_t foe_time=g.fighters[1].state_time;
        EQ(g.timer_frames,timer-1u);

        tick2(&g,&p1,&p2);
        EQ(g.pause_time,2u);
        EQ(g.pause_move_time,1u);
        EQ(g.fighters[0].state_time,1u);
        EQ(g.fighters[1].state_time,foe_time);
        EQ(g.timer_frames,timer-1u);

        tick2(&g,&p1,&p2);
        EQ(g.pause_time,1u);
        EQ(g.pause_move_time,0u);
        EQ(g.fighters[0].state_time,2u);
        EQ(g.fighters[1].state_time,foe_time);

        tick2(&g,&p1,&p2);
        EQ(g.pause_time,0u);
        EQ(g.fighters[0].state_time,2u);
        EQ(g.fighters[1].state_time,foe_time);
    }

    /* A HitDef that becomes active only after Pause starts can still hit
     * the frozen opponent while the Pause owner has movetime remaining. */
    {
        ik_cns_hitdef_t hit{};
        hit.state_number=937;
        hit.trigger_kind=IK_CNS_TRIGGER_TIME_EQ;
        hit.trigger_value=1;
        hit.damage=10;
        hit.priority=4u;
        hit.hit_flags=IK_CNS_HIT_DEFAULT;
        hit.attack_attr_mask=IK_CNS_ATTR_SPECIAL_ATTACK;

        const ik_cns_controller_t ctrls[] = {
            {937,IK_CNS_CTRL_PAUSE,IK_CNS_TRIGGER_TIME_EQ,
             1,0,3,2,0u},
        };

        ik_cns_state_t states[2]{};
        states[0].number=937;
        states[0].anim=910;
        states[0].state_type=IK_CNS_STATE_STAND;
        states[0].move_type=IK_CNS_MOVE_ATTACK;
        states[0].physics=IK_CNS_PHYS_NONE;
        states[0].hitdef_count=1u;
        states[0].controller_count=1u;

        states[1].number=0;
        states[1].anim=0;
        states[1].state_type=IK_CNS_STATE_STAND;
        states[1].move_type=IK_CNS_MOVE_IDLE;
        states[1].physics=IK_CNS_PHYS_NONE;
        states[1].ctrl=1;

        ik_cns_asset_t asset{};
        asset.constants.life=1000;
        asset.constants.ground_back=15;
        asset.constants.ground_front=16;
        asset.constants.air_back=12;
        asset.constants.air_front=12;
        asset.constants.height=60;
        asset.states=states;
        asset.state_count=2u;
        asset.hitdefs=&hit;
        asset.hitdef_count=1u;
        asset.controllers=ctrls;
        asset.controller_count=1u;

        ik_fight_init(&g,&asset);
        place(&g,100,145);
        g.fighters[0].state=937;
        g.fighters[0].anim=910;
        g.fighters[0].ctrl=0;
        g.fighters[1].state=0;
        g.fighters[1].anim=0;

        ik_fight_controls_t p1{};
        ik_fight_controls_t p2{};
        tick2(&g,&p1,&p2);
        EQ(g.pause_time,3u);
        EQ(g.fighters[1].hp,1000);

        tick2(&g,&p1,&p2);
        EQ(g.pause_time,2u);
        EQ(g.fighters[1].hp,990);
        EQ(g.fighters[0].move_hit,1u);
    }

    /* NotHitBy is evaluated before contact resolution. A one-tick SCA
     * window therefore rejects a standing attack without consuming damage. */
    {
        ik_cns_hitdef_t hit{};
        hit.state_number=200;
        hit.trigger_kind=IK_CNS_TRIGGER_TIME_EQ;
        hit.trigger_value=0;
        hit.damage=25;
        hit.priority=4u;
        hit.hit_flags=IK_CNS_HIT_DEFAULT;

        const ik_cns_controller_t ctrls[] = {
            {931,IK_CNS_CTRL_NOT_HIT_BY,IK_CNS_TRIGGER_TIME_EQ,
             1,0,
             IK_CNS_REVERSAL_STATE_STAND|
             IK_CNS_REVERSAL_STATE_CROUCH|
             IK_CNS_REVERSAL_STATE_AIR,
             1,0u},
        };

        ik_cns_state_t states[2]{};
        states[0].number=200;
        states[0].anim=200;
        states[0].state_type=IK_CNS_STATE_STAND;
        states[0].move_type=IK_CNS_MOVE_ATTACK;
        states[0].physics=IK_CNS_PHYS_STAND;
        states[0].hitdef_count=1u;

        states[1].number=931;
        states[1].anim=0;
        states[1].state_type=IK_CNS_STATE_STAND;
        states[1].move_type=IK_CNS_MOVE_IDLE;
        states[1].physics=IK_CNS_PHYS_STAND;
        states[1].controller_count=1u;

        ik_cns_asset_t asset{};
        asset.constants.life=1000;
        asset.constants.ground_back=15;
        asset.constants.ground_front=16;
        asset.constants.air_back=12;
        asset.constants.air_front=12;
        asset.constants.height=60;
        asset.states=states;
        asset.state_count=2u;
        asset.hitdefs=&hit;
        asset.hitdef_count=1u;
        asset.controllers=ctrls;
        asset.controller_count=1u;

        ik_fight_init(&g,&asset);
        place(&g,100,145);
        g.fighters[0].state=200;
        g.fighters[0].anim=200;
        g.fighters[0].ctrl=0;
        g.fighters[1].state=931;
        g.fighters[1].anim=0;
        g.fighters[1].ctrl=0;

        ik_fight_controls_t p1{};
        ik_fight_controls_t p2{};
        tick2(&g,&p1,&p2);
        EQ(g.fighters[1].hp,1000);
        EQ(g.hits_p1,0u);
        EQ(g.fighters[1].not_hit_by_time,1u);

        tick2(&g,&p1,&p2);
        EQ(g.fighters[1].not_hit_by_time,0u);
    }

    /* HitOverride is a contact fallback, not generic invulnerability.
     * A projectile-class incoming HitDef is redirected into the authored
     * state without applying ordinary damage. */
    {
        ik_cns_hitdef_t hit{};
        hit.state_number=910;
        hit.trigger_kind=IK_CNS_TRIGGER_TIME_EQ;
        hit.trigger_value=0;
        hit.damage=99;
        hit.priority=4u;
        hit.hit_flags=IK_CNS_HIT_DEFAULT;
        hit.attack_attr_mask=IK_CNS_ATTR_NORMAL_PROJECTILE;

        const ik_cns_hitoverride_t overrides[] = {
            {932,0u,5u,IK_CNS_REVERSAL_STATE_STAND,
             IK_CNS_ATTR_NORMAL_PROJECTILE|
             IK_CNS_ATTR_SPECIAL_PROJECTILE|
             IK_CNS_ATTR_HYPER_PROJECTILE,
             933},
        };

        ik_cns_state_t states[3]{};
        states[0].number=910;
        states[0].anim=910;
        states[0].state_type=IK_CNS_STATE_STAND;
        states[0].move_type=IK_CNS_MOVE_ATTACK;
        states[0].physics=IK_CNS_PHYS_NONE;
        states[0].hitdef_count=1u;

        states[1].number=932;
        states[1].anim=0;
        states[1].state_type=IK_CNS_STATE_STAND;
        states[1].move_type=IK_CNS_MOVE_IDLE;
        states[1].physics=IK_CNS_PHYS_STAND;
        states[1].hitoverride_count=1u;

        states[2].number=933;
        states[2].anim=0;
        states[2].state_type=IK_CNS_STATE_STAND;
        states[2].move_type=IK_CNS_MOVE_IDLE;
        states[2].physics=IK_CNS_PHYS_STAND;

        ik_cns_asset_t asset{};
        asset.constants.life=1000;
        asset.constants.ground_back=15;
        asset.constants.ground_front=16;
        asset.constants.air_back=12;
        asset.constants.air_front=12;
        asset.constants.height=60;
        asset.states=states;
        asset.state_count=3u;
        asset.hitdefs=&hit;
        asset.hitdef_count=1u;
        asset.hitoverrides=overrides;
        asset.hitoverride_count=1u;

        ik_fight_init(&g,&asset);
        place(&g,100,145);
        g.fighters[0].state=910;
        g.fighters[0].anim=910;
        g.fighters[0].ctrl=0;
        g.fighters[1].state=932;
        g.fighters[1].anim=0;
        g.fighters[1].ctrl=0;

        ik_fight_controls_t p1{};
        ik_fight_controls_t p2{};
        tick2(&g,&p1,&p2);
        EQ(g.fighters[1].state,933);
        EQ(g.fighters[1].hp,1000);
        EQ(g.hits_p1,0u);
        EQ(g.fighters[0].move_contact,1u);
    }

    /* SuperPause freezes the fight at the authored controller tick and
     * applies its power delta exactly once. */
    {
        const ik_cns_controller_t ctrls[] = {
            {3000,IK_CNS_CTRL_SUPER_PAUSE,IK_CNS_TRIGGER_TIME_EQ,
             1,0,30,-1000,0u},
        };
        ik_cns_state_t state{};
        state.number=3000;
        state.anim=0;
        state.state_type=IK_CNS_STATE_STAND;
        state.move_type=IK_CNS_MOVE_ATTACK;
        state.physics=IK_CNS_PHYS_STAND;
        state.controller_count=1u;

        ik_cns_asset_t asset{};
        asset.constants.life=1000;
        asset.constants.ground_back=15;
        asset.constants.ground_front=16;
        asset.constants.air_back=12;
        asset.constants.air_front=12;
        asset.constants.height=60;
        asset.states=&state;
        asset.state_count=1u;
        asset.controllers=ctrls;
        asset.controller_count=1u;

        ik_fight_init(&g,&asset);
        g.fighters[0].state=3000;
        g.fighters[0].anim=0;
        g.fighters[0].power=1000;
        g.fighters[0].ctrl=0;
        ik_fight_controls_t p1{};
        ik_fight_controls_t p2{};

        tick2(&g,&p1,&p2);
        EQ(g.pause_time,30u);
        EQ(g.fighters[0].power,0);
        EQ(g.fighters[0].state_time,0u);
        tick2(&g,&p1,&p2);
        EQ(g.pause_time,29u);
        EQ(g.fighters[0].power,0);
    }

    /* MoveHit is distinct from MoveContact: a real damaging contact arms the
     * 3050 -> 3051 success transition on the next controller tick. */
    {
        ik_cns_hitdef_t hit{};
        hit.state_number=3050;
        hit.trigger_kind=IK_CNS_TRIGGER_TIME_EQ;
        hit.trigger_value=0;
        hit.damage=10;
        hit.priority=4u;
        hit.hit_flags=IK_CNS_HIT_DEFAULT;
        hit.attack_attr_mask=IK_CNS_ATTR_HYPER_ATTACK;

        const ik_cns_controller_t ctrls[] = {
            {3050,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_MOVE_HIT,
             0,0,3051,0,0u},
        };

        ik_cns_state_t states[2]{};
        states[0].number=3050;
        states[0].anim=910;
        states[0].state_type=IK_CNS_STATE_STAND;
        states[0].move_type=IK_CNS_MOVE_ATTACK;
        states[0].physics=IK_CNS_PHYS_NONE;
        states[0].hitdef_count=1u;
        states[0].controller_count=1u;

        states[1].number=3051;
        states[1].anim=0;
        states[1].state_type=IK_CNS_STATE_STAND;
        states[1].move_type=IK_CNS_MOVE_ATTACK;
        states[1].physics=IK_CNS_PHYS_STAND;

        ik_cns_asset_t asset{};
        asset.constants.life=1000;
        asset.constants.ground_back=15;
        asset.constants.ground_front=16;
        asset.constants.air_back=12;
        asset.constants.air_front=12;
        asset.constants.height=60;
        asset.states=states;
        asset.state_count=2u;
        asset.hitdefs=&hit;
        asset.hitdef_count=1u;
        asset.controllers=ctrls;
        asset.controller_count=1u;

        ik_fight_init(&g,&asset);
        place(&g,100,145);
        g.fighters[0].state=3050;
        g.fighters[0].anim=910;
        g.fighters[0].ctrl=0;
        ik_fight_controls_t p1{};
        ik_fight_controls_t p2{};

        tick2(&g,&p1,&p2);
        EQ(g.fighters[0].state,3050);
        EQ(g.fighters[0].move_contact,1u);
        EQ(g.fighters[0].move_hit,1u);
        EQ(g.fighters[1].hp,990);

        tick2(&g,&p1,&p2);
        EQ(g.fighters[0].state,3051);
    }

    /* A character with state 191 starts in pre-intro, holds the round
     * timer while AssertSpecial Intro is active, then releases into fight. */
    {
        const ik_cns_controller_t ctrls[] = {
            {191,IK_CNS_CTRL_ASSERT_INTRO,IK_CNS_TRIGGER_ALWAYS,
             0,0,0,0,0u},
            {191,IK_CNS_CTRL_CHANGE_STATE,IK_CNS_TRIGGER_TIME_EQ,
             2,0,0,1,IK_CNS_CTRL_HAS_CTRL},
        };

        ik_cns_state_t states[2]{};
        states[0].number=0;
        states[0].anim=0;
        states[0].state_type=IK_CNS_STATE_STAND;
        states[0].move_type=IK_CNS_MOVE_IDLE;
        states[0].physics=IK_CNS_PHYS_STAND;
        states[0].ctrl=1;

        states[1].number=191;
        states[1].anim=0;
        states[1].state_type=IK_CNS_STATE_STAND;
        states[1].move_type=IK_CNS_MOVE_IDLE;
        states[1].physics=IK_CNS_PHYS_STAND;
        states[1].controller_count=2u;

        ik_cns_asset_t asset{};
        asset.constants.life=1000;
        asset.constants.ground_back=15;
        asset.constants.ground_front=16;
        asset.constants.air_back=12;
        asset.constants.air_front=12;
        asset.constants.height=60;
        asset.states=states;
        asset.state_count=2u;
        asset.controllers=ctrls;
        asset.controller_count=2u;

        ik_fight_init(&g,&asset);
        EQ(g.round_state,0u);
        EQ(g.fighters[0].state,191);
        EQ(g.fighters[1].state,191);
        const uint32_t timer=g.timer_frames;

        ik_fight_controls_t p1{};
        ik_fight_controls_t p2{};
        tick2(&g,&p1,&p2);
        EQ(g.round_state,1u);
        EQ(g.timer_frames,timer);

        tick2(&g,&p1,&p2);
        EQ(g.round_state,1u);
        EQ(g.timer_frames,timer);

        tick2(&g,&p1,&p2);
        EQ(g.fighters[0].state,0);
        EQ(g.fighters[1].state,0);
        EQ(g.round_state,1u);
        EQ(g.timer_frames,timer);

        tick2(&g,&p1,&p2);
        EQ(g.round_state,2u);
        EQ(g.timer_frames,timer);

        tick2(&g,&p1,&p2);
        EQ(g.timer_frames,timer-1u);
    }

    /* State-level AssertSpecial noAutoTurn is applied before the generic
     * facing step, while MakeDust emits the common fightfx action 120. */
    {
        const ik_cns_controller_t ctrls[] = {
            {100,IK_CNS_CTRL_MAKE_DUST,IK_CNS_TRIGGER_TIME_EQ,
             1,0,0,0,0u},
        };
        ik_cns_state_t state{};
        state.number=100;
        state.anim=100;
        state.state_type=IK_CNS_STATE_STAND;
        state.move_type=IK_CNS_MOVE_IDLE;
        state.physics=IK_CNS_PHYS_STAND;
        state.ctrl=1;
        state.controller_count=1u;
        state.assert_special_flags=
            IK_CNS_STATE_ASSERT_NO_WALK |
            IK_CNS_STATE_ASSERT_NO_AUTO_TURN;

        ik_cns_asset_t asset{};
        asset.constants.life=1000;
        asset.constants.ground_back=15;
        asset.constants.ground_front=16;
        asset.constants.air_back=12;
        asset.constants.air_front=12;
        asset.constants.height=60;
        asset.states=&state;
        asset.state_count=1u;
        asset.controllers=ctrls;
        asset.controller_count=1u;

        ik_fight_init(&g,&asset);
        g.fighters[0].state=100;
        g.fighters[0].anim=100;
        g.fighters[0].facing=1;
        g.fighters[0].ctrl=1;
        g.fighters[0].x=150;
        g.fighters[0].x_q8=150*IK_CNS_Q8_ONE;
        g.fighters[1].x=100;
        g.fighters[1].x_q8=100*IK_CNS_Q8_ONE;

        ik_fight_controls_t p1{};
        ik_fight_controls_t p2{};
        tick2(&g,&p1,&p2);

        EQ(g.fighters[0].facing,1);
        EQ(g.effect_count,1u);
        EQ(g.effect_events[0].action,120);
    }

    /* Air recovery state 5210 applies its source Time=0 palette flash,
     * SCA NotHitBy window and turns only when P2 starts behind the fighter. */
    {
        const int32_t mul_identity =
            256 | (256 << 9) | (256 << 18);
        const ik_cns_controller_t ctrls[] = {
            {5210,IK_CNS_CTRL_PAL_FX,IK_CNS_TRIGGER_TIME_EQ,
             1,0,3,128|(128<<9)|(128<<18),0u,
             0,1,mul_identity,0,1,0},
            {5210,IK_CNS_CTRL_TURN,
             IK_CNS_TRIGGER_P2_DIST_X_LT_Q8_AT_TIME,
             -20*IK_CNS_Q8_ONE,1,0,0,0u},
            {5210,IK_CNS_CTRL_NOT_HIT_BY,IK_CNS_TRIGGER_TIME_EQ,
             1,0,7,15,0u},
        };
        ik_cns_state_t state{};
        state.number=5210;
        state.anim=5210;
        state.state_type=IK_CNS_STATE_AIR;
        state.move_type=IK_CNS_MOVE_IDLE;
        state.physics=IK_CNS_PHYS_NONE;
        state.ctrl=0;
        state.controller_count=3u;

        ik_cns_asset_t asset{};
        asset.constants.life=1000;
        asset.constants.ground_back=15;
        asset.constants.ground_front=16;
        asset.constants.air_back=12;
        asset.constants.air_front=12;
        asset.constants.height=60;
        asset.states=&state;
        asset.state_count=1u;
        asset.controllers=ctrls;
        asset.controller_count=3u;

        ik_fight_init(&g,&asset);
        g.fighters[0].state=5210;
        g.fighters[0].anim=5210;
        g.fighters[0].facing=1;
        g.fighters[0].ctrl=0;
        g.fighters[0].on_ground=0;
        g.fighters[0].x=150;
        g.fighters[0].x_q8=150*IK_CNS_Q8_ONE;
        g.fighters[1].x=100;
        g.fighters[1].x_q8=100*IK_CNS_Q8_ONE;

        ik_fight_controls_t p1{};
        ik_fight_controls_t p2{};
        tick2(&g,&p1,&p2);

        EQ(g.fighters[0].facing,-1);
        EQ(g.fighters[0].not_hit_by_mask,7u);
        EQ(g.fighters[0].not_hit_by_time,15u);
        EQ(g.fighters[0].palfx_time,3u);
        EQ(g.fighters[0].palfx_add_r,128);
        EQ(g.fighters[0].palfx_add_g,128);
        EQ(g.fighters[0].palfx_add_b,128);
    }

    /* PalFX stores additive and sinusoidal RGB modulation with an authored
     * cycle, then advances phase while the effect is alive. */
    {
        const ik_cns_controller_t ctrls[] = {
            {3080,IK_CNS_CTRL_PAL_FX,IK_CNS_TRIGGER_TIME_EQ,
             1,0,20,(32 | (16 << 9)),0u,
             (64 | (32 << 9) | (5 << 18)),3,
             (256 | (192 << 9) | (128 << 18)),
             ((0 & 0x1ff) | ((-64 & 0x1ff) << 9) |
              ((-128 & 0x1ff) << 18)),
             5,0},
        };
        ik_cns_state_t state{};
        state.number=3080;
        state.anim=0;
        state.state_type=IK_CNS_STATE_STAND;
        state.move_type=IK_CNS_MOVE_ATTACK;
        state.physics=IK_CNS_PHYS_NONE;
        state.controller_count=1u;

        ik_cns_asset_t asset{};
        asset.constants.life=1000;
        asset.constants.ground_back=15;
        asset.constants.ground_front=16;
        asset.constants.air_back=12;
        asset.constants.air_front=12;
        asset.constants.height=60;
        asset.states=&state;
        asset.state_count=1u;
        asset.controllers=ctrls;
        asset.controller_count=1u;

        ik_fight_init(&g,&asset);
        g.fighters[0].state=3080;
        g.fighters[0].anim=0;
        g.fighters[0].ctrl=0;
        ik_fight_controls_t p1{};
        ik_fight_controls_t p2{};

        tick2(&g,&p1,&p2);
        EQ(g.fighters[0].palfx_time,19u);
        EQ(g.fighters[0].palfx_add_r,32);
        EQ(g.fighters[0].palfx_add_g,16);
        EQ(g.fighters[0].palfx_add_b,0);
        EQ(g.fighters[0].palfx_sin_r,64);
        EQ(g.fighters[0].palfx_sin_g,32);
        EQ(g.fighters[0].palfx_sin_b,5);
        EQ(g.fighters[0].palfx_cycle,3u);
        EQ(g.fighters[0].palfx_phase,1u);
        EQ(g.fighters[0].palfx_mul_r,256u);
        EQ(g.fighters[0].palfx_mul_g,192u);
        EQ(g.fighters[0].palfx_mul_b,128u);
        EQ(g.fighters[0].palfx_sinmul_r,0);
        EQ(g.fighters[0].palfx_sinmul_g,-64);
        EQ(g.fighters[0].palfx_sinmul_b,-128);
        EQ(g.fighters[0].palfx_sinmul_cycle,5u);
        EQ(g.fighters[0].palfx_sinmul_phase,1u);
    }

    /* Compiled PlaySnd rows emit generic sound events at their authored
     * trigger ticks instead of relying on hard-coded attack-state audio. */
    {
        const ik_cns_playsnd_t sounds[] = {
            {3070,IK_CNS_TRIGGER_TIME_EQ,1,0,3},
        };
        ik_cns_state_t state{};
        state.number=3070;
        state.anim=0;
        state.state_type=IK_CNS_STATE_STAND;
        state.move_type=IK_CNS_MOVE_ATTACK;
        state.physics=IK_CNS_PHYS_NONE;
        state.playsnd_count=1u;

        ik_cns_asset_t asset{};
        asset.constants.life=1000;
        asset.constants.ground_back=15;
        asset.constants.ground_front=16;
        asset.constants.air_back=12;
        asset.constants.air_front=12;
        asset.constants.height=60;
        asset.states=&state;
        asset.state_count=1u;
        asset.playsnds=sounds;
        asset.playsnd_count=1u;

        ik_fight_init(&g,&asset);
        g.fighters[0].state=3070;
        g.fighters[0].anim=0;
        g.fighters[0].ctrl=0;
        ik_fight_controls_t p1{};
        ik_fight_controls_t p2{};

        tick2(&g,&p1,&p2);
        EQ(g.sound_count,1u);
        EQ(g.sound_events[0].group,0);
        EQ(g.sound_events[0].item,3);
    }

    /* AfterImage initializes trail parameters and AfterImageTime rearms the
     * short keepalive without recreating renderer history. */
    {
        const ik_cns_controller_t ctrls[] = {
            {3060,IK_CNS_CTRL_AFTER_IMAGE,IK_CNS_TRIGGER_TIME_EQ,
             1,0,2,13,0u,1,2},
            {3060,IK_CNS_CTRL_AFTER_IMAGE_TIME,IK_CNS_TRIGGER_ALWAYS,
             0,0,2,0,0u},
        };
        ik_cns_state_t state{};
        state.number=3060;
        state.anim=0;
        state.state_type=IK_CNS_STATE_STAND;
        state.move_type=IK_CNS_MOVE_ATTACK;
        state.physics=IK_CNS_PHYS_NONE;
        state.controller_count=2u;

        ik_cns_asset_t asset{};
        asset.constants.life=1000;
        asset.constants.ground_back=15;
        asset.constants.ground_front=16;
        asset.constants.air_back=12;
        asset.constants.air_front=12;
        asset.constants.height=60;
        asset.states=&state;
        asset.state_count=1u;
        asset.controllers=ctrls;
        asset.controller_count=2u;

        ik_fight_init(&g,&asset);
        g.fighters[0].state=3060;
        g.fighters[0].anim=0;
        g.fighters[0].ctrl=0;
        ik_fight_controls_t p1{};
        ik_fight_controls_t p2{};

        tick2(&g,&p1,&p2);
        EQ(g.fighters[0].afterimage_length,13u);
        EQ(g.fighters[0].afterimage_timegap,1u);
        EQ(g.fighters[0].afterimage_framegap,2u);
        OK(g.fighters[0].afterimage_time>0u);

        tick2(&g,&p1,&p2);
        OK(g.fighters[0].afterimage_time>0u);
    }

    /* HitFallDamage consumes stored HitDef fall.damage once and replays the
     * authored fall environment shake on ground impact. */
    {
        const ik_cns_controller_t ctrls[] = {
            {5110,IK_CNS_CTRL_FALL_ENV_SHAKE,IK_CNS_TRIGGER_TIME_EQ,
             1,0,0,0,0u},
            {5110,IK_CNS_CTRL_HIT_FALL_DAMAGE,IK_CNS_TRIGGER_TIME_EQ,
             1,0,0,0,0u},
        };
        ik_cns_state_t state{};
        state.number=5110;
        state.anim=0;
        state.state_type=IK_CNS_STATE_LIEDOWN;
        state.move_type=IK_CNS_MOVE_HIT;
        state.physics=IK_CNS_PHYS_NONE;
        state.controller_count=2u;

        ik_cns_asset_t asset{};
        asset.constants.life=1000;
        asset.constants.ground_back=15;
        asset.constants.ground_front=16;
        asset.constants.air_back=12;
        asset.constants.air_front=12;
        asset.constants.height=60;
        asset.states=&state;
        asset.state_count=1u;
        asset.controllers=ctrls;
        asset.controller_count=2u;

        ik_fight_init(&g,&asset);
        g.fighters[0].state=5110;
        g.fighters[0].anim=0;
        g.fighters[0].state_time=0u;
        g.fighters[0].hp=500;
        g.fighters[0].gethit_fall_damage=70;
        g.fighters[0].gethit_fall_envshake_time=15u;
        g.fighters[0].gethit_fall_envshake_ampl=6;
        g.fighters[0].gethit_fall_envshake_freq=178u;

        ik_fight_controls_t p1{};
        ik_fight_controls_t p2{};
        tick2(&g,&p1,&p2);

        EQ(g.fighters[0].hp,430);
        EQ(g.fighters[0].gethit_fall_damage,0);
        EQ(g.env_shake_time,15u);
        EQ(g.env_shake_ampl,6);
        EQ(g.env_shake_freq,178u);

        tick2(&g,&p1,&p2);
        EQ(g.fighters[0].hp,430);
    }

    /* Helper throws own their target through a generational entity handle.
     * The helper's subsequent TargetBind/TargetState controllers operate on
     * the captured root fighter and release that binding deterministically. */
    {
        ik_cns_hitdef_t hit{};
        hit.state_number=946;
        hit.trigger_kind=IK_CNS_TRIGGER_TIME_EQ;
        hit.trigger_value=0;
        hit.priority=4u;
        hit.hit_flags=IK_CNS_HIT_DEFAULT;
        hit.flags=IK_CNS_HITDEF_THROW;
        hit.attack_attr_mask=IK_CNS_ATTR_NORMAL_THROW;
        hit.p1_state_no=947;
        hit.p2_state_no=948;
        hit.p1_spr_priority=2;

        const ik_cns_controller_t ctrls[] = {
            {947,IK_CNS_CTRL_TARGET_BIND,IK_CNS_TRIGGER_ALWAYS,
             0,0,12*IK_CNS_Q8_ONE,-20*IK_CNS_Q8_ONE,0u},
            {947,IK_CNS_CTRL_TARGET_STATE,IK_CNS_TRIGGER_TIME_EQ,
             2,0,0,0,0u},
        };

        ik_cns_state_t states[4]{};
        states[0].number=0;
        states[0].anim=0;
        states[0].state_type=IK_CNS_STATE_STAND;
        states[0].move_type=IK_CNS_MOVE_IDLE;
        states[0].physics=IK_CNS_PHYS_STAND;
        states[0].ctrl=1;

        states[1].number=946;
        states[1].anim=910;
        states[1].state_type=IK_CNS_STATE_STAND;
        states[1].move_type=IK_CNS_MOVE_ATTACK;
        states[1].physics=IK_CNS_PHYS_NONE;
        states[1].hitdef_count=1u;

        states[2].number=947;
        states[2].anim=0;
        states[2].state_type=IK_CNS_STATE_STAND;
        states[2].move_type=IK_CNS_MOVE_ATTACK;
        states[2].physics=IK_CNS_PHYS_NONE;
        states[2].controller_count=2u;

        states[3].number=948;
        states[3].anim=0;
        states[3].state_type=IK_CNS_STATE_STAND;
        states[3].move_type=IK_CNS_MOVE_HIT;
        states[3].physics=IK_CNS_PHYS_NONE;

        ik_cns_asset_t asset{};
        asset.constants.life=1000;
        asset.constants.ground_back=15;
        asset.constants.ground_front=16;
        asset.constants.air_back=12;
        asset.constants.air_front=12;
        asset.constants.height=60;
        asset.states=states;
        asset.state_count=4u;
        asset.hitdefs=&hit;
        asset.hitdef_count=1u;
        asset.controllers=ctrls;
        asset.controller_count=2u;

        ik_entity_pool_t pool{};
        ik_entity_pool_init(&pool);
        ik_entity_handle_t p1_entity{};
        ik_entity_handle_t p2_entity{};
        OK(ik_entity_spawn(
            &pool,IK_ENTITY_PLAYER,1,0u,
            ik_entity_invalid_handle(),&p1_entity));
        OK(ik_entity_spawn(
            &pool,IK_ENTITY_PLAYER,2,1u,
            ik_entity_invalid_handle(),&p2_entity));

        ik_fight_init(&g,&asset);
        ik_fight_bind_entities(&g,&pool,p1_entity,p2_entity);
        place(&g,100,145);

        ik_entity_t* root0=ik_entity_get(&pool,p1_entity);
        ik_entity_t* root1=ik_entity_get(&pool,p2_entity);
        OK(root0!=nullptr); OK(root1!=nullptr);
        root0->x_q8=100*IK_CNS_Q8_ONE;
        root0->y_q8=IK_FLOOR_Y*IK_CNS_Q8_ONE;
        root0->facing=1;
        root1->x_q8=145*IK_CNS_Q8_ONE;
        root1->y_q8=IK_FLOOR_Y*IK_CNS_Q8_ONE;
        root1->facing=-1;

        ik_entity_handle_t helper{};
        OK(ik_entity_spawn(
            &pool,IK_ENTITY_HELPER,92,0u,p1_entity,&helper));
        ik_entity_t* helper_entity=ik_entity_get(&pool,helper);
        OK(helper_entity!=nullptr);
        helper_entity->x_q8=100*IK_CNS_Q8_ONE;
        helper_entity->y_q8=IK_FLOOR_Y*IK_CNS_Q8_ONE;
        helper_entity->facing=1;
        helper_entity->life=1000;

        ik_entity_runtime_t runtime{};
        ik_entity_runtime_init(
            &runtime,&pool,&asset,&k_table,&k_table);
        OK(ik_entity_runtime_enter_state(&runtime,helper,946));

        ik_fight_controls_t p1{};
        ik_fight_controls_t p2{};
        tick2(&g,&p1,&p2);

        helper_entity=ik_entity_get(&pool,helper);
        OK(helper_entity!=nullptr);
        EQ(helper_entity->state_no,947);
        EQ(g.fighters[1].state,948);
        OK(ik_entity_handle_equal(
            helper_entity->target,p2_entity));
        OK(ik_entity_handle_equal(
            g.fighters[1].bound_entity,helper));

        tick2(&g,&p1,&p2);
        helper_entity=ik_entity_get(&pool,helper);
        OK(helper_entity!=nullptr);
        EQ(g.fighters[1].x_q8,
           helper_entity->x_q8+12*IK_CNS_Q8_ONE);
        EQ(g.fighters[1].y_q8,
           helper_entity->y_q8-20*IK_CNS_Q8_ONE);

        tick2(&g,&p1,&p2);
        tick2(&g,&p1,&p2);
        EQ(g.fighters[1].state,0);
        OK(!ik_entity_handle_is_valid(
            g.fighters[1].bound_entity));
        helper_entity=ik_entity_get(&pool,helper);
        OK(helper_entity!=nullptr);
        OK(!ik_entity_handle_is_valid(helper_entity->target));
    }

    /* Dynamic helper attacks participate in ReversalDef before the normal
     * entity hit/guard path. AA matches physical NA/SA/HA, not projectiles. */
    {
        ik_cns_hitdef_t hit{};
        hit.state_number=941;
        hit.trigger_kind=IK_CNS_TRIGGER_TIME_EQ;
        hit.trigger_value=0;
        hit.damage=88;
        hit.priority=4u;
        hit.hit_flags=IK_CNS_HIT_DEFAULT;
        hit.attack_attr_mask=IK_CNS_ATTR_SPECIAL_ATTACK;

        const ik_cns_reversaldef_t reversals[] = {
            {934,0u,8u,IK_CNS_REVERSAL_STATE_STAND,
             IK_CNS_ATTR_NORMAL_ATTACK|
             IK_CNS_ATTR_SPECIAL_ATTACK|
             IK_CNS_ATTR_HYPER_ATTACK,
             0u,0u,40,0,0,6,0,935,2,1},
        };

        ik_cns_state_t states[3]{};
        states[0].number=941;
        states[0].anim=910;
        states[0].state_type=IK_CNS_STATE_STAND;
        states[0].move_type=IK_CNS_MOVE_ATTACK;
        states[0].physics=IK_CNS_PHYS_NONE;
        states[0].hitdef_count=1u;

        states[1].number=934;
        states[1].anim=910;
        states[1].state_type=IK_CNS_STATE_STAND;
        states[1].move_type=IK_CNS_MOVE_IDLE;
        states[1].physics=IK_CNS_PHYS_STAND;
        states[1].reversal_count=1u;

        states[2].number=935;
        states[2].anim=0;
        states[2].state_type=IK_CNS_STATE_STAND;
        states[2].move_type=IK_CNS_MOVE_IDLE;
        states[2].physics=IK_CNS_PHYS_STAND;

        ik_cns_asset_t asset{};
        asset.constants.life=1000;
        asset.constants.ground_back=15;
        asset.constants.ground_front=16;
        asset.constants.air_back=12;
        asset.constants.air_front=12;
        asset.constants.height=60;
        asset.states=states;
        asset.state_count=3u;
        asset.hitdefs=&hit;
        asset.hitdef_count=1u;
        asset.reversals=reversals;
        asset.reversal_count=1u;

        ik_entity_pool_t pool{};
        ik_entity_pool_init(&pool);
        ik_entity_handle_t p1_entity{};
        ik_entity_handle_t p2_entity{};
        OK(ik_entity_spawn(
            &pool,IK_ENTITY_PLAYER,1,0u,
            ik_entity_invalid_handle(),&p1_entity));
        OK(ik_entity_spawn(
            &pool,IK_ENTITY_PLAYER,2,1u,
            ik_entity_invalid_handle(),&p2_entity));

        ik_fight_init(&g,&asset);
        ik_fight_bind_entities(&g,&pool,p1_entity,p2_entity);
        place(&g,100,145);
        ik_entity_t* root0=ik_entity_get(&pool,p1_entity);
        ik_entity_t* root1=ik_entity_get(&pool,p2_entity);
        OK(root0!=nullptr); OK(root1!=nullptr);
        root0->x_q8=100*IK_CNS_Q8_ONE;
        root0->y_q8=IK_FLOOR_Y*IK_CNS_Q8_ONE;
        root0->facing=1;
        root1->x_q8=145*IK_CNS_Q8_ONE;
        root1->y_q8=IK_FLOOR_Y*IK_CNS_Q8_ONE;
        root1->facing=-1;
        g.fighters[1].state=934;
        g.fighters[1].anim=910;
        g.fighters[1].ctrl=0;

        ik_entity_handle_t helper{};
        OK(ik_entity_spawn(
            &pool,IK_ENTITY_HELPER,91,0u,p1_entity,&helper));
        ik_entity_t* helper_entity=ik_entity_get(&pool,helper);
        OK(helper_entity!=nullptr);
        helper_entity->x_q8=100*IK_CNS_Q8_ONE;
        helper_entity->y_q8=IK_FLOOR_Y*IK_CNS_Q8_ONE;
        helper_entity->facing=1;
        helper_entity->life=1000;

        ik_entity_runtime_t runtime{};
        ik_entity_runtime_init(
            &runtime,&pool,&asset,&k_table,&k_table);
        OK(ik_entity_runtime_enter_state(&runtime,helper,941));

        ik_fight_controls_t p1{};
        ik_fight_controls_t p2{};
        tick2(&g,&p1,&p2);

        EQ(g.fighters[1].state,935);
        EQ(g.fighters[1].hp,1000);
        EQ(ik_entity_count_type(&pool,IK_ENTITY_HELPER),1u);
        helper_entity=ik_entity_get(&pool,helper);
        OK(helper_entity!=nullptr);
        EQ(helper_entity->move_contact,1u);
    }

    /* Projectile entities participate in the same deterministic contact
     * pass as helpers. KFM-style HitOverride AP catches projectile HitDefs,
     * redirects the defender and consumes the projectile without damage. */
    {
        ik_cns_hitdef_t hit{};
        hit.state_number=940;
        hit.trigger_kind=IK_CNS_TRIGGER_TIME_EQ;
        hit.trigger_value=0;
        hit.damage=77;
        hit.priority=4u;
        hit.hit_flags=IK_CNS_HIT_DEFAULT;
        hit.attack_attr_mask=IK_CNS_ATTR_NORMAL_PROJECTILE;

        const ik_cns_hitoverride_t overrides[] = {
            {932,0u,8u,IK_CNS_REVERSAL_STATE_STAND,
             IK_CNS_ATTR_NORMAL_PROJECTILE|
             IK_CNS_ATTR_SPECIAL_PROJECTILE|
             IK_CNS_ATTR_HYPER_PROJECTILE,
             933},
        };

        ik_cns_state_t states[3]{};
        states[0].number=940;
        states[0].anim=910;
        states[0].state_type=IK_CNS_STATE_STAND;
        states[0].move_type=IK_CNS_MOVE_ATTACK;
        states[0].physics=IK_CNS_PHYS_NONE;
        states[0].hitdef_count=1u;

        states[1].number=932;
        states[1].anim=0;
        states[1].state_type=IK_CNS_STATE_STAND;
        states[1].move_type=IK_CNS_MOVE_IDLE;
        states[1].physics=IK_CNS_PHYS_STAND;
        states[1].hitoverride_count=1u;

        states[2].number=933;
        states[2].anim=0;
        states[2].state_type=IK_CNS_STATE_STAND;
        states[2].move_type=IK_CNS_MOVE_IDLE;
        states[2].physics=IK_CNS_PHYS_STAND;

        ik_cns_asset_t asset{};
        asset.constants.life=1000;
        asset.constants.ground_back=15;
        asset.constants.ground_front=16;
        asset.constants.air_back=12;
        asset.constants.air_front=12;
        asset.constants.height=60;
        asset.states=states;
        asset.state_count=3u;
        asset.hitdefs=&hit;
        asset.hitdef_count=1u;
        asset.hitoverrides=overrides;
        asset.hitoverride_count=1u;

        ik_entity_pool_t pool{};
        ik_entity_pool_init(&pool);
        ik_entity_handle_t p1_entity{};
        ik_entity_handle_t p2_entity{};
        OK(ik_entity_spawn(
            &pool,IK_ENTITY_PLAYER,1,0u,
            ik_entity_invalid_handle(),&p1_entity));
        OK(ik_entity_spawn(
            &pool,IK_ENTITY_PLAYER,2,1u,
            ik_entity_invalid_handle(),&p2_entity));

        ik_fight_init(&g,&asset);
        ik_fight_bind_entities(&g,&pool,p1_entity,p2_entity);
        place(&g,100,145);
        ik_entity_t* root0=ik_entity_get(&pool,p1_entity);
        ik_entity_t* root1=ik_entity_get(&pool,p2_entity);
        OK(root0!=nullptr); OK(root1!=nullptr);
        root0->x_q8=100*IK_CNS_Q8_ONE; root0->y_q8=IK_FLOOR_Y*IK_CNS_Q8_ONE; root0->facing=1;
        root1->x_q8=145*IK_CNS_Q8_ONE; root1->y_q8=IK_FLOOR_Y*IK_CNS_Q8_ONE; root1->facing=-1;
        g.fighters[1].state=932;
        g.fighters[1].anim=0;
        g.fighters[1].ctrl=0;

        ik_entity_runtime_t runtime{};
        ik_entity_runtime_init(
            &runtime,&pool,&asset,&k_table,&k_table);

        ik_entity_handle_t projectile{};
        OK(ik_entity_runtime_spawn_projectile(
            &runtime,p1_entity,77,940,
            0,0,0,0,&projectile));
        OK(ik_entity_get(&pool,projectile)!=nullptr);

        ik_fight_controls_t p1{};
        ik_fight_controls_t p2{};
        tick2(&g,&p1,&p2);

        EQ(g.fighters[1].state,933);
        EQ(g.fighters[1].hp,1000);
        EQ(ik_entity_count_type(&pool,IK_ENTITY_PROJECTILE),0u);
    }

    /* A classic Projectile controller keeps its embedded HitDef alive for
     * projhits contacts and rearms only after projmisstime expires. */
    {
        ik_cns_hitdef_t hit{};
        hit.state_number=950;
        hit.trigger_kind=IK_CNS_TRIGGER_ALWAYS;
        hit.damage=10;
        hit.priority=4u;
        hit.hit_flags=IK_CNS_HIT_DEFAULT;
        hit.attack_attr_mask=IK_CNS_ATTR_SPECIAL_PROJECTILE;

        ik_cns_state_t states[1]{};
        states[0].number=0;
        states[0].anim=910;
        states[0].state_type=IK_CNS_STATE_STAND;
        states[0].move_type=IK_CNS_MOVE_IDLE;
        states[0].physics=IK_CNS_PHYS_STAND;
        states[0].ctrl=1;

        ik_cns_projectile_t specs[1]{};
        specs[0].id=50;
        specs[0].anim_no=910;
        specs[0].hit_anim_no=-1;
        specs[0].remove_anim_no=-1;
        specs[0].cancel_anim_no=-1;
        specs[0].hitdef_global=0;
        specs[0].pos_x_q8=0;
        specs[0].pos_y_q8=0;
        specs[0].velmul_x_q8=IK_CNS_Q8_ONE;
        specs[0].velmul_y_q8=IK_CNS_Q8_ONE;
        specs[0].remove_time=-1;
        specs[0].edge_bound=100;
        specs[0].stage_bound=100;
        specs[0].hits=2u;
        specs[0].miss_time=2u;
        specs[0].priority=1u;
        specs[0].remove_on_hit=0u;
        specs[0].spr_priority=3;

        ik_cns_asset_t asset{};
        asset.constants.life=1000;
        asset.constants.ground_back=15;
        asset.constants.ground_front=16;
        asset.constants.air_back=12;
        asset.constants.air_front=12;
        asset.constants.height=60;
        asset.states=states;
        asset.state_count=1u;
        asset.hitdefs=&hit;
        asset.hitdef_count=1u;
        asset.projectiles=specs;
        asset.projectile_count=1u;

        ik_entity_pool_t pool{};
        ik_entity_pool_init(&pool);
        ik_entity_handle_t p1_entity{};
        ik_entity_handle_t p2_entity{};
        OK(ik_entity_spawn(
            &pool,IK_ENTITY_PLAYER,1,0u,
            ik_entity_invalid_handle(),&p1_entity));
        OK(ik_entity_spawn(
            &pool,IK_ENTITY_PLAYER,2,1u,
            ik_entity_invalid_handle(),&p2_entity));

        ik_fight_init(&g,&asset);
        ik_fight_bind_entities(&g,&pool,p1_entity,p2_entity);
        place(&g,100,145);
        ik_entity_t* root0=ik_entity_get(&pool,p1_entity);
        ik_entity_t* root1=ik_entity_get(&pool,p2_entity);
        OK(root0!=nullptr); OK(root1!=nullptr);
        root0->x_q8=100*IK_CNS_Q8_ONE;
        root0->y_q8=IK_FLOOR_Y*IK_CNS_Q8_ONE;
        root0->facing=1;
        root1->x_q8=145*IK_CNS_Q8_ONE;
        root1->y_q8=IK_FLOOR_Y*IK_CNS_Q8_ONE;
        root1->facing=-1;

        ik_entity_runtime_t runtime{};
        ik_entity_runtime_init(
            &runtime,&pool,&asset,&k_table,&k_table);
        ik_entity_handle_t projectile{};
        OK(ik_entity_runtime_spawn_projectile_spec(
            &runtime,p1_entity,&specs[0],&projectile));

        ik_fight_controls_t p1{};
        ik_fight_controls_t p2{};
        tick2(&g,&p1,&p2);
        EQ(g.fighters[1].hp,990);
        root0=ik_entity_get(&pool,p1_entity);
        OK(root0!=nullptr);
        EQ(root0->proj_query_contact,1u);
        EQ(root0->proj_query_hit,1u);
        EQ(root0->proj_query_guarded,0u);
        EQ(root0->proj_query_contact_time,0);
        EQ(root0->proj_query_hit_time,0);
        const ik_entity_t* shot=
            ik_entity_get_const(&pool,projectile);
        OK(shot!=nullptr);
        EQ(shot->projectile_hits_left,1u);
        EQ(shot->projectile_hit_cooldown,2u);

        tick2(&g,&p1,&p2);
        EQ(g.fighters[1].hp,990);
        root0=ik_entity_get(&pool,p1_entity);
        OK(root0!=nullptr);
        EQ(root0->proj_query_contact_time,1);
        EQ(root0->proj_query_hit_time,1);
        shot=ik_entity_get_const(&pool,projectile);
        OK(shot!=nullptr);
        EQ(shot->projectile_hit_cooldown,1u);

        tick2(&g,&p1,&p2);
        EQ(g.fighters[1].hp,980);
        root0=ik_entity_get(&pool,p1_entity);
        OK(root0!=nullptr);
        EQ(root0->proj_query_contact_time,0);
        EQ(root0->proj_query_hit_time,0);
        OK(ik_entity_get_const(&pool,projectile)==nullptr);
    }

    /* Projectile priority trades decrement both projectiles before either
     * can contact a fighter. Priority 2 survives priority 1 with value 1. */
    {
        ik_cns_hitdef_t hit{};
        hit.state_number=951;
        hit.trigger_kind=IK_CNS_TRIGGER_ALWAYS;
        hit.damage=1;
        hit.priority=4u;
        hit.hit_flags=IK_CNS_HIT_DEFAULT;
        hit.attack_attr_mask=IK_CNS_ATTR_NORMAL_PROJECTILE;

        ik_cns_state_t state{};
        state.number=0;
        state.anim=0;
        state.state_type=IK_CNS_STATE_STAND;
        state.move_type=IK_CNS_MOVE_IDLE;
        state.physics=IK_CNS_PHYS_NONE;
        state.ctrl=1;

        ik_cns_asset_t asset{};
        asset.constants.life=1000;
        asset.constants.ground_back=15;
        asset.constants.ground_front=16;
        asset.constants.air_back=12;
        asset.constants.air_front=12;
        asset.constants.height=60;
        asset.states=&state;
        asset.state_count=1u;
        asset.hitdefs=&hit;
        asset.hitdef_count=1u;

        ik_entity_pool_t pool{};
        ik_entity_pool_init(&pool);
        ik_entity_handle_t p1_entity{};
        ik_entity_handle_t p2_entity{};
        OK(ik_entity_spawn(
            &pool,IK_ENTITY_PLAYER,1,0u,
            ik_entity_invalid_handle(),&p1_entity));
        OK(ik_entity_spawn(
            &pool,IK_ENTITY_PLAYER,2,1u,
            ik_entity_invalid_handle(),&p2_entity));

        ik_fight_init(&g,&asset);
        ik_fight_bind_entities(&g,&pool,p1_entity,p2_entity);
        place(&g,60,260);
        ik_entity_t* root0=ik_entity_get(&pool,p1_entity);
        ik_entity_t* root1=ik_entity_get(&pool,p2_entity);
        OK(root0!=nullptr); OK(root1!=nullptr);
        root0->x_q8=60*IK_CNS_Q8_ONE;
        root0->y_q8=IK_FLOOR_Y*IK_CNS_Q8_ONE;
        root0->facing=1;
        root1->x_q8=260*IK_CNS_Q8_ONE;
        root1->y_q8=IK_FLOOR_Y*IK_CNS_Q8_ONE;
        root1->facing=-1;

        ik_cns_projectile_t p0{};
        p0.id=1;
        p0.anim_no=910;
        p0.hit_anim_no=-1;
        p0.remove_anim_no=-1;
        p0.cancel_anim_no=-1;
        p0.hitdef_global=0;
        p0.pos_x_q8=100*IK_CNS_Q8_ONE;
        p0.velmul_x_q8=IK_CNS_Q8_ONE;
        p0.velmul_y_q8=IK_CNS_Q8_ONE;
        p0.remove_time=-1;
        p0.edge_bound=200;
        p0.stage_bound=200;
        p0.hits=1u;
        p0.priority=2u;
        p0.remove_on_hit=0u;

        ik_cns_projectile_t p1spec=p0;
        p1spec.id=2;
        p1spec.priority=1u;

        ik_entity_runtime_t runtime{};
        ik_entity_runtime_init(
            &runtime,&pool,&asset,&k_table,&k_table);
        ik_entity_handle_t h0{};
        ik_entity_handle_t h1{};
        OK(ik_entity_runtime_spawn_projectile_spec(
            &runtime,p1_entity,&p0,&h0));
        OK(ik_entity_runtime_spawn_projectile_spec(
            &runtime,p2_entity,&p1spec,&h1));

        ik_fight_controls_t c0{};
        ik_fight_controls_t c1{};
        tick2(&g,&c0,&c1);

        const ik_entity_t* surviving=
            ik_entity_get_const(&pool,h0);
        OK(surviving!=nullptr);
        EQ(surviving->projectile_priority,1u);
        OK(ik_entity_get_const(&pool,h1)==nullptr);
        EQ(ik_entity_count_type(&pool,IK_ENTITY_PROJECTILE),1u);
    }

    /* A player Helper controller must allocate a real entity, execute that
     * helper's CNS on the shared runtime, and let DestroySelf retire the
     * generational handle without touching either root player. */
    {
        const ik_cns_controller_t helper_ctrls[] = {
            {0,IK_CNS_CTRL_HELPER,IK_CNS_TRIGGER_COMMAND_ACTIVE,
             IK_CNS_COMMAND_A,0,0,0,0u},
            {900,IK_CNS_CTRL_DESTROY_SELF,IK_CNS_TRIGGER_TIME_EQ,
             1,0,0,0,0u},
        };
        ik_cns_state_t helper_states[2] = {k_states[0], k_states[0]};
        helper_states[0].number=0;
        helper_states[0].anim=0;
        helper_states[0].hitdef_ofs=0u;
        helper_states[0].hitdef_count=0u;
        helper_states[0].playsnd_ofs=0u;
        helper_states[0].playsnd_count=0u;
        helper_states[0].controller_ofs=0u;
        helper_states[0].controller_count=1u;
        helper_states[1].number=900;
        helper_states[1].anim=0;
        helper_states[1].hitdef_ofs=0u;
        helper_states[1].hitdef_count=0u;
        helper_states[1].playsnd_ofs=0u;
        helper_states[1].playsnd_count=0u;
        helper_states[1].controller_ofs=1u;
        helper_states[1].controller_count=1u;

        const ik_cns_helper_t helpers[] = {
            {77,900,10*IK_CNS_Q8_ONE,0,1,
             IK_CNS_HELPER_POS_P1,0u,0u},
        };

        ik_cns_asset_t asset=k_cns;
        asset.states=helper_states;
        asset.state_count=2u;
        asset.hitdefs=nullptr;
        asset.hitdef_count=0u;
        asset.playsnds=nullptr;
        asset.playsnd_count=0u;
        asset.controllers=helper_ctrls;
        asset.controller_count=2u;
        asset.helpers=helpers;
        asset.helper_count=1u;

        ik_entity_pool_t pool{};
        ik_entity_pool_init(&pool);
        ik_entity_handle_t p1_entity{};
        ik_entity_handle_t p2_entity{};
        OK(ik_entity_spawn(
            &pool,IK_ENTITY_PLAYER,1,0u,
            ik_entity_invalid_handle(),&p1_entity));
        OK(ik_entity_spawn(
            &pool,IK_ENTITY_PLAYER,2,1u,
            ik_entity_invalid_handle(),&p2_entity));

        ik_fight_init(&g,&asset);
        ik_fight_bind_entities(
            &g,&pool,p1_entity,p2_entity);

        ik_fight_controls_t create{};
        create.a=1u;
        tick(&g,&create);
        EQ(ik_entity_count_type(&pool,IK_ENTITY_HELPER),1u);

        const ik_entity_t* spawned=nullptr;
        for(uint8_t slot=0u;slot<IK_ENTITY_CAPACITY;++slot){
            if(pool.entities[slot].type==IK_ENTITY_HELPER){
                spawned=&pool.entities[slot];
                break;
            }
        }
        OK(spawned!=nullptr);
        EQ(spawned->id,77);
        EQ(spawned->state_no,900);
        EQ(spawned->owner_player,0u);
        OK(ik_entity_handle_equal(spawned->parent,p1_entity));
        OK(ik_entity_handle_equal(spawned->root,p1_entity));

        ik_fight_controls_t idle{};
        tick(&g,&idle);
        EQ(ik_entity_count_type(&pool,IK_ENTITY_HELPER),0u);
        EQ(ik_entity_count_type(&pool,IK_ENTITY_PLAYER),2u);

        ik_entity_handle_t round_helper{};
        OK(ik_entity_spawn(
            &pool,IK_ENTITY_HELPER,88,0u,p1_entity,&round_helper));
        OK(ik_entity_get(&pool,round_helper)!=nullptr);
        ik_fight_reset(&g);
        OK(ik_entity_get(&pool,round_helper)==nullptr);
        EQ(ik_entity_count_type(&pool,IK_ENTITY_HELPER),0u);
        EQ(ik_entity_count_type(&pool,IK_ENTITY_PLAYER),2u);
        OK(g.entities==&pool);
    }

    std::puts("[test] ikemen_fight OK");
    return 0;
}
