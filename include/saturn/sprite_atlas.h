#ifndef SATURN_SPRITE_ATLAS_H
#define SATURN_SPRITE_ATLAS_H

#include <stdint.h>
#include "saturn/core.h"
#include "saturn/resource_plan.h"
#include "saturn/sprite_batch.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A SATL sprite atlas: the file tools/shmup_assets.py pack writes. One file
 * holds every frame of a game's sprites, trimmed to their opaque pixels, as
 * INDEX8 or LUT4 character patterns, with a shared palette, shared LUT4
 * tables, and residency GROUPS.
 *
 * A group is a self-contained, sector-aligned run of pixel data. Loading a
 * group writes its bytes into one VRAM slot in a single pass (no per-frame
 * upload, no per-frame allocation); identical pixel data is stored once per
 * group in the file. Two slots let a stage's sprites stay on screen while the
 * next stage's are prefetched into the other slot a chunk per frame, then the
 * frames are rebuilt from the new slot at the transition: the swap itself
 * writes no VRAM.
 *
 * The library never allocates. The tables (the file up to the first group's
 * pixel data) must stay readable for as long as the atlas is used; group pixel
 * data only needs to be readable while it is being uploaded, so it can come
 * from RAM, a RAM cartridge or a disc-read buffer. All multi-byte fields in
 * the file are big-endian and are read bytewise. */

#define SAT_SPRITE_ATLAS_MAX_LUTS 32u
#define SAT_SPRITE_ATLAS_MAX_SLOTS 4u
#define SAT_SPRITE_ATLAS_MAX_GROUPS 16u
#define SAT_SPRITE_ATLAS_MAX_FRAMES 1024u

#define SAT_SPRITE_ATLAS_ORIENT_NONE 0u
#define SAT_SPRITE_ATLAS_ORIENT_CW 1u    /* sprites were rotated 90 degrees clockwise */
#define SAT_SPRITE_ATLAS_ORIENT_CCW 2u

typedef struct sat_sprite_atlas_slot {
    uint32_t vram_offset;   /* VDP1 VRAM byte offset of the slot */
    uint32_t capacity;      /* bytes reserved */
    uint32_t written;       /* bytes of the loading group received so far */
    int16_t group;          /* group resident in the slot or being loaded, -1 none */
    uint8_t resident;       /* the whole group is in VRAM */
    uint8_t reserved;
} sat_sprite_atlas_slot_t;

typedef struct sat_sprite_atlas_stats {
    uint32_t group_loads;      /* groups written to VRAM */
    uint32_t loads_skipped;    /* load requests for a group already resident */
    uint32_t bytes_uploaded;   /* pixel bytes written to VRAM */
    uint32_t lut_uploads;      /* LUT4 tables written (each at most once) */
} sat_sprite_atlas_stats_t;

typedef struct sat_sprite_atlas {
    const uint8_t* tables;
    uint32_t table_bytes;
    uint32_t total_bytes;
    uint32_t crc32;
    uint16_t frame_count;
    uint16_t lut_count;
    uint16_t group_count;
    uint16_t palette_entries;
    uint16_t orientation;
    uint8_t crc_checked;
    uint8_t luts_uploaded;
    uint16_t lut_colr[SAT_SPRITE_ATLAS_MAX_LUTS];
    uint16_t palette_bank;
    uint8_t palette_bank_set;
    uint8_t reserved;
    sat_sprite_atlas_slot_t slot[SAT_SPRITE_ATLAS_MAX_SLOTS];
    int8_t loading_slot;    /* slot with an upload in progress, -1 none */
    sat_sprite_atlas_stats_t stats;
} sat_sprite_atlas_t;

typedef struct sat_sprite_atlas_frame_info {
    uint16_t width;
    uint16_t height;
    int16_t pivot_x;        /* of the trimmed frame: where the sprite's anchor is */
    int16_t pivot_y;
    uint16_t format;        /* SAT_VDP1_TEXTURE_INDEXED8 or _LUT4 */
    uint16_t lut;           /* index into the atlas LUT tables (LUT4 only) */
    uint16_t group;
    uint16_t radius;        /* suggested collision radius in pixels */
    uint16_t category;      /* the packer's category id */
    uint32_t data_offset;   /* in the file */
    uint32_t data_bytes;
} sat_sprite_atlas_frame_info_t;

typedef struct sat_sprite_atlas_group_info {
    uint32_t data_offset;   /* in the file, sector aligned */
    uint32_t data_bytes;    /* VRAM a slot needs for the group */
    uint16_t first_frame;
    uint16_t frame_count;
} sat_sprite_atlas_group_info_t;

/* How many bytes of the file open needs, from its first 64: the tables end
 * where the first group's pixel data starts. *out_total_bytes is the whole
 * file. Read that much (or the whole file) and pass it to open. */
sat_result_t sat_sprite_atlas_peek(const void* header, uint32_t bytes,
                                   uint32_t* out_table_bytes, uint32_t* out_total_bytes);

