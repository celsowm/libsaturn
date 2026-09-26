#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "saturn/save.h"
#include "src/hal/storage/backup.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

namespace saturn::hal::bup {

static Result g_init_result = Result::Ok;
static Result g_stat_result = Result::Ok;
static Result g_write_result = Result::Ok;
static Result g_read_result = Result::Ok;
static Result g_remove_result = Result::Ok;
static Result g_verify_result = Result::Ok;
static Result g_format_result = Result::Ok;
static int32_t g_directory_count = 1;
static uint8_t g_last_overwrite = 0xFFu;
static uint32_t g_format_calls = 0u;
static uint32_t g_write_calls = 0u;
static uint16_t g_last_directory_capacity = 0u;
static uint32_t g_stat_calls = 0u;
static uint32_t g_directory_calls = 0u;
static uint16_t g_cart_index = 0xFFFFu;
static uint16_t g_cart_partitions = 0u;
static uint32_t g_last_device = 0xFFFFu;
static uint32_t g_device_calls[3] = {0u, 0u, 0u};

// Every BIOS call must name a device that exists: the internal 0, or the
// cartridge's Config index. The BIOS-side selector is what these record.
static bool device_ok(uint32_t device) {
    if (device >= 3u) return false;
    if (device != 0u && device != g_cart_index) return false;
    g_last_device = device;
    ++g_device_calls[device];
    return true;
}
static Dir g_dir = {};
static Stat g_stat = {32768u, 512u, 64u, 30000u, 480u, 12u};

Result init(Config out_configs[3]) {
    if (g_init_result != Result::Ok) return g_init_result;
    // Config[0] identifies the internal unit by unit_id=1, while BUP
    // operations MUST use configuration-table device index 0.
    out_configs[0] = {1u, 1u};
    out_configs[1] = {};
    out_configs[2] = {};
    if (g_cart_index < 3u) {
        out_configs[g_cart_index] = {2u, g_cart_partitions};
    }
    return Result::Ok;
}

Result select_partition(uint32_t, uint16_t) { return Result::Ok; }

Result format(uint32_t device) {
    OK(device_ok(device));
    ++g_format_calls;
    return g_format_result;
}

Result stat(uint32_t device, uint32_t, Stat* out_stat) {
    OK(device_ok(device));
    ++g_stat_calls;
    if (g_stat_result != Result::Ok) return g_stat_result;
    *out_stat = g_stat;
    return Result::Ok;
}

static char g_last_pattern[16] = {};
int32_t directory(uint32_t device, const char* pattern, uint16_t capacity, Dir* out_entries) {
    OK(device_ok(device));
    std::strncpy(g_last_pattern, pattern, sizeof(g_last_pattern) - 1u);
    ++g_directory_calls;
    g_last_directory_capacity = capacity;
    if (capacity != 0u && out_entries != nullptr && g_directory_count != 0) {
        out_entries[0] = g_dir;
        if (capacity > 1u && (g_directory_count > 1 || g_directory_count < -1)) {
            out_entries[1] = g_dir;
            out_entries[1].filename[0] = 'B';
        }
    }
    return g_directory_count;
}

Result write(uint32_t device, Dir*, const void*, uint8_t overwrite) {
    OK(device_ok(device));
    ++g_write_calls;
    g_last_overwrite = overwrite;
    return g_write_result;
}

Result read(uint32_t device, const char*, void* data) {
    OK(device_ok(device));
    if (g_read_result == Result::Ok) {
        static const uint8_t payload[4] = {1u, 2u, 3u, 4u};
        std::memcpy(data, payload, sizeof(payload));
    }
    return g_read_result;
}

Result remove(uint32_t device, const char*) {
    OK(device_ok(device));
    return g_remove_result;
}

Result verify(uint32_t device, const char*, const void*) {
    OK(device_ok(device));
    return g_verify_result;
}

}  // namespace saturn::hal::bup

