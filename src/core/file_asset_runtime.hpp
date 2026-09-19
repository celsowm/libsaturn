#ifndef SATURN_CORE_FILE_ASSET_RUNTIME_HPP
#define SATURN_CORE_FILE_ASSET_RUNTIME_HPP

#include <stdint.h>

#include "saturn/asset.h"
#include "saturn/file.h"

namespace saturn::core {

struct FileMount {
    char path[SAT_FILE_PATH_MAX];
    const uint8_t* data;
    uint32_t size;
    sat_file_read_at_fn read_at;
    void* context;
    uint8_t used;
};

struct FileHandle {
    uint16_t mount_slot;
    uint16_t generation;
    uint32_t position;
    uint8_t used;
};

struct AssetEntry {
    char path[SAT_FILE_PATH_MAX];
    sat_asset_info_t info;
    uint16_t generation;
    uint8_t used;
};

struct AssetCacheBlock {
    char source_path[SAT_FILE_PATH_MAX];
    uint32_t block_index;
    uint32_t valid_bytes;
    uint32_t last_used;
    uint8_t data[SAT_ASSET_CACHE_BLOCK_BYTES];
    uint8_t used;
};

struct AssetPrefetchRequest {
    char source_path[SAT_FILE_PATH_MAX];
    uint32_t next_block;
    uint32_t end_block;
    uint32_t cached_bytes;
    sat_result_t result;
    uint16_t generation;
    uint8_t state;
    uint8_t used;
};

struct FileAssetRuntime {
    FileMount mounts[SAT_FILE_MOUNT_CAPACITY];
    FileHandle handles[SAT_FILE_HANDLE_CAPACITY];
    AssetEntry assets[SAT_ASSET_CAPACITY];
    AssetCacheBlock cache[SAT_ASSET_CACHE_BLOCK_CAPACITY];
    AssetPrefetchRequest prefetch[SAT_ASSET_PREFETCH_CAPACITY];
    uint32_t cache_clock;
    uint32_t cache_hits;
    uint32_t cache_misses;
    uint32_t cache_fills;
    uint32_t prefetch_completed;
    uint32_t prefetch_failed;
};

extern FileAssetRuntime g_file_asset_runtime;

sat_result_t normalize_path(const char* input, char* output, uint16_t output_size);
uint16_t next_file_generation(uint16_t generation);
void file_asset_runtime_reset(FileAssetRuntime& runtime);
sat_result_t asset_cache_read_at(
    FileAssetRuntime& runtime,
    const char* source_path,
    uint32_t offset,
    void* destination,
    uint32_t bytes,
    uint32_t* out_read
);
sat_result_t asset_cache_fill(
    FileAssetRuntime& runtime,
    const char* source_path,
    uint32_t block_index,
    uint32_t* out_valid_bytes
);

inline FileHandle* resolve_file(FileAssetRuntime& runtime, sat_file_t file) {
    if (file.slot >= SAT_FILE_HANDLE_CAPACITY || file.generation == 0u) return nullptr;
    FileHandle& handle = runtime.handles[file.slot];
    if (handle.used == 0u || handle.generation != file.generation) return nullptr;
    return &handle;
}

inline const FileHandle* resolve_file(const FileAssetRuntime& runtime, sat_file_t file) {
    if (file.slot >= SAT_FILE_HANDLE_CAPACITY || file.generation == 0u) return nullptr;
    const FileHandle& handle = runtime.handles[file.slot];
    if (handle.used == 0u || handle.generation != file.generation) return nullptr;
    return &handle;
}

inline AssetEntry* resolve_asset(FileAssetRuntime& runtime, sat_asset_t asset) {
    if (asset.slot >= SAT_ASSET_CAPACITY || asset.generation == 0u) return nullptr;
    AssetEntry& entry = runtime.assets[asset.slot];
    if (entry.used == 0u || entry.generation != asset.generation) return nullptr;
    return &entry;
}

inline const AssetEntry* resolve_asset(const FileAssetRuntime& runtime, sat_asset_t asset) {
    if (asset.slot >= SAT_ASSET_CAPACITY || asset.generation == 0u) return nullptr;
    const AssetEntry& entry = runtime.assets[asset.slot];
    if (entry.used == 0u || entry.generation != asset.generation) return nullptr;
    return &entry;
}

}  // namespace saturn::core

#endif /* SATURN_CORE_FILE_ASSET_RUNTIME_HPP */
