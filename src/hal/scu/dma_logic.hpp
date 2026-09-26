#ifndef SATURN_HAL_SCU_DMA_LOGIC_HPP
#define SATURN_HAL_SCU_DMA_LOGIC_HPP

#include <stdint.h>

/* Pure SCU-DMA policy: which transfers the hardware allows, and the register
 * values that start them. No register access, so host tests cover it.
 * Addresses are physical (bits 26..0); callers mask cache-through and cached
 * aliases with physical(). Register semantics: SCU manual 2.1 and 3.2. */
namespace saturn::hal::scu::dma_logic {

constexpr uint8_t kLevelCount = 3u;
/* Bytes one transfer may move: level 0 up to 1 MiB, levels 1 and 2 up to
 * 4 KiB. A count register of 0 means that maximum. */
constexpr uint32_t kMaxBytes[kLevelCount] = {0x100000u, 0x1000u, 0x1000u};
/* Longest indirect list this library builds (three longwords each). */
constexpr uint8_t kMaxListEntries = 16u;
constexpr uint8_t kWordsPerEntry = 3u;

constexpr uint32_t kRegisterBase = 0x25FE0000u;
constexpr uint32_t kLevelStride = 0x20u;
constexpr uint32_t kOffsetRead = 0x00u;
constexpr uint32_t kOffsetWrite = 0x04u;
constexpr uint32_t kOffsetCount = 0x08u;
constexpr uint32_t kOffsetAdd = 0x0Cu;
constexpr uint32_t kOffsetEnable = 0x10u;
constexpr uint32_t kOffsetMode = 0x14u;
constexpr uint32_t kRegisterStop = 0x25FE0060u;
constexpr uint32_t kRegisterStatus = 0x25FE007Cu;
constexpr uint32_t kRegisterIstatus = 0x25FE00A4u;

/* D0EN..D2EN: bit 8 permits DMA, bit 0 starts it when the factor is 111. */
constexpr uint32_t kEnableGo = 0x00000101u;
/* DxMD: bit 24 indirect, bit 16 read update, bit 8 write update, bits 2..0
 * the start factor; 111 = the GO bit. Addresses are not updated (hold). */
constexpr uint32_t kModeDirectImmediate = 0x00000007u;
constexpr uint32_t kModeIndirectImmediate = 0x01000007u;
/* Last table entry: bit 31 of its read address. */
constexpr uint32_t kIndirectEnd = 0x80000000u;

/* DxAD: bit 8 = read +4 (the only legal value off the A-bus CS2 space),
 * bits 2..0 = write increment 1 -> 2 bytes (B-bus), 2 -> 4 bytes. */
constexpr uint32_t kAddToBBus = 0x00000101u;
constexpr uint32_t kAddToWorkRam = 0x00000102u;

/* DSTA busy = operating or waiting, per level. */
constexpr uint32_t kStatusBusy[kLevelCount] = {0x00000030u, 0x00000300u, 0x00003000u};
/* IST bit of the level's DMA-end interrupt, and of "DMA illegal". */
constexpr uint32_t kIstEnd[kLevelCount] = {1u << 11u, 1u << 10u, 1u << 9u};
constexpr uint32_t kIstIllegal = 1u << 12u;

enum class Region : uint8_t {
    Other,
    WorkRamL,
    WorkRamH,
    ABus,
    Scsp,  /* Sound RAM only */
    Vdp1,  /* VRAM and frame buffer */
    Vdp2   /* VRAM and colour RAM */
};

enum class Status : uint8_t {
    Ok,
    BadLevel,
    BadSize,       /* zero, over the level's maximum, or not longword sized */
    Misaligned,    /* an address is not a longword boundary */
    IllegalRoute,  /* the manual forbids this source/destination pair */
    BadList,       /* empty, too long, or entries that disagree on the add value */
    NoTable        /* an indirect table must live in Work RAM-H */
};

/* Strips the cache and cache-through bits: 0x06000000, 0x26000000 and the
 * mirrors up to 0x27FFFFFF are one location. */
inline uint32_t physical(uint32_t address) {
    return address & 0x07FFFFFFu;
}

/* Physical address of a pointer (the host tests build 64-bit). */
inline uint32_t physical(const void* pointer) {
    return physical(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pointer)));
}

inline Region classify(uint32_t phys) {
    if (phys >= 0x00200000u && phys < 0x00300000u) return Region::WorkRamL;
    if (phys >= 0x02000000u && phys < 0x05900000u) return Region::ABus;
    if (phys >= 0x05A00000u && phys < 0x05A80000u) return Region::Scsp;
    if (phys >= 0x05C00000u && phys < 0x05CC0000u) return Region::Vdp1;
    if (phys >= 0x05E00000u && phys < 0x05F80000u) return Region::Vdp2;
    if (phys >= 0x06000000u && phys < 0x08000000u) return Region::WorkRamH;
    return Region::Other;
}

