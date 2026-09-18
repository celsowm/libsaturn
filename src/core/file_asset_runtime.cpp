#include "src/core/file_asset_runtime.hpp"

#include <stddef.h>

namespace saturn::core {

FileAssetRuntime g_file_asset_runtime = {};

uint16_t next_file_generation(uint16_t generation) {
    ++generation;
    return generation == 0u ? 1u : generation;
}

void file_asset_runtime_reset(FileAssetRuntime& runtime) {
    for (uint16_t i = 0u; i < SAT_FILE_HANDLE_CAPACITY; ++i) {
        FileHandle& handle = runtime.handles[i];
        const uint16_t generation = next_file_generation(handle.generation);
        handle = {};
        handle.generation = generation;
    }
    for (uint16_t i = 0u; i < SAT_FILE_MOUNT_CAPACITY; ++i) runtime.mounts[i] = {};
    for (uint16_t i = 0u; i < SAT_ASSET_CAPACITY; ++i) {
        AssetEntry& entry = runtime.assets[i];
        const uint16_t generation = next_file_generation(entry.generation);
        entry = {};
        entry.generation = generation;
    }
    for (uint16_t i = 0u; i < SAT_ASSET_CACHE_BLOCK_CAPACITY; ++i) {
        runtime.cache[i] = {};
    }
    for (uint16_t i = 0u; i < SAT_ASSET_PREFETCH_CAPACITY; ++i) {
        AssetPrefetchRequest& request = runtime.prefetch[i];
        const uint16_t generation = next_file_generation(request.generation);
        request = {};
        request.generation = generation;
    }
    runtime.cache_clock = 0u;
    runtime.cache_hits = 0u;
    runtime.cache_misses = 0u;
    runtime.cache_fills = 0u;
    runtime.prefetch_completed = 0u;
    runtime.prefetch_failed = 0u;
}

namespace {

uint16_t find_mount(const FileAssetRuntime& runtime, const char* source_path) {
    for (uint16_t i = 0u; i < SAT_FILE_MOUNT_CAPACITY; ++i) {
        if (runtime.mounts[i].used != 0u &&
            __builtin_strcmp(runtime.mounts[i].path, source_path) == 0) return i;
    }
    return SAT_FILE_MOUNT_CAPACITY;
}

uint16_t choose_cache_slot(FileAssetRuntime& runtime) {
    uint16_t selected = SAT_ASSET_CACHE_BLOCK_CAPACITY;
    uint32_t oldest = 0xFFFFFFFFu;
    for (uint16_t i = 0u; i < SAT_ASSET_CACHE_BLOCK_CAPACITY; ++i) {
        if (runtime.cache[i].used == 0u) return i;
        if (runtime.cache[i].last_used < oldest) {
            oldest = runtime.cache[i].last_used;
            selected = i;
        }
    }
    return selected;
}

}  // namespace

sat_result_t asset_cache_fill(
    FileAssetRuntime& runtime,
    const char* source_path,
    uint32_t block_index,
    uint32_t* out_valid_bytes
) {
    if (source_path == nullptr || out_valid_bytes == nullptr) return SAT_ERR_INVALID_ARG;
    for (uint16_t i = 0u; i < SAT_ASSET_CACHE_BLOCK_CAPACITY; ++i) {
        AssetCacheBlock& block = runtime.cache[i];
        if (block.used != 0u && block.block_index == block_index &&
            __builtin_strcmp(block.source_path, source_path) == 0) {
            block.last_used = ++runtime.cache_clock;
            *out_valid_bytes = block.valid_bytes;
            ++runtime.cache_hits;
            return SAT_OK;
        }
    }
    ++runtime.cache_misses;
    const uint16_t mount_slot = find_mount(runtime, source_path);
    if (mount_slot == SAT_FILE_MOUNT_CAPACITY) return SAT_ERR_NOT_FOUND;
    const FileMount& mount = runtime.mounts[mount_slot];
    const uint32_t offset = block_index * SAT_ASSET_CACHE_BLOCK_BYTES;
    if (offset >= mount.size) return SAT_ERR_INVALID_ARG;
    const uint32_t valid_bytes = (mount.size - offset) < SAT_ASSET_CACHE_BLOCK_BYTES
        ? mount.size - offset : SAT_ASSET_CACHE_BLOCK_BYTES;
    const uint16_t cache_slot = choose_cache_slot(runtime);
    if (cache_slot >= SAT_ASSET_CACHE_BLOCK_CAPACITY) return SAT_ERR_CAPACITY;
    AssetCacheBlock& block = runtime.cache[cache_slot];
    uint32_t total = 0u;
    while (total < valid_bytes) {
        uint32_t read = 0u;
        sat_result_t status = SAT_OK;
        if (mount.read_at != nullptr) {
            status = mount.read_at(mount.context, offset + total,
                                   block.data + total, valid_bytes - total, &read);
        } else {
            for (uint32_t i = total; i < valid_bytes; ++i) {
                block.data[i] = mount.data[offset + i];
            }
            read = valid_bytes - total;
        }
        if (status != SAT_OK || read == 0u || read > valid_bytes - total) return SAT_ERR_IO;
        total += read;
    }
    uint16_t j = 0u;
    while (source_path[j] != '\0' && j + 1u < SAT_FILE_PATH_MAX) {
        block.source_path[j] = source_path[j];
        ++j;
    }
    block.source_path[j] = '\0';
    block.block_index = block_index;
    block.valid_bytes = static_cast<uint16_t>(valid_bytes);
    block.last_used = ++runtime.cache_clock;
    block.used = 1u;
    ++runtime.cache_fills;
    *out_valid_bytes = valid_bytes;
    return SAT_OK;
}

sat_result_t asset_cache_read_at(
    FileAssetRuntime& runtime,
    const char* source_path,
    uint32_t offset,
    void* destination,
    uint32_t bytes,
    uint32_t* out_read
) {
    if (source_path == nullptr || out_read == nullptr ||
        (destination == nullptr && bytes != 0u)) return SAT_ERR_INVALID_ARG;
    *out_read = 0u;
    if (bytes == 0u) return SAT_OK;
    uint8_t* output = static_cast<uint8_t*>(destination);
    uint32_t remaining = bytes;
    while (remaining != 0u) {
        const uint32_t block_index = offset / SAT_ASSET_CACHE_BLOCK_BYTES;
        const uint32_t in_block = offset % SAT_ASSET_CACHE_BLOCK_BYTES;
        uint32_t valid_bytes = 0u;
        SAT_TRY(asset_cache_fill(runtime, source_path, block_index, &valid_bytes));
        if (in_block >= valid_bytes) return SAT_ERR_IO;
        uint32_t count = valid_bytes - in_block;
        if (count > remaining) count = remaining;
        for (uint32_t i = 0u; i < count; ++i) {
            for (uint16_t slot = 0u; slot < SAT_ASSET_CACHE_BLOCK_CAPACITY; ++slot) {
                const AssetCacheBlock& block = runtime.cache[slot];
                if (block.used != 0u && block.block_index == block_index &&
                    __builtin_strcmp(block.source_path, source_path) == 0) {
                    output[i] = block.data[in_block + i];
                    break;
                }
            }
        }
        output += count;
        offset += count;
        remaining -= count;
        *out_read += count;
        if (count == 0u) return SAT_ERR_IO;
    }
    return SAT_OK;
}

sat_result_t normalize_path(const char* input, char* output, uint16_t output_size) {
    if (input == nullptr || output == nullptr || output_size < 2u || input[0] == '\0') {
        return SAT_ERR_INVALID_ARG;
    }
    uint16_t written = 0u;
    uint16_t i = 0u;
    while (input[i] != '\0') {
        while (input[i] == '/' || input[i] == '\\') ++i;
        if (input[i] == '\0') break;
        const uint16_t component_start = i;
        while (input[i] != '\0' && input[i] != '/' && input[i] != '\\') ++i;
        const uint16_t component_length = static_cast<uint16_t>(i - component_start);
        if (component_length == 1u && input[component_start] == '.') continue;
        if ((component_length == 2u && input[component_start] == '.' &&
             input[component_start + 1u] == '.')) {
            return SAT_ERR_INVALID_ARG;
        }
        if (written != 0u) {
            if (written + 1u >= output_size) return SAT_ERR_CAPACITY;
            output[written++] = '/';
        }
        if (written + component_length >= output_size) return SAT_ERR_CAPACITY;
        for (uint16_t j = 0u; j < component_length; ++j) {
            output[written++] = input[component_start + j];
        }
    }
    if (written == 0u) return SAT_ERR_INVALID_ARG;
    output[written] = '\0';
    return SAT_OK;
}

}  // namespace saturn::core
