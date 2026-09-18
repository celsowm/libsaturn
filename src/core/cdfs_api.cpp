#include "saturn/cdfs.h"

#include <stddef.h>

namespace {

constexpr uint8_t kDirectoryFlag = 0x02u;
constexpr uint32_t kPrimaryVolumeDescriptorLba = 16u;

uint32_t le32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

bool ascii_equal_ci(uint8_t a, char b) {
    if (a >= 'a' && a <= 'z') a = static_cast<uint8_t>(a - ('a' - 'A'));
    if (b >= 'a' && b <= 'z') b = static_cast<char>(b - ('a' - 'A'));
    return a == static_cast<uint8_t>(b);
}

uint16_t iso_name_length(const uint8_t* name, uint16_t length) {
    while (length != 0u && name[length - 1u] == ';') --length;
    if (length >= 2u && name[length - 2u] == ';' && name[length - 1u] == '1') {
        length = static_cast<uint16_t>(length - 2u);
    }
    return length;
}

bool component_equal(const uint8_t* iso_name, uint16_t iso_length, const char* component,
                     uint16_t component_length) {
    const uint16_t visible_length = iso_name_length(iso_name, iso_length);
    if (visible_length != component_length) return false;
    for (uint16_t i = 0u; i < visible_length; ++i) {
        if (!ascii_equal_ci(iso_name[i], component[i])) return false;
    }
    return true;
}

sat_result_t read_sector(sat_cdfs_volume_t* volume, uint32_t lba) {
    if (volume == nullptr || volume->device == nullptr) return SAT_ERR_NOT_INITIALIZED;
    return sat_cd_read_sectors(volume->device, lba, 1u, volume->sector_buffer);
}

sat_result_t validate_volume(const sat_cdfs_volume_t* volume) {
    if (volume == nullptr || volume->mounted == 0u || volume->device == nullptr) {
        return SAT_ERR_NOT_INITIALIZED;
    }
    return SAT_OK;
}

sat_result_t validate_path(const char* path) {
    if (path == nullptr || path[0] == '\0') return SAT_ERR_INVALID_ARG;
    uint16_t total_length = 0u;
    const char* component = path;
    for (;;) {
        const char* end = component;
        while (*end != '\0' && *end != '/') ++end;
        const uint16_t length = static_cast<uint16_t>(end - component);
        if (length == 0u || (length == 1u && component[0] == '.') ||
            (length == 2u && component[0] == '.' && component[1] == '.')) {
            return SAT_ERR_INVALID_ARG;
        }
        total_length = static_cast<uint16_t>(total_length + length);
        if (total_length >= SAT_CDFS_PATH_MAX) return SAT_ERR_CAPACITY;
        if (*end == '\0') break;
        component = end + 1;
    }
    return SAT_OK;
}

sat_result_t find_child(
    sat_cdfs_volume_t* volume,
    const sat_cdfs_file_t& directory,
    const char* component,
    uint16_t component_length,
    sat_cdfs_file_t* out_file
) {
    if ((directory.directory & 1u) == 0u) return SAT_ERR_INVALID_ARG;
    const uint32_t sector_count =
        (directory.size + SAT_CD_SECTOR_BYTES - 1u) / SAT_CD_SECTOR_BYTES;
    uint32_t remaining = directory.size;
    for (uint32_t sector = 0u; sector < sector_count; ++sector) {
        SAT_TRY(read_sector(volume, directory.extent_lba + sector));
        const uint32_t valid_bytes = remaining < SAT_CD_SECTOR_BYTES
            ? remaining : SAT_CD_SECTOR_BYTES;
        uint32_t offset = 0u;
        while (offset < valid_bytes) {
            const uint8_t length = volume->sector_buffer[offset];
            if (length == 0u) break;
            if (length < 34u || offset + length > valid_bytes) return SAT_ERR_IO;
            const uint8_t name_length = volume->sector_buffer[offset + 32u];
            if (static_cast<uint32_t>(33u + name_length) > length) return SAT_ERR_IO;
            if (name_length != 1u && component_equal(
                    volume->sector_buffer + offset + 33u, name_length,
                    component, component_length)) {
                out_file->extent_lba = le32(volume->sector_buffer + offset + 2u);
                out_file->size = le32(volume->sector_buffer + offset + 10u);
                out_file->directory = (volume->sector_buffer[offset + 25u] & kDirectoryFlag) != 0u;
                out_file->reserved[0] = out_file->reserved[1] = out_file->reserved[2] = 0u;
                return SAT_OK;
            }
            offset += length;
        }
        remaining -= valid_bytes;
    }
    return SAT_ERR_NOT_FOUND;
}

}  // namespace

