#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <vector>

#include "examples/city_walk/city_format.h"
#include "examples/city_walk/city_grid.h"

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); std::exit(1); } } while (0)

/* ------------------------------------------------------------------ */
/* grid                                                                */
/* ------------------------------------------------------------------ */

static void world_to_chunk_at_corners_and_negatives() {
    /* The grid origin is (-128, -320): that world point is chunk (0,0). */
    city_pos_t p = city_pos_from_world(-128 << 16, -320 << 16);
    CHECK(p.chunk_x == 0 && p.chunk_z == 0 && p.local_x == 0 && p.local_z == 0);
    /* One fixed-point step below it is chunk -1, at the far edge. */
    p = city_pos_from_world((-128 << 16) - 1, (-320 << 16) - 1);
    CHECK(p.chunk_x == -1 && p.chunk_z == -1);
    CHECK(p.local_x == CITY_CHUNK_FX - 1 && p.local_z == CITY_CHUNK_FX - 1);
    /* Far corner: 16 chunks in, exclusive. */
    p = city_pos_from_world((-128 + 16 * 32) << 16, (-320 + 16 * 32) << 16);
    CHECK(p.chunk_x == 16 && p.chunk_z == 16);
    /* A negative world coordinate well inside the grid. */
    p = city_pos_from_world(-100 << 16, 0);
    CHECK(p.chunk_x == 0 && p.local_x == 28 << 16);
    CHECK(p.chunk_z == 10 && p.local_z == 0);
    CHECK(city_chunk_valid(0, 0) && city_chunk_valid(15, 15));
    CHECK(!city_chunk_valid(-1, 0) && !city_chunk_valid(0, 16));
}

static void rebase_crosses_every_edge() {
    city_pos_t p = {4, -3, CITY_CHUNK_FX + 5, -7};
    CHECK(city_pos_rebase(&p) == 1);
    CHECK(p.chunk_x == 5 && p.local_x == 5);
    CHECK(p.chunk_z == -4 && p.local_z == CITY_CHUNK_FX - 7);
    city_pos_t q = {7, 7, 100, 200};
    CHECK(city_pos_rebase(&q) == 0);
    CHECK(q.chunk_x == 7 && q.local_x == 100);
    /* A jump of several chunks in one call still lands in range. */
    city_pos_t far_jump = {0, 0, 5 * CITY_CHUNK_FX + 9, -3 * CITY_CHUNK_FX - 1};
    city_pos_rebase(&far_jump);
    CHECK(far_jump.chunk_x == 5 && far_jump.local_x == 9);
    CHECK(far_jump.chunk_z == -4 && far_jump.local_z == CITY_CHUNK_FX - 1);
    CHECK(far_jump.local_x >= 0 && far_jump.local_x < CITY_CHUNK_FX);
}

static void ring_slot_is_a_bijection_for_every_centre() {
    for (int lod = 0; lod < CITY_LOD_COUNT; ++lod) {
        int r = CITY_RING_RADIUS(lod);
        for (int cz = -20; cz <= 20; ++cz) {
            for (int cx = -20; cx <= 20; ++cx) {
                std::set<int> seen;
                for (int dz = -r; dz <= r; ++dz) {
                    for (int dx = -r; dx <= r; ++dx) {
                        int slot = city_ring_slot((uint8_t)lod, cx + dx, cz + dz);
                        CHECK(slot >= city_ring_base((uint8_t)lod));
                        CHECK(slot < city_ring_base((uint8_t)lod) + CITY_RING_SLOTS(lod));
                        seen.insert(slot);
                    }
                }
                CHECK((int)seen.size() == CITY_RING_SLOTS(lod));
            }
        }
    }
    CHECK(city_ring_base(1) == CITY_RING_SLOTS(0));
    CHECK(city_ring_base(2) == CITY_RING_SLOTS(0) + CITY_RING_SLOTS(1));
    CHECK(CITY_SLOTS_TOTAL == CITY_RING_SLOTS(0) + CITY_RING_SLOTS(1) + CITY_RING_SLOTS(2));
}

static void lod_bands() {
    CHECK(city_lod_for_distance(0) == 0 && city_lod_for_distance(1) == 0);
    CHECK(city_lod_for_distance(2) == 1);
    CHECK(city_lod_for_distance(3) == 2);
    CHECK(city_lod_for_distance(4) == -1);
}

/* The guard for the 16.16 sign flip. Not a spot check: every player chunk,
 * every ring cell of every LOD, every corner of the cell including the
 * permitted overhang, and every player offset inside the chunk. */
static void coordinates_stay_under_the_fixed_point_limit() {
    const int32_t limit = CITY_FX_LIMIT_UNITS << 16;
    int32_t worst = 0;
    for (int pcx = 0; pcx < CITY_GRID_X; ++pcx) {
        for (int lod = 0; lod < CITY_LOD_COUNT; ++lod) {
            int r = CITY_RING_RADIUS(lod);
            for (int dx = -r; dx <= r; ++dx) {
                int cx = pcx + dx;
                if (cx < 0 || cx >= CITY_GRID_X) continue;
                int32_t origin = city_origin_rel_fx(cx, pcx);
                for (int corner = 0; corner < 2; ++corner) {
                    int32_t edge = corner == 0 ? -(CITY_OVERHANG_UNITS << 16)
                                               : CITY_CHUNK_FX + (CITY_OVERHANG_UNITS << 16);
                    int32_t v = origin + edge;
                    CHECK(city_abs32(v) < limit);
                    if (city_abs32(v) > worst) worst = city_abs32(v);
                }
            }
        }
    }
    /* And the difference to the eye anywhere in the player's chunk. */
    for (int32_t local = 0; local < CITY_CHUNK_FX; local += CITY_CHUNK_FX / 8) {
        CHECK(city_abs32(worst - local) < limit);
    }
    CHECK(city_max_abs_coord_fx() == worst);
    CHECK(city_max_abs_coord_fx() == ((4 * CITY_CHUNK_UNITS + CITY_OVERHANG_UNITS) << 16));
    CHECK(city_max_abs_coord_fx() < limit);
}

/* ------------------------------------------------------------------ */
/* residency                                                           */
/* ------------------------------------------------------------------ */

static int resident_count(const city_residency_t* r) {
    int n = 0;
    for (int i = 0; i < CITY_SLOTS_TOTAL; ++i) {
        n += r->slots[i].state == CITY_SLOT_READY || r->slots[i].state == CITY_SLOT_EMPTY;
    }
    return n;
}

static void drain(city_residency_t* r, int pcx, int pcz, uint16_t faces_lod0 = 100,
                  uint16_t faces_lod1 = 40, uint16_t faces_lod2 = 12) {
    for (;;) {
        int slot = city_res_next(r, pcx, pcz);
        if (slot < 0) break;
        int lod = slot < 9 ? 0 : (slot < 34 ? 1 : 2);
        uint16_t faces = lod == 0 ? faces_lod0 : (lod == 1 ? faces_lod1 : faces_lod2);
        CHECK(city_res_complete(r, slot, faces, 0));
    }
}

