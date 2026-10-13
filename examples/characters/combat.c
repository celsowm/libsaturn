#include "combat.h"

static sat_fx16_t absolute(sat_fx16_t x) { return x<0?-x:x; }
static sat_fx16_t arena_clamp(sat_fx16_t x) {
    const sat_fx16_t limit=COMBAT_ARENA_HALF_Q16;
    return x>limit?limit:(x<-limit?-limit:x);
}
/* Integer square root keeps collision independent of floating point and rendering. */
static uint32_t distance_root(uint64_t square) {
    uint64_t bit=(uint64_t)1<<62,result=0;
    while(bit>square)bit>>=2;
    while(bit) {
        if(square>=result+bit) { square-=result+bit;result=(result>>1)+bit; }
        else result>>=1;
        bit>>=2;
    }
    return (uint32_t)result;
}
static void separate_bodies(combat_round_t* r) {
    combat_fighter_t* a=&r->fighters[0];combat_fighter_t* b=&r->fighters[1];
    const int64_t diameter=COMBAT_BODY_DIAMETER_Q16;
    for(unsigned pass=0;pass<COMBAT_COLLISION_PASSES;++pass) {
        int64_t dx=(int64_t)b->position.x-a->position.x;
        int64_t dz=(int64_t)b->position.z-a->position.z;
        uint64_t square=(uint64_t)(dx*dx+dz*dz);
        if(square>=(uint64_t)(diameter*diameter))return;
        uint32_t distance=distance_root(square);
        if(!distance) { dx=a->facing;dz=0;distance=1; }
        int64_t correction=diameter-distance+COMBAT_COLLISION_SLOP_Q16;
        /* A wall can absorb one half of the push: subsequent passes give the
           remaining correction to the fighter who can still move. */
        int64_t share=pass?correction:(correction+1)/2;
        sat_fx16_t px=(sat_fx16_t)(dx*share/distance);
        sat_fx16_t pz=(sat_fx16_t)(dz*share/distance);
        a->position.x=arena_clamp(a->position.x-px);
        a->position.z=arena_clamp(a->position.z-pz);
        b->position.x=arena_clamp(b->position.x+px);
        b->position.z=arena_clamp(b->position.z+pz);
    }
}
static void enter(combat_fighter_t* f,combat_state_t state) {
    if (f->state!=state) { f->state=state; f->state_ticks=0; }
}
void combat_reset(combat_round_t* r) {
    __builtin_memset(r,0,sizeof(*r)); r->round_ticks=COMBAT_ROUND_SECONDS*COMBAT_TICK_HZ;
    r->fighters[0].position.x=-SAT_FX16_ONE; r->fighters[1].position.x=SAT_FX16_ONE;
    for (unsigned i=0;i<2;++i) { r->fighters[i].health=COMBAT_HEALTH; r->fighters[i].facing=i?-1:1; }
}
static unsigned duration(combat_state_t s) {
    switch(s) {
        case COMBAT_PUNCH:return COMBAT_PUNCH_TICKS;
        case COMBAT_KICK:return COMBAT_KICK_TICKS;
        case COMBAT_JUMP:return COMBAT_JUMP_TICKS;
        case COMBAT_HIT:return COMBAT_HIT_TICKS;
        case COMBAT_KNOCKDOWN:return COMBAT_KNOCKDOWN_TICKS;
        default:return 0;
    }
}
static void act(combat_fighter_t* f,uint16_t pad) {
    unsigned busy=duration(f->state);
    if (busy && f->state_ticks<busy) return;
    if (pad&SAT_PAD_A) { enter(f,COMBAT_PUNCH); ++f->attack_serial; }
    else if(pad&SAT_PAD_C) { enter(f,COMBAT_KICK); ++f->attack_serial; }
    else if(pad&SAT_PAD_X) enter(f,COMBAT_JUMP);
    else if(pad&SAT_PAD_B) enter(f,COMBAT_GUARD);
    else {
        sat_fx16_t dx=0,dz=0;
        if(pad&SAT_PAD_LEFT)dx-=COMBAT_STEP_Q16;
        if(pad&SAT_PAD_RIGHT)dx+=COMBAT_STEP_Q16;
        if(pad&SAT_PAD_UP)dz-=COMBAT_STEP_Q16;
        if(pad&SAT_PAD_DOWN)dz+=COMBAT_STEP_Q16;
        f->position.x+=dx; f->position.z+=dz;
        enter(f,dx||dz?COMBAT_WALK:COMBAT_IDLE);
        if(f->position.x>COMBAT_ARENA_HALF_Q16)f->position.x=COMBAT_ARENA_HALF_Q16;
        if(f->position.x<-COMBAT_ARENA_HALF_Q16)f->position.x=-COMBAT_ARENA_HALF_Q16;
        if(f->position.z>COMBAT_ARENA_HALF_Q16)f->position.z=COMBAT_ARENA_HALF_Q16;
        if(f->position.z<-COMBAT_ARENA_HALF_Q16)f->position.z=-COMBAT_ARENA_HALF_Q16;
    }
}
static void hit(combat_round_t* r,unsigned attacker) {
    combat_fighter_t* a=&r->fighters[attacker]; combat_fighter_t* b=&r->fighters[1-attacker];
    if((a->state!=COMBAT_PUNCH && a->state!=COMBAT_KICK) || a->state_ticks!=COMBAT_ATTACK_FRAME ||
        b->state==COMBAT_JUMP || b->state==COMBAT_KNOCKDOWN ||
        absolute(b->position.x-a->position.x)>COMBAT_ATTACK_RANGE_Q16 ||
        absolute(b->position.z-a->position.z)>COMBAT_BODY_RADIUS_Q16*2) return;
    const unsigned blocked=b->state==COMBAT_GUARD;
    const unsigned damage=blocked?COMBAT_GUARD_DAMAGE:(a->state==COMBAT_PUNCH?COMBAT_PUNCH_DAMAGE:COMBAT_KICK_DAMAGE);
    b->health=b->health>damage?b->health-damage:0; ++b->hit_serial;
    if(blocked) ++r->blocked;
    else { ++r->hits; ++b->consecutive_hits;
        if(!b->health || b->consecutive_hits>=3) {
            enter(b,COMBAT_KNOCKDOWN); b->consecutive_hits=0; ++r->knockdowns;
        } else enter(b,COMBAT_HIT);
    }
}
void combat_tick(combat_round_t* r,uint16_t buttons) {
    if(r->round_over) return;
    ++r->ticks; if(r->round_ticks)--r->round_ticks;
    for(unsigned i=0;i<2;++i) { ++r->fighters[i].state_ticks;
        if(duration(r->fighters[i].state) && r->fighters[i].state_ticks>=duration(r->fighters[i].state))
            enter(&r->fighters[i],COMBAT_IDLE);
    }
    act(&r->fighters[0],buttons);
    // Deterministic opponent: approach, alternate attacks, occasionally guard.
    combat_fighter_t* ai=&r->fighters[1]; const combat_fighter_t* player=&r->fighters[0];
    uint16_t ai_pad=0;
    sat_fx16_t dx=player->position.x-ai->position.x, dz=player->position.z-ai->position.z;
    if(absolute(dx)>COMBAT_ATTACK_RANGE_Q16*3/4)ai_pad|=dx<0?SAT_PAD_LEFT:SAT_PAD_RIGHT;
    if(absolute(dz)>COMBAT_BODY_RADIUS_Q16)ai_pad|=dz<0?SAT_PAD_UP:SAT_PAD_DOWN;
    if(!ai_pad) {
        unsigned phase=r->ticks%COMBAT_AI_CYCLE_TICKS;
        if(phase<COMBAT_TICK_HZ)ai_pad=SAT_PAD_B;
        else if(phase<COMBAT_TICK_HZ+COMBAT_AI_ATTACK_WINDOW_TICKS)
            ai_pad=(r->ticks/COMBAT_AI_CYCLE_TICKS)%2?SAT_PAD_A:SAT_PAD_C;
        // Leave a punishable recovery window rather than attacking nonstop.
    }
    act(ai,ai_pad);
    separate_bodies(r);
    r->fighters[0].facing=r->fighters[0].position.x<=r->fighters[1].position.x?1:-1;
    r->fighters[1].facing=-r->fighters[0].facing;
    hit(r,0); hit(r,1);
    if(!r->round_ticks || !r->fighters[0].health || !r->fighters[1].health)r->round_over=1;
}
