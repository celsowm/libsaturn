#include "bh_render.h"

#include "bullet_hell_stress/shmup_atlas_ids.h"

#define B(shape) {SHMUP_BULLET_##shape##_BLUE, SHMUP_BULLET_##shape##_GREEN, SHMUP_BULLET_##shape##_ORANGE, \
                  SHMUP_BULLET_##shape##_PINK, SHMUP_BULLET_##shape##_RED, SHMUP_BULLET_##shape##_VIOLET}

/* Shape and colour order is the simulation's (bh_sim.h). */
static const uint16_t kBullet[BH_SHAPES][BH_COLORS] = {
    B(DIAMOND), B(ORB), B(RING), B(STAR), B(NEEDLE), B(SHARD)};

/* Stage 1 red, stage 2 violet, stage 3 blue: each stage's enemies are one residency group. */
static const uint16_t kEnemy[BH_STAGES][BH_ENEMY_TYPES] = {
    {SHMUP_ENEMY_DRONE_RED, SHMUP_ENEMY_INTERCEPTOR_RED, SHMUP_ENEMY_TURRET_RED, SHMUP_SHIP_ENEMY_RED,
     SHMUP_BOSS_CORE_RED},
    {SHMUP_ENEMY_DRONE_VIOLET, SHMUP_ENEMY_INTERCEPTOR_VIOLET, SHMUP_ENEMY_TURRET_VIOLET, SHMUP_SHIP_ENEMY_GREEN,
     SHMUP_BOSS_CORE_VIOLET},
    {SHMUP_ENEMY_DRONE_BLUE, SHMUP_ENEMY_INTERCEPTOR_BLUE, SHMUP_ENEMY_TURRET_BLUE, SHMUP_SHIP_ENEMY_BLUE,
     SHMUP_BOSS_CORE_BLUE}};

static const uint16_t kExplosion[BH_EXPLOSION_FRAMES] = {
    SHMUP_EXPLOSION_00, SHMUP_EXPLOSION_01, SHMUP_EXPLOSION_02, SHMUP_EXPLOSION_03,
    SHMUP_EXPLOSION_04, SHMUP_EXPLOSION_05, SHMUP_EXPLOSION_06, SHMUP_EXPLOSION_07};

/* Stage palettes: pickups follow the enemy colour of the stage. */
static const uint16_t kPickupPower[BH_STAGES] = {SHMUP_PICKUP_POWER_RED, SHMUP_PICKUP_POWER_VIOLET, SHMUP_PICKUP_POWER_BLUE};
static const uint16_t kPickupBomb[BH_STAGES] = {SHMUP_PICKUP_BOMB_RED, SHMUP_PICKUP_BOMB_VIOLET, SHMUP_PICKUP_BOMB_BLUE};
static const uint16_t kPickupScore[BH_COLORS] = {
    SHMUP_PICKUP_BLUE, SHMUP_PICKUP_GREEN, SHMUP_PICKUP_ORANGE, SHMUP_PICKUP_PINK, SHMUP_PICKUP_RED, SHMUP_PICKUP_VIOLET};

/* The bank frames are red, so the ship is the red one. */
static const uint16_t kShip[BH_BANK_FRAMES] = {
    SHMUP_SHIP_PLAYER_BANK_LEFT, SHMUP_SHIP_PLAYER_BANK_CENTER_LEFT, SHMUP_SHIP_PLAYER_RED,
    SHMUP_SHIP_PLAYER_BANK_CENTER_RIGHT, SHMUP_SHIP_PLAYER_BANK_RIGHT};

static const uint16_t kDigit[10] = {
    SHMUP_HUD_GLYPH_0, SHMUP_HUD_GLYPH_1, SHMUP_HUD_GLYPH_2, SHMUP_HUD_GLYPH_3, SHMUP_HUD_GLYPH_4,
    SHMUP_HUD_GLYPH_5, SHMUP_HUD_GLYPH_6, SHMUP_HUD_GLYPH_7, SHMUP_HUD_GLYPH_8, SHMUP_HUD_GLYPH_9};
