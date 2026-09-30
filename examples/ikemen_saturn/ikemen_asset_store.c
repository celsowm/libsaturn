#include "ikemen_asset_store.h"

#define IK_CART_ALIGNMENT 32u

static uint32_t min_u32(uint32_t a, uint32_t b) {
    return a < b ? a : b;
}

static sat_result_t refresh_cart_info(ik_asset_store_t* store) {
    sat_ram_cart_info_t info = {SAT_RAM_CART_NONE, 0u, 0u, 0u};
    SAT_TRY(sat_ram_cart_info(&info));
    store->cart_capacity = info.capacity;
    store->cart_free_bytes = info.free_bytes;
    return SAT_OK;
}

sat_result_t ik_asset_store_init(ik_asset_store_t* store) {
    if (store == 0) return SAT_ERR_INVALID_ARG;
    *store = (ik_asset_store_t){0};

    sat_result_t st = sat_ram_cart_init();
    if (st != SAT_OK) return st;

    sat_ram_cart_info_t info = {SAT_RAM_CART_NONE, 0u, 0u, 0u};
    SAT_TRY(sat_ram_cart_info(&info));
    if (info.type != SAT_RAM_CART_4MB) return SAT_ERR_UNSUPPORTED;
    store->cart_capacity = info.capacity;
    store->cart_free_bytes = info.free_bytes;

    SAT_TRY(sat_cd_block_init(&store->block, SAT_CD_BLOCK_DEFAULT_TIMEOUT));
    SAT_TRY(sat_cd_block_bind_device(&store->block, &store->device, 0u));
    return sat_cdfs_mount(&store->volume, &store->device);
}

sat_result_t ik_asset_store_load_blob(
    ik_asset_store_t* store,
    uint8_t slot,
    const char* disc_path,
    uint32_t expected_bytes,
    uint8_t* staging,
    uint32_t staging_bytes
) {
    if (store == 0 || slot >= IK_ASSET_SLOT_COUNT || disc_path == 0 ||
        expected_bytes == 0u || staging == 0 ||
        staging_bytes < SAT_CD_SECTOR_BYTES) {
        return SAT_ERR_INVALID_ARG;
    }
    if (store->blobs[slot].loaded) return SAT_ERR_ALREADY_EXISTS;
    if (expected_bytes > store->cart_free_bytes) return SAT_ERR_CAPACITY;

    sat_cdfs_file_t file = {0u, 0u, 0u, {0u, 0u, 0u}};
    SAT_TRY(sat_cdfs_lookup(&store->volume, disc_path, &file));
    if (file.directory != 0u || file.size != expected_bytes) {
        return SAT_ERR_VERSION;
    }

    ik_asset_blob_t* blob = &store->blobs[slot];
    SAT_TRY(sat_ram_cart_buffer_alloc(
        file.size, IK_CART_ALIGNMENT, &blob->buffer));

    for (uint32_t offset = 0u; offset < file.size;) {
        const uint32_t chunk = min_u32(staging_bytes, file.size - offset);
        uint32_t read = 0u;
        SAT_TRY(sat_cdfs_read_at(
            &store->volume, &file, offset, staging, chunk, &read));
        if (read != chunk) return SAT_ERR_IO;
        SAT_TRY(sat_ram_cart_buffer_write_at(
            &blob->buffer, offset, staging, chunk));
        offset += chunk;
    }

    blob->bytes = file.size;
    blob->loaded = 1u;
    return refresh_cart_info(store);
}

sat_result_t ik_asset_store_read(
    const ik_asset_store_t* store,
    uint8_t slot,
    uint32_t offset,
    void* destination,
    uint32_t bytes
) {
    if (store == 0 || slot >= IK_ASSET_SLOT_COUNT ||
        (destination == 0 && bytes != 0u)) {
        return SAT_ERR_INVALID_ARG;
    }
    const ik_asset_blob_t* blob = &store->blobs[slot];
    if (!blob->loaded) return SAT_ERR_NOT_INITIALIZED;
    if (offset > blob->bytes || bytes > blob->bytes - offset) {
        return SAT_ERR_IO;
    }
    return sat_ram_cart_buffer_read_at(
        &blob->buffer, offset, destination, bytes);
}

sat_result_t ik_asset_store_read_sprite(
    const ik_asset_store_t* store,
    uint8_t slot,
    const ik_sprite_source_t* source,
    uint8_t* destination,
    uint32_t destination_capacity
) {
    if (source == 0 || destination == 0 || source->data_size == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    if (source->data_size > destination_capacity) return SAT_ERR_CAPACITY;
    return ik_asset_store_read(
        store, slot, source->data_ofs, destination, source->data_size);
}
