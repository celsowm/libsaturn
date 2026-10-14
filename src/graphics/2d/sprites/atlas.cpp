#include "saturn/sprite_atlas.h"

#include "saturn/vdp1.h"

/* SATL atlas loader and VRAM residency (see saturn/sprite_atlas.h and
 * tools/shmup_assets.py for the file layout).
 *
 *   0   header, 64 bytes: "SATL", u16 version, u16 flags, u16 frames, u16 luts,
 *       u16 groups, u16 palette entries, u32 frame table, lut table, palette,
 *       group table offsets, u32 total bytes, u32 crc32 of bytes 64.., u16
 *       orientation
 *   64  frame records, 32 bytes: u16 w, h, s16 pivot x, y, u16 format, lut,
 *       u32 data offset, data bytes, u16 group, radius, category, 0, u32 hash
 *   ... lut tables (32 bytes each), palette (2 bytes per entry), group table
 *       (u32 data offset, data bytes, u16 first frame, frame count, u32 0)
 *   ... group pixel data, each group starting on a 2048-byte boundary */

namespace {

constexpr uint32_t kHeaderBytes = 64u;
constexpr uint32_t kFrameBytes = 32u;
constexpr uint32_t kLutBytes = 32u;
constexpr uint32_t kGroupBytes = 16u;
constexpr uint32_t kSector = 2048u;
constexpr uint16_t kVersion = 1u;

inline uint16_t rd16(const uint8_t* p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}
inline uint32_t rd32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | p[3];
}

uint32_t crc32_update(uint32_t crc, const uint8_t* data, uint32_t bytes) {
    static const uint32_t kNibble[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u,
        0x4DB26158u, 0x5005713Cu, 0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
        0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu};
    crc = ~crc;
    for (uint32_t i = 0; i < bytes; ++i) {
        crc ^= data[i];
        crc = (crc >> 4) ^ kNibble[crc & 15u];
        crc = (crc >> 4) ^ kNibble[crc & 15u];
    }
    return ~crc;
}

inline uint32_t align_up(uint32_t v, uint32_t a) { return (v + a - 1u) / a * a; }

/* Table sizes from the header alone. false when a count is out of range. */
struct Layout {
    uint32_t frame_off, lut_off, palette_off, group_off, table_bytes, total;
    uint16_t frames, luts, groups, palette;
};

bool read_layout(const uint8_t* h, Layout* out) {
    if (h[0] != 'S' || h[1] != 'A' || h[2] != 'T' || h[3] != 'L') return false;
    if (rd16(h + 4) != kVersion) return false;
    out->frames = rd16(h + 8);
    out->luts = rd16(h + 10);
    out->groups = rd16(h + 12);
    out->palette = rd16(h + 14);
    out->frame_off = rd32(h + 16);
    out->lut_off = rd32(h + 20);
    out->palette_off = rd32(h + 24);
    out->group_off = rd32(h + 28);
    out->total = rd32(h + 32);
    if (out->frames == 0u || out->frames > SAT_SPRITE_ATLAS_MAX_FRAMES) return false;
    if (out->luts > SAT_SPRITE_ATLAS_MAX_LUTS) return false;
    if (out->groups == 0u || out->groups > SAT_SPRITE_ATLAS_MAX_GROUPS) return false;
    if (out->palette == 0u || out->palette > 256u) return false;
    if (out->frame_off != kHeaderBytes) return false;
    if (out->lut_off != out->frame_off + kFrameBytes * out->frames) return false;
    if (out->palette_off != out->lut_off + kLutBytes * out->luts) return false;
    if (out->group_off != out->palette_off + 2u * out->palette) return false;
    out->table_bytes = align_up(out->group_off + kGroupBytes * out->groups, kSector);
    return out->total >= out->table_bytes;
}

