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
#define SB_COURSE_COUNT 4u
#define SB_SEESAW_LIMIT SB_F(3)
#define SB_SEESAW_SPEED (SB_F(1)/8)
#define SB_SEESAW_RETURN (SB_F(1)/20)
#define SB_COURSE3_HOLE_COUNT 6u
#define SB_PICKUP_COUNT 8u
#define SB_GEM_RADIUS SB_F(2)
#define SB_GEM_BASE_OFFSET SB_F(4)
#define SB_GEM_HALF_HEIGHT SB_F(2)

enum { SB_UP=1u, SB_DOWN=2u, SB_LEFT=4u, SB_RIGHT=8u, SB_JUMP=16u, SB_BRAKE=32u };
enum { SB_EVENT_JUMP=1u, SB_EVENT_LAND=2u, SB_EVENT_PICKUP=4u,
       SB_EVENT_CHECKPOINT=8u, SB_EVENT_FALL=16u, SB_EVENT_WIN=32u,
       SB_EVENT_WARNING=64u };
enum { SB_FIXED=0u, SB_MOVING=1u, SB_COLLAPSING=2u, SB_LIFT=3u, SB_SEESAW=4u };
enum { SB_SURFACE_NORMAL=0u, SB_SURFACE_SLICK=1u,
       SB_SURFACE_GRIP=2u, SB_SURFACE_AIR=3u };

typedef struct sb_platform {
    int16_t x, y, z, half_x, half_z;
    uint8_t kind, surface;
} sb_platform_t;

static const sb_platform_t sb_stage[SB_PLATFORM_COUNT] = {
    { 0,  0,   0, 18, 18, SB_FIXED, SB_SURFACE_NORMAL}, /* Pier */
    { 0,  0,  36, 13, 13, SB_FIXED, SB_SURFACE_NORMAL},
    {14,  3,  68, 12, 12, SB_FIXED, SB_SURFACE_NORMAL},
    {15,  3, 102, 13, 13, SB_FIXED, SB_SURFACE_GRIP}, /* Checkpoint A */
    { 2,  5, 137, 13, 13, SB_MOVING, SB_SURFACE_NORMAL},
    {-7,  5, 169, 13, 12, SB_FIXED, SB_SURFACE_SLICK},
    { 0,  7, 202, 13, 12, SB_FIXED, SB_SURFACE_NORMAL}, /* Checkpoint B */
    { 9,  7, 235, 13, 12, SB_COLLAPSING, SB_SURFACE_NORMAL},
    { 9, 10, 266, 13, 12, SB_FIXED, SB_SURFACE_NORMAL},
    { 0, 11, 299, 18, 15, SB_FIXED, SB_SURFACE_NORMAL} /* Finish */
};

/* Second course: narrower landing zones, varied widths and four elevators.
 * Adjacent deck gaps are bounded for the original jump speed; checkpoints
 * remain at stages 3 and 6. No new assets or expansion RAM are required. */
static const sb_platform_t sb_stage_two[SB_PLATFORM_COUNT] = {
    {  0, 0,   0, 20, 17, SB_FIXED, SB_SURFACE_NORMAL},
    { -2, 2,  29,  8, 11, SB_FIXED, SB_SURFACE_NORMAL},
    {  8, 4,  58,  9, 10, SB_LIFT, SB_SURFACE_NORMAL},
    { 12, 8,  86,  7, 10, SB_FIXED, SB_SURFACE_GRIP},
    {  1, 8, 113, 12,  9, SB_LIFT, SB_SURFACE_NORMAL},
    { -9,12, 141,  7, 11, SB_FIXED, SB_SURFACE_NORMAL},
    { -2,14, 169, 11, 11, SB_LIFT, SB_SURFACE_NORMAL},
    {  6,16, 196,  7,  9, SB_FIXED, SB_SURFACE_SLICK},
    { 12,18, 223, 10,  8, SB_LIFT, SB_SURFACE_NORMAL},
    {  0,20, 251, 17, 15, SB_FIXED, SB_SURFACE_NORMAL}
};

/* Third course uses wide, long solid piers with genuine rectangular
 * openings, not black painted decals. A hole is local to a platform, so
 * all collision and render geometry use the same immutable coordinates. */
