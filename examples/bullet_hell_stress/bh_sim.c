#include "bh_sim.h"

#include "saturn/collide2d.h"

/* See bh_sim.h. Plain C that also builds as C++ for the host tests. */

#define FP BH_FP

enum { FLAG_ARRIVED = 1, FLAG_RETREAT = 2, FLAG_DEAD = 4 };

/* ------------------------------------------------------------------ trig */

static const int16_t kQuarterSin[65] = {
    0, 804, 1608, 2410, 3212, 4011, 4808, 5602, 6393, 7179, 7962, 8739, 9512, 10278, 11039, 11793,
    12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530, 18204, 18868, 19519, 20159, 20787, 21403,
    22005, 22594, 23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790, 27245, 27683, 28105, 28510,
    28898, 29268, 29621, 29956, 30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971, 32137, 32285,
    32412, 32521, 32609, 32678, 32728, 32757, 32767};

/* atan(i / 32) in 1/16 angle units (256 angle units = a turn). */
static const uint16_t kAtan16[33] = {
    0, 20, 41, 61, 81, 101, 121, 140, 160, 179, 197, 216, 234, 252, 269, 286, 302,
    318, 334, 349, 364, 379, 393, 406, 419, 432, 445, 457, 469, 480, 491, 502, 512};

int32_t bh_sin(uint32_t a) {
    a &= 255u;
    const uint32_t i = a & 63u;
    switch (a >> 6) {
    case 0: return kQuarterSin[i];
    case 1: return kQuarterSin[64u - i];
    case 2: return -kQuarterSin[i];
    default: return -kQuarterSin[64u - i];
    }
}
int32_t bh_cos(uint32_t a) { return bh_sin(a + 64u); }

/* Angle of a vector in the first octant pair: atan(small / large), 0..32. */
static uint32_t atan_ratio(int32_t small, int32_t large) {
    if (large == 0) return 0;
    const int32_t z = (small << 10) / large; /* 0..1024 */
    const int32_t i = z >> 5, f = z & 31;
    const int32_t a16 = (int32_t)kAtan16[i] + (((int32_t)kAtan16[i + (i < 32)] - (int32_t)kAtan16[i]) * f >> 5);
    return (uint32_t)((a16 + 8) >> 4);
}

uint8_t bh_atan2(int32_t dy, int32_t dx) {
    const int32_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    if (ax == 0 && ay == 0) return 64;
    const uint32_t a = ay <= ax ? atan_ratio(ay, ax) : 64u - atan_ratio(ax, ay);
    uint32_t r;
    if (dx >= 0) r = dy >= 0 ? a : 256u - a;
    else r = dy >= 0 ? 128u - a : 128u + a;
    return (uint8_t)r;
}

static uint32_t rnd(bh_sim_t* s) {
    s->rng = s->rng * 1664525u + 1013904223u;
    return s->rng >> 16;
}

/* ---------------------------------------------------------------- params */

void bh_params_default(bh_params_t* p) {
    static const uint8_t bullet[BH_SHAPES] = {4, 4, 4, 4, 3, 3};
    static const uint8_t enemy[BH_ENEMY_TYPES] = {6, 9, 8, 6, 24};
    __builtin_memcpy(p->bullet_radius, bullet, sizeof(bullet));
    __builtin_memcpy(p->enemy_radius, enemy, sizeof(enemy));
    p->shot_radius[0] = 4;
    p->shot_radius[1] = 6;
    p->player_radius = 2;
    p->pickup_radius = 4;
    p->stage_frames = 2700;
    p->boss_frame = 1500;
}

/* ----------------------------------------------------------------- pools */

static const uint8_t kStageColors[BH_STAGES][2] = {
    {BH_COLOR_RED, BH_COLOR_ORANGE}, {BH_COLOR_VIOLET, BH_COLOR_PINK}, {BH_COLOR_BLUE, BH_COLOR_GREEN}};

static int spawn_bullet(bh_sim_t* s, int32_t x, int32_t y, uint32_t angle, int32_t speed, uint8_t shape,
                        uint8_t color) {
    if (s->bullet_count >= BH_MAX_BULLETS) {
        s->stats.bullets_dropped++;
        return 0;
    }
    bh_bullet_t* b = &s->bullets[s->bullet_count++];
    b->x = x;
    b->y = y;
    b->vx = (int16_t)((bh_cos(angle) * speed) >> 15);
    b->vy = (int16_t)((bh_sin(angle) * speed) >> 15);
    b->shape = shape;
    b->color = color;
    b->age = 0;
    s->stats.bullets_spawned++;
    if (s->bullet_count > s->stats.peak_bullets) s->stats.peak_bullets = s->bullet_count;
    return 1;
}

static void spawn_explosion(bh_sim_t* s, int32_t x, int32_t y, uint8_t delay) {
    if (s->explosion_count >= BH_MAX_EXPLOSIONS) return;
    bh_explosion_t* e = &s->explosions[s->explosion_count++];
    e->x = x;
    e->y = y;
    e->frame = 0;
    e->tick = 0;
    e->delay = delay;
    e->pad = 0;
}

