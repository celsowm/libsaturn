#ifndef SATURN_CD_H
#define SATURN_CD_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SAT_CD_SECTOR_BYTES 2048u

/* A sector transport is deliberately separate from ISO9660 and VFS policy.
 * A real Saturn CD Block driver, a test image, or another storage device can
 * provide this callback. A successful call must transfer every requested
 * sector; partial sector transfers are SAT_ERR_IO. */
typedef sat_result_t (*sat_cd_read_sectors_fn)(
    void* context,
    uint32_t lba,
    uint32_t sector_count,
    void* destination
);

typedef struct sat_cd_device {
    sat_cd_read_sectors_fn read_sectors;
    void* context;
    uint32_t sector_count;
} sat_cd_device_t;

sat_result_t sat_cd_device_init(
    sat_cd_device_t* out_device,
    sat_cd_read_sectors_fn read_sectors,
    void* context,
    uint32_t sector_count
);
sat_result_t sat_cd_read_sectors(
    const sat_cd_device_t* device,
    uint32_t lba,
    uint32_t sector_count,
    void* destination
);

/* Arrival observer: a transport that fills a read in batches (the CD Block
 * does, as sectors come off the disc) reports where each batch ends, so the
 * reader can work on the first bytes while the drive reads the rest
 * (sat_load_lz decodes that way). `end` is one past the last byte stored.
 * One observer at a time, set around one read; NULL clears it. The observer
 * runs inside the read and must not read the disc. */
typedef void (*sat_cd_arrival_fn)(void* context, const void* end);
void sat_cd_set_arrival_observer(sat_cd_arrival_fn fn, void* context);
/* For transports: reports a batch stored up to `end`. */
void sat_cd_notify_arrival(const void* end);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_CD_H */