static const sb_platform_t sb_stage_three[SB_PLATFORM_COUNT] = {
    {  0, 0,   0, 18, 24, SB_FIXED, SB_SURFACE_NORMAL},
    {  0, 0,  50, 17, 27, SB_FIXED, SB_SURFACE_NORMAL},
    {  4, 0, 100, 16, 27, SB_FIXED, SB_SURFACE_NORMAL},
    {  4, 0, 150, 19, 27, SB_FIXED, SB_SURFACE_GRIP},
    { -3, 0, 200, 17, 28, SB_FIXED, SB_SURFACE_NORMAL},
    { -3, 0, 250, 16, 27, SB_FIXED, SB_SURFACE_NORMAL},
    {  2, 0, 300, 19, 27, SB_FIXED, SB_SURFACE_GRIP},
    {  2, 0, 350, 17, 28, SB_FIXED, SB_SURFACE_NORMAL},
    {  0, 0, 400, 17, 27, SB_FIXED, SB_SURFACE_NORMAL},
    {  0, 0, 450, 20, 27, SB_FIXED, SB_SURFACE_NORMAL}
};
/* Course 4: six genuinely tilting boards with stable checkpoint piers.
 * Successive boards overlap slightly to remain reachable using the
 * original jump, while the hinge is always at the deck's central Y. */
static const sb_platform_t sb_stage_four[SB_PLATFORM_COUNT] = {
    {  0,0,   0,16,17,SB_FIXED,  SB_SURFACE_NORMAL},
    {  0,0,  33,12,20,SB_SEESAW,SB_SURFACE_NORMAL},
    {  2,1,  71,11,20,SB_SEESAW,SB_SURFACE_NORMAL},
    {  2,2, 108,16,17,SB_FIXED,  SB_SURFACE_GRIP},
    { -2,2, 144,12,21,SB_SEESAW,SB_SURFACE_NORMAL},
    { -1,3, 184,11,21,SB_SEESAW,SB_SURFACE_NORMAL},
    {  0,4, 222,16,17,SB_FIXED,  SB_SURFACE_GRIP},
    {  3,4, 259,12,21,SB_SEESAW,SB_SURFACE_NORMAL},
    {  3,5, 299,11,21,SB_SEESAW,SB_SURFACE_NORMAL},
    {  0,6, 338,18,17,SB_FIXED,  SB_SURFACE_NORMAL}
};

typedef struct sb_hole {
    uint8_t platform; /* index into Course 3's shared deck table */
    int16_t x_offset, z_offset, half_x, half_z;
} sb_hole_t;
/* Hole 2 spans most of the walkway: the side rails are too narrow to
 * cross with the whole 4-unit pig footprint, encouraging a short jump.
 * The remaining holes alternate sides, with an optional safe path around. */
static const sb_hole_t sb_course3_holes[SB_COURSE3_HOLE_COUNT] = {
    {1u, -7,  8, 5, 6},
    {2u,  0, 10,12, 5},
    {4u,  8, -9, 5, 5},
    {5u, -8, 10, 5, 5},
    {7u,  0, 10,12, 5},
    {8u,  8, -9, 5, 5}
};
typedef struct sb_deck_slice {
    int32_t min_x,max_x,min_z,max_z;
} sb_deck_slice_t;

