#ifndef SKYBRIDGE_GAME_H
#define SKYBRIDGE_GAME_H

#include <stdint.h>

/* Pure, allocation-free, 60 Hz platformer logic; buildable on the host. */
#define SB_F(n) ((int32_t)(n) * 65536)
#define SB_HALF (32768)
#define SB_PLAYER_HALF SB_F(2)
#define SB_PLAYER_HEIGHT SB_F(5)
#define SB_PLATFORM_COUNT 10u
#define SB_PICKUP_COUNT 8u

enum { SB_UP=1u, SB_DOWN=2u, SB_LEFT=4u, SB_RIGHT=8u, SB_JUMP=16u };
enum { SB_EVENT_JUMP=1u, SB_EVENT_LAND=2u, SB_EVENT_PICKUP=4u,
       SB_EVENT_CHECKPOINT=8u, SB_EVENT_FALL=16u, SB_EVENT_WIN=32u,
       SB_EVENT_WARNING=64u };
enum { SB_FIXED=0u, SB_MOVING=1u, SB_COLLAPSING=2u };

typedef struct sb_platform {
    int16_t x, y, z, half_x, half_z;
    uint8_t kind;
} sb_platform_t;

static const sb_platform_t sb_stage[SB_PLATFORM_COUNT] = {
    { 0,  0,   0, 18, 18, SB_FIXED}, /* Pier */
    { 0,  0,  36, 13, 13, SB_FIXED},
    {14,  3,  68, 12, 12, SB_FIXED},
    {15,  3, 102, 13, 13, SB_FIXED}, /* Checkpoint A */
    { 2,  5, 137, 13, 13, SB_MOVING},
    {-7,  5, 169, 13, 12, SB_FIXED},
    { 0,  7, 202, 13, 12, SB_FIXED}, /* Checkpoint B */
    { 9,  7, 235, 13, 12, SB_COLLAPSING},
    { 9, 10, 266, 13, 12, SB_FIXED},
    { 0, 11, 299, 18, 15, SB_FIXED} /* Finish */
};

typedef struct sb_game {
    int32_t x, y, z, vx, vy, vz;
    int32_t moving_x;
    uint32_t ticks;
    uint16_t pickups;            /* 8-bit completion mask */
    uint16_t collapse_ticks;     /* 0=ready, 1..30 warning, 31..150 absent */
    uint8_t checkpoint;          /* stage index 0, 3 or 6 */
    uint8_t coyote, jump_buffer, finished, paused;
    int8_t support;              /* -1 when airborne */
} sb_game_t;

