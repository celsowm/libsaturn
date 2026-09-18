#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "saturn/cdfs.h"
#include "saturn/file.h"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

namespace {

constexpr uint32_t kSectorCount = 32u;
uint8_t g_image[kSectorCount * SAT_CD_SECTOR_BYTES] = {};

void put_le32(uint8_t* destination, uint32_t value) {
    destination[0] = static_cast<uint8_t>(value);
    destination[1] = static_cast<uint8_t>(value >> 8);
    destination[2] = static_cast<uint8_t>(value >> 16);
    destination[3] = static_cast<uint8_t>(value >> 24);
}

uint8_t add_record(uint8_t* directory, uint16_t offset, uint32_t lba, uint32_t size,
                  uint8_t flags, const char* name, uint8_t name_length) {
    uint8_t length = static_cast<uint8_t>(33u + name_length + ((name_length & 1u) == 0u ? 1u : 0u));
    directory[offset] = length;
    put_le32(directory + offset + 2u, lba);
    put_le32(directory + offset + 10u, size);
    directory[offset + 25u] = flags;
    directory[offset + 32u] = name_length;
    for (uint8_t i = 0u; i < name_length; ++i) directory[offset + 33u + i] = name[i];
    return length;
}

sat_result_t read_image(void*, uint32_t lba, uint32_t count, void* destination) {
    if (destination == nullptr || count == 0u || lba >= kSectorCount ||
        count > kSectorCount - lba) return SAT_ERR_IO;
    std::memcpy(destination, g_image + lba * SAT_CD_SECTOR_BYTES,
                count * SAT_CD_SECTOR_BYTES);
    return SAT_OK;
}

void make_image() {
    std::memset(g_image, 0, sizeof(g_image));
    uint8_t* pvd = g_image + 16u * SAT_CD_SECTOR_BYTES;
    pvd[0] = 1u;
    pvd[1] = 'C'; pvd[2] = 'D'; pvd[3] = '0'; pvd[4] = '0'; pvd[5] = '1'; pvd[6] = 1u;
    pvd[156u] = 34u;
    put_le32(pvd + 158u, 20u);
    put_le32(pvd + 166u, SAT_CD_SECTOR_BYTES);
    pvd[181u] = 2u;
    pvd[182u] = 0u;

    uint8_t* root = g_image + 20u * SAT_CD_SECTOR_BYTES;
    uint16_t offset = 0u;
    offset = static_cast<uint16_t>(offset + add_record(root, offset, 20u, SAT_CD_SECTOR_BYTES, 2u, "\0", 1u));
    offset = static_cast<uint16_t>(offset + add_record(root, offset, 20u, SAT_CD_SECTOR_BYTES, 2u, "\1", 1u));
    offset = static_cast<uint16_t>(offset + add_record(root, offset, 21u, SAT_CD_SECTOR_BYTES, 2u, "ASSETS", 6u));
    (void)add_record(root, offset, 22u, 5u, 0u, "ROOT.TXT;1", 10u);

    uint8_t* assets = g_image + 21u * SAT_CD_SECTOR_BYTES;
    (void)add_record(assets, 0u, 21u, SAT_CD_SECTOR_BYTES, 2u, "\0", 1u);
    (void)add_record(assets, 34u, 21u, SAT_CD_SECTOR_BYTES, 2u, "\1", 1u);
    (void)add_record(assets, 68u, 23u, SAT_CD_SECTOR_BYTES + 2u, 0u, "PLAYER.BIN;1", 12u);
    for (uint32_t i = 0u; i < SAT_CD_SECTOR_BYTES; ++i) g_image[23u * SAT_CD_SECTOR_BYTES + i] = static_cast<uint8_t>(i);
    g_image[24u * SAT_CD_SECTOR_BYTES] = 0xA0u;
    g_image[24u * SAT_CD_SECTOR_BYTES + 1u] = 0xA1u;
    std::memcpy(g_image + 22u * SAT_CD_SECTOR_BYTES, "hello", 5u);
}

}  // namespace

int main() {
    make_image();
    sat_cd_device_t device{};
    OK(sat_cd_device_init(&device, read_image, nullptr, kSectorCount) == SAT_OK);
    sat_cdfs_volume_t volume{};
    OK(sat_cdfs_mount(&volume, &device) == SAT_OK);

    sat_cdfs_file_t file{};
    OK(sat_cdfs_lookup(&volume, "assets/player.bin", &file) == SAT_OK);
    OK(file.extent_lba == 23u && file.size == SAT_CD_SECTOR_BYTES + 2u && file.directory == 0u);
    uint8_t output[4] = {};
    uint32_t read = 0u;
    OK(sat_cdfs_read_at(&volume, &file, SAT_CD_SECTOR_BYTES - 2u,
                        output, sizeof(output), &read) == SAT_OK && read == 4u);
    OK(output[0] == 0xFEu && output[1] == 0xFFu && output[2] == 0xA0u && output[3] == 0xA1u);

    sat_cdfs_file_source_t source{&volume, file};
    std::memset(output, 0, sizeof(output));
    OK(sat_cdfs_file_read_at(&source, 0u, output, sizeof(output), &read) == SAT_OK &&
       read == sizeof(output) && output[0] == 0u && output[3] == 3u);
    OK(sat_file_reset() == SAT_OK);
    OK(sat_file_register_backend("assets/player.bin", file.size, sat_cdfs_file_read_at, &source) == SAT_OK);
    sat_file_t virtual_file{};
    OK(sat_file_open("assets/player.bin", &virtual_file) == SAT_OK);
    std::memset(output, 0, sizeof(output));
    OK(sat_file_read(virtual_file, output, sizeof(output), &read) == SAT_OK &&
       read == sizeof(output) && output[0] == 0u && output[3] == 3u);
    OK(sat_file_close(virtual_file) == SAT_OK);
    OK(sat_cdfs_lookup(&volume, "ROOT.TXT", &file) == SAT_OK && file.extent_lba == 22u);
    OK(sat_cdfs_lookup(&volume, "assets/../bad", &file) == SAT_ERR_INVALID_ARG);
    OK(sat_cdfs_lookup(&volume, "missing.bin", &file) == SAT_ERR_NOT_FOUND);
    OK(sat_cdfs_unmount(&volume) == SAT_OK);
    OK(sat_cdfs_lookup(&volume, "ROOT.TXT", &file) == SAT_ERR_NOT_INITIALIZED);
    std::puts("cdfs logic: OK");
    return 0;
}
