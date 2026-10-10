#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "saturn/cart_overlay.h"
#include "saturn/cdfs.h"
#include "saturn/lz.h"
#include "src/storage/cartridge/overlay_hal.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

namespace {

constexpr uint32_t kSectorCount = 32u;
uint8_t g_image[kSectorCount * SAT_CD_SECTOR_BYTES] = {};

constexpr uint32_t kSlotBase = 0x02440000u;
constexpr uint32_t kSlotBytes = 3u * SAT_CD_SECTOR_BYTES;
alignas(4) uint8_t g_slot[kSlotBytes] = {};
uint32_t g_last_write_base = 0u;
int g_purges = 0;
int g_reads = 0;
bool g_fail_reads = false;

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
    if (g_fail_reads && lba >= 22u) return SAT_ERR_IO;
    if (lba >= 22u) ++g_reads;
    std::memcpy(destination, g_image + lba * SAT_CD_SECTOR_BYTES, count * SAT_CD_SECTOR_BYTES);
    return SAT_OK;
}

// tools/satlz.py of "Saturn " x 20, bytes 0..63, 300 zero bytes, "end" (as tests/host/test_lz.cpp).
const uint8_t kPacked[] = {
    0x53,0x4c,0x5a,0x31,0x00,0x00,0x01,0xfb,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x56,0x7f,0x53,0x61,0x74,
    0x75,0x72,0x6e,0x20,0x00,0x07,0x72,0xff,0x32,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,
    0x0b,0x0c,0x0d,0x0e,0x0f,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,
    0x1f,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,0x30,0x31,0x32,
    0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,0x00,0x00,0x01,0xff,0x19,0x30,0x65,
    0x6e,0x64,
};
constexpr uint32_t kUnpacked = 507u;

// Root: LV00.BIN (1.5 sectors of 0xA0), LV01.BIN (2 sectors of 0xB1), BIG.BIN (4 sectors),
// PACK.LZ (an SLZ1 file, sector 30).
void make_image() {
    std::memset(g_image, 0, sizeof(g_image));
    uint8_t* pvd = g_image + 16u * SAT_CD_SECTOR_BYTES;
    pvd[0] = 1u;
    pvd[1] = 'C'; pvd[2] = 'D'; pvd[3] = '0'; pvd[4] = '0'; pvd[5] = '1'; pvd[6] = 1u;
    pvd[156u] = 34u;
    put_le32(pvd + 158u, 20u);
    put_le32(pvd + 166u, SAT_CD_SECTOR_BYTES);

    uint8_t* root = g_image + 20u * SAT_CD_SECTOR_BYTES;
    uint16_t offset = 0u;
    offset = static_cast<uint16_t>(offset + add_record(root, offset, 20u, SAT_CD_SECTOR_BYTES, 2u, "\0", 1u));
    offset = static_cast<uint16_t>(offset + add_record(root, offset, 20u, SAT_CD_SECTOR_BYTES, 2u, "\1", 1u));
    offset = static_cast<uint16_t>(offset + add_record(root, offset, 22u, SAT_CD_SECTOR_BYTES * 3u / 2u, 0u, "LV00.BIN;1", 10u));
    offset = static_cast<uint16_t>(offset + add_record(root, offset, 24u, SAT_CD_SECTOR_BYTES * 2u, 0u, "LV01.BIN;1", 10u));
    offset = static_cast<uint16_t>(offset + add_record(root, offset, 26u, SAT_CD_SECTOR_BYTES * 4u, 0u, "BIG.BIN;1", 9u));
    (void)add_record(root, offset, 30u, sizeof(kPacked), 0u, "PACK.LZ;1", 9u);
    std::memset(g_image + 22u * SAT_CD_SECTOR_BYTES, 0xA0, SAT_CD_SECTOR_BYTES * 3u / 2u);
    std::memset(g_image + 24u * SAT_CD_SECTOR_BYTES, 0xB1, SAT_CD_SECTOR_BYTES * 2u);
    std::memcpy(g_image + 30u * SAT_CD_SECTOR_BYTES, kPacked, sizeof(kPacked));
}

}  // namespace

// The slot is g_slot; the loader must write the cache-through mirror of the slot's base.
namespace saturn::storage::cart_overlay {
void* write_pointer(uint32_t cached_base) {
    g_last_write_base = cached_base | kCacheThroughBit;
    return g_slot + (cached_base - kSlotBase);
}
void purge_cpu_cache() { ++g_purges; }
}  // namespace saturn::storage::cart_overlay