static const uint16_t kLetter[26] = {
    SHMUP_HUD_GLYPH_A, SHMUP_HUD_GLYPH_B, SHMUP_HUD_GLYPH_C, SHMUP_HUD_GLYPH_D, SHMUP_HUD_GLYPH_E,
    SHMUP_HUD_GLYPH_F, SHMUP_HUD_GLYPH_G, SHMUP_HUD_GLYPH_H, SHMUP_HUD_GLYPH_I, SHMUP_HUD_GLYPH_J,
    SHMUP_HUD_GLYPH_K, SHMUP_HUD_GLYPH_L, SHMUP_HUD_GLYPH_M, SHMUP_HUD_GLYPH_N, SHMUP_HUD_GLYPH_O,
    SHMUP_HUD_GLYPH_P, SHMUP_HUD_GLYPH_Q, SHMUP_HUD_GLYPH_R, SHMUP_HUD_GLYPH_S, SHMUP_HUD_GLYPH_T,
    SHMUP_HUD_GLYPH_U, SHMUP_HUD_GLYPH_V, SHMUP_HUD_GLYPH_W, SHMUP_HUD_GLYPH_X, SHMUP_HUD_GLYPH_Y,
    SHMUP_HUD_GLYPH_Z};

uint16_t bh_frame_bullet(uint8_t shape, uint8_t color) { return kBullet[shape % BH_SHAPES][color % BH_COLORS]; }
uint16_t bh_frame_enemy(uint16_t stage, uint8_t type) { return kEnemy[stage % BH_STAGES][type % BH_ENEMY_TYPES]; }
uint16_t bh_frame_explosion(uint8_t index) { return kExplosion[index % BH_EXPLOSION_FRAMES]; }
uint8_t bh_stage_group(uint16_t stage) { return (uint8_t)(1u + stage % BH_STAGES); }

typedef struct sink {
    const bh_view_t* view;
    const uint8_t* ready;
    const sat_sprite_frame_t* frames;
    uint32_t clip_base;      /* index offset of the user-clipped copy of every frame */
    sat_sprite_instance_t* out;
    uint32_t n, cap;
} sink_t;

/* Logical position in 1/256 px. */
static inline void put(sink_t* s, int32_t x_fp, int32_t y_fp, uint16_t frame) {
    if (s->n >= s->cap || !s->ready[frame]) return;
    sat_sprite_instance_t* i = &s->out[s->n++];
    bh_view_map(s->view, x_fp >> BH_FP_SHIFT, y_fp >> BH_FP_SHIFT, &i->x, &i->y);
    i->frame = frame;
    i->reserved = 0;
}

/* A sprite of the playfield. The field is a rectangle of the 640 x 224 screen with the HUD panels
 * beside it, so anything that crosses the field's edge (an enemy flying in from above, a shot
 * leaving) must not be drawn over a panel: those sprites use the user-clipped copy of their frame.
 * The sprites wholly inside, almost all of them, keep the cheaper unclipped frame. `extent` is the
 * most the sprite reaches from its position, in logical pixels. */
static inline void put_field(sink_t* s, int32_t x_fp, int32_t y_fp, uint16_t frame, int32_t extent) {
    if (s->n >= s->cap || !s->ready[frame]) return;
    const int32_t y = y_fp >> BH_FP_SHIFT;
    const uint16_t f = (y < extent || y > BH_FIELD_H - extent) ? (uint16_t)(frame + s->clip_base) : frame;
    sat_sprite_instance_t* i = &s->out[s->n++];
    bh_view_map(s->view, x_fp >> BH_FP_SHIFT, y, &i->x, &i->y);
    i->frame = f;
    i->reserved = 0;
}

static inline int32_t extent_of(const sink_t* s, uint16_t frame) {
    const sat_sprite_frame_t* f = &s->frames[frame];
    const int32_t w = sat_sprite_frame_width(f), h = sat_sprite_frame_height(f);
    return w > h ? w : h;
}

/* HUD text lives in the panels beside the field. Logical y beyond the field (below it at y >= 448,
 * above it at y < 0) maps onto the 96-pixel panels the 640 x 224 screen leaves free. */
#define HUD_PITCH 9

static void text(sink_t* s, int32_t lx, int32_t ly, const char* str) {
    for (; *str; ++str, lx += HUD_PITCH) {
        const char c = *str;
        uint16_t f;
        if (c >= '0' && c <= '9') f = kDigit[c - '0'];
        else if (c >= 'A' && c <= 'Z') f = kLetter[c - 'A'];
        else if (c == ':') f = SHMUP_HUD_GLYPH_COLON;
        else if (c == '.') f = SHMUP_HUD_GLYPH_DOT;
        else if (c == '-') f = SHMUP_HUD_GLYPH_MINUS;
        else if (c == '+') f = SHMUP_HUD_GLYPH_PLUS;
        else if (c == '/') f = SHMUP_HUD_GLYPH_SLASH;
        else continue;
        put(s, lx << BH_FP_SHIFT, ly << BH_FP_SHIFT, f);
    }
}