/* Validates the blob and prepares the atlas. Every offset, size, format,
 * group range and frame/group relation is checked before anything is used; a
 * malformed or truncated file is refused with SAT_ERR_INVALID_ARG and the
 * atlas is left unusable. When `bytes` covers the whole file its CRC-32 is
 * verified too (crc_checked). */
sat_result_t sat_sprite_atlas_open(sat_sprite_atlas_t* atlas, const void* blob, uint32_t bytes);

uint32_t sat_sprite_atlas_frame_count(const sat_sprite_atlas_t* atlas);
uint32_t sat_sprite_atlas_group_count(const sat_sprite_atlas_t* atlas);
uint32_t sat_sprite_atlas_lut_count(const sat_sprite_atlas_t* atlas);
sat_result_t sat_sprite_atlas_frame_info(const sat_sprite_atlas_t* atlas, uint32_t frame,
                                         sat_sprite_atlas_frame_info_t* out);
sat_result_t sat_sprite_atlas_group_info(const sat_sprite_atlas_t* atlas, uint32_t group,
                                         sat_sprite_atlas_group_info_t* out);
/* The largest group, which sizes a slot. */
uint32_t sat_sprite_atlas_max_group_bytes(const sat_sprite_atlas_t* atlas);

/* Adds the atlas's VDP1 VRAM needs to a resource plan: `slots` slots of the
 * largest group plus the LUT4 tables. */
sat_result_t sat_sprite_atlas_plan_add(const sat_sprite_atlas_t* atlas, sat_resource_plan_t* plan,
                                       uint32_t slots);

/* The shared palette, 256 entries RGB555 in the layout sat_palette_upload_indexed8
 * takes, converted to host order. out must hold palette_entries (<= 256)
 * entries; *out_entries receives the count. */
sat_result_t sat_sprite_atlas_palette(const sat_sprite_atlas_t* atlas, uint16_t* out,
                                      uint32_t capacity, uint32_t* out_entries);
/* Uploads the shared palette to CRAM bank `bank` and remembers the bank for
 * INDEX8 frames. */
sat_result_t sat_sprite_atlas_upload_palette(sat_sprite_atlas_t* atlas, uint16_t bank);

/* Reserves VRAM for a slot, `bytes` (0: the largest group), once per slot. */
sat_result_t sat_sprite_atlas_reserve_slot(sat_sprite_atlas_t* atlas, uint32_t slot, uint32_t bytes);
/* Uploads the shared LUT4 tables, once; later calls do nothing. */
sat_result_t sat_sprite_atlas_upload_luts(sat_sprite_atlas_t* atlas);

/* Writes group `group` into `slot` in one call; `data` holds the group's
 * data_bytes. A group already resident in the slot is not written again
 * (stats.loads_skipped). */
sat_result_t sat_sprite_atlas_load_group(sat_sprite_atlas_t* atlas, uint32_t slot, uint32_t group,
                                         const void* data, uint32_t bytes);
/* The same in pieces, for prefetching a chunk per frame: begin, any number of
 * chunks in order, end. Beginning on a slot drops what it held, so the frames
 * built from it are refused until end. Chunks may be any size (the last ends
 * exactly at data_bytes). Returns SAT_OK from begin without writing when the
 * group is already resident: *out_skipped is then 1 and chunk/end must not
 * follow. */
sat_result_t sat_sprite_atlas_upload_begin(sat_sprite_atlas_t* atlas, uint32_t slot, uint32_t group,
                                           uint8_t* out_skipped);
sat_result_t sat_sprite_atlas_upload_chunk(sat_sprite_atlas_t* atlas, const void* data, uint32_t bytes);
sat_result_t sat_sprite_atlas_upload_end(sat_sprite_atlas_t* atlas);
/* Bytes still to be sent for the upload in progress, 0 when none. */
uint32_t sat_sprite_atlas_upload_remaining(const sat_sprite_atlas_t* atlas);
/* Forgets what a slot holds (its VRAM stays reserved). */
sat_result_t sat_sprite_atlas_evict(sat_sprite_atlas_t* atlas, uint32_t slot);
/* The slot a group is resident in, or SAT_ERR_NOT_FOUND. */
sat_result_t sat_sprite_atlas_group_slot(const sat_sprite_atlas_t* atlas, uint32_t group, uint32_t* out_slot);

/* Fills frames[i] for every frame i of a resident group, ready for
 * sat_sprite_batch_draw, with the sprite flags given (SAT_SPRITE_FLAG_OPAQUE,
 * _MESH, flips). frames needs frame_count entries, indexed by the atlas's
 * global frame index; entries of other groups are left alone, so one array
 * serves every group and a swap only rewrites the new group's range.
 * Requires the shared palette bank (INDEX8) and LUT tables (LUT4) to have
 * been uploaded when such frames exist. */
sat_result_t sat_sprite_atlas_build_frames(const sat_sprite_atlas_t* atlas, uint32_t group,
                                           uint16_t flags, sat_sprite_frame_t* frames,
                                           uint32_t frame_capacity);

#ifdef __cplusplus
}
#endif
#endif
