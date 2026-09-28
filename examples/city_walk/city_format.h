#ifndef CITY_FORMAT_H
#define CITY_FORMAT_H

/* CITY.BIN layout and the blob decoder. Everything is big-endian, aligned to
 * 32 bytes and free of pointers, so a blob is position independent: it can be
 * copied from the cart into any residency slot and read in place.
 *
 * tools/model_pipeline/emit_bin.py writes exactly this layout, and
 * tests/test_city_chunker.py reads it back with an independent parser, so the
 * two sides check each other rather than sharing one implementation.
 *
 * FILE      header(128) | materials(4 * count) | TOC(16 * 256 * 3) |
 *           ground bitmap + palette (VDP2, optional) | texture palette | blobs...
 *
 * GROUND. Roads and pavement are flat, so they are not VDP1 faces at all: the
 * chunker rasterises every near-horizontal low face into one top-down 8 bpp
 * bitmap that the loader streams straight into VDP2 VRAM (an RBG0 rotation
 * plane), and removes those faces from the blobs. Only [blob_base, total) is
 * copied to the cart.
 * TOC ENTRY u32 offset (from blob_base) | u16 bytes | u16 vertex_count |
 *           u16 face_count | u8 bank | u8 flags(bit0 = empty chunk) |
 *           u16 texture_bytes | u16 0
 * BLOB      header(32) | vertices(6 * vc) | faces(10 * fc) | collision(12 * n)
 *
 * FACADES (version 2; flags added in version 3). Walls are blocks whose faces carry textures baked from
 * the source model. A blob's texture block follows it, 32-byte aligned and in
 * the same cart bank:
 *   u16 count | u16 0 | count x {u16 width, u16 height, u16 texel_offset/8, u16 flags}
 *   | INDEX8 texels, each 8-byte aligned, offsets from the block start
 * It is copied as is into the slot's VDP1 VRAM, so a texture's character
 * address is (slot VRAM + 8 * texel_offset/8) / 8. Byte 9 of a face is its
 * texture index + 1 (0: solid colour from byte 8). The shared 256-colour
 * texture palette goes to one CRAM bank at load.
 */

#include <stdint.h>

#include "saturn/core.h"
#include "saturn/mesh3d.h"

#include "city_grid.h"

#define CITY_MAGIC 0x43545931u /* "CTY1" */
#define CITY_BLOB_MAGIC 0x43484E4Bu /* "CHNK" */
#define CITY_VERSION 3u
#define CITY_HEADER_BYTES 128u
#define CITY_TOC_ENTRY_BYTES 16u
#define CITY_TEXTURE_TABLE_HEADER 4u
#define CITY_TEXTURE_ENTRY_BYTES 8u
#define CITY_TEXTURE_MAX_WIDTH 504u
#define CITY_TEXTURE_MAX_HEIGHT 255u
#define CITY_TEXTURE_FLAG_CUTOUT 0x0001u
#define CITY_TEXTURE_FLAG_BILLBOARD 0x0002u
#define CITY_TEXTURE_FLAG_MASK (CITY_TEXTURE_FLAG_CUTOUT | CITY_TEXTURE_FLAG_BILLBOARD)
#define CITY_TOC_ENTRIES (CITY_CHUNK_COUNT * CITY_LOD_COUNT)
#define CITY_MATERIAL_ENTRY_BYTES 4u
#define CITY_MATERIAL_MAX 240u
#define CITY_BLOB_HEADER_BYTES 32u
#define CITY_VERTEX_BYTES 6u
#define CITY_FACE_BYTES 10u
#define CITY_BOX_BYTES 12u
#define CITY_BANK_BYTES (2u * 1024u * 1024u)
#define CITY_ALIGN 32u

#define CITY_TOC_FLAG_EMPTY 0x01u
#define CITY_BLOB_FLAG_COLLISION 0x01u

/* Per-LOD blob capacities compiled into the runtime. The archive header
 * records what the chunker actually used, and city_header_check_caps refuses a
 * mismatch instead of truncating. */
