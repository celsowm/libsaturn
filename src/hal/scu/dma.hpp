#ifndef SATURN_HAL_SCU_DMA_HPP
#define SATURN_HAL_SCU_DMA_HPP

#include <stdint.h>

#include "saturn/core.h"
#include "src/hal/scu/dma_logic.hpp"

namespace saturn::hal::scu::dma {

enum class Path : uint8_t { None, Scu, Cpu, Sh2 };

struct Stats {
    uint32_t scu_transfers;  /* completed SCU-DMA jobs (a list counts once) */
    uint32_t cpu_copies;     /* copy() calls served by the CPU */
    uint32_t sh2_copies;     /* copy() calls served by the SH-2 DMAC */
    uint32_t bytes_scu;
    uint32_t bytes_cpu;
    uint32_t bytes_sh2;
    uint32_t timeouts;       /* waits that hit the bound; DMA was force-stopped */
    uint32_t illegal;        /* DMA-illegal interrupt status seen */
    Path last_path;
};

/* Off makes copy() use the CPU only (for A/B comparison); start() and
 * start_list() still drive the hardware. */
void set_enabled(bool enabled);
bool enabled();

/* True while the level is operating or waiting. Once it reads false the
 * level's job is finished: destination Work RAM is invalidated in the cache
 * and the DMA-end status bit cleared. */
bool busy(uint8_t level);

/* Starts one transfer or one indirect list (entries share a destination kind)
 * without waiting. Addresses are CPU addresses in any cache alias; buffers
 * must stay valid until wait() or busy() reports the end. SAT_ERR_BUSY while
 * the level runs (or level 2 while level 1 runs), SAT_ERR_UNSUPPORTED for a
 * route the manual forbids, SAT_ERR_INVALID_ARG for sizes, alignment or list
 * shape. */
sat_result_t start(uint8_t level, const void* src, void* dst, uint32_t bytes);
sat_result_t start_list(uint8_t level, const void* const* srcs, void* const* dsts,
                        const uint32_t* bytes, uint8_t count);

/* Polls until the level ends. SAT_ERR_TIMEOUT after forcing DMA to stop,
 * SAT_ERR_IO when the SCU flagged an illegal transfer. */
sat_result_t wait(uint8_t level);

/* Copies with SCU-DMA level 0 when the route is legal and the copy is worth
 * it, else with the CPU, and waits. Same result on either path; only
 * SAT_ERR_TIMEOUT / SAT_ERR_IO report a DMA that failed (the destination is
 * then undefined). */
sat_result_t copy(void* dst, const void* src, uint32_t bytes);

/* Work RAM to Work RAM through the on-chip DMAC (channel 0), waiting for it.
 * Never chosen by copy(): on Mednafen it is slower than the CPU loop. */
sat_result_t copy_sh2(void* dst, const void* src, uint32_t bytes);

Stats stats();

}  // namespace saturn::hal::scu::dma

#endif
