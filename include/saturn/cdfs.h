#ifndef SATURN_CDFS_H
#define SATURN_CDFS_H

#include <stdint.h>

#include "saturn/cd.h"
#include "saturn/file.h"

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

/* Where `bytes` of `file` from `offset` lie on the disc, for a reader that
 * wants sectors rather than bytes (sat_cd_stream_begin): the first sector's
 * LBA and the sector count, rounded up. `offset` must be a sector multiple
 * (SAT_ERR_INVALID_ARG otherwise, or when the range leaves the file;
 * bytes == 0 means to the end of the file). The destination needs room for
 * whole sectors. Reads nothing: look the file up first, since a lookup
 * reads the disc and would finish a stream in flight. */
sat_result_t sat_cdfs_file_sectors(
    const sat_cdfs_file_t* file,
    uint32_t offset,
    uint32_t bytes,
    uint32_t* out_lba,
    uint32_t* out_sectors
);

/* Adapter for sat_file_register_backend. The source object is caller-owned
 * and must outlive the registered file mount. */
typedef struct sat_cdfs_file_source {
    sat_cdfs_volume_t* volume;
    sat_cdfs_file_t file;
} sat_cdfs_file_source_t;

typedef struct sat_cdfs_source_desc {
    const char* disc_path;
    const char* source_path;
    uint32_t expected_size;
} sat_cdfs_source_desc_t;

/* Resolves every disc path first, then registers the validated source set as
 * bounded VFS backends. No heap or implicit asset registration is involved. */
sat_result_t sat_cdfs_register_source_manifest(
    sat_cdfs_volume_t* volume,
    const sat_cdfs_source_desc_t* descs, uint16_t count,
    sat_cdfs_file_source_t* out_sources);

/* The disc file behind a VFS path registered with sat_cdfs_register_source_manifest
 * (or sat_file_register_backend with sat_cdfs_file_read_at). SAT_ERR_NOT_FOUND for
 * an unknown path, SAT_ERR_UNSUPPORTED when the path is backed by something else
 * (a blob, another reader). The file stays valid as long as the mount does. */
sat_result_t sat_cdfs_source_file(const char* source_path, const sat_cdfs_file_t** out_file);

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
