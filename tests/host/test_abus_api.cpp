#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "saturn/abus.h"
#include "saturn/ram_cart.h"
#include "src/hal/storage/abus.hpp"
#include "src/storage/cartridge/abus_logic.hpp"

namespace {
uint8_t g_id = 0xFFu;
uint8_t g_cs0[0x800000] = {};
uint8_t g_cs1[0x1000] = {};
unsigned g_reads = 0u, g_writes = 0u, g_ram_inits = 0u;
}  // namespace

namespace saturn::hal::abus {
uint8_t read_id() { return g_id; }
void read(uint32_t area, uint32_t offset, void* dst, uint32_t bytes) {
    ++g_reads;
    assert(area <= 1u);
    std::memcpy(dst, (area == 0u ? g_cs0 : g_cs1) + offset, bytes);
}
void write(uint32_t area, uint32_t offset, const void* src, uint32_t bytes) {
    ++g_writes;
    assert(area == 0u);
    std::memcpy(g_cs0 + offset, src, bytes);
}
}  // namespace saturn::hal::abus

extern "C" sat_result_t sat_ram_cart_init(void) {
    ++g_ram_inits;
    return SAT_OK;
}

static void logic_classifies_ids_and_windows() {
    using namespace saturn::core::abus_logic;
    assert(classify(0xFFu) == Kind::None && classify(0x5Au) == Kind::Dram1Mb && classify(0x5Cu) == Kind::Dram4Mb);
    assert(classify(0x21u) == Kind::BackupMemory && classify(0x24u) == Kind::BackupMemory);
    assert(classify(0x20u) == Kind::Unknown && classify(0x25u) == Kind::Unknown && classify(0x00u) == Kind::Unknown);
    assert(backup_capacity(0x21u) == 512u * 1024u && backup_capacity(0x24u) == 4u * 1024u * 1024u);
    assert(range_ok(0u, 0u, kCs0Bytes) && !range_ok(0u, 1u, kCs0Bytes) && !range_ok(2u, 0u, 1u));
    assert(range_ok(1u, kCs1Bytes, 0u) && !range_ok(1u, kCs1Bytes, 1u));
    /* 1 MiB cart: two 512 KiB banks; a write may not straddle the gap */
    assert(write_ok(Kind::Dram1Mb, 0u, kDramBank0, 512u * 1024u));
    assert(!write_ok(Kind::Dram1Mb, 0u, kDramBank0, 512u * 1024u + 1u));
    assert(!write_ok(Kind::Dram1Mb, 0u, kDramBank0 + 512u * 1024u - 2u, 4u));
    assert(write_ok(Kind::Dram1Mb, 0u, kDramBank1 + 100u, 16u));
    assert(write_ok(Kind::Dram4Mb, 0u, kDramBank0 + 2u * 1024u * 1024u - 4u, 4u));
    assert(!write_ok(Kind::Dram4Mb, 0u, kDramBank0 + 2u * 1024u * 1024u - 4u, 8u));
    assert(!write_ok(Kind::Dram4Mb, 0u, 0u, 4u));                       /* below the banks */
    assert(!write_ok(Kind::Dram4Mb, 1u, kDramBank0, 4u));               /* CS1 is never written */
    assert(!write_ok(Kind::BackupMemory, 0u, kDramBank0, 4u) && !write_ok(Kind::Unknown, 0u, kDramBank0, 4u));
    assert(!write_ok(Kind::None, 0u, kDramBank0, 4u));
    assert(read_ok(Kind::Dram1Mb) && read_ok(Kind::Unknown) && !read_ok(Kind::BackupMemory) && !read_ok(Kind::None));
}

