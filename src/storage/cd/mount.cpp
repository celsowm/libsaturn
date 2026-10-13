#include "saturn/cd_mount.h"

extern "C" sat_result_t sat_cd_mount_default(
    sat_cd_block_t* block,
    sat_cd_block_progress_fn progress, void* progress_context,
    sat_cd_device_t* device, uint32_t sector_count,
    sat_cdfs_volume_t* volume
) {
    if (block != nullptr) {
        sat_result_t st = sat_cd_block_init(block, SAT_CD_BLOCK_DEFAULT_TIMEOUT);
        if (st != SAT_OK) return st;
        if (progress != nullptr) {
            st = sat_cd_block_set_progress_service(
                block, progress, progress_context);
            if (st != SAT_OK) return st;
        }
    }
    if (device != nullptr) {
        sat_result_t st = sat_cd_block_bind_device(block, device, sector_count);
        if (st != SAT_OK) return st;
    }
    if (volume != nullptr) {
        return sat_cdfs_mount(volume, device);
    }
    return SAT_OK;
}
