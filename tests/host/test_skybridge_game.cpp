#include <cassert>
#include <cstdint>
#include <iostream>
#include "examples/skybridge_3d/game.h"

static uint16_t tick(sb_game_t& g,uint16_t held=0,uint16_t pressed=0) {
    return sb_tick(&g,held,pressed,0,SB_F(1),SB_F(1),0);
}
static void place(sb_game_t& g,uint8_t i) {
    g.x=sb_platform_x(&g,i);
    g.z=SB_F(sb_stage[i].z);
    g.y=SB_F(sb_stage[i].y);
    g.vx=g.vy=g.vz=0;
    g.support=(int8_t)i;
    g.coyote=5;
}
int main() {
    /* A gamepad press must match the visible camera-right direction. */
    int32_t rx=0,rz=0;
    sb_camera_right(0,SB_F(1),&rx,&rz);
    assert(rx==-SB_F(1) && rz==0); // camera faces +Z, right is -X
    sb_camera_right(SB_F(1),0,&rx,&rz);
    assert(rx==0 && rz==SB_F(1)); // yaw +90, right is +Z
    sb_camera_right(0,-SB_F(1),&rx,&rz);
    assert(rx==SB_F(1) && rz==0);
    sb_camera_right(-SB_F(1),0,&rx,&rz);
    assert(rx==0 && rz==-SB_F(1));
    {
        sb_game_t moving;
        sb_init(&moving);
        sb_camera_right(0,SB_F(1),&rx,&rz);
        sb_tick(&moving,SB_RIGHT,0u,0,SB_F(1),rx,rz);
        assert(moving.vx<0 && moving.x<0);
        sb_init(&moving);
        sb_tick(&moving,SB_LEFT,0u,0,SB_F(1),rx,rz);
        assert(moving.vx>0 && moving.x>0);
    }
    sb_game_t g;
    sb_init(&g);
    assert(g.x==0 && g.y==0 && g.z==SB_F(-4));
    assert(g.support==0 && g.pickups==0 && g.checkpoint==0);
    assert(sb_moving_offset(0)==-SB_F(4));
    assert(sb_moving_offset(60)==SB_F(4));
    assert(sb_moving_offset(120)==-SB_F(4));

    for(int i=0;i<20;++i) tick(g);
    assert(g.support==0 && g.y==0);
    uint16_t ev=tick(g,SB_JUMP,SB_JUMP);
    assert(ev & SB_EVENT_JUMP);
    assert(g.support==-1 && g.vy>0);
    int32_t highest=g.y;
    for(int i=0;i<45;++i) {
        ev=tick(g,0,0);
        if(g.y>highest)highest=g.y;
    }
    assert(highest>=SB_F(5));
    assert(g.support==0 && g.y==0);

    place(g,1);
    ev=tick(g);
    assert((ev & SB_EVENT_PICKUP)!=0u);
    assert((g.pickups&1u)!=0u);
    assert((tick(g)&SB_EVENT_PICKUP)==0u);

    place(g,3);
    ev=tick(g);
    assert((ev & SB_EVENT_CHECKPOINT)!=0u && g.checkpoint==3u);
    g.y=SB_F(-30);
    g.support=-1;
    ev=tick(g);
    assert(ev&SB_EVENT_FALL);
    assert(g.support==3 && g.y==SB_F(3) && g.pickups&1u);

    place(g,4);
    int32_t x=g.x;
    tick(g);
    assert(g.support==4 && g.x!=x); // moving platform carries a grounded player
    place(g,6);
    assert(tick(g)&SB_EVENT_CHECKPOINT);
    assert(g.checkpoint==6);

    place(g,7);
    ev=tick(g);
    assert(ev&SB_EVENT_WARNING);
    assert(g.collapse_ticks>0);
    for(int i=0;i<31;++i)tick(g);
    assert(!sb_platform_active(&g,7));
    for(int i=0;i<150;++i)tick(g);
    assert(sb_platform_active(&g,7));

    /* Check finish and pause without depending on a scripted human route. */
    place(g,9);
    g.pickups=0xFFu;
    ev=tick(g);
    assert(ev&SB_EVENT_WIN);
    assert(g.finished==1u);
    uint32_t saved=g.ticks;
    assert(tick(g)==0 && g.ticks==saved);
    sb_init(&g);
    assert(!g.finished && g.pickups==0);

    /* Horizontal block cannot be crossed while the player's feet are below it. */
    g.x=SB_F(0);
    g.z=SB_F(36-13-2-1);
    g.y=SB_F(-3);
    g.vz=SB_F(1);
    g.support=-1;
    g.coyote=0;
    tick(g,SB_UP);
    assert(g.z<=SB_F(36-13-2));

    std::cout << "skybridge: fixed-step gameplay tests passed\\n";
    return 0;
}
