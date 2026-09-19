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
    /* Rotating the follow camera must NOT change the pilot's physical
     * coordinates or its supporting deck. Old camera offsets also varied
     * in horizontal radius (33 units at 90 deg, 43 at 0 deg), so assert a
     * constant 42-unit orbit at four cardinal directions. */
    {
        const int32_t axes[4][2]={
            {0,SB_F(1)},{SB_F(1),0},{0,-SB_F(1)},{-SB_F(1),0}
        };
        for(int yaw=0;yaw<4;++yaw) {
            int32_t ex,ez,lx,lz;
            sb_camera_offset(axes[yaw][0],axes[yaw][1],&ex,&ez,&lx,&lz);
            assert(ex==-sb_mul(axes[yaw][0],SB_F(42)));
            assert(ez==-sb_mul(axes[yaw][1],SB_F(42)));
            assert(lx==sb_mul(axes[yaw][0],SB_F(8)));
            assert(lz==sb_mul(axes[yaw][1],SB_F(8)));
            assert((int64_t)ex*ex+(int64_t)ez*ez==
                   (int64_t)SB_F(42)*SB_F(42));
        }
        sb_game_t orbit;
        sb_init(&orbit);
        for(int yaw=0;yaw<4;++yaw) {
            for(int t=0;t<20;++t) {
                sb_tick(&orbit,0u,0u,axes[yaw][0],axes[yaw][1],0,0);
                assert(orbit.x==0 && orbit.y==0 && orbit.z==SB_F(-4));
                assert(orbit.support==0);
                assert(sb_supported_footprint(&orbit,0u));
            }
        }
    }
    /* A cube merely touching a deck with its last corner must not stay
     * grounded in the air when the camera is rotated to a side view. */
    {
        sb_game_t edge;
        sb_init(&edge);
        edge.x=SB_F(sb_stage[0].half_x)+SB_F(1);
        edge.z=SB_F(sb_stage[0].z);
        assert(sb_horizontal_overlap(&edge,0u));
        assert(!sb_supported_footprint(&edge,0u));
        tick(edge);
        assert(edge.support==-1 && edge.y<0);
        edge.x=SB_F(sb_stage[0].half_x)-SB_PLAYER_HALF;
        edge.y=0;edge.vy=0;edge.vx=0;edge.vz=0;
        assert(sb_supported_footprint(&edge,0u));
        tick(edge);
        assert(edge.support==0 && edge.y==0);
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
