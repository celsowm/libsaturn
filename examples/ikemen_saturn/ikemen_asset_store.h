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

#define IK_ASSET_SLOT_P1 0u
#define IK_ASSET_SLOT_P2 1u
#define IK_ASSET_SLOT_FIGHTFX 2u
#define IK_ASSET_SLOT_COUNT 3u

typedef struct ik_asset_blob {
    sat_ram_cart_buffer_t buffer;
    uint32_t bytes;
    uint8_t loaded;
} ik_asset_blob_t;

typedef struct ik_asset_store {
    sat_cd_block_t block;
    sat_cd_device_t device;
    sat_cdfs_volume_t volume;
    ik_asset_blob_t blobs[IK_ASSET_SLOT_COUNT];
    uint32_t cart_capacity;
    uint32_t cart_free_bytes;
} ik_asset_store_t;

/* Initializes the mandatory 4 MiB cartridge and the shared CD/CDFS transport.
 * Blob allocations are explicit and live for the whole fight. */
sat_result_t ik_asset_store_init(ik_asset_store_t* store);

/* Loads one disc file into a named resident cart slot. Slots are deliberately
 * generic: P1/P2 use packed SFF data today and FIGHTFX is reserved for the
 * common effects pack. */
sat_result_t ik_asset_store_load_blob(
    ik_asset_store_t* store,
    uint8_t slot,
    const char* disc_path,
    uint32_t expected_bytes,
    uint8_t* staging,
    uint32_t staging_bytes);

sat_result_t ik_asset_store_read(
    const ik_asset_store_t* store,
    uint8_t slot,
    uint32_t offset,
    void* destination,
    uint32_t bytes);

sat_result_t ik_asset_store_read_sprite(
    const ik_asset_store_t* store,
    uint8_t slot,
    const ik_sprite_source_t* source,
    uint8_t* destination,
    uint32_t destination_capacity);

#ifdef __cplusplus
}
#endif

#endif