int main() {
    logic_classifies_ids_and_windows();

    sat_abus_info_t info{};
    assert(sat_abus_detect(nullptr) == SAT_ERR_INVALID_ARG);
    /* nothing in the slot */
    g_id = 0xFFu;
    assert(sat_abus_detect(&info) == SAT_OK && info.kind == SAT_ABUS_NONE && info.id == 0xFFu);
    uint8_t buf[16] = {};
    assert(sat_abus_read(SAT_ABUS_CS0, 0u, buf, 4u) == SAT_ERR_NOT_CONNECTED);
    assert(sat_abus_write(SAT_ABUS_CS0, 0x400000u, buf, 4u) == SAT_ERR_NOT_CONNECTED);
    assert(g_reads == 0u && g_writes == 0u && g_ram_inits == 0u);

    /* backup cartridge: detected, but never touched raw */
    g_id = 0x23u;
    assert(sat_abus_detect(&info) == SAT_OK && info.kind == SAT_ABUS_BACKUP_MEMORY &&
           info.backup_bytes == 2u * 1024u * 1024u);
    assert(sat_abus_read(SAT_ABUS_CS1, 0u, buf, 4u) == SAT_ERR_UNSUPPORTED);
    assert(sat_abus_write(SAT_ABUS_CS0, 0x400000u, buf, 4u) == SAT_ERR_UNSUPPORTED);
    assert(g_reads == 0u && g_writes == 0u && g_ram_inits == 0u);

    /* a ROM cartridge (unknown ID): readable, never writable */
    g_id = 0x00u;
    g_cs0[0x10] = 0xAB;
    assert(sat_abus_detect(&info) == SAT_OK && info.kind == SAT_ABUS_UNKNOWN);
    assert(sat_abus_read(SAT_ABUS_CS0, 0x10u, buf, 1u) == SAT_OK && buf[0] == 0xABu);
    assert(sat_abus_write(SAT_ABUS_CS0, 0x400000u, buf, 4u) == SAT_ERR_UNSUPPORTED && g_writes == 0u);

    /* DRAM expansion: write and read back inside a bank */
    g_id = 0x5Cu;
    assert(sat_abus_detect(&info) == SAT_OK && info.kind == SAT_ABUS_DRAM_4MB && info.backup_bytes == 0u);
    const uint8_t pattern[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    assert(sat_abus_write(SAT_ABUS_CS0, 0x600000u + 32u, pattern, 8u) == SAT_OK && g_ram_inits == 1u);
    std::memset(buf, 0, sizeof(buf));
    assert(sat_abus_read(SAT_ABUS_CS0, 0x600000u + 32u, buf, 8u) == SAT_OK && std::memcmp(buf, pattern, 8u) == 0);
    /* refusals leave memory alone */
    const unsigned writes = g_writes;
    assert(sat_abus_write(SAT_ABUS_CS0, 0x100u, pattern, 8u) == SAT_ERR_UNSUPPORTED);
    assert(sat_abus_write(SAT_ABUS_CS1, 0u, pattern, 8u) == SAT_ERR_UNSUPPORTED);
    assert(sat_abus_write(SAT_ABUS_CS0, 0x400000u + 2u * 1024u * 1024u - 4u, pattern, 8u) == SAT_ERR_UNSUPPORTED);
    assert(sat_abus_write(SAT_ABUS_CS0, 0x1FFFFFFu, pattern, 8u) == SAT_ERR_INVALID_ARG);
    assert(sat_abus_write(SAT_ABUS_CS0, 0u, nullptr, 8u) == SAT_ERR_INVALID_ARG);
    assert(sat_abus_read(SAT_ABUS_CS0, 0x2000000u, buf, 1u) == SAT_ERR_INVALID_ARG);
    assert(sat_abus_read(static_cast<sat_abus_area_t>(2), 0u, buf, 1u) == SAT_ERR_INVALID_ARG);
    assert(g_writes == writes);
    assert(sat_abus_read(SAT_ABUS_CS0, 0u, nullptr, 0u) == SAT_OK);

    std::puts("PASS: test_abus_api.cpp");
    return 0;
}
