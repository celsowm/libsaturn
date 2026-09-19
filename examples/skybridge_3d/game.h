#ifndef SKYBRIDGE_GAME_H
#define SKYBRIDGE_GAME_H

#include <stdint.h>

/* Pure, allocation-free, 60 Hz platformer logic; buildable on the host. */
#define SB_F(n) ((int32_t)(n) * 65536)
#define SB_HALF (32768)
#define SB_FADE_START SB_F(66)
#define SB_FADE_END SB_F(134)
#define SB_PLAYER_HALF SB_F(2)
#define SB_PLAYER_HEIGHT SB_F(5)
#define SB_PLATFORM_COUNT 10u
#define SB_COURSE_COUNT 2u
#define SB_PICKUP_COUNT 8u
#define SB_GEM_RADIUS SB_F(2)
#define SB_GEM_BASE_OFFSET SB_F(4)
#define SB_GEM_HALF_HEIGHT SB_F(2)

enum { SB_UP=1u, SB_DOWN=2u, SB_LEFT=4u, SB_RIGHT=8u, SB_JUMP=16u };
enum { SB_EVENT_JUMP=1u, SB_EVENT_LAND=2u, SB_EVENT_PICKUP=4u,
       SB_EVENT_CHECKPOINT=8u, SB_EVENT_FALL=16u, SB_EVENT_WIN=32u,
       SB_EVENT_WARNING=64u };
enum { SB_FIXED=0u, SB_MOVING=1u, SB_COLLAPSING=2u, SB_LIFT=3u };

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

/* Second course: narrower landing zones, varied widths and four elevators.
 * Adjacent deck gaps are bounded for the original jump speed; checkpoints
 * remain at stages 3 and 6. No new assets or expansion RAM are required. */
static const sb_platform_t sb_stage_two[SB_PLATFORM_COUNT] = {
    {  0, 0,   0, 20, 17, SB_FIXED},
    { -2, 2,  29,  8, 11, SB_FIXED},
    {  8, 4,  58,  9, 10, SB_LIFT},
    { 12, 8,  86,  7, 10, SB_FIXED},
    {  1, 8, 113, 12,  9, SB_LIFT},
    { -9,12, 141,  7, 11, SB_FIXED},
    { -2,14, 169, 11, 11, SB_LIFT},
    {  6,16, 196,  7,  9, SB_FIXED},
    { 12,18, 223, 10,  8, SB_LIFT},
    {  0,20, 251, 17, 15, SB_FIXED}
};

typedef struct sb_game {
    int32_t x, y, z, vx, vy, vz;
    int32_t moving_x;
    uint32_t ticks;
    uint16_t pickups;            /* optional, 8-bit collectible mask */
    uint16_t collapse_ticks;     /* 0=ready, 1..30 warning, 31..150 absent */
    uint8_t checkpoint;          /* stage index 0, 3 or 6 */
    uint8_t course;              /* 0=original, 1=variable-width elevators */
    uint8_t coyote, jump_buffer, finished, paused;
    int8_t facing_x, facing_z;    /* persistent cardinal snout direction */
    int8_t support;              /* -1 when airborne */
} sb_game_t;

static inline int32_t sb_abs(int32_t n) { return n < 0 ? -n : n; }
static inline int32_t sb_clamp(int32_t n, int32_t low, int32_t high) {
    return n < low ? low : (n > high ? high : n);
}
static inline int32_t sb_mul(int32_t a, int32_t b) {
    return (int32_t)(((int64_t)a * b) >> 16);
}
/* VDP1 has no Z-buffer. Sort independent actors (pig and gems) by their
 * 3D camera-space centre depth, NOT by platform id or world Z. The view
 * vector includes camera pitch, so a raised gem and a grounded avatar
 * can swap their screen-space depth as the follow camera orbits.
 * Return int64 to avoid overflow with distant world coordinates. */
static inline int64_t sb_actor_view_depth(
    int32_t x,int32_t y,int32_t z,
    int32_t eye_x,int32_t eye_y,int32_t eye_z,
    int32_t view_x,int32_t view_y,int32_t view_z) {
    return ((int64_t)(x-eye_x)*view_x+
            (int64_t)(y-eye_y)*view_y+
            (int64_t)(z-eye_z)*view_z)>>16u;
}

static inline const sb_platform_t* sb_course_platforms(const sb_game_t* g) {
    return g->course==1u?sb_stage_two:sb_stage;
}
/* Vertical elevators follow a triangular 180-tick cycle. Each lift's
 * phase is staggered so the whole course is not moving in lockstep. */