static inline int32_t sb_abs(int32_t n) { return n < 0 ? -n : n; }
static inline int32_t sb_clamp(int32_t n, int32_t low, int32_t high) {
    return n < low ? low : (n > high ? high : n);
}
static inline int32_t sb_mul(int32_t a, int32_t b) {
    return (int32_t)(((int64_t)a * b) >> 16);
}
static inline int32_t sb_platform_x(const sb_game_t* g, uint8_t id) {
    return SB_F(sb_stage[id].x) + (id == 4u ? g->moving_x : 0);
}
static inline int sb_platform_active(const sb_game_t* g, uint8_t id) {
    return id != 7u || g->collapse_ticks <= 30u || g->collapse_ticks >= 150u;
}
static inline int sb_horizontal_overlap(const sb_game_t* g, uint8_t id) {
    const sb_platform_t* p = &sb_stage[id];
    return sb_abs(g->x - sb_platform_x(g, id)) < SB_F(p->half_x) + SB_PLAYER_HALF &&
           sb_abs(g->z - SB_F(p->z)) < SB_F(p->half_z) + SB_PLAYER_HALF;
}
static inline int32_t sb_moving_offset(uint32_t tick) {
    /* 4-unit triangular motion, 120 ticks per round trip. */
    uint32_t phase = tick % 120u;
    int32_t t = (int32_t)(phase < 60u ? phase : 120u - phase);
    return ((t * 8 - 240) * SB_F(1)) / 60;
}
static inline void sb_respawn(sb_game_t* g) {
    uint8_t cp = g->checkpoint;
    g->x = sb_platform_x(g, cp);
    g->z = SB_F(sb_stage[cp].z - 4);
    g->y = SB_F(sb_stage[cp].y);
    g->vx = g->vy = g->vz = 0;
    g->support = (int8_t)cp;
    g->coyote = 5u;
    g->jump_buffer = 0u;
    g->collapse_ticks = 0u;
}
static inline void sb_init(sb_game_t* g) {
    *g = (sb_game_t){0};
    g->moving_x = sb_moving_offset(0u);
    sb_respawn(g);
}
static inline void sb_side_collide(sb_game_t* g, int32_t old_x, int32_t old_z) {
    uint8_t i;
    for (i=0u; i<SB_PLATFORM_COUNT; ++i) {
        const sb_platform_t* p = &sb_stage[i];
        const int32_t cx = sb_platform_x(g, i), cz = SB_F(p->z);
        const int32_t left = cx - SB_F(p->half_x) - SB_PLAYER_HALF;
        const int32_t right = cx + SB_F(p->half_x) + SB_PLAYER_HALF;
        const int32_t back = cz - SB_F(p->half_z) - SB_PLAYER_HALF;
        const int32_t front = cz + SB_F(p->half_z) + SB_PLAYER_HALF;
        if (!sb_platform_active(g,i) || g->y >= SB_F(p->y) - SB_F(1)/16 ||
            g->y + SB_PLAYER_HEIGHT <= SB_F(p->y-5)) continue;
        if (g->z > back && g->z < front) {
            if (old_x <= left && g->x > left) { g->x=left; g->vx=0; }
            if (old_x >= right && g->x < right) { g->x=right; g->vx=0; }
        }
        if (g->x > left && g->x < right) {
            if (old_z <= back && g->z > back) { g->z=back; g->vz=0; }
            if (old_z >= front && g->z < front) { g->z=front; g->vz=0; }
        }
    }
}
/* Camera basis is supplied as unit 16.16 forward (fx,fz), right (rx,rz). */
static inline uint16_t sb_tick(sb_game_t* g, uint16_t held, uint16_t pressed,
                               int32_t fx, int32_t fz, int32_t rx, int32_t rz) {
    uint16_t event = 0u;
    uint8_t i;
    int32_t want_f=0, want_r=0, old_x, old_z, old_y, best_y;
    int8_t old_support=g->support, best=-1;
    if (g->finished || g->paused) return 0u;
    ++g->ticks;
    {
        int32_t old_move=g->moving_x;
        g->moving_x=sb_moving_offset(g->ticks);
        if (old_support==4) g->x+=g->moving_x-old_move;
    }
    if (g->collapse_ticks) {
        ++g->collapse_ticks;
        if (g->collapse_ticks >= 150u) g->collapse_ticks=0u;
    }
    if (old_support==7 && g->collapse_ticks==0u) {
        g->collapse_ticks=1u;
        event|=SB_EVENT_WARNING;
    }
    if (old_support==7 && !sb_platform_active(g,7u)) {
        g->support=-1;
        old_support=-1;
    }
    if (held & SB_UP) ++want_f;
    if (held & SB_DOWN) --want_f;
    if (held & SB_RIGHT) ++want_r;
    if (held & SB_LEFT) --want_r;
    {
        int32_t dx=sb_mul(fx,SB_F(want_f))+sb_mul(rx,SB_F(want_r));
        int32_t dz=sb_mul(fz,SB_F(want_f))+sb_mul(rz,SB_F(want_r));
        int32_t cap=SB_F(1)+SB_HALF;
        int32_t target_x=sb_clamp(dx*3/2,-cap,cap);
        int32_t target_z=sb_clamp(dz*3/2,-cap,cap);
        int32_t accel=(old_support>=0)?SB_F(1)/5:SB_F(1)/12;
        g->vx+=sb_clamp(target_x-g->vx,-accel,accel);
        g->vz+=sb_clamp(target_z-g->vz,-accel,accel);
    }
    if (pressed & SB_JUMP) g->jump_buffer=7u;
    else if (g->jump_buffer) --g->jump_buffer;
    if (old_support>=0) g->coyote=5u;
    else if (g->coyote) --g->coyote;
    if (g->jump_buffer && g->coyote) {
        g->vy=SB_F(1)+SB_F(4)/5;
        g->support=-1;
        g->coyote=0u;
        g->jump_buffer=0u;
        old_support=-1;
        event|=SB_EVENT_JUMP;
    }
    if (!(held & SB_JUMP) && g->vy>SB_F(1)/3) g->vy-=SB_F(1)/10;
    g->vy-=SB_F(1)/9;
    if (g->vy<-SB_F(3)) g->vy=-SB_F(3);
    old_x=g->x; old_z=g->z; old_y=g->y;
    g->x+=g->vx; g->z+=g->vz;
    sb_side_collide(g,old_x,old_z);
    g->y+=g->vy;
    best_y=-SB_F(100);
    if (g->vy<=0) {
        for (i=0u;i<SB_PLATFORM_COUNT;++i) {
            int32_t top=SB_F(sb_stage[i].y);
            if (!sb_platform_active(g,i) || !sb_horizontal_overlap(g,i)) continue;
            if (old_y>=top-SB_F(1)/8 && g->y<=top && top>best_y) {
                best=(int8_t)i; best_y=top;
            }
        }
        if (best>=0) {
            if (old_support<0 && g->vy<-SB_F(1)/2) event|=SB_EVENT_LAND;
            g->y=best_y; g->vy=0; g->support=best; g->coyote=5u;
        } else g->support=-1;
    } else {
        g->support=-1;
        for (i=0u;i<SB_PLATFORM_COUNT;++i) {
            int32_t underside=SB_F(sb_stage[i].y-5);
            if (!sb_platform_active(g,i) || !sb_horizontal_overlap(g,i)) continue;
            if (old_y+SB_PLAYER_HEIGHT<=underside &&
                g->y+SB_PLAYER_HEIGHT>=underside) {
                g->y=underside-SB_PLAYER_HEIGHT; g->vy=0;
                break;
            }
        }
    }
    if (g->support>=0) {
        uint8_t id=(uint8_t)g->support;
        if (id>=1u && id<=8u && !(g->pickups & (1u<<(id-1u))) &&
            sb_abs(g->x-sb_platform_x(g,id))<SB_F(6) &&
            sb_abs(g->z-SB_F(sb_stage[id].z))<SB_F(6)) {
            g->pickups|=(uint16_t)(1u<<(id-1u));
            event|=SB_EVENT_PICKUP;
        }
        if ((id==3u || id==6u) && g->checkpoint<id) {
            g->checkpoint=id;
            event|=SB_EVENT_CHECKPOINT;
        }
        if (id==9u && g->pickups==0xFFu) {
            g->finished=1u;
            event|=SB_EVENT_WIN;
        }
    }
    if (g->y<SB_F(-28)) {
        sb_respawn(g);
        event|=SB_EVENT_FALL;
    }
    return event;
}

#endif
