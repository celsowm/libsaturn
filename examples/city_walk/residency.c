/* city_walk residency: the cart -> work RAM (and VDP1 VRAM) half of the stream.
 *
 * The policy (which slot holds which cell, what to fetch next, what is safe
 * to draw) is the pure state machine in city_grid.h, tested natively. This file
 * only owns the memory and does the copies.
 *
 * Slots are direct-mapped and fixed-size, so there is no allocator and no
 * fragmentation: a cell always lands at the same address for a given ring
 * position, and eviction is simply "this slot now belongs to another cell".
 * The renderer never blocks on a page-in: an unloaded cell falls back to a
 * coarser resident LOD and is counted as a pop-in.
 *
 * Facade textures. Every slot also owns a fixed span of VDP1 VRAM, reserved
 * once (9 x 28 KiB + 25 x 4 KiB + 49 x 1 KiB = 401 KiB). A blob's texture
 * block goes there unchanged, so a texture's character address is the span
 * plus its offset. The copy waits until just before sat_end_frame: the VDP1
 * may still be drawing the previous list, which could read the texels this
 * slot held before, and sat_end_frame waits for that list anyway. Until the
 * copy is done the slot stays PENDING, so it is never drawn half-loaded.
 */
#include <stdint.h>

#include "saturn/example_util.h"
#include "saturn/vdp1.h"

#include "city_walk.h"

uint8_t g_ring_pool[CITY_POOL_BYTES] __attribute__((aligned(32), section(".wram_l")));
city_residency_t g_res;

static uint16_t g_slot_bytes[CITY_SLOTS_TOTAL];

#define TEX_TABLE_ENTRIES (9u * CITY_LOD0_FACES + 25u * CITY_LOD1_FACES + 49u * CITY_LOD2_FACES)
#define TEX_VRAM_BYTES (9u * CITY_TEX_BYTES_LOD0 + 25u * CITY_TEX_BYTES_LOD1 + 49u * CITY_TEX_BYTES_LOD2)

static city_slot_texture_t g_tex_table[TEX_TABLE_ENTRIES] CITY_WRAM_L;
static uint16_t g_tex_count[CITY_SLOTS_TOTAL];
static uint32_t g_tex_vram;

/* The slot served this frame whose texture block still has to be copied. */
static struct {
    int active;
    uint16_t slot;
    uint16_t faces;
    uint16_t bytes;
    const uint8_t* src;
} g_pending;

static uint8_t slot_lod(uint16_t slot) {
    return slot < 9u ? 0u : (slot < 34u ? 1u : 2u);
}

static uint8_t* slot_ptr(uint16_t slot) {
    if (slot < 9u) return g_ring_pool + slot * CITY_SLOT_BYTES_LOD0;
    if (slot < 34u) {
        return g_ring_pool + 9u * CITY_SLOT_BYTES_LOD0 +
               (uint32_t)(slot - 9u) * CITY_SLOT_BYTES_LOD1;
    }
    return g_ring_pool + 9u * CITY_SLOT_BYTES_LOD0 + 25u * CITY_SLOT_BYTES_LOD1 +
           (uint32_t)(slot - 34u) * CITY_SLOT_BYTES_LOD2;
}

static uint16_t slot_capacity(uint16_t slot) {
    uint8_t lod = slot_lod(slot);
    return lod == 0u ? CITY_SLOT_BYTES_LOD0 : (lod == 1u ? CITY_SLOT_BYTES_LOD1
                                                        : CITY_SLOT_BYTES_LOD2);
}

static uint16_t slot_tex_capacity(uint16_t slot) {
    uint8_t lod = slot_lod(slot);
    return lod == 0u ? CITY_TEX_BYTES_LOD0 : (lod == 1u ? CITY_TEX_BYTES_LOD1
                                                       : CITY_TEX_BYTES_LOD2);
}

