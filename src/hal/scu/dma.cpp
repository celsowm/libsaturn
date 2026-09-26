#include "src/hal/scu/dma.hpp"

#include "src/hal/sh2/cache.hpp"
#include "src/hal/sh2/dmac.hpp"

namespace saturn::hal::scu::dma {

namespace {

namespace logic = saturn::hal::scu::dma_logic;

#define SCU_REG(addr) (*reinterpret_cast<volatile uint32_t*>(addr))

/* Setup plus polling costs more than moving a few longwords by hand. */
constexpr uint32_t kMinDmaBytes = 64u;
constexpr uint32_t kWaitSpins = 4000000u;
constexpr uint32_t kStopSpins = 100000u;

struct Pending {
    bool active;
    uint8_t count;
    uint32_t dst[logic::kMaxListEntries];
    uint32_t bytes[logic::kMaxListEntries];
};

Pending g_pending[logic::kLevelCount];
/* Indirect tables: read by the SCU from Work RAM-H, so they live in .bss. */
alignas(16) uint32_t g_table[logic::kLevelCount]
                            [logic::kMaxListEntries * logic::kWordsPerEntry];
Stats g_stats;
bool g_enabled = true;

sat_result_t from_status(logic::Status st) {
    switch (st) {
        case logic::Status::Ok: return SAT_OK;
        case logic::Status::IllegalRoute:
        case logic::Status::NoTable: return SAT_ERR_UNSUPPORTED;
        default: return SAT_ERR_INVALID_ARG;
    }
}

inline uint32_t status_register() { return SCU_REG(logic::kRegisterStatus); }

/* Level 2 must not start while level 1 runs (SCU manual 2.1). */
bool level_blocked(uint8_t level) {
    const uint32_t status = status_register();
    if ((status & logic::kStatusBusy[level]) != 0u) return true;
    return level == 2u && (status & logic::kStatusBusy[1]) != 0u;
}

/* Writing 0 clears an IST bit; ones leave the others alone. */
inline void clear_ist(uint32_t bits) { SCU_REG(logic::kRegisterIstatus) = ~bits; }

void invalidate_destinations(const Pending& p) {
    for (uint8_t i = 0u; i < p.count; ++i) {
        if (logic::classify(p.dst[i]) != logic::Region::WorkRamH) continue;
        /* The SCU wrote behind the cache: drop what it still holds. */
        sh2::cache::invalidate_range(0x06000000u | (p.dst[i] & 0x000FFFFFu), p.bytes[i]);
    }
}

/* Called once a level reads idle. */
sat_result_t finish(uint8_t level) {
    Pending& p = g_pending[level];
    if (!p.active) return SAT_OK;
    p.active = false;
    invalidate_destinations(p);
    const bool illegal = (SCU_REG(logic::kRegisterIstatus) & logic::kIstIllegal) != 0u;
    clear_ist(logic::kIstEnd[level] | logic::kIstIllegal);
    if (illegal) {
        ++g_stats.illegal;
        return SAT_ERR_IO;
    }
    ++g_stats.scu_transfers;
    return SAT_OK;
}

void remember(uint8_t level, const logic::Transfer* list, uint8_t count) {
    Pending& p = g_pending[level];
    p.active = true;
    p.count = count;
    for (uint8_t i = 0u; i < count; ++i) {
        p.dst[i] = list[i].dst;
        p.bytes[i] = list[i].bytes;
    }
}

void launch(uint8_t level, uint32_t read, uint32_t write, uint32_t count, uint32_t add,
            uint32_t mode) {
    SCU_REG(logic::level_register(level, logic::kOffsetRead)) = read;
    SCU_REG(logic::level_register(level, logic::kOffsetWrite)) = write;
    SCU_REG(logic::level_register(level, logic::kOffsetCount)) = count;
    SCU_REG(logic::level_register(level, logic::kOffsetAdd)) = add;
    SCU_REG(logic::level_register(level, logic::kOffsetMode)) = mode;
    SCU_REG(logic::level_register(level, logic::kOffsetEnable)) = logic::kEnableGo;
}

void cpu_copy(void* dst, const void* src, uint32_t bytes) {
    const uint32_t d = logic::physical(dst);
    const logic::Region region = logic::classify_span(d, bytes);
    const uint32_t both = logic::physical(dst) | logic::physical(src) | bytes;
    if (region == logic::Region::WorkRamH && (both & 3u) == 0u) {
        uint32_t* out = static_cast<uint32_t*>(dst);
        const uint32_t* in = static_cast<const uint32_t*>(src);
        for (uint32_t i = 0u; i < bytes / 4u; ++i) out[i] = in[i];
    } else if (((d | bytes) & 1u) == 0u && (logic::physical(src) & 1u) == 0u) {
        volatile uint16_t* out = static_cast<volatile uint16_t*>(dst);
        const uint16_t* in = static_cast<const uint16_t*>(src);
        for (uint32_t i = 0u; i < bytes / 2u; ++i) out[i] = in[i];
    } else if (((d | bytes) & 1u) == 0u) {
        /* Halfword destination (VDP2 and colour RAM refuse byte writes),
         * odd source: assemble each word from two bytes, big-endian. */
        volatile uint16_t* out = static_cast<volatile uint16_t*>(dst);
        const uint8_t* in = static_cast<const uint8_t*>(src);
        for (uint32_t i = 0u; i < bytes / 2u; ++i) {
            out[i] = static_cast<uint16_t>((static_cast<uint16_t>(in[i * 2u]) << 8u) |
                                           in[i * 2u + 1u]);
        }
    } else {
        volatile uint8_t* out = static_cast<volatile uint8_t*>(dst);
        const uint8_t* in = static_cast<const uint8_t*>(src);
        for (uint32_t i = 0u; i < bytes; ++i) out[i] = in[i];
    }
}

}  // namespace

void set_enabled(bool enabled) { g_enabled = enabled; }
bool enabled() { return g_enabled; }

bool busy(uint8_t level) {
    if (level >= logic::kLevelCount) return false;
    if ((status_register() & logic::kStatusBusy[level]) != 0u) return true;
    finish(level);
    return false;
}

sat_result_t start(uint8_t level, const void* src, void* dst, uint32_t bytes) {
    const logic::Transfer t{logic::physical(src),
                            logic::physical(dst), bytes};
    logic::Plan plan{};
    SAT_TRY(from_status(logic::plan_direct(level, t, &plan)));
    if (level_blocked(level)) return SAT_ERR_BUSY;
    finish(level);
    remember(level, &t, 1u);
    launch(level, plan.read, plan.write, plan.count, plan.add, plan.mode);
    return SAT_OK;
}

sat_result_t start_list(uint8_t level, const void* const* srcs, void* const* dsts,
                        const uint32_t* bytes, uint8_t count) {
    if (level >= logic::kLevelCount) return SAT_ERR_INVALID_ARG;
    if (srcs == nullptr || dsts == nullptr || bytes == nullptr || count == 0u ||
        count > logic::kMaxListEntries) {
        return SAT_ERR_INVALID_ARG;
    }
    logic::Transfer list[logic::kMaxListEntries];
    for (uint8_t i = 0u; i < count; ++i) {
        list[i] = {logic::physical(srcs[i]),
                   logic::physical(dsts[i]), bytes[i]};
    }
    const uint32_t table = logic::physical(g_table[level]);
    if (logic::classify(table) != logic::Region::WorkRamH) return SAT_ERR_UNSUPPORTED;
    uint32_t add = 0u;
    SAT_TRY(from_status(
        logic::build_indirect(level, list, count, g_table[level], &add, nullptr)));
    if (level_blocked(level)) return SAT_ERR_BUSY;
    finish(level);
    remember(level, list, count);
    /* Indirect mode reads the table address from the write register. */
    launch(level, 0u, table, 0u, add, logic::kModeIndirectImmediate);
    return SAT_OK;
}

sat_result_t wait(uint8_t level) {
    if (level >= logic::kLevelCount) return SAT_ERR_INVALID_ARG;
    for (uint32_t spins = 0u; spins < kWaitSpins; ++spins) {
        if ((status_register() & logic::kStatusBusy[level]) == 0u) return finish(level);
    }
    /* Force-stop so the level cannot keep the bus, then report it. */
    SCU_REG(logic::kRegisterStop) = 1u;
    for (uint32_t spins = 0u; spins < kStopSpins; ++spins) {
        if ((status_register() & logic::kStatusBusy[level]) == 0u) break;
    }
    SCU_REG(logic::kRegisterStop) = 0u;
    g_pending[level].active = false;
    clear_ist(logic::kIstEnd[level] | logic::kIstIllegal);
    ++g_stats.timeouts;
    return SAT_ERR_TIMEOUT;
}

sat_result_t copy(void* dst, const void* src, uint32_t bytes) {
    if (bytes == 0u) return SAT_OK;
    if (dst == nullptr || src == nullptr) return SAT_ERR_INVALID_ARG;
    /* Only the route and alignment decide: the level's size limit is met by
     * chunking, and the whole span must stay in one region per side. */
    const uint32_t s = logic::physical(src);
    const uint32_t d = logic::physical(dst);
    const bool routable = bytes >= kMinDmaBytes && ((s | d | bytes) & 3u) == 0u &&
        logic::route_allowed(logic::classify_span(s, bytes), logic::classify_span(d, bytes));
    if (!g_enabled || !routable || level_blocked(0u)) {
        cpu_copy(dst, src, bytes);
        ++g_stats.cpu_copies;
        g_stats.bytes_cpu += bytes;
        g_stats.last_path = Path::Cpu;
        return SAT_OK;
    }
    uint32_t done = 0u;
    while (done < bytes) {
        uint32_t chunk = bytes - done;
        if (chunk > logic::kMaxBytes[0]) chunk = logic::kMaxBytes[0];
        SAT_TRY(start(0u, static_cast<const uint8_t*>(src) + done,
                      static_cast<uint8_t*>(dst) + done, chunk));
        SAT_TRY(wait(0u));
        done += chunk;
    }
    g_stats.bytes_scu += bytes;
    g_stats.last_path = Path::Scu;
    return SAT_OK;
}

sat_result_t copy_sh2(void* dst, const void* src, uint32_t bytes) {
    if (bytes == 0u) return SAT_OK;
    if (dst == nullptr || src == nullptr) return SAT_ERR_INVALID_ARG;
    if (!sh2::dmac::can_copy(dst, src, bytes)) return SAT_ERR_UNSUPPORTED;
    SAT_TRY(sh2::dmac::copy(dst, src, bytes));
    ++g_stats.sh2_copies;
    g_stats.bytes_sh2 += bytes;
    g_stats.last_path = Path::Sh2;
    return SAT_OK;
}

Stats stats() { return g_stats; }

}  // namespace saturn::hal::scu::dma