inline const uint8_t* frame_rec(const sat_sprite_atlas_t* a, uint32_t i) {
    return a->tables + kHeaderBytes + kFrameBytes * i;
}
inline const uint8_t* group_rec(const sat_sprite_atlas_t* a, uint32_t g) {
    const uint32_t group_off = kHeaderBytes + kFrameBytes * a->frame_count + kLutBytes * a->lut_count +
                               2u * a->palette_entries;
    return a->tables + group_off + kGroupBytes * g;
}

bool ready(const sat_sprite_atlas_t* a) { return a != nullptr && a->tables != nullptr; }

}  // namespace

extern "C" sat_result_t sat_sprite_atlas_peek(const void* header, uint32_t bytes,
                                              uint32_t* out_table_bytes, uint32_t* out_total_bytes) {
    if (header == nullptr || bytes < kHeaderBytes) return SAT_ERR_INVALID_ARG;
    Layout l{};
    if (!read_layout(static_cast<const uint8_t*>(header), &l)) return SAT_ERR_INVALID_ARG;
    if (out_table_bytes != nullptr) *out_table_bytes = l.table_bytes;
    if (out_total_bytes != nullptr) *out_total_bytes = l.total;
    return SAT_OK;
}

extern "C" sat_result_t sat_sprite_atlas_open(sat_sprite_atlas_t* atlas, const void* blob, uint32_t bytes) {
    if (atlas == nullptr) return SAT_ERR_INVALID_ARG;
    *atlas = sat_sprite_atlas_t{};
    atlas->loading_slot = -1;
    for (uint32_t s = 0; s < SAT_SPRITE_ATLAS_MAX_SLOTS; ++s) atlas->slot[s].group = -1;
    if (blob == nullptr || bytes < kHeaderBytes) return SAT_ERR_INVALID_ARG;
    const uint8_t* b = static_cast<const uint8_t*>(blob);
    Layout l{};
    if (!read_layout(b, &l) || bytes < l.table_bytes) return SAT_ERR_INVALID_ARG;
    if (bytes > l.total) return SAT_ERR_INVALID_ARG;

    const uint32_t crc = rd32(b + 36);
    const uint16_t orientation = rd16(b + 40);
    if (orientation > SAT_SPRITE_ATLAS_ORIENT_CCW) return SAT_ERR_INVALID_ARG;
    if (bytes == l.total && crc32_update(0u, b + kHeaderBytes, l.total - kHeaderBytes) != crc) {
        return SAT_ERR_VERIFY_FAILED;
    }

    /* Groups: ordered, sector aligned, inside the file, owning consecutive frames. */
    const uint8_t* gt = b + l.group_off;
    uint32_t next_data = l.table_bytes;
    uint32_t next_frame = 0u;
    for (uint32_t g = 0; g < l.groups; ++g) {
        const uint8_t* r = gt + kGroupBytes * g;
        const uint32_t off = rd32(r), size = rd32(r + 4);
        const uint32_t first = rd16(r + 8), count = rd16(r + 10);
        if ((off % kSector) != 0u || off < next_data || size == 0u || (size % 8u) != 0u) return SAT_ERR_INVALID_ARG;
        if (off > l.total || size > l.total - off) return SAT_ERR_INVALID_ARG;
        if (first != next_frame || count == 0u || count > l.frames - first) return SAT_ERR_INVALID_ARG;
        next_data = off + size;
        next_frame = first + count;
    }
    if (next_frame != l.frames) return SAT_ERR_INVALID_ARG;

    /* Frames: each wholly inside the data of the group that owns it. */
    for (uint32_t i = 0; i < l.frames; ++i) {
        const uint8_t* r = b + kHeaderBytes + kFrameBytes * i;
        const uint32_t w = rd16(r), h = rd16(r + 2);
        const uint32_t fmt = rd16(r + 8), lut = rd16(r + 10);
        const uint32_t off = rd32(r + 12), size = rd32(r + 16);
        const uint32_t group = rd16(r + 20);
        if (w == 0u || (w % 8u) != 0u || h == 0u || h > 255u) return SAT_ERR_INVALID_ARG;
        if (fmt > SAT_VDP1_TEXTURE_LUT4) return SAT_ERR_INVALID_ARG;
        if (fmt == SAT_VDP1_TEXTURE_LUT4 && lut >= l.luts) return SAT_ERR_INVALID_ARG;
        if (size != (fmt == SAT_VDP1_TEXTURE_LUT4 ? w * h / 2u : w * h)) return SAT_ERR_INVALID_ARG;
        if (group >= l.groups) return SAT_ERR_INVALID_ARG;
        const uint8_t* gr = gt + kGroupBytes * group;
        const uint32_t g_off = rd32(gr), g_size = rd32(gr + 4);
        const uint32_t g_first = rd16(gr + 8), g_count = rd16(gr + 10);
        if (i < g_first || i >= g_first + g_count) return SAT_ERR_INVALID_ARG;
        if ((off % 8u) != 0u || off < g_off || off - g_off > g_size || size > g_size - (off - g_off)) {
            return SAT_ERR_INVALID_ARG;
        }
    }

    atlas->tables = b;
    atlas->table_bytes = l.table_bytes;
    atlas->total_bytes = l.total;
    atlas->crc32 = crc;
    atlas->frame_count = l.frames;
    atlas->lut_count = l.luts;
    atlas->group_count = l.groups;
    atlas->palette_entries = l.palette;
    atlas->orientation = orientation;
    atlas->crc_checked = bytes == l.total ? 1u : 0u;
    return SAT_OK;
}

