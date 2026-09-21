#ifndef SATURN_SAVE_SCHEMA_H
#define SATURN_SAVE_SCHEMA_H

#include <stdint.h>
#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Stable wire prefix for a typed save payload. The caller owns the enclosing
 * byte buffer and remains responsible for endian-aware field serialization. */
typedef struct sat_save_schema_header {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint32_t payload_size;
    uint32_t checksum;
} sat_save_schema_header_t;

sat_result_t sat_save_schema_requirements(uint32_t payload_size,
                                          uint32_t* out_wire_size);
sat_result_t sat_save_schema_encode(sat_save_schema_header_t* out,
                                    uint32_t magic, uint16_t version,
                                    const void* payload, uint32_t payload_size);
sat_result_t sat_save_schema_validate(const sat_save_schema_header_t* header,
                                      uint32_t expected_magic,
                                      uint16_t expected_version,
                                      const void* payload,
                                      uint32_t payload_capacity,
                                      uint32_t* out_payload_size);
uint32_t sat_save_schema_checksum(const void* payload, uint32_t payload_size);

#ifdef __cplusplus
}
#endif
#endif
