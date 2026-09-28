#ifndef CITY_GRID_H
#define CITY_GRID_H

/* Pure grid, residency and frame-planning logic for city_walk. No hardware, no
 * library calls: tests/host/test_city_walk.cpp exercises all of it natively.
 *
 * WORLD -> GRID. The city is 488 x 488 units, and 16.16 fixed point flips its
 * sign above ~181 units, so no coordinate ever holds the whole city. The world
 * is cut into 16 x 16 chunks of 32 units. The player is a (chunk, local)
 * pair, and every vertex is decoded relative to the PLAYER'S chunk corner, so
 * the worst magnitude anywhere is the far edge of the outermost ring cell plus
 * the overhang a chunk's triangles may poke past its own border:
 *     (radius 3 + 1 chunk) * 32 + CITY_OVERHANG_UNITS = 144 units < 176.
 * city_max_abs_coord_fx() computes that from the constants and the test pins
 * it, so changing a ring radius or the overhang cannot silently reintroduce
 * the sign flip.
 */

#include <stdint.h>

#define CITY_GRID_X 16
#define CITY_GRID_Z 16
#define CITY_CHUNK_COUNT (CITY_GRID_X * CITY_GRID_Z)
#define CITY_CHUNK_UNITS 32
#define CITY_CHUNK_FX (CITY_CHUNK_UNITS << 16)
#define CITY_ORIGIN_X_UNITS (-128)
#define CITY_ORIGIN_Z_UNITS (-320)
#define CITY_LOD_COUNT 3

/* int16 vertex ticks: 64 per unit, decoded with a shift (16.16 / 64). */
#define CITY_QUANT_SCALE 64
#define CITY_QUANT_SHIFT 10
/* How far a chunk's triangles may extend past the chunk border. The chunker
 * clips or rejects anything wider; city_blob_decode rejects a blob that lies. */
#define CITY_OVERHANG_UNITS 16
/* Hard sign-flip ceiling with margin (see the project's fx16 overflow note). */
#define CITY_FX_LIMIT_UNITS 176

/* Solid faces the renderer may submit per frame (measured, plan section 0). */
#define CITY_FACE_CAP 500u

/* Rings are direct-mapped: LOD l covers Chebyshev radius l + 1. */
#define CITY_RING_RADIUS(lod) ((int32_t)(lod) + 1)
#define CITY_RING_DIM(lod) (2 * CITY_RING_RADIUS(lod) + 1)
#define CITY_RING_SLOTS(lod) (CITY_RING_DIM(lod) * CITY_RING_DIM(lod))
#define CITY_SLOTS_TOTAL (9 + 25 + 49)
#define CITY_MAX_DRAW_ITEMS 49

enum { CITY_SLOT_FREE = 0, CITY_SLOT_PENDING = 1, CITY_SLOT_READY = 2, CITY_SLOT_EMPTY = 3 };

typedef struct city_pos {
    int32_t chunk_x, chunk_z; /* grid coordinates; valid cells are 0..15 */
    int32_t local_x, local_z; /* 16.16 within the chunk, 0 <= local < CITY_CHUNK_FX */
} city_pos_t;

typedef struct city_slot {
    int16_t chunk_x, chunk_z;
    uint16_t generation; /* bumped whenever the slot is (re)assigned */
    uint16_t faces;      /* set by city_res_complete; 0 for an empty chunk */
    uint8_t state;
    uint8_t reserved;
} city_slot_t;

typedef struct city_residency {
    city_slot_t slots[CITY_SLOTS_TOTAL];
    uint16_t generation;
    uint32_t loads;
    uint32_t evictions;
    uint8_t primed; /* recenter has run at least once */
} city_residency_t;

typedef struct city_draw_item {
    int16_t chunk_x, chunk_z;
    uint16_t slot;   /* global slot index */
    uint16_t faces;
    uint8_t lod;
    uint8_t pass;    /* painter pass: nearest cell highest */
    uint16_t distance; /* units from the player to the cell centre (for the fade) */
} city_draw_item_t;

typedef struct city_plan {
    city_draw_item_t items[CITY_MAX_DRAW_ITEMS];
    uint16_t count;
    uint16_t pop_in;         /* cells drawn coarser than wanted, or missing */
    uint16_t degraded;       /* LOD0 -> LOD1 to honour the face budget */
    uint16_t budget_skipped; /* cells dropped entirely by the face budget */
    uint32_t faces;
} city_plan_t;

/* Return 0 to skip a cell (frustum reject). May be NULL. */
typedef int (*city_visible_fn)(void* context, int32_t chunk_x, int32_t chunk_z,
                               uint8_t lod, uint16_t slot);

