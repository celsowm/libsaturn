#include "saturn/loader.h"

#include "saturn/lz.h"
#include "src/storage/compression/lz_stream.hpp"

namespace {

namespace lz = saturn::storage::lz;

constexpr uint16_t kMaxItems = 32u;

struct Resolved {
    sat_cdfs_file_t file;
    uint32_t bytes;
};

struct LzLoad {
    lz::Stream stream;
    uint8_t* base;
    uint8_t* packed;
    uint32_t file_bytes;
    uint32_t capacity;
    uint32_t expected;
    bool begun;
    sat_result_t status;
};

/* Decodes what has arrived of the packed file, up to `end`. */
void on_lz_arrival(void* context, const void* end) {
    LzLoad& load = *static_cast<LzLoad*>(context);
    const uint8_t* const arrived = static_cast<const uint8_t*>(end);
    if (load.status != SAT_OK || arrived <= load.packed || arrived > load.packed + load.file_bytes) {
        return;
    }
    if (!load.begun) {
        if (static_cast<uint32_t>(arrived - load.packed) < SAT_LZ_HEADER_BYTES) return;
        sat_lz_header_t header;
        load.status = sat_lz_read_header(load.packed, &header);
        if (load.status != SAT_OK) return;
        if (load.expected != 0u && header.decoded != load.expected) {
            load.status = SAT_ERR_VERSION;
            return;
        }
        if (header.decoded > load.capacity || header.margin > load.capacity - header.decoded) {
            load.status = SAT_ERR_CAPACITY;
            return;
        }
        load.status = lz::begin(load.stream, load.packed, load.file_bytes, load.base, header.decoded);
        if (load.status != SAT_OK) return;
        load.begun = true;
    }
    load.status = lz::step(load.stream, arrived);
}

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

extern "C" sat_result_t sat_load_lz(
    sat_cdfs_volume_t* volume,
    const char* disc_path,
    void* destination, uint32_t capacity,
    uint32_t expected_decoded,
    sat_load_progress_fn progress, void* context,
    uint32_t* out_bytes) {
    if (out_bytes != nullptr) *out_bytes = 0u;
    if (volume == nullptr || disc_path == nullptr || destination == nullptr ||
        (reinterpret_cast<uintptr_t>(destination) & 3u) != 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    sat_cdfs_file_t file;
    SAT_TRY(sat_cdfs_lookup(volume, disc_path, &file));
    if (file.directory != 0u) return SAT_ERR_INVALID_ARG;
    if (file.size < SAT_LZ_HEADER_BYTES) return SAT_ERR_VERSION;
    if (file.size > capacity) return SAT_ERR_CAPACITY;

    // The file goes at the end; the decoder writes from the start and never
    // overtakes its input when capacity >= decoded + margin (tools/satlz.py).
    // It decodes each batch of sectors as it arrives, while the drive reads
    // the next, and whatever is left once the read is done.
    LzLoad load = {};
    load.base = static_cast<uint8_t*>(destination);
    load.packed = load.base + ((capacity - file.size) & ~3u);
    load.file_bytes = file.size;
    load.capacity = capacity;
    load.expected = expected_decoded;
    const sat_load_item_t item = { disc_path, file.size, 0u, file.size, load.packed };
    sat_cd_set_arrival_observer(on_lz_arrival, &load);
    const sat_result_t read = sat_load_files(volume, &item, 1u, 0u, progress, context);
    sat_cd_set_arrival_observer(nullptr, nullptr);
    SAT_TRY(read);

    on_lz_arrival(&load, load.packed + file.size);
    SAT_TRY(load.status);
    if (!load.begun || !lz::done(load.stream)) return SAT_ERR_IO;
    if (out_bytes != nullptr) *out_bytes = static_cast<uint32_t>(load.stream.out_end - load.base);
    return SAT_OK;
}