static void spawn_pickup(bh_sim_t* s, int32_t x, int32_t y, uint8_t type) {
    if (s->pickup_count >= BH_MAX_PICKUPS) return;
    bh_pickup_t* p = &s->pickups[s->pickup_count++];
    p->x = x;
    p->y = y;
    p->vy = -512; /* pops up, then falls */
    p->pad = 0;
    p->type = type;
    p->color = kStageColors[s->stage][type & 1];
    p->age = 0;
}

static void spawn_shot(bh_sim_t* s, int32_t x, int32_t y, int16_t vx, int16_t vy, uint8_t kind, uint8_t damage) {
    if (s->shot_count >= BH_MAX_SHOTS) return;
    bh_shot_t* sh = &s->shots[s->shot_count++];
    sh->x = x;
    sh->y = y;
    sh->vx = vx;
    sh->vy = vy;
    sh->kind = kind;
    sh->damage = damage;
    sh->pad0 = sh->pad1 = 0;
}

/* ---------------------------------------------------------------- enemies */

typedef struct wave {
    uint16_t t;
    uint8_t type, x, count, spacing, pattern;
} wave_t;

static const wave_t kWaves[] = {
    {60, BH_ENEMY_DRONE, 40, 5, 14, BH_PAT_AIMED},
    {200, BH_ENEMY_DRONE, 184, 5, 14, BH_PAT_FAN},
    {340, BH_ENEMY_TURRET, 56, 1, 0, BH_PAT_SPIRAL},
    {340, BH_ENEMY_TURRET, 168, 1, 0, BH_PAT_RING},
    {520, BH_ENEMY_INTERCEPTOR, 112, 3, 30, BH_PAT_RING},
    {700, BH_ENEMY_SCOUT, 0, 6, 16, BH_PAT_RAIN},
    {760, BH_ENEMY_SCOUT, 223, 6, 16, BH_PAT_AIMED},
    {900, BH_ENEMY_DRONE, 90, 4, 18, BH_PAT_FAN},
    {900, BH_ENEMY_DRONE, 134, 4, 18, BH_PAT_AIMED},
    {1040, BH_ENEMY_TURRET, 40, 1, 0, BH_PAT_SPIRAL},
    {1040, BH_ENEMY_TURRET, 112, 1, 0, BH_PAT_RING},
    {1040, BH_ENEMY_TURRET, 184, 1, 0, BH_PAT_SPIRAL},
    {1200, BH_ENEMY_INTERCEPTOR, 60, 2, 40, BH_PAT_AIMED},
    {1200, BH_ENEMY_INTERCEPTOR, 164, 2, 40, BH_PAT_FAN},
    {1320, BH_ENEMY_SCOUT, 0, 8, 12, BH_PAT_RAIN},
};

static const int16_t kBaseHp[BH_ENEMY_TYPES] = {5, 12, 18, 3, 380};

static int diff(const bh_sim_t* s) { return 1 + (int)s->stage; }

static uint16_t fire_period(const bh_sim_t* s, uint8_t pattern) {
    switch (pattern) {
    case BH_PAT_AIMED: return (uint16_t)(78 - diff(s) * 10);
    case BH_PAT_FAN: return 90;
    case BH_PAT_RING: return 100;
    case BH_PAT_SPIRAL: return 5;
    default: return 14;
    }
}

static void spawn_enemy(bh_sim_t* s, uint8_t type, int32_t x, int32_t y, uint8_t pattern, int16_t aux) {
    if (s->enemy_count >= BH_MAX_ENEMIES) return;
    bh_enemy_t* e = &s->enemies[s->enemy_count++];
    __builtin_memset(e, 0, sizeof(*e));
    e->x = x;
    e->y = y;
    e->anchor_x = x;
    e->type = type;
    e->pattern = pattern;
    e->aux = aux;
    e->color = (uint8_t)(rnd(s) & 1u);
    e->hp = (int16_t)(kBaseHp[type] + kBaseHp[type] * (int)s->stage / 2);
    e->fire = (uint16_t)(30 + (rnd(s) & 31u));
}

static void run_script(bh_sim_t* s) {
    const uint16_t t = s->stage_timer;
    for (unsigned i = 0; i < sizeof(kWaves) / sizeof(kWaves[0]); ++i) {
        const wave_t* w = &kWaves[i];
        if (t < w->t) continue;
        const uint16_t dt = (uint16_t)(t - w->t);
        const uint16_t spacing = w->spacing ? w->spacing : 1u;
        if (dt % spacing != 0u || dt / spacing >= w->count) continue;
        int32_t x = FP(w->x), y = FP(-24);
        int16_t aux = 0;
        if (w->type == BH_ENEMY_SCOUT) {
            const int from_left = w->x < 112;
            x = from_left ? FP(-14) : FP(238);
            aux = (int16_t)(56 + 34 * ((dt / spacing) % 3u));
            y = FP(aux);
        }
        spawn_enemy(s, w->type, x, y, w->pattern, aux);
    }
    if (!s->boss_spawned && t >= s->params.boss_frame) {
        s->boss_spawned = 1;
        spawn_enemy(s, BH_ENEMY_BOSS, FP(112), FP(-40), BH_PAT_SPIRAL, 0);
    }
}