static inline int32_t city_abs32(int32_t v) { return v < 0 ? -v : v; }
static inline int32_t city_max32(int32_t a, int32_t b) { return a > b ? a : b; }

static inline int32_t city_pmod(int32_t v, int32_t n) {
    int32_t r = v % n;
    return r < 0 ? r + n : r;
}

/* Floor division by CITY_CHUNK_FX, valid for negatives. */
static inline int32_t city_floor_chunk(int32_t world_fx_from_origin) {
    int32_t q = world_fx_from_origin / CITY_CHUNK_FX;
    if ((world_fx_from_origin % CITY_CHUNK_FX) < 0) --q;
    return q;
}

static inline city_pos_t city_pos_from_world(int32_t world_x_fx, int32_t world_z_fx) {
    int32_t rx = world_x_fx - (CITY_ORIGIN_X_UNITS * 65536);
    int32_t rz = world_z_fx - (CITY_ORIGIN_Z_UNITS * 65536);
    city_pos_t p;
    p.chunk_x = city_floor_chunk(rx);
    p.chunk_z = city_floor_chunk(rz);
    p.local_x = rx - p.chunk_x * CITY_CHUNK_FX;
    p.local_z = rz - p.chunk_z * CITY_CHUNK_FX;
    return p;
}

/* Returns 1 when the position crossed into another chunk. */
static inline int city_pos_rebase(city_pos_t* p) {
    int moved = 0;
    while (p->local_x >= CITY_CHUNK_FX) { p->local_x -= CITY_CHUNK_FX; ++p->chunk_x; moved = 1; }
    while (p->local_x < 0)              { p->local_x += CITY_CHUNK_FX; --p->chunk_x; moved = 1; }
    while (p->local_z >= CITY_CHUNK_FX) { p->local_z -= CITY_CHUNK_FX; ++p->chunk_z; moved = 1; }
    while (p->local_z < 0)              { p->local_z += CITY_CHUNK_FX; --p->chunk_z; moved = 1; }
    return moved;
}

static inline int city_chunk_valid(int32_t cx, int32_t cz) {
    return cx >= 0 && cx < CITY_GRID_X && cz >= 0 && cz < CITY_GRID_Z;
}

static inline int32_t city_chunk_index(int32_t cx, int32_t cz) {
    return cz * CITY_GRID_X + cx;
}

/* Where a chunk's min corner sits relative to the player's chunk corner. This
 * is the value a blob's vertices are offset by; it changes only when the
 * player crosses a chunk border, never per frame. */
static inline int32_t city_origin_rel_fx(int32_t chunk, int32_t player_chunk) {
    return (chunk - player_chunk) * CITY_CHUNK_FX;
}

static inline int32_t city_ring_base(uint8_t lod) {
    return lod == 0 ? 0 : (lod == 1 ? 9 : 34);
}

/* Direct-mapped slot: N consecutive absolute coordinates hit N distinct
 * residues, so a ring never collides with itself whatever its centre. */
static inline uint16_t city_ring_slot(uint8_t lod, int32_t cx, int32_t cz) {
    int32_t n = CITY_RING_DIM(lod);
    return (uint16_t)(city_ring_base(lod) + city_pmod(cz, n) * n + city_pmod(cx, n));
}

static inline int32_t city_cheb(int32_t dcx, int32_t dcz) {
    return city_max32(city_abs32(dcx), city_abs32(dcz));
}

/* LOD wanted at a Chebyshev chunk distance; -1 outside every ring. */
static inline int city_lod_for_distance(int32_t d) {
    if (d <= 1) return 0;
    if (d <= 2) return 1;
    if (d <= 3) return 2;
    return -1;
}

/* Worst coordinate magnitude any decoded vertex can take, from the constants. */
static inline int32_t city_max_abs_coord_fx(void) {
    int32_t worst = 0;
    for (int lod = 0; lod < CITY_LOD_COUNT; ++lod) {
        int32_t r = CITY_RING_RADIUS(lod);
        for (int32_t d = -r; d <= r; ++d) {
            int32_t lo = d * CITY_CHUNK_FX - (CITY_OVERHANG_UNITS << 16);
            int32_t hi = (d + 1) * CITY_CHUNK_FX + (CITY_OVERHANG_UNITS << 16);
            worst = city_max32(worst, city_max32(city_abs32(lo), city_abs32(hi)));
        }
    }
    return worst;
}

/* ------------------------------------------------------------------ */
/* Residency state machine                                             */
/* ------------------------------------------------------------------ */