/* First and last byte must fall in the same region. */
inline Region classify_span(uint32_t phys, uint32_t bytes) {
    if (bytes == 0u) return Region::Other;
    const Region first = classify(phys);
    const uint64_t last = static_cast<uint64_t>(phys) + bytes - 1u;
    if (last > 0x07FFFFFFull) return Region::Other;
    return classify(static_cast<uint32_t>(last)) == first ? first : Region::Other;
}

inline bool is_b_bus(Region r) {
    return r == Region::Scsp || r == Region::Vdp1 || r == Region::Vdp2;
}

/* SCU manual 2.1: no writes to the A-bus, nothing from the VDP2 area or
 * Work RAM-L, and only A<->B or Work RAM-H<->A/B pairs. */
inline bool route_allowed(Region src, Region dst) {
    const bool src_ok = src == Region::WorkRamH || src == Region::ABus ||
                        src == Region::Vdp1 || src == Region::Scsp;
    const bool dst_ok = dst == Region::WorkRamH || is_b_bus(dst);
    if (!src_ok || !dst_ok) return false;
    if (src == Region::WorkRamH && dst == Region::WorkRamH) return false;
    if (is_b_bus(src) && is_b_bus(dst)) return false;
    return true;
}

inline uint32_t add_value(Region dst) {
    return is_b_bus(dst) ? kAddToBBus : kAddToWorkRam;
}

/* Register value for a byte count; the maximum is written as 0. */
inline uint32_t encode_count(uint8_t level, uint32_t bytes) {
    return bytes == kMaxBytes[level] ? 0u : bytes;
}

struct Transfer {
    uint32_t src;    /* physical */
    uint32_t dst;    /* physical */
    uint32_t bytes;
};

/* Registers of one direct transfer. */
struct Plan {
    uint32_t read;
    uint32_t write;
    uint32_t count;
    uint32_t add;
    uint32_t mode;
    Region dst_region;
};

inline Status check_transfer(uint8_t level, const Transfer& t, Region* dst_region) {
    if (level >= kLevelCount) return Status::BadLevel;
    if (t.bytes == 0u || t.bytes > kMaxBytes[level] || (t.bytes & 3u) != 0u) {
        return Status::BadSize;
    }
    if (((t.src | t.dst) & 3u) != 0u) return Status::Misaligned;
    const Region s = classify_span(t.src, t.bytes);
    const Region d = classify_span(t.dst, t.bytes);
    if (!route_allowed(s, d)) return Status::IllegalRoute;
    if (dst_region != nullptr) *dst_region = d;
    return Status::Ok;
}

inline Status plan_direct(uint8_t level, const Transfer& t, Plan* out) {
    Region d = Region::Other;
    const Status st = check_transfer(level, t, &d);
    if (st != Status::Ok) return st;
    out->read = t.src;
    out->write = t.dst;
    out->count = encode_count(level, t.bytes);
    out->add = add_value(d);
    out->mode = kModeDirectImmediate;
    out->dst_region = d;
    return Status::Ok;
}

/* Fills `words` (kWordsPerEntry per transfer: count, write, read; the last
 * read address carries the end bit) and returns the shared add value. One
 * DxAD serves the whole list, so every destination must want the same one. */
inline Status build_indirect(uint8_t level, const Transfer* list, uint8_t count,
                             uint32_t* words, uint32_t* add, Region* first_dst) {
    if (level >= kLevelCount) return Status::BadLevel;
    if (list == nullptr || words == nullptr || count == 0u || count > kMaxListEntries) {
        return Status::BadList;
    }
    uint32_t shared_add = 0u;
    for (uint8_t i = 0u; i < count; ++i) {
        Region d = Region::Other;
        const Status st = check_transfer(level, list[i], &d);
        if (st != Status::Ok) return st;
        const uint32_t a = add_value(d);
        if (i == 0u) {
            shared_add = a;
            if (first_dst != nullptr) *first_dst = d;
        } else if (a != shared_add) {
            return Status::BadList;
        }
        uint32_t* w = words + static_cast<uint32_t>(i) * kWordsPerEntry;
        w[0] = encode_count(level, list[i].bytes);
        w[1] = list[i].dst;
        w[2] = list[i].src | (i + 1u == count ? kIndirectEnd : 0u);
    }
    if (add != nullptr) *add = shared_add;
    return Status::Ok;
}

inline uint32_t level_register(uint8_t level, uint32_t offset) {
    return kRegisterBase + static_cast<uint32_t>(level) * kLevelStride + offset;
}

}  // namespace saturn::hal::scu::dma_logic

#endif