static void enemy_fire(bh_sim_t* s, bh_enemy_t* e, uint8_t pattern) {
    const int d = diff(s);
    const uint8_t color = kStageColors[s->stage][e->color & 1u];
    const int big = e->type == BH_ENEMY_BOSS;
    switch (pattern) {
    case BH_PAT_AIMED: {
        const uint8_t a = bh_atan2(s->player.y - e->y, s->player.x - e->x);
        const int n = 2 + d;
        for (int i = 0; i < n; ++i) {
            spawn_bullet(s, e->x, e->y, (uint32_t)(a + ((2 * i - (n - 1)) * 5) / 2), 560 + d * 64, BH_SHAPE_ORB,
                         color);
        }
        break;
    }
    case BH_PAT_FAN: {
        const int n = 3 + 2 * d + (big ? 4 : 0);
        for (int i = 0; i < n; ++i) {
            spawn_bullet(s, e->x, e->y, (uint32_t)(64 + (2 * i - (n - 1)) * 3), 480 + d * 32, BH_SHAPE_NEEDLE,
                         color);
        }
        break;
    }
    case BH_PAT_RING: {
        const int n = 12 + 4 * d + (big ? 8 : 0);
        for (int i = 0; i < n; ++i) {
            spawn_bullet(s, e->x, e->y, (uint32_t)(e->aux + i * 256 / n), 400 + d * 32, BH_SHAPE_DIAMOND, color);
        }
        e->aux = (int16_t)(e->aux + 5);
        break;
    }
    case BH_PAT_SPIRAL: {
        const int arms = (d >= 2 ? 4 : 3) + (big ? 2 : 0);
        for (int i = 0; i < arms; ++i) {
            spawn_bullet(s, e->x, e->y, (uint32_t)(e->aux + i * 256 / arms), 380 + d * 24, BH_SHAPE_STAR, color);
        }
        e->aux = (int16_t)(e->aux + 9);
        break;
    }
    default:
        for (int i = 0; i < 2; ++i) {
            const int32_t ox = (int32_t)(rnd(s) % 17u) - 8;
            /* Keep the established GCC/SH-2 draw order explicitly: argument
             * evaluation order differs between GCC and Clang. */
            const int32_t speed = 520 + (int32_t)(rnd(s) & 255u);
            const uint32_t angle = 64u + rnd(s) % 9u - 4u;
            spawn_bullet(s, e->x + FP(ox), e->y, angle, speed,
                         BH_SHAPE_SHARD, color);
        }
        break;
    }
}

static void enemy_move(bh_sim_t* s, bh_enemy_t* e) {
    const uint32_t age = e->age;
    switch (e->type) {
    case BH_ENEMY_DRONE:
        e->y += 300;
        e->x = e->anchor_x + ((bh_sin(age * 3u) * FP(28)) >> 15);
        break;
    case BH_ENEMY_INTERCEPTOR:
        if (!(e->flags & FLAG_ARRIVED)) {
            e->y += 640;
            if (e->y >= FP(110)) {
                e->flags |= FLAG_ARRIVED;
                e->aux = (int16_t)age;
            }
        } else if (age > (uint32_t)e->aux + 150u) {
            e->flags |= FLAG_RETREAT;
            e->y -= 576;
        }
        break;
    case BH_ENEMY_TURRET:
        e->y += 128;
        break;
    case BH_ENEMY_SCOUT: {
        const int dir = e->anchor_x < FP(112) ? 1 : -1;
        e->x += dir * 512;
        e->y = FP(e->aux) + ((bh_sin(age * 4u) * FP(10)) >> 15);
        break;
    }
    default: /* boss */
        if (!(e->flags & FLAG_ARRIVED)) {
            e->y += 340;
            if (e->y >= FP(96)) {
                e->flags |= FLAG_ARRIVED;
                e->aux = 0;
                e->age = 0;
            }
        } else if (age > 2400u) {
            e->flags |= FLAG_RETREAT;
            e->y -= 256;
        } else {
            e->x = FP(112) + ((bh_sin(age * 2u) * FP(60)) >> 15);
            e->y = FP(96) + ((bh_sin(age * 3u) * FP(16)) >> 15);
        }
        break;
    }
    e->age++;
    (void)s;
}

static int enemy_offscreen(const bh_enemy_t* e) {
    if (e->flags & FLAG_RETREAT) return e->y < FP(-56);
    return e->y > FP(472) || e->x < FP(-40) || e->x > FP(264);
}