static inline void city_res_init(city_residency_t* res) {
    for (int i = 0; i < CITY_SLOTS_TOTAL; ++i) {
        res->slots[i].chunk_x = 0;
        res->slots[i].chunk_z = 0;
        res->slots[i].generation = 0;
        res->slots[i].faces = 0;
        res->slots[i].state = CITY_SLOT_FREE;
        res->slots[i].reserved = 0;
    }
    res->generation = 0;
    res->loads = 0;
    res->evictions = 0;
    res->primed = 0;
}

static inline void city_res_free_slot(city_residency_t* res, city_slot_t* slot) {
    if (slot->state == CITY_SLOT_READY || slot->state == CITY_SLOT_EMPTY) ++res->evictions;
    slot->state = CITY_SLOT_FREE;
    slot->faces = 0;
}

/* Re-derives every ring around the player's chunk. A slot already holding the
 * right cell is left alone (READY stays READY); a slot that must change hands
 * counts an eviction if it held data, and becomes PENDING under a new
 * generation. Cells outside the grid free their slot. Cheap enough to call on
 * every chunk crossing, and idempotent for an unchanged centre. */
static inline void city_res_recenter(city_residency_t* res, int32_t pcx, int32_t pcz) {
    for (int lod = 0; lod < CITY_LOD_COUNT; ++lod) {
        int32_t r = CITY_RING_RADIUS(lod);
        for (int32_t dz = -r; dz <= r; ++dz) {
            for (int32_t dx = -r; dx <= r; ++dx) {
                int32_t cx = pcx + dx;
                int32_t cz = pcz + dz;
                city_slot_t* slot = &res->slots[city_ring_slot((uint8_t)lod, cx, cz)];
                if (!city_chunk_valid(cx, cz)) {
                    /* A slot maps exactly one position of its ring, so an
                     * off-grid cell owns it alone: whatever it held is out. */
                    city_res_free_slot(res, slot);
                    continue;
                }
                if (slot->state != CITY_SLOT_FREE && slot->chunk_x == cx && slot->chunk_z == cz) {
                    continue;
                }
                city_res_free_slot(res, slot);
                slot->chunk_x = (int16_t)cx;
                slot->chunk_z = (int16_t)cz;
                slot->generation = ++res->generation;
                slot->state = CITY_SLOT_PENDING;
            }
        }
    }
    res->primed = 1;
}

/* The pending slot to service next: nearest chunk first, then finest LOD.
 * Returns -1 when nothing is pending. */
static inline int city_res_next(const city_residency_t* res, int32_t pcx, int32_t pcz) {
    int best = -1;
    int32_t best_key = 0;
    for (int lod = 0; lod < CITY_LOD_COUNT; ++lod) {
        int32_t base = city_ring_base((uint8_t)lod);
        int32_t count = CITY_RING_SLOTS(lod);
        for (int32_t i = 0; i < count; ++i) {
            const city_slot_t* slot = &res->slots[base + i];
            if (slot->state != CITY_SLOT_PENDING) continue;
            int32_t key = city_cheb(slot->chunk_x - pcx, slot->chunk_z - pcz) * 4 + lod;
            if (best < 0 || key < best_key) {
                best = (int)(base + i);
                best_key = key;
            }
        }
    }
    return best;
}

/* Marks a PENDING slot serviced. `empty` is the TOC's empty-chunk flag. */
static inline int city_res_complete(city_residency_t* res, int slot_index,
                                    uint16_t faces, int empty) {
    if (slot_index < 0 || slot_index >= CITY_SLOTS_TOTAL) return 0;
    city_slot_t* slot = &res->slots[slot_index];
    if (slot->state != CITY_SLOT_PENDING) return 0;
    slot->faces = empty ? 0u : faces;
    slot->state = empty ? CITY_SLOT_EMPTY : CITY_SLOT_READY;
    ++res->loads;
    return 1;
}

static inline int city_res_pending_count(const city_residency_t* res) {
    int n = 0;
    for (int i = 0; i < CITY_SLOTS_TOTAL; ++i) n += res->slots[i].state == CITY_SLOT_PENDING;
    return n;
}

/* The slot for exactly this cell at this LOD, or -1 when it is absent, holds
 * a different cell, or is not loaded. The cell comparison is what makes a slot
 * that was reassigned mid-frame impossible to draw. */
