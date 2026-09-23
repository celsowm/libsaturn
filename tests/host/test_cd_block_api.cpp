#include <cstdio>
#include <cstdlib>

#include "saturn/cd_block.h"

#if SAT_CD_BLOCK_HIRQ_CSCT != 0x0004u
#error "CSCT must select the CD Block sector-stored flag"
#endif

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

static sat_result_t progress_callback(void* context) {
    ++*static_cast<uint16_t*>(context);
    return SAT_OK;
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
    OK(progress_calls == 0u);
    block.progress_active=1u;
    OK(sat_cd_block_set_progress_service(&block,nullptr,nullptr)==SAT_ERR_BUSY);
    block.progress_active=0u;
    OK(sat_cd_block_set_progress_service(&block,nullptr,nullptr)==SAT_OK);
    OK(block.progress == nullptr && block.progress_context == nullptr);
    std::puts("cd block api: OK");
    return 0;
}
