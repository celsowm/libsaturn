#ifndef SATURN_HAL_SH2_DMAC_LOGIC_HPP
#define SATURN_HAL_SH2_DMAC_LOGIC_HPP

#include <stdint.h>

#include "src/hal/scu/dma_logic.hpp"

/* Pure policy for the SH-2's on-chip DMA controller (SH7604 hardware manual,
 * DMAC): channel 0 as a longword memory-to-memory copier for the transfers
 * the SCU cannot do, i.e. Work RAM to Work RAM. No register access, so host
 * tests cover it. */
namespace saturn::hal::sh2::dmac_logic {

constexpr uint32_t kSar0 = 0xFFFFFF80u;
constexpr uint32_t kDar0 = 0xFFFFFF84u;
constexpr uint32_t kTcr0 = 0xFFFFFF88u;
constexpr uint32_t kChcr0 = 0xFFFFFF8Cu;
constexpr uint32_t kDmaor = 0xFFFFFFB0u;

/* CHCR: DM=01 destination +, SM=01 source +, TS=10 longwords, AR auto
 * request, TB burst, DE enable. IE stays 0: completion is polled, so no
 * on-chip interrupt priority is needed. */
constexpr uint32_t kChcrLongCopy = 0x5A11u;
constexpr uint32_t kChcrTe = 0x0002u;
constexpr uint32_t kChcrDe = 0x0001u;
/* DMAOR: DME enables the controller; AE flags an address error. */
constexpr uint32_t kDmaorDme = 0x0001u;
constexpr uint32_t kDmaorAe = 0x0004u;

/* Below this the setup and polling cost more than the CPU loop. */
constexpr uint32_t kMinBytes = 128u;
/* TCR is 24 bits of longwords. */
constexpr uint32_t kMaxBytes = 0x03FFFFFCu;

enum class Status : uint8_t {
    Ok,
    BadSize,     /* under the minimum, over the maximum, or not longwords */
    Misaligned,
    NotRam,      /* an endpoint is not Work RAM */
    Overlap      /* a forward copy would read what it already wrote */
};

inline bool is_work_ram(saturn::hal::scu::dma_logic::Region r) {
    using saturn::hal::scu::dma_logic::Region;
    return r == Region::WorkRamL || r == Region::WorkRamH;
}

/* True when [src, src+bytes) and [dst, dst+bytes) share a byte and dst is
 * above src: the case a forward copy corrupts. */
inline bool forward_overlap(uint32_t src, uint32_t dst, uint32_t bytes) {
    return dst > src && dst < src + bytes;
}

struct Plan {
    uint32_t units; /* TCR: longwords */
    uint32_t chcr;
};

inline Status plan_copy(uint32_t src_phys, uint32_t dst_phys, uint32_t bytes, Plan* out) {
    using namespace saturn::hal::scu::dma_logic;
    if (bytes < kMinBytes || bytes > kMaxBytes || (bytes & 3u) != 0u) return Status::BadSize;
    if (((src_phys | dst_phys) & 3u) != 0u) return Status::Misaligned;
    if (!is_work_ram(classify_span(src_phys, bytes)) ||
        !is_work_ram(classify_span(dst_phys, bytes))) {
        return Status::NotRam;
    }
    if (forward_overlap(src_phys, dst_phys, bytes)) return Status::Overlap;
    out->units = bytes / 4u;
    out->chcr = kChcrLongCopy;
    return Status::Ok;
}

}  // namespace saturn::hal::sh2::dmac_logic

#endif