extern "C" uint32_t sat_sprite_atlas_frame_count(const sat_sprite_atlas_t* atlas) {
    return ready(atlas) ? atlas->frame_count : 0u;
}
extern "C" uint32_t sat_sprite_atlas_group_count(const sat_sprite_atlas_t* atlas) {
    return ready(atlas) ? atlas->group_count : 0u;
}
extern "C" uint32_t sat_sprite_atlas_lut_count(const sat_sprite_atlas_t* atlas) {
    return ready(atlas) ? atlas->lut_count : 0u;
}

extern "C" sat_result_t sat_sprite_atlas_frame_info(const sat_sprite_atlas_t* atlas, uint32_t frame,
                                                    sat_sprite_atlas_frame_info_t* out) {
    if (!ready(atlas) || out == nullptr || frame >= atlas->frame_count) return SAT_ERR_INVALID_ARG;
    const uint8_t* r = frame_rec(atlas, frame);
    out->width = rd16(r);
    out->height = rd16(r + 2);
    out->pivot_x = static_cast<int16_t>(rd16(r + 4));
    out->pivot_y = static_cast<int16_t>(rd16(r + 6));
    out->format = rd16(r + 8);
    out->lut = rd16(r + 10);
    out->data_offset = rd32(r + 12);
    out->data_bytes = rd32(r + 16);
    out->group = rd16(r + 20);
    out->radius = rd16(r + 22);
    out->category = rd16(r + 24);
    return SAT_OK;
}

extern "C" sat_result_t sat_sprite_atlas_group_info(const sat_sprite_atlas_t* atlas, uint32_t group,
                                                    sat_sprite_atlas_group_info_t* out) {
    if (!ready(atlas) || out == nullptr || group >= atlas->group_count) return SAT_ERR_INVALID_ARG;
    const uint8_t* r = group_rec(atlas, group);
    out->data_offset = rd32(r);
    out->data_bytes = rd32(r + 4);
    out->first_frame = rd16(r + 8);
    out->frame_count = rd16(r + 10);
    return SAT_OK;
}

extern "C" uint32_t sat_sprite_atlas_max_group_bytes(const sat_sprite_atlas_t* atlas) {
    if (!ready(atlas)) return 0u;
    uint32_t best = 0u;
    for (uint32_t g = 0; g < atlas->group_count; ++g) {
        const uint32_t size = rd32(group_rec(atlas, g) + 4);
        if (size > best) best = size;
    }
    return best;
}