static void first_recenter_queues_every_in_grid_cell() {
    city_residency_t r;
    city_res_init(&r);
    city_res_recenter(&r, 8, 8);
    CHECK(r.primed);
    CHECK(city_res_pending_count(&r) == CITY_SLOTS_TOTAL);
    /* Nearest chunk first, then finest LOD: the first two are the player's own
     * cell at LOD0, then at LOD1. */
    int a = city_res_next(&r, 8, 8);
    CHECK(a == city_ring_slot(0, 8, 8));
    CHECK(city_res_complete(&r, a, 100, 0));
    int b = city_res_next(&r, 8, 8);
    CHECK(b == city_ring_slot(1, 8, 8));
    drain(&r, 8, 8);
    CHECK(city_res_pending_count(&r) == 0);
    CHECK(r.loads == CITY_SLOTS_TOTAL);
    CHECK(r.evictions == 0);
    /* Idempotent for an unchanged centre: nothing new to load. */
    city_res_recenter(&r, 8, 8);
    CHECK(city_res_pending_count(&r) == 0);
}

static void corner_player_leaves_off_grid_cells_free() {
    city_residency_t r;
    city_res_init(&r);
    city_res_recenter(&r, 0, 0);
    /* LOD0 3x3 around (0,0): only the 2x2 in-grid cells exist. */
    int pending_lod0 = 0;
    for (int i = 0; i < 9; ++i) pending_lod0 += r.slots[i].state == CITY_SLOT_PENDING;
    CHECK(pending_lod0 == 4);
    CHECK(city_res_lookup(&r, 0, -1, 0, 0, 0) == -1);
}

static void crossing_one_chunk_pages_exactly_the_new_edge() {
    city_residency_t r;
    city_res_init(&r);
    city_res_recenter(&r, 8, 8);
    drain(&r, 8, 8);
    uint32_t loads_before = r.loads;
    city_res_recenter(&r, 9, 8);
    /* Moving one step east reveals one new column per ring: 3 + 5 + 7. */
    CHECK(city_res_pending_count(&r) == 3 + 5 + 7);
    drain(&r, 9, 8);
    CHECK(r.loads - loads_before == 15);
    CHECK(r.evictions == 15);
    CHECK(resident_count(&r) == CITY_SLOTS_TOTAL);
}

static void a_slot_reassigned_mid_load_is_not_drawable() {
    city_residency_t r;
    city_res_init(&r);
    city_res_recenter(&r, 8, 8);
    drain(&r, 8, 8);
    int old_slot = city_res_lookup(&r, 0, 7, 8, 8, 8);
    CHECK(old_slot >= 0);
    uint16_t old_generation = r.slots[old_slot].generation;
    /* Player moves two chunks east: (7,8) leaves the LOD0 ring, and its slot
     * now belongs to a different cell. */
    city_res_recenter(&r, 10, 8);
    CHECK(city_res_lookup(&r, 0, 7, 8, 10, 8) == -1);
    CHECK(r.slots[old_slot].generation != old_generation);
    CHECK(r.slots[old_slot].state == CITY_SLOT_PENDING);
    /* Pending is not drawable either. */
    CHECK(city_res_lookup(&r, 0, r.slots[old_slot].chunk_x, r.slots[old_slot].chunk_z, 10, 8) == -1);
}

static void a_long_walk_never_draws_a_stale_cell() {
    city_residency_t r;
    city_res_init(&r);
    int pcx = 1, pcz = 1;
    city_res_recenter(&r, pcx, pcz);
    /* Wander across the whole grid, one chunk at a time, servicing at most ONE
     * pending slot per step -- the runtime budget. Leaves the queue backed up. */
    int step_dx = 1, step_dz = 0;
    for (int step = 0; step < 200; ++step) {
        int nx = pcx + step_dx, nz = pcz + step_dz;
        if (!city_chunk_valid(nx, nz)) {
            int t = step_dx; step_dx = -step_dz; step_dz = t; /* turn */
            nx = pcx + step_dx; nz = pcz + step_dz;
            if (!city_chunk_valid(nx, nz)) { step_dx = -step_dx; step_dz = -step_dz; nx = pcx + step_dx; nz = pcz + step_dz; }
        }
        pcx = nx; pcz = nz;
        city_res_recenter(&r, pcx, pcz);
        int slot = city_res_next(&r, pcx, pcz);
        if (slot >= 0) {
            CHECK(city_res_complete(&r, slot, 50, 0));
        }
        /* Loads minus evictions is exactly what is resident. */
        CHECK((int)(r.loads - r.evictions) == resident_count(&r));
        /* Every resident slot is inside its own ring around the player. */
        for (int lod = 0; lod < CITY_LOD_COUNT; ++lod) {
            int base = city_ring_base((uint8_t)lod);
            for (int i = 0; i < CITY_RING_SLOTS(lod); ++i) {
                const city_slot_t& s = r.slots[base + i];
                if (s.state != CITY_SLOT_READY) continue;
                CHECK(city_cheb(s.chunk_x - pcx, s.chunk_z - pcz) <= CITY_RING_RADIUS(lod));
                CHECK(city_ring_slot((uint8_t)lod, s.chunk_x, s.chunk_z) == base + i);
            }
        }
    }
    /* The queue drains once the player stops. */
    drain(&r, pcx, pcz);
    CHECK(city_res_pending_count(&r) == 0);
    CHECK((int)(r.loads - r.evictions) == resident_count(&r));
}

static void empty_chunks_are_ready_without_faces() {
    city_residency_t r;
    city_res_init(&r);
    city_res_recenter(&r, 4, 4);
    int slot = city_res_next(&r, 4, 4);
    CHECK(city_res_complete(&r, slot, 999, 1));
    CHECK(r.slots[slot].state == CITY_SLOT_EMPTY && r.slots[slot].faces == 0);
    CHECK(city_res_lookup(&r, 0, 4, 4, 4, 4) == slot);
    /* Completing twice, or a free slot, is refused. */
    CHECK(!city_res_complete(&r, slot, 1, 0));
    CHECK(!city_res_complete(&r, -1, 1, 0));
    CHECK(!city_res_complete(&r, CITY_SLOTS_TOTAL, 1, 0));
}

/* ------------------------------------------------------------------ */
/* frame planning                                                      */
/* ------------------------------------------------------------------ */

static city_pos_t centred(int cx, int cz) {
    city_pos_t p = {cx, cz, CITY_CHUNK_FX / 2, CITY_CHUNK_FX / 2};
    return p;
}

static void plan_orders_nearest_first_with_highest_pass_nearest() {
    city_residency_t r;
    city_res_init(&r);
    city_res_recenter(&r, 8, 8);
    drain(&r, 8, 8, 10, 5, 2);
    city_plan_t plan;
    city_pos_t pos = centred(8, 8);
    city_plan_frame(&r, &pos, CITY_FACE_CAP, 0, 0, &plan);
    CHECK(plan.count == 49);
    CHECK(plan.pop_in == 0 && plan.degraded == 0 && plan.budget_skipped == 0);
    /* Own cell first, painted last. */
    CHECK(plan.items[0].chunk_x == 8 && plan.items[0].chunk_z == 8);
    CHECK(plan.items[0].pass == plan.count - 1);
    CHECK(plan.items[plan.count - 1].pass == 0);
    int32_t previous = -1;
    int16_t previous_cz = -32768;
    int16_t previous_cx = -32768;
    for (int i = 0; i < plan.count; ++i) {
        int32_t d2 = city_cell_dist2(plan.items[i].chunk_x - 8, plan.items[i].chunk_z - 8, &pos);
        CHECK(d2 >= previous);
        if (d2 == previous) {
            CHECK(plan.items[i].chunk_z > previous_cz ||
                  (plan.items[i].chunk_z == previous_cz &&
                   plan.items[i].chunk_x > previous_cx));
        }
        previous = d2;
        previous_cz = plan.items[i].chunk_z;
        previous_cx = plan.items[i].chunk_x;
        if (i > 0) CHECK(plan.items[i].pass < plan.items[i - 1].pass);
        CHECK(plan.items[i].pass <= 255);
    }
    /* LOD follows the Chebyshev band. */
    for (int i = 0; i < plan.count; ++i) {
        int d = (int)city_cheb(plan.items[i].chunk_x - 8, plan.items[i].chunk_z - 8);
        CHECK(plan.items[i].lod == city_lod_for_distance(d));
    }
}