typedef struct sb_game {
    int32_t x, y, z, vx, vy, vz;
    int32_t moving_x;
    int32_t seesaw_tilt[SB_PLATFORM_COUNT]; /* +/-3 world units at each Z end */
    uint32_t ticks;
    uint16_t pickups;            /* optional, 8-bit collectible mask */
    uint16_t collapse_ticks;     /* 0=ready, 1..30 warning, 31..150 absent */
    uint8_t checkpoint;          /* stage index 0, 3 or 6 */
    uint8_t course;              /* 0=original, 1=elevators, 2=holes, 3=seesaws */
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
static inline const sb_platform_t* sb_course_platforms(const sb_game_t* g) {
    return g->course==3u?sb_stage_four:
           (g->course==2u?sb_stage_three:
           (g->course==1u?sb_stage_two:sb_stage));
}
/* Surface coefficients are explicit per platform; the original opening
 * decks (including the fixed second deck) retain their old handling.
 * All magnitudes are 16.16 world units per 60 Hz simulation tick. */
static inline uint8_t sb_ground_surface(const sb_game_t* g,int8_t support) {
    return support>=0 && support<(int8_t)SB_PLATFORM_COUNT
        ? sb_course_platforms(g)[(uint8_t)support].surface
        : SB_SURFACE_AIR;
}
static inline int32_t sb_surface_friction(uint8_t surface) {
    if(surface==SB_SURFACE_SLICK)return SB_F(1)/20;
    if(surface==SB_SURFACE_GRIP)return SB_F(2)/5;
    if(surface==SB_SURFACE_AIR)return SB_F(1)/12;
    return SB_F(1)/5;
}
/* Reusable fixed-point velocity control: no proportional drag that leaves
 * a tiny forever-sliding residual, and no sign flip when braking crosses 0. */
static inline int32_t sb_approach(int32_t value,int32_t target,int32_t amount) {
    if(value<target)return value+sb_clamp(target-value,0,amount);
    if(value>target)return value-sb_clamp(value-target,0,amount);
    return value;
}
static inline int32_t sb_move_axis(int32_t velocity,int32_t desired,
                                  uint8_t surface,uint8_t brake) {
    int32_t rate;
    if(brake) {
        rate=surface==SB_SURFACE_AIR?SB_F(1)/8:SB_F(3)/5;
        return sb_approach(velocity,0,rate);
    }
    if(desired==0)return sb_approach(velocity,0,
                                      sb_surface_friction(surface));
    if((velocity<0 && desired>0)||(velocity>0 && desired<0))
        rate=surface==SB_SURFACE_AIR?SB_F(1)/12:SB_F(2)/5;
    else
        rate=surface==SB_SURFACE_AIR?SB_F(1)/12:SB_F(1)/5;
    return sb_approach(velocity,desired,rate);
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
/* A seesaw's deck is a single planar surface. Its signed tilt is exactly
 * the height at the +Z end relative to its hinge; the -Z end has -tilt.
 * Collision, gem placement, shadows and rendered corner vertices must
 * all use this same function, including when the pig walks uphill. */
static inline int32_t sb_platform_surface_y(
    const sb_game_t* g,uint8_t id,int32_t x,int32_t z) {
    const sb_platform_t* p=&sb_course_platforms(g)[id];
    int32_t base=sb_platform_y(g,id);
    (void)x;
    if(p->kind!=SB_SEESAW)return base;
    int32_t dz=sb_clamp(z-SB_F(p->z),
                        -SB_F(p->half_z),SB_F(p->half_z));
    return base+(int32_t)((int64_t)g->seesaw_tilt[id]*dz/SB_F(p->half_z));
}
static inline void sb_update_seesaws(sb_game_t* g,int8_t rider) {
    for(uint8_t id=0u;id<SB_PLATFORM_COUNT;++id) {
        const sb_platform_t* p=&sb_course_platforms(g)[id];
        if(p->kind!=SB_SEESAW)continue;
        int32_t target=0,rate=SB_SEESAW_RETURN;
        if(rider==(int8_t)id) {
            const int32_t dz=sb_clamp(g->z-SB_F(p->z),
                                     -SB_F(p->half_z),SB_F(p->half_z));
            target=-(int32_t)((int64_t)dz*SB_SEESAW_LIMIT/
                               SB_F(p->half_z));
            rate=SB_SEESAW_SPEED;
        }
        g->seesaw_tilt[id]=sb_approach(g->seesaw_tilt[id],target,rate);
    }
}

static inline int32_t sb_platform_x(const sb_game_t* g, uint8_t id) {
    const sb_platform_t* p=&sb_course_platforms(g)[id];
    return SB_F(p->x) + (p->kind==SB_MOVING ? g->moving_x : 0);
}
static inline const sb_hole_t* sb_platform_hole(const sb_game_t* g,
                                                uint8_t id) {
    if(g->course!=2u)return 0;
    for(uint8_t i=0u;i<SB_COURSE3_HOLE_COUNT;++i)
        if(sb_course3_holes[i].platform==id)return &sb_course3_holes[i];
    return 0;
}
/* Maximum four non-overlapping rectangular SOLID slabs around an opening.
 * The exact same bounds are used to render the platform and to determine
 * floor support; the hole itself never gets a top or underside polygon.
 * Slices omit zero-area cells and are returned in a fixed, stable order. */
static inline uint8_t sb_deck_slices(const sb_game_t* g,uint8_t id,
                                    sb_deck_slice_t out[4]) {
    const sb_platform_t* p=&sb_course_platforms(g)[id];
    const int32_t cx=sb_platform_x(g,id),cz=SB_F(p->z);
    const int32_t lx=cx-SB_F(p->half_x),rx=cx+SB_F(p->half_x);
    const int32_t bz=cz-SB_F(p->half_z),fz=cz+SB_F(p->half_z);
    const sb_hole_t* h=sb_platform_hole(g,id);
    if(!h) {
        out[0]=(sb_deck_slice_t){lx,rx,bz,fz};
        return 1u;
    }
    const int32_t hl=cx+SB_F(h->x_offset-h->half_x);
    const int32_t hr=cx+SB_F(h->x_offset+h->half_x);
    const int32_t hb=cz+SB_F(h->z_offset-h->half_z);
    const int32_t hf=cz+SB_F(h->z_offset+h->half_z);
    uint8_t count=0u;
    if(lx<hl)out[count++]=(sb_deck_slice_t){lx,hl,bz,fz};
    if(hr<rx)out[count++]=(sb_deck_slice_t){hr,rx,bz,fz};
    if(bz<hb)out[count++]=(sb_deck_slice_t){hl,hr,bz,hb};
    if(hf<fz)out[count++]=(sb_deck_slice_t){hl,hr,hf,fz};
    return count;
}
/* No invisible collision over the aperture. Nearly the entire pig's
 * footprint must fit within one solid slab; this conservatively avoids
 * balancing on disconnected strips or standing on empty central space. */
static inline int sb_deck_footprint(const sb_game_t* g,uint8_t id) {
    sb_deck_slice_t pieces[4];
    uint8_t count=sb_deck_slices(g,id,pieces);
    for(uint8_t i=0u;i<count;++i) {
        const int32_t inset=SB_PLAYER_HALF-SB_HALF;
        if(g->x>=pieces[i].min_x+inset &&
           g->x<=pieces[i].max_x-inset &&
           g->z>=pieces[i].min_z+inset &&
           g->z<=pieces[i].max_z-inset)
            return 1;
    }
    return 0;
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
    return sb_deck_footprint(g,id);
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
    deck_y=sb_platform_surface_y(g,id,sb_platform_x(g,id),
                                 SB_F(sb_course_platforms(g)[id].z));
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
    g->y = sb_platform_surface_y(g,cp,g->x,g->z);
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
        if (!sb_platform_active(g,i) ||
            (sb_platform_hole(g,i) && !sb_deck_footprint(g,i)) ||
            g->y >= sb_platform_surface_y(g,i,g->x,g->z) - SB_F(1)/16 ||
            g->y + SB_PLAYER_HEIGHT <=
                sb_platform_surface_y(g,i,g->x,g->z)-SB_F(5)) continue;
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
        uint8_t surface=sb_ground_surface(g,old_support);
        uint8_t brake=(uint8_t)((held&SB_BRAKE)!=0u);
        g->vx=sb_move_axis(g->vx,target_x,surface,brake);
        g->vz=sb_move_axis(g->vz,target_z,surface,brake);
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
    /* The board reacts to where the rider stands this tick, not where
     * they stood one frame earlier. A grounded pig follows the newly
     * tilted plane both uphill and downhill; a jump immediately detaches. */
    sb_update_seesaws(g,old_support);
    if(old_support>=0 &&
       sb_course_platforms(g)[(uint8_t)old_support].kind==SB_SEESAW &&
       g->support==(int8_t)old_support &&
       sb_supported_footprint(g,(uint8_t)old_support) && g->vy<=0) {
        g->y=sb_platform_surface_y(
            g,(uint8_t)old_support,g->x,g->z);
        g->vy=0;
    }
    g->y+=g->vy;
    best_y=-SB_F(100);
    if (g->vy<=0) {
        for (i=0u;i<SB_PLATFORM_COUNT;++i) {
            int32_t top=sb_platform_surface_y(g,i,g->x,g->z);
            int32_t previous_top=sb_platform_y_at(g,i,g->ticks-1u);
            if(sb_course_platforms(g)[i].kind==SB_SEESAW)
                previous_top=old_y; /* last grounded height before moving */
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
            int32_t underside=sb_platform_surface_y(g,i,g->x,g->z)-SB_F(5);
            if (!sb_platform_active(g,i) || !sb_horizontal_overlap(g,i) ||
                (sb_platform_hole(g,i) && !sb_deck_footprint(g,i))) continue;
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