int main() {
    using namespace saturn::hal::bup;

    sat_save_storage_info_t info{};
    sat_save_device_info_t device_info{};
    OK(sat_save_device_info(SAT_SAVE_BACKUP_CARTRIDGE, &device_info) ==
       SAT_ERR_NOT_INITIALIZED);
    OK(sat_save_storage_info(SAT_SAVE_INTERNAL, 0u, &info) == SAT_ERR_NOT_INITIALIZED);

    g_init_result = Result::NotConnected;
    OK(sat_save_init() == SAT_ERR_NOT_CONNECTED);
    g_init_result = Result::Ok;
    OK(sat_save_init() == SAT_OK);

    OK(g_format_calls == 0u);

    // The read-only query inspects the BUP_Init Config snapshot. It must
    // never call BUP_Stat, BUP_Dir, or any mutating BIOS operation.
    OK(sat_save_device_info(SAT_SAVE_INTERNAL, nullptr) == SAT_ERR_INVALID_ARG);
    OK(sat_save_device_info(static_cast<sat_save_device_t>(99), &device_info) ==
       SAT_ERR_INVALID_ARG);
    OK(sat_save_device_info(SAT_SAVE_INTERNAL, &device_info) == SAT_OK &&
       device_info.connected == 1u && device_info.partition_count == 1u);
    OK(sat_save_device_info(SAT_SAVE_BACKUP_CARTRIDGE, &device_info) == SAT_OK &&
       device_info.connected == 0u && device_info.partition_count == 0u);
    OK(g_stat_calls == 0u && g_directory_calls == 0u &&
       g_write_calls == 0u && g_format_calls == 0u);

    // Simulate each possible non-internal Config entry. Unit ID 2 is
    // descriptive; it must not be mistaken for the BIOS call selector.
    g_cart_index = 2u;
    g_cart_partitions = 3u;
    OK(sat_save_init() == SAT_OK);
    OK(sat_save_device_info(SAT_SAVE_BACKUP_CARTRIDGE, &device_info) == SAT_OK &&
       device_info.connected == 1u && device_info.partition_count == 3u);
    // The cartridge is now a real device: its BIOS selector is its Config index,
    // never the unit ID and never the internal 0.
    OK(sat_save_storage_info(SAT_SAVE_BACKUP_CARTRIDGE, 0u, &info) == SAT_OK);
    OK(g_last_device == 2u && g_device_calls[0] == 0u && g_device_calls[2] == 1u);
    g_cart_index = 1u;
    g_cart_partitions = 2u;
    OK(sat_save_init() == SAT_OK);
    OK(sat_save_device_info(SAT_SAVE_BACKUP_CARTRIDGE, &device_info) == SAT_OK &&
       device_info.connected == 1u && device_info.partition_count == 2u);
    g_cart_partitions = 0u;
    OK(sat_save_init() == SAT_OK);
    OK(sat_save_device_info(SAT_SAVE_BACKUP_CARTRIDGE, &device_info) == SAT_OK &&
       device_info.connected == 0u && device_info.partition_count == 0u);
    g_cart_index = 0xFFFFu;
    g_cart_partitions = 0u;
    OK(sat_save_init() == SAT_OK);
    OK(g_stat_calls == 1u /* only the explicit cartridge storage_info */ && g_directory_calls == 0u &&
       g_write_calls == 0u && g_format_calls == 0u);

    OK(sat_save_storage_info(SAT_SAVE_INTERNAL, 128u, &info) == SAT_OK);
    OK(info.total_size == 32768u && info.total_blocks == 512u &&
       info.block_size == 64u && info.free_size == 30000u &&
       info.free_blocks == 480u && info.fit_count == 12u);

    g_stat_result = Result::Unformatted;
    OK(sat_save_storage_info(SAT_SAVE_INTERNAL, 0u, &info) == SAT_ERR_UNFORMATTED);
    g_stat_result = Result::WriteProtected;
    OK(sat_save_storage_info(SAT_SAVE_INTERNAL, 0u, &info) == SAT_ERR_WRITE_PROTECTED);
    g_stat_result = Result::Ok;

    std::memset(&g_dir, 0, sizeof(g_dir));
    std::memcpy(g_dir.filename, "SAVE_A", 6u);
    std::memcpy(g_dir.comment, "slot one", 8u);
    g_dir.language = 1u;
    g_dir.date = 1234u;
    g_dir.data_size = 4u;
    g_dir.block_size = 2u;

    sat_save_entry_t entries[2] = {};
    uint16_t total = 0u;
    g_directory_count = -3;
    OK(sat_save_list(SAT_SAVE_INTERNAL, "*", entries, 2u, &total) == SAT_OK);
    // BUP_Dir has no wildcard: "*", empty and null all mean an empty name (every file).
    OK(g_last_pattern[0] == 0);
    OK(sat_save_list(SAT_SAVE_INTERNAL, nullptr, entries, 2u, &total) == SAT_OK && g_last_pattern[0] == 0);
    OK(sat_save_list(SAT_SAVE_INTERNAL, "SAVE_A", entries, 2u, &total) == SAT_OK);
    OK(std::strcmp(g_last_pattern, "SAVE_A") == 0);
    OK(total == 3u && std::strcmp(entries[0].name, "SAVE_A") == 0 &&
       std::strcmp(entries[0].comment, "slot one") == 0 &&
       entries[0].data_size == 4u && entries[0].block_size == 2u);
    OK(entries[1].name[0] == 'B');

    OK(sat_save_list(SAT_SAVE_INTERNAL, nullptr, nullptr, 0u, &total) == SAT_OK &&
       total == 3u && g_last_directory_capacity == 1u);
    OK(sat_save_list(SAT_SAVE_INTERNAL, "123456789012", entries, 2u, &total) ==
       SAT_ERR_INVALID_ARG);

    g_directory_count = 1;
    uint8_t read_buffer[4] = {};
    uint32_t read_size = 0u;
    OK(sat_save_read(SAT_SAVE_INTERNAL, "SAVE_A", read_buffer, 3u, &read_size) ==
       SAT_ERR_CAPACITY && read_size == 4u);
    OK(sat_save_read(SAT_SAVE_INTERNAL, "SAVE_A", read_buffer, sizeof(read_buffer),
                     &read_size) == SAT_OK);
    OK(read_size == 4u && read_buffer[0] == 1u && read_buffer[3] == 4u);

    g_directory_count = 0;
    OK(sat_save_read(SAT_SAVE_INTERNAL, "SAVE_A", read_buffer, sizeof(read_buffer),
                     &read_size) == SAT_ERR_NOT_FOUND);
    g_directory_count = 1;

    const uint8_t payload[4] = {1u, 2u, 3u, 4u};
    sat_save_record_t record{"SAVE_A", "slot one", SAT_SAVE_ENGLISH, 1234u};

    g_write_result = Result::Found;
    OK(sat_save_write(SAT_SAVE_INTERNAL, &record, payload, sizeof(payload), 0u) ==
       SAT_ERR_ALREADY_EXISTS);
    OK(g_last_overwrite == 0u);

    g_write_result = Result::Ok;
    OK(sat_save_write(SAT_SAVE_INTERNAL, &record, payload, sizeof(payload), 1u) == SAT_OK);
    OK(g_last_overwrite == 1u && g_write_calls >= 2u);

    sat_save_record_t too_long{"123456789012", "", SAT_SAVE_ENGLISH, 0u};
    OK(sat_save_write(SAT_SAVE_INTERNAL, &too_long, payload, sizeof(payload), 1u) ==
       SAT_ERR_INVALID_ARG);
    sat_save_record_t bad_comment{"SAVE_A", "12345678901", SAT_SAVE_ENGLISH, 0u};
    OK(sat_save_write(SAT_SAVE_INTERNAL, &bad_comment, payload, sizeof(payload), 1u) ==
       SAT_ERR_INVALID_ARG);

    g_verify_result = Result::NoMatch;
    OK(sat_save_verify(SAT_SAVE_INTERNAL, "SAVE_A", payload, sizeof(payload)) ==
       SAT_ERR_VERIFY_FAILED);
    g_verify_result = Result::Ok;
    OK(sat_save_verify(SAT_SAVE_INTERNAL, "SAVE_A", payload, 3u) ==
       SAT_ERR_VERIFY_FAILED);
    OK(sat_save_verify(SAT_SAVE_INTERNAL, "SAVE_A", payload, sizeof(payload)) == SAT_OK);

    g_remove_result = Result::NotFound;
    OK(sat_save_delete(SAT_SAVE_INTERNAL, "SAVE_A") == SAT_ERR_NOT_FOUND);
    g_remove_result = Result::Ok;
    OK(sat_save_delete(SAT_SAVE_INTERNAL, "SAVE_A") == SAT_OK);

    g_format_result = Result::Ok;
    OK(sat_save_format(SAT_SAVE_INTERNAL) == SAT_OK && g_format_calls == 1u);

    // No cartridge: refused before any BIOS call (no stat, no format, no write).
    {
        const uint32_t stats = g_stat_calls, dirs = g_directory_calls, writes = g_write_calls,
                       formats = g_format_calls;
        OK(g_cart_index == 0xFFFFu);
        OK(sat_save_storage_info(SAT_SAVE_BACKUP_CARTRIDGE, 0u, &info) == SAT_ERR_NOT_CONNECTED);
        OK(sat_save_format(SAT_SAVE_BACKUP_CARTRIDGE) == SAT_ERR_NOT_CONNECTED);
        OK(sat_save_delete(SAT_SAVE_BACKUP_CARTRIDGE, "SAVE_A") == SAT_ERR_NOT_CONNECTED);
        sat_save_record_t rec{"SAVE_A", "cmt", SAT_SAVE_ENGLISH, 0u};
        OK(sat_save_write(SAT_SAVE_BACKUP_CARTRIDGE, &rec, payload, sizeof(payload), 1u) ==
           SAT_ERR_NOT_CONNECTED);
        OK(g_stat_calls == stats && g_directory_calls == dirs && g_write_calls == writes &&
           g_format_calls == formats);
    }

    // With a cartridge every operation goes to its selector, and the internal
    // device keeps using 0.
    g_cart_index = 1u;
    g_cart_partitions = 1u;
    OK(sat_save_init() == SAT_OK);
    {
        for (uint32_t& c : g_device_calls) c = 0u;
        sat_save_entry_t entries[2] = {};
        uint16_t total = 0u;
        uint8_t buffer[8] = {};
        uint32_t size = 0u;
        sat_save_record_t rec{"SAVE_A", "cmt", SAT_SAVE_ENGLISH, 0u};
        g_directory_count = 1;
        OK(sat_save_storage_info(SAT_SAVE_BACKUP_CARTRIDGE, 0u, &info) == SAT_OK);
        OK(sat_save_list(SAT_SAVE_BACKUP_CARTRIDGE, "*", entries, 2u, &total) == SAT_OK);
        OK(sat_save_write(SAT_SAVE_BACKUP_CARTRIDGE, &rec, payload, sizeof(payload), 1u) == SAT_OK);
        OK(sat_save_read(SAT_SAVE_BACKUP_CARTRIDGE, "SAVE_A", buffer, sizeof(buffer), &size) == SAT_OK);
        OK(sat_save_verify(SAT_SAVE_BACKUP_CARTRIDGE, "SAVE_A", payload, sizeof(payload)) == SAT_OK);
        OK(sat_save_delete(SAT_SAVE_BACKUP_CARTRIDGE, "SAVE_A") == SAT_OK);
        OK(sat_save_format(SAT_SAVE_BACKUP_CARTRIDGE) == SAT_OK);
        OK(g_device_calls[0] == 0u && g_device_calls[1] > 6u);
        // ... and an internal call afterwards does not drift to the cartridge.
        for (uint32_t& c : g_device_calls) c = 0u;
        OK(sat_save_storage_info(SAT_SAVE_INTERNAL, 0u, &info) == SAT_OK);
        OK(g_device_calls[0] >= 1u && g_device_calls[1] == 0u);
    }

    std::puts("save api: OK");
    return 0;
}