static inline int32_t sb_lift_offset(uint32_t tick,uint8_t id) {
    uint32_t phase=(tick+(uint32_t)id*23u)%180u;
    int32_t ramp=(int32_t)(phase<90u?phase:180u-phase);
    return ((ramp*2-90)*SB_F(4))/90;
}
static inline int32_t sb_platform_y_at(const sb_game_t* g,uint8_t id,uint32_t tick) {
    const sb_platform_t* p=&sb_course_platforms(g)[id];
    return SB_F(p->y)+(p->kind==SB_LIFT?sb_lift_offset(tick,id):0);
}
static inline int32_t sb_platform_y(const sb_game_t* g,uint8_t id) {
    return sb_platform_y_at(g,id,g->ticks);
}
static inline int32_t sb_platform_x(const sb_game_t* g, uint8_t id) {
    const sb_platform_t* p=&sb_course_platforms(g)[id];
    return SB_F(p->x) + (p->kind==SB_MOVING ? g->moving_x : 0);
}
static inline int sb_platform_active(const sb_game_t* g, uint8_t id) {
    return sb_course_platforms(g)[id].kind!=SB_COLLAPSING ||
           g->collapse_ticks <= 30u || g->collapse_ticks >= 150u;
}
static inline int sb_horizontal_overlap(const sb_game_t* g, uint8_t id) {
    const sb_platform_t* p = &sb_course_platforms(g)[id];
    return sb_abs(g->x - sb_platform_x(g, id)) < SB_F(p->half_x) + SB_PLAYER_HALF &&
           sb_abs(g->z - SB_F(p->z)) < SB_F(p->half_z) + SB_PLAYER_HALF;
}
/* AABB contact is useful for side/head collision, but is too permissive
 * for GROUNDING: it allowed the cube centre to move completely outside the
 * platform while its outermost corner still overlapped. From an oblique
 * camera it then looked as if the cube floated in front of the slab.
 * Keep nearly the whole footprint on the deck; coyote time handles edges. */
static inline int sb_supported_footprint(const sb_game_t* g, uint8_t id) {
    const sb_platform_t* p=&sb_course_platforms(g)[id];
    const int32_t safe_half_x=SB_F(p->half_x)-SB_PLAYER_HALF+SB_HALF;
    const int32_t safe_half_z=SB_F(p->half_z)-SB_PLAYER_HALF+SB_HALF;
    return sb_abs(g->x-sb_platform_x(g,id))<=safe_half_x &&
           sb_abs(g->z-SB_F(p->z))<=safe_half_z;
}
/* Place the orbit camera at one CONSTANT radius in the horizontal plane.
 * The previous x/z arm lengths (33 vs 43) caused the perspective/zoom to
 * change while rotating, making the avatar apparently slip across the deck. */
static inline void sb_camera_offset(int32_t fx,int32_t fz,
                                    int32_t* eye_x,int32_t* eye_z,
                                    int32_t* look_x,int32_t* look_z) {
    *eye_x=-sb_mul(fx,SB_F(42));
    *eye_z=-sb_mul(fz,SB_F(42));
    *look_x=sb_mul(fx,SB_F(8));
    *look_z=sb_mul(fz,SB_F(8));
}

/* The gems are octahedra hovering at (deck top + 4), with a +/- 0.5
 * unit visual bob. Test the player's actual world-space 3D AABB against
 * a gem's horizontal sphere and conservative vertical bob envelope.
 * The check is independent of support/grounding or camera orientation,
 * works during jumps, and follows the moving deck's current X position. */