static void isqrt_is_exact_and_items_carry_their_distance() {
    for (uint32_t v = 0u; v < 70000u; ++v) {
        uint32_t r = city_isqrt(v);
        CHECK(r * r <= v && (r + 1u) * (r + 1u) > v);
    }
    CHECK(city_isqrt(0xFFFFFFFFu) == 65535u);
    city_residency_t r;
    city_res_init(&r);
    city_res_recenter(&r, 8, 8);
    drain(&r, 8, 8, 10, 5, 2);
    city_pos_t pos = centred(8, 8);
    city_plan_t plan;
    city_plan_frame(&r, &pos, CITY_FACE_CAP, 0, 0, &plan);
    for (int i = 0; i < plan.count; ++i) {
        int32_t d2 = city_cell_dist2(plan.items[i].chunk_x - 8, plan.items[i].chunk_z - 8, &pos);
        CHECK((int32_t)plan.items[i].distance * plan.items[i].distance <= d2);
        if (i > 0) CHECK(plan.items[i].distance >= plan.items[i - 1].distance);
    }
    CHECK(plan.items[0].distance == 0u);
    CHECK(plan.items[plan.count - 1].distance > 90u); /* the LOD2 ring is ~100 units out */
}

static void plan_is_deterministic() {
    city_residency_t r;
    city_res_init(&r);
    city_res_recenter(&r, 5, 9);
    drain(&r, 5, 9, 10, 5, 2);
    city_pos_t pos = centred(5, 9);
    city_plan_t a, b;
    city_plan_frame(&r, &pos, CITY_FACE_CAP, 0, 0, &a);
    city_plan_frame(&r, &pos, CITY_FACE_CAP, 0, 0, &b);
    CHECK(a.count == b.count);
    for (int i = 0; i < a.count; ++i) {
        CHECK(a.items[i].chunk_x == b.items[i].chunk_x && a.items[i].chunk_z == b.items[i].chunk_z);
        CHECK(a.items[i].slot == b.items[i].slot && a.items[i].pass == b.items[i].pass);
    }
}

static int wanted_lod_of(const city_draw_item_t& item, int pcx, int pcz) {
    return city_lod_for_distance(city_cheb(item.chunk_x - pcx, item.chunk_z - pcz));
}

static void budget_is_a_guarantee_not_a_hope() {
    city_residency_t r;
    city_res_init(&r);
    city_res_recenter(&r, 8, 8);
    drain(&r, 8, 8, 176, 64, 24); /* the worst-case size of every LOD */
    city_pos_t pos = centred(8, 8);
    for (uint32_t budget = 0u; budget <= 1600u; budget += 25u) {
        city_plan_t plan;
        city_plan_frame(&r, &pos, budget, 0, 0, &plan);
        CHECK(plan.faces <= budget);
        uint32_t sum = 0u;
        for (int i = 0; i < plan.count; ++i) sum += plan.items[i].faces;
        CHECK(sum == plan.faces);
    }
    /* Even the full worst case fits the real cap, by degrading and dropping. */
    city_plan_t plan;
    city_plan_frame(&r, &pos, CITY_FACE_CAP, 0, 0, &plan);
    CHECK(plan.faces <= CITY_FACE_CAP);
    CHECK(plan.degraded > 0);
}

/* The budget is spent from far to near: nearest-first greed would let the street
 * eat everything and leave holes at the horizon. */
static void budget_is_spent_from_far_to_near() {
    city_residency_t r;
    city_res_init(&r);
    city_res_recenter(&r, 8, 8);
    drain(&r, 8, 8, 100, 40, 12);
    city_pos_t pos = centred(8, 8);
    for (uint32_t budget = 100u; budget <= 1900u; budget += 100u) {
        city_plan_t plan;
        city_plan_frame(&r, &pos, budget, 0, 0, &plan);
        CHECK(plan.faces <= budget);
        std::set<int> planned;
        int32_t worst_planned = -1;
        for (int i = 0; i < plan.count; ++i) {
            planned.insert(plan.items[i].chunk_z * 100 + plan.items[i].chunk_x);
            int32_t d2 = city_cell_dist2(plan.items[i].chunk_x - 8, plan.items[i].chunk_z - 8, &pos);
            if (d2 > worst_planned) worst_planned = d2;
            /* Never more than one LOD coarser than the distance calls for. */
            int wanted = wanted_lod_of(plan.items[i], 8, 8);
            CHECK(plan.items[i].lod >= wanted && plan.items[i].lod <= wanted + 1);
        }
        /* Whatever was dropped is farther than everything that was kept. */
        for (int dz = -3; dz <= 3; ++dz) {
            for (int dx = -3; dx <= 3; ++dx) {
                if (planned.count((8 + dz) * 100 + (8 + dx))) continue;
                CHECK(city_cell_dist2(dx, dz, &pos) >= worst_planned);
            }
        }
        if (plan.budget_skipped > 0) {
            /* A cell is only dropped once nothing could coarsen any further. */
            for (int i = 0; i < plan.count; ++i) {
                int wanted = wanted_lod_of(plan.items[i], 8, 8);
                int limit = wanted + 1 < CITY_LOD_COUNT - 1 ? wanted + 1 : CITY_LOD_COUNT - 1;
                CHECK(plan.items[i].lod == limit);
            }
        }
    }
    /* A moderate squeeze coarsens the outer rings and touches no street cell. */
    city_plan_t plan;
    city_plan_frame(&r, &pos, 1500u, 0, 0, &plan);
    CHECK(plan.degraded > 0 && plan.budget_skipped == 0);
    for (int i = 0; i < plan.count; ++i) {
        if (wanted_lod_of(plan.items[i], 8, 8) == 0) CHECK(plan.items[i].lod == 0);
    }
    /* The player's own cell is the last to give up detail. */
    city_plan_frame(&r, &pos, 900u, 0, 0, &plan);
    CHECK(plan.items[0].chunk_x == 8 && plan.items[0].chunk_z == 8 && plan.items[0].lod == 0);
}

/* A chunk can have detail up close and nothing at LOD2 (its props are too thin
 * to survive the coarsest blocks). Squeezing the budget must drop such a cell,
 * never plan an item for its empty slot: that blob has no bytes to decode. */
