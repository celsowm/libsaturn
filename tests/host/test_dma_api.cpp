#include <cassert>
#include <cstdint>
#include <cstdio>

#include "saturn/dma.h"
#include "src/core/runtime/state.hpp"
#include "src/hal/scu/dma.hpp"

namespace {
using saturn::hal::scu::dma::Path;
using saturn::hal::scu::dma::Stats;

bool g_enabled = true;
bool g_busy[3] = {};
uint8_t g_last_level = 0xFFu;
uint8_t g_last_count = 0u;
uint32_t g_last_bytes = 0u;
uint32_t g_copies = 0u;
sat_result_t g_next = SAT_OK;
Stats g_stats{};
}

namespace saturn::hal::scu::dma {
void set_enabled(bool enabled) { g_enabled = enabled; }
bool enabled() { return g_enabled; }
bool busy(uint8_t level) { return level < 3u && g_busy[level]; }
sat_result_t start(uint8_t level, const void*, void*, uint32_t bytes) {
    g_last_level = level;
    g_last_count = 1u;
    g_last_bytes = bytes;
    return g_next;
}
sat_result_t start_list(uint8_t level, const void* const* srcs, void* const* dsts,
                        const uint32_t* bytes, uint8_t count) {
    assert(srcs != nullptr && dsts != nullptr && bytes != nullptr);
    g_last_level = level;
    g_last_count = count;
    g_last_bytes = bytes[0];
    return g_next;
}
sat_result_t wait(uint8_t level) { return level < 3u ? g_next : SAT_ERR_INVALID_ARG; }
sat_result_t copy(void*, const void*, uint32_t bytes) {
    ++g_copies;
    g_last_bytes = bytes;
    return g_next;
}
Stats stats() { return g_stats; }
}

int main() {
    using saturn::core::g_state;
    alignas(4) uint8_t buf[256] = {};

    /* Nothing before sat_init. */
    g_state = {};
    assert(sat_dma_copy(buf, buf, 64u) == SAT_ERR_NOT_INITIALIZED);
    assert(sat_dma_start(0u, buf, buf, 64u) == SAT_ERR_NOT_INITIALIZED);
    assert(sat_dma_wait(0u) == SAT_ERR_NOT_INITIALIZED);
    assert(sat_dma_busy(0u) == 0);

    g_state.initialized = true;
    assert(sat_dma_copy(buf, buf, 64u) == SAT_OK && g_copies == 1u && g_last_bytes == 64u);
    g_next = SAT_ERR_TIMEOUT;
    assert(sat_dma_copy(buf, buf, 64u) == SAT_ERR_TIMEOUT);
    g_next = SAT_OK;

    /* Direct and list starts reach the HAL with the same shape. */
    assert(sat_dma_start(2u, buf, buf, 128u) == SAT_OK);
    assert(g_last_level == 2u && g_last_count == 1u && g_last_bytes == 128u);
    sat_dma_transfer_t list[3] = {{buf, buf, 16u}, {buf, buf, 32u}, {buf, buf, 48u}};
    assert(sat_dma_start_list(1u, list, 3u) == SAT_OK);
    assert(g_last_level == 1u && g_last_count == 3u && g_last_bytes == 16u);
    assert(sat_dma_start_list(1u, nullptr, 3u) == SAT_ERR_INVALID_ARG);
    assert(sat_dma_start_list(1u, list, 0u) == SAT_ERR_INVALID_ARG);
    assert(sat_dma_start_list(1u, list, SAT_DMA_MAX_LIST + 1u) == SAT_ERR_INVALID_ARG);

    g_busy[1] = true;
    assert(sat_dma_busy(1u) == 1 && sat_dma_busy(0u) == 0 && sat_dma_busy(3u) == 0);
    assert(sat_dma_wait(3u) == SAT_ERR_INVALID_ARG);

    /* The route question is answered without the hardware. */
    assert(sat_dma_scu_capable(reinterpret_cast<void*>(0x25C00000u),
                               reinterpret_cast<void*>(0x06010000u), 256u) == 1);
    assert(sat_dma_scu_capable(reinterpret_cast<void*>(0x25C00000u),
                               reinterpret_cast<void*>(0x06010002u), 256u) == 0);
    assert(sat_dma_scu_capable(reinterpret_cast<void*>(0x25E00000u),
                               reinterpret_cast<void*>(0x25E00100u), 256u) == 0);
    assert(sat_dma_scu_capable(reinterpret_cast<void*>(0x06010000u),
                               reinterpret_cast<void*>(0x06020000u), 256u) == 0);
    assert(sat_dma_scu_capable(nullptr, buf, 4u) == 0);
    assert(sat_dma_scu_capable(buf, buf, 0u) == 0);

    /* Statistics and the switch pass through. */
    g_stats.scu_transfers = 7u;
    g_stats.bytes_cpu = 99u;
    g_stats.last_path = Path::Cpu;
    sat_dma_stats_t out{};
    sat_dma_get_stats(&out);
    assert(out.scu_transfers == 7u && out.bytes_cpu == 99u && out.last_path == SAT_DMA_PATH_CPU);
    sat_dma_get_stats(nullptr);
    sat_dma_set_enabled(0);
    assert(!g_enabled);
    sat_dma_set_enabled(1);
    assert(g_enabled);

    std::printf("PASS: test_dma_api.cpp\n");
    return 0;
}