#define CITY_LOD0_VERTS 512u
#define CITY_LOD0_FACES 256u
#define CITY_LOD1_VERTS 144u
#define CITY_LOD1_FACES 64u
#define CITY_LOD2_VERTS 48u
#define CITY_LOD2_FACES 24u
#define CITY_LOD2_BOXES 24u
#define CITY_SLOT_BYTES_LOD0 5664u
#define CITY_SLOT_BYTES_LOD1 1536u
#define CITY_SLOT_BYTES_LOD2 1024u
/* VDP1 VRAM per ring slot for its texture block. */
#define CITY_TEX_BYTES_LOD0 28672u
#define CITY_TEX_BYTES_LOD1 4096u
#define CITY_TEX_BYTES_LOD2 1024u

typedef struct city_header {
    uint16_t version, flags;
    uint16_t grid_x, grid_z;
    int32_t origin_x_fx, origin_z_fx;
    uint16_t chunk_units, quant_scale;
    uint8_t lod_count;
    uint16_t material_count;
    uint32_t material_offset, toc_offset, blob_base, total_bytes;
    int32_t world_min_y_fx, world_max_y_fx;
    uint32_t toc_crc32;
    uint16_t max_vertices[3], max_faces[3], max_blob_bytes[3];
    uint16_t max_collision_boxes;
    /* VDP2 ground plane; ground_bytes == 0 means the archive has none. */
    uint32_t ground_offset, ground_bytes, ground_palette_offset;
    int32_t ground_y_fx;
    uint16_t ground_width, ground_height, ground_palette_count;
    uint16_t ground_units_per_dot_x, ground_units_per_dot_z;
    /* Facade textures: palette (count 0 = the archive has none). */
    uint32_t texture_palette_offset;
    uint16_t texture_palette_count;
    uint16_t max_texture_bytes[3];
} city_header_t;

typedef struct city_toc_entry {
    uint32_t offset;
    uint16_t bytes, vertex_count, face_count;
    uint8_t bank, flags;
    uint16_t texture_bytes; /* the block right after the blob, 32-byte aligned */
} city_toc_entry_t;

/* One texture of a blob's block. */
typedef struct city_texture_entry {
    uint16_t width, height;
    uint16_t offset8; /* texel offset from the block start, in 8-byte units */
    uint16_t flags;   /* CUTOUT/BILLBOARD in archive v3 */
} city_texture_entry_t;

typedef struct city_blob_header {
    uint16_t chunk_index;
    uint8_t lod, flags;
    uint16_t vertex_count, face_count;
    int16_t bbox_min[3], bbox_max[3];
    uint16_t vertex_offset, face_offset, collision_offset, collision_count;
} city_blob_header_t;

/* Big-endian readers. Byte-wise on a little-endian host; direct aligned loads
 * on the SH-2, where this is the decode loop's hot path. */
