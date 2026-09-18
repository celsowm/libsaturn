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
