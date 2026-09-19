#ifndef SATURN_ASSET_H
#define SATURN_ASSET_H

#include <stdint.h>

#include "saturn/audio.h"
#include "saturn/core.h"
#include "saturn/font.h"
#include "saturn/file.h"
#include "saturn/texture.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SAT_ASSET_CAPACITY 32u
#define SAT_ASSET_CACHE_BLOCK_BYTES 65536u
#define SAT_ASSET_CACHE_BLOCK_CAPACITY 4u
#define SAT_ASSET_PREFETCH_CAPACITY 4u

typedef enum sat_asset_kind {
    SAT_ASSET_DATA = 0,
    SAT_ASSET_TEXTURE = 1,
    SAT_ASSET_FONT = 2,
    SAT_ASSET_SOUND = 3,
    SAT_ASSET_STREAM = 4
} sat_asset_kind_t;

typedef struct sat_asset {
    uint16_t slot;
    uint16_t generation;
} sat_asset_t;

typedef struct sat_asset_desc {
    const char* logical_path;
    /* Optional physical/runtime source path for a non-embedded payload. */
    const char* source_path;
    const void* data;
    uint32_t size;
    uint32_t pitch;
    uint16_t width;
    uint16_t height;
    const uint16_t* palette_rgb555;
    uint16_t palette_count;
    uint32_t sample_rate;
    uint32_t sample_count;
    uint8_t channels;
    uint8_t format;
    uint16_t flags;
    const sat_font_glyph_t* glyphs;
    uint16_t glyph_count;
    uint16_t line_height;
    uint16_t fallback_glyph;
    sat_asset_kind_t kind;
} sat_asset_desc_t;

typedef struct sat_asset_info {
    sat_asset_kind_t kind;
    const char* source_path;
    const void* data;
    uint32_t size;
    uint32_t pitch;
    uint16_t width;
    uint16_t height;
    const uint16_t* palette_rgb555;
    uint16_t palette_count;
    uint32_t sample_rate;
    uint32_t sample_count;
    uint8_t channels;
    uint8_t format;
    uint16_t flags;
    const sat_font_glyph_t* glyphs;
    uint16_t glyph_count;
    uint16_t line_height;
    uint16_t fallback_glyph;
} sat_asset_info_t;

typedef struct sat_asset_prefetch {
    uint16_t slot;
    uint16_t generation;
} sat_asset_prefetch_t;

typedef enum sat_asset_prefetch_state {
    SAT_ASSET_PREFETCH_PENDING = 0,
    SAT_ASSET_PREFETCH_COMPLETE = 1,
    SAT_ASSET_PREFETCH_FAILED = 2
} sat_asset_prefetch_state_t;

typedef struct sat_asset_cache_stats {
    uint16_t used;
    uint16_t capacity;
    uint16_t prefetch_pending;
    uint16_t prefetch_capacity;
    uint32_t hits;
    uint32_t misses;
    uint32_t fills;
    uint32_t prefetch_completed;
    uint32_t prefetch_failed;
} sat_asset_cache_stats_t;

sat_result_t sat_asset_reset(void);
sat_result_t sat_asset_register(
    const sat_asset_desc_t* desc,
    sat_asset_t* out_asset
);
sat_result_t sat_asset_open(const char* logical_path, sat_asset_t* out_asset);
sat_result_t sat_asset_info(sat_asset_t asset, sat_asset_info_t* out_info);
sat_result_t sat_asset_close(sat_asset_t asset);
uint16_t sat_asset_count(void);
uint16_t sat_asset_capacity(void);

/* Submit a bounded, cooperative cache warm-up for a non-resident logical
 * asset. No worker thread is created: each sat_asset_prefetch_update call
 * services at most one cache block, making CD-backed work explicit and
 * deterministic. The request owns no caller storage. */
sat_result_t sat_asset_prefetch_submit(
    const char* logical_path,
    uint32_t offset,
    uint32_t bytes,
    sat_asset_prefetch_t* out_request
);
sat_result_t sat_asset_prefetch_update(void);
sat_result_t sat_asset_prefetch_status(
    sat_asset_prefetch_t request,
    sat_asset_prefetch_state_t* out_state,
    sat_result_t* out_result,
    uint32_t* out_cached_bytes
);
sat_result_t sat_asset_prefetch_cancel(sat_asset_prefetch_t request);
sat_result_t sat_asset_cache_stats(sat_asset_cache_stats_t* out_stats);

/* Typed logical-path loaders. The registered payload and auxiliary metadata
 * remain caller-owned for the lifetime of the returned runtime object. */
sat_result_t sat_asset_load_data(
    const char* logical_path,
    const void** out_data,
    uint32_t* out_size
);
sat_result_t sat_asset_read_at(
    const char* logical_path,
    uint32_t offset,
    void* destination,
    uint32_t bytes,
    uint32_t* out_read
);
sat_result_t sat_texture_load(const char* logical_path, sat_texture_t* out_texture);
sat_result_t sat_sound_load(const char* logical_path, sat_sound_t* out_sound);
sat_result_t sat_font_load(const char* logical_path, sat_font_t* out_font);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_ASSET_H */