static void coarsening_onto_an_empty_lod_drops_the_cell() {
    city_residency_t r;
    city_res_init(&r);
    city_res_recenter(&r, 8, 8);
    for (;;) {
        int slot = city_res_next(&r, 8, 8);
        if (slot < 0) break;
        int lod = slot < 9 ? 0 : (slot < 34 ? 1 : 2);
        CHECK(city_res_complete(&r, slot, lod == 0 ? 100 : 40, lod == 2));
    }
    city_pos_t pos = centred(8, 8);
    for (uint32_t budget = 100u; budget <= 1900u; budget += 100u) {
        city_plan_t plan;
        city_plan_frame(&r, &pos, budget, 0, 0, &plan);
        CHECK(plan.faces <= budget);
        for (int i = 0; i < plan.count; ++i) {
            CHECK(plan.items[i].faces > 0);
            CHECK(r.slots[plan.items[i].slot].state == CITY_SLOT_READY);
        }
    }
}

static void missing_lod_falls_back_to_a_coarser_resident_one() {
    city_residency_t r;
    city_res_init(&r);
    city_res_recenter(&r, 8, 8);
    drain(&r, 8, 8, 10, 5, 2);
    /* The player's own LOD0 cell "un-loads": it was reassigned and not yet
     * paged in. The LOD1 ring still covers it. */
    int slot = city_ring_slot(0, 8, 8);
    r.slots[slot].state = CITY_SLOT_PENDING;
    city_plan_t plan;
    city_pos_t pos = centred(8, 8);
    city_plan_frame(&r, &pos, CITY_FACE_CAP, 0, 0, &plan);
    CHECK(plan.pop_in == 1);
    bool found = false;
    for (int i = 0; i < plan.count; ++i) {
        if (plan.items[i].chunk_x == 8 && plan.items[i].chunk_z == 8) {
            CHECK(plan.items[i].lod == 1);
            found = true;
        }
    }
    CHECK(found);
    /* With nothing resident at all the cell is simply missing, never garbage. */
    city_res_init(&r);
    city_res_recenter(&r, 8, 8);
    city_plan_frame(&r, &pos, CITY_FACE_CAP, 0, 0, &plan);
    CHECK(plan.count == 0 && plan.pop_in == 49);
}

static int reject_east(void* context, int32_t cx, int32_t cz, uint8_t lod, uint16_t slot) {
    (void)cz; (void)lod; (void)slot;
    return cx <= *(int*)context;
}

static void visibility_callback_culls_before_budget() {
    city_residency_t r;
    city_res_init(&r);
    city_res_recenter(&r, 8, 8);
    drain(&r, 8, 8, 10, 5, 2);
    city_plan_t all, culled;
    city_pos_t pos = centred(8, 8);
    int max_cx = 8;
    city_plan_frame(&r, &pos, CITY_FACE_CAP, 0, 0, &all);
    city_plan_frame(&r, &pos, CITY_FACE_CAP, reject_east, &max_cx, &culled);
    CHECK(culled.count < all.count);
    for (int i = 0; i < culled.count; ++i) CHECK(culled.items[i].chunk_x <= 8);
}

/* ------------------------------------------------------------------ */
/* format                                                              */
/* ------------------------------------------------------------------ */

static void put16(std::vector<uint8_t>& v, uint32_t x) { v.push_back((uint8_t)(x >> 8)); v.push_back((uint8_t)x); }
static void put32(std::vector<uint8_t>& v, uint32_t x) { put16(v, x >> 16); put16(v, x & 0xFFFFu); }

struct blob_spec {
    uint16_t vertices = 4, faces = 1, index_override = 0xFFFF, material = 2;
    uint8_t texture = 0; /* face byte 9: texture index + 1 */
    int16_t bbox_min = 0, bbox_max = 32 * 64;
    uint32_t magic = CITY_BLOB_MAGIC;
    bool truncate_faces = false;
    std::vector<std::vector<int16_t>> boxes; /* cx, cy, cz, hx, hy, hz in ticks */
};

static std::vector<uint8_t> make_blob(const blob_spec& s) {
    std::vector<uint8_t> b;
    put32(b, s.magic);
    put16(b, 77);            /* chunk_index */
    b.push_back(0);          /* lod */
    b.push_back(0);          /* flags */
    put16(b, s.vertices);
    put16(b, s.faces);
    for (int i = 0; i < 3; ++i) put16(b, (uint16_t)s.bbox_min);
    for (int i = 0; i < 3; ++i) put16(b, (uint16_t)s.bbox_max);
    uint32_t face_off = 32u + 6u * s.vertices;
    face_off = (face_off + 3u) & ~3u;
    uint32_t box_off = s.boxes.empty() ? 0u : ((face_off + 10u * s.faces + 1u) & ~1u);
    put16(b, 32);
    put16(b, face_off);
    put16(b, box_off);
    put16(b, (uint16_t)s.boxes.size());
    CHECK(b.size() == 32);
    for (uint16_t i = 0; i < s.vertices; ++i) {
        put16(b, (uint16_t)(i * 64));        /* x: i units */
        put16(b, (uint16_t)(int16_t)(-64));  /* y: -1 unit */
        put16(b, (uint16_t)(i * 128));       /* z: 2i units */
    }
    while (b.size() < face_off) b.push_back(0);
    for (uint16_t f = 0; f < s.faces; ++f) {
        put16(b, 0); put16(b, 1); put16(b, 2);
        put16(b, s.index_override == 0xFFFF ? 3 : s.index_override);
        b.push_back((uint8_t)s.material);
        b.push_back(s.texture);
    }
    if (!s.boxes.empty()) {
        while (b.size() < box_off) b.push_back(0);
        for (const auto& box : s.boxes) {
            for (int i = 0; i < 6; ++i) put16(b, (uint16_t)box[i]);
        }
        b[7] = 1; /* collision flag */
    }
    if (s.truncate_faces && !b.empty()) b.resize(b.size() - 4);
    return b;
}

struct scratch {
    sat_vec3_t verts[8];
    uint16_t indices[4 * 4];
    uint16_t materials[4];
    sat_mesh_t mesh;
    scratch() {
        std::memset(verts, 0x5A, sizeof(verts));
        std::memset(indices, 0x5A, sizeof(indices));
        std::memset(materials, 0x5A, sizeof(materials));
        mesh.vertices = verts;
        mesh.indices = indices;
        mesh.vertex_cap = 8;
        mesh.vertex_count = 0;
        mesh.face_cap = 4;
        mesh.face_count = 0;
    }
    bool untouched() const {
        for (unsigned i = 0; i < sizeof(verts); ++i) if (((const uint8_t*)verts)[i] != 0x5A) return false;
        for (unsigned i = 0; i < sizeof(indices); ++i) if (((const uint8_t*)indices)[i] != 0x5A) return false;
        return mesh.vertex_count == 0 && mesh.face_count == 0;
    }
};

static sat_result_t decode(const std::vector<uint8_t>& b, scratch& s, uint16_t materials = 5) {
    return city_blob_decode(b.data(), (uint32_t)b.size(), 96 << 16, -64 << 16, -9 << 16,
                            materials, 0, &s.mesh, s.materials, 4);
}