/* Same layout as the ring pool: LOD0 slots first, then LOD1, then LOD2. */
static uint32_t slot_tex_vram(uint16_t slot) {
    if (slot < 9u) return g_tex_vram + slot * CITY_TEX_BYTES_LOD0;
    if (slot < 34u) {
        return g_tex_vram + 9u * CITY_TEX_BYTES_LOD0 + (uint32_t)(slot - 9u) * CITY_TEX_BYTES_LOD1;
    }
    return g_tex_vram + 9u * CITY_TEX_BYTES_LOD0 + 25u * CITY_TEX_BYTES_LOD1 +
           (uint32_t)(slot - 34u) * CITY_TEX_BYTES_LOD2;
}

static city_slot_texture_t* slot_tex_table(uint16_t slot) {
    if (slot < 9u) return g_tex_table + slot * CITY_LOD0_FACES;
    if (slot < 34u) return g_tex_table + 9u * CITY_LOD0_FACES + (uint32_t)(slot - 9u) * CITY_LOD1_FACES;
    return g_tex_table + 9u * CITY_LOD0_FACES + 25u * CITY_LOD1_FACES +
           (uint32_t)(slot - 34u) * CITY_LOD2_FACES;
}

static uint16_t slot_face_cap(uint16_t slot) {
    uint8_t lod = slot_lod(slot);
    return lod == 0u ? CITY_LOD0_FACES : (lod == 1u ? CITY_LOD1_FACES : CITY_LOD2_FACES);
}

void residency_init(void) {
    city_res_init(&g_res);
    for (uint32_t i = 0u; i < CITY_SLOTS_TOTAL; ++i) {
        g_slot_bytes[i] = 0u;
        g_tex_count[i] = 0u;
    }
    g_pending.active = 0;
    if (g_archive.header.texture_palette_count != 0u && g_tex_vram == 0u) {
        sat_example_must(sat_vdp1_vram_reserve(TEX_VRAM_BYTES, &g_tex_vram));
        g_city.texture_vram_base = g_tex_vram;
    }
}

void residency_recenter(const city_pos_t* pos) {
    city_res_recenter(&g_res, pos->chunk_x, pos->chunk_z);
}

const uint8_t* residency_slot_blob(uint16_t slot, uint16_t* out_bytes) {
    *out_bytes = g_slot_bytes[slot];
    return slot_ptr(slot);
}

uint16_t residency_slot_textures(uint16_t slot, const city_slot_texture_t** out_table,
                                 uint32_t* out_vram_base) {
    *out_table = slot_tex_table(slot);
    *out_vram_base = slot_tex_vram(slot);
    return g_tex_count[slot];
}

/* Reads a blob's texture table straight from the cart into the slot's
 * compact table. A block that does not validate leaves the slot untextured:
 * its faces are then drawn in their solid colours, never with garbage. */
static void load_texture_table(uint16_t slot, const uint8_t* block, uint16_t bytes,
                               uint16_t faces) {
    uint16_t count = 0u;
    city_slot_texture_t* table = slot_tex_table(slot);
    g_tex_count[slot] = 0u;
    if (bytes > slot_tex_capacity(slot) ||
        city_texture_block_check(block, bytes, faces < slot_face_cap(slot) ? faces : slot_face_cap(slot),
                                 &count) != SAT_OK) {
        ++g_city.texture_failures;
        return;
    }
    for (uint16_t i = 0u; i < count; ++i) {
        city_texture_entry_t t = city_texture_entry(block, i);
        table[i].width8 = (uint8_t)(t.width / 8u);
        table[i].height = (uint8_t)t.height;
        table[i].offset8 = t.offset8;
        table[i].flags = t.flags;
    }
    g_pending.active = 1;
    g_pending.slot = slot;
    g_pending.faces = faces;
    g_pending.bytes = bytes;
    g_pending.src = block;
    g_tex_count[slot] = count; /* only drawn once the slot completes */
}