extern "C" sat_result_t sat_cdfs_mount(
    sat_cdfs_volume_t* out_volume,
    const sat_cd_device_t* device
) {
    if (out_volume == nullptr || device == nullptr) return SAT_ERR_INVALID_ARG;
    out_volume->device = device;
    out_volume->mounted = 0u;
    SAT_TRY(read_sector(out_volume, kPrimaryVolumeDescriptorLba));
    const uint8_t* pvd = out_volume->sector_buffer;
    if (pvd[0] != 1u || pvd[1] != 'C' || pvd[2] != 'D' || pvd[3] != '0' ||
        pvd[4] != '0' || pvd[5] != '1' || pvd[6] != 1u) {
        out_volume->device = nullptr;
        return SAT_ERR_IO;
    }
    const uint8_t root_length = pvd[156u];
    if (root_length < 34u || 156u + root_length > SAT_CD_SECTOR_BYTES) {
        out_volume->device = nullptr;
        return SAT_ERR_IO;
    }
    out_volume->root.extent_lba = le32(pvd + 158u);
    out_volume->root.size = le32(pvd + 166u);
    out_volume->root.directory = 1u;
    out_volume->root.reserved[0] = out_volume->root.reserved[1] = out_volume->root.reserved[2] = 0u;
    out_volume->mounted = 1u;
    return SAT_OK;
}

extern "C" sat_result_t sat_cdfs_unmount(sat_cdfs_volume_t* volume) {
    if (volume == nullptr) return SAT_ERR_INVALID_ARG;
    volume->device = nullptr;
    volume->root = {};
    volume->mounted = 0u;
    return SAT_OK;
}

extern "C" sat_result_t sat_cdfs_lookup(
    sat_cdfs_volume_t* volume,
    const char* path,
    sat_cdfs_file_t* out_file
) {
    SAT_TRY(validate_volume(volume));
    if (out_file == nullptr) return SAT_ERR_INVALID_ARG;
    SAT_TRY(validate_path(path));
    sat_cdfs_file_t current = volume->root;
    const char* begin = path;
    for (;;) {
        const char* end = begin;
        while (*end != '\0' && *end != '/') ++end;
        const uint16_t length = static_cast<uint16_t>(end - begin);
        SAT_TRY(find_child(volume, current, begin, length, &current));
        if (*end == '\0') {
            *out_file = current;
            return SAT_OK;
        }
        if ((current.directory & 1u) == 0u) return SAT_ERR_NOT_FOUND;
        begin = end + 1;
    }
}

extern "C" sat_result_t sat_cdfs_read_at(
    sat_cdfs_volume_t* volume,
    const sat_cdfs_file_t* file,
    uint32_t offset,
    void* destination,
    uint32_t bytes,
    uint32_t* out_read
) {
    SAT_TRY(validate_volume(volume));
    if (file == nullptr || out_read == nullptr || (destination == nullptr && bytes != 0u)) {
        return SAT_ERR_INVALID_ARG;
    }
    *out_read = 0u;
    if (offset >= file->size || bytes == 0u) return SAT_OK;
    uint32_t remaining = bytes < file->size - offset ? bytes : file->size - offset;
    uint32_t position = offset;
    uint8_t* output = static_cast<uint8_t*>(destination);
    while (remaining != 0u) {
        const uint32_t sector_index = position / SAT_CD_SECTOR_BYTES;
        const uint32_t sector_offset = position % SAT_CD_SECTOR_BYTES;
        uint32_t chunk = (remaining < SAT_CD_SECTOR_BYTES - sector_offset)
            ? remaining : SAT_CD_SECTOR_BYTES - sector_offset;
        if (sector_offset == 0u && remaining >= SAT_CD_SECTOR_BYTES) {
            const uint32_t full_sectors = remaining / SAT_CD_SECTOR_BYTES;
            SAT_TRY(sat_cd_read_sectors(
                volume->device, file->extent_lba + sector_index, full_sectors, output));
            chunk = full_sectors * SAT_CD_SECTOR_BYTES;
        } else {
            SAT_TRY(read_sector(volume, file->extent_lba + sector_index));
            for (uint32_t i = 0u; i < chunk; ++i) {
                output[i] = volume->sector_buffer[sector_offset + i];
            }
        }
        output += chunk;
        position += chunk;
        remaining -= chunk;
        *out_read += chunk;
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_cdfs_file_read_at(
    void* context,
    uint32_t offset,
    void* destination,
    uint32_t bytes,
    uint32_t* out_read
) {
    if (context == nullptr) return SAT_ERR_INVALID_ARG;
    sat_cdfs_file_source_t* source = static_cast<sat_cdfs_file_source_t*>(context);
    return sat_cdfs_read_at(source->volume, &source->file, offset, destination, bytes, out_read);
}
