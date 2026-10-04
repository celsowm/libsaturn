#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "saturn/cart_overlay.h"
#include "saturn/cdfs.h"
#include "src/storage/cartridge/overlay_hal.hpp"

#define OK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d\n", __FILE__, __LINE__); std::exit(1); } } while (0)

namespace {

constexpr uint32_t kSectorCount = 32u;
uint8_t g_image[kSectorCount * SAT_CD_SECTOR_BYTES] = {};

constexpr uint32_t kSlotBase = 0x02440000u;
constexpr uint32_t kSlotBytes = 3u * SAT_CD_SECTOR_BYTES;
uint8_t g_slot[kSlotBytes] = {};
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

// Root: LV00.BIN (1.5 sectors of 0xA0), LV01.BIN (2 sectors of 0xB1), BIG.BIN (4 sectors).
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
    (void)add_record(root, offset, 26u, SAT_CD_SECTOR_BYTES * 4u, 0u, "BIG.BIN;1", 9u);
    std::memset(g_image + 22u * SAT_CD_SECTOR_BYTES, 0xA0, SAT_CD_SECTOR_BYTES * 3u / 2u);
    std::memset(g_image + 24u * SAT_CD_SECTOR_BYTES, 0xB1, SAT_CD_SECTOR_BYTES * 2u);
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

    std::puts("cart overlay: OK");
    return 0;
}
