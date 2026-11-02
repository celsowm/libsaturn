#ifndef SATURN_ASSET_MEMORY_H
#define SATURN_ASSET_MEMORY_H
/* How many file mounts, open files, assets, cache blocks and prefetch requests a program has (optional; most
 * programs never include this).
 *
 * The file and asset registries are tables of slots. The library supplies the table and the slots it names
 * (src/storage/files/asset_memory.cpp): SAT_FILE_MOUNT_CAPACITY mounts, SAT_FILE_HANDLE_CAPACITY handles,
 * SAT_ASSET_CAPACITY assets, SAT_ASSET_CACHE_BLOCK_CAPACITY cache blocks and SAT_ASSET_PREFETCH_CAPACITY prefetch
 * requests, about 13 KiB of Work RAM. A program that registers a few assets can define the table itself in one source
 * file with slots of its own; the linker then leaves the library's table, and its slots, out of the program:
 *
 *     #include "saturn/asset_memory.h"
 *     static sat_file_mount_slot_t mounts[2];
 *     static sat_file_handle_slot_t handles[2];
 *     static sat_asset_slot_t assets[2];
 *     static sat_asset_cache_slot_t cache[1];
 *     static sat_asset_prefetch_slot_t prefetch[1];
 *     SAT_ASSET_MEMORY(mounts, handles, assets, cache, prefetch);
 *
 * The slots are zero-initialized storage the library owns while the program runs. A mount, handle or asset past the
 * capacity fails as SAT_ERR_CAPACITY, the same as past the default. A cache of N blocks (sat_asset_cache_configure)
 * needs N cache slots; the capacity of each array is its element count. */
#include <stdint.h>

#include "saturn/asset.h"
#include "saturn/file.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sat_file_mount_slot {
    char path[SAT_FILE_PATH_MAX];
    const uint8_t* data;
    uint32_t size;
    sat_file_read_at_fn read_at;
    void* context;
    uint8_t used;
} sat_file_mount_slot_t;

typedef struct sat_file_handle_slot {
    uint16_t mount_slot;
    uint16_t generation;
    uint32_t position;
    uint8_t used;
} sat_file_handle_slot_t;

typedef struct sat_asset_slot {
    char path[SAT_FILE_PATH_MAX];
    sat_asset_info_t info;
    uint16_t generation;
    uint8_t used;
} sat_asset_slot_t;

typedef struct sat_asset_cache_slot {
    char source_path[SAT_FILE_PATH_MAX];
    uint32_t block_index;
    uint32_t valid_bytes;
    uint32_t last_used;
    uint8_t* data;
    uint8_t used;
} sat_asset_cache_slot_t;

typedef struct sat_asset_prefetch_slot {
    char source_path[SAT_FILE_PATH_MAX];
    uint32_t next_block;
    uint32_t end_block;
    uint32_t cached_bytes;
    sat_result_t result;
    uint16_t generation;
    uint8_t state;
    uint8_t used;
} sat_asset_prefetch_slot_t;

typedef struct sat_asset_memory {
    sat_file_mount_slot_t* mounts;
    sat_file_handle_slot_t* handles;
    sat_asset_slot_t* assets;
    sat_asset_cache_slot_t* cache;
    sat_asset_prefetch_slot_t* prefetch;
    uint16_t mount_capacity, handle_capacity, asset_capacity, cache_capacity, prefetch_capacity;
} sat_asset_memory_t;

extern const sat_asset_memory_t sat_asset_memory;

#ifdef __cplusplus
}
#endif

#define SAT_ASSET_MEMORY(mount_slots, handle_slots, asset_slots, cache_slots, prefetch_slots) \
    const sat_asset_memory_t sat_asset_memory = { \
        (mount_slots), (handle_slots), (asset_slots), (cache_slots), (prefetch_slots), \
        (uint16_t)(sizeof(mount_slots) / sizeof((mount_slots)[0])), \
        (uint16_t)(sizeof(handle_slots) / sizeof((handle_slots)[0])), \
        (uint16_t)(sizeof(asset_slots) / sizeof((asset_slots)[0])), \
        (uint16_t)(sizeof(cache_slots) / sizeof((cache_slots)[0])), \
        (uint16_t)(sizeof(prefetch_slots) / sizeof((prefetch_slots)[0]))}

#endif
