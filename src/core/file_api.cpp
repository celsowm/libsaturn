#include "saturn/file.h"

#include "src/core/file_asset_runtime.hpp"

namespace {

sat_result_t path_for_lookup(const char* path, char* normalized) {
    return saturn::core::normalize_path(path, normalized, SAT_FILE_PATH_MAX);
}

sat_result_t register_mount(
    const char* path,
    uint32_t size,
    const uint8_t* data,
    sat_file_read_at_fn read_at,
    void* context
) {
    if ((data == nullptr && read_at == nullptr && size != 0u) ||
        (data != nullptr && read_at != nullptr)) return SAT_ERR_INVALID_ARG;
    if (data == nullptr && read_at == nullptr && size == 0u) {
        /* Empty blobs are valid; a null reader means the zero-byte blob
         * backend and never gets called. */
    }
    char normalized[SAT_FILE_PATH_MAX] = {};
    SAT_TRY(path_for_lookup(path, normalized));
    using namespace saturn::core;
    for (uint16_t i = 0u; i < SAT_FILE_MOUNT_CAPACITY; ++i) {
        if (g_file_asset_runtime.mounts[i].used != 0u &&
            __builtin_strcmp(g_file_asset_runtime.mounts[i].path, normalized) == 0) {
            return SAT_ERR_INVALID_ARG;
        }
    }
    for (uint16_t i = 0u; i < SAT_FILE_MOUNT_CAPACITY; ++i) {
        FileMount& mount = g_file_asset_runtime.mounts[i];
        if (mount.used != 0u) continue;
        uint16_t j = 0u;
        while (normalized[j] != '\0') {
            mount.path[j] = normalized[j];
            ++j;
        }
        mount.path[j] = '\0';
        mount.data = data;
        mount.size = size;
        mount.read_at = read_at;
        mount.context = context;
        mount.used = 1u;
        return SAT_OK;
    }
    return SAT_ERR_CAPACITY;
}

}  // namespace

extern "C" sat_result_t sat_file_reset(void) {
    saturn::core::file_asset_runtime_reset(saturn::core::g_file_asset_runtime);
    return SAT_OK;
}

extern "C" sat_result_t sat_file_register_blob(
    const char* path,
    const void* data,
    uint32_t size
) {
    if (data == nullptr && size != 0u) return SAT_ERR_INVALID_ARG;
    return register_mount(path, size, static_cast<const uint8_t*>(data), nullptr, nullptr);
}

extern "C" sat_result_t sat_file_register_backend(
    const char* path,
    uint32_t size,
    sat_file_read_at_fn read_at,
    void* context
) {
    if (read_at == nullptr) return SAT_ERR_INVALID_ARG;
    return register_mount(path, size, nullptr, read_at, context);
}

extern "C" sat_result_t sat_file_open(const char* path, sat_file_t* out_file) {
    if (out_file == nullptr) return SAT_ERR_INVALID_ARG;
    char normalized[SAT_FILE_PATH_MAX] = {};
    SAT_TRY(path_for_lookup(path, normalized));
    using namespace saturn::core;
    uint16_t mount_slot = SAT_FILE_MOUNT_CAPACITY;
    for (uint16_t i = 0u; i < SAT_FILE_MOUNT_CAPACITY; ++i) {
        if (g_file_asset_runtime.mounts[i].used != 0u &&
            __builtin_strcmp(g_file_asset_runtime.mounts[i].path, normalized) == 0) {
            mount_slot = i;
            break;
        }
    }
    if (mount_slot == SAT_FILE_MOUNT_CAPACITY) return SAT_ERR_NOT_FOUND;
    for (uint16_t i = 0u; i < SAT_FILE_HANDLE_CAPACITY; ++i) {
        FileHandle& handle = g_file_asset_runtime.handles[i];
        if (handle.used != 0u) continue;
        if (handle.generation == 0u) handle.generation = 1u;
        handle.mount_slot = mount_slot;
        handle.position = 0u;
        handle.used = 1u;
        out_file->slot = i;
        out_file->generation = handle.generation;
        return SAT_OK;
    }
    return SAT_ERR_CAPACITY;
}

