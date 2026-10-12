#include "saturn/exec.h"

#include "saturn/cd.h"
#include "src/storage/cd/exec_hal.hpp"

namespace exec_hal = saturn::storage::exec;
namespace logic = saturn::storage::exec::logic;

namespace {

/* Sectors per read: what sat_load_files reads at a time (SAT_LOAD_DEFAULT_CHUNK_BYTES). */
constexpr uint32_t kChunkSectors = 128u;
constexpr uint32_t kReadAttempts = 3u;

bool outside_staging(const void* p, uint32_t bytes) {
    return !logic::touches_work_ram_low(reinterpret_cast<uintptr_t>(p), bytes);
}

sat_result_t read_image(const sat_cd_device_t* device, uint32_t lba, uint32_t sectors) {
    uint8_t* destination = static_cast<uint8_t*>(exec_hal::staging());
    while (sectors != 0u) {
        const uint32_t count = sectors < kChunkSectors ? sectors : kChunkSectors;
        sat_result_t rc = SAT_ERR_IO;
        for (uint32_t attempt = 0; attempt < kReadAttempts && rc != SAT_OK; ++attempt) {
            rc = sat_cd_read_sectors(device, lba, count, destination);
        }
        if (rc != SAT_OK) return rc;
        lba += count;
        sectors -= count;
        destination += count * logic::kSectorBytes;
    }
    return SAT_OK;
}

}  // namespace

extern "C" sat_result_t sat_exec(sat_cdfs_volume_t* volume, const char* disc_path,
                                 const void* arg, uint32_t arg_bytes) {
    if (volume == nullptr || disc_path == nullptr || (arg == nullptr && arg_bytes != 0u)) {
        return SAT_ERR_INVALID_ARG;
    }
    if (arg_bytes > SAT_EXEC_ARG_MAX) return SAT_ERR_CAPACITY;
    const sat_cd_device_t* device = volume->device;
    if (volume->mounted == 0u || device == nullptr) return SAT_ERR_INVALID_ARG;
    /* The staging read overwrites Work RAM Low: whatever the read itself uses must be elsewhere. */
    sat_cd_block_t* block = exec_hal::bound_block(device);
    if (!outside_staging(volume, sizeof(*volume)) || !outside_staging(device, sizeof(*device)) ||
        (block != nullptr && !outside_staging(block, sizeof(*block)))) {
        return SAT_ERR_INVALID_ARG;
    }

    sat_cdfs_file_t file;
    SAT_TRY(sat_cdfs_lookup(volume, disc_path, &file));
    if (file.directory != 0u || file.size == 0u) return SAT_ERR_INVALID_ARG;
    if (file.size > SAT_EXEC_MAX_BYTES) return SAT_ERR_CAPACITY;
    uint32_t lba = 0u;
    uint32_t sectors = 0u;
    SAT_TRY(sat_cdfs_file_sectors(&file, 0u, 0u, &lba, &sectors));
    if (block != nullptr) {
        /* A background stream still landing would race the staging read. */
        SAT_TRY(sat_cd_stream_finish(block));
        (void)sat_cd_block_set_progress_service(block, nullptr, nullptr);
    }
    sat_cd_set_arrival_observer(nullptr, nullptr);

    /* The argument may sit in Work RAM Low too: seal it before the read. */
    logic::seal(*exec_hal::handoff(), arg, arg_bytes);
    exec_hal::stop_machine();
    if (read_image(device, lba, sectors) != SAT_OK) exec_hal::reset_machine();
    exec_hal::start_image(file.size);
}

extern "C" sat_result_t sat_exec_arg(void* out, uint32_t capacity, uint32_t* out_bytes) {
    if (out == nullptr && capacity != 0u) return SAT_ERR_INVALID_ARG;
    logic::Handoff* handoff = exec_hal::handoff();
    if (!logic::valid(*handoff)) return SAT_ERR_NOT_FOUND;
    if (out_bytes != nullptr) *out_bytes = handoff->bytes;
    if (handoff->bytes > capacity) return SAT_ERR_CAPACITY;
    uint8_t* bytes = static_cast<uint8_t*>(out);
    for (uint32_t i = 0; i < handoff->bytes; ++i) bytes[i] = handoff->data[i];
    logic::forget(*handoff);
    return SAT_OK;
}
