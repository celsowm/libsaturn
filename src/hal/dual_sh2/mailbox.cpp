#include "src/hal/dual_sh2/mailbox.hpp"

#include "src/hal/dual_sh2/memory.hpp"
#include "src/hal/dual_sh2/signal.hpp"
#include "src/hal/sh2/cpu.hpp"

namespace saturn::hal::dual_sh2::mailbox {

namespace {

bool publish(volatile memory::MailboxSlot* slot, uint32_t generation,
             uint32_t command, uint32_t argument0, uint32_t argument1,
             bool to_slave) {
    if (slot->sequence != slot->acknowledgment) return false;
    sh2::compiler_barrier();
    const bool published = protocol::publish_slot(
        *slot, generation, command, argument0, argument1);
    sh2::compiler_barrier();
    if (!published) return false;
    if (to_slave) signal::send_to_slave(); else signal::send_to_master();
    return true;
}

bool consume(volatile memory::MailboxSlot* slot, uint32_t generation,
             protocol::Message* out_message) {
    const bool consumed = protocol::consume_slot(*slot, generation, out_message);
    sh2::compiler_barrier();
    if (!consumed) return false;
    signal::acknowledge();
    return true;
}

}  // namespace

bool master_send(uint32_t command, uint32_t argument0, uint32_t argument1) {
    return publish(&memory::control()->master_to_slave, memory::generation(),
                   command, argument0, argument1, true);
}

bool master_receive(protocol::Message* out_message) {
    return consume(&memory::control()->slave_to_master, memory::generation(),
                   out_message);
}

bool master_has_response() {
    const volatile memory::MailboxSlot& slot = memory::control()->slave_to_master;
    return protocol::has_message(slot.sequence, slot.acknowledgment);
}

bool slave_receive(protocol::Message* out_message) {
    return consume(&memory::control()->master_to_slave, memory::generation(),
                   out_message);
}

bool slave_send(uint32_t command, uint32_t argument0, uint32_t argument1) {
    return publish(&memory::control()->slave_to_master, memory::generation(),
                   command, argument0, argument1, false);
}

bool slave_has_request() {
    const volatile memory::MailboxSlot& slot = memory::control()->master_to_slave;
    return protocol::has_message(slot.sequence, slot.acknowledgment);
}

}  // namespace saturn::hal::dual_sh2::mailbox