extern "C" sat_result_t sat_sprite_atlas_plan_add(const sat_sprite_atlas_t* atlas, sat_resource_plan_t* plan,
                                                  uint32_t slots) {
    if (!ready(atlas) || plan == nullptr || slots == 0u || slots > SAT_SPRITE_ATLAS_MAX_SLOTS) {
        return SAT_ERR_INVALID_ARG;
    }
    const uint32_t group_bytes = sat_sprite_atlas_max_group_bytes(atlas);
    for (uint32_t s = 0; s < slots; ++s) {
        SAT_TRY(sat_resource_plan_add(plan, SAT_RESOURCE_VDP1_VRAM, group_bytes, 8u, 1u));
    }
    if (atlas->lut_count != 0u) {
        SAT_TRY(sat_resource_plan_add(plan, SAT_RESOURCE_VDP1_VRAM, atlas->lut_count * kLutBytes, 32u, 1u));
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_sprite_atlas_palette(const sat_sprite_atlas_t* atlas, uint16_t* out,
                                                 uint32_t capacity, uint32_t* out_entries) {
    if (!ready(atlas) || out == nullptr || capacity < atlas->palette_entries) return SAT_ERR_INVALID_ARG;
    const uint8_t* p = atlas->tables + kHeaderBytes + kFrameBytes * atlas->frame_count +
                       kLutBytes * atlas->lut_count;
    for (uint32_t i = 0; i < atlas->palette_entries; ++i) out[i] = rd16(p + 2u * i);
    if (out_entries != nullptr) *out_entries = atlas->palette_entries;
    return SAT_OK;
}

extern "C" sat_result_t sat_sprite_atlas_upload_palette(sat_sprite_atlas_t* atlas, uint16_t bank) {
    if (!ready(atlas)) return SAT_ERR_INVALID_ARG;
    uint16_t palette[256] = {};
    uint32_t entries = 0u;
    SAT_TRY(sat_sprite_atlas_palette(atlas, palette, 256u, &entries));
    SAT_TRY(sat_palette_upload_indexed8(palette, bank));
    atlas->palette_bank = bank;
    atlas->palette_bank_set = 1u;
    return SAT_OK;
}

extern "C" sat_result_t sat_sprite_atlas_reserve_slot(sat_sprite_atlas_t* atlas, uint32_t slot, uint32_t bytes) {
    if (!ready(atlas) || slot >= SAT_SPRITE_ATLAS_MAX_SLOTS) return SAT_ERR_INVALID_ARG;
    sat_sprite_atlas_slot_t& s = atlas->slot[slot];
    if (s.capacity != 0u) return SAT_ERR_ALREADY_EXISTS;
    if (bytes == 0u) bytes = sat_sprite_atlas_max_group_bytes(atlas);
    uint32_t offset = 0u;
    SAT_TRY(sat_vdp1_vram_reserve(bytes, &offset));
    s.vram_offset = offset;
    s.capacity = bytes;
    s.written = 0u;
    s.group = -1;
    s.resident = 0u;
    return SAT_OK;
}

extern "C" sat_result_t sat_sprite_atlas_upload_luts(sat_sprite_atlas_t* atlas) {
    if (!ready(atlas)) return SAT_ERR_INVALID_ARG;
    if (atlas->luts_uploaded != 0u) return SAT_OK;
    const uint8_t* p = atlas->tables + kHeaderBytes + kFrameBytes * atlas->frame_count;
    for (uint32_t i = 0; i < atlas->lut_count; ++i) {
        uint16_t entries[16];
        for (uint32_t e = 0; e < 16u; ++e) entries[e] = rd16(p + kLutBytes * i + 2u * e);
        SAT_TRY(sat_vdp1_upload_lut(entries, &atlas->lut_colr[i]));
        ++atlas->stats.lut_uploads;
    }
    atlas->luts_uploaded = 1u;
    return SAT_OK;
}

extern "C" sat_result_t sat_sprite_atlas_upload_begin(sat_sprite_atlas_t* atlas, uint32_t slot, uint32_t group,
                                                      uint8_t* out_skipped) {
    if (!ready(atlas) || slot >= SAT_SPRITE_ATLAS_MAX_SLOTS || group >= atlas->group_count) {
        return SAT_ERR_INVALID_ARG;
    }
    if (atlas->loading_slot >= 0) return SAT_ERR_BUSY;
    sat_sprite_atlas_slot_t& s = atlas->slot[slot];
    if (s.capacity == 0u) return SAT_ERR_NOT_INITIALIZED;
    const uint32_t size = rd32(group_rec(atlas, group) + 4);
    if (size > s.capacity) return SAT_ERR_CAPACITY;
    if (out_skipped != nullptr) *out_skipped = 0u;
    if (s.resident != 0u && s.group == static_cast<int16_t>(group)) {
        ++atlas->stats.loads_skipped;
        if (out_skipped != nullptr) *out_skipped = 1u;
        return SAT_OK;
    }
    /* A group resident in another slot would be duplicated, never wanted: the
     * caller's frames come from one slot per group. */
    s.group = static_cast<int16_t>(group);
    s.resident = 0u;
    s.written = 0u;
    atlas->loading_slot = static_cast<int8_t>(slot);
    return SAT_OK;
}

extern "C" sat_result_t sat_sprite_atlas_upload_chunk(sat_sprite_atlas_t* atlas, const void* data, uint32_t bytes) {
    if (!ready(atlas) || atlas->loading_slot < 0) return SAT_ERR_INVALID_ARG;
    if (bytes == 0u) return SAT_OK;
    if (data == nullptr) return SAT_ERR_INVALID_ARG;
    sat_sprite_atlas_slot_t& s = atlas->slot[atlas->loading_slot];
    const uint32_t size = rd32(group_rec(atlas, static_cast<uint32_t>(s.group)) + 4);
    if (bytes > size - s.written) return SAT_ERR_INVALID_ARG;
    SAT_TRY(sat_vdp1_vram_write(s.vram_offset + s.written, data, bytes));
    s.written += bytes;
    atlas->stats.bytes_uploaded += bytes;
    return SAT_OK;
}

extern "C" sat_result_t sat_sprite_atlas_upload_end(sat_sprite_atlas_t* atlas) {
    if (!ready(atlas) || atlas->loading_slot < 0) return SAT_ERR_INVALID_ARG;
    sat_sprite_atlas_slot_t& s = atlas->slot[atlas->loading_slot];
    const uint32_t size = rd32(group_rec(atlas, static_cast<uint32_t>(s.group)) + 4);
    if (s.written != size) return SAT_ERR_INVALID_ARG;
    s.resident = 1u;
    ++atlas->stats.group_loads;
    atlas->loading_slot = -1;
    return SAT_OK;
}

extern "C" uint32_t sat_sprite_atlas_upload_remaining(const sat_sprite_atlas_t* atlas) {
    if (!ready(atlas) || atlas->loading_slot < 0) return 0u;
    const sat_sprite_atlas_slot_t& s = atlas->slot[atlas->loading_slot];
    return rd32(group_rec(atlas, static_cast<uint32_t>(s.group)) + 4) - s.written;
}

extern "C" sat_result_t sat_sprite_atlas_load_group(sat_sprite_atlas_t* atlas, uint32_t slot, uint32_t group,
                                                    const void* data, uint32_t bytes) {
    if (!ready(atlas) || group >= atlas->group_count) return SAT_ERR_INVALID_ARG;
    if (bytes != rd32(group_rec(atlas, group) + 4)) return SAT_ERR_INVALID_ARG;
    uint8_t skipped = 0u;
    SAT_TRY(sat_sprite_atlas_upload_begin(atlas, slot, group, &skipped));
    if (skipped != 0u) return SAT_OK;
    sat_result_t st = sat_sprite_atlas_upload_chunk(atlas, data, bytes);
    if (st == SAT_OK) st = sat_sprite_atlas_upload_end(atlas);
    if (st != SAT_OK) {
        /* Leave the slot empty rather than half-filled. */
        atlas->slot[slot].resident = 0u;
        atlas->slot[slot].group = -1;
        atlas->loading_slot = -1;
    }
    return st;
}

extern "C" sat_result_t sat_sprite_atlas_evict(sat_sprite_atlas_t* atlas, uint32_t slot) {
    if (!ready(atlas) || slot >= SAT_SPRITE_ATLAS_MAX_SLOTS) return SAT_ERR_INVALID_ARG;
    if (atlas->loading_slot == static_cast<int8_t>(slot)) atlas->loading_slot = -1;
    atlas->slot[slot].group = -1;
    atlas->slot[slot].resident = 0u;
    atlas->slot[slot].written = 0u;
    return SAT_OK;
}

extern "C" sat_result_t sat_sprite_atlas_group_slot(const sat_sprite_atlas_t* atlas, uint32_t group,
                                                    uint32_t* out_slot) {
    if (!ready(atlas) || group >= atlas->group_count) return SAT_ERR_INVALID_ARG;
    for (uint32_t s = 0; s < SAT_SPRITE_ATLAS_MAX_SLOTS; ++s) {
        if (atlas->slot[s].resident != 0u && atlas->slot[s].group == static_cast<int16_t>(group)) {
            if (out_slot != nullptr) *out_slot = s;
            return SAT_OK;
        }
    }
    return SAT_ERR_NOT_FOUND;
}

extern "C" sat_result_t sat_sprite_atlas_build_frames(const sat_sprite_atlas_t* atlas, uint32_t group,
                                                      uint16_t flags, sat_sprite_frame_t* frames,
                                                      uint32_t frame_capacity) {
    if (!ready(atlas) || frames == nullptr || group >= atlas->group_count) return SAT_ERR_INVALID_ARG;
    if (frame_capacity < atlas->frame_count) return SAT_ERR_CAPACITY;
    uint32_t slot = 0u;
    SAT_TRY(sat_sprite_atlas_group_slot(atlas, group, &slot));
    sat_sprite_atlas_group_info_t gi{};
    SAT_TRY(sat_sprite_atlas_group_info(atlas, group, &gi));

    /* Preflight: what the frames need must be ready before any is written, so
     * a failure leaves the array as it was. */
    bool needs_bank = false, needs_lut = false;
    for (uint32_t i = 0; i < gi.frame_count; ++i) {
        if (rd16(frame_rec(atlas, gi.first_frame + i) + 8) == SAT_VDP1_TEXTURE_LUT4) needs_lut = true;
        else needs_bank = true;
    }
    if (needs_bank && atlas->palette_bank_set == 0u) return SAT_ERR_NOT_INITIALIZED;
    if (needs_lut && atlas->luts_uploaded == 0u) return SAT_ERR_NOT_INITIALIZED;

    const uint32_t base = atlas->slot[slot].vram_offset;
    for (uint32_t i = 0; i < gi.frame_count; ++i) {
        const uint32_t index = gi.first_frame + i;
        sat_sprite_atlas_frame_info_t f{};
        SAT_TRY(sat_sprite_atlas_frame_info(atlas, index, &f));
        const uint32_t vram = base + (f.data_offset - gi.data_offset);
        const uint16_t palette = f.format == SAT_VDP1_TEXTURE_LUT4 ? atlas->lut_colr[f.lut]
                                                                   : atlas->palette_bank;
        SAT_TRY(sat_sprite_frame_init_raw(&frames[index], static_cast<uint16_t>(vram / 8u), f.width, f.height,
                                          f.format, palette, flags, f.pivot_x, f.pivot_y));
    }
    return SAT_OK;
}
