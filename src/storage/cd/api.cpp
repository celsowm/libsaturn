#include "saturn/cd.h"

extern "C" sat_result_t sat_cd_device_init(
    sat_cd_device_t* out_device,
    sat_cd_read_sectors_fn read_sectors,
    void* context,
    uint32_t sector_count
) {
    if (out_device == nullptr || read_sectors == nullptr) return SAT_ERR_INVALID_ARG;
    out_device->read_sectors = read_sectors;
    out_device->context = context;
    out_device->sector_count = sector_count;
    return SAT_OK;
}

extern "C" sat_result_t sat_cd_read_sectors(
    const sat_cd_device_t* device,
    uint32_t lba,
    uint32_t sector_count,
    void* destination
) {
    if (device == nullptr || device->read_sectors == nullptr || destination == nullptr ||
        sector_count == 0u) return SAT_ERR_INVALID_ARG;
    if (device->sector_count != 0u &&
        (lba > device->sector_count || sector_count > device->sector_count - lba)) {
        return SAT_ERR_IO;
    }
    return device->read_sectors(device->context, lba, sector_count, destination);
}
