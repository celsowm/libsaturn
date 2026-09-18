#ifndef SATURN_ASSET_H
#define SATURN_ASSET_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SAT_ASSET_CAPACITY 32u

typedef enum sat_asset_kind {
    SAT_ASSET_DATA = 0,
    SAT_ASSET_TEXTURE = 1,
    SAT_ASSET_FONT = 2,
    SAT_ASSET_SOUND = 3,
    SAT_ASSET_STREAM = 4
} sat_asset_kind_t;

typedef struct sat_asset {
    uint16_t slot;
    uint16_t generation;
} sat_asset_t;

typedef struct sat_asset_desc {
    const char* logical_path;
    const void* data;
    uint32_t size;
    uint16_t width;
    uint16_t height;
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t format;
    uint16_t flags;
    sat_asset_kind_t kind;
} sat_asset_desc_t;

typedef struct sat_asset_info {
    sat_asset_kind_t kind;
    const void* data;
    uint32_t size;
    uint16_t width;
    uint16_t height;
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t format;
    uint16_t flags;
} sat_asset_info_t;

sat_result_t sat_asset_reset(void);
sat_result_t sat_asset_register(
    const sat_asset_desc_t* desc,
    sat_asset_t* out_asset
);
sat_result_t sat_asset_open(const char* logical_path, sat_asset_t* out_asset);
sat_result_t sat_asset_info(sat_asset_t asset, sat_asset_info_t* out_info);
sat_result_t sat_asset_close(sat_asset_t asset);
uint16_t sat_asset_count(void);
uint16_t sat_asset_capacity(void);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_ASSET_H */
