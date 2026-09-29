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
    F(5100,0,2,0,0),
    F(5160,0,4,0,0),
    F(5110,0,0,0,0),
    F(5120,0,3,0,0),

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
    {200,IK_CNS_TRIGGER_ANIM_ELEM_EQ,3,23,0,3,8,8,IK_CNS_GROUND_HIGH,5,11,15,-1024,0,-358,-768,0,-10,-76,5,0,6,0,0,
     IK_CNS_GUARD_STAND|IK_CNS_GUARD_CROUCH|IK_CNS_GUARD_AIR,5,11,11,-1024,-537,-384,1,1},
    {210,IK_CNS_TRIGGER_ANIM_ELEM_EQ,3,57,0,4,12,12,IK_CNS_GROUND_HIGH,12,16,16,-1408,0,-640,-1024,1,-10,-70,5,2,6,0,IK_CNS_HITDEF_FORCE_NO_FALL},
    {230,IK_CNS_TRIGGER_TIME_EQ,0,26,0,4,12,12,IK_CNS_GROUND_LOW,10,14,14,-1280,0,-640,-896,0,-10,-37,5,1,6,0,0},
    {240,IK_CNS_TRIGGER_TIME_EQ,0,63,0,4,12,12,IK_CNS_GROUND_LOW,12,17,17,-1536,0,-563,-819,1,-10,-60,5,2,6,0,0},
    {400,IK_CNS_TRIGGER_TIME_EQ,0,23,0,3,10,11,IK_CNS_GROUND_LOW,4,9,9,-1024,0,-384,-768,0,-10,-42,5,0,6,0,0,
     IK_CNS_GUARD_CROUCH,4,9,9,-1024,-576,-384},
    {410,IK_CNS_TRIGGER_ANIM_ELEM_EQ,3,37,0,4,12,12,IK_CNS_GROUND_LOW,12,17,17,-1024,0,-768,-1024,1,-10,-55,5,2,6,0,0},
    {410,IK_CNS_TRIGGER_ANIM_ELEM_EQ,4,36,0,4,12,12,IK_CNS_GROUND_HIGH,12,17,17,-1792,0,-768,-1024,-1,-10,-83,5,2,6,0,0},
    {430,IK_CNS_TRIGGER_TIME_EQ,0,28,0,4,12,12,IK_CNS_GROUND_LOW,6,10,10,-1280,0,-512,-768,0,-10,-8,5,1,6,0,0},
    {440,IK_CNS_TRIGGER_TIME_EQ,0,72,0,4,12,12,IK_CNS_GROUND_TRIP,10,17,17,-384,-512,-307,-768,1,-5,-10,5,2,6,0,IK_CNS_HITDEF_FALL,
     IK_CNS_GUARD_CROUCH,10,17,17,-1280,-461,-384,2,2,0,-1152,0,1,4},
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
};
#undef S
#undef C
#undef A

static const ik_cns_asset_t k_cns = {
    {1000,15,16,12,12,60,160,614,-563,1178,0,-1152,-973,0,-2150,-653,640,1024,-2074,0,-2074,-653,640,1,35,113,218,210,512,13,
     60,6400,3840,0,5120,102,3072,13},
    k_states,(uint16_t)(sizeof(k_states)/sizeof(k_states[0])),
    k_hitdefs,13u,
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

static void tick(ik_fight_t* g, const ik_fight_controls_t* p) {
    ik_fight_update(g,p,nullptr,&k_table);
}

static void tick2(ik_fight_t* g,
                  const ik_fight_controls_t* p1,
                  const ik_fight_controls_t* p2) {
    ik_fight_update(g,p1,p2,&k_table);
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
        EQ(g.fighters[1].state,5000);
        EQ(g.fighters[1].vx_q8,0);
        idle(&g,9);
        OK(g.fighters[1].state==5001 || g.fighters[1].state==0);
    }

    /* MA guardflag blocks standing. Guard hit enters common 150/151
     * rather than the legacy damage state and does not count as a hit. */
    {
        ik_fight_init(&g,&k_cns); place(&g,100,145);
        const int hp=g.fighters[1].hp;
        ik_fight_controls_t p1{}; request(&p1,IK_STATE_PUNCH);
        ik_fight_controls_t p2{}; p2.back=1;
        for(int i=0;i<12 && !(g.events&IK_EVENT_GUARD);++i) {
            tick2(&g,&p1,&p2);
            p1.has_state_request=0u;
        }
        OK((g.events&IK_EVENT_GUARD)!=0u);
        EQ(g.fighters[1].hp,hp);
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
        p1={}; request(&p1,IK_STATE_CROUCH_PUNCH);
        p2={}; p2.back=1; p2.down=1;
        for(int i=0;i<8 && !(g.events&IK_EVENT_GUARD);++i) {
            tick2(&g,&p1,&p2);
            p1.has_state_request=0u;
        }
        OK((g.events&IK_EVENT_GUARD)!=0u);
        EQ(g.fighters[1].hp,1000);
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

    std::puts("[test] ikemen_fight OK");
    return 0;
}