/* Boss pattern by phase; non-boss enemies fire their own pattern while on screen. */
static void enemy_think(bh_sim_t* s, bh_enemy_t* e) {
    if (s->steady_target) return; /* benchmark: bullets come from the top-up */
    if (e->y < FP(8) || (e->flags & FLAG_RETREAT)) return;
    if (e->type == BH_ENEMY_BOSS && !(e->flags & FLAG_ARRIVED)) return;
    uint8_t pattern = e->pattern;
    uint16_t period = fire_period(s, pattern);
    if (e->type == BH_ENEMY_BOSS) {
        static const uint8_t phases[4] = {BH_PAT_SPIRAL, BH_PAT_RING, BH_PAT_FAN, BH_PAT_AIMED};
        const uint32_t phase = (e->age / 240u) & 3u;
        pattern = phases[phase];
        period = pattern == BH_PAT_SPIRAL ? 3u : pattern == BH_PAT_RING ? 45u : pattern == BH_PAT_FAN ? 30u : 40u;
        if (pattern == BH_PAT_AIMED && (e->age % 10u) == 0u) enemy_fire(s, e, BH_PAT_RAIN);
    }
    if (e->fire > 0) {
        e->fire--;
        return;
    }
    e->fire = period;
    enemy_fire(s, e, pattern);
}

/* ----------------------------------------------------------------- player */

static void reset_player(bh_sim_t* s) {
    bh_player_t* p = &s->player;
    __builtin_memset(p, 0, sizeof(*p));
    p->x = FP(112);
    p->y = FP(400);
    p->alive = 1;
    p->bank = 2;
    p->lives = 3;
    p->bombs = 3;
    p->invuln = 120;
}

static void player_fire(bh_sim_t* s) {
    bh_player_t* p = &s->player;
    const int width = p->power >= 2 ? 3 : (int)p->power + 1; /* pairs of shots */
    for (int i = 0; i < width; ++i) {
        const int off = 5 + 7 * i;
        const int16_t spread = (int16_t)(i * 48);
        spawn_shot(s, p->x - FP(off), p->y - FP(16), (int16_t)-spread, -2560, 0, 1);
        spawn_shot(s, p->x + FP(off), p->y - FP(16), spread, -2560, 0, 1);
    }
    for (int i = 0; i < bh_option_count(p); ++i) {
        int32_t ox, oy;
        bh_option_pos(p, i, &ox, &oy);
        spawn_shot(s, ox, oy - FP(20), 0, -3072, 1, 2);
    }
}

static void player_update(bh_sim_t* s, uint16_t in) {
    bh_player_t* p = &s->player;
    if (p->invuln) p->invuln--;
    if (!p->alive) {
        if (s->game_over) return;
        if (p->respawn && --p->respawn == 0) {
            p->alive = 1;
            p->x = FP(112);
            p->y = FP(400);
            p->invuln = 180;
            p->power = p->power > 1 ? (uint8_t)(p->power - 1) : p->power;
        }
        return;
    }
    p->focus = (in & BH_IN_FOCUS) ? 1u : 0u;
    int32_t speed = p->focus ? 384 : 768;
    const int h = ((in & BH_IN_RIGHT) ? 1 : 0) - ((in & BH_IN_LEFT) ? 1 : 0);
    const int v = ((in & BH_IN_DOWN) ? 1 : 0) - ((in & BH_IN_UP) ? 1 : 0);
    if (h && v) speed = (speed * 181) >> 8;
    p->x += h * speed;
    p->y += v * speed;
    if (p->x < FP(12)) p->x = FP(12);
    if (p->x > FP(212)) p->x = FP(212);
    if (p->y < FP(16)) p->y = FP(16);
    if (p->y > FP(432)) p->y = FP(432);

    if (h > 0) p->steer = (int16_t)(p->steer < 0 ? p->steer + 2 : (p->steer < 12 ? p->steer + 1 : 12));
    else if (h < 0) p->steer = (int16_t)(p->steer > 0 ? p->steer - 2 : (p->steer > -12 ? p->steer - 1 : -12));
    else p->steer = (int16_t)(p->steer > 0 ? p->steer - 1 : (p->steer < 0 ? p->steer + 1 : 0));
    p->bank = (uint8_t)(2 + (p->steer >= 10 ? 2 : p->steer >= 4 ? 1 : p->steer <= -10 ? -2 : p->steer <= -4 ? -1 : 0));

    if (p->shot_cooldown) p->shot_cooldown--;
    if ((in & BH_IN_SHOT) && p->shot_cooldown == 0) {
        p->shot_cooldown = 4;
        player_fire(s);
    }
    if ((in & BH_IN_BOMB) && !(s->prev_input & BH_IN_BOMB) && p->bombs > 0) {
        p->bombs--;
        s->bomb_timer = 60;
        if (p->invuln < 150) p->invuln = 150;
        s->bullet_count = 0;
        for (unsigned i = 0; i < s->enemy_count; ++i) {
            s->enemies[i].hp = (int16_t)(s->enemies[i].hp - 25);
            spawn_explosion(s, s->enemies[i].x, s->enemies[i].y, (uint8_t)(i & 7u));
        }
    }
}

