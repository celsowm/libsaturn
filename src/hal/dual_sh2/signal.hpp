#ifndef SATURN_HAL_DUAL_SH2_SIGNAL_HPP
#define SATURN_HAL_DUAL_SH2_SIGNAL_HPP

namespace saturn::hal::dual_sh2::signal {

void send_to_slave();
void send_to_master();
bool pending();
void acknowledge();

}  // namespace saturn::hal::dual_sh2::signal

#endif
