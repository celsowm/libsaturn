#include <cassert>
#include <cstdint>
#include <cstdio>

#include "saturn/irq.h"
#include "src/core/runtime/state.hpp"
#include "src/hal/scu/irq.hpp"

namespace {
bool live = false;
saturn::hal::scu::irq::Handler handlers[14] = {};
void* users[14] = {};
uint32_t counts[14] = {};
int32_t timer0_line = -1;
int32_t timer1_dots = -1;
bool timer1_line_only = false;

void on_timer0(void* user) { ++*static_cast<int*>(user); }
}

namespace saturn::hal::scu::irq {
bool start() { return live; }
void stop() {}
bool live() { return ::live; }
void set_handler(uint8_t source, Handler handler, void* user) {
    assert(source < 14u);
    handlers[source] = handler;
    users[source] = user;
}
uint32_t count(uint8_t source) { return source < 14u ? counts[source] : 0u; }
void set_timer0_line(uint16_t line) { timer0_line = line; }
void set_timer1(uint16_t dots, bool line_only) {
    timer1_dots = dots;
    timer1_line_only = line_only;
}
}

int main() {
    using saturn::core::g_state;
    int fired = 0;

    /* Nothing before sat_init. */
    assert(sat_irq_active() == 0);
    assert(sat_irq_set_handler(SAT_IRQ_TIMER0, on_timer0, &fired) == SAT_ERR_NOT_INITIALIZED);
    assert(sat_irq_set_timer0_line(100u) == SAT_ERR_NOT_INITIALIZED);

    /* Initialized, but the host never delivered VBlank-IN: polling mode. */
    g_state = {};
    g_state.initialized = true;
    assert(sat_irq_active() == 0);
    assert(sat_irq_set_handler(SAT_IRQ_TIMER0, on_timer0, &fired) == SAT_ERR_UNSUPPORTED);
    assert(sat_irq_set_timer1(100u, 0u) == SAT_ERR_UNSUPPORTED);
    assert(handlers[SAT_IRQ_TIMER0] == nullptr);

    live = true;
    assert(sat_irq_active() == 1);
    assert(sat_irq_set_handler(SAT_IRQ_TIMER0, on_timer0, &fired) == SAT_OK);
    assert(handlers[SAT_IRQ_TIMER0] == on_timer0 && users[SAT_IRQ_TIMER0] == &fired);
    handlers[SAT_IRQ_TIMER0](users[SAT_IRQ_TIMER0]);
    assert(fired == 1);
    assert(sat_irq_set_handler(SAT_IRQ_TIMER0, nullptr, nullptr) == SAT_OK);
    assert(handlers[SAT_IRQ_TIMER0] == nullptr);

    /* Arguments are checked before the hardware is touched. */
    assert(sat_irq_set_handler(SAT_IRQ_SOURCE_COUNT, on_timer0, &fired) == SAT_ERR_INVALID_ARG);
    assert(sat_irq_set_handler(static_cast<sat_irq_source_t>(-1), on_timer0, &fired) ==
           SAT_ERR_INVALID_ARG);
    assert(sat_irq_set_timer0_line(264u) == SAT_ERR_INVALID_ARG);
    assert(timer0_line == -1);
    assert(sat_irq_set_timer0_line(263u) == SAT_OK && timer0_line == 263);
    assert(sat_irq_set_timer0_line(0u) == SAT_OK && timer0_line == 0);
    assert(sat_irq_set_timer1(0u, 0u) == SAT_ERR_INVALID_ARG);
    assert(sat_irq_set_timer1(512u, 0u) == SAT_ERR_INVALID_ARG);
    assert(timer1_dots == -1);
    assert(sat_irq_set_timer1(0x1AAu, 1u) == SAT_OK);
    assert(timer1_dots == 0x1AA && timer1_line_only);

    counts[SAT_IRQ_VBLANK_IN] = 42u;
    assert(sat_irq_count(SAT_IRQ_VBLANK_IN) == 42u);
    assert(sat_irq_count(SAT_IRQ_SOURCE_COUNT) == 0u);
    std::puts("test_irq_api: OK");
    return 0;
}
