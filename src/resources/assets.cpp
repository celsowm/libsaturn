#include "saturn/asset.h"

#include "src/storage/files/asset_runtime.hpp"

namespace {

sat_result_t lookup_asset(const char* logical_path, sat_asset_info_t* out_info);

bool asset_descriptor_valid(const sat_asset_desc_t* desc) {
    return desc != nullptr && desc->logical_path != nullptr &&
           !(desc->data != nullptr && desc->source_path != nullptr) &&
           !(desc->data == nullptr && desc->source_path == nullptr && desc->size != 0u);
}

sat_result_t normalize_source_path(const char* source_path, char* normalized) {
    return saturn::core::normalize_path(source_path, normalized, SAT_FILE_PATH_MAX);
}

saturn::core::AssetPrefetchRequest* resolve_prefetch(sat_asset_prefetch_t request) {
    using namespace saturn::core;
    if (request.slot >= SAT_ASSET_PREFETCH_CAPACITY || request.generation == 0u) return nullptr;
    AssetPrefetchRequest& entry = g_file_asset_runtime.prefetch[request.slot];
    if (entry.used == 0u || entry.generation != request.generation) return nullptr;
    return &entry;
}

}  // namespace

extern "C" sat_result_t sat_asset_reset(void) {
    saturn::core::file_asset_runtime_reset(saturn::core::g_file_asset_runtime);
    return SAT_OK;
}

extern "C" sat_result_t sat_asset_register(
    const sat_asset_desc_t* desc,
    sat_asset_t* out_asset
) {
    if (!asset_descriptor_valid(desc) || out_asset == nullptr) return SAT_ERR_INVALID_ARG;
    char normalized[SAT_FILE_PATH_MAX] = {};
    SAT_TRY(saturn::core::normalize_path(desc->logical_path, normalized, SAT_FILE_PATH_MAX));
    using namespace saturn::core;
    // Check the complete registry before reusing a hole: a later live slot
    // may already own the same normalized logical path.
    uint16_t free_slot = SAT_ASSET_CAPACITY;
    for (uint16_t i = 0u; i < SAT_ASSET_CAPACITY; ++i) {
        const AssetEntry& candidate = g_file_asset_runtime.assets[i];
        if (candidate.used != 0u) {
            if (__builtin_strcmp(candidate.path, normalized) == 0) return SAT_ERR_INVALID_ARG;
        } else if (free_slot == SAT_ASSET_CAPACITY) {
            free_slot = i;
        }
    }
    if (free_slot == SAT_ASSET_CAPACITY) return SAT_ERR_CAPACITY;
    AssetEntry& entry = g_file_asset_runtime.assets[free_slot];
    if (entry.generation == 0u) entry.generation = 1u;
    uint16_t j = 0u;
    while (normalized[j] != '\0') {
        entry.path[j] = normalized[j];
        ++j;
    }
    entry.path[j] = '\0';
    entry.info.kind = desc->kind;
    entry.info.source_path = desc->source_path;
    entry.info.data = desc->data;
    entry.info.size = desc->size;
    entry.info.pitch = desc->pitch;
    entry.info.width = desc->width;
    entry.info.height = desc->height;
    entry.info.palette_rgb555 = desc->palette_rgb555;
    entry.info.palette_count = desc->palette_count;
    entry.info.sample_rate = desc->sample_rate;
    entry.info.sample_count = desc->sample_count;
    entry.info.channels = desc->channels;
    entry.info.format = desc->format;
    entry.info.flags = desc->flags;
    entry.info.glyphs = desc->glyphs;
    entry.info.glyph_count = desc->glyph_count;
    entry.info.line_height = desc->line_height;
    entry.info.fallback_glyph = desc->fallback_glyph;
    entry.used = 1u;
    out_asset->slot = free_slot;
    out_asset->generation = entry.generation;
    return SAT_OK;
}

