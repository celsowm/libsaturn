#ifndef SATURN_CDFS_H
#define SATURN_CDFS_H

#include <stdint.h>

#include "saturn/cd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SAT_CDFS_PATH_MAX 96u

typedef struct sat_cdfs_file {
    uint32_t extent_lba;
    uint32_t size;
    uint8_t directory;
    uint8_t reserved[3];
} sat_cdfs_file_t;

/* Caller-owned mount state. The sector buffer makes lookup and unaligned
 * read_at operations bounded and heap-free; a volume is not reentrant while
 * one operation is using it. */
typedef struct sat_cdfs_volume {
    const sat_cd_device_t* device;
    sat_cdfs_file_t root;
    uint8_t mounted;
    uint8_t reserved[3];
    uint8_t sector_buffer[SAT_CD_SECTOR_BYTES];
} sat_cdfs_volume_t;

sat_result_t sat_cdfs_mount(
    sat_cdfs_volume_t* out_volume,
    const sat_cd_device_t* device
);
sat_result_t sat_cdfs_unmount(sat_cdfs_volume_t* volume);
sat_result_t sat_cdfs_lookup(
    sat_cdfs_volume_t* volume,
    const char* path,
    sat_cdfs_file_t* out_file
);
sat_result_t sat_cdfs_read_at(
    sat_cdfs_volume_t* volume,
    const sat_cdfs_file_t* file,
    uint32_t offset,
    void* destination,
    uint32_t bytes,
    uint32_t* out_read
);

/* Adapter for sat_file_register_backend. The source object is caller-owned
 * and must outlive the registered file mount. */
typedef struct sat_cdfs_file_source {
    sat_cdfs_volume_t* volume;
    sat_cdfs_file_t file;
} sat_cdfs_file_source_t;

sat_result_t sat_cdfs_file_read_at(
    void* context,
    uint32_t offset,
    void* destination,
    uint32_t bytes,
    uint32_t* out_read
);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_CDFS_H */