static void player_die(bh_sim_t* s) {
    bh_player_t* p = &s->player;
    spawn_explosion(s, p->x, p->y, 0);
    spawn_explosion(s, p->x, p->y, 4);
    p->alive = 0;
    p->steer = 0;
    p->bank = 2;
    if (p->lives > 0) p->lives--;
    s->bullet_count = 0; /* the field is cleared on a death */
    if (p->lives == 0) {
        s->game_over = 1;
        s->game_over_timer = 180;
    } else {
        p->respawn = 60;
    }
}

/* -------------------------------------------------------------- collision */

uint16_t bh_bullets_hitting_player(const bh_sim_t* s, uint16_t* first) {
    int32_t r[BH_SHAPES];
    for (int i = 0; i < BH_SHAPES; ++i) r[i] = ((int32_t)s->params.bullet_radius[i] + s->params.player_radius) * 256;
    const int32_t px = s->player.x, py = s->player.y;
    uint16_t hits = 0;
    const bh_bullet_t* b = s->bullets;
    for (uint16_t i = 0; i < s->bullet_count; ++i, ++b) {
        const int32_t rr = r[b->shape];
        const int32_t dx = b->x - px;
        if (dx >= rr || dx <= -rr) continue;
        const int32_t dy = b->y - py;
        if (bh_circle_hit(dx, dy, rr)) {
            if (hits == 0 && first) *first = i;
            hits++;
        }
    }
    return hits;
}

static sat_box2_t box_of(int32_t x, int32_t y, int32_t radius_px) {
    sat_box2_t b;
    b.center.x = x << 8;
    b.center.y = y << 8;
    b.half.x = radius_px << 16;
    b.half.y = radius_px << 16;
    return b;
}

static int enemy_hittable(const bh_enemy_t* e) { return e->y >= FP(4) && !(e->flags & (FLAG_DEAD | FLAG_RETREAT)); }

uint16_t bh_find_shot_hits(bh_sim_t* s, bh_hit_pair_t* out, uint16_t cap) {
    sat_spatial_clear(&s->spatial);
    for (uint16_t i = 0; i < s->enemy_count; ++i) {
        const bh_enemy_t* e = &s->enemies[i];
        if (!enemy_hittable(e)) continue;
        const sat_box2_t box = box_of(e->x, e->y, s->params.enemy_radius[e->type]);
        if (sat_spatial_insert(&s->spatial, i, &box) != SAT_OK) s->stats.spatial_overflow++;
    }
    uint16_t n = 0;
    uint16_t ids[BH_MAX_ENEMIES];
    for (uint16_t i = 0; i < s->shot_count; ++i) {
        const bh_shot_t* sh = &s->shots[i];
        const int32_t sr = s->params.shot_radius[sh->kind ? 1 : 0];
        const sat_box2_t box = box_of(sh->x, sh->y, sr);
        uint16_t found = 0;
        sat_spatial_query(&s->spatial, &box, ids, BH_MAX_ENEMIES, &found);
        for (uint16_t k = 0; k < found; ++k) {
            const bh_enemy_t* e = &s->enemies[ids[k]];
            sat_circle_t a, c;
            a.center = box.center;
            a.radius = sr << 16;
            c.center.x = e->x << 8;
            c.center.y = e->y << 8;
            c.radius = (int32_t)s->params.enemy_radius[e->type] << 16;
            if (!sat_circle_overlap(&a, &c)) continue;
            if (n < cap) {
                out[n].shot = i;
                out[n].enemy = ids[k];
            }
            n++;
        }
    }
    return n < cap ? n : cap;
}

static void kill_enemy(bh_sim_t* s, const bh_enemy_t* e) {
    const int boss = e->type == BH_ENEMY_BOSS;
    s->stats.enemies_killed++;
    s->score += boss ? 20000u : (e->type == BH_ENEMY_TURRET ? 600u : 300u) * (uint32_t)diff(s);
    if (boss) {
        for (int i = 0; i < 10; ++i) {
            const int32_t ox = (int32_t)(rnd(s) % 61u) - 30, oy = (int32_t)(rnd(s) % 41u) - 20;
            spawn_explosion(s, e->x + FP(ox), e->y + FP(oy), (uint8_t)(i * 4));
        }
        for (int i = 0; i < 4; ++i) spawn_pickup(s, e->x + FP(i * 10 - 15), e->y, BH_PICKUP_POWER);
        spawn_pickup(s, e->x, e->y, BH_PICKUP_BOMB);
        s->boss_dead = 1;
        s->stage_end_delay = 150;
        s->bullet_count = 0;
        return;
    }
    spawn_explosion(s, e->x, e->y, 0);
    const uint32_t r = rnd(s) & 15u;
    if (r < 3u) spawn_pickup(s, e->x, e->y, BH_PICKUP_POWER);
    else if (r == 3u && s->player.bombs < 5u) spawn_pickup(s, e->x, e->y, BH_PICKUP_BOMB);
    else if (r < 8u) spawn_pickup(s, e->x, e->y, BH_PICKUP_SCORE);
}