extern "C" sat_result_t sat_asset_register_manifest(
    const sat_asset_desc_t* descs, uint16_t count, sat_asset_t* out_assets) {
    if ((!descs && count) || (!out_assets && count) || count == 0u)
        return SAT_ERR_INVALID_ARG;
    if (count > sat_asset_capacity() - sat_asset_count()) return SAT_ERR_CAPACITY;
    // Fully validate the batch, including normalized path collisions with
    // existing entries, before publishing any handle. The bounded registry
    // is modified only by the subsequent, non-failing insertion phase.
    char normalized[SAT_FILE_PATH_MAX] = {};
    char previous[SAT_FILE_PATH_MAX] = {};
    for (uint16_t i = 0u; i < count; ++i) {
        if (!asset_descriptor_valid(&descs[i])) return SAT_ERR_INVALID_ARG;
        SAT_TRY(saturn::core::normalize_path(
            descs[i].logical_path, normalized, SAT_FILE_PATH_MAX));
        for (uint16_t slot = 0u; slot < SAT_ASSET_CAPACITY; ++slot) {
            const saturn::core::AssetEntry& entry =
                saturn::core::g_file_asset_runtime.assets[slot];
            if (entry.used != 0u && __builtin_strcmp(entry.path, normalized) == 0)
                return SAT_ERR_INVALID_ARG;
        }
        for (uint16_t j = 0u; j < i; ++j) {
            SAT_TRY(saturn::core::normalize_path(
                descs[j].logical_path, previous, SAT_FILE_PATH_MAX));
            if (__builtin_strcmp(normalized, previous) == 0) return SAT_ERR_INVALID_ARG;
        }
    }
    for (uint16_t i = 0u; i < count; ++i) {
        const sat_result_t status = sat_asset_register(&descs[i], &out_assets[i]);
        if (status != SAT_OK) return status;
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_asset_open(const char* logical_path, sat_asset_t* out_asset) {
    if (logical_path == nullptr || out_asset == nullptr) return SAT_ERR_INVALID_ARG;
    char normalized[SAT_FILE_PATH_MAX] = {};
    SAT_TRY(saturn::core::normalize_path(logical_path, normalized, SAT_FILE_PATH_MAX));
    for (uint16_t i = 0u; i < SAT_ASSET_CAPACITY; ++i) {
        const saturn::core::AssetEntry& entry = saturn::core::g_file_asset_runtime.assets[i];
        if (entry.used != 0u && __builtin_strcmp(entry.path, normalized) == 0) {
            out_asset->slot = i;
            out_asset->generation = entry.generation;
            return SAT_OK;
        }
    }
    return SAT_ERR_NOT_FOUND;
}

extern "C" sat_result_t sat_asset_info(sat_asset_t asset, sat_asset_info_t* out_info) {
    if (out_info == nullptr) return SAT_ERR_INVALID_ARG;
    const saturn::core::AssetEntry* entry = saturn::core::resolve_asset(
        saturn::core::g_file_asset_runtime, asset);
    if (entry == nullptr) return SAT_ERR_INVALID_ARG;
    *out_info = entry->info;
    return SAT_OK;
}

extern "C" sat_result_t sat_asset_close(sat_asset_t asset) {
    saturn::core::AssetEntry* entry = saturn::core::resolve_asset(
        saturn::core::g_file_asset_runtime, asset);
    if (entry == nullptr) return SAT_ERR_INVALID_ARG;
    entry->used = 0u;
    entry->generation = saturn::core::next_file_generation(entry->generation);
    entry->path[0] = '\0';
    entry->info = {};
    return SAT_OK;
}

extern "C" uint16_t sat_asset_count(void) {
    uint16_t count = 0u;
    for (uint16_t i = 0u; i < SAT_ASSET_CAPACITY; ++i) {
        count += saturn::core::g_file_asset_runtime.assets[i].used != 0u ? 1u : 0u;
    }
    return count;
}

extern "C" uint16_t sat_asset_capacity(void) {
    return SAT_ASSET_CAPACITY;
}

extern "C" sat_result_t sat_asset_prefetch_submit(
    const char* logical_path,
    uint32_t offset,
    uint32_t bytes,
    sat_asset_prefetch_t* out_request
) {
    if (logical_path == nullptr || out_request == nullptr) return SAT_ERR_INVALID_ARG;
    sat_asset_info_t info{};
    SAT_TRY(lookup_asset(logical_path, &info));
    if (info.source_path == nullptr) return SAT_ERR_UNSUPPORTED;
    if (offset > info.size) return SAT_ERR_INVALID_ARG;
    char source_path[SAT_FILE_PATH_MAX] = {};
    SAT_TRY(normalize_source_path(info.source_path, source_path));
    uint32_t end = offset + bytes;
    if (end < offset || end > info.size) end = info.size;
    const uint32_t first_block = offset / SAT_ASSET_CACHE_BLOCK_BYTES;
    const uint32_t end_block = end == offset
        ? first_block : (end - 1u) / SAT_ASSET_CACHE_BLOCK_BYTES;
    using namespace saturn::core;
    for (uint16_t i = 0u; i < SAT_ASSET_PREFETCH_CAPACITY; ++i) {
        AssetPrefetchRequest& request = g_file_asset_runtime.prefetch[i];
        if (request.used != 0u) continue;
        if (request.generation == 0u) request.generation = 1u;
        uint16_t j = 0u;
        while (source_path[j] != '\0') {
            request.source_path[j] = source_path[j];
            ++j;
        }
        request.source_path[j] = '\0';
        request.next_block = first_block;
        request.end_block = end_block;
        request.cached_bytes = 0u;
        request.result = SAT_OK;
        request.state = bytes == 0u ? SAT_ASSET_PREFETCH_COMPLETE : SAT_ASSET_PREFETCH_PENDING;
        request.used = 1u;
        out_request->slot = i;
        out_request->generation = request.generation;
        return SAT_OK;
    }
    return SAT_ERR_CAPACITY;
}

extern "C" sat_result_t sat_asset_prefetch_update(void) {
    using namespace saturn::core;
    for (uint16_t i = 0u; i < SAT_ASSET_PREFETCH_CAPACITY; ++i) {
        AssetPrefetchRequest& request = g_file_asset_runtime.prefetch[i];
        if (request.used == 0u || request.state != SAT_ASSET_PREFETCH_PENDING) continue;
        uint32_t valid_bytes = 0u;
        const sat_result_t status = asset_cache_fill(
            g_file_asset_runtime, request.source_path, request.next_block, &valid_bytes);
        if (status != SAT_OK) {
            request.result = status;
            request.state = SAT_ASSET_PREFETCH_FAILED;
            ++g_file_asset_runtime.prefetch_failed;
            return status;
        }
        request.cached_bytes += valid_bytes;
        if (request.next_block >= request.end_block) {
            request.state = SAT_ASSET_PREFETCH_COMPLETE;
            ++g_file_asset_runtime.prefetch_completed;
        } else {
            ++request.next_block;
        }
        return SAT_OK;
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_asset_prefetch_status(
    sat_asset_prefetch_t request,
    sat_asset_prefetch_state_t* out_state,
    sat_result_t* out_result,
    uint32_t* out_cached_bytes
) {
    if (out_state == nullptr || out_result == nullptr || out_cached_bytes == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    const saturn::core::AssetPrefetchRequest* entry = resolve_prefetch(request);
    if (entry == nullptr) return SAT_ERR_INVALID_ARG;
    *out_state = static_cast<sat_asset_prefetch_state_t>(entry->state);
    *out_result = entry->result;
    *out_cached_bytes = entry->cached_bytes;
    return SAT_OK;
}

extern "C" sat_result_t sat_asset_prefetch_cancel(sat_asset_prefetch_t request) {
    saturn::core::AssetPrefetchRequest* entry = resolve_prefetch(request);
    if (entry == nullptr) return SAT_ERR_INVALID_ARG;
    entry->used = 0u;
    entry->generation = saturn::core::next_file_generation(entry->generation);
    return SAT_OK;
}

extern "C" sat_result_t sat_asset_cache_configure(void* memory, uint16_t blocks) {
    using namespace saturn::core;
    if ((memory == nullptr && blocks != 0u) ||
        (memory != nullptr && (blocks == 0u || blocks > SAT_ASSET_CACHE_BLOCK_CAPACITY))) {
        return SAT_ERR_INVALID_ARG;
    }
    for (const auto& request : g_file_asset_runtime.prefetch) {
        if (request.used != 0u && request.state == SAT_ASSET_PREFETCH_PENDING) {
            return SAT_ERR_BUSY;
        }
    }
    asset_cache_configure(g_file_asset_runtime, static_cast<uint8_t*>(memory), blocks);
    return SAT_OK;
}

extern "C" sat_result_t sat_asset_cache_stats(sat_asset_cache_stats_t* out_stats) {
    if (out_stats == nullptr) return SAT_ERR_INVALID_ARG;
    using namespace saturn::core;
    uint16_t used = 0u;
    uint16_t pending = 0u;
    for (uint16_t i = 0u; i < g_file_asset_runtime.cache_block_count; ++i) {
        used += g_file_asset_runtime.cache[i].used != 0u ? 1u : 0u;
    }
    for (uint16_t i = 0u; i < SAT_ASSET_PREFETCH_CAPACITY; ++i) {
        pending += g_file_asset_runtime.prefetch[i].used != 0u &&
                   g_file_asset_runtime.prefetch[i].state == SAT_ASSET_PREFETCH_PENDING ? 1u : 0u;
    }
    out_stats->used = used;
    out_stats->capacity = g_file_asset_runtime.cache_block_count;
    out_stats->prefetch_pending = pending;
    out_stats->prefetch_capacity = SAT_ASSET_PREFETCH_CAPACITY;
    out_stats->hits = g_file_asset_runtime.cache_hits;
    out_stats->misses = g_file_asset_runtime.cache_misses;
    out_stats->fills = g_file_asset_runtime.cache_fills;
    out_stats->prefetch_completed = g_file_asset_runtime.prefetch_completed;
    out_stats->prefetch_failed = g_file_asset_runtime.prefetch_failed;
    return SAT_OK;
}

namespace {

sat_result_t lookup_asset(const char* logical_path, sat_asset_info_t* out_info) {
    if (logical_path == nullptr || out_info == nullptr) return SAT_ERR_INVALID_ARG;
    sat_asset_t asset{};
    SAT_TRY(sat_asset_open(logical_path, &asset));
    return sat_asset_info(asset, out_info);
}

}  // namespace

extern "C" sat_result_t sat_asset_load_data(
    const char* logical_path,
    const void** out_data,
    uint32_t* out_size
) {
    if (out_data == nullptr || out_size == nullptr) return SAT_ERR_INVALID_ARG;
    sat_asset_info_t info{};
    SAT_TRY(lookup_asset(logical_path, &info));
    if (info.kind != SAT_ASSET_DATA) return SAT_ERR_UNSUPPORTED;
    if (info.source_path != nullptr) return SAT_ERR_IO;
    if (info.data == nullptr && info.size != 0u) return SAT_ERR_IO;
    *out_data = info.data;
    *out_size = info.size;
    return SAT_OK;
}

extern "C" sat_result_t sat_asset_read_at(
    const char* logical_path,
    uint32_t offset,
    void* destination,
    uint32_t bytes,
    uint32_t* out_read
) {
    if (logical_path == nullptr || out_read == nullptr ||
        (destination == nullptr && bytes != 0u)) return SAT_ERR_INVALID_ARG;
    sat_asset_info_t info{};
    SAT_TRY(lookup_asset(logical_path, &info));
    if (offset > info.size) return SAT_ERR_INVALID_ARG;
    *out_read = 0u;
    if (bytes == 0u || offset == info.size) return SAT_OK;
    if (info.source_path != nullptr) {
        char source_path[SAT_FILE_PATH_MAX] = {};
        SAT_TRY(normalize_source_path(info.source_path, source_path));
        const uint32_t available = info.size - offset;
        const uint32_t count = bytes < available ? bytes : available;
        return saturn::core::asset_cache_read_at(
            saturn::core::g_file_asset_runtime, source_path, offset,
            destination, count, out_read);
    }
    if (info.data == nullptr) return SAT_ERR_IO;
    const uint32_t available = info.size - offset;
    const uint32_t count = bytes < available ? bytes : available;
    const uint8_t* source = static_cast<const uint8_t*>(info.data) + offset;
    uint8_t* output = static_cast<uint8_t*>(destination);
    for (uint32_t i = 0u; i < count; ++i) output[i] = source[i];
    *out_read = count;
    return SAT_OK;
}

extern "C" sat_result_t sat_texture_load(const char* logical_path, sat_texture_t* out_texture) {
    if (out_texture == nullptr) return SAT_ERR_INVALID_ARG;
    sat_asset_info_t info{};
    SAT_TRY(lookup_asset(logical_path, &info));
    if (info.kind != SAT_ASSET_TEXTURE && info.kind != SAT_ASSET_FONT) {
        return SAT_ERR_UNSUPPORTED;
    }
    if (info.data == nullptr || info.width == 0u || info.height == 0u ||
        info.palette_rgb555 == nullptr || info.palette_count != 256u) {
        return SAT_ERR_INVALID_ARG;
    }
    if (info.format != static_cast<uint8_t>(SAT_PIXEL_INDEX8)) return SAT_ERR_UNSUPPORTED;
    const uint32_t pitch = info.pitch == 0u ? info.width : info.pitch;
    if (pitch > 0xFFFFu || info.size < pitch * info.height) return SAT_ERR_INVALID_ARG;

    sat_surface_t surface{};
    surface.pixels = const_cast<void*>(info.data);
    surface.width = info.width;
    surface.height = info.height;
    surface.pitch = static_cast<uint16_t>(pitch);
    surface.format = SAT_PIXEL_INDEX8;
    surface.palette_rgb555 = info.palette_rgb555;
    surface.palette_count = info.palette_count;
    return sat_texture_create_from_surface(out_texture, &surface, SAT_TEXTURE_PERSISTENT_SOURCE);
}

extern "C" sat_result_t sat_sound_load(const char* logical_path, sat_sound_t* out_sound) {
    if (out_sound == nullptr) return SAT_ERR_INVALID_ARG;
    sat_asset_info_t info{};
    SAT_TRY(lookup_asset(logical_path, &info));
    if (info.kind != SAT_ASSET_SOUND || info.data == nullptr || info.sample_rate == 0u ||
        info.channels != 1u) {
        return SAT_ERR_INVALID_ARG;
    }
    if (info.format != SAT_AUDIO_PCM_S8 && info.format != SAT_AUDIO_PCM_S16) {
        return SAT_ERR_UNSUPPORTED;
    }
    const uint32_t bytes_per_sample = info.format == SAT_AUDIO_PCM_S16 ? 2u : 1u;
    if (info.size % bytes_per_sample != 0u) return SAT_ERR_INVALID_ARG;
    const uint32_t sample_count = info.sample_count == 0u
        ? info.size / bytes_per_sample
        : info.sample_count;
    if (sample_count == 0u || sample_count > 65535u ||
        sample_count * bytes_per_sample > info.size) return SAT_ERR_INVALID_ARG;

    sat_sound_desc_t desc{};
    desc.samples = info.data;
    desc.sample_count = sample_count;
    desc.sample_rate = info.sample_rate;
    desc.format = info.format;
    return sat_sound_create(out_sound, &desc);
}

extern "C" sat_result_t sat_font_load(const char* logical_path, sat_font_t* out_font) {
    if (out_font == nullptr) return SAT_ERR_INVALID_ARG;
    sat_asset_info_t info{};
    SAT_TRY(lookup_asset(logical_path, &info));
    if (info.kind != SAT_ASSET_FONT || info.glyphs == nullptr || info.glyph_count == 0u ||
        info.line_height == 0u) return SAT_ERR_INVALID_ARG;

    sat_texture_t atlas{};
    SAT_TRY(sat_texture_load(logical_path, &atlas));
    const sat_result_t st = sat_font_init(
        out_font, atlas, info.glyphs, info.glyph_count, info.line_height, info.fallback_glyph);
    if (st != SAT_OK) (void)sat_texture_destroy(atlas);
    return st;
}