#if defined(__sh__) || defined(__SH2__)
static inline uint16_t city_be16(const uint8_t* p) { return *(const uint16_t*)p; }
static inline uint32_t city_be32(const uint8_t* p) { return *(const uint32_t*)p; }
#else
static inline uint16_t city_be16(const uint8_t* p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
static inline uint32_t city_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
#endif
static inline int16_t city_bes16(const uint8_t* p) { return (int16_t)city_be16(p); }
static inline int32_t city_bes32(const uint8_t* p) { return (int32_t)city_be32(p); }

/* CRC-32 (IEEE, reflected), bitwise: it covers ~10 KB once at load. */
static inline uint32_t city_crc32(const uint8_t* data, uint32_t bytes) {
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0u; i < bytes; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

/* Parses and validates the 128-byte header against `bytes` (the length of the
 * buffer it came from, which may be just the header). */
static inline sat_result_t city_header_parse(const uint8_t* data, uint32_t bytes,
                                             city_header_t* out) {
    if (data == 0 || out == 0 || bytes < CITY_HEADER_BYTES) return SAT_ERR_INVALID_ARG;
    if (city_be32(data) != CITY_MAGIC) return SAT_ERR_INVALID_ARG;
    out->version = city_be16(data + 0x04);
    if (out->version != CITY_VERSION) return SAT_ERR_VERSION;
    out->flags = city_be16(data + 0x06);
    out->grid_x = city_be16(data + 0x08);
    out->grid_z = city_be16(data + 0x0A);
    out->origin_x_fx = city_bes32(data + 0x0C);
    out->origin_z_fx = city_bes32(data + 0x10);
    out->chunk_units = city_be16(data + 0x14);
    out->quant_scale = city_be16(data + 0x16);
    out->lod_count = data[0x18];
    out->material_count = city_be16(data + 0x1A);
    out->material_offset = city_be32(data + 0x1C);
    out->toc_offset = city_be32(data + 0x20);
    out->blob_base = city_be32(data + 0x24);
    out->total_bytes = city_be32(data + 0x28);
    out->world_min_y_fx = city_bes32(data + 0x2C);
    out->world_max_y_fx = city_bes32(data + 0x30);
    out->toc_crc32 = city_be32(data + 0x34);
    for (int i = 0; i < 3; ++i) {
        out->max_vertices[i] = city_be16(data + 0x38 + 2 * i);
        out->max_faces[i] = city_be16(data + 0x3E + 2 * i);
        out->max_blob_bytes[i] = city_be16(data + 0x44 + 2 * i);
    }
    out->max_collision_boxes = city_be16(data + 0x4A);
    out->ground_offset = city_be32(data + 0x4C);
    out->ground_bytes = city_be32(data + 0x50);
    out->ground_palette_offset = city_be32(data + 0x54);
    out->ground_y_fx = city_bes32(data + 0x58);
    out->ground_width = city_be16(data + 0x5C);
    out->ground_height = city_be16(data + 0x5E);
    out->ground_palette_count = city_be16(data + 0x60);
    out->ground_units_per_dot_x = city_be16(data + 0x62);
    out->ground_units_per_dot_z = city_be16(data + 0x64);
    out->texture_palette_offset = city_be32(data + 0x68);
    out->texture_palette_count = city_be16(data + 0x6C);
    for (int i = 0; i < 3; ++i) out->max_texture_bytes[i] = city_be16(data + 0x6E + 2 * i);

    if (out->grid_x != CITY_GRID_X || out->grid_z != CITY_GRID_Z ||
        out->chunk_units != CITY_CHUNK_UNITS || out->quant_scale != CITY_QUANT_SCALE ||
        out->lod_count != CITY_LOD_COUNT ||
        out->origin_x_fx != (CITY_ORIGIN_X_UNITS * 65536) ||
        out->origin_z_fx != (CITY_ORIGIN_Z_UNITS * 65536)) {
        return SAT_ERR_INVALID_ARG;
    }
    if (out->material_count == 0u || out->material_count > CITY_MATERIAL_MAX) {
        return SAT_ERR_CAPACITY;
    }
    if (out->material_offset < CITY_HEADER_BYTES ||
        out->toc_offset < out->material_offset +
            (uint32_t)out->material_count * CITY_MATERIAL_ENTRY_BYTES ||
        out->blob_base < out->toc_offset + CITY_TOC_ENTRIES * CITY_TOC_ENTRY_BYTES ||
        (out->blob_base & (CITY_ALIGN - 1u)) != 0u ||
        out->total_bytes < out->blob_base) {
        return SAT_ERR_INVALID_ARG;
    }
    if (out->ground_bytes != 0u) {
        uint32_t toc_end = out->toc_offset + CITY_TOC_ENTRIES * CITY_TOC_ENTRY_BYTES;
        uint32_t palette_bytes = (uint32_t)out->ground_palette_count * 2u;
        if ((uint32_t)out->ground_width * out->ground_height != out->ground_bytes ||
            out->ground_units_per_dot_x == 0u || out->ground_units_per_dot_z == 0u ||
            out->ground_palette_count == 0u ||
            out->ground_palette_count > 256u ||
            out->ground_offset < toc_end ||
            out->ground_offset + out->ground_bytes > out->ground_palette_offset ||
            out->ground_palette_offset + palette_bytes > out->blob_base) {
            return SAT_ERR_INVALID_ARG;
        }
    }
    if (out->texture_palette_count != 0u) {
        uint32_t toc_end = out->toc_offset + CITY_TOC_ENTRIES * CITY_TOC_ENTRY_BYTES;
        if (out->texture_palette_count > 256u || out->texture_palette_offset < toc_end ||
            out->texture_palette_offset + (uint32_t)out->texture_palette_count * 2u > out->blob_base) {
            return SAT_ERR_INVALID_ARG;
        }
    }
    return SAT_OK;
}

/* The slot sizes and caps the runtime was compiled with must cover what the
 * archive holds. Returns SAT_ERR_CAPACITY, never a silent truncation. */
static inline sat_result_t city_header_check_caps(const city_header_t* h) {
    static const uint16_t vert_cap[3] = {CITY_LOD0_VERTS, CITY_LOD1_VERTS, CITY_LOD2_VERTS};
    static const uint16_t face_cap[3] = {CITY_LOD0_FACES, CITY_LOD1_FACES, CITY_LOD2_FACES};
    static const uint16_t slot_cap[3] = {CITY_SLOT_BYTES_LOD0, CITY_SLOT_BYTES_LOD1,
                                         CITY_SLOT_BYTES_LOD2};
    static const uint16_t tex_cap[3] = {CITY_TEX_BYTES_LOD0, CITY_TEX_BYTES_LOD1,
                                        CITY_TEX_BYTES_LOD2};
    for (int i = 0; i < 3; ++i) {
        if (h->max_vertices[i] > vert_cap[i] || h->max_faces[i] > face_cap[i] ||
            h->max_blob_bytes[i] > slot_cap[i] || h->max_texture_bytes[i] > tex_cap[i]) {
            return SAT_ERR_CAPACITY;
        }
    }
    if (h->max_collision_boxes > CITY_LOD2_BOXES) return SAT_ERR_CAPACITY;
    return SAT_OK;
}

/* Where a blob's texture block starts, relative to blob_base. */
static inline uint32_t city_texture_block_offset(const city_toc_entry_t* e) {
    return e->offset + (((uint32_t)e->bytes + CITY_ALIGN - 1u) & ~(CITY_ALIGN - 1u));
}

static inline int city_toc_index(int32_t chunk_index, uint8_t lod) {
    return (int)(chunk_index * CITY_LOD_COUNT + lod);
}

/* `toc` points at the first TOC entry; `blob_base` is the file offset the
 * entries are relative to, and `total_bytes` bounds every blob. */
static inline sat_result_t city_toc_read(const uint8_t* toc, int32_t chunk_index, uint8_t lod,
                                         uint32_t blob_base, uint32_t total_bytes,
                                         city_toc_entry_t* out) {
    if (toc == 0 || out == 0 || chunk_index < 0 || chunk_index >= CITY_CHUNK_COUNT ||
        lod >= CITY_LOD_COUNT) {
        return SAT_ERR_INVALID_ARG;
    }
    const uint8_t* e = toc + (uint32_t)city_toc_index(chunk_index, lod) * CITY_TOC_ENTRY_BYTES;
    out->offset = city_be32(e);
    out->bytes = city_be16(e + 4);
    out->vertex_count = city_be16(e + 6);
    out->face_count = city_be16(e + 8);
    out->bank = e[10];
    out->flags = e[11];
    out->texture_bytes = city_be16(e + 12);
    if ((out->flags & CITY_TOC_FLAG_EMPTY) != 0u) return SAT_OK;
    if (out->bank > 1u || (out->offset & (CITY_ALIGN - 1u)) != 0u ||
        out->bytes < CITY_BLOB_HEADER_BYTES ||
        blob_base + out->offset + out->bytes > total_bytes ||
        blob_base + city_texture_block_offset(out) + out->texture_bytes > total_bytes) {
        return SAT_ERR_INVALID_ARG;
    }
    return SAT_OK;
}

/* Validates a blob's texture block (``bytes`` long) and returns its texture
 * count. Every texture must be a VDP1 INDEX8 size and lie inside the block
 * after the table, so the renderer can trust any entry it reads later. */
static inline sat_result_t city_texture_block_check(const uint8_t* block, uint32_t bytes,
                                                    uint16_t max_count, uint16_t* out_count) {
    if (block == 0 || out_count == 0 || bytes < CITY_TEXTURE_TABLE_HEADER) {
        return SAT_ERR_INVALID_ARG;
    }
    uint16_t count = city_be16(block);
    uint32_t table_end = CITY_TEXTURE_TABLE_HEADER + (uint32_t)count * CITY_TEXTURE_ENTRY_BYTES;
    if (count == 0u || count > max_count || count > 255u || table_end > bytes) {
        return SAT_ERR_INVALID_ARG;
    }
    for (uint16_t i = 0u; i < count; ++i) {
        const uint8_t* e = block + CITY_TEXTURE_TABLE_HEADER + (uint32_t)i * CITY_TEXTURE_ENTRY_BYTES;
        uint32_t w = city_be16(e), h = city_be16(e + 2), at = (uint32_t)city_be16(e + 4) * 8u;
        uint16_t flags = city_be16(e + 6);
        if ((w & 7u) != 0u || w < 8u || w > CITY_TEXTURE_MAX_WIDTH || h == 0u ||
            h > CITY_TEXTURE_MAX_HEIGHT || (flags & ~CITY_TEXTURE_FLAG_MASK) != 0u ||
            at < table_end || at + w * h > bytes) {
            return SAT_ERR_INVALID_ARG;
        }
    }
    *out_count = count;
    return SAT_OK;
}

static inline city_texture_entry_t city_texture_entry(const uint8_t* block, uint16_t index) {
    const uint8_t* e = block + CITY_TEXTURE_TABLE_HEADER + (uint32_t)index * CITY_TEXTURE_ENTRY_BYTES;
    city_texture_entry_t t;
    t.width = city_be16(e);
    t.height = city_be16(e + 2);
    t.offset8 = city_be16(e + 4);
    t.flags = city_be16(e + 6);
    return t;
}

static inline sat_result_t city_blob_header_parse(const uint8_t* blob, uint32_t bytes,
                                                  city_blob_header_t* out) {
    if (blob == 0 || out == 0 || bytes < CITY_BLOB_HEADER_BYTES) return SAT_ERR_INVALID_ARG;
    if (city_be32(blob) != CITY_BLOB_MAGIC) return SAT_ERR_INVALID_ARG;
    out->chunk_index = city_be16(blob + 0x04);
    out->lod = blob[0x06];
    out->flags = blob[0x07];
    out->vertex_count = city_be16(blob + 0x08);
    out->face_count = city_be16(blob + 0x0A);
    for (int i = 0; i < 3; ++i) {
        out->bbox_min[i] = city_bes16(blob + 0x0C + 2 * i);
        out->bbox_max[i] = city_bes16(blob + 0x12 + 2 * i);
    }
    out->vertex_offset = city_be16(blob + 0x18);
    out->face_offset = city_be16(blob + 0x1A);
    out->collision_offset = city_be16(blob + 0x1C);
    out->collision_count = city_be16(blob + 0x1E);

    /* Every section must lie inside the bytes we were given. */
    uint32_t vertex_end = (uint32_t)out->vertex_offset + (uint32_t)out->vertex_count * CITY_VERTEX_BYTES;
    uint32_t face_end = (uint32_t)out->face_offset + (uint32_t)out->face_count * CITY_FACE_BYTES;
    if (out->vertex_offset != CITY_BLOB_HEADER_BYTES || vertex_end > out->face_offset ||
        face_end > bytes || out->lod >= CITY_LOD_COUNT) {
        return SAT_ERR_INVALID_ARG;
    }
    if (out->collision_count != 0u) {
        uint32_t box_end = (uint32_t)out->collision_offset +
                           (uint32_t)out->collision_count * CITY_BOX_BYTES;
        if (out->collision_offset < face_end || box_end > bytes) return SAT_ERR_INVALID_ARG;
    }
    return SAT_OK;
}

/* Decodes a blob into a caller-bound scratch mesh.
 *
 *   x = origin_rel_x + (i16 << 10)      z = origin_rel_z + (i16 << 10)
 *   y = base_y_fx    + (i16 << 10)
 *
 * origin_rel_* is city_origin_rel_fx(chunk, player_chunk), so the result is
 * already in the player-chunk frame and the mesh is submitted with
 * world == NULL (no per-vertex matrix). Validates everything it indexes with:
 * counts against the mesh caps (SAT_ERR_CAPACITY, nothing written past them),
 * vertex indices against vertex_count, materials against material_count, and
 * the bbox against the overhang the format promises, which is what keeps the
 * decoded magnitude under the fixed-point limit. Face byte 9 is reserved.
 * material_map, when non-NULL, has material_count entries and translates the
 * archive's material index into the caller's own (the solid pool deduplicates
 * equal colours, so its handles need not match the archive's numbering). */
/* As city_blob_decode, and also checks each face's texture byte against
 * ``texture_count`` (the blob's validated texture block) and copies it to
 * ``out_face_textures`` (index + 1, 0 = solid; may be NULL). */
static inline sat_result_t city_blob_decode_ex(const uint8_t* blob, uint32_t bytes,
                                               sat_fx16_t origin_rel_x, sat_fx16_t origin_rel_z,
                                               sat_fx16_t base_y_fx, uint16_t material_count,
                                               const uint16_t* material_map,
                                               sat_mesh_t* out_mesh,
                                               uint16_t* out_face_materials,
                                               uint16_t face_material_cap,
                                               uint16_t texture_count,
                                               uint8_t* out_face_textures) {
    city_blob_header_t h;
    sat_result_t status = city_blob_header_parse(blob, bytes, &h);
    if (status != SAT_OK) return status;
    if (out_mesh == 0 || out_mesh->vertices == 0 || out_mesh->indices == 0 ||
        out_face_materials == 0) {
        return SAT_ERR_INVALID_ARG;
    }
    if (h.vertex_count > out_mesh->vertex_cap || h.face_count > out_mesh->face_cap ||
        h.face_count > face_material_cap) {
        return SAT_ERR_CAPACITY;
    }
    const int16_t lo = (int16_t)(-CITY_OVERHANG_UNITS * CITY_QUANT_SCALE);
    const int16_t hi = (int16_t)((CITY_CHUNK_UNITS + CITY_OVERHANG_UNITS) * CITY_QUANT_SCALE);
    if (h.bbox_min[0] < lo || h.bbox_min[2] < lo || h.bbox_max[0] > hi || h.bbox_max[2] > hi) {
        return SAT_ERR_INVALID_ARG;
    }

    /* Validate the faces before touching the mesh, so a rejected blob leaves
     * it exactly as it was. */
    const uint8_t* faces = blob + h.face_offset;
    for (uint32_t f = 0u; f < h.face_count; ++f) {
        const uint8_t* p = faces + f * CITY_FACE_BYTES;
        if (city_be16(p) >= h.vertex_count || city_be16(p + 2) >= h.vertex_count ||
            city_be16(p + 4) >= h.vertex_count || city_be16(p + 6) >= h.vertex_count ||
            p[8] >= material_count || p[9] > texture_count) {
            return SAT_ERR_INVALID_ARG;
        }
    }

    const uint8_t* v = blob + h.vertex_offset;
    for (uint32_t i = 0u; i < h.vertex_count; ++i, v += CITY_VERTEX_BYTES) {
        out_mesh->vertices[i].x = origin_rel_x + ((int32_t)city_bes16(v) << CITY_QUANT_SHIFT);
        out_mesh->vertices[i].y = base_y_fx + ((int32_t)city_bes16(v + 2) << CITY_QUANT_SHIFT);
        out_mesh->vertices[i].z = origin_rel_z + ((int32_t)city_bes16(v + 4) << CITY_QUANT_SHIFT);
    }
    for (uint32_t f = 0u; f < h.face_count; ++f) {
        const uint8_t* p = faces + f * CITY_FACE_BYTES;
        uint16_t* idx = out_mesh->indices + f * 4u;
        idx[0] = city_be16(p);
        idx[1] = city_be16(p + 2);
        idx[2] = city_be16(p + 4);
        idx[3] = city_be16(p + 6);
        out_face_materials[f] = material_map != 0 ? material_map[p[8]] : (uint16_t)p[8];
        if (out_face_textures != 0) out_face_textures[f] = p[9];
    }
    out_mesh->vertex_count = h.vertex_count;
    out_mesh->face_count = h.face_count;
    return SAT_OK;
}

/* The same without texture output: texture bytes are accepted unchecked. */
static inline sat_result_t city_blob_decode(const uint8_t* blob, uint32_t bytes,
                                            sat_fx16_t origin_rel_x, sat_fx16_t origin_rel_z,
                                            sat_fx16_t base_y_fx, uint16_t material_count,
                                            const uint16_t* material_map,
                                            sat_mesh_t* out_mesh,
                                            uint16_t* out_face_materials,
                                            uint16_t face_material_cap) {
    return city_blob_decode_ex(blob, bytes, origin_rel_x, origin_rel_z, base_y_fx,
                               material_count, material_map, out_mesh, out_face_materials,
                               face_material_cap, 255u, 0);
}

/* Walking collision: pushes a circle (x, z, radius) out of the blob's boxes.
 *
 * The boxes are the LOD2 blobs' collision section, in the same player-chunk
 * frame as the geometry (origin_rel_* is city_origin_rel_fx). Each box is
 * an axis-aligned footprint that starts at street level, so height does not
 * enter: anything that reaches above the walker's step height was already
 * filtered in by the chunker (curbs are not boxes). The push goes along the
 * axis of least penetration, twice, so a walker driven into an inside corner
 * ends up outside both walls instead of stuck in the second one.
 * Returns how many boxes moved the walker. */
static inline int city_blob_push_out(const uint8_t* blob, uint32_t bytes,
                                     sat_fx16_t origin_rel_x, sat_fx16_t origin_rel_z,
                                     sat_fx16_t radius, sat_fx16_t* x, sat_fx16_t* z) {
    city_blob_header_t h;
    int pushed = 0;
    if (city_blob_header_parse(blob, bytes, &h) != SAT_OK || h.collision_count == 0u) return 0;
    for (int pass = 0; pass < 2; ++pass) {
        for (uint16_t i = 0u; i < h.collision_count; ++i) {
            const uint8_t* box = blob + h.collision_offset + (uint32_t)i * CITY_BOX_BYTES;
            sat_fx16_t bx = origin_rel_x + ((int32_t)city_bes16(box) << CITY_QUANT_SHIFT);
            sat_fx16_t bz = origin_rel_z + ((int32_t)city_bes16(box + 4) << CITY_QUANT_SHIFT);
            sat_fx16_t hx = ((int32_t)city_bes16(box + 6) << CITY_QUANT_SHIFT) + radius;
            sat_fx16_t hz = ((int32_t)city_bes16(box + 10) << CITY_QUANT_SHIFT) + radius;
            sat_fx16_t dx = *x - bx;
            sat_fx16_t dz = *z - bz;
            sat_fx16_t pen_x = hx - (dx < 0 ? -dx : dx);
            sat_fx16_t pen_z = hz - (dz < 0 ? -dz : dz);
            if (pen_x <= 0 || pen_z <= 0) continue;
            if (pen_x < pen_z) *x += dx >= 0 ? pen_x : -pen_x;
            else *z += dz >= 0 ? pen_z : -pen_z;
            ++pushed;
        }
    }
    return pushed;
}

#endif /* CITY_FORMAT_H */
