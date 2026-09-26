#ifndef SATURN_ABUS_H
#define SATURN_ABUS_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The A-Bus cartridge slot, one layer below sat_ram_cart_* and sat_save_*.
 *
 * sat_abus_detect() reads only the cartridge ID byte and touches nothing else,
 * so it is safe with any cartridge or none. Raw reads are for expansion RAM and
 * ROM cartridges; writes reach only the DRAM banks of a RAM expansion. A Backup
 * Memory cartridge is never read or written raw: its contents belong to the BIOS
 * backup library (sat_save_*, SAT_SAVE_BACKUP_CARTRIDGE), and stray writes could
 * corrupt saves. Chip select 2 belongs to the CD block and is not offered.
 *
 * Writes are raw: they know nothing about sat_ram_cart_alloc() allocations, so
 * do not mix the two on the same bytes. */

typedef enum sat_abus_kind {
    SAT_ABUS_NONE = 0,            /* nothing in the slot (ID 0xFF) */
    SAT_ABUS_DRAM_1MB = 1,        /* ID 0x5A */
    SAT_ABUS_DRAM_4MB = 2,        /* ID 0x5C */
    SAT_ABUS_BACKUP_MEMORY = 3,   /* ID 0x21-0x24: 4, 8, 16 or 32 Mbit */
    SAT_ABUS_UNKNOWN = 4          /* something else, e.g. a ROM cartridge */
} sat_abus_kind_t;

typedef struct sat_abus_info {
    sat_abus_kind_t kind;
    uint8_t id;                   /* the raw ID byte */
    uint8_t reserved0;
    uint16_t reserved1;
    uint32_t backup_bytes;        /* Backup Memory cartridges: capacity, else 0 */
} sat_abus_info_t;

typedef enum sat_abus_area {
    SAT_ABUS_CS0 = 0,             /* A-Bus 0x02000000-0x03FFFFFF, 32 MiB */
    SAT_ABUS_CS1 = 1              /* A-Bus 0x04000000-0x04FFFFFF, 16 MiB */
} sat_abus_area_t;

/* Reads the ID byte and classifies the cartridge. */
sat_result_t sat_abus_detect(sat_abus_info_t* out_info);

/* Copies bytes out of a chip-select window at `offset`. SAT_ERR_NOT_CONNECTED
 * for an empty slot, SAT_ERR_UNSUPPORTED for a Backup Memory cartridge,
 * SAT_ERR_INVALID_ARG outside the window. */
sat_result_t sat_abus_read(sat_abus_area_t area, uint32_t offset, void* destination, uint32_t bytes);

/* Copies bytes into the DRAM banks of a RAM expansion (CS0 offsets 0x400000
 * and 0x600000, each 512 KiB on the 1 MiB cartridge and 2 MiB on the 4 MiB
 * one; one bank per call). Sets the A-Bus up first if needed. Anything else is
 * SAT_ERR_UNSUPPORTED (wrong cartridge or window) or SAT_ERR_INVALID_ARG. */
sat_result_t sat_abus_write(sat_abus_area_t area, uint32_t offset, const void* source, uint32_t bytes);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_ABUS_H */
