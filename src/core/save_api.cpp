#include "saturn/save.h"

#include <stdint.h>

#include "src/hal/bup.hpp"

namespace {

using saturn::hal::bup::Dir;
using saturn::hal::bup::Result;
using saturn::hal::bup::Stat;

bool g_save_initialized = false;

sat_result_t map_result(Result result) {
    switch (result) {
        case Result::Ok: return SAT_OK;
        case Result::NotConnected: return SAT_ERR_NOT_CONNECTED;
        case Result::Unformatted: return SAT_ERR_UNFORMATTED;
        case Result::WriteProtected: return SAT_ERR_WRITE_PROTECTED;
        case Result::NoSpace: return SAT_ERR_CAPACITY;
        case Result::NotFound: return SAT_ERR_NOT_FOUND;
        case Result::Found: return SAT_ERR_ALREADY_EXISTS;
        case Result::NoMatch: return SAT_ERR_VERIFY_FAILED;
        case Result::Broken: return SAT_ERR_IO;
        case Result::TransportError: return SAT_ERR_IO;
    }
    return SAT_ERR_IO;
}

sat_result_t map_device(sat_save_device_t device, uint32_t* out_device) {
    if (out_device == nullptr) return SAT_ERR_INVALID_ARG;
    switch (device) {
        case SAT_SAVE_INTERNAL:
            *out_device = 0u;
            return SAT_OK;
        case SAT_SAVE_BACKUP_CARTRIDGE:
            return SAT_ERR_UNSUPPORTED;
    }
    return SAT_ERR_INVALID_ARG;
}

uint32_t string_length_bounded(const char* text, uint32_t limit_plus_one) {
    if (text == nullptr) return 0u;
    uint32_t length = 0u;
    while (length < limit_plus_one && text[length] != '\0') ++length;
    return length;
}

sat_result_t validate_name(const char* name) {
    if (name == nullptr || name[0] == '\0') return SAT_ERR_INVALID_ARG;
    const uint32_t length = string_length_bounded(name, SAT_SAVE_NAME_MAX + 1u);
    return length <= SAT_SAVE_NAME_MAX && name[length] == '\0'
        ? SAT_OK : SAT_ERR_INVALID_ARG;
}

sat_result_t validate_pattern(const char* pattern) {
    if (pattern == nullptr || pattern[0] == '\0') return SAT_OK;
    const uint32_t length = string_length_bounded(pattern, SAT_SAVE_NAME_MAX + 1u);
    return length <= SAT_SAVE_NAME_MAX && pattern[length] == '\0'
        ? SAT_OK : SAT_ERR_INVALID_ARG;
}

sat_result_t validate_comment(const char* comment) {
    if (comment == nullptr) return SAT_OK;
    const uint32_t length = string_length_bounded(comment, SAT_SAVE_COMMENT_MAX + 1u);
    return length <= SAT_SAVE_COMMENT_MAX && comment[length] == '\0'
        ? SAT_OK : SAT_ERR_INVALID_ARG;
}

void copy_text(uint8_t* destination, uint32_t capacity, const char* source) {
    uint32_t i = 0u;
    if (source != nullptr) {
        while (i + 1u < capacity && source[i] != '\0') {
            destination[i] = static_cast<uint8_t>(source[i]);
            ++i;
        }
    }
    while (i < capacity) destination[i++] = 0u;
}

void copy_entry(sat_save_entry_t* destination, const Dir& source) {
    uint32_t i = 0u;
    for (; i < SAT_SAVE_NAME_MAX && source.filename[i] != 0u; ++i) {
        destination->name[i] = static_cast<char>(source.filename[i]);
    }
    destination->name[i] = '\0';

    i = 0u;
    for (; i < SAT_SAVE_COMMENT_MAX && source.comment[i] != 0u; ++i) {
        destination->comment[i] = static_cast<char>(source.comment[i]);
    }
    destination->comment[i] = '\0';

    destination->language = static_cast<sat_save_language_t>(source.language);
    destination->date = source.date;
    destination->data_size = source.data_size;
    destination->block_size = source.block_size;
    destination->reserved = 0u;
}

sat_result_t require_initialized() {
    return g_save_initialized ? SAT_OK : SAT_ERR_NOT_INITIALIZED;
}

sat_result_t stat_device(
    uint32_t device,
    uint32_t prospective_data_size,
    Stat* out_stat
) {
    return map_result(
        saturn::hal::bup::stat(device, prospective_data_size, out_stat));
}

sat_result_t find_entry(uint32_t device, const char* name, Dir* out_entry) {
    Stat storage{};
    SAT_TRY(stat_device(device, 0u, &storage));
    const int32_t count = saturn::hal::bup::directory(device, name, 1u, out_entry);
    return count == 0 ? SAT_ERR_NOT_FOUND : SAT_OK;
}

}  // namespace

extern "C" sat_result_t sat_save_init(void) {
    saturn::hal::bup::Config configs[3] = {};
    const sat_result_t result = map_result(saturn::hal::bup::init(configs));
    if (result == SAT_OK) g_save_initialized = true;
    return result;
}

