#ifndef SATURN_RAM_CART_H
#define SATURN_RAM_CART_H

#include <stddef.h>
#include <stdint.h>
#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Volatile expansion DRAM, NOT the persistent Backup Memory cartridge.
 * Physical banks are separate, even when their sizes total 4 MiB. */
typedef enum sat_ram_cart_type {
    SAT_RAM_CART_NONE = 0,
    SAT_RAM_CART_1MB = 1,
    SAT_RAM_CART_4MB = 4
} sat_ram_cart_type_t;

typedef struct sat_ram_cart_info {
    sat_ram_cart_type_t type;
    uint32_t capacity;
    uint32_t free_bytes;
    uint8_t bank_count;
} sat_ram_cart_info_t;

typedef struct sat_ram_cart_buffer {
    uint8_t* bank[2];
    uint32_t bank_bytes[2];
    uint32_t size;
    uint32_t generation;
} sat_ram_cart_buffer_t;

sat_result_t sat_ram_cart_init(void);
sat_result_t sat_ram_cart_info(sat_ram_cart_info_t* out_info);

/* Single-bank allocation: never silently spans the physical bank gap. */
void* sat_ram_cart_alloc(size_t size, size_t align);

/* Transactional allocation of a logical buffer across up to two banks. */
sat_result_t sat_ram_cart_buffer_alloc(
    uint32_t size, size_t align, sat_ram_cart_buffer_t* out_buffer);
sat_result_t sat_ram_cart_buffer_write_at(
    const sat_ram_cart_buffer_t* buffer, uint32_t offset,
    const void* source, uint32_t bytes);
sat_result_t sat_ram_cart_buffer_read_at(
    const sat_ram_cart_buffer_t* buffer, uint32_t offset,
    void* destination, uint32_t bytes);

/* VFS adapter. The context is a live sat_ram_cart_buffer_t* and must outlive
 * the mount. See sat_file_register_backend in saturn/file.h. */
sat_result_t sat_ram_cart_file_read_at(
    void* context, uint32_t offset, void* destination,
    uint32_t bytes, uint32_t* out_read);

/* Invalidates previous pointers and logical buffers. Unmount cache/VFS first. */
void sat_ram_cart_reset(void);

#ifdef __cplusplus
}
#endif
#endif /* SATURN_RAM_CART_H */
