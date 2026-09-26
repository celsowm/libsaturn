#ifndef SATURN_HAL_SH2_DMAC_HPP
#define SATURN_HAL_SH2_DMAC_HPP

#include <stdint.h>

#include "saturn/core.h"
#include "src/hal/sh2/dmac_logic.hpp"

namespace saturn::hal::sh2::dmac {

/* True when a Work RAM to Work RAM copy of this shape is one the DMAC takes
 * (longword aligned and sized, at least kMinBytes, no forward overlap). */
bool can_copy(const void* dst, const void* src, uint32_t bytes);

/* Copies with this SH-2's DMAC channel 0 and waits, then invalidates the
 * destination in the cache. SAT_ERR_UNSUPPORTED when can_copy() is false,
 * SAT_ERR_TIMEOUT after stopping a transfer that never ended, SAT_ERR_IO on
 * a DMAC address error. */
sat_result_t copy(void* dst, const void* src, uint32_t bytes);

}  // namespace saturn::hal::sh2::dmac

#endif