static void decode_places_vertices_in_the_player_frame() {
    scratch s;
    std::vector<uint8_t> b = make_blob(blob_spec());
    CHECK(decode(b, s) == SAT_OK);
    CHECK(s.mesh.vertex_count == 4 && s.mesh.face_count == 1);
    /* x = origin_rel + i units; y = base_y - 1 unit; z = origin_rel + 2i units. */
    for (int i = 0; i < 4; ++i) {
        CHECK(s.verts[i].x == (96 << 16) + (i << 16));
        CHECK(s.verts[i].y == (-9 << 16) - (1 << 16));
        CHECK(s.verts[i].z == (-64 << 16) + (2 * i << 16));
    }
    CHECK(s.indices[0] == 0 && s.indices[1] == 1 && s.indices[2] == 2 && s.indices[3] == 3);
    CHECK(s.materials[0] == 2);
    /* A material map translates archive indices into the caller's numbering. */
    scratch mapped;
    const uint16_t map[5] = {10, 11, 12, 13, 14};
    CHECK(city_blob_decode(b.data(), (uint32_t)b.size(), 0, 0, 0, 5, map, &mapped.mesh,
                           mapped.materials, 4) == SAT_OK);
    CHECK(mapped.materials[0] == 12);
}

static void decode_refuses_corrupt_blobs_and_writes_nothing() {
    {
        scratch s; blob_spec spec; spec.magic = 0xDEADBEEFu;
        CHECK(decode(make_blob(spec), s) == SAT_ERR_INVALID_ARG && s.untouched());
    }
    {   /* more vertices than the scratch mesh holds */
        scratch s; blob_spec spec; spec.vertices = 9;
        CHECK(decode(make_blob(spec), s) == SAT_ERR_CAPACITY && s.untouched());
    }
    {   /* more faces than the mesh */
        scratch s; blob_spec spec; spec.faces = 5;
        CHECK(decode(make_blob(spec), s) == SAT_ERR_CAPACITY && s.untouched());
    }
    {   /* face index one past the last vertex */
        scratch s; blob_spec spec; spec.index_override = 4;
        CHECK(decode(make_blob(spec), s) == SAT_ERR_INVALID_ARG && s.untouched());
    }
    {   /* material index >= material_count */
        scratch s; blob_spec spec; spec.material = 5;
        CHECK(decode(make_blob(spec), s, 5) == SAT_ERR_INVALID_ARG && s.untouched());
    }
    {   /* triangle poking further outside the chunk than the format promises */
        scratch s; blob_spec spec; spec.bbox_min = (int16_t)(-17 * 64);
        CHECK(decode(make_blob(spec), s) == SAT_ERR_INVALID_ARG && s.untouched());
        scratch t; blob_spec ok; ok.bbox_min = (int16_t)(-16 * 64); ok.bbox_max = (int16_t)(48 * 64);
        CHECK(decode(make_blob(ok), t) == SAT_OK);
        scratch u; blob_spec over; over.bbox_max = (int16_t)(48 * 64 + 1);
        CHECK(decode(make_blob(over), u) == SAT_ERR_INVALID_ARG);
    }
    {   /* face table runs past the buffer */
        scratch s; blob_spec spec; spec.truncate_faces = true;
        CHECK(decode(make_blob(spec), s) == SAT_ERR_INVALID_ARG && s.untouched());
    }
    {   /* shorter than a header, and null arguments */
        scratch s; std::vector<uint8_t> tiny(16, 0);
        CHECK(decode(tiny, s) == SAT_ERR_INVALID_ARG);
        CHECK(city_blob_decode(0, 0, 0, 0, 0, 5, 0, &s.mesh, s.materials, 4) == SAT_ERR_INVALID_ARG);
    }
}

static void crc32_matches_the_standard_vector() {
    const char* text = "123456789";
    CHECK(city_crc32((const uint8_t*)text, 9) == 0xCBF43926u);
    CHECK(city_crc32((const uint8_t*)text, 0) == 0u);
}

static std::vector<uint8_t> make_header(uint32_t ground_bytes = 0, uint32_t blob_base = 0x6000) {
    std::vector<uint8_t> h;
    put32(h, CITY_MAGIC);
    put16(h, 2); put16(h, 0);                      /* version, flags */
    put16(h, 16); put16(h, 16);                    /* grid */
    put32(h, (uint32_t)(-128 << 16)); put32(h, (uint32_t)(-320 << 16));
    put16(h, 32); put16(h, 64);                    /* chunk units, quant */
    h.push_back(3); h.push_back(0);                /* lod_count, reserved */
    put16(h, 12);                                  /* material_count */
    put32(h, 128);                                 /* material_offset */
    put32(h, 176);                                 /* toc_offset */
    put32(h, blob_base);
    put32(h, blob_base + 4096);                    /* total_bytes */
    put32(h, (uint32_t)(-9 << 16)); put32(h, 44 << 16);
    put32(h, 0x1234ABCDu);                         /* toc_crc32 */
    for (int i = 0; i < 3; ++i) put16(h, i == 0 ? 300 : (i == 1 ? 100 : 40));
    for (int i = 0; i < 3; ++i) put16(h, i == 0 ? 150 : (i == 1 ? 60 : 20));
    for (int i = 0; i < 3; ++i) put16(h, i == 0 ? 4000 : (i == 1 ? 1500 : 1000));
    put16(h, 20);                                  /* max_collision_boxes */
    /* ground */
    put32(h, 0x3100);                              /* ground_offset (after the 16-byte TOC) */
    put32(h, ground_bytes);
    put32(h, 0x3100 + ground_bytes);               /* palette offset */
    put32(h, (uint32_t)(-9 << 16));                /* ground_y */
    put16(h, 512); put16(h, 256);                  /* w, h */
    put16(h, 200);                                 /* palette count */
    put16(h, 1); put16(h, 2);                      /* units per dot x, z */
    while (h.size() < 128) h.push_back(0);
    return h;
}

static void header_parse_accepts_a_good_header() {
    city_header_t h;
    std::vector<uint8_t> raw = make_header();
    CHECK(city_header_parse(raw.data(), (uint32_t)raw.size(), &h) == SAT_OK);
    CHECK(h.material_count == 12 && h.toc_offset == 176 && h.blob_base == 0x6000);
    CHECK(h.world_min_y_fx == (-9 << 16) && h.toc_crc32 == 0x1234ABCDu);
    CHECK(h.max_vertices[0] == 300 && h.max_faces[2] == 20 && h.max_blob_bytes[1] == 1500);
    CHECK(h.ground_bytes == 0 && h.ground_units_per_dot_x == 1 && h.ground_units_per_dot_z == 2);
    CHECK(city_header_check_caps(&h) == SAT_OK);
}

