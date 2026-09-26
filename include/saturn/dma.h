#ifndef SATURN_DMA_H
#define SATURN_DMA_H

#include <stdint.h>
#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SCU DMA (levels 0-2, direct and indirect). Master SH-2 only.
 *
 * What the hardware allows (SCU manual 2.1): Work RAM-H, the A-bus and the
 * B-bus (VDP1, VDP2 VRAM/CRAM, Sound RAM) as endpoints, but never a write to
 * the A-bus, a read from VDP2, any use of Work RAM-L, Work RAM-H to itself, or
 * B-bus to B-bus. Addresses and sizes are longword multiples; a level moves
 * at most 4 KiB (levels 1, 2) or 1 MiB (level 0). Cached, cache-through and
 * mirrored addresses of one location are all accepted.
 *
 * The library uses it for its own uploads (textures, VDP1 command lists) and
 * keeps the CPU as fallback everywhere a copy is not legal or too small to
 * pay for the setup. */

#define SAT_DMA_LEVELS 3u
#define SAT_DMA_MAX_LIST 16u

typedef enum sat_dma_path {
    SAT_DMA_PATH_NONE = 0, /* nothing copied yet */
    SAT_DMA_PATH_SCU = 1,
    SAT_DMA_PATH_CPU = 2
} sat_dma_path_t;

typedef struct sat_dma_stats {
    uint32_t scu_transfers; /* SCU jobs finished (an indirect list counts once) */
    uint32_t cpu_copies;    /* sat_dma_copy calls the CPU served */
    uint32_t bytes_scu;
    uint32_t bytes_cpu;
    uint32_t timeouts;      /* waits that hit the bound and force-stopped DMA */
    uint32_t illegal;       /* the SCU flagged an illegal transfer */
    sat_dma_path_t last_path;
} sat_dma_stats_t;

typedef struct sat_dma_transfer {
    const void* src;
    void* dst;
    uint32_t bytes;
} sat_dma_transfer_t;

/* Copies and waits, on level 0 when the route is legal and worth it (64 bytes
 * or more, longword aligned), else with the CPU; the result is the same
 * either way. Sizes above 1 MiB are split. SAT_ERR_TIMEOUT or SAT_ERR_IO mean
 * a DMA that did not finish: the destination is then undefined. */
sat_result_t sat_dma_copy(void* dst, const void* src, uint32_t bytes);

/* 1 when sat_dma_copy would use the SCU for this copy, else 0. */
int sat_dma_scu_capable(const void* dst, const void* src, uint32_t bytes);

/* Asynchronous, no CPU fallback. Buffers must stay valid until
 * sat_dma_busy() reads 0 or sat_dma_wait() returns. Errors:
 * SAT_ERR_INVALID_ARG (level, size, alignment), SAT_ERR_UNSUPPORTED (a route
 * the manual forbids), SAT_ERR_BUSY (the level runs, or level 2 while level
 * 1 runs). */
sat_result_t sat_dma_start(uint8_t level, const void* src, void* dst, uint32_t bytes);

/* One start, up to SAT_DMA_MAX_LIST transfers (indirect mode). Entries must
 * all write the same kind of destination: B-bus, or Work RAM-H. */
sat_result_t sat_dma_start_list(uint8_t level, const sat_dma_transfer_t* transfers,
                                uint32_t count);

/* 1 while the level is operating or waiting. */
int sat_dma_busy(uint8_t level);

/* Waits for the level; SAT_ERR_TIMEOUT after forcing it to stop. */
sat_result_t sat_dma_wait(uint8_t level);

/* 0 makes sat_dma_copy and the library's own uploads use the CPU only, for
 * comparing the two paths. Default 1. */
void sat_dma_set_enabled(int enabled);

void sat_dma_get_stats(sat_dma_stats_t* out_stats);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_DMA_H */
