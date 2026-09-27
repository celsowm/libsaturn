#include <cassert>
#include <cstdint>
#include <cstdio>

#include "saturn/time.h"
#include "src/core/runtime/time_logic.hpp"
#include "src/hal/scu/irq_logic.hpp"

static uint16_t raw_frc_delta(uint16_t then, uint16_t now) {
    return static_cast<uint16_t>(now - then);
}

static void raw_counter_wrap_is_wrap_safe() {
    assert(raw_frc_delta(0xFFFEu, 0x0003u) == 5u);
    assert(raw_frc_delta(0x1234u, 0x1234u) == 0u);
}

static void extended_ticks_keep_subtick_precision() {
    uint64_t total = 0u;
    uint16_t last = 0xFFF0u;
    total = saturn::hal::scu::irq_logic::extend_ticks(
        total, &last, 0x0005u);
    assert(total == 21u);

    /* Observe the counter near the end of the next revolution before the
     * second wrap; every individual observation remains less than one wrap
     * apart, as the runtime's VBlank observation contract requires. */
    total = saturn::hal::scu::irq_logic::extend_ticks(
        total, &last, 0xFFFAu);
    assert(total == 65546u);
    total = saturn::hal::scu::irq_logic::extend_ticks(
        total, &last, 0x0008u);
    assert(total == 65560u);

    /* This is the value sat_time_ticks exposes; no millisecond conversion or
     * per-call floor is involved. */
    assert(saturn::core::time_logic::ticks_to_ms(24u, 3500u, 60u) == 0u);
}

static void public_types_match_the_contract() {
    static_assert(sizeof(sat_time_ticks()) == sizeof(uint64_t));
    static_assert(sizeof(sat_time_frc()) == sizeof(uint16_t));
}

int main() {
    raw_counter_wrap_is_wrap_safe();
    extended_ticks_keep_subtick_precision();
    public_types_match_the_contract();
    std::puts("time API logic: OK");
    return 0;
}
