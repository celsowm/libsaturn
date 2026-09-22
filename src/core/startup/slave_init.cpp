#include "saturn/dual_sh2.h"

#include "src/core/startup/memory_layout.hpp"
#include "src/hal/dual_sh2/mailbox.hpp"
#include "src/hal/dual_sh2/memory.hpp"
#include "src/hal/dual_sh2/signal.hpp"
#include "src/hal/sh2/cpu.hpp"
#include "src/hal/sh2/frt.hpp"

extern "C" void saturn_slave_init(void) {
    using namespace saturn;

    /* The BIOS provides the vector table and initial FRT route.  Establish the
     * local context again without touching the Master's BSS, stack or devices. */
    hal::sh2::set_interrupt_mask(0x0Fu);
    hal::sh2::write_vector_base(core::startup::kSlaveVectorBase);
    hal::sh2::frt::configure_polling();

    if (hal::dual_sh2::memory::control()->magic != hal::dual_sh2::memory::kMagic ||
        hal::dual_sh2::memory::status() != hal::dual_sh2::memory::SharedStatus::Starting) {
        hal::dual_sh2::memory::set_status(hal::dual_sh2::memory::SharedStatus::Fault);
        hal::dual_sh2::signal::send_to_master();
        return;
    }

    const uint32_t entry_address = hal::dual_sh2::memory::control()->slave_entry;
    const uint32_t context_address = hal::dual_sh2::memory::control()->slave_context;
    hal::dual_sh2::memory::set_status(hal::dual_sh2::memory::SharedStatus::Ready);
    hal::dual_sh2::signal::send_to_master();

    const sat_dual_sh2_entry_t entry =
        reinterpret_cast<sat_dual_sh2_entry_t>(entry_address);
    if (entry != nullptr) {
        entry(reinterpret_cast<void*>(context_address));
    }

    /* Returning from an application callback is the only safe point at which
     * the Master may issue SSHOFF.  Do not spin on external memory here. */
    hal::dual_sh2::memory::set_status(hal::dual_sh2::memory::SharedStatus::Stopping);
    hal::dual_sh2::memory::acknowledge_shutdown();
    hal::dual_sh2::memory::set_status(hal::dual_sh2::memory::SharedStatus::Offline);
    hal::dual_sh2::signal::send_to_master();
    hal::sh2::set_interrupt_mask(0x0Fu);
}
