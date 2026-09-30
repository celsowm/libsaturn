#include "ikemen_asset_store.h"

#define IK_CART_ALIGNMENT 32u

static uint32_t min_u32(uint32_t a, uint32_t b) { return a < b ? a : b; }

static sat_result_t require_4mb_cart(ik_asset_store_t* store) {
    sat_ram_cart_info_t info = {SAT_RAM_CART_NONE, 0u, 0u, 0u};
    sat_result_t st = sat_ram_cart_init();
    if (st != SAT_OK) return st;
    st = sat_ram_cart_info(&info);
    if (st != SAT_OK) return st;
    if (info.type != SAT_RAM_CART_4MB) return SAT_ERR_UNSUPPORTED;
    store->cart_capacity = info.capacity;
    store->cart_free_bytes = info.free_bytes;
    return SAT_OK;
}

sat_result_t ik_asset_store_init(
    ik_asset_store_t* store, const char* sprite_disc_path,
    uint32_t expected_sprite_bytes, uint8_t* staging, uint32_t staging_bytes) {
    if (store == 0 || sprite_disc_path == 0 || expected_sprite_bytes == 0u ||
        staging == 0 || staging_bytes < SAT_CD_SECTOR_BYTES) return SAT_ERR_INVALID_ARG;

    *store = (ik_asset_store_t){0};
    SAT_TRY(require_4mb_cart(store));
    if (expected_sprite_bytes > store->cart_free_bytes) return SAT_ERR_CAPACITY;

    SAT_TRY(sat_cd_block_init(&store->block, SAT_CD_BLOCK_DEFAULT_TIMEOUT));
    SAT_TRY(sat_cd_block_bind_device(&store->block, &store->device, 0u));
    SAT_TRY(sat_cdfs_mount(&store->volume, &store->device));

    sat_cdfs_file_t file = {0u, 0u, 0u, {0u, 0u, 0u}};
    SAT_TRY(sat_cdfs_lookup(&store->volume, sprite_disc_path, &file));
    if (file.directory != 0u || file.size != expected_sprite_bytes) return SAT_ERR_VERSION;

    SAT_TRY(sat_ram_cart_buffer_alloc(file.size, IK_CART_ALIGNMENT, &store->sprites));
    for (uint32_t offset = 0u; offset < file.size;) {
        const uint32_t chunk = min_u32(staging_bytes, file.size - offset);
        uint32_t read = 0u;
        SAT_TRY(sat_cdfs_read_at(&store->volume, &file, offset, staging, chunk, &read));
        if (read != chunk) return SAT_ERR_IO;
        SAT_TRY(sat_ram_cart_buffer_write_at(&store->sprites, offset, staging, chunk));
        offset += chunk;
    }

    store->sprite_bytes = file.size;
    sat_ram_cart_info_t info = {SAT_RAM_CART_NONE, 0u, 0u, 0u};
    SAT_TRY(sat_ram_cart_info(&info));
    store->cart_capacity = info.capacity;
    store->cart_free_bytes = info.free_bytes;
    return SAT_OK;
}

sat_result_t ik_asset_store_read_sprite(
    const ik_asset_store_t* store, const ik_sprite_source_t* source,
    uint8_t* destination, uint32_t destination_capacity) {
    if (store == 0 || source == 0 || destination == 0 ||
        store->sprite_bytes == 0u || source->data_size == 0u) return SAT_ERR_INVALID_ARG;
    if (source->data_size > destination_capacity) return SAT_ERR_CAPACITY;
    if (source->data_ofs > store->sprite_bytes ||
        source->data_size > store->sprite_bytes - source->data_ofs) return SAT_ERR_IO;
    return sat_ram_cart_buffer_read_at(
        &store->sprites, source->data_ofs, destination, source->data_size);
}
