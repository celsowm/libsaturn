#include <cstdio>
#include <cstdlib>

#include "saturn/cd_block.h"
#include "src/hal/cd/progress.hpp"

#if SAT_CD_BLOCK_HIRQ_CSCT != 0x0004u
#error "CSCT must select the CD Block sector-stored flag"
#endif

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

static sat_result_t progress_callback(void* context) {
    ++*static_cast<uint16_t*>(context);
    return SAT_OK;
}

/* A hook that (wrongly) reads the disc again while a read is in flight. */
static sat_result_t nested_read_callback(void* context) {
    static uint8_t sector[SAT_CD_SECTOR_BYTES];
    return sat_cd_block_read_sectors(static_cast<sat_cd_block_t*>(context), 0u, 1u, sector);
}

int main() {
    sat_cd_block_t block{};
    OK(sat_cd_block_init(&block, 8u) == SAT_ERR_UNSUPPORTED);
    sat_cd_device_t device{};
    OK(sat_cd_block_bind_device(&block, &device, 0u) == SAT_ERR_NOT_INITIALIZED);
    OK(sat_cd_block_read_sectors(nullptr, 0u, 1u, nullptr) == SAT_ERR_INVALID_ARG);
    uint8_t sector[SAT_CD_SECTOR_BYTES] = {};
    block.initialized = 1u;
    uint16_t progress_calls=0u;
    OK(sat_cd_block_set_progress_service(nullptr, progress_callback, &progress_calls)
       == SAT_ERR_INVALID_ARG);
    OK(sat_cd_block_set_progress_service(
        &block, progress_callback, &progress_calls)==SAT_OK);
    OK(block.progress == progress_callback &&
       block.progress_context == &progress_calls);
    // Host-only CD emulation has no ready command; an immediate BUSY does
    // not enter the long wait loop and must not spuriously call progress.
    OK(sat_cd_block_read_sectors(&block, 0u, 1u, sector) == SAT_ERR_BUSY);
    OK(progress_calls == 0u && block.read_active == 0u);
    block.read_active=1u;
    OK(sat_cd_block_read_sectors(&block,0u,1u,sector)==SAT_ERR_BUSY);
    OK(sat_cd_block_set_progress_service(&block,nullptr,nullptr)==SAT_ERR_BUSY);
    block.read_active=0u;
    block.progress_active=1u;
    OK(sat_cd_block_set_progress_service(&block,nullptr,nullptr)==SAT_ERR_BUSY);
    block.progress_active=0u;
    OK(sat_cd_block_set_progress_service(&block,nullptr,nullptr)==SAT_OK);
    OK(block.progress == nullptr && block.progress_context == nullptr);

    // The nested read is refused, and the refusal is counted instead of
    // silently dropped; a later successful hook keeps the last error.
    sat_cd_block_t nested{};
    nested.initialized = 1u;
    OK(sat_cd_block_set_progress_service(&nested, nested_read_callback, &nested) == SAT_OK);
    OK(nested.progress_error_count == 0u && nested.progress_last_error == SAT_OK);
    nested.read_active = 1u;  // as inside sat_cd_block_read_sectors' wait
    saturn::hal::cd::pump_progress(&nested);
    saturn::hal::cd::pump_progress(&nested);
    OK(nested.progress_error_count == 2u && nested.progress_last_error == SAT_ERR_BUSY);
    OK(nested.progress_active == 0u);
    nested.read_active = 0u;
    nested.progress = progress_callback;
    nested.progress_context = &progress_calls;
    saturn::hal::cd::pump_progress(&nested);
    OK(progress_calls == 1u && nested.progress_error_count == 2u &&
       nested.progress_last_error == SAT_ERR_BUSY);
    // Re-entry from inside a hook is suppressed, not counted.
    nested.progress_active = 1u;
    saturn::hal::cd::pump_progress(&nested);
    OK(progress_calls == 1u && nested.progress_error_count == 2u);
    std::puts("cd block api: OK");
    return 0;
}
