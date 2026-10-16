#include "src/storage/files/asset_runtime.hpp"
#include "src/core/memory/section.hpp"

#include <stddef.h>

namespace saturn::core {

/* Mounts, handles and asset entries. The built-in cache blocks live in
 * asset_cache.cpp, linked only by programs that use the asset API. */
FileAssetRuntime g_file_asset_runtime SATURN_WRAM_L = {};

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
    for (uint16_t i = 0u; i < SAT_ASSET_CACHE_BLOCK_CAPACITY; ++i) runtime.cache[i] = {};
    runtime.cache_block_count = 0u;
    runtime.cache_configured = 0u; /* the first fill configures the built-in blocks */
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