int main() {
    make_image();
    sat_cd_device_t device{};
    OK(sat_cd_device_init(&device, read_image, nullptr, kSectorCount) == SAT_OK);
    sat_cdfs_volume_t volume{};
    OK(sat_cdfs_mount(&volume, &device) == SAT_OK);

    // init: the slot must be word-aligned and inside the 4 MiB cart window (either view).
    sat_cart_overlay_t overlay{};
    OK(sat_cart_overlay_init(nullptr, kSlotBase, kSlotBytes) == SAT_ERR_INVALID_ARG);
    OK(sat_cart_overlay_init(&overlay, 0x06000000u, kSlotBytes) == SAT_ERR_INVALID_ARG);
    OK(sat_cart_overlay_init(&overlay, kSlotBase + 2u, kSlotBytes) == SAT_ERR_INVALID_ARG);
    OK(sat_cart_overlay_init(&overlay, kSlotBase, 0u) == SAT_ERR_INVALID_ARG);
    OK(sat_cart_overlay_init(&overlay, 0x027FF000u, 0x2000u) == SAT_ERR_INVALID_ARG);
    OK(sat_cart_overlay_init(&overlay, kSlotBase | 0x20000000u, kSlotBytes) == SAT_OK);
    OK(overlay.base == kSlotBase && overlay.resident == SAT_CART_OVERLAY_NONE);

    // First load reads the file into the slot through the cache-through view, then purges.
    OK(sat_cart_overlay_load(&overlay, &volume, 0, "LV00.BIN", SAT_CD_SECTOR_BYTES * 3u / 2u,
                             nullptr, nullptr) == SAT_OK);
    OK(overlay.resident == 0 && g_purges == 1 && g_last_write_base == (kSlotBase | 0x20000000u));
    OK(g_slot[0] == 0xA0u && g_slot[SAT_CD_SECTOR_BYTES * 3u / 2u - 1u] == 0xA0u);
    OK(g_slot[SAT_CD_SECTOR_BYTES * 3u / 2u] == 0u);

    // Already resident: no read, no purge.
    const int reads = g_reads;
    OK(sat_cart_overlay_load(&overlay, &volume, 0, "LV00.BIN", 0u, nullptr, nullptr) == SAT_OK);
    OK(g_reads == reads && g_purges == 1);

    // Another id swaps the slot.
    OK(sat_cart_overlay_load(&overlay, &volume, 1, "LV01.BIN", 0u, nullptr, nullptr) == SAT_OK);
    OK(overlay.resident == 1 && g_purges == 2 && g_slot[0] == 0xB1u &&
       g_slot[SAT_CD_SECTOR_BYTES * 2u - 1u] == 0xB1u);

    // Refusals before any byte moves: too big, wrong size, missing, bad id. The slot keeps 1.
    OK(sat_cart_overlay_load(&overlay, &volume, 2, "BIG.BIN", 0u, nullptr, nullptr) == SAT_ERR_CAPACITY);
    OK(sat_cart_overlay_load(&overlay, &volume, 0, "LV00.BIN", 7u, nullptr, nullptr) == SAT_ERR_VERSION);
    OK(sat_cart_overlay_load(&overlay, &volume, 3, "NOPE.BIN", 0u, nullptr, nullptr) == SAT_ERR_NOT_FOUND);
    OK(sat_cart_overlay_load(&overlay, &volume, -1, "LV00.BIN", 0u, nullptr, nullptr) == SAT_ERR_INVALID_ARG);
    OK(overlay.resident == 1 && g_slot[0] == 0xB1u && g_purges == 2);

    // A read that fails half way leaves the slot empty (and the cache purged).
    g_fail_reads = true;
    OK(sat_cart_overlay_load(&overlay, &volume, 0, "LV00.BIN", 0u, nullptr, nullptr) == SAT_ERR_IO);
    OK(overlay.resident == SAT_CART_OVERLAY_NONE && g_purges == 3);
    g_fail_reads = false;
    OK(sat_cart_overlay_load(&overlay, &volume, 1, "LV01.BIN", 0u, nullptr, nullptr) == SAT_OK);
    OK(overlay.resident == 1 && g_purges == 4);

    // forget: the next load of the same id reads again.
    sat_cart_overlay_forget(&overlay);
    OK(overlay.resident == SAT_CART_OVERLAY_NONE);
    const int before = g_reads;
    OK(sat_cart_overlay_load(&overlay, &volume, 1, "LV01.BIN", 0u, nullptr, nullptr) == SAT_OK);
    OK(g_reads > before && overlay.resident == 1);

    // An SLZ1 overlay is decoded in place in the slot; the plain loader's checks still apply.
    std::memset(g_slot, 0xEE, sizeof(g_slot));
    const int purges = g_purges;
    OK(sat_cart_overlay_load_lz(&overlay, &volume, 5, "PACK.LZ", 7u, nullptr, nullptr) == SAT_ERR_VERSION);
    OK(overlay.resident == SAT_CART_OVERLAY_NONE && g_purges == purges + 1);
    OK(sat_cart_overlay_load_lz(&overlay, &volume, 5, "PACK.LZ", kUnpacked, nullptr, nullptr) == SAT_OK);
    OK(overlay.resident == 5 && g_purges == purges + 2);
    OK(std::memcmp(g_slot, "Saturn Saturn ", 14u) == 0 && g_slot[140] == 0u && g_slot[141] == 1u &&
       g_slot[203] == 63u && g_slot[204] == 0u && g_slot[503] == 0u && std::memcmp(g_slot + 504, "end", 3u) == 0);
    const int lz_reads = g_reads;
    OK(sat_cart_overlay_load_lz(&overlay, &volume, 5, "PACK.LZ", 0u, nullptr, nullptr) == SAT_OK);
    OK(g_reads == lz_reads);
    OK(sat_cart_overlay_load_lz(&overlay, &volume, 6, "BIG.BIN", 0u, nullptr, nullptr) == SAT_ERR_CAPACITY);
    OK(sat_cart_overlay_load_lz(&overlay, &volume, 6, "LV01.BIN", 0u, nullptr, nullptr) == SAT_ERR_VERSION);
    OK(overlay.resident == SAT_CART_OVERLAY_NONE);
    // The file fits a 400-byte slot but its 507 decoded bytes do not: refused after the read.
    sat_cart_overlay_t small{};
    OK(sat_cart_overlay_init(&small, kSlotBase, 400u) == SAT_OK);
    OK(sat_cart_overlay_load_lz(&small, &volume, 5, "PACK.LZ", 0u, nullptr, nullptr) == SAT_ERR_CAPACITY);
    OK(small.resident == SAT_CART_OVERLAY_NONE);

    std::puts("cart overlay: OK");
    return 0;
}
