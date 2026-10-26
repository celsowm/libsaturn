#ifndef SATURN_HAL_DUAL_SH2_IDLE_WAIT_HPP
#define SATURN_HAL_DUAL_SH2_IDLE_WAIT_HPP

#include <stdint.h>
#include "src/hal/dual_sh2/protocol.hpp"

namespace saturn::hal::dual_sh2 {

/* /128 FRT ticks: approximately 1.2 ms at the normal video clocks. This is a
 * maximum interval between maintenance/sequence checks, not a task deadline. */
constexpr uint16_t kIdleMaintenanceTicks = 256u;

template<typename Transport>
bool wait_for_request(Transport& transport, protocol::Message* message) {
    uint16_t maintained = transport.now();
    bool first = true;
    for (;;) {
        const bool signalled = transport.pending();
        const uint16_t now = transport.now();
        const bool maintenance = first ||
            static_cast<uint16_t>(now - maintained) >= kIdleMaintenanceTicks;
        if (!signalled && !maintenance) continue;
        /* Clear before checking the mailbox. A publish racing the clear is
         * found by the subsequent sequence read; a signal racing consume's
         * acknowledgment is recovered by the bounded maintenance scan. */
        if (signalled) transport.acknowledge();
        if (maintenance) {
            maintained = now;
            transport.heartbeat();
        }
        if (transport.stopping()) return false;
        if (transport.receive(message)) return true;
        first = false;
    }
}

}  // namespace saturn::hal::dual_sh2
#endif
