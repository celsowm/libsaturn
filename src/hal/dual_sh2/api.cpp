#include "saturn/dual_sh2.h"

#include "src/hal/dual_sh2/lifecycle.hpp"
#include "src/hal/dual_sh2/mailbox.hpp"
#include "src/hal/dual_sh2/memory.hpp"
#include "src/hal/dual_sh2/protocol.hpp"
#include "src/hal/dual_sh2/signal.hpp"

extern "C" sat_result_t sat_dual_sh2_configure_slave(
    sat_dual_sh2_entry_t entry, void* context) {
    return saturn::hal::dual_sh2::lifecycle::configure(entry, context);
}

extern "C" sat_result_t sat_dual_sh2_start(uint32_t timeout_ticks) {
    return saturn::hal::dual_sh2::lifecycle::start(timeout_ticks);
}

extern "C" sat_dual_sh2_state_t sat_dual_sh2_state(void) {
    return saturn::hal::dual_sh2::lifecycle::state();
}

extern "C" uint8_t sat_dual_sh2_available(void) {
    return sat_dual_sh2_state() == SAT_DUAL_SH2_READY ? 1u : 0u;
}

extern "C" sat_result_t sat_dual_sh2_stop(uint32_t timeout_ticks) {
    return saturn::hal::dual_sh2::lifecycle::stop(timeout_ticks);
}

extern "C" sat_result_t sat_dual_sh2_reset(uint32_t timeout_ticks) {
    return saturn::hal::dual_sh2::lifecycle::reset(timeout_ticks);
}

extern "C" sat_result_t sat_dual_sh2_send(
    uint32_t command, uint32_t argument0, uint32_t argument1) {
    return saturn::hal::dual_sh2::lifecycle::master_send(
        command, argument0, argument1);
}

extern "C" sat_result_t sat_dual_sh2_receive(sat_dual_sh2_message_t* out_message) {
    return saturn::hal::dual_sh2::lifecycle::master_receive(out_message);
}

extern "C" sat_result_t sat_dual_sh2_wait_response(
    sat_dual_sh2_message_t* out_message, uint32_t timeout_ticks) {
    return saturn::hal::dual_sh2::lifecycle::wait_response(out_message, timeout_ticks);
}

extern "C" sat_result_t sat_dual_sh2_signal_slave(void) {
    if (!sat_dual_sh2_available()) return SAT_ERR_NOT_CONNECTED;
    saturn::hal::dual_sh2::signal::send_to_slave();
    return SAT_OK;
}

extern "C" sat_result_t sat_dual_sh2_slave_receive(
    sat_dual_sh2_message_t* out_message) {
    if (out_message == nullptr) return SAT_ERR_INVALID_ARG;
    saturn::hal::dual_sh2::protocol::Message message{};
    if (!saturn::hal::dual_sh2::mailbox::slave_receive(&message)) {
        return SAT_ERR_NOT_FOUND;
    }
    out_message->sequence = message.sequence;
    out_message->command = message.command;
    out_message->argument0 = message.argument0;
    out_message->argument1 = message.argument1;
    return SAT_OK;
}

extern "C" sat_result_t sat_dual_sh2_slave_send(
    uint32_t command, uint32_t argument0, uint32_t argument1) {
    return saturn::hal::dual_sh2::mailbox::slave_send(
        command, argument0, argument1) ? SAT_OK : SAT_ERR_BUSY;
}

extern "C" uint8_t sat_dual_sh2_slave_stop_requested(void) {
    return saturn::hal::dual_sh2::memory::shutdown_requested() ? 1u : 0u;
}

extern "C" void sat_dual_sh2_slave_heartbeat(void) {
    saturn::hal::dual_sh2::memory::heartbeat();
}

extern "C" uint32_t sat_dual_sh2_slave_heartbeat_count(void) {
    return saturn::hal::dual_sh2::memory::heartbeat_value();
}

extern "C" sat_result_t sat_dual_sh2_slave_signal_master(void) {
    saturn::hal::dual_sh2::signal::send_to_master();
    return SAT_OK;
}
