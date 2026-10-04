#include "saturn/loader.h"

namespace {

constexpr uint16_t kMaxItems = 32u;

struct Resolved {
    sat_cdfs_file_t file;
    uint32_t bytes;
};

}  // namespace

extern "C" sat_result_t sat_load_files(
    sat_cdfs_volume_t* volume,
    const sat_load_item_t* items, uint16_t count,
    uint32_t chunk_bytes,
    sat_load_progress_fn progress, void* context) {
    if (volume == nullptr || items == nullptr || count == 0u || count > kMaxItems) {
        return SAT_ERR_INVALID_ARG;
    }
    if (chunk_bytes == 0u) chunk_bytes = SAT_LOAD_DEFAULT_CHUNK_BYTES;
    chunk_bytes -= chunk_bytes % SAT_CD_SECTOR_BYTES;
    if (chunk_bytes == 0u) chunk_bytes = SAT_CD_SECTOR_BYTES;

    Resolved resolved[kMaxItems];
    uint32_t total = 0u;
    for (uint16_t i = 0u; i < count; ++i) {
        const sat_load_item_t& item = items[i];
        if (item.disc_path == nullptr || item.destination == nullptr) return SAT_ERR_INVALID_ARG;
        SAT_TRY(sat_cdfs_lookup(volume, item.disc_path, &resolved[i].file));
        const sat_cdfs_file_t& file = resolved[i].file;
        if (file.directory != 0u) return SAT_ERR_INVALID_ARG;
        if (item.expected_size != 0u && file.size != item.expected_size) return SAT_ERR_VERSION;
        if (item.offset > file.size) return SAT_ERR_INVALID_ARG;
        const uint32_t bytes = item.bytes != 0u ? item.bytes : file.size - item.offset;
        if (bytes > file.size - item.offset || bytes > 0xFFFFFFFFu - total) {
            return SAT_ERR_INVALID_ARG;
        }
        resolved[i].bytes = bytes;
        total += bytes;
    }

    uint32_t done = 0u;
    if (progress != nullptr) progress(context, 0u, total);
    for (uint16_t i = 0u; i < count; ++i) {
        uint8_t* destination = static_cast<uint8_t*>(items[i].destination);
        uint32_t offset = items[i].offset;
        uint32_t remaining = resolved[i].bytes;
        while (remaining != 0u) {
            // Keep every chunk after the first on a sector boundary: a whole-sector
            // read streams in one drive command, a partial one goes through a buffer.
            uint32_t chunk = chunk_bytes - offset % SAT_CD_SECTOR_BYTES;
            if (chunk > remaining) chunk = remaining;
            uint32_t got = 0u;
            SAT_TRY(sat_cdfs_read_at(volume, &resolved[i].file, offset, destination, chunk, &got));
            if (got != chunk) return SAT_ERR_IO;
            destination += chunk;
            offset += chunk;
            remaining -= chunk;
            done += chunk;
            if (progress != nullptr) progress(context, done, total);
        }
    }
    return SAT_OK;
}
