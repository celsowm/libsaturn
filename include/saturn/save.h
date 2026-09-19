#ifndef SATURN_SAVE_H
#define SATURN_SAVE_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SAT_SAVE_NAME_MAX 11u
#define SAT_SAVE_COMMENT_MAX 10u

typedef enum sat_save_device {
    SAT_SAVE_INTERNAL = 0,
    /* Reserved for the persistent Backup Memory cartridge. The initial
     * implementation intentionally returns SAT_ERR_UNSUPPORTED for it until
     * device detection and cartridge acceptance are proven. */
    SAT_SAVE_BACKUP_CARTRIDGE = 1
} sat_save_device_t;

typedef enum sat_save_language {
    SAT_SAVE_JAPANESE = 0,
    SAT_SAVE_ENGLISH = 1,
    SAT_SAVE_FRENCH = 2,
    SAT_SAVE_GERMAN = 3,
    SAT_SAVE_SPANISH = 4,
    SAT_SAVE_ITALIAN = 5
} sat_save_language_t;

typedef struct sat_save_record {
    const char* name;
    const char* comment;
    sat_save_language_t language;
    /* Raw BUP timestamp. Zero is allowed when the caller does not yet have an
     * RTC-derived timestamp. */
    uint32_t date;
} sat_save_record_t;

typedef struct sat_save_entry {
    char name[SAT_SAVE_NAME_MAX + 1u];
    char comment[SAT_SAVE_COMMENT_MAX + 1u];
    sat_save_language_t language;
    uint32_t date;
    uint32_t data_size;
    uint16_t block_size;
    uint16_t reserved;
} sat_save_entry_t;

typedef struct sat_save_storage_info {
    uint32_t total_size;
    uint32_t total_blocks;
    uint32_t block_size;
    uint32_t free_size;
    uint32_t free_blocks;
    /* Number of records of prospective_data_size that the BIOS reports can
     * still fit. This is not the current directory-entry count. */
    uint32_t fit_count;
} sat_save_storage_info_t;

/* Initializes the Boot ROM BUP service. Never formats storage implicitly. */
sat_result_t sat_save_init(void);

/* Query storage capacity. prospective_data_size may be zero. */
sat_result_t sat_save_storage_info(
    sat_save_device_t device,
    uint32_t prospective_data_size,
    sat_save_storage_info_t* out_info);

/* List matching records. A null/empty pattern means "*".
 * out_total receives the total number of matches even when capacity is
 * smaller; entries contains min(*out_total, capacity) records. */
sat_result_t sat_save_list(
    sat_save_device_t device,
    const char* pattern,
    sat_save_entry_t* entries,
    uint16_t capacity,
    uint16_t* out_total);

sat_result_t sat_save_read(
    sat_save_device_t device,
    const char* name,
    void* destination,
    uint32_t capacity,
    uint32_t* out_size);

sat_result_t sat_save_write(
    sat_save_device_t device,
    const sat_save_record_t* record,
    const void* data,
    uint32_t data_size,
    uint8_t overwrite);

sat_result_t sat_save_verify(
    sat_save_device_t device,
    const char* name,
    const void* data,
    uint32_t data_size);

sat_result_t sat_save_delete(
    sat_save_device_t device,
    const char* name);

/* Destructive. The caller must request formatting explicitly. */
sat_result_t sat_save_format(sat_save_device_t device);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_SAVE_H */