static void apply_shot_hits(bh_sim_t* s) {
    bh_hit_pair_t pairs[BH_MAX_SHOTS * 2];
    const uint16_t n = bh_find_shot_hits(s, pairs, BH_MAX_SHOTS * 2);
    for (uint16_t i = 0; i < n; ++i) {
        bh_shot_t* sh = &s->shots[pairs[i].shot];
        if (sh->damage == 0) continue; /* already spent on another enemy */
        s->enemies[pairs[i].enemy].hp = (int16_t)(s->enemies[pairs[i].enemy].hp - sh->damage);
        s->score += 10u;
        s->stats.shot_hits++;
        sh->damage = 0;
    }
    /* Drop spent shots (swap-remove). */
    for (uint16_t i = 0; i < s->shot_count;) {
        if (s->shots[i].damage == 0) s->shots[i] = s->shots[--s->shot_count];
        else ++i;
    }
}

static void reap_enemies(bh_sim_t* s) {
    for (uint16_t i = 0; i < s->enemy_count;) {
        bh_enemy_t* e = &s->enemies[i];
        if (e->hp <= 0 && !(e->flags & FLAG_DEAD)) {
            kill_enemy(s, e);
            s->enemies[i] = s->enemies[--s->enemy_count];
        } else if (enemy_offscreen(e)) {
            if (e->type == BH_ENEMY_BOSS) {
                s->boss_dead = 1;
                s->stage_end_delay = 30;
            }
            s->enemies[i] = s->enemies[--s->enemy_count];
        } else {
            ++i;
        }
    }
}

/* ------------------------------------------------------------- bullets etc */

static void bullets_move(bh_sim_t* s) {
    bh_bullet_t* b = s->bullets;
    uint16_t n = s->bullet_count;
    const int32_t lo = FP(-16), hix = FP(BH_FIELD_W + 16), hiy = FP(BH_FIELD_H + 16);
    for (uint16_t i = 0; i < n;) {
        bh_bullet_t* p = &b[i];
        p->x += p->vx;
        p->y += p->vy;
        p->age++;
        if (p->x < lo || p->x > hix || p->y < lo || p->y > hiy) {
            *p = b[--n];
            s->stats.bullets_culled++;
        } else {
            ++i;
        }
    }
    s->bullet_count = n;
}

static void shots_move(bh_sim_t* s) {
    for (uint16_t i = 0; i < s->shot_count;) {
        bh_shot_t* sh = &s->shots[i];
        sh->x += sh->vx;
        sh->y += sh->vy;
        if (sh->y < FP(-40)) s->shots[i] = s->shots[--s->shot_count];
        else ++i;
    }
}

static void explosions_update(bh_sim_t* s) {
    for (uint16_t i = 0; i < s->explosion_count;) {
        bh_explosion_t* e = &s->explosions[i];
        if (e->delay) {
            e->delay--;
            ++i;
            continue;
        }
        if (++e->tick >= 3u) {
            e->tick = 0;
            e->frame++;
        }
        if (e->frame >= BH_EXPLOSION_FRAMES) s->explosions[i] = s->explosions[--s->explosion_count];
        else ++i;
    }
}

static void pickups_update(bh_sim_t* s) {
    bh_player_t* pl = &s->player;
    const int32_t grab = (s->params.pickup_radius + 14) * 256;
    const int32_t magnet = 56 * 256;
    for (uint16_t i = 0; i < s->pickup_count;) {
        bh_pickup_t* p = &s->pickups[i];
        p->age++;
        int remove = 0;
        if (pl->alive && p->age > 20u) {
            const int32_t dx = pl->x - p->x, dy = pl->y - p->y;
            if (bh_circle_hit(dx, dy, grab)) {
                remove = 1;
                if (p->type == BH_PICKUP_POWER) {
                    if (pl->power < 4u) pl->power++;
                    s->score += 100u;
                } else if (p->type == BH_PICKUP_BOMB) {
                    if (pl->bombs < 5u) pl->bombs++;
                    s->score += 500u;
                } else {
                    s->score += 1000u;
                }
            } else if (bh_circle_hit(dx, dy, magnet)) {
                const int32_t sx = dx > 768 ? 768 : dx < -768 ? -768 : dx;
                const int32_t sy = dy > 768 ? 768 : dy < -768 ? -768 : dy;
                p->x += sx;
                p->y += sy;
            }
        }
        if (!remove) {
            if (p->vy < 128) p->vy = (int16_t)(p->vy + 16);
            p->y += p->vy;
            if (p->y > FP(BH_FIELD_H + 16)) remove = 1;
        }
        if (remove) s->pickups[i] = s->pickups[--s->pickup_count];
        else ++i;
    }
}

/* Benchmark: keep the pool at exactly steady_target. Three fans of mixed shapes
 * and speeds; the first fill places each bullet part-way along its path so the
 * run starts in steady state rather than as a blob at the emitters. */