static void header_parse_rejects_bad_fields() {
    city_header_t h;
    std::vector<uint8_t> raw = make_header();
    CHECK(city_header_parse(raw.data(), 100, &h) == SAT_ERR_INVALID_ARG); /* short */
    std::vector<uint8_t> bad = raw; bad[0] = 'X';
    CHECK(city_header_parse(bad.data(), 128, &h) == SAT_ERR_INVALID_ARG);
    bad = raw; bad[5] = 1; /* version 1: no facade textures, 12-byte TOC */
    CHECK(city_header_parse(bad.data(), 128, &h) == SAT_ERR_VERSION);
    bad = raw; bad[5] = 3;
    CHECK(city_header_parse(bad.data(), 128, &h) == SAT_ERR_VERSION);
    /* A texture block bigger than the slot's VDP1 VRAM is refused. */
    bad = raw; bad[0x6E] = 0x40; bad[0x6F] = 0x01; /* max_texture_bytes[0] = 16385 */
    CHECK(city_header_parse(bad.data(), 128, &h) == SAT_OK);
    CHECK(city_header_check_caps(&h) == SAT_ERR_CAPACITY);
    /* A texture palette must sit between the TOC and the blobs. */
    bad = raw; bad[0x6D] = 16; /* 16 colours at offset 0 */
    CHECK(city_header_parse(bad.data(), 128, &h) == SAT_ERR_INVALID_ARG);
    bad = raw; bad[0x6D] = 16; bad[0x69] = 0x00; bad[0x6A] = 0x40; bad[0x6B] = 0x00; /* at 0x4000 */
    CHECK(city_header_parse(bad.data(), 128, &h) == SAT_OK && h.texture_palette_count == 16);
    bad = raw; bad[9] = 17; /* grid_x */
    CHECK(city_header_parse(bad.data(), 128, &h) == SAT_ERR_INVALID_ARG);
    bad = raw; bad[0x1B] = 0; bad[0x1A] = 0; /* zero materials */
    CHECK(city_header_parse(bad.data(), 128, &h) == SAT_ERR_CAPACITY);
    bad = raw; bad[0x27] = 0x10; /* unaligned blob_base */
    CHECK(city_header_parse(bad.data(), 128, &h) == SAT_ERR_INVALID_ARG);
    /* Caps larger than the runtime was built for are refused, not truncated. */
    bad = raw; bad[0x39] = 0xFF; bad[0x38] = 0x01; /* max_vertices[0] = 511 */
    CHECK(city_header_parse(bad.data(), 128, &h) == SAT_OK);
    CHECK(city_header_check_caps(&h) == SAT_ERR_CAPACITY);
}

static void header_validates_the_ground_section() {
    city_header_t h;
    std::vector<uint8_t> ok = make_header(512u * 256u, 0x3100 + 512u * 256u + 512u);
    CHECK(city_header_parse(ok.data(), 128, &h) == SAT_OK);
    CHECK(h.ground_width == 512 && h.ground_height == 256 && h.ground_bytes == 512u * 256u);
    CHECK(h.ground_palette_count == 200);
    /* bytes must equal width * height for 8 bpp. */
    std::vector<uint8_t> wrong = make_header(512u * 128u, 0x3100 + 512u * 256u + 512u);
    CHECK(city_header_parse(wrong.data(), 128, &h) == SAT_ERR_INVALID_ARG);
    /* palette must end before the blobs start. */
    std::vector<uint8_t> overlap = make_header(512u * 256u, 0x3100 + 512u * 256u + 16u);
    CHECK(city_header_parse(overlap.data(), 128, &h) == SAT_ERR_INVALID_ARG);
}

static void toc_read_indexes_chunk_major_lod_minor() {
    std::vector<uint8_t> toc(CITY_TOC_ENTRIES * CITY_TOC_ENTRY_BYTES, 0);
    int index = city_toc_index(37, 2);
    CHECK(index == 37 * 3 + 2);
    uint8_t* e = toc.data() + index * CITY_TOC_ENTRY_BYTES;
    std::vector<uint8_t> entry;
    put32(entry, 0x400); put16(entry, 848); put16(entry, 48); put16(entry, 24);
    entry.push_back(1); entry.push_back(0);
    std::memcpy(e, entry.data(), entry.size());
    city_toc_entry_t t;
    CHECK(city_toc_read(toc.data(), 37, 2, 0x6000, 0x6000 + 0x2500, &t) == SAT_OK);
    CHECK(t.offset == 0x400 && t.bytes == 848 && t.vertex_count == 48 && t.face_count == 24);
    CHECK(t.bank == 1 && t.flags == 0 && t.texture_bytes == 0);
    /* The texture block starts at the next 32-byte boundary after the blob and
     * must end inside the file. */
    CHECK(city_texture_block_offset(&t) == 0x400 + 864);
    e[12] = 0x18; /* 0x1800 texture bytes: ends at 0x6000 + 0x2460, inside */
    CHECK(city_toc_read(toc.data(), 37, 2, 0x6000, 0x6000 + 0x2500, &t) == SAT_OK &&
          t.texture_bytes == 0x1800);
    e[12] = 0x20; /* 0x2000: past the end of the file */
    CHECK(city_toc_read(toc.data(), 37, 2, 0x6000, 0x6000 + 0x2500, &t) == SAT_ERR_INVALID_ARG);
    e[12] = 0;
    /* Out of the file, a bad bank, an unaligned offset, an out-of-range index. */
    CHECK(city_toc_read(toc.data(), 37, 2, 0x6000, 0x6000 + 0x400, &t) == SAT_ERR_INVALID_ARG);
    e[10] = 2;
    CHECK(city_toc_read(toc.data(), 37, 2, 0x6000, 0x8000, &t) == SAT_ERR_INVALID_ARG);
    e[10] = 1; e[3] = 0x21;
    CHECK(city_toc_read(toc.data(), 37, 2, 0x6000, 0x8000, &t) == SAT_ERR_INVALID_ARG);
    CHECK(city_toc_read(toc.data(), 256, 0, 0x6000, 0x8000, &t) == SAT_ERR_INVALID_ARG);
    CHECK(city_toc_read(toc.data(), 0, 3, 0x6000, 0x8000, &t) == SAT_ERR_INVALID_ARG);
    /* An empty chunk is valid with a zero-size entry. */
    CHECK(city_toc_read(toc.data(), 0, 0, 0x6000, 0x8000, &t) == SAT_ERR_INVALID_ARG); /* all zero, not flagged empty */
    toc[11] = CITY_TOC_FLAG_EMPTY;
    CHECK(city_toc_read(toc.data(), 0, 0, 0x6000, 0x8000, &t) == SAT_OK && (t.flags & CITY_TOC_FLAG_EMPTY));
}

/* A texture block: u16 count, u16 0, count x {w, h, offset/8, 0}, texels. */
static std::vector<uint8_t> make_texture_block(uint16_t count, uint16_t w, uint16_t h) {
    std::vector<uint8_t> b;
    put16(b, count); put16(b, 0);
    uint32_t at = (4u + 8u * count + 7u) & ~7u;
    for (uint16_t i = 0; i < count; ++i) {
        put16(b, w); put16(b, h); put16(b, (uint16_t)(at / 8u)); put16(b, 0);
        at += ((uint32_t)w * h + 7u) & ~7u;
    }
    b.resize(at, 0x33);
    return b;
}

