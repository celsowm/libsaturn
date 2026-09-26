#include "src/hal/sh2/dmac.hpp"

#include "src/hal/sh2/cache.hpp"

namespace saturn::hal::sh2::dmac {

namespace {

namespace logic = saturn::hal::sh2::dmac_logic;
namespace scu_logic = saturn::hal::scu::dma_logic;

#define SH2_DMAC_REG(addr) (*reinterpret_cast<volatile uint32_t*>(addr))

/* A 1 MiB copy takes well under a frame; this only bounds a dead channel. */
constexpr uint32_t kWaitSpins = 4000000u;

}  // namespace

bool can_copy(const void* dst, const void* src, uint32_t bytes) {
    logic::Plan plan{};
    return logic::plan_copy(scu_logic::physical(src), scu_logic::physical(dst), bytes,
                            &plan) == logic::Status::Ok;
}

sat_result_t copy(void* dst, const void* src, uint32_t bytes) {
    const uint32_t s = scu_logic::physical(src);
    const uint32_t d = scu_logic::physical(dst);
    logic::Plan plan{};
    if (logic::plan_copy(s, d, bytes, &plan) != logic::Status::Ok) return SAT_ERR_UNSUPPORTED;

    SH2_DMAC_REG(logic::kChcr0) = 0u;
    SH2_DMAC_REG(logic::kSar0) = s;
    SH2_DMAC_REG(logic::kDar0) = d;
    SH2_DMAC_REG(logic::kTcr0) = plan.units;
    SH2_DMAC_REG(logic::kDmaor) = logic::kDmaorDme;
    SH2_DMAC_REG(logic::kChcr0) = plan.chcr;

    sat_result_t result = SAT_ERR_TIMEOUT;
    for (uint32_t spins = 0u; spins < kWaitSpins; ++spins) {
        if ((SH2_DMAC_REG(logic::kChcr0) & logic::kChcrTe) != 0u) {
            result = SAT_OK;
            break;
        }
    }
    if ((SH2_DMAC_REG(logic::kDmaor) & logic::kDmaorAe) != 0u) result = SAT_ERR_IO;
    /* Stops the channel and clears TE / AE for the next transfer. */
    SH2_DMAC_REG(logic::kChcr0) = 0u;
    SH2_DMAC_REG(logic::kDmaor) = logic::kDmaorDme;
    if (result != SAT_OK) return result;

    /* The DMAC wrote behind the cache. Work RAM-H repeats every MiB; the
     * cache indexes its primary copy. */
    const uint32_t cached = scu_logic::classify(d) == scu_logic::Region::WorkRamH
        ? (0x06000000u | (d & 0x000FFFFFu)) : d;
    cache::invalidate_range(cached, bytes);
    return SAT_OK;
}

}  // namespace saturn::hal::sh2::dmac
