#include "src/hal/dual_sh2/lifecycle.hpp"

#include "src/core/startup/memory_layout.hpp"
#include "src/hal/dual_sh2/mailbox.hpp"
#include "src/hal/dual_sh2/memory.hpp"
#include "src/hal/dual_sh2/protocol.hpp"
#include "src/hal/dual_sh2/signal.hpp"
#include "src/hal/sh2/frt.hpp"
#include "src/hal/smpc/smpc.hpp"

extern "C" void saturn_slave_entry(void) asm("_saturn_slave_entry");

namespace saturn::hal::dual_sh2::lifecycle {

namespace {

sat_dual_sh2_state_t g_state = SAT_DUAL_SH2_OFFLINE;

bool wait_for_status(memory::SharedStatus wanted, uint32_t timeout_ticks) {
    const uint16_t start = sh2::frt::counter();
    uint16_t previous = start;
    uint32_t no_progress = 0u;
    constexpr uint32_t kFallbackSpins = 2000000u;
    for (uint32_t spins = 0u; spins < kFallbackSpins; ++spins) {
        if (memory::status() == wanted) return true;
        if (memory::status() == memory::SharedStatus::Fault) return false;
        const uint16_t now = sh2::frt::counter();
        if (now != previous) {
            previous = now;
            no_progress = 0u;
            if (static_cast<uint16_t>(now - start) >=
                static_cast<uint16_t>(timeout_ticks)) {
                return false;
            }
        } else if (++no_progress >= kFallbackSpins / 4u && timeout_ticks == 0u) {
            return false;
        }
    }
    return false;
}

}  // namespace

sat_result_t configure(sat_dual_sh2_entry_t entry, void* context) {
    if (entry == nullptr) return SAT_ERR_INVALID_ARG;
    if (g_state != SAT_DUAL_SH2_OFFLINE) return SAT_ERR_BUSY;
    memory::set_entry(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(entry)),
                      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(context)));
    return SAT_OK;
}

sat_result_t start(uint32_t timeout_ticks) {
    if (timeout_ticks == 0u) return SAT_ERR_TIMEOUT;
    if (g_state != SAT_DUAL_SH2_OFFLINE) return SAT_ERR_BUSY;

    volatile memory::ControlBlock* const old_control = memory::control();
    const uint32_t entry = old_control->slave_entry;
    const uint32_t context = old_control->slave_context;
    if (entry == 0u) return SAT_ERR_INVALID_ARG;

    g_state = SAT_DUAL_SH2_STARTING;
    if (!smpc::slave_off()) {
        g_state = SAT_DUAL_SH2_FAULT;
        memory::set_status(memory::SharedStatus::Fault);
        return SAT_ERR_TIMEOUT;
    }

    const uint32_t generation = protocol::next_sequence(memory::generation());
    memory::reset(generation);
    memory::set_entry(entry, context);
    memory::write_slave_entry_vector(
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&saturn_slave_entry)));
    memory::set_status(memory::SharedStatus::Starting);

    if (!smpc::slave_on()) {
        g_state = SAT_DUAL_SH2_FAULT;
        memory::set_status(memory::SharedStatus::Fault);
        return SAT_ERR_TIMEOUT;
    }

    if (!wait_for_status(memory::SharedStatus::Ready, timeout_ticks)) {
        g_state = SAT_DUAL_SH2_FAULT;
        memory::set_status(memory::SharedStatus::Fault);
        return SAT_ERR_TIMEOUT;
    }
    g_state = SAT_DUAL_SH2_READY;
    return SAT_OK;
}

sat_dual_sh2_state_t state() {
    const memory::SharedStatus shared = memory::status();
    if (shared == memory::SharedStatus::Fault) return SAT_DUAL_SH2_FAULT;
    if (g_state == SAT_DUAL_SH2_READY && shared == memory::SharedStatus::Offline) {
        /* The configured callback may return without a Master stop request.
         * Fold that terminal condition back into the local lifecycle so a
         * subsequent configure/start sequence is not permanently reported as
         * busy. */
        g_state = SAT_DUAL_SH2_OFFLINE;
        return SAT_DUAL_SH2_OFFLINE;
    }
    return g_state;
}

sat_result_t stop(uint32_t timeout_ticks) {
    if (g_state == SAT_DUAL_SH2_OFFLINE) return SAT_OK;
    if (g_state != SAT_DUAL_SH2_READY && g_state != SAT_DUAL_SH2_STARTING) {
        return SAT_ERR_BUSY;
    }
    if (timeout_ticks == 0u) return SAT_ERR_TIMEOUT;

    g_state = SAT_DUAL_SH2_STOPPING;
    memory::request_shutdown();
    signal::send_to_slave();
    if (!wait_for_status(memory::SharedStatus::Offline, timeout_ticks)) {
        g_state = SAT_DUAL_SH2_FAULT;
        memory::set_status(memory::SharedStatus::Fault);
        return SAT_ERR_TIMEOUT;
    }

    /* SSHOFF is issued only after the Slave callback has returned and the
     * shared status says it no longer accesses external memory. */
    if (!smpc::slave_off()) {
        g_state = SAT_DUAL_SH2_FAULT;
        memory::set_status(memory::SharedStatus::Fault);
        return SAT_ERR_TIMEOUT;
    }
    g_state = SAT_DUAL_SH2_OFFLINE;
    return SAT_OK;
}

sat_result_t reset(uint32_t timeout_ticks) {
    SAT_TRY(stop(timeout_ticks));
    return start(timeout_ticks);
}

sat_result_t master_send(uint32_t command, uint32_t argument0, uint32_t argument1) {
    if (g_state != SAT_DUAL_SH2_READY) return SAT_ERR_NOT_CONNECTED;
    return mailbox::master_send(command, argument0, argument1)
        ? SAT_OK : SAT_ERR_BUSY;
}

sat_result_t master_receive(sat_dual_sh2_message_t* out_message) {
    if (out_message == nullptr) return SAT_ERR_INVALID_ARG;
    if (g_state != SAT_DUAL_SH2_READY) return SAT_ERR_NOT_CONNECTED;
    protocol::Message message{};
    if (!mailbox::master_receive(&message)) return SAT_ERR_NOT_FOUND;
    out_message->sequence = message.sequence;
    out_message->command = message.command;
    out_message->argument0 = message.argument0;
    out_message->argument1 = message.argument1;
    return SAT_OK;
}

sat_result_t wait_response(sat_dual_sh2_message_t* out_message, uint32_t timeout_ticks) {
    if (out_message == nullptr) return SAT_ERR_INVALID_ARG;
    if (timeout_ticks == 0u) return SAT_ERR_TIMEOUT;
    const uint16_t start_tick = sh2::frt::counter();
    const uint32_t spins_limit = 2000000u;
    for (uint32_t spins = 0u; spins < spins_limit; ++spins) {
        const sat_result_t result = master_receive(out_message);
        if (result == SAT_OK) return SAT_OK;
        if (result != SAT_ERR_NOT_FOUND) return result;
        if (static_cast<uint16_t>(sh2::frt::counter() - start_tick) >=
            static_cast<uint16_t>(timeout_ticks)) {
            return SAT_ERR_TIMEOUT;
        }
    }
    return SAT_ERR_TIMEOUT;
}

}  // namespace saturn::hal::dual_sh2::lifecycle
