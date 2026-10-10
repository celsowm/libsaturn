#include "src/storage/files/asset_runtime.hpp"

namespace saturn::core {

namespace {

/* The built-in cache blocks (~269 KB). Only this file references them, and
 * only assets.cpp references this file, so a program that never uses the
 * asset API does not link them. crt0 zeroes .wram_l. */
uint8_t g_asset_cache_blocks[SAT_ASSET_CACHE_DEFAULT_BLOCK_CAPACITY][SAT_ASSET_CACHE_BLOCK_BYTES]
    __attribute__((section(".wram_l"), aligned(4))) = {};

}  // namespace

void asset_cache_configure(FileAssetRuntime& runtime, uint8_t* memory, uint16_t blocks) {
    const uint16_t count = blocks == 0u ? SAT_ASSET_CACHE_DEFAULT_BLOCK_CAPACITY : blocks;
    runtime.cache_block_count = count;
    runtime.cache_configured = 1u;
    for (uint16_t i = 0u; i < SAT_ASSET_CACHE_BLOCK_CAPACITY; ++i) {
        runtime.cache[i] = {};
        if (i < count) {
            runtime.cache[i].data = memory == nullptr
                ? g_asset_cache_blocks[i]
                : memory + static_cast<uint32_t>(i) * SAT_ASSET_CACHE_BLOCK_BYTES;
        }
    }
    runtime.cache_clock = 0u;
    runtime.cache_hits = 0u;
    runtime.cache_misses = 0u;
    runtime.cache_fills = 0u;
}

namespace {

using CopyWord = uint32_t __attribute__((may_alias));

uint16_t find_mount(const FileAssetRuntime& runtime, const char* source_path) {
    for (uint16_t i = 0u; i < SAT_FILE_MOUNT_CAPACITY; ++i) {
        if (runtime.mounts[i].used != 0u &&
            __builtin_strcmp(runtime.mounts[i].path, source_path) == 0) return i;
    }
    return SAT_FILE_MOUNT_CAPACITY;
}

uint16_t choose_cache_slot(FileAssetRuntime& runtime) {
    uint16_t selected = runtime.cache_block_count;
    uint32_t oldest = 0xFFFFFFFFu;
    for (uint16_t i = 0u; i < runtime.cache_block_count; ++i) {
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
    if (runtime.cache_configured == 0u) asset_cache_configure(runtime, nullptr, 0u);
    for (uint16_t i = 0u; i < runtime.cache_block_count; ++i) {
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
    if (cache_slot >= runtime.cache_block_count) return SAT_ERR_CAPACITY;
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
    block.valid_bytes = valid_bytes;
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
        const AssetCacheBlock* cached_block = nullptr;
        for (uint16_t slot = 0u; slot < runtime.cache_block_count; ++slot) {
            const AssetCacheBlock& block = runtime.cache[slot];
            if (block.used != 0u && block.block_index == block_index &&
                __builtin_strcmp(block.source_path, source_path) == 0) {
                cached_block = &block;
                break;
            }
        }
        if (cached_block == nullptr) return SAT_ERR_IO;

        // Resolve the cache block once, then copy linearly.  The old hot path
        // repeated the cache scan and source-path strcmp for every output
        // byte, turning an 8 KiB music feed into tens of thousands of string
        // comparisons on the SH-2 and causing periodic audio starvation.
        const uint8_t* source = cached_block->data + in_block;
        uint32_t copied = 0u;
        if (((reinterpret_cast<uintptr_t>(output) |
              reinterpret_cast<uintptr_t>(source)) & 3u) == 0u) {
            CopyWord* out_words = reinterpret_cast<CopyWord*>(output);
            const CopyWord* in_words = reinterpret_cast<const CopyWord*>(source);
            const uint32_t words = count / 4u;
            for (uint32_t i = 0u; i < words; ++i) out_words[i] = in_words[i];
            copied = words * 4u;
        }
        for (; copied < count; ++copied) output[copied] = source[copied];
        output += count;
        offset += count;
        remaining -= count;
        *out_read += count;
        if (count == 0u) return SAT_ERR_IO;
    }
    return SAT_OK;
}

}  // namespace saturn::core
