#include "bh_stream.h"

#include "saturn/vdp2.h"

#define SECTOR 2048u
#define BG_W 512u
#define BG_H 256u
#define BG_PALETTE_BYTES 512u

/* A job's `arg`: the group or background index in the low 16 bits, the atlas slot or CRAM bank
 * above them. */
#define ARG(target, slot) ((uint32_t)(target) | ((uint32_t)(slot) << 16))
#define ARG_TARGET(arg) ((uint16_t)((arg) & 0xFFFFu))
#define ARG_SLOT(arg) ((uint8_t)((arg) >> 16))

/* ---- group sink ---------------------------------------------------------------------------- */

static sat_result_t group_begin(void* user, uint32_t arg, sat_disc_extent_t* extent) {
    bh_stream_t* s = (bh_stream_t*)user;
    sat_sprite_atlas_group_info_t gi;
    uint8_t skipped = 0;
    sat_result_t r = sat_sprite_atlas_group_info(s->atlas, ARG_TARGET(arg), &gi);
    if (r == SAT_OK) r = sat_sprite_atlas_upload_begin(s->atlas, ARG_SLOT(arg), ARG_TARGET(arg), &skipped);
    if (r != SAT_OK) return r;
    if (skipped) {   /* already resident: nothing to read */
        extent->file = 0;
        return SAT_OK;
    }
    s->bytes_left = gi.data_bytes;
    extent->file = &s->atlas_file;
    extent->offset = gi.data_offset;
    extent->bytes = gi.data_bytes;
    return SAT_OK;
}

static sat_result_t group_chunk(void* user, uint32_t arg, const uint8_t* data, uint32_t bytes) {
    bh_stream_t* s = (bh_stream_t*)user;
    (void)arg;
    sat_result_t r = sat_sprite_atlas_upload_chunk(s->atlas, data, bytes);
    s->bytes_left -= bytes;
    if (r == SAT_OK && s->bytes_left == 0u) r = sat_sprite_atlas_upload_end(s->atlas);
    return r;
}

static void group_done(void* user, uint32_t arg, sat_result_t result) {
    bh_stream_t* s = (bh_stream_t*)user;
    if (result == SAT_OK) s->groups_done |= 1u << ARG_TARGET(arg);
}

static const sat_disc_job_ops_t k_group_ops = {group_begin, group_chunk, group_done};

/* ---- background sink ----------------------------------------------------------------------- */

static sat_result_t background_begin(void* user, uint32_t arg, sat_disc_extent_t* extent) {
    bh_stream_t* s = (bh_stream_t*)user;
    s->first_segment = 1u;
    s->bytes_left = 0u;
    s->vram_word = s->bg_vram[ARG_TARGET(arg)] >> 1;
    extent->file = &s->bg_file[ARG_TARGET(arg)];
    extent->offset = 0u;
    extent->bytes = 0u;
    return SAT_OK;
}

static sat_result_t background_chunk(void* user, uint32_t arg, const uint8_t* p, uint32_t avail) {
    bh_stream_t* s = (bh_stream_t*)user;
    sat_result_t r = SAT_OK;
    if (s->first_segment) {
        /* Sector 0: 16-byte header, then the 256-entry CRAM palette; pixels start at the next sector. */
        uint16_t w, h, flags;
        __builtin_memcpy(&w, p + 4, 2); __builtin_memcpy(&h, p + 6, 2); __builtin_memcpy(&flags, p + 10, 2);
        if (!(p[0] == 'S' && p[1] == 'B' && p[2] == 'G' && p[3] == '1') || w != BG_W || h != BG_H || flags != 0) return SAT_ERR_INVALID_ARG;
        r = sat_vdp2_palette_upload((const uint16_t*)(const void*)(p + BH_BG_HEADER_BYTES), BG_PALETTE_BYTES / 2u,
                                    (uint16_t)(ARG_SLOT(arg) * 256u));
        if (r != SAT_OK) return r;
        s->first_segment = 0u;
        s->bytes_left = BG_W * BG_H;
        p += SECTOR;
        avail -= SECTOR;
    }
    const uint32_t n = s->bytes_left < avail ? s->bytes_left : avail;
    if (n) {
        r = sat_vdp2_vram_write_words(s->vram_word, (const uint16_t*)(const void*)p, n >> 1);
        s->vram_word += n >> 1;
        s->bytes_left -= n;
    }
    return r;
}

static void background_done(void* user, uint32_t arg, sat_result_t result) {
    bh_stream_t* s = (bh_stream_t*)user;
    if (result == SAT_OK) s->backgrounds_done |= 1u << ARG_TARGET(arg);
}

static const sat_disc_job_ops_t k_background_ops = {background_begin, background_chunk, background_done};

/* ---- API ----------------------------------------------------------------------------------- */

sat_result_t bh_stream_init(bh_stream_t* s, sat_cd_block_t* block, sat_cdfs_volume_t* volume,
                            sat_sprite_atlas_t* atlas, const char* atlas_path,
                            const char* const* bg_paths, uint8_t bg_count) {
    if (!s || !block || !volume || !atlas || !atlas_path || bg_count > BH_STREAM_MAX_BACKGROUNDS) return SAT_ERR_INVALID_ARG;
    sat_result_t r = sat_disc_stream_init(&s->disc, block);
    if (r != SAT_OK) return r;
    s->atlas = atlas;
    s->bg_files = 0u;
    s->groups_done = s->backgrounds_done = 0u;
    r = sat_cdfs_lookup(volume, atlas_path, &s->atlas_file);
    if (r != SAT_OK) return r;
    for (uint8_t i = 0; i < bg_count; ++i) {
        r = sat_cdfs_lookup(volume, bg_paths[i], &s->bg_file[i]);
        if (r != SAT_OK) return r;
        if (s->bg_file[i].size != 2048u + BG_W * BG_H) return SAT_ERR_INVALID_ARG;
    }
    s->bg_files = bg_count;
    return SAT_OK;
}

sat_result_t bh_stream_queue_group(bh_stream_t* s, uint16_t group, uint8_t slot) {
    if (group >= BH_STREAM_MAX_GROUPS) return SAT_ERR_INVALID_ARG;
    const sat_disc_job_t job = {&k_group_ops, s, ARG(group, slot), {0, 0, 0}};
    return sat_disc_stream_queue(&s->disc, &job);
}

sat_result_t bh_stream_queue_background(bh_stream_t* s, uint16_t index, uint8_t cram_bank, uint32_t vram_bytes) {
    if (index >= s->bg_files || (vram_bytes & 0x1FFFFu) || vram_bytes + BG_W * BG_H > 0x80000u) return SAT_ERR_INVALID_ARG;
    const sat_disc_job_t job = {&k_background_ops, s, ARG(index, cram_bank), {0, 0, 0}};
    const sat_result_t r = sat_disc_stream_queue(&s->disc, &job);
    if (r == SAT_OK) s->bg_vram[index] = vram_bytes;
    return r;
}

sat_result_t bh_stream_music_start(bh_stream_t* s, sat_disc_music_rings_t* rings, sat_cdfs_volume_t* volume,
                                   const char* path, uint32_t bytes, uint32_t sample_rate, uint32_t sample_bytes) {
    if (!s || !volume || !path) return SAT_ERR_INVALID_ARG;
    const sat_result_t r = sat_cdfs_lookup(volume, path, &s->music_file);
    if (r != SAT_OK) return r;
    return sat_disc_stream_music_start(&s->disc, rings, &s->music_file, bytes, sample_rate, sample_bytes);
}
