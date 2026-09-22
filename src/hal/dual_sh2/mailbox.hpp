#ifndef SATURN_HAL_DUAL_SH2_MAILBOX_HPP
#define SATURN_HAL_DUAL_SH2_MAILBOX_HPP

#include "src/hal/dual_sh2/protocol.hpp"

namespace saturn::hal::dual_sh2::mailbox {

bool master_send(uint32_t command, uint32_t argument0, uint32_t argument1);
bool master_receive(protocol::Message* out_message);
bool master_has_response();
bool slave_receive(protocol::Message* out_message);
bool slave_send(uint32_t command, uint32_t argument0, uint32_t argument1);
bool slave_has_request();

}  // namespace saturn::hal::dual_sh2::mailbox

#endif
