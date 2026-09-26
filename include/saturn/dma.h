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
    SAT_DMA_PATH_CPU = 2,
    SAT_DMA_PATH_SH2 = 3  /* sat_dma_copy_sh2 */
} sat_dma_path_t;

typedef struct sat_dma_stats {
    uint32_t scu_transfers; /* SCU jobs finished (an indirect list counts once) */
    uint32_t cpu_copies;    /* sat_dma_copy calls the CPU served */
    uint32_t sh2_copies;    /* sat_dma_copy_sh2 calls that finished */
    uint32_t bytes_scu;
    uint32_t bytes_cpu;
    uint32_t bytes_sh2;
    uint32_t timeouts;      /* waits that hit the bound and force-stopped DMA */
    uint32_t illegal;       /* the SCU flagged an illegal transfer */
    sat_dma_path_t last_path;
} sat_dma_stats_t;

typedef struct sat_dma_transfer {
    const void* src;
    void* dst;
    uint32_t bytes;
} sat_dma_transfer_t;

/* Copies and waits, on SCU level 0 when the route is legal and worth it (64
 * bytes or more, longword aligned), else with the CPU; the result is the same
 * either way. Sizes above 1 MiB are split. Source and destination must not
 * overlap (memcpy rules). SAT_ERR_TIMEOUT or SAT_ERR_IO mean a DMA that did
 * not finish: the destination is then undefined. */
sat_result_t sat_dma_copy(void* dst, const void* src, uint32_t bytes);

/* Work RAM (either half) to Work RAM through this SH-2's own DMAC, channel 0:
 * the one route the SCU refuses. 128 bytes or more, longword aligned, and not
 * overlapping upward (moving down inside a buffer is fine). Waits, and
 * invalidates the destination in the cache. Opt-in only: it bypasses the
 * cache and the CPU stays stalled, and on Mednafen it copies slower than the
 * plain CPU loop (about 52 ms against 38 ms for 512 KiB), so sat_dma_copy
 * never picks it. SAT_ERR_UNSUPPORTED when the copy does not fit those rules,
 * SAT_ERR_TIMEOUT / SAT_ERR_IO when the channel failed. */
sat_result_t sat_dma_copy_sh2(void* dst, const void* src, uint32_t bytes);

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

/* Sends each frame's VDP1 command table and Gouraud tables to VRAM by SCU-DMA
 * (sat_end_frame) instead of CPU stores. Default 0: the result is the same
 * either way, and the speed depends on the machine. Measured with a
 * 400-command table: Mednafen copies it in 0.30 ms by DMA against 1.45 ms by
 * CPU, but Ymir with both SH-2s busy (dino_demo, ~760 commands per frame) takes
 * 667 FRT ticks by DMA against 380 by CPU per submit and drops from 20.0 to
 * 18.5 fps, because the DMA shares Work RAM-H with the Slave. Turn it on for
 * single-CPU programs with big command tables, and measure. */
void sat_dma_set_command_list(int enabled);

/* 0 makes sat_dma_copy and the library's own uploads use the CPU only, for
 * comparing the two paths. Default 1. */
void sat_dma_set_enabled(int enabled);

void sat_dma_get_stats(sat_dma_stats_t* out_stats);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_DMA_H */