static inline int city_res_lookup(const city_residency_t* res, uint8_t lod,
                                  int32_t cx, int32_t cz, int32_t pcx, int32_t pcz) {
    if (!city_chunk_valid(cx, cz)) return -1;
    if (city_cheb(cx - pcx, cz - pcz) > CITY_RING_RADIUS(lod)) return -1;
    int idx = city_ring_slot(lod, cx, cz);
    const city_slot_t* slot = &res->slots[idx];
    if (slot->chunk_x != cx || slot->chunk_z != cz) return -1;
    if (slot->state != CITY_SLOT_READY && slot->state != CITY_SLOT_EMPTY) return -1;
    return idx;
}

/* ------------------------------------------------------------------ */
/* Frame planning                                                      */
/* ------------------------------------------------------------------ */

/* Painter passes: higher paints last, so the NEAREST cell gets the highest.
 * The ground is not a VDP1 face (it is a VDP2 plane), so one pass per cell is
 * enough: faces only ever compete with faces of the same cell or a neighbour. */

typedef struct city_cell {
    int16_t cx, cz;
    int32_t dist2; /* squared units from player to the chunk centre */
} city_cell_t;

/* Total deterministic ordering for frame cells: nearest first, then (cz,cx).
 * A max-heap gives O(N log N) worst-case without scratch. CITY_MAX_DRAW_ITEMS
 * is small today, but keeping the planner non-quadratic prevents ring growth
 * from turning a content-size change into a frame-time cliff. */
static inline int city_cell_less(const city_cell_t* a, const city_cell_t* b) {
    return a->dist2 < b->dist2 ||
           (a->dist2 == b->dist2 &&
            (a->cz < b->cz || (a->cz == b->cz && a->cx < b->cx)));
}

static inline void city_cell_sift_down(
    city_cell_t* cells, int root, int end) {
    city_cell_t value = cells[root];
    for (;;) {
        int child = root * 2 + 1;
        if (child >= end) break;
        if (child + 1 < end &&
            city_cell_less(&cells[child], &cells[child + 1])) ++child;
        if (!city_cell_less(&value, &cells[child])) break;
        cells[root] = cells[child];
        root = child;
    }
    cells[root] = value;
}

static inline void city_cells_sort(city_cell_t* cells, int count) {
    if (count < 2) return;
    for (int i = count / 2; i > 0; --i)
        city_cell_sift_down(cells, i - 1, count);
    for (int end = count - 1; end > 0; --end) {
        city_cell_t tmp = cells[0];
        cells[0] = cells[end];
        cells[end] = tmp;
        city_cell_sift_down(cells, 0, end);
    }
}

