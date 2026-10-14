#include "examples/characters/combat.h"
#include <cassert>
#include <cstring>
#include <cstdio>
static void assert_separated(const combat_round_t& r) {
    int64_t dx=(int64_t)r.fighters[1].position.x-r.fighters[0].position.x;
    int64_t dz=(int64_t)r.fighters[1].position.z-r.fighters[0].position.z;
    int64_t diameter=COMBAT_BODY_DIAMETER_Q16;
    assert(dx*dx+dz*dz>=diameter*diameter);
}
int main() {
    combat_round_t a,b;combat_reset(&a);combat_reset(&b);
    for(unsigned t=0;t<COMBAT_TICK_HZ*4;++t) {
        uint16_t buttons=t<COMBAT_TICK_HZ?SAT_PAD_RIGHT:SAT_PAD_A;
        combat_tick(&a,buttons);combat_tick(&b,buttons);
        assert(std::memcmp(&a,&b,sizeof(a))==0);
    }
    assert(a.hits && a.fighters[0].health+a.fighters[1].health<COMBAT_HEALTH*2);
    combat_reset(&a);a.fighters[0].position.x=0;a.fighters[1].position.x=SAT_FX16_ONE/2;
    a.fighters[1].state=COMBAT_GUARD;a.fighters[0].state=COMBAT_PUNCH;
    a.fighters[0].state_ticks=COMBAT_ATTACK_FRAME-1;
    combat_tick(&a,SAT_PAD_A);assert(a.blocked && a.fighters[1].health==COMBAT_HEALTH-COMBAT_GUARD_DAMAGE);
    combat_reset(&a);a.fighters[0].state=COMBAT_JUMP;
    combat_tick(&a,0);assert(a.fighters[0].state==COMBAT_JUMP);
    combat_reset(&a);a.fighters[1].health=COMBAT_PUNCH_DAMAGE;
    a.ticks=COMBAT_TICK_HZ+COMBAT_TICK_HZ/2;
    a.fighters[0].position.x=0;a.fighters[1].position.x=SAT_FX16_ONE/2;
    a.fighters[0].state=COMBAT_PUNCH;a.fighters[0].state_ticks=COMBAT_ATTACK_FRAME-1;
    combat_tick(&a,SAT_PAD_A);assert(a.round_over && a.knockdowns && !a.fighters[1].health);
    combat_reset(&a);assert(!a.round_over && a.fighters[1].health==COMBAT_HEALTH);
    // Walk into the opponent for longer than it takes to traverse the arena.
    combat_reset(&a);
    for(unsigned t=0;t<COMBAT_TICK_HZ*8;++t) {
        combat_tick(&a,SAT_PAD_RIGHT);assert_separated(a);
        assert(a.fighters[0].position.x<a.fighters[1].position.x);
    }
    // Coincident spawns, diagonal overlaps and a fighter pinned to the wall.
    for(unsigned scenario=0;scenario<3;++scenario) {
        combat_reset(&a);
        a.fighters[0].position.x=scenario==2?COMBAT_ARENA_HALF_Q16:0;
        a.fighters[1].position.x=a.fighters[0].position.x;
        a.fighters[1].position.z=scenario==1?COMBAT_BODY_RADIUS_Q16:0;
        combat_tick(&a,0);assert_separated(a);
        for(const auto& f:a.fighters)assert(f.position.x<=COMBAT_ARENA_HALF_Q16);
    }
    // Crossing sides remains possible after sidestepping clear of the body.
    combat_reset(&a);a.fighters[0].position.z=-COMBAT_ARENA_HALF_Q16;
    a.fighters[1].state=COMBAT_KNOCKDOWN;
    for(unsigned t=0;t<COMBAT_TICK_HZ*2;++t) {
        a.fighters[1].state_ticks=0;
        combat_tick(&a,SAT_PAD_RIGHT);assert_separated(a);
    }
    assert(a.fighters[0].position.x>a.fighters[1].position.x);
    // Explicit second-controller input must not be replaced with AI input.
    combat_reset(&a);
    combat_tick_dual(&a,0,SAT_PAD_C);
    assert(a.fighters[1].state==COMBAT_KICK);
    const int64_t diagonal=SAT_FX16_ONE*23/5;
    for(unsigned scenario=0;scenario<4;++scenario) {
        combat_reset(&a);
        for(auto& f:a.fighters) {
            f.position.x=scenario&1?-COMBAT_ARENA_HALF_Q16:COMBAT_ARENA_HALF_Q16;
            f.position.z=scenario&2?-COMBAT_ARENA_HALF_Q16:COMBAT_ARENA_HALF_Q16;
        }
        for(unsigned t=0;t<COMBAT_TICK_HZ*6;++t) {
            combat_tick_dual(&a,SAT_PAD_RIGHT|SAT_PAD_DOWN,SAT_PAD_LEFT|SAT_PAD_UP);
            combat_constrain_octagon(&a,(sat_fx16_t)diagonal);
            assert_separated(a);
            for(const auto& f:a.fighters) {
                int64_t x=f.position.x,z=f.position.z;
                assert((x<0?-x:x)+(z<0?-z:z)<=diagonal);
            }
        }
    }
    std::puts("test_character_combat: deterministic combat and body collision passed");
}
