#include "saturn/asset_memory.h"
#include "src/core/memory/section.hpp"

/* The default table (saturn/asset_memory.h): a program that defines `sat_asset_memory` leaves this file, and the
 * slots below, out of the link. */
namespace {

SATURN_WRAM_L sat_file_mount_slot_t g_mounts[SAT_FILE_MOUNT_CAPACITY];
SATURN_WRAM_L sat_file_handle_slot_t g_handles[SAT_FILE_HANDLE_CAPACITY];
SATURN_WRAM_L sat_asset_slot_t g_assets[SAT_ASSET_CAPACITY];
SATURN_WRAM_L sat_asset_cache_slot_t g_cache[SAT_ASSET_CACHE_BLOCK_CAPACITY];
SATURN_WRAM_L sat_asset_prefetch_slot_t g_prefetch[SAT_ASSET_PREFETCH_CAPACITY];

}  // namespace

extern "C" const sat_asset_memory_t sat_asset_memory = {
    g_mounts, g_handles, g_assets, g_cache, g_prefetch,
    SAT_FILE_MOUNT_CAPACITY, SAT_FILE_HANDLE_CAPACITY, SAT_ASSET_CAPACITY,
    SAT_ASSET_CACHE_BLOCK_CAPACITY, SAT_ASSET_PREFETCH_CAPACITY};
