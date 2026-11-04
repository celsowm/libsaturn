#include <cassert>
#include <cstdint>

static uint32_t timer0, timer1, mode, ims;
static uint16_t icr, ipra, iprb;
#define SCU_T0C timer0
#define SCU_T1S timer1
#define SCU_T1MD mode
#define SCU_IMS ims
#define SH2_ICR icr
#define SH2_IPRA ipra
#define SH2_IPRB iprb
#define SAT_SCU_INTERRUPT_ATTRIBUTE
#include "src/hal/scu/irq.cpp"

static unsigned dma_hook_calls;
extern "C" void sat_scu_dma_illegal_isr() { ++dma_hook_calls; }

namespace saturn::hal::sh2 {
static uint32_t sr = 0xF0u;
uint32_t save_and_mask_interrupts() { uint32_t old = sr; sr |= 0xF0u; return old; }
void restore_interrupts(uint32_t value) { sr = value; }
void set_interrupt_mask(uint8_t mask) { sr = (sr & ~0xF0u) | (mask << 4u); }
namespace interrupt {
Handler install(uint8_t, Handler) { return nullptr; }
void restore(uint8_t, Handler) {}
}
}
namespace saturn::hal::scu {
void on_vblank_in() {}
uint16_t ticks_per_frame() { return 0u; }
uint64_t elapsed_ticks() { return 0u; }
}

static void count_callback(void* user) { ++*static_cast<unsigned*>(user); }
static void check_dma_hook_first(void* user) {
    assert(dma_hook_calls == 1u);
    ++*static_cast<unsigned*>(user);
}

int main() {
    namespace irq = saturn::hal::scu::irq;
    assert(!irq::retain(7u));
    irq::g_installed = true;
    irq::g_live = true;
    irq::g_enabled = 1u;
    saturn::hal::sh2::sr = 0xF0u;
    assert(!irq::retain(7u)); /* caller's critical section stays masked */
    assert(saturn::hal::sh2::sr == 0xF0u);
    saturn::hal::sh2::sr = 0xE0u;
    unsigned calls = 0u;
    assert(irq::retain(7u) && irq::retain(7u));
    assert((ims & (1u << 7u)) == 0u);
    irq::set_handler(7u, count_callback, &calls);
    irq::dispatch(7u);
    assert(calls == 1u && irq::count(7u) == 1u);
    irq::set_handler(7u, nullptr, nullptr);
    assert((ims & (1u << 7u)) == 0u);
    irq::release(7u);
    assert((ims & (1u << 7u)) == 0u);
    irq::release(7u);
    assert((ims & (1u << 7u)) != 0u);
    irq::set_handler(7u, count_callback, &calls);
    assert(irq::retain(7u));
    saturn::hal::sh2::sr = 0xF0u;
    irq::release(7u);
    assert(saturn::hal::sh2::sr == 0xF0u);
    assert((ims & (1u << 7u)) == 0u);
    unsigned dma_handler_calls = 0u;
    irq::set_handler(12u, check_dma_hook_first, &dma_handler_calls);
    irq::dispatch(12u);
    assert(dma_hook_calls == 1u && dma_handler_calls == 1u);
    irq::stop();
    assert(!irq::retain(7u));
}
