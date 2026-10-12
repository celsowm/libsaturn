#ifndef SATURN_STORAGE_CD_EXEC_HAL_HPP
#define SATURN_STORAGE_CD_EXEC_HAL_HPP

#include <stdint.h>

#include "saturn/cd_block.h"
#include "src/storage/cd/exec_logic.hpp"

/* The hardware side of sat_exec, apart so host tests can stage into a buffer
 * and catch the jump (exec_hal.cpp on the Saturn). */
namespace saturn::storage::exec {

/* Where the image is read: Work RAM Low from its base. */
void* staging();
/* The hand-off block (cache-through on the Saturn). */
logic::Handoff* handoff();
/* The CD block behind a device bound by sat_cd_block_bind_device, else NULL. */
sat_cd_block_t* bound_block(const sat_cd_device_t* device);
/* Interrupts back to the BIOS vectors and masked, Slave off, SCU DMA and DSP
 * stopped, SCSP slots keyed off with the sound CPU off, VDP1 idle, VDP2
 * registers cleared with the display off. */
void stop_machine();
/* Copies `bytes` of the staged image to SAT_EXEC_LOAD_ADDRESS and jumps there. */
[[noreturn]] void start_image(uint32_t bytes);
/* SMPC SYSRES: the BIOS boots the disc again. */
[[noreturn]] void reset_machine();

}  // namespace saturn::storage::exec

#endif /* SATURN_STORAGE_CD_EXEC_HAL_HPP */
