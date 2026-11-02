#ifndef SATURN_CORE_FILE_ASSET_RUNTIME_HPP
#define SATURN_CORE_FILE_ASSET_RUNTIME_HPP

#include <stdint.h>

#include "saturn/asset.h"
#include "saturn/asset_memory.h"
#include "saturn/file.h"

namespace saturn::core {

using FileMount = sat_file_mount_slot_t;
using FileHandle = sat_file_handle_slot_t;
using AssetEntry = sat_asset_slot_t;
using AssetCacheBlock = sat_asset_cache_slot_t;
using AssetPrefetchRequest = sat_asset_prefetch_slot_t;

/* The slots are the program's (saturn/asset_memory.h); only the counters live here. */
inline uint16_t mount_capacity() { return sat_asset_memory.mount_capacity; }
inline uint16_t handle_capacity() { return sat_asset_memory.handle_capacity; }
inline uint16_t asset_capacity() { return sat_asset_memory.asset_capacity; }
inline uint16_t prefetch_capacity() { return sat_asset_memory.prefetch_capacity; }
inline uint16_t cache_capacity() { return sat_asset_memory.cache_capacity; }

struct FileAssetRuntime {
    FileMount* mounts() const { return sat_asset_memory.mounts; }
    FileHandle* handles() const { return sat_asset_memory.handles; }
    AssetEntry* assets() const { return sat_asset_memory.assets; }
    AssetCacheBlock* cache() const { return sat_asset_memory.cache; }
    AssetPrefetchRequest* prefetch() const { return sat_asset_memory.prefetch; }
    uint16_t cache_block_count;
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
void asset_cache_configure(FileAssetRuntime& runtime, uint8_t* memory, uint16_t blocks);
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
    if (file.slot >= handle_capacity() || file.generation == 0u) return nullptr;
    FileHandle& handle = runtime.handles()[file.slot];
    if (handle.used == 0u || handle.generation != file.generation) return nullptr;
    return &handle;
}

inline const FileHandle* resolve_file(const FileAssetRuntime& runtime, sat_file_t file) {
    if (file.slot >= handle_capacity() || file.generation == 0u) return nullptr;
    const FileHandle& handle = runtime.handles()[file.slot];
    if (handle.used == 0u || handle.generation != file.generation) return nullptr;
    return &handle;
}

inline AssetEntry* resolve_asset(FileAssetRuntime& runtime, sat_asset_t asset) {
    if (asset.slot >= asset_capacity() || asset.generation == 0u) return nullptr;
    AssetEntry& entry = runtime.assets()[asset.slot];
    if (entry.used == 0u || entry.generation != asset.generation) return nullptr;
    return &entry;
}

inline const AssetEntry* resolve_asset(const FileAssetRuntime& runtime, sat_asset_t asset) {
    if (asset.slot >= asset_capacity() || asset.generation == 0u) return nullptr;
    const AssetEntry& entry = runtime.assets()[asset.slot];
    if (entry.used == 0u || entry.generation != asset.generation) return nullptr;
    return &entry;
}

}  // namespace saturn::core

#endif /* SATURN_CORE_FILE_ASSET_RUNTIME_HPP */
