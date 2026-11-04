#include <cassert>
#include <cstdint>

/* Exercise the actual DMA HAL with ordinary host memory in place of MMIO.
 * Status changes are supplied explicitly: this is not a hardware model. */
static uint32_t registers[64];
static bool irq_available = true;
static uint32_t illegal_irqs;
static volatile uint32_t& fake_register(uint32_t address) {
    assert(address >= 0x25FE0000u && address < 0x25FE0100u);
    return registers[(address - 0x25FE0000u) / 4u];
}
#define SCU_REG(addr) fake_register(addr)
#include "src/hal/scu/dma.cpp"

namespace saturn::hal::scu::irq {
bool live() { return irq_available; }
uint32_t count(uint8_t) { return illegal_irqs; }
bool retain(uint8_t) {
    if (!irq_available) return false;
    fake_register(dma_logic::kRegisterIstatus) &= ~dma_logic::kIstIllegal;
    ++illegal_irqs;
    return true;
}
void release(uint8_t) {}
}
namespace saturn::hal::sh2::cache {
bool invalidate_range(uint32_t, uint32_t) { return true; }
}
namespace saturn::hal::sh2 {
uint32_t save_and_mask_interrupts() { return 0u; }
void restore_interrupts(uint32_t) {}
}
namespace saturn::hal::sh2::dmac {
bool can_copy(const void*, const void*, uint32_t) { return false; }
sat_result_t copy(void*, const void*, uint32_t) { return SAT_ERR_UNSUPPORTED; }
}

int main() {
    using namespace saturn::hal::scu;
    const void* const src = reinterpret_cast<const void*>(0x06004000u);
    void* const dst = reinterpret_cast<void*>(0x05C40000u);
    auto& ist = fake_register(dma_logic::kRegisterIstatus);
    auto& status = fake_register(dma_logic::kRegisterStatus);

    const void* srcs[] = {src};
    void* dsts[] = {dst};
    const uint32_t sizes[] = {64u};
    dma::g_table[0][0] = 0x12345678u;
    status = dma_logic::kStatusBusy[0];
    assert(dma::start_list(0u, srcs, dsts, sizes, 1u) == SAT_ERR_BUSY);
    assert(dma::g_table[0][0] == 0x12345678u);
    status = 0u;

    assert(dma::start(0u, src, dst, 64u) == SAT_OK);
    status = dma_logic::kStatusBusy[0];
    assert(dma::busy(0u));
    status = 0u;
    ist = dma_logic::kIstIllegal;
    assert(!dma::busy(0u));
    assert(!dma::busy(0u));
    assert(dma::wait(0u) == SAT_ERR_IO);
    assert(dma::wait(0u) == SAT_OK);
    assert(dma::stats().illegal == 1u);

    ist = 0u;
    assert(dma::start(0u, src, dst, 64u) == SAT_OK);
    ist = dma_logic::kIstIllegal;
    assert(!dma::busy(0u));
    assert(dma::start(0u, src, dst, 64u) == SAT_ERR_IO);
    ist = 0u;
    assert(dma::start(0u, src, dst, 64u) == SAT_OK);
    ist = dma_logic::kIstEnd[0] | (1u << 4u);
    assert(dma::wait(0u) == SAT_OK);
    assert(ist == (dma_logic::kIstEnd[0] | (1u << 4u)));
    assert(dma::stats().illegal == 2u);

    ist = 0u;
    assert(dma::start(0u, src, dst, 64u) == SAT_OK);
    assert(dma::start(1u, src, dst, 64u) == SAT_OK);
    ist = dma_logic::kIstIllegal;
    assert(dma::wait(0u) == SAT_ERR_IO);
    ist = 0u; /* A hardware clear leaves the shared status clear. */
    assert(dma::wait(1u) == SAT_ERR_IO);
    assert(dma::stats().illegal == 3u);

    /* The public IRQ handler may consume IST before either wait observes it. */
    ist = 0u;
    assert(dma::start(0u, src, dst, 64u) == SAT_OK);
    assert(dma::start(1u, src, dst, 64u) == SAT_OK);
    dma::sat_scu_dma_illegal_isr();
    assert(dma::wait(0u) == SAT_ERR_IO);
    assert(dma::wait(1u) == SAT_ERR_IO);
    assert(dma::stats().illegal == 4u);
    dma::sat_scu_dma_illegal_isr();
    dma::sat_scu_dma_illegal_isr();
    assert(dma::stats().illegal == 6u);


    ist = 0u;
    assert(dma::start(0u, src, dst, 64u) == SAT_OK);
    assert(dma::start(1u, src, dst, 64u) == SAT_OK);
    status = dma_logic::kStatusBusy[0] | dma_logic::kStatusBusy[1];
    assert(dma::wait(0u) == SAT_ERR_TIMEOUT);
    assert(dma::busy(0u)); /* mock stop has not made the hardware idle */
    status = 0u;
    ist = 0u;
    assert(dma::wait(1u) == SAT_ERR_TIMEOUT);
    ist = 0u;
    assert(dma::wait(0u) == SAT_ERR_TIMEOUT);

    /* No interrupt delivery: preserve the status and refuse another job.
     * Once delivery returns, acknowledge it and allow a new transfer. */
    irq_available = false;
    ist = dma_logic::kIstIllegal | (1u << 4u);
    const uint32_t errors_before = dma::stats().illegal;
    assert(dma::start(0u, src, dst, 64u) == SAT_ERR_IO);
    assert(dma::start(0u, src, dst, 64u) == SAT_ERR_IO);
    assert(ist == (dma_logic::kIstIllegal | (1u << 4u)));
    assert(dma::stats().illegal == errors_before + 1u);
    irq_available = true;
    assert(dma::start(0u, src, dst, 64u) == SAT_OK);
    assert(ist == (1u << 4u));
    assert(dma::wait(0u) == SAT_OK);
}