/* Integer square root, rounded down. */
static inline uint32_t city_isqrt(uint32_t value) {
    uint32_t root = 0u;
    uint32_t bit = 1u << 30;
    while (bit > value) bit >>= 2;
    while (bit != 0u) {
        if (value >= root + bit) {
            value -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return root;
}

static inline int32_t city_cell_dist2(int32_t dcx, int32_t dcz, const city_pos_t* pos) {
    int32_t dx = ((dcx * CITY_CHUNK_FX) + (CITY_CHUNK_FX >> 1) - pos->local_x) >> 16;
    int32_t dz = ((dcz * CITY_CHUNK_FX) + (CITY_CHUNK_FX >> 1) - pos->local_z) >> 16;
    return dx * dx + dz * dz;
}

/* Builds this frame's draw list, nearest first: for every in-grid cell of the
 * widest ring it picks the wanted LOD, falls back to a coarser resident LOD
 * when that one has not paged in yet (counted as pop_in), rejects cells the
 * visibility callback refuses, and enforces `face_budget` far to near: the
 * farthest cells coarsen first (at most one LOD below what their distance
 * calls for), then the farthest are dropped. The budget makes CITY_FACE_CAP a
 * guarantee instead of a hope. */
static inline void city_plan_frame(const city_residency_t* res, const city_pos_t* pos,
                                   uint32_t face_budget, city_visible_fn visible,
                                   void* context, city_plan_t* out) {
    city_cell_t cells[CITY_MAX_DRAW_ITEMS];
    int n = 0;
    out->count = 0;
    out->pop_in = 0;
    out->degraded = 0;
    out->budget_skipped = 0;
    out->faces = 0;

    const int32_t r = CITY_RING_RADIUS(CITY_LOD_COUNT - 1);
    for (int32_t dz = -r; dz <= r; ++dz) {
        for (int32_t dx = -r; dx <= r; ++dx) {
            if (!city_chunk_valid(pos->chunk_x + dx, pos->chunk_z + dz)) continue;
            city_cell_t cell;
            cell.cx = (int16_t)(pos->chunk_x + dx);
            cell.cz = (int16_t)(pos->chunk_z + dz);
            cell.dist2 = city_cell_dist2(dx, dz, pos);
            cells[n++] = cell;
        }
    }
    city_cells_sort(cells, n);

    /* Pick every cell's LOD as if there were no budget, nearest first. */
    struct { int16_t cell; int8_t wanted, chosen; uint16_t slot; uint8_t alive; } cand[CITY_MAX_DRAW_ITEMS];
    int m = 0;
    uint32_t total = 0u;
    for (int i = 0; i < n; ++i) {
        int32_t dcx = cells[i].cx - pos->chunk_x;
        int32_t dcz = cells[i].cz - pos->chunk_z;
        int wanted = city_lod_for_distance(city_cheb(dcx, dcz));
        if (wanted < 0) continue;
        int chosen = -1;
        int slot = -1;
        for (int lod = wanted; lod < CITY_LOD_COUNT; ++lod) {
            slot = city_res_lookup(res, (uint8_t)lod, cells[i].cx, cells[i].cz,
                                   pos->chunk_x, pos->chunk_z);
            if (slot >= 0) { chosen = lod; break; }
        }
        if (chosen < 0) { ++out->pop_in; continue; }
        if (chosen > wanted) ++out->pop_in;
        if (res->slots[slot].state == CITY_SLOT_EMPTY) continue;
        if (visible != 0 && !visible(context, cells[i].cx, cells[i].cz, (uint8_t)chosen, (uint16_t)slot)) {
            continue;
        }
        cand[m].cell = (int16_t)i;
        cand[m].wanted = (int8_t)wanted;
        cand[m].chosen = (int8_t)chosen;
        cand[m].slot = (uint16_t)slot;
        cand[m].alive = 1u;
        total += res->slots[slot].faces;
        ++m;
    }

    /* Over budget: spend it from FAR to NEAR. The farthest cell that can go one
     * LOD coarser (never more than one below what its distance calls for) does
     * so; when nothing can, the farthest cell is dropped. Near cells therefore
     * keep their detail and the horizon thins out before the street does,
     * instead of nearest-first greed eating the whole budget and leaving holes
     * at the far end. */
    /* Each candidate can degrade at most once: chosen may advance only as far
     * as wanted+1. The old loop restarted a reverse scan after every single
     * change, making the planner O(N^2). One far-to-near degradation pass
     * visits candidates in exactly the same priority order; if that is still
     * over budget, one far-to-near drop pass reproduces the old policy. */
    for (int j = m - 1; j >= 0 && total > face_budget; --j) {
        int limit = cand[j].wanted + 1;
        if (!cand[j].alive) continue;
        if (limit > CITY_LOD_COUNT - 1) limit = CITY_LOD_COUNT - 1;
        for (int lod = cand[j].chosen + 1; lod <= limit; ++lod) {
            int ns = city_res_lookup(res, (uint8_t)lod,
                                     cells[cand[j].cell].cx,
                                     cells[cand[j].cell].cz,
                                     pos->chunk_x, pos->chunk_z);
            if (ns < 0) continue;
            total -= res->slots[cand[j].slot].faces;
            total += res->slots[ns].faces;
            cand[j].chosen = (int8_t)lod;
            cand[j].slot = (uint16_t)ns;
            ++out->degraded;
            break;
        }
    }
    for (int j = m - 1; j >= 0 && total > face_budget; --j) {
        if (!cand[j].alive) continue;
        total -= res->slots[cand[j].slot].faces;
        cand[j].alive = 0u;
        ++out->budget_skipped;
    }

    for (int j = 0; j < m; ++j) {
        city_draw_item_t* item;
        if (!cand[j].alive) continue;
        /* Coarsening can land on an EMPTY slot (a chunk whose props vanish at
         * LOD2): that cell simply has nothing to draw at this distance. */
        if (res->slots[cand[j].slot].state == CITY_SLOT_EMPTY) continue;
        item = &out->items[out->count++];
        item->chunk_x = cells[cand[j].cell].cx;
        item->chunk_z = cells[cand[j].cell].cz;
        item->slot = cand[j].slot;
        item->faces = res->slots[cand[j].slot].faces;
        item->lod = (uint8_t)cand[j].chosen;
        item->pass = 0;
        item->distance = (uint16_t)city_isqrt((uint32_t)cells[cand[j].cell].dist2);
        out->faces += item->faces;
    }

    /* Nearest is item 0 and must paint last: highest pass. */
    for (int i = 0; i < out->count; ++i) {
        out->items[i].pass = (uint8_t)(out->count - 1 - i);
    }
}

#endif /* CITY_GRID_H */