static inline int sb_gem_contact(const sb_game_t* g,uint8_t id) {
    int32_t dx,dz;
    int32_t deck_y;
    if(id<1u || id>SB_PICKUP_COUNT || !sb_platform_active(g,id))
        return 0;
    deck_y=sb_platform_y(g,id);
    if(g->y+SB_PLAYER_HEIGHT < deck_y+SB_F(1) ||
       g->y > deck_y+SB_F(7))
        return 0;
    dx=sb_abs(g->x-sb_platform_x(g,id))-SB_PLAYER_HALF;
    dz=sb_abs(g->z-SB_F(sb_course_platforms(g)[id].z))-SB_PLAYER_HALF;
    if(dx<0)dx=0;
    if(dz<0)dz=0;
    return (int64_t)dx*dx+(int64_t)dz*dz <=
           (int64_t)SB_GEM_RADIUS*SB_GEM_RADIUS;
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
    g->z = SB_F(sb_course_platforms(g)[cp].z - 4);
    g->y = sb_platform_y(g,cp);
    g->vx = g->vy = g->vz = 0;
    g->support = (int8_t)cp;
    g->coyote = 5u;
    g->jump_buffer = 0u;
    g->collapse_ticks = 0u;
    /* Respawns retain the visual facing; first boot starts toward camera. */
}
static inline void sb_init(sb_game_t* g) {
    *g = (sb_game_t){0};
    g->moving_x = sb_moving_offset(0u);
    g->facing_z=-1;
    sb_respawn(g);
}
static inline void sb_start_course(sb_game_t* g,uint8_t course) {
    sb_init(g);
    g->course=(uint8_t)(course%SB_COURSE_COUNT);
    sb_respawn(g);
}
static inline void sb_side_collide(sb_game_t* g, int32_t old_x, int32_t old_z) {
    uint8_t i;
    for (i=0u; i<SB_PLATFORM_COUNT; ++i) {
        const sb_platform_t* p = &sb_course_platforms(g)[i];
        const int32_t cx = sb_platform_x(g, i), cz = SB_F(p->z);
        const int32_t left = cx - SB_F(p->half_x) - SB_PLAYER_HALF;
        const int32_t right = cx + SB_F(p->half_x) + SB_PLAYER_HALF;
        const int32_t back = cz - SB_F(p->half_z) - SB_PLAYER_HALF;
        const int32_t front = cz + SB_F(p->half_z) + SB_PLAYER_HALF;
        if (!sb_platform_active(g,i) || g->y >= sb_platform_y(g,i) - SB_F(1)/16 ||
            g->y + SB_PLAYER_HEIGHT <= sb_platform_y(g,i)-SB_F(5)) continue;
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
/* In the Saturn camera's right-handed look-at basis, +Z forward means
 * -X on the right side of the screen; reverse signs at this single point. */
static inline void sb_camera_right(int32_t fx,int32_t fz,
                                   int32_t* rx,int32_t* rz) {
    *rx=-fz;
    *rz=fx;
}
/* Update only when one horizontal axis is clearly dominant. Momentum
 * near a diagonal or low speed cannot rapidly flip the pig's head between
 * axes. Stopping, jumping and camera-only turns retain its last facing. */
static inline void sb_update_facing(sb_game_t* g) {
    const int32_t ax=sb_abs(g->vx),az=sb_abs(g->vz);
    const int32_t threshold=SB_F(1)/4,margin=SB_F(1)/8;
    if(ax>threshold && ax>az+margin) {
        g->facing_x=(int8_t)(g->vx>0?1:-1);
        g->facing_z=0;
    } else if(az>threshold && az>ax+margin) {
        g->facing_x=0;
        g->facing_z=(int8_t)(g->vz>0?1:-1);
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
        if(old_support>=0 && sb_platform_active(g,(uint8_t)old_support)) {
            uint8_t support_id=(uint8_t)old_support;
            uint8_t kind=sb_course_platforms(g)[support_id].kind;
            if(kind==SB_MOVING)g->x+=g->moving_x-old_move;
            if(kind==SB_LIFT) {
                /* Carry the rider at the same instant the deck rises/falls,
                 * BEFORE gravity and the landing sweep use the new top. */
                int32_t old_top=sb_platform_y_at(g,support_id,g->ticks-1u);
                int32_t new_top=sb_platform_y(g,support_id);
                g->y+=new_top-old_top;
            }
        }
    }
    if (g->collapse_ticks) {
        ++g->collapse_ticks;
        if (g->collapse_ticks >= 150u) g->collapse_ticks=0u;
    }
    if (old_support>=0 &&
        sb_course_platforms(g)[(uint8_t)old_support].kind==SB_COLLAPSING &&
        g->collapse_ticks==0u) {
        g->collapse_ticks=1u;
        event|=SB_EVENT_WARNING;
    }
    if (old_support>=0 && !sb_platform_active(g,(uint8_t)old_support)) {
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
    sb_update_facing(g);
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
            int32_t top=sb_platform_y(g,i);
            int32_t previous_top=sb_platform_y_at(g,i,g->ticks-1u);
            if (!sb_platform_active(g,i) || !sb_supported_footprint(g,i)) continue;
            if (old_y>=previous_top-SB_F(1)/8 && g->y<=top && top>best_y) {
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
            int32_t underside=sb_platform_y(g,i)-SB_F(5);
            if (!sb_platform_active(g,i) || !sb_horizontal_overlap(g,i)) continue;
            if (old_y+SB_PLAYER_HEIGHT<=underside &&
                g->y+SB_PLAYER_HEIGHT>=underside) {
                g->y=underside-SB_PLAYER_HEIGHT; g->vy=0;
                break;
            }
        }
    }
    /* Collect against the gem's actual position, not an arbitrary
     * +/-6-unit square or the identifier of the ground beneath the player.
     * Airborne pickups and gems riding moving platforms are supported. */
    for(i=1u;i<=SB_PICKUP_COUNT;++i) {
        uint16_t bit=(uint16_t)(1u<<(i-1u));
        if(!(g->pickups&bit) && sb_gem_contact(g,i)) {
            g->pickups|=bit;
            event|=SB_EVENT_PICKUP;
        }
    }
    if(g->support>=0) {
        uint8_t id=(uint8_t)g->support;
        if ((id==3u || id==6u) && g->checkpoint<id) {
            g->checkpoint=id;
            event|=SB_EVENT_CHECKPOINT;
        }
        /* Gems remain optional: reaching the finish platform is enough. */
        if(id==9u) {
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
