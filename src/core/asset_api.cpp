#include "saturn/asset.h"

#include "src/core/file_asset_runtime.hpp"

extern "C" sat_result_t sat_asset_reset(void) {
    saturn::core::file_asset_runtime_reset(saturn::core::g_file_asset_runtime);
    return SAT_OK;
}

extern "C" sat_result_t sat_asset_register(
    const sat_asset_desc_t* desc,
    sat_asset_t* out_asset
) {
    if (desc == nullptr || out_asset == nullptr || desc->logical_path == nullptr ||
        (desc->data == nullptr && desc->size != 0u)) return SAT_ERR_INVALID_ARG;
    char normalized[SAT_FILE_PATH_MAX] = {};
    SAT_TRY(saturn::core::normalize_path(desc->logical_path, normalized, SAT_FILE_PATH_MAX));
    using namespace saturn::core;
    for (uint16_t i = 0u; i < SAT_ASSET_CAPACITY; ++i) {
        AssetEntry& entry = g_file_asset_runtime.assets[i];
        if (entry.used != 0u) {
            if (__builtin_strcmp(entry.path, normalized) == 0) return SAT_ERR_INVALID_ARG;
            continue;
        }
        if (entry.generation == 0u) entry.generation = 1u;
        uint16_t j = 0u;
        while (normalized[j] != '\0') {
            entry.path[j] = normalized[j];
            ++j;
        }
        entry.path[j] = '\0';
        entry.info.kind = desc->kind;
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
        out_asset->slot = i;
        out_asset->generation = entry.generation;
        return SAT_OK;
    }
    return SAT_ERR_CAPACITY;
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
    if (info.data == nullptr && info.size != 0u) return SAT_ERR_IO;
    *out_data = info.data;
    *out_size = info.size;
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