static void steady_topup(bh_sim_t* s, int prefill) {
    static const int16_t ex[3] = {56, 112, 168};
    static const int16_t ey[3] = {56, 36, 56};
    while (s->bullet_count < s->steady_target) {
        const uint32_t k = s->steady_seq++;
        const uint32_t e = k % 3u;
        const uint32_t angle = 20u + (k * 13u) % 89u;
        const int32_t speed = 256 + (int32_t)(k % 5u) * 96;
        const uint8_t shape = (uint8_t)((k / 3u) % BH_SHAPES);
        const uint8_t color = kStageColors[s->stage][(k >> 3) & 1u];
        const uint16_t before = s->bullet_count;
        spawn_bullet(s, FP(ex[e]), FP(ey[e]), angle, speed, shape, color);
        if (prefill && s->bullet_count > before) {
            bh_bullet_t* b = &s->bullets[before];
            const int32_t age = (int32_t)(rnd(s) % 220u);
            b->x += b->vx * age;
            b->y += b->vy * age;
            b->age = (uint16_t)age;
        }
    }
    if (s->bullet_count > s->steady_target) s->bullet_count = s->steady_target;
}

/* ------------------------------------------------------------------ stage */

static void next_stage(bh_sim_t* s) {
    s->stage = (uint16_t)((s->stage + 1u) % BH_STAGES);
    s->stage_serial++;
    s->stage_changed = 1;
    s->stage_timer = 0;
    s->stage_end_delay = 0;
    s->boss_spawned = s->boss_dead = 0;
    s->enemy_count = 0;
    s->bullet_count = 0;
    s->pickup_count = 0;
    s->score += 10000u;
}

static void reset_game(bh_sim_t* s) {
    s->score = 0;
    s->stage = 0;
    s->stage_timer = 0;
    s->stage_end_delay = 0;
    s->boss_spawned = s->boss_dead = 0;
    s->game_over = 0;
    s->enemy_count = s->bullet_count = s->shot_count = s->explosion_count = s->pickup_count = 0;
    reset_player(s);
}

void bh_sim_init(bh_sim_t* s, const bh_params_t* params, uint32_t seed) {
    __builtin_memset(s, 0, sizeof(*s));
    if (params) s->params = *params;
    else bh_params_default(&s->params);
    s->rng = seed ? seed : 1u;
    sat_spatial_init(&s->spatial, s->sp_heads, 7, 14, 5, s->sp_entries,
                     (uint16_t)(sizeof(s->sp_entries) / sizeof(s->sp_entries[0])), s->sp_stamps, s->sp_items,
                     BH_MAX_ENEMIES);
    reset_player(s);
}

void bh_sim_set_god(bh_sim_t* s, uint8_t god) { s->god = god; }

void bh_sim_set_steady_bullets(bh_sim_t* s, uint16_t count) {
    if (count > BH_MAX_BULLETS) count = BH_MAX_BULLETS;
    s->steady_target = count;
    if (count) {
        s->god = 1;
        s->bullet_count = 0;
        steady_topup(s, 1);
    }
}

void bh_sim_set_stage_time(bh_sim_t* s, uint16_t frames) { s->stage_timer = frames; }

void bh_sim_update(bh_sim_t* s, uint16_t input) {
    s->stage_changed = 0;
    if (s->game_over) {
        if (s->game_over_timer && --s->game_over_timer == 0) {
            reset_game(s);
        }
        input = 0;
    }
    player_update(s, input);
    s->prev_input = input;
    if (s->bomb_timer) s->bomb_timer--;

    s->stage_timer++;
    run_script(s);
    for (uint16_t i = 0; i < s->enemy_count; ++i) {
        enemy_move(s, &s->enemies[i]);
        enemy_think(s, &s->enemies[i]);
    }
    bullets_move(s);
    shots_move(s);
    apply_shot_hits(s);
    reap_enemies(s);
    if (s->steady_target) steady_topup(s, 0);

    /* Enemy bullets against the player: every bullet is tested every frame,
     * even in god mode, so the benchmark carries the full collision cost. */
    s->stats.bullet_tests += s->bullet_count;
    if (s->player.alive) {
        const uint16_t hits = bh_bullets_hitting_player(s, 0);
        if (hits) {
            s->stats.player_hits += hits;
            if (!s->god && s->player.invuln == 0) player_die(s);
        }
    }

    explosions_update(s);
    pickups_update(s);

    s->scroll_y += 384u;
    if (s->boss_dead && s->stage_end_delay && --s->stage_end_delay == 0) next_stage(s);
    else if (s->stage_timer >= s->params.stage_frames) next_stage(s);
    s->frame++;
}

/* ------------------------------------------------------------- demo / hash */

