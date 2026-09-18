#include <cstdio>
#include <cstdlib>

#include "saturn/cd_block.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

int main() {
    sat_cd_block_t block{};
    OK(sat_cd_block_init(&block, 8u) == SAT_ERR_UNSUPPORTED);
    sat_cd_device_t device{};
    OK(sat_cd_block_bind_device(&block, &device, 0u) == SAT_ERR_NOT_INITIALIZED);
    OK(sat_cd_block_read_sectors(nullptr, 0u, 1u, nullptr) == SAT_ERR_INVALID_ARG);
    uint8_t sector[SAT_CD_SECTOR_BYTES] = {};
    block.initialized = 1u;
    OK(sat_cd_block_read_sectors(&block, 0u, 1u, sector) == SAT_ERR_BUSY);
    std::puts("cd block api: OK");
    return 0;
}
