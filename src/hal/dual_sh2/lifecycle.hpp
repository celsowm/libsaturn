#ifndef SATURN_HAL_DUAL_SH2_LIFECYCLE_HPP
#define SATURN_HAL_DUAL_SH2_LIFECYCLE_HPP

#include <stdint.h>

#include "saturn/dual_sh2.h"

namespace saturn::hal::dual_sh2::lifecycle {

sat_result_t configure(sat_dual_sh2_entry_t entry, void* context);
sat_result_t start(uint32_t timeout_ticks);
sat_dual_sh2_state_t state();
sat_result_t stop(uint32_t timeout_ticks);
sat_result_t reset(uint32_t timeout_ticks);
sat_result_t master_send(uint32_t command, uint32_t argument0, uint32_t argument1);
sat_result_t master_receive(sat_dual_sh2_message_t* out_message);
sat_result_t wait_response(sat_dual_sh2_message_t* out_message, uint32_t timeout_ticks);

}  // namespace saturn::hal::dual_sh2::lifecycle

#endif