static void number(sink_t* s, int32_t lx, int32_t ly, const char* label, uint32_t value, uint32_t digits) {
    char buf[24];
    uint32_t n = 0;
    for (; label[n] && n < 10; ++n) buf[n] = label[n];
    for (uint32_t d = digits; d > 0; --d) {
        uint32_t p = 1;
        for (uint32_t k = 1; k < d; ++k) p *= 10u;
        buf[n++] = (char)('0' + (value / p) % 10u);
    }
    buf[n] = 0;
    text(s, lx, ly, buf);
}

static void pills(sink_t* s, int32_t lx, int32_t ly, uint16_t frame, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) put(s, (lx + (int32_t)i * 44) << BH_FP_SHIFT, ly << BH_FP_SHIFT, frame);
}

/* The bullets are most of the list, so they get their own loop: which frames are resident is
 * decided once per shape and colour (not per bullet), and the view of a quarter-turned field is two
 * adds, no multiplies (a multiply is several cycles on the SH-2). */
/* x >> BH_FP_SHIFT, rounded down. The SH-2 has no arithmetic shift by 8, so gcc calls libgcc for
 * `>>` here (about 12 cycles, twice a bullet). A bias makes it a logical shift, which is one
 * instruction; it holds while |x| < 2^23, i.e. within +-32768 px. */
static inline int32_t px_of(int32_t x_fp) {
    return (int32_t)(((uint32_t)x_fp + (1u << 23)) >> BH_FP_SHIFT) - (1 << (23 - BH_FP_SHIFT));
}

/* They skip the instance list altogether: each bullet's command is written straight into the frame's
 * list through a sat_sprite_batch window (bh_render_bullets), so a bullet costs one pass over the
 * pool instead of a pass to build an instance and another to turn it into a command. */
void bh_render_bullets(const bh_sim_t* sim, const bh_view_t* view, const sat_sprite_frame_t* frames,
                       uint32_t clip_base, const uint8_t* frame_ready, sat_sprite_batch_stats_t* stats) {
    enum { BULLET_EXTENT = 16 };   /* the atlas's bullets are at most 16 x 16 */
    const sat_sprite_frame_t* frame[BH_SHAPES][BH_COLORS];
    const sat_sprite_frame_t* clipped[BH_SHAPES][BH_COLORS];
    for (uint32_t a = 0; a < BH_SHAPES; ++a)
        for (uint32_t c = 0; c < BH_COLORS; ++c) {
            const uint16_t f = kBullet[a][c];
            frame[a][c] = frame_ready[f] ? &frames[f] : 0;
            clipped[a][c] = frame_ready[f] ? &frames[f + clip_base] : 0;
        }
    const uint32_t count = sim->bullet_count;
    sat_sprite_batch_window_t win;
    if (sat_sprite_batch_begin(&win, count, 0u) != SAT_OK) return;
    sat_sprite_batch_window_t w = win;   /* a local: the loop keeps its fields in registers */
    const bh_bullet_t* b = sim->bullets;
    const bh_view_t* v = view;
    if (v->sxx == 0 && v->syy == 0 && v->sxy == -1 && v->syx == 1) {   /* SAT_TATE_ROT_CW */
        const int32_t c0 = v->c0, d0 = v->d0;
        for (uint32_t i = 0; i < count; ++i, ++b) {
            const int32_t ly = px_of(b->y);
            const sat_sprite_frame_t* f = (ly < BULLET_EXTENT || ly > BH_FIELD_H - BULLET_EXTENT)
                                              ? clipped[b->shape][b->color] : frame[b->shape][b->color];
            if (!f) continue;
            (void)sat_sprite_batch_emit(&w, f, c0 - ly, d0 + px_of(b->x));
        }
    } else {
        for (uint32_t i = 0; i < count; ++i, ++b) {
            const int32_t ly = px_of(b->y);
            const sat_sprite_frame_t* f = (ly < BULLET_EXTENT || ly > BH_FIELD_H - BULLET_EXTENT)
                                              ? clipped[b->shape][b->color] : frame[b->shape][b->color];
            if (!f) continue;
            int16_t x, y;
            bh_view_map(v, px_of(b->x), ly, &x, &y);
            (void)sat_sprite_batch_emit(&w, f, x, y);
        }
    }
    const sat_sprite_batch_window_t done = w;   /* a copy: &w must not escape, or w lives in memory */
    (void)sat_sprite_batch_end(&done, stats);
}

