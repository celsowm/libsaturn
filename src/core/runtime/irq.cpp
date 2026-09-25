#include "saturn/irq.h"

#include "src/core/runtime/state.hpp"
#include "src/hal/scu/irq.hpp"
#include "src/hal/scu/irq_logic.hpp"

namespace {

namespace irq = saturn::hal::scu::irq;
namespace logic = saturn::hal::scu::irq_logic;

bool valid_source(sat_irq_source_t source) {
    return static_cast<uint32_t>(source) < logic::kSourceCount;
}

sat_result_t require_active() {
    SAT_TRY(saturn::core::require_initialized());
    return irq::live() ? SAT_OK : SAT_ERR_UNSUPPORTED;
}

}  // namespace

extern "C" int sat_irq_active(void) {
    return saturn::core::g_state.initialized && irq::live() ? 1 : 0;
}

extern "C" sat_result_t sat_irq_set_handler(sat_irq_source_t source,
                                            sat_irq_handler_t handler, void* user) {
    if (!valid_source(source)) return SAT_ERR_INVALID_ARG;
    SAT_TRY(require_active());
    irq::set_handler(static_cast<uint8_t>(source), handler, user);
    return SAT_OK;
}

extern "C" uint32_t sat_irq_count(sat_irq_source_t source) {
    if (!valid_source(source)) return 0u;
    return irq::count(static_cast<uint8_t>(source));
}

extern "C" sat_result_t sat_irq_set_timer0_line(uint16_t line) {
    if (line > logic::kTimer0MaxLine) return SAT_ERR_INVALID_ARG;
    SAT_TRY(require_active());
    irq::set_timer0_line(line);
    return SAT_OK;
}

extern "C" sat_result_t sat_irq_set_timer1(uint16_t dots, uint8_t timer0_line_only) {
    if (dots == 0u || dots > logic::kTimer1MaxDots) return SAT_ERR_INVALID_ARG;
    SAT_TRY(require_active());
    irq::set_timer1(dots, timer0_line_only != 0u);
    return SAT_OK;
}