/* One cart -> slot copy of at most 4096 bytes with 32-bit loads (measured at
 * 0.34 ms per 4 KiB; the 16-bit path was 0.53 ms). A blob starts 32-byte
 * aligned in the cart, and reading up to three bytes past its end stays inside
 * the archive because the loader allocated 32 bytes of slack. */
static void serve(uint16_t slot) {
    const city_header_t* h = &g_archive.header;
    city_slot_t* s = &g_res.slots[slot];
    city_toc_entry_t e;
    uint8_t lod = slot_lod(slot);
    sat_result_t st = city_toc_read(g_archive.toc, city_chunk_index(s->chunk_x, s->chunk_z),
                                    lod, h->blob_base, h->total_bytes, &e);
    g_tex_count[slot] = 0u;
    if (st != SAT_OK) {
        ++g_city.decode_failures;
        city_res_complete(&g_res, slot, 0u, 1);
        return;
    }
    if ((e.flags & CITY_TOC_FLAG_EMPTY) != 0u) {
        g_slot_bytes[slot] = 0u;
        city_res_complete(&g_res, slot, 0u, 1);
        return;
    }
    if (e.bytes > slot_capacity(slot) || g_archive.bank[e.bank] == 0) {
        ++g_city.decode_failures;
        city_res_complete(&g_res, slot, 0u, 1);
        return;
    }
    {
        const uint8_t* bank = g_archive.bank[e.bank] - (uint32_t)e.bank * CITY_BANK_BYTES;
        const volatile uint32_t* src = (const volatile uint32_t*)(bank + e.offset);
        uint32_t* dst = (uint32_t*)slot_ptr(slot);
        uint32_t words = ((uint32_t)e.bytes + 3u) / 4u;
        for (uint32_t i = 0u; i < words; ++i) dst[i] = src[i];
        g_slot_bytes[slot] = e.bytes;
        if (e.texture_bytes != 0u && g_tex_vram != 0u) {
            load_texture_table(slot, bank + city_texture_block_offset(&e), e.texture_bytes,
                               e.face_count);
            if (g_pending.active && g_pending.slot == slot) return; /* completes after upload */
        }
    }
    city_res_complete(&g_res, slot, e.face_count, 0);
}

void residency_upload_textures(void) {
    uint16_t t_mark;
    sat_result_t st;
    if (!g_pending.active) return;
    g_pending.active = 0;
    t_mark = city_frt_now();
    st = sat_vdp1_vram_write(slot_tex_vram(g_pending.slot), g_pending.src, g_pending.bytes);
    g_city.t_texture += (uint16_t)(city_frt_now() - t_mark);
    if (st != SAT_OK) {
        g_tex_count[g_pending.slot] = 0u; /* draw it solid rather than wrong */
        ++g_city.texture_failures;
    } else {
        ++g_city.textures_uploaded;
        g_city.texture_bytes_uploaded += g_pending.bytes;
    }
    city_res_complete(&g_res, g_pending.slot, g_pending.faces, 0);
}

int residency_service_one(const city_pos_t* pos) {
    int slot;
    if (g_pending.active) return 0; /* one page-in per frame, textures included */
    slot = city_res_next(&g_res, pos->chunk_x, pos->chunk_z);
    if (slot < 0) return 0;
    serve((uint16_t)slot);
    /* g_city.chunks_loaded / evictions are written by the main loop together
     * with resident_slots: three counters from one instant. */
    return 1;
}

void residency_prime(const city_pos_t* pos) {
    residency_recenter(pos);
    for (;;) {
        int served = residency_service_one(pos);
        /* Before the first frame (or between frames, for a teleport) the
         * VDP1 is idle: textures go in straight away. */
        residency_upload_textures();
        if (!served) break;
    }
    g_city.chunks_loaded = g_res.loads;
    g_city.evictions = g_res.evictions;
    g_city.prime_loads = g_res.loads;
}
