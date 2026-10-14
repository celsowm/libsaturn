#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "examples/bullet_hell_stress/bh_sim.h"
#include "saturn/collide2d.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

namespace {

constexpr double kPi = 3.14159265358979323846;

uint32_t g_rng = 0xBADC0DEu;
uint32_t rnd() { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }

/* The reference: the library's own circle test at 16.16. */
bool lib_hit(int32_t ax, int32_t ay, int32_t ar_px, int32_t bx, int32_t by, int32_t br_px) {
    sat_circle_t a{}, b{};
    a.center.x = ax << 8; a.center.y = ay << 8; a.radius = ar_px << 16;
    b.center.x = bx << 8; b.center.y = by << 8; b.radius = br_px << 16;
    return sat_circle_overlap(&a, &b) != 0;
}

bh_sim_t* make_sim(uint32_t seed) {
    bh_sim_t* s = static_cast<bh_sim_t*>(std::calloc(1, sizeof(bh_sim_t)));
    bh_sim_init(s, nullptr, seed);
    return s;
}

/* 3000 frames of autopilot play; returns the final hash and checks pool bounds each frame. */
uint32_t play(uint32_t seed, int frames, uint16_t steady = 0) {
    bh_sim_t* s = make_sim(seed);
    if (steady) bh_sim_set_steady_bullets(s, steady);
    for (int i = 0; i < frames; ++i) {
        bh_sim_update(s, bh_demo_input(s));
        OK(s->bullet_count <= BH_MAX_BULLETS && s->shot_count <= BH_MAX_SHOTS);
        OK(s->enemy_count <= BH_MAX_ENEMIES && s->explosion_count <= BH_MAX_EXPLOSIONS);
        OK(s->pickup_count <= BH_MAX_PICKUPS);
    }
    const uint32_t h = bh_sim_hash(s);
    std::free(s);
    return h;
}

}  // namespace