uint16_t bh_demo_input(const bh_sim_t* s) {
    const bh_player_t* p = &s->player;
    uint16_t in = BH_IN_SHOT;
    const int32_t tx = FP(112) + ((bh_sin(s->frame * 2u) * FP(84)) >> 15);
    const int32_t ty = FP(380) + ((bh_sin(s->frame * 3u + 64u) * FP(44)) >> 15);
    if (tx - p->x > FP(6)) in |= BH_IN_RIGHT;
    else if (p->x - tx > FP(6)) in |= BH_IN_LEFT;
    if (ty - p->y > FP(6)) in |= BH_IN_DOWN;
    else if (p->y - ty > FP(6)) in |= BH_IN_UP;

    int32_t best = 0x7FFFFFFF, bdx = 0, bdy = 0;
    const bh_bullet_t* b = s->bullets;
    for (uint16_t i = 0; i < s->bullet_count; ++i, ++b) {
        const int32_t dx = b->x - p->x, dy = b->y - p->y;
        if (dx > FP(34) || dx < -FP(34) || dy > FP(20) || dy < -FP(56)) continue;
        const int32_t d = (dx >> 8) * (dx >> 8) + (dy >> 8) * (dy >> 8);
        if (d < best) {
            best = d;
            bdx = dx;
            bdy = dy;
        }
    }
    if (best != 0x7FFFFFFF) {
        in &= (uint16_t) ~(BH_IN_LEFT | BH_IN_RIGHT | BH_IN_UP | BH_IN_DOWN);
        in |= (bdx > 0 || (bdx == 0 && p->x > FP(112))) ? BH_IN_LEFT : BH_IN_RIGHT;
        if (p->x < FP(30)) in = (uint16_t)((in & ~BH_IN_LEFT) | BH_IN_RIGHT);
        if (p->x > FP(194)) in = (uint16_t)((in & ~BH_IN_RIGHT) | BH_IN_LEFT);
        if (bdy < 0 && p->y < FP(420)) in |= BH_IN_DOWN;
        if (best < 12 * 12) in |= BH_IN_FOCUS;
        if (best < 7 * 7 && p->bombs > 0 && !s->god) in |= BH_IN_BOMB;
    }
    return in;
}

static uint32_t mix(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

uint32_t bh_sim_hash(const bh_sim_t* s) {
    uint32_t h = 2166136261u;
    h = mix(h, s->rng);
    h = mix(h, s->frame);
    h = mix(h, s->score);
    h = mix(h, s->scroll_y);
    h = mix(h, ((uint32_t)s->stage << 16) | s->stage_timer);
    h = mix(h, ((uint32_t)s->stage_serial << 16) | s->stage_end_delay);
    h = mix(h, ((uint32_t)s->boss_spawned << 24) | ((uint32_t)s->boss_dead << 16) | ((uint32_t)s->god << 8) |
                   s->game_over);
    const bh_player_t* p = &s->player;
    h = mix(h, (uint32_t)p->x);
    h = mix(h, (uint32_t)p->y);
    h = mix(h, ((uint32_t)p->alive << 24) | ((uint32_t)p->bank << 16) | ((uint32_t)p->lives << 8) | p->bombs);
    h = mix(h, ((uint32_t)p->power << 24) | ((uint32_t)p->shot_cooldown << 16) | p->invuln);
    h = mix(h, ((uint32_t)p->respawn << 16) | (uint16_t)p->steer);
    h = mix(h, ((uint32_t)s->bullet_count << 16) | s->shot_count);
    h = mix(h, ((uint32_t)s->enemy_count << 16) | s->explosion_count);
    h = mix(h, s->pickup_count);
    for (uint16_t i = 0; i < s->bullet_count; ++i) {
        const bh_bullet_t* b = &s->bullets[i];
        h = mix(h, (uint32_t)b->x);
        h = mix(h, (uint32_t)b->y);
        h = mix(h, ((uint32_t)(uint16_t)b->vx << 16) | (uint16_t)b->vy);
        h = mix(h, ((uint32_t)b->shape << 24) | ((uint32_t)b->color << 16) | b->age);
    }
    for (uint16_t i = 0; i < s->shot_count; ++i) {
        const bh_shot_t* b = &s->shots[i];
        h = mix(h, (uint32_t)b->x);
        h = mix(h, (uint32_t)b->y);
        h = mix(h, ((uint32_t)b->kind << 8) | b->damage);
    }
    for (uint16_t i = 0; i < s->enemy_count; ++i) {
        const bh_enemy_t* e = &s->enemies[i];
        h = mix(h, (uint32_t)e->x);
        h = mix(h, (uint32_t)e->y);
        h = mix(h, ((uint32_t)(uint16_t)e->hp << 16) | e->age);
        h = mix(h, ((uint32_t)e->type << 24) | ((uint32_t)e->pattern << 16) | e->fire);
        h = mix(h, ((uint32_t)(uint16_t)e->aux << 8) | e->flags);
    }
    for (uint16_t i = 0; i < s->explosion_count; ++i) {
        const bh_explosion_t* e = &s->explosions[i];
        h = mix(h, (uint32_t)e->x);
        h = mix(h, (uint32_t)e->y);
        h = mix(h, ((uint32_t)e->frame << 16) | ((uint32_t)e->tick << 8) | e->delay);
    }
    for (uint16_t i = 0; i < s->pickup_count; ++i) {
        const bh_pickup_t* e = &s->pickups[i];
        h = mix(h, (uint32_t)e->x);
        h = mix(h, (uint32_t)e->y);
        h = mix(h, ((uint32_t)e->type << 16) | e->age);
    }
    return h;
}
