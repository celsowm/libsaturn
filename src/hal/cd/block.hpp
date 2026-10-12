#ifndef SATURN_HAL_CD_BLOCK_HPP
#define SATURN_HAL_CD_BLOCK_HPP

#include "saturn/cd_block.h"

namespace saturn::hal::cd {

/* The CD block behind a device made by sat_cd_block_bind_device; NULL for any
 * other transport. */
sat_cd_block_t* bound_block(const sat_cd_device_t* device);

}  // namespace saturn::hal::cd

#endif /* SATURN_HAL_CD_BLOCK_HPP */
