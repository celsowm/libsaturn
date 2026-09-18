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

struct FileAssetRuntime {
    FileMount mounts[SAT_FILE_MOUNT_CAPACITY];
    FileHandle handles[SAT_FILE_HANDLE_CAPACITY];
    AssetEntry assets[SAT_ASSET_CAPACITY];
};

extern FileAssetRuntime g_file_asset_runtime;

sat_result_t normalize_path(const char* input, char* output, uint16_t output_size);
uint16_t next_file_generation(uint16_t generation);
void file_asset_runtime_reset(FileAssetRuntime& runtime);

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
