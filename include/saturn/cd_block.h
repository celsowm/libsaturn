#ifndef SATURN_CD_BLOCK_H
#define SATURN_CD_BLOCK_H

#include <stdint.h>

#include "saturn/cd.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Hardware-specific SH-2 CD Block communication registers. The BIOS is still
 * responsible for disc authentication; this layer starts from an available
 * CD Block and reads 2048-byte user-data sectors. */
#define SAT_CD_BLOCK_BASE 0x25890000u
#define SAT_CD_BLOCK_DTR (SAT_CD_BLOCK_BASE + 0x00u)
#define SAT_CD_BLOCK_HIRQ (SAT_CD_BLOCK_BASE + 0x08u)
#define SAT_CD_BLOCK_HIRQ_MASK (SAT_CD_BLOCK_BASE + 0x0Cu)
#define SAT_CD_BLOCK_CR1 (SAT_CD_BLOCK_BASE + 0x18u)
#define SAT_CD_BLOCK_CR2 (SAT_CD_BLOCK_BASE + 0x1Cu)
#define SAT_CD_BLOCK_CR3 (SAT_CD_BLOCK_BASE + 0x20u)
#define SAT_CD_BLOCK_CR4 (SAT_CD_BLOCK_BASE + 0x24u)

#define SAT_CD_BLOCK_HIRQ_CMOK 0x0001u
#define SAT_CD_BLOCK_HIRQ_DRDY 0x0002u
#define SAT_CD_BLOCK_HIRQ_ESEL 0x0040u
#define SAT_CD_BLOCK_HIRQ_PEND 0x0010u
#define SAT_CD_BLOCK_HIRQ_CSCT 0x0008u
#define SAT_CD_BLOCK_HIRQ_EFLS 0x0200u

#define SAT_CD_BLOCK_DEFAULT_TIMEOUT 0x240000u

typedef struct sat_cd_block {
    uint32_t timeout_iterations;
    uint8_t initialized;
    uint8_t reserved[3];
} sat_cd_block_t;

/* Initializes the CD Block command/filter state. This does not perform disc
 * authentication and must run after the platform/BIOS has made the block
 * accessible. */
sat_result_t sat_cd_block_init(sat_cd_block_t* out_block, uint32_t timeout_iterations);

/* Reads ISO user-data sectors by LBA. The transport converts LBA to the CD
 * Block's FAD address and transfers exactly sector_count * 2048 bytes. */
sat_result_t sat_cd_block_read_sectors(
    sat_cd_block_t* block,
    uint32_t lba,
    uint32_t sector_count,
    void* destination
);

/* Binds the initialized hardware reader to the generic CDFS sector boundary. */
sat_result_t sat_cd_block_bind_device(
    sat_cd_block_t* block,
    sat_cd_device_t* out_device,
    uint32_t sector_count
);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_CD_BLOCK_H */