static void texture_blocks_are_checked_entry_by_entry() {
    uint16_t count = 0;
    std::vector<uint8_t> ok = make_texture_block(3, 16, 10);
    CHECK(city_texture_block_check(ok.data(), (uint32_t)ok.size(), 176, &count) == SAT_OK && count == 3);
    city_texture_entry_t t = city_texture_entry(ok.data(), 2);
    CHECK(t.width == 16 && t.height == 10 && t.offset8 * 8u == 32u + 2u * 160u);
    /* More textures than the slot's faces, a truncated block, a width that is
     * not a VDP1 size, a texture reaching past the block, an empty table. */
    CHECK(city_texture_block_check(ok.data(), (uint32_t)ok.size(), 2, &count) == SAT_ERR_INVALID_ARG);
    CHECK(city_texture_block_check(ok.data(), (uint32_t)ok.size() - 1u, 176, &count) == SAT_ERR_INVALID_ARG);
    std::vector<uint8_t> bad = make_texture_block(1, 12, 10);
    CHECK(city_texture_block_check(bad.data(), (uint32_t)bad.size(), 176, &count) == SAT_ERR_INVALID_ARG);
    bad = make_texture_block(1, 16, 10); bad[8] = 0x01; /* offset/8 = 256 */
    CHECK(city_texture_block_check(bad.data(), (uint32_t)bad.size(), 176, &count) == SAT_ERR_INVALID_ARG);
    bad = make_texture_block(1, 16, 10); bad[8] = 0; bad[9] = 0; /* texels over the table */
    CHECK(city_texture_block_check(bad.data(), (uint32_t)bad.size(), 176, &count) == SAT_ERR_INVALID_ARG);
    bad = make_texture_block(0, 16, 10);
    CHECK(city_texture_block_check(bad.data(), (uint32_t)bad.size(), 176, &count) == SAT_ERR_INVALID_ARG);
}

static void decode_returns_face_textures_and_refuses_a_missing_one() {
    blob_spec spec; spec.texture = 2;
    std::vector<uint8_t> b = make_blob(spec);
    scratch s;
    uint8_t textures[4] = {0xEE, 0xEE, 0xEE, 0xEE};
    CHECK(city_blob_decode_ex(b.data(), (uint32_t)b.size(), 0, 0, 0, 5, 0, &s.mesh, s.materials, 4,
                              2, textures) == SAT_OK);
    CHECK(textures[0] == 2 && textures[1] == 0xEE);
    scratch refused;
    CHECK(city_blob_decode_ex(b.data(), (uint32_t)b.size(), 0, 0, 0, 5, 0, &refused.mesh,
                              refused.materials, 4, 1, textures) == SAT_ERR_INVALID_ARG);
    CHECK(refused.untouched());
    /* A solid face is fine with no texture block at all. */
    spec.texture = 0;
    b = make_blob(spec);
    scratch solid;
    CHECK(city_blob_decode_ex(b.data(), (uint32_t)b.size(), 0, 0, 0, 5, 0, &solid.mesh,
                              solid.materials, 4, 0, textures) == SAT_OK && textures[0] == 0);
}

static void slot_sizes_match_the_documented_byte_budget() {
    CHECK(CITY_BLOB_HEADER_BYTES + CITY_LOD0_VERTS * CITY_VERTEX_BYTES + CITY_LOD0_FACES * CITY_FACE_BYTES
          == CITY_SLOT_BYTES_LOD0);
    CHECK(CITY_BLOB_HEADER_BYTES + CITY_LOD1_VERTS * CITY_VERTEX_BYTES + CITY_LOD1_FACES * CITY_FACE_BYTES
          == CITY_SLOT_BYTES_LOD1);
    CHECK(CITY_BLOB_HEADER_BYTES + CITY_LOD2_VERTS * CITY_VERTEX_BYTES + CITY_LOD2_FACES * CITY_FACE_BYTES
          + CITY_LOD2_BOXES * CITY_BOX_BYTES <= CITY_SLOT_BYTES_LOD2);
    /* Residency footprint promised in the plan: 122.5 KiB. */
    uint32_t total = 9 * CITY_SLOT_BYTES_LOD0 + 25 * CITY_SLOT_BYTES_LOD1 + 49 * CITY_SLOT_BYTES_LOD2;
    CHECK(total == 122u * 1024u + 512u);
    /* The worst archive still fits one 2 MiB bank. */
    CHECK(256u * (CITY_SLOT_BYTES_LOD0 + CITY_SLOT_BYTES_LOD1 + CITY_SLOT_BYTES_LOD2) < CITY_BANK_BYTES);
}


/* ------------------------------------------------------------------ */
/* walking collision                                                   */
/* ------------------------------------------------------------------ */

/* One box: centre (10, 10) units, half extents 4 x 3 units, at chunk origin 0. */
static std::vector<uint8_t> boxed_blob() {
    blob_spec spec;
    spec.boxes.push_back({(int16_t)(10 * 64), 0, (int16_t)(10 * 64), (int16_t)(4 * 64), 0,
                          (int16_t)(3 * 64)});
    return make_blob(spec);
}

static int push(const std::vector<uint8_t>& blob, sat_fx16_t radius, int32_t* x, int32_t* z,
                sat_fx16_t ox = 0, sat_fx16_t oz = 0) {
    return city_blob_push_out(blob.data(), (uint32_t)blob.size(), ox, oz, radius, x, z);
}

static void push_out_leaves_a_free_walker_alone() {
    std::vector<uint8_t> blob = boxed_blob();
    int32_t x = 20 << 16, z = 10 << 16;
    CHECK(push(blob, 1 << 15, &x, &z) == 0);
    CHECK(x == 20 << 16 && z == 10 << 16);
    /* Just outside the radius-padded box: 4 + 0.5 = 4.5 from the centre in x. */
    x = (10 << 16) + (9 << 15) + 1;
    z = 10 << 16;
    CHECK(push(blob, 1 << 15, &x, &z) == 0);
}

static void push_out_takes_the_axis_of_least_penetration() {
    std::vector<uint8_t> blob = boxed_blob();
    /* Penetrating 1 unit from the east face: pushed east, z untouched. */
    int32_t x = (10 << 16) + (3 << 16), z = 10 << 16;
    CHECK(push(blob, 0, &x, &z) >= 1);
    CHECK(x == (10 << 16) + (4 << 16) && z == 10 << 16);
    /* From the north (z smaller): pushed along -z by the shallower penetration. */
    x = 10 << 16;
    z = (10 << 16) - (2 << 16);
    CHECK(push(blob, 0, &x, &z) >= 1);
    CHECK(z == (10 << 16) - (3 << 16) && x == 10 << 16);
}

static void push_out_honours_the_radius_and_chunk_origin() {
    std::vector<uint8_t> blob = boxed_blob();
    /* Same box, but its chunk sits one chunk east of the player's frame. */
    const sat_fx16_t origin = CITY_CHUNK_FX;
    int32_t x = origin + (10 << 16) - (3 << 16) - 100, z = 10 << 16;
    CHECK(push(blob, 1 << 15, &x, &z, origin, 0) >= 1);
    /* Ends exactly at the west face minus the radius. */
    CHECK(x == origin + (10 << 16) - (4 << 16) - (1 << 15));
}

static void push_out_resolves_an_inside_corner() {
    /* Two boxes forming an L; a walker driven into the corner must end outside both. */
    blob_spec spec;
    spec.boxes.push_back({(int16_t)(10 * 64), 0, (int16_t)(2 * 64), (int16_t)(10 * 64), 0, (int16_t)(2 * 64)});
    spec.boxes.push_back({(int16_t)(2 * 64), 0, (int16_t)(10 * 64), (int16_t)(2 * 64), 0, (int16_t)(10 * 64)});
    std::vector<uint8_t> blob = make_blob(spec);
    int32_t x = (3 << 16) + (1 << 15), z = (3 << 16) + (1 << 15); /* both walls reach here */
    push(blob, 1 << 15, &x, &z);
    int32_t x2 = x, z2 = z;
    CHECK(push(blob, 1 << 15, &x2, &z2) == 0); /* a second call finds nothing left to fix */
    CHECK(x2 == x && z2 == z);
}

