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
        entry.info.width = desc->width;
        entry.info.height = desc->height;
        entry.info.sample_rate = desc->sample_rate;
        entry.info.channels = desc->channels;
        entry.info.format = desc->format;
        entry.info.flags = desc->flags;
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
