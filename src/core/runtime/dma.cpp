#include "saturn/dma.h"

#include "src/core/runtime/state.hpp"
#include "src/hal/scu/dma.hpp"

namespace {

namespace dma = saturn::hal::scu::dma;
namespace logic = saturn::hal::scu::dma_logic;

static_assert(SAT_DMA_LEVELS == logic::kLevelCount, "level count");
static_assert(SAT_DMA_MAX_LIST == logic::kMaxListEntries, "list length");

}  // namespace

extern "C" sat_result_t sat_dma_copy(void* dst, const void* src, uint32_t bytes) {
    SAT_TRY(saturn::core::require_initialized());
    return dma::copy(dst, src, bytes);
}

extern "C" int sat_dma_scu_capable(const void* dst, const void* src, uint32_t bytes) {
    if (dst == nullptr || src == nullptr || bytes == 0u || (bytes & 3u) != 0u) return 0;
    const uint32_t s = logic::physical(src);
    const uint32_t d = logic::physical(dst);
    if (((s | d) & 3u) != 0u) return 0;
    return logic::route_allowed(logic::classify_span(s, bytes), logic::classify_span(d, bytes))
        ? 1 : 0;
}

extern "C" sat_result_t sat_dma_start(uint8_t level, const void* src, void* dst,
                                      uint32_t bytes) {
    SAT_TRY(saturn::core::require_initialized());
    return dma::start(level, src, dst, bytes);
}

extern "C" sat_result_t sat_dma_start_list(uint8_t level, const sat_dma_transfer_t* transfers,
                                           uint32_t count) {
    if (transfers == nullptr || count == 0u || count > SAT_DMA_MAX_LIST) {
        return SAT_ERR_INVALID_ARG;
    }
    SAT_TRY(saturn::core::require_initialized());
    const void* srcs[SAT_DMA_MAX_LIST];
    void* dsts[SAT_DMA_MAX_LIST];
    uint32_t sizes[SAT_DMA_MAX_LIST];
    for (uint32_t i = 0u; i < count; ++i) {
        srcs[i] = transfers[i].src;
        dsts[i] = transfers[i].dst;
        sizes[i] = transfers[i].bytes;
    }
    return dma::start_list(level, srcs, dsts, sizes, static_cast<uint8_t>(count));
}

extern "C" int sat_dma_busy(uint8_t level) {
    return saturn::core::g_state.initialized && dma::busy(level) ? 1 : 0;
}

extern "C" sat_result_t sat_dma_wait(uint8_t level) {
    SAT_TRY(saturn::core::require_initialized());
    return dma::wait(level);
}

extern "C" void sat_dma_set_enabled(int enabled) {
    dma::set_enabled(enabled != 0);
}

extern "C" void sat_dma_get_stats(sat_dma_stats_t* out_stats) {
    if (out_stats == nullptr) return;
    const dma::Stats s = dma::stats();
    out_stats->scu_transfers = s.scu_transfers;
    out_stats->cpu_copies = s.cpu_copies;
    out_stats->bytes_scu = s.bytes_scu;
    out_stats->bytes_cpu = s.bytes_cpu;
    out_stats->timeouts = s.timeouts;
    out_stats->illegal = s.illegal;
    out_stats->last_path = static_cast<sat_dma_path_t>(s.last_path);
}
