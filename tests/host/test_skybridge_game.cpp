#include <cassert>
#include <cstdint>
#include <iostream>
#include "examples/skybridge_3d/game.h"

static uint16_t tick(sb_game_t& g,uint16_t held=0,uint16_t pressed=0) {
    return sb_tick(&g,held,pressed,0,SB_F(1),SB_F(1),0);
}
static void place(sb_game_t& g,uint8_t i) {
    g.x=sb_platform_x(&g,i);
    g.z=SB_F(sb_course_platforms(&g)[i].z);
    g.y=sb_platform_y(&g,i);
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
    assert(g.facing_x==0 && g.facing_z==-1); // pink pig faces camera on spawn
    assert(g.x==0 && g.y==0 && g.z==SB_F(-4));
    assert(g.support==0 && g.pickups==0 && g.checkpoint==0);
    /* Visual facing is persistent and independent from camera yaw. It
     * changes only for clear horizontal movement, not small diagonal
     * velocity, input release, a jump or a mere camera orbit. */
    {
        sb_game_t pig;
        sb_init(&pig);
        int8_t initial_x=pig.facing_x,initial_z=pig.facing_z;
        pig.vx=SB_F(1)/16;
        pig.vz=SB_F(1)/16;
        sb_update_facing(&pig);
        assert(pig.facing_x==initial_x && pig.facing_z==initial_z);
        pig.vx=-SB_F(1);
        pig.vz=SB_F(1)/16;
        sb_update_facing(&pig);
        assert(pig.facing_x==-1 && pig.facing_z==0);
        pig.vx=0;
        pig.vz=0;
        sb_update_facing(&pig);
        assert(pig.facing_x==-1 && pig.facing_z==0);
        pig.vx=SB_F(3)/4;
        pig.vz=SB_F(3)/4;
        sb_update_facing(&pig);
        assert(pig.facing_x==-1 && pig.facing_z==0);
        pig.vx=SB_F(1)/16;
        pig.vz=-SB_F(1);
        sb_update_facing(&pig);
        assert(pig.facing_x==0 && pig.facing_z==-1);
        sb_init(&pig);
        tick(pig,SB_RIGHT);
        assert(pig.facing_x==-1 && pig.facing_z==0);
        const int32_t facing_x=pig.facing_x,facing_z=pig.facing_z;
        pig.paused=1;
        for(int yaw=0;yaw<4;++yaw)
            sb_tick(&pig,0,0,0,SB_F(1),SB_F(1),0);
        assert(pig.facing_x==facing_x && pig.facing_z==facing_z);
        pig.paused=0;
        pig.vx=0;
        pig.vz=0;
        assert(tick(pig,SB_JUMP,SB_JUMP)&SB_EVENT_JUMP);
        assert(pig.facing_x==facing_x && pig.facing_z==facing_z);
        sb_start_course(&pig,1u);
        assert(pig.facing_x==0 && pig.facing_z==-1);
        assert(pig.course==1u && pig.y==0);
    }
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

    /* A gem is a small world-space object, not a +/-6-unit square tied
     * to support==id. Far/diagonal/vertical misses must never collect. */
    {
        sb_game_t item;
        sb_init(&item);
        place(item,1);
        assert(!sb_gem_contact(&item,0u));
        assert(!sb_gem_contact(&item,9u));
        assert(sb_gem_contact(&item,1u));
        item.x=SB_F(sb_stage[1].x)+SB_F(5);
        assert(!sb_gem_contact(&item,1u));
        assert((tick(item)&SB_EVENT_PICKUP)==0u);
        item.x=SB_F(sb_stage[1].x)+SB_F(4);
        item.z=SB_F(sb_stage[1].z)+SB_F(4);
        assert(!sb_gem_contact(&item,1u));
        assert((tick(item)&SB_EVENT_PICKUP)==0u);
        item.x=SB_F(sb_stage[1].x);
        item.z=SB_F(sb_stage[1].z);
        item.y=SB_F(sb_stage[1].y)+SB_F(11);
        item.support=-1;
        assert(!sb_gem_contact(&item,1u));
        assert((tick(item)&SB_EVENT_PICKUP)==0u);
        item.y=SB_F(sb_stage[1].y)-SB_F(10);
        assert(!sb_gem_contact(&item,1u));
        assert((tick(item)&SB_EVENT_PICKUP)==0u);
        /* Being airborne must not prevent touching a gem in 3D. */
        item.y=SB_F(sb_stage[1].y)+SB_F(2);
        item.vy=0;
        item.support=-1;
        assert(sb_gem_contact(&item,1u));
        assert(tick(item)&SB_EVENT_PICKUP);
        assert(item.support==-1);
        assert(item.pickups==1u);
        assert((tick(item)&SB_EVENT_PICKUP)==0u);
        /* The collectible follows the CURRENT moving-platform offset. */
        sb_init(&item);
        place(item,4);
        int32_t center=sb_platform_x(&item,4u);
        item.x=center+SB_F(5);
        assert(!sb_gem_contact(&item,4u));
        item.moving_x+=SB_F(3);
        assert(sb_gem_contact(&item,4u));
        /* A collapsed platform must not leave an invisible collectible. */
        sb_init(&item);
        place(item,7);
        assert(sb_gem_contact(&item,7u));
        item.collapse_ticks=32u;
        assert(!sb_gem_contact(&item,7u));
    }

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

    /* The finishing arch is an unconditional goal: gems are bonus score.
     * Verify zero and partial collection, then retain win/pause behavior. */
    place(g,9);
    g.pickups=0u;
    ev=tick(g);
    assert(ev&SB_EVENT_WIN);
    assert(g.finished==1u && g.pickups==0u);
    uint32_t saved=g.ticks;
    assert(tick(g)==0 && g.ticks==saved);
    sb_init(&g);
    place(g,9);
    g.pickups=0x35u;
    ev=tick(g);
    assert(ev&SB_EVENT_WIN);
    assert(g.finished==1u && g.pickups==0x35u);
    sb_init(&g);
    assert(!g.finished && g.pickups==0);

    /* Course 2 is a distinct 10-deck stage, not a cosmetic scene swap.
     * Narrow ledges must change ground collision extents and four elevator
     * decks must actually change their Y across the cycle. */
    {
        sb_game_t lift;
        sb_start_course(&lift,1u);
        assert(lift.course==1u && lift.support==0 && lift.checkpoint==0u);
        assert(lift.pickups==0u && lift.ticks==0u && lift.y==SB_F(0));
        assert(sb_course_platforms(&lift)==sb_stage_two);
        assert(sb_course_platforms(&lift)[1].half_x<
               sb_stage[1].half_x);
        assert(sb_course_platforms(&lift)[3].half_x<
               sb_stage[3].half_x);
        for(uint8_t id=1u;id<SB_PLATFORM_COUNT;++id) {
            const sb_platform_t* p=&sb_course_platforms(&lift)[id];
            const sb_platform_t* prev=&sb_course_platforms(&lift)[id-1u];
            assert(p->half_x>SB_PLAYER_HALF/65536);
            assert(p->half_z>SB_PLAYER_HALF/65536);
            /* Adjacent gaps at rest fit a plausible 35-unit jump. */
            assert(p->z-prev->z-(p->half_z+prev->half_z)<=17);
            if(p->kind==SB_LIFT) {
                assert(sb_platform_y_at(&lift,id,0u)!=
                       sb_platform_y_at(&lift,id,90u));
            }
        }
        const uint8_t moving_ids[4]={2u,4u,6u,8u};
        for(uint8_t n=0u;n<4u;++n) {
            uint8_t id=moving_ids[n];
            sb_start_course(&lift,1u);
            place(lift,id);
            int32_t x=lift.x,z=lift.z;
            int32_t top_start=lift.y;
            int saw_up=0,saw_down=0;
            for(int t=0;t<180;++t) {
                int32_t before=lift.y;
                uint16_t event=tick(lift);
                assert(!(event&SB_EVENT_FALL));
                assert(lift.support==(int8_t)id);
                assert(lift.y==sb_platform_y(&lift,id));
                assert(lift.vy==0);
                assert(lift.x==x && lift.z==z);
                if(lift.y>before)saw_up=1;
                if(lift.y<before)saw_down=1;
            }
            assert(saw_up && saw_down && top_start==lift.y);
        }
        /* Airborne: jumping from a moving deck detaches its passenger.
         * The floor itself keeps moving after the jump. */
        sb_start_course(&lift,1u);
        place(lift,2u);
        assert(tick(lift,SB_JUMP,SB_JUMP)&SB_EVENT_JUMP);
        assert(lift.support==-1);
        int32_t after_jump=lift.y;
        tick(lift,SB_JUMP);
        assert(lift.support==-1 && lift.y>after_jump);
        /* Descending player lands on an elevator using its swept old/new
         * top rather than a static height; a gem rides at that same top. */
        sb_start_course(&lift,1u);
        place(lift,2u);
        lift.y+=SB_F(1);
        lift.vy=-SB_F(1);
        lift.support=-1;
        tick(lift);
        assert(lift.support==2);
        assert(lift.y==sb_platform_y(&lift,2u));
        assert(lift.pickups&(1u<<1u));
        /* Narrow second-course deck does not inherit first-course bounds. */
        sb_start_course(&lift,1u);
        lift.x=SB_F(sb_stage_two[1].x+sb_stage_two[1].half_x+1);
        lift.z=SB_F(sb_stage_two[1].z);
        assert(!sb_supported_footprint(&lift,1u));
        /* The first course still uses its horizontal moving deck; the
         * second course has no collapsing bridge at index 7. */
        place(lift,7u);
        for(int i=0;i<200;++i)tick(lift);
        assert(sb_platform_active(&lift,7u));
        assert(lift.collapse_ticks==0u);
        /* Reaching course two's goal needs no optional gems. */
        sb_start_course(&lift,1u);
        place(lift,9u);
        assert(lift.pickups==0u);
        assert(tick(lift)&SB_EVENT_WIN);
        assert(lift.finished && lift.course==1u);
        sb_start_course(&lift,0u);
        assert(!lift.finished && lift.course==0u && lift.pickups==0u);
        assert(sb_course_platforms(&lift)==sb_stage);
    }

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