static void push_out_ignores_blobs_without_boxes_and_bad_data() {
    blob_spec spec;
    std::vector<uint8_t> plain = make_blob(spec);
    int32_t x = 5 << 16, z = 5 << 16;
    CHECK(push(plain, 1 << 15, &x, &z) == 0);
    std::vector<uint8_t> garbage(64, 0xEE);
    CHECK(push(garbage, 1 << 15, &x, &z) == 0);
    CHECK(x == 5 << 16 && z == 5 << 16);
}

/* Cross-language check: parse an archive the Python packer wrote, with the
 * same city_format.h the Saturn build uses. Run as: test_city_walk CITY.BIN */
static int check_real_archive(const char* path) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) { std::fprintf(stderr, "cannot open %s\n", path); return 1; }
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> data((size_t)size);
    CHECK(std::fread(data.data(), 1, data.size(), f) == data.size());
    std::fclose(f);

    city_header_t h;
    CHECK(city_header_parse(data.data(), (uint32_t)data.size(), &h) == SAT_OK);
    CHECK(h.total_bytes == data.size());
    CHECK(city_header_check_caps(&h) == SAT_OK);
    uint32_t toc_end = h.toc_offset + CITY_TOC_ENTRIES * CITY_TOC_ENTRY_BYTES;
    CHECK(city_crc32(data.data() + h.material_offset, toc_end - h.material_offset) == h.toc_crc32);

    static sat_vec3_t verts[CITY_LOD0_VERTS];
    static uint16_t indices[CITY_LOD0_FACES * 4];
    static uint16_t materials[CITY_LOD0_FACES];
    int blobs = 0, empty = 0, textured_blobs = 0, textures_total = 0;
    int32_t worst = 0;
    for (int chunk = 0; chunk < CITY_CHUNK_COUNT; ++chunk) {
        for (uint8_t lod = 0; lod < CITY_LOD_COUNT; ++lod) {
            city_toc_entry_t e;
            CHECK(city_toc_read(data.data() + h.toc_offset, chunk, lod, h.blob_base,
                                h.total_bytes, &e) == SAT_OK);
            if (e.flags & CITY_TOC_FLAG_EMPTY) { ++empty; continue; }
            CHECK(e.offset / CITY_BANK_BYTES == e.bank);
            CHECK((e.offset + e.bytes - 1u) / CITY_BANK_BYTES == e.bank);
            const uint8_t* blob = data.data() + h.blob_base + e.offset;
            sat_mesh_t mesh;
            mesh.vertices = verts;
            mesh.indices = indices;
            mesh.vertex_cap = CITY_LOD0_VERTS;
            mesh.face_cap = CITY_LOD0_FACES;
            mesh.vertex_count = mesh.face_count = 0;
            int32_t cx = chunk % CITY_GRID_X;
            /* The worst player chunk for this ring: as far as the ring reaches. */
            int32_t pcx = cx + CITY_RING_RADIUS(lod);
            CHECK(city_blob_decode(blob, e.bytes, city_origin_rel_fx(cx, pcx),
                                   city_origin_rel_fx(chunk / CITY_GRID_X, chunk / CITY_GRID_X + CITY_RING_RADIUS(lod)),
                                   h.world_min_y_fx, h.material_count, 0, &mesh, materials,
                                   CITY_LOD0_FACES) == SAT_OK);
            CHECK(mesh.vertex_count == e.vertex_count && mesh.face_count == e.face_count);
            city_blob_header_t bh;
            CHECK(city_blob_header_parse(blob, e.bytes, &bh) == SAT_OK);
            CHECK(bh.chunk_index == chunk && bh.lod == lod);
            for (int i = 0; i < mesh.vertex_count; ++i) {
                worst = city_max32(worst, city_max32(city_abs32(verts[i].x), city_abs32(verts[i].z)));
            }
            uint16_t tex_count = 0;
            static const uint16_t tex_cap[3] = {CITY_TEX_BYTES_LOD0, CITY_TEX_BYTES_LOD1,
                                                CITY_TEX_BYTES_LOD2};
            if (e.texture_bytes) {
                const uint8_t* block = data.data() + h.blob_base + city_texture_block_offset(&e);
                CHECK(e.texture_bytes <= tex_cap[lod]);
                CHECK(city_texture_block_check(block, e.texture_bytes, e.face_count, &tex_count) == SAT_OK);
                CHECK((city_texture_block_offset(&e) + e.texture_bytes - 1u) / CITY_BANK_BYTES == e.bank);
                ++textured_blobs;
                textures_total += tex_count;
            }
            static uint8_t face_tex[CITY_LOD0_FACES];
            mesh.vertex_count = mesh.face_count = 0;
            CHECK(city_blob_decode_ex(blob, e.bytes, 0, 0, h.world_min_y_fx, h.material_count, 0,
                                      &mesh, materials, CITY_LOD0_FACES, tex_count, face_tex) == SAT_OK);
            ++blobs;
        }
    }
    CHECK(blobs + empty == CITY_TOC_ENTRIES);
    CHECK(worst < (CITY_FX_LIMIT_UNITS << 16));
    std::printf("PASS: archive %s: %d blobs (%d textured, %d textures), %d empty entries, "
                "worst |coord| %.1f units\n",
                path, blobs, textured_blobs, textures_total, empty, worst / 65536.0);
    return 0;
}

int main(int argc, char** argv) {
    if (argc > 1) return check_real_archive(argv[1]);
    world_to_chunk_at_corners_and_negatives();
    rebase_crosses_every_edge();
    ring_slot_is_a_bijection_for_every_centre();
    lod_bands();
    coordinates_stay_under_the_fixed_point_limit();
    first_recenter_queues_every_in_grid_cell();
    corner_player_leaves_off_grid_cells_free();
    crossing_one_chunk_pages_exactly_the_new_edge();
    a_slot_reassigned_mid_load_is_not_drawable();
    a_long_walk_never_draws_a_stale_cell();
    empty_chunks_are_ready_without_faces();
    plan_orders_nearest_first_with_highest_pass_nearest();
    isqrt_is_exact_and_items_carry_their_distance();
    plan_is_deterministic();
    budget_is_a_guarantee_not_a_hope();
    budget_is_spent_from_far_to_near();
    missing_lod_falls_back_to_a_coarser_resident_one();
    coarsening_onto_an_empty_lod_drops_the_cell();
    visibility_callback_culls_before_budget();
    decode_places_vertices_in_the_player_frame();
    decode_refuses_corrupt_blobs_and_writes_nothing();
    crc32_matches_the_standard_vector();
    header_parse_accepts_a_good_header();
    header_parse_rejects_bad_fields();
    header_validates_the_ground_section();
    toc_read_indexes_chunk_major_lod_minor();
    slot_sizes_match_the_documented_byte_budget();
    texture_blocks_are_checked_entry_by_entry();
    decode_returns_face_textures_and_refuses_a_missing_one();
    push_out_leaves_a_free_walker_alone();
    push_out_takes_the_axis_of_least_penetration();
    push_out_honours_the_radius_and_chunk_origin();
    push_out_resolves_an_inside_corner();
    push_out_ignores_blobs_without_boxes_and_bad_data();
    std::printf("PASS: test_city_walk.cpp (34 tests)\n");
    return 0;
}
