#ifndef SATURN_FILE_H
#define SATURN_FILE_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SAT_FILE_PATH_MAX 96u
#define SAT_FILE_MOUNT_CAPACITY 32u
#define SAT_FILE_HANDLE_CAPACITY 16u

typedef struct sat_file {
    uint16_t slot;
    uint16_t generation;
} sat_file_t;

typedef enum sat_file_seek_origin {
    SAT_FILE_SEEK_SET = 0,
    SAT_FILE_SEEK_CURRENT = 1,
    SAT_FILE_SEEK_END = 2
} sat_file_seek_origin_t;

/* Storage backends implement bounded reads at a logical byte offset. The
 * callback may return fewer bytes than requested; returning SAT_ERR_IO leaves
 * the file position unchanged. `context` and its backing storage remain
 * caller-owned for the lifetime of the mount. */
typedef sat_result_t (*sat_file_read_at_fn)(
    void* context,
    uint32_t offset,
    void* destination,
    uint32_t bytes,
    uint32_t* out_read
);

/* The initial VFS is read-only and caller-backed. Paths use forward slashes,
 * are case-sensitive, and reject `..` components. The registered data and
 * path storage remain owned by LibSaturn's fixed tables / caller until reset. */
sat_result_t sat_file_reset(void);
sat_result_t sat_file_register_blob(
    const char* path,
    const void* data,
    uint32_t size
);
sat_result_t sat_file_register_backend(
    const char* path,
    uint32_t size,
    sat_file_read_at_fn read_at,
    void* context
);
sat_result_t sat_file_open(const char* path, sat_file_t* out_file);
sat_result_t sat_file_read(
    sat_file_t file,
    void* destination,
    uint32_t bytes,
    uint32_t* out_read
);
sat_result_t sat_file_seek(
    sat_file_t file,
    int32_t offset,
    sat_file_seek_origin_t origin
);
sat_result_t sat_file_tell(sat_file_t file, uint32_t* out_position);
sat_result_t sat_file_size(sat_file_t file, uint32_t* out_size);
sat_result_t sat_file_close(sat_file_t file);
uint16_t sat_file_mount_count(void);
uint16_t sat_file_handle_capacity(void);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_FILE_H */