uint32_t bh_render_build(const bh_sim_t* sim, const bh_view_t* view, const sat_sprite_frame_t* frames,
                         uint32_t clip_base, const uint8_t* frame_ready, const bh_hud_t* hud,
                         sat_sprite_instance_t* out, uint32_t capacity, uint32_t* bullets_at) {
    sink_t s = {view, frame_ready, frames, clip_base, out, 0, capacity};
    const uint16_t stage = sim->stage % BH_STAGES;
    for (uint16_t i = 0; i < sim->pickup_count; ++i) {
        const bh_pickup_t* p = &sim->pickups[i];
        const uint16_t f = p->type == BH_PICKUP_POWER ? kPickupPower[stage]
                           : p->type == BH_PICKUP_BOMB ? kPickupBomb[stage] : kPickupScore[p->color % BH_COLORS];
        put_field(&s, p->x, p->y, f, extent_of(&s, f));
    }
    for (uint16_t i = 0; i < sim->enemy_count; ++i) {
        const bh_enemy_t* e = &sim->enemies[i];
        const uint16_t f = kEnemy[stage][e->type];
        put_field(&s, e->x, e->y, f, extent_of(&s, f));
    }
    const bh_player_t* pl = &sim->player;
    if (pl->alive && !(pl->invuln & 2u)) {
        for (int i = 0; i < bh_option_count(pl); ++i) {
            int32_t ox, oy;
            bh_option_pos(pl, i, &ox, &oy);
            put_field(&s, ox, oy, SHMUP_SHIP_OPTION_BLUE, extent_of(&s, SHMUP_SHIP_OPTION_BLUE));
        }
        const uint16_t ship = kShip[pl->bank % BH_BANK_FRAMES];
        put_field(&s, pl->x, pl->y, ship, extent_of(&s, ship));
    }
    for (uint16_t i = 0; i < sim->shot_count; ++i) {
        const bh_shot_t* sh = &sim->shots[i];
        const uint16_t f = sh->kind ? SHMUP_PLAYER_LASER_PINK : SHMUP_PLAYER_SHOT_BLUE;
        put_field(&s, sh->x, sh->y, f, extent_of(&s, f));
    }
    *bullets_at = s.n;   /* the bullets go here, between the shots and the explosions */
    for (uint16_t i = 0; i < sim->explosion_count; ++i) {
        const bh_explosion_t* e = &sim->explosions[i];
        if (e->delay) continue;
        const uint16_t f = kExplosion[e->frame % BH_EXPLOSION_FRAMES];
        put_field(&s, e->x, e->y, f, extent_of(&s, f));
    }

    /* Bottom panel (logical y 448..544): score and the three gauges. */
    number(&s, 8, 460, "SCORE ", sim->score, 8);
    pills(&s, 24, 484, SHMUP_HUD_BAR_LIFE, sim->player.lives);
    pills(&s, 24, 504, SHMUP_HUD_BAR_BOMB, sim->player.bombs);
    pills(&s, 24, 524, SHMUP_HUD_BAR_POWER, sim->player.power);
    /* Top panel (logical y -96..0): stage, bullets and frame rate. */
    number(&s, 8, -88, "STAGE ", hud->stage, 1);
    number(&s, 8, -68, "BULLETS ", hud->bullets, 4);
    number(&s, 8, -48, "FPS ", hud->fps, 2);
    if (hud->show_timing) {   /* the top panel is 224 px (24 glyphs) long: two pairs a line */
        number(&s, 98, -48, "SRV ", hud->ticks[0], 4);
        number(&s, 8, -28, "SIM ", hud->ticks[1], 4);
        number(&s, 116, -28, "BLD ", hud->ticks[2], 4);
        number(&s, 8, -8, "DRW ", hud->ticks[3], 4);
        number(&s, 116, -8, "END ", hud->ticks[4], 4);
    }
    if (sim->game_over) text(&s, 135, 460, "GAME OVER");
    return s.n;
}