extern "C" sat_result_t sat_save_storage_info(
    sat_save_device_t device,
    uint32_t prospective_data_size,
    sat_save_storage_info_t* out_info
) {
    if (out_info == nullptr) return SAT_ERR_INVALID_ARG;
    SAT_TRY(require_initialized());

    uint32_t bios_device = 0u;
    SAT_TRY(map_device(device, &bios_device));

    Stat stat{};
    SAT_TRY(stat_device(bios_device, prospective_data_size, &stat));
    out_info->total_size = stat.total_size;
    out_info->total_blocks = stat.total_blocks;
    out_info->block_size = stat.block_size;
    out_info->free_size = stat.free_size;
    out_info->free_blocks = stat.free_blocks;
    out_info->fit_count = stat.fit_count;
    return SAT_OK;
}

extern "C" sat_result_t sat_save_list(
    sat_save_device_t device,
    const char* pattern,
    sat_save_entry_t* entries,
    uint16_t capacity,
    uint16_t* out_total
) {
    if (out_total == nullptr || (capacity != 0u && entries == nullptr)) {
        return SAT_ERR_INVALID_ARG;
    }
    SAT_TRY(require_initialized());
    SAT_TRY(validate_pattern(pattern));

    uint32_t bios_device = 0u;
    SAT_TRY(map_device(device, &bios_device));

    Stat storage{};
    SAT_TRY(stat_device(bios_device, 0u, &storage));

    const char* effective_pattern =
        pattern == nullptr || pattern[0] == '\0' ? "*" : pattern;

    constexpr uint16_t kBatchCapacity = 32u;
    if (capacity > kBatchCapacity) return SAT_ERR_CAPACITY;

    Dir raw[kBatchCapacity] = {};
    const int32_t count = saturn::hal::bup::directory(
        bios_device, effective_pattern, capacity, capacity == 0u ? nullptr : raw);

    const uint32_t total = count < 0
        ? static_cast<uint32_t>(-static_cast<int64_t>(count))
        : static_cast<uint32_t>(count);
    if (total > 0xFFFFu) return SAT_ERR_CAPACITY;
    *out_total = static_cast<uint16_t>(total);

    uint32_t returned = total;
    if (returned > capacity) returned = capacity;
    for (uint32_t i = 0u; i < returned; ++i) {
        copy_entry(&entries[i], raw[i]);
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_save_read(
    sat_save_device_t device,
    const char* name,
    void* destination,
    uint32_t capacity,
    uint32_t* out_size
) {
    if (destination == nullptr || out_size == nullptr) return SAT_ERR_INVALID_ARG;
    SAT_TRY(require_initialized());
    SAT_TRY(validate_name(name));

    uint32_t bios_device = 0u;
    SAT_TRY(map_device(device, &bios_device));

    Dir entry{};
    SAT_TRY(find_entry(bios_device, name, &entry));
    *out_size = entry.data_size;
    if (capacity < entry.data_size) return SAT_ERR_CAPACITY;
    return map_result(saturn::hal::bup::read(bios_device, name, destination));
}

extern "C" sat_result_t sat_save_write(
    sat_save_device_t device,
    const sat_save_record_t* record,
    const void* data,
    uint32_t data_size,
    uint8_t overwrite
) {
    if (record == nullptr || data == nullptr || data_size == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    SAT_TRY(require_initialized());
    SAT_TRY(validate_name(record->name));
    SAT_TRY(validate_comment(record->comment));
    if (record->language < SAT_SAVE_JAPANESE || record->language > SAT_SAVE_ITALIAN) {
        return SAT_ERR_INVALID_ARG;
    }

    uint32_t bios_device = 0u;
    SAT_TRY(map_device(device, &bios_device));

    Stat storage{};
    SAT_TRY(stat_device(bios_device, data_size, &storage));

    Dir entry{};
    copy_text(entry.filename, sizeof(entry.filename), record->name);
    copy_text(entry.comment, sizeof(entry.comment), record->comment);
    entry.language = static_cast<uint8_t>(record->language);
    entry.date = record->date;
    entry.data_size = data_size;
    entry.block_size = 0u;

    return map_result(saturn::hal::bup::write(
        bios_device, &entry, data, overwrite != 0u ? 1u : 0u));
}

extern "C" sat_result_t sat_save_verify(
    sat_save_device_t device,
    const char* name,
    const void* data,
    uint32_t data_size
) {
    if (data == nullptr) return SAT_ERR_INVALID_ARG;
    SAT_TRY(require_initialized());
    SAT_TRY(validate_name(name));

    uint32_t bios_device = 0u;
    SAT_TRY(map_device(device, &bios_device));

    Dir entry{};
    SAT_TRY(find_entry(bios_device, name, &entry));
    if (entry.data_size != data_size) return SAT_ERR_VERIFY_FAILED;
    return map_result(saturn::hal::bup::verify(bios_device, name, data));
}

extern "C" sat_result_t sat_save_delete(
    sat_save_device_t device,
    const char* name
) {
    SAT_TRY(require_initialized());
    SAT_TRY(validate_name(name));

    uint32_t bios_device = 0u;
    SAT_TRY(map_device(device, &bios_device));
    return map_result(saturn::hal::bup::remove(bios_device, name));
}

extern "C" sat_result_t sat_save_format(sat_save_device_t device) {
    SAT_TRY(require_initialized());

    uint32_t bios_device = 0u;
    SAT_TRY(map_device(device, &bios_device));
    return map_result(saturn::hal::bup::format(bios_device));
}
