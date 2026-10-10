#include "saturn/cart_overlay.h"

#include "src/storage/cartridge/overlay_hal.hpp"

namespace overlay_hal = saturn::storage::cart_overlay;

extern "C" sat_result_t sat_cart_overlay_init(sat_cart_overlay_t* overlay, uint32_t base,
                                              uint32_t capacity) {
    if (overlay == nullptr) return SAT_ERR_INVALID_ARG;
    const uint32_t cached = base & ~overlay_hal::kCacheThroughBit;
    if (capacity == 0u || (cached & 3u) != 0u || cached < overlay_hal::kCartBase ||
        cached >= overlay_hal::kCartEnd || capacity > overlay_hal::kCartEnd - cached) {
        return SAT_ERR_INVALID_ARG;
    }
    overlay->base = cached;
    overlay->capacity = capacity;
    overlay->resident = SAT_CART_OVERLAY_NONE;
    return SAT_OK;
}

extern "C" sat_result_t sat_cart_overlay_load(
    sat_cart_overlay_t* overlay,
    sat_cdfs_volume_t* volume,
    int32_t id,
    const char* disc_path,
    uint32_t expected_size,
    sat_load_progress_fn progress, void* context) {
    if (overlay == nullptr || overlay->capacity == 0u || volume == nullptr ||
        disc_path == nullptr || id < 0) {
        return SAT_ERR_INVALID_ARG;
    }
    if (overlay->resident == id) return SAT_OK;

    sat_cdfs_file_t file;
    SAT_TRY(sat_cdfs_lookup(volume, disc_path, &file));
    if (file.directory != 0u) return SAT_ERR_INVALID_ARG;
    if (expected_size != 0u && file.size != expected_size) return SAT_ERR_VERSION;
    if (file.size > overlay->capacity) return SAT_ERR_CAPACITY;

    overlay->resident = SAT_CART_OVERLAY_NONE;
    if (file.size != 0u) {
        const sat_load_item_t item = {
            disc_path, file.size, 0u, file.size, overlay_hal::write_pointer(overlay->base)
        };
        const sat_result_t rc = sat_load_files(volume, &item, 1u, 0u, progress, context);
        overlay_hal::purge_cpu_cache();
        if (rc != SAT_OK) return rc;
    }
    overlay->resident = id;
    return SAT_OK;
}

extern "C" sat_result_t sat_cart_overlay_load_lz(
    sat_cart_overlay_t* overlay,
    sat_cdfs_volume_t* volume,
    int32_t id,
    const char* disc_path,
    uint32_t expected_decoded,
    sat_load_progress_fn progress, void* context) {
    if (overlay == nullptr || overlay->capacity == 0u || volume == nullptr ||
        disc_path == nullptr || id < 0) {
        return SAT_ERR_INVALID_ARG;
    }
    if (overlay->resident == id) return SAT_OK;

    sat_cdfs_file_t file;
    SAT_TRY(sat_cdfs_lookup(volume, disc_path, &file));
    if (file.directory != 0u) return SAT_ERR_INVALID_ARG;
    if (file.size > overlay->capacity) return SAT_ERR_CAPACITY;

    overlay->resident = SAT_CART_OVERLAY_NONE;
    const sat_result_t rc = sat_load_lz(volume, disc_path, overlay_hal::write_pointer(overlay->base),
                                        overlay->capacity, expected_decoded, progress, context, nullptr);
    overlay_hal::purge_cpu_cache();
    if (rc != SAT_OK) return rc;
    overlay->resident = id;
    return SAT_OK;
}

extern "C" void sat_cart_overlay_forget(sat_cart_overlay_t* overlay) {
    if (overlay != nullptr) overlay->resident = SAT_CART_OVERLAY_NONE;
}
