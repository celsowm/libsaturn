#include "src/hal/dual_sh2/signal.hpp"

#include "src/hal/sh2/cpu.hpp"
#include "src/hal/sh2/frt.hpp"

namespace saturn::hal::dual_sh2::signal {

namespace {

volatile uint16_t& master_to_slave_signal() {
    return *reinterpret_cast<volatile uint16_t*>(0x21000000u);
}

volatile uint16_t& slave_to_master_signal() {
    return *reinterpret_cast<volatile uint16_t*>(0x21800000u);
}

}  // namespace

void send_to_slave() {
    master_to_slave_signal() = 0xFFFFu;
    sh2::compiler_barrier();
}

void send_to_master() {
    slave_to_master_signal() = 0xFFFFu;
    sh2::compiler_barrier();
}

bool pending() {
    return sh2::frt::capture_pending();
}

void acknowledge() {
    sh2::frt::acknowledge_capture();
}

}  // namespace saturn::hal::dual_sh2::signal
