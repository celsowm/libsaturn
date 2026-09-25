#ifndef SATURN_HAL_CD_PROGRESS_HPP
#define SATURN_HAL_CD_PROGRESS_HPP

#include "saturn/cd_block.h"

namespace saturn::hal::cd {

/* The HAL knows only this explicit per-block hook, never an audio symbol.
 * Suppress recursive callbacks during one service invocation, and record
 * a failing hook instead of dropping its status. */
inline void pump_progress(sat_cd_block_t* block) {
    if (block->progress == nullptr || block->progress_active != 0u) return;
    block->progress_active = 1u;
    const sat_result_t status = block->progress(block->progress_context);
    block->progress_active = 0u;
    if (status != SAT_OK) {
        ++block->progress_error_count;
        block->progress_last_error = status;
    }
}

}  // namespace saturn::hal::cd

#endif