int main() {
    /* Trig: the table is a true sine, the angle of a vector is within one unit. */
    for (uint32_t a = 0; a < 256; ++a) {
        const double rad = a * 2.0 * kPi / 256.0;
        OK(std::abs(bh_sin(a) - std::lround(std::sin(rad) * 32767.0)) <= 12);
        OK(std::abs(bh_cos(a) - std::lround(std::cos(rad) * 32767.0)) <= 12);
    }
    OK(bh_sin(0) == 0 && bh_sin(64) == 32767 && bh_sin(128) == 0 && bh_sin(192) == -32767);
    for (int i = 0; i < 20000; ++i) {
        const int32_t dx = static_cast<int32_t>(rnd() % 40001u) - 20000, dy = static_cast<int32_t>(rnd() % 40001u) - 20000;
        if (dx == 0 && dy == 0) continue;
        double ang = std::atan2(static_cast<double>(dy), static_cast<double>(dx)) * 256.0 / (2.0 * kPi);
        if (ang < 0) ang += 256.0;
        double diff = std::fabs(ang - bh_atan2(dy, dx));
        diff = std::min(diff, 256.0 - diff);
        OK(diff <= 1.0);
    }
    OK(bh_atan2(0, 100) == 0 && bh_atan2(100, 0) == 64 && bh_atan2(0, -100) == 128 && bh_atan2(-100, 0) == 192);

    /* bh_circle_hit == sat_circle_overlap, including strict touching edges. */
    for (int i = 0; i < 200000; ++i) {
        const int32_t r_px = 1 + static_cast<int32_t>(rnd() % 30u);
        const int32_t r = r_px * 256;
        const int32_t dx = static_cast<int32_t>(rnd() % static_cast<uint32_t>(2 * r + 64)) - r - 32;
        const int32_t dy = static_cast<int32_t>(rnd() % static_cast<uint32_t>(2 * r + 64)) - r - 32;
        sat_circle_t a{}, b{};
        a.center.x = dx << 8; a.center.y = dy << 8; a.radius = (r_px - r_px / 2) << 16;
        b.radius = (r_px / 2) << 16;
        OK((sat_circle_overlap(&a, &b) != 0) == (bh_circle_hit(dx, dy, r) != 0));
    }
    {
        /* 3-4-5 triangles touch exactly: strict, so no hit; one unit closer is a hit. */
        const int32_t r = 5 * 256;
        OK(!bh_circle_hit(3 * 256, 4 * 256, r) && !bh_circle_hit(-3 * 256, -4 * 256, r));
        OK(!bh_circle_hit(r, 0, r) && !bh_circle_hit(0, -r, r));
        OK(bh_circle_hit(3 * 256 - 1, 4 * 256, r) && bh_circle_hit(r - 1, 0, r));
    }

    /* Determinism: same seed and input, same state; a different seed differs. */
    {
        const uint32_t a = play(7u, 3000), b = play(7u, 3000), c = play(8u, 3000);
        OK(a == b);
        OK(a != c);
        OK(a == 0xE54909C7u); /* pinned: any simulation change must update this on purpose */
        std::printf("golden hash seed 7, 3000 frames: 0x%08X\n", a);
        const uint32_t s1 = play(7u, 400, 1024), s2 = play(7u, 400, 1024);
        OK(s1 == s2);
    }
    {
        /* Stepping in two halves equals one run (no hidden state outside the struct). */
        bh_sim_t* s = make_sim(7u);
        for (int i = 0; i < 3000; ++i) bh_sim_update(s, bh_demo_input(s));
        OK(bh_sim_hash(s) == play(7u, 3000));
        std::free(s);
    }

    /* Bullet-vs-player: the fast path equals a brute-force sat_circle_overlap sweep. */
    {
        bh_sim_t* s = make_sim(11u);
        bh_sim_set_steady_bullets(s, 1024);
        long total_hits = 0;
        for (int f = 0; f < 120; ++f) {
            bh_sim_update(s, 0);
            for (int probe = 0; probe < 6; ++probe) {
                /* Park the player on a random bullet (offset small) so overlaps happen. */
                const bh_bullet_t& pb = s->bullets[rnd() % s->bullet_count];
                s->player.x = pb.x + static_cast<int32_t>(rnd() % 1025u) - 512;
                s->player.y = pb.y + static_cast<int32_t>(rnd() % 1025u) - 512;
                uint16_t first = 0xFFFF;
                const uint16_t fast = bh_bullets_hitting_player(s, &first);
                uint16_t ref = 0, ref_first = 0xFFFF;
                for (uint16_t i = 0; i < s->bullet_count; ++i) {
                    const bh_bullet_t& b = s->bullets[i];
                    if (lib_hit(b.x, b.y, s->params.bullet_radius[b.shape], s->player.x, s->player.y,
                                s->params.player_radius)) {
                        if (ref == 0) ref_first = i;
                        ++ref;
                    }
                }
                OK(fast == ref);
                OK(first == ref_first || fast == 0);
                total_hits += fast;
            }
        }
        OK(total_hits > 100); /* the probe did exercise hits */
        /* Strict edge on a real bullet: exactly touching is not a hit. */
        s->bullet_count = 1;
        s->bullets[0].shape = BH_SHAPE_ORB;
        s->player.x = 100 * 256; s->player.y = 200 * 256;
        const int32_t rr = (s->params.bullet_radius[BH_SHAPE_ORB] + s->params.player_radius) * 256;
        s->bullets[0].x = s->player.x + rr; s->bullets[0].y = s->player.y;
        OK(bh_bullets_hitting_player(s, nullptr) == 0);
        s->bullets[0].x = s->player.x + rr - 1;
        OK(bh_bullets_hitting_player(s, nullptr) == 1);
        std::free(s);
    }

    /* Shot-vs-enemy: sat_spatial broad phase equals the O(n^2) circle sweep. */
    {
        bh_sim_t* s = make_sim(5u);
        long total = 0;
        for (int f = 0; f < 2400; ++f) {
            bh_sim_update(s, bh_demo_input(s));
            /* The update has already spent the shots that overlapped, so park a few on enemies. */
            if (s->enemy_count) {
                for (uint16_t i = 0; i < s->shot_count; i += 2) {
                    const bh_enemy_t& en = s->enemies[rnd() % s->enemy_count];
                    const int32_t reach = (s->params.enemy_radius[en.type] + 6) * 256;
                    s->shots[i].x = en.x + static_cast<int32_t>(rnd() % static_cast<uint32_t>(2 * reach)) - reach;
                    s->shots[i].y = en.y + static_cast<int32_t>(rnd() % static_cast<uint32_t>(2 * reach)) - reach;
                }
            }
            std::vector<bh_hit_pair_t> got(BH_MAX_SHOTS * 2);
            const uint16_t n = bh_find_shot_hits(s, got.data(), static_cast<uint16_t>(got.size()));
            got.resize(n);
            std::vector<std::pair<int, int>> ref, mine;
            for (uint16_t i = 0; i < s->shot_count; ++i) {
                for (uint16_t e = 0; e < s->enemy_count; ++e) {
                    const bh_enemy_t& en = s->enemies[e];
                    if (en.y < 4 * 256 || (en.flags & 6)) continue; /* the hittable rule */
                    const bh_shot_t& sh = s->shots[i];
                    if (lib_hit(sh.x, sh.y, s->params.shot_radius[sh.kind ? 1 : 0], en.x, en.y,
                                s->params.enemy_radius[en.type])) {
                        ref.emplace_back(i, e);
                    }
                }
            }
            for (const bh_hit_pair_t& p : got) mine.emplace_back(p.shot, p.enemy);
            std::sort(ref.begin(), ref.end());
            std::sort(mine.begin(), mine.end());
            OK(ref == mine);
            total += static_cast<long>(ref.size());
        }
        OK(total > 50);
        OK(s->stats.spatial_overflow == 0);
        OK(s->stats.enemies_killed > 5);
        std::printf("2400 frames: %ld shot/enemy overlaps, %u kills, score %u\n", total, s->stats.enemies_killed, s->score);
        std::free(s);
    }

    /* Steady mode holds exactly N bullets and tests all of them every frame. */
    for (uint16_t n : {128, 256, 512, 1024}) {
        bh_sim_t* s = make_sim(3u);
        bh_sim_set_steady_bullets(s, n);
        OK(s->bullet_count == n);
        for (int f = 0; f < 300; ++f) {
            const uint32_t before = s->stats.bullet_tests;
            bh_sim_update(s, bh_demo_input(s));
            OK(s->bullet_count == n);
            OK(s->stats.bullet_tests - before == n);
            OK(s->player.lives == 3 && s->player.alive);
        }
        OK(s->stats.bullets_dropped == 0);
        /* the bullets spread over the field, not a blob */
        int32_t lo = 1 << 30, hi = -(1 << 30);
        for (uint16_t i = 0; i < s->bullet_count; ++i) { lo = std::min(lo, s->bullets[i].y); hi = std::max(hi, s->bullets[i].y); }
        OK(hi - lo > 200 * 256 || n < 256);
        std::free(s);
    }

    /* Pool capacity: a full pool refuses new bullets and counts them; nothing overflows. */
    {
        bh_sim_t* s = make_sim(9u);
        bh_sim_set_god(s, 1);
        s->stage_timer = 1100;
        for (int f = 0; f < 600; ++f) {
            s->bullet_count = BH_MAX_BULLETS; /* keep it full */
            for (uint16_t i = 0; i < BH_MAX_BULLETS; ++i) { s->bullets[i] = bh_bullet_t{}; s->bullets[i].x = 5000; s->bullets[i].y = 5000; }
            bh_sim_update(s, 0);
            OK(s->bullet_count <= BH_MAX_BULLETS);
        }
        OK(s->stats.bullets_dropped > 0);
        std::free(s);
    }

    /* A hit kills the player once, then invulnerability and respawn protect it. */
    {
        bh_sim_t* s = make_sim(2u);
        s->player.invuln = 0;
        s->bullet_count = 1;
        s->bullets[0] = bh_bullet_t{};
        s->bullets[0].x = s->player.x; s->bullets[0].y = s->player.y; s->bullets[0].shape = BH_SHAPE_ORB;
        bh_sim_update(s, 0);
        OK(!s->player.alive && s->player.lives == 2 && s->bullet_count == 0);
        for (int f = 0; f < 70; ++f) bh_sim_update(s, 0);
        OK(s->player.alive && s->player.invuln > 100);
        s->bullet_count = 1;
        s->bullets[0].x = s->player.x; s->bullets[0].y = s->player.y;
        bh_sim_update(s, 0);
        OK(s->player.alive && s->player.lives == 2); /* invulnerable */
        std::free(s);
    }

    /* Stages: a short stage ends on time, wraps after BH_STAGES, and flags the change. */
    {
        bh_params_t p;
        bh_params_default(&p);
        p.stage_frames = 500;
        p.boss_frame = 250;
        bh_sim_t* s = static_cast<bh_sim_t*>(std::calloc(1, sizeof(bh_sim_t)));
        bh_sim_init(s, &p, 4u);
        bh_sim_set_god(s, 1);
        uint16_t changes = 0;
        for (int f = 0; f < 500 * 4 + 50; ++f) {
            bh_sim_update(s, bh_demo_input(s));
            if (s->stage_changed) {
                ++changes;
                OK(s->stage_serial == changes && s->stage == changes % BH_STAGES);
                OK(s->bullet_count == 0 && s->stage_timer == 0);
            }
        }
        OK(changes >= 4);
        std::free(s);
    }

    std::printf("PASS test_bh_sim\n");
    return 0;
}
