#ifndef IKEMEN_ASSET_STORE_H
#define IKEMEN_ASSET_STORE_H
#include <stdint.h>
#include "saturn/cd_block.h"
#include "saturn/cdfs.h"
#include "saturn/core.h"
#include "saturn/ram_cart.h"
#include "ikemen_anim.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct ik_asset_store {
    sat_cd_block_t block;
    sat_cd_device_t device;
    sat_cdfs_volume_t volume;
    sat_ram_cart_buffer_t sprites;
    uint32_t sprite_bytes;
    uint32_t cart_capacity;
    uint32_t cart_free_bytes;
} ik_asset_store_t;
sat_result_t ik_asset_store_init(
    ik_asset_store_t* store, const char* sprite_disc_path,
    uint32_t expected_sprite_bytes, uint8_t* staging, uint32_t staging_bytes);
sat_result_t ik_asset_store_read_sprite(
    const ik_asset_store_t* store, const ik_sprite_source_t* source,
    uint8_t* destination, uint32_t destination_capacity);
#ifdef __cplusplus
}
#endif
#endif
