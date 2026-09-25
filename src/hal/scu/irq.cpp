#include "src/hal/scu/irq.hpp"

#include "src/hal/scu/irq_logic.hpp"
#include "src/hal/scu/scu.hpp"
#include "src/hal/sh2/cpu.hpp"
#include "src/hal/sh2/interrupt.hpp"

namespace saturn::hal::scu::irq {

namespace {

namespace logic = saturn::hal::scu::irq_logic;

#define SCU_T0C (*reinterpret_cast<volatile uint32_t*>(0x25FE0090u))
#define SCU_T1S (*reinterpret_cast<volatile uint32_t*>(0x25FE0094u))
#define SCU_T1MD (*reinterpret_cast<volatile uint32_t*>(0x25FE0098u))
#define SCU_IMS (*reinterpret_cast<volatile uint32_t*>(0x25FE00A0u))
/* SH-2 interrupt control: VECMD (bit 0) takes IRL vectors from the SCU
 * (0x40 + source) instead of auto-vectors. */
#define SH2_ICR (*reinterpret_cast<volatile uint16_t*>(0xFFFFFEE0u))
/* On-chip interrupt priorities: IPRA = DIVU, DMAC, WDT; IPRB = SCI, FRT.
 * Level 0 is never taken, whatever the SR mask. */
#define SH2_IPRA (*reinterpret_cast<volatile uint16_t*>(0xFFFFFEE2u))
#define SH2_IPRB (*reinterpret_cast<volatile uint16_t*>(0xFFFFFE60u))
constexpr uint16_t kIcrVecmd = 0x0001u;
constexpr uint32_t kStartSpins = 4000000u;

Handler g_handler[logic::kSourceCount];
void* g_user[logic::kSourceCount];
volatile uint32_t g_count[logic::kSourceCount];
sh2::interrupt::Handler g_previous[logic::kSourceCount];
/* IMS is write-only: this shadow is what every handler restores on exit. */
volatile uint32_t g_ims = logic::kImsAllMasked;
uint16_t g_enabled = 0u;
uint8_t g_previous_sr_mask = 0xFu;
uint16_t g_previous_ipra = 0u;
uint16_t g_previous_iprb = 0u;
bool g_installed = false;
bool g_live = false;
bool g_timer0_line_only = false;

/* Runs at the source's own level, so only higher levels can nest. */
inline void dispatch(uint8_t source) {
    g_count[source] = g_count[source] + 1u;
    if (source == logic::kVblankIn) on_vblank_in();
    const Handler handler = g_handler[source];
    if (handler != nullptr) handler(g_user[source]);
    /* Taking an SCU interrupt in vector mode can leave IMS fully masked
     * (Ymir models it; the BIOS dispatcher rewrites IMS on every exit for
     * the same reason). Writing the shadow is correct either way. */
    SCU_IMS = g_ims;
}

#define SAT_SCU_ISR(n) \
    extern "C" __attribute__((interrupt_handler)) void sat_scu_isr_##n() { dispatch(n); }
SAT_SCU_ISR(0) SAT_SCU_ISR(1) SAT_SCU_ISR(2) SAT_SCU_ISR(3) SAT_SCU_ISR(4)
SAT_SCU_ISR(5) SAT_SCU_ISR(6) SAT_SCU_ISR(7) SAT_SCU_ISR(8) SAT_SCU_ISR(9)
SAT_SCU_ISR(10) SAT_SCU_ISR(11) SAT_SCU_ISR(12) SAT_SCU_ISR(13)
#undef SAT_SCU_ISR

constexpr sh2::interrupt::Handler kIsr[logic::kSourceCount] = {
    sat_scu_isr_0, sat_scu_isr_1, sat_scu_isr_2, sat_scu_isr_3, sat_scu_isr_4,
    sat_scu_isr_5, sat_scu_isr_6, sat_scu_isr_7, sat_scu_isr_8, sat_scu_isr_9,
    sat_scu_isr_10, sat_scu_isr_11, sat_scu_isr_12, sat_scu_isr_13};

/* Applies g_enabled: IMS, timer enable and the SR mask, with every level
 * masked while the three change together. */
void apply(uint32_t status) {
    g_ims = logic::compose_ims(g_enabled);
    SCU_T1MD = logic::compose_t1md(logic::timers_needed(g_enabled), g_timer0_line_only);
    SCU_IMS = g_ims;
    const uint8_t mask = g_installed ? logic::sr_mask(g_enabled) : g_previous_sr_mask;
    sh2::restore_interrupts((status & ~0x000000F0u) |
                            (static_cast<uint32_t>(mask) << 4u));
}

}  // namespace

bool start() {
    if (g_live) return true;
    const uint32_t status = sh2::save_and_mask_interrupts();
    g_previous_sr_mask = static_cast<uint8_t>((status >> 4u) & 0x0Fu);
    /* SR mask 15 used to hide every on-chip source the BIOS left enabled.
     * Lowering it would admit them -- the Master FRT input capture, which
     * the Slave raises to signal, stormed through a BIOS stub that returns
     * without clearing it. The library owns no on-chip interrupt: park
     * them all at level 0. */
    g_previous_ipra = SH2_IPRA;
    g_previous_iprb = SH2_IPRB;
    SH2_IPRA = 0u;
    SH2_IPRB = 0u;
    for (uint8_t s = 0u; s < logic::kSourceCount; ++s) {
        g_previous[s] = sh2::interrupt::install(logic::vector(s), kIsr[s]);
        g_count[s] = 0u;
    }
    SH2_ICR = static_cast<uint16_t>(SH2_ICR | kIcrVecmd);
    g_installed = true;
    g_enabled = static_cast<uint16_t>(g_enabled | (1u << logic::kVblankIn));
    apply(status);

    /* Proof that the host delivers VBlank-IN before anything relies on it.
     * The SCU raises nothing new while an earlier interrupt still waits for
     * the SH-2 to take it, and one the BIOS left behind at a level below 15
     * would wait forever behind mask 14. Every SCU vector is ours now, so
     * admit all levels until the first VBlank-IN has drained the queue. */
    sh2::set_interrupt_mask(0u);
    const uint16_t frame = ticks_per_frame();
    const uint64_t begin = elapsed_ticks();
    uint32_t spins = 0u;
    while (g_count[logic::kVblankIn] == 0u) {
        const bool stalled = frame != 0u
            ? logic::vblank_stalled(elapsed_ticks() - begin, frame)
            : ++spins >= kStartSpins;
        if (stalled) {
            stop();
            return false;
        }
    }
    sh2::set_interrupt_mask(logic::sr_mask(g_enabled));
    g_live = true;
    return true;
}

void stop() {
    const uint32_t status = sh2::save_and_mask_interrupts();
    g_enabled = 0u;
    g_installed = false;
    g_live = false;
    g_ims = logic::kImsAllMasked;
    SCU_T1MD = 0u;
    SCU_IMS = g_ims;
    for (uint8_t s = 0u; s < logic::kSourceCount; ++s) {
        sh2::interrupt::restore(logic::vector(s), g_previous[s]);
        g_handler[s] = nullptr;
        g_user[s] = nullptr;
    }
    SH2_IPRA = g_previous_ipra;
    SH2_IPRB = g_previous_iprb;
    sh2::restore_interrupts((status & ~0x000000F0u) |
                            (static_cast<uint32_t>(g_previous_sr_mask) << 4u));
}

bool live() {
    return g_live;
}

void set_handler(uint8_t source, Handler handler, void* user) {
    if (source >= logic::kSourceCount) return;
    const uint32_t status = sh2::save_and_mask_interrupts();
    g_handler[source] = handler;
    g_user[source] = user;
    /* Without our vectors an unmasked source would reach the BIOS table. */
    if (!g_installed) {
        sh2::restore_interrupts(status);
        return;
    }
    if (handler != nullptr || source == logic::kVblankIn) {
        g_enabled = static_cast<uint16_t>(g_enabled | (1u << source));
    } else {
        g_enabled = static_cast<uint16_t>(g_enabled & ~(1u << source));
    }
    apply(status);
}

uint32_t count(uint8_t source) {
    return source < logic::kSourceCount ? g_count[source] : 0u;
}

void set_timer0_line(uint16_t line) {
    SCU_T0C = line;
}

void set_timer1(uint16_t dots, bool timer0_line_only) {
    const uint32_t status = sh2::save_and_mask_interrupts();
    SCU_T1S = dots;
    g_timer0_line_only = timer0_line_only;
    apply(status);
}

}  // namespace saturn::hal::scu::irq