extern "C" sat_result_t sat_file_read(
    sat_file_t file,
    void* destination,
    uint32_t bytes,
    uint32_t* out_read
) {
    using namespace saturn::core;
    if (out_read == nullptr || (destination == nullptr && bytes != 0u)) return SAT_ERR_INVALID_ARG;
    FileHandle* handle = resolve_file(g_file_asset_runtime, file);
    if (handle == nullptr || handle->mount_slot >= SAT_FILE_MOUNT_CAPACITY) return SAT_ERR_INVALID_ARG;
    const FileMount& mount = g_file_asset_runtime.mounts[handle->mount_slot];
    const uint32_t available = mount.size - handle->position;
    const uint32_t count = bytes < available ? bytes : available;
    if (count == 0u) {
        *out_read = 0u;
        return SAT_OK;
    }
    if (mount.read_at != nullptr) {
        uint32_t backend_read = 0u;
        const sat_result_t st = mount.read_at(
            mount.context, handle->position, destination, count, &backend_read);
        if (st != SAT_OK) {
            *out_read = 0u;
            return st;
        }
        if (backend_read > count) {
            *out_read = 0u;
            return SAT_ERR_IO;
        }
        handle->position += backend_read;
        *out_read = backend_read;
        return SAT_OK;
    }
    const uint8_t* source = mount.data + handle->position;
    uint8_t* output = static_cast<uint8_t*>(destination);
    for (uint32_t i = 0u; i < count; ++i) output[i] = source[i];
    handle->position += count;
    *out_read = count;
    return SAT_OK;
}

extern "C" sat_result_t sat_file_seek(
    sat_file_t file,
    int32_t offset,
    sat_file_seek_origin_t origin
) {
    using namespace saturn::core;
    FileHandle* handle = resolve_file(g_file_asset_runtime, file);
    if (handle == nullptr || handle->mount_slot >= SAT_FILE_MOUNT_CAPACITY) return SAT_ERR_INVALID_ARG;
    const FileMount& mount = g_file_asset_runtime.mounts[handle->mount_slot];
    int64_t target = 0;
    if (origin == SAT_FILE_SEEK_SET) target = offset;
    else if (origin == SAT_FILE_SEEK_CURRENT) target = static_cast<int64_t>(handle->position) + offset;
    else if (origin == SAT_FILE_SEEK_END) target = static_cast<int64_t>(mount.size) + offset;
    else return SAT_ERR_INVALID_ARG;
    if (target < 0 || target > mount.size) return SAT_ERR_INVALID_ARG;
    handle->position = static_cast<uint32_t>(target);
    return SAT_OK;
}

extern "C" sat_result_t sat_file_tell(sat_file_t file, uint32_t* out_position) {
    if (out_position == nullptr) return SAT_ERR_INVALID_ARG;
    const saturn::core::FileHandle* handle = saturn::core::resolve_file(saturn::core::g_file_asset_runtime, file);
    if (handle == nullptr) return SAT_ERR_INVALID_ARG;
    *out_position = handle->position;
    return SAT_OK;
}

extern "C" sat_result_t sat_file_size(sat_file_t file, uint32_t* out_size) {
    if (out_size == nullptr) return SAT_ERR_INVALID_ARG;
    const saturn::core::FileHandle* handle = saturn::core::resolve_file(saturn::core::g_file_asset_runtime, file);
    if (handle == nullptr || handle->mount_slot >= SAT_FILE_MOUNT_CAPACITY) return SAT_ERR_INVALID_ARG;
    *out_size = saturn::core::g_file_asset_runtime.mounts[handle->mount_slot].size;
    return SAT_OK;
}

extern "C" sat_result_t sat_file_close(sat_file_t file) {
    using namespace saturn::core;
    FileHandle* handle = resolve_file(g_file_asset_runtime, file);
    if (handle == nullptr) return SAT_ERR_INVALID_ARG;
    handle->used = 0u;
    handle->generation = next_file_generation(handle->generation);
    return SAT_OK;
}

extern "C" uint16_t sat_file_mount_count(void) {
    uint16_t count = 0u;
    for (uint16_t i = 0u; i < SAT_FILE_MOUNT_CAPACITY; ++i) {
        count += saturn::core::g_file_asset_runtime.mounts[i].used != 0u ? 1u : 0u;
    }
    return count;
}

extern "C" uint16_t sat_file_handle_capacity(void) {
    return SAT_FILE_HANDLE_CAPACITY;
}
