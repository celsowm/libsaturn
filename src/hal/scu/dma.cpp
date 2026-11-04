#include "src/hal/scu/dma.hpp"
#include "src/hal/scu/irq.hpp"

#include "src/hal/sh2/cache.hpp"
#include "src/hal/sh2/cpu.hpp"
#include "src/hal/sh2/dmac.hpp"

namespace saturn::hal::scu::dma {

namespace {

namespace logic = saturn::hal::scu::dma_logic;

#ifndef SCU_REG
#define SCU_REG(addr) (*reinterpret_cast<volatile uint32_t*>(addr))
#endif

/* Setup plus polling costs more than moving a few longwords by hand. */
constexpr uint32_t kMinDmaBytes = 64u;
constexpr uint32_t kWaitSpins = 4000000u;
constexpr uint32_t kStopSpins = 100000u;

struct Pending {
    volatile bool active;
    uint8_t count;
    volatile sat_result_t result;
    uint32_t dst[logic::kMaxListEntries];
    uint32_t bytes[logic::kMaxListEntries];
};

Pending g_pending[logic::kLevelCount];
/* Indirect tables: read by the SCU from Work RAM-H, so they live in .bss. */
alignas(16) uint32_t g_table[logic::kLevelCount]
                            [logic::kMaxListEntries * logic::kWordsPerEntry];
Stats g_stats;
bool g_enabled = true;
volatile bool g_illegal_seen = false;
volatile uint32_t g_illegal_count = 0u;
constexpr uint8_t kIllegalIrqSource = 12u;

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

/* IST is read-only. A shared error is conservatively attached to every
 * outstanding job, even if the hardware cannot identify its level. */
void record_illegal() {
    if (!g_illegal_seen) g_illegal_count = g_illegal_count + 1u;
    g_illegal_seen = true;
    for (uint8_t i = 0u; i < logic::kLevelCount; ++i) {
        if (g_pending[i].active && g_pending[i].result == SAT_OK) {
            g_pending[i].result = SAT_ERR_IO;
        }
    }
}

bool observe_illegal() {
    const uint32_t status = sh2::save_and_mask_interrupts();
    const bool pending = (SCU_REG(logic::kRegisterIstatus) & logic::kIstIllegal) != 0u;
    if (!pending) {
        g_illegal_seen = false;
    } else {
        record_illegal();
    }
    sh2::restore_interrupts(status);
    return pending;
}

/* Let the SH-2 accept the event; never write IST. If IRQs cannot be used or
 * acknowledgement stalls, leave the condition latched and refuse new jobs. */
bool acknowledge_illegal() {
    const uint32_t before = irq::count(kIllegalIrqSource);
    if ((SCU_REG(logic::kRegisterIstatus) & logic::kIstIllegal) == 0u) {
        g_illegal_seen = false;
        return true;
    }
    if (!irq::retain(kIllegalIrqSource)) return false;
    uint32_t spins = 0u;
    while (irq::count(kIllegalIrqSource) == before && irq::live() && ++spins < kStopSpins) {}
    const bool accepted = irq::count(kIllegalIrqSource) != before;
    irq::release(kIllegalIrqSource);
    const bool cleared = accepted && (SCU_REG(logic::kRegisterIstatus) & logic::kIstIllegal) == 0u;
    if (cleared) g_illegal_seen = false;
    return cleared;
}

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
    if (!p.active) return p.result;
    invalidate_destinations(p);
    if (observe_illegal()) (void)acknowledge_illegal();
    p.active = false;
    if (p.result != SAT_OK) return p.result;
    ++g_stats.scu_transfers;
    return SAT_OK;
}

/* busy() has no error return. Keep a completion error until wait() or the
 * next start reports it, rather than losing it when busy() notices idle. */
sat_result_t take_completion(uint8_t level) {
    const sat_result_t result = finish(level);
    g_pending[level].result = SAT_OK;
    return result;
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
    (void)finish(level);
    return false;
}

sat_result_t start(uint8_t level, const void* src, void* dst, uint32_t bytes) {
    const logic::Transfer t{logic::physical(src),
                            logic::physical(dst), bytes};
    logic::Plan plan{};
    SAT_TRY(from_status(logic::plan_direct(level, t, &plan)));
    if (level_blocked(level)) return SAT_ERR_BUSY;
    SAT_TRY(take_completion(level));
    if (observe_illegal() && !acknowledge_illegal()) return SAT_ERR_IO;
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
    if (level_blocked(level)) return SAT_ERR_BUSY;
    const uint32_t table = logic::physical(g_table[level]);
    if (logic::classify(table) != logic::Region::WorkRamH) return SAT_ERR_UNSUPPORTED;
    SAT_TRY(take_completion(level));
    if (observe_illegal() && !acknowledge_illegal()) return SAT_ERR_IO;
    uint32_t add = 0u;
    SAT_TRY(from_status(
        logic::build_indirect(level, list, count, g_table[level], &add, nullptr)));
    remember(level, list, count);
    /* Indirect mode reads the table address from the write register. */
    launch(level, 0u, table, 0u, add, logic::kModeIndirectImmediate);
    return SAT_OK;
}

sat_result_t wait(uint8_t level) {
    if (level >= logic::kLevelCount) return SAT_ERR_INVALID_ARG;
    for (uint32_t spins = 0u; spins < kWaitSpins; ++spins) {
        if ((status_register() & logic::kStatusBusy[level]) == 0u) return take_completion(level);
    }
    /* DSTP stops all levels, not just the one being waited for. Until each
     * is observed idle, its buffers may still be touched by the hardware. */
    for (uint8_t i = 0u; i < logic::kLevelCount; ++i) {
        if (g_pending[i].active) g_pending[i].result = SAT_ERR_TIMEOUT;
    }
    SCU_REG(logic::kRegisterStop) = 1u;
    for (uint32_t spins = 0u; spins < kStopSpins; ++spins) {
        if ((status_register() & logic::kStatusBusy[level]) == 0u) break;
    }
    SCU_REG(logic::kRegisterStop) = 0u;
    if (observe_illegal()) (void)acknowledge_illegal();
    ++g_stats.timeouts;
    if ((status_register() & logic::kStatusBusy[level]) == 0u) {
        (void)take_completion(level);
    }
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

Stats stats() {
    Stats result = g_stats;
    result.illegal = g_illegal_count;
    return result;
}

extern "C" void sat_scu_dma_illegal_isr() {
    record_illegal();
    /* Each delivered IRQ is a distinct event. A prior IST observation, if
     * any, has already been counted and is deduplicated by record_illegal. */
    g_illegal_seen = false;
}

}  // namespace saturn::hal::scu::dma
