#ifndef SATURN_CORE_ABUS_LOGIC_HPP
#define SATURN_CORE_ABUS_LOGIC_HPP

#include <stdint.h>

/* Pure A-Bus cartridge policy: what a cartridge ID byte means, how big each
 * chip-select window is, and which accesses are allowed. No register access. */
namespace saturn::core::abus_logic {

enum class Kind : uint8_t { None = 0, Dram1Mb = 1, Dram4Mb = 2, BackupMemory = 3, Unknown = 4 };

/* The ID byte at the top of CS1 (A-Bus 0x04FFFFFF, read uncached at 0x24FFFFFF):
 * 0xFF with nothing plugged in, 0x5A/0x5C for the 1 MiB / 4 MiB DRAM
 * expansions, 0x21-0x24 for the 4/8/16/32 Mbit Backup Memory cartridges. */
constexpr Kind classify(uint8_t id) {
    return id == 0xFFu ? Kind::None
         : id == 0x5Au ? Kind::Dram1Mb
         : id == 0x5Cu ? Kind::Dram4Mb
         : (id >= 0x21u && id <= 0x24u) ? Kind::BackupMemory
                                        : Kind::Unknown;
}

constexpr uint32_t backup_capacity(uint8_t id) {
    return id == 0x21u ? 512u * 1024u
         : id == 0x22u ? 1024u * 1024u
         : id == 0x23u ? 2u * 1024u * 1024u
         : id == 0x24u ? 4u * 1024u * 1024u
                       : 0u;
}

/* Chip-select windows exposed to callers: CS0 is A-Bus 0x02000000-0x03FFFFFF
 * (cartridge ROM/RAM), CS1 is 0x04000000-0x04FFFFFF. CS2 belongs to the CD
 * block and is not offered. */
constexpr uint32_t kCs0Bytes = 32u * 1024u * 1024u;
constexpr uint32_t kCs1Bytes = 16u * 1024u * 1024u;

constexpr uint32_t area_bytes(uint32_t area) {
    return area == 0u ? kCs0Bytes : area == 1u ? kCs1Bytes : 0u;
}

constexpr bool range_ok(uint32_t area, uint32_t offset, uint32_t bytes) {
    const uint32_t size = area_bytes(area);
    return size != 0u && offset <= size && bytes <= size - offset;
}

/* DRAM expansion banks inside CS0: bank 0 at offset 4 MiB, bank 1 at 6 MiB,
 * each 512 KiB (1 MiB cart) or 2 MiB (4 MiB cart). */
constexpr uint32_t kDramBank0 = 0x400000u;
constexpr uint32_t kDramBank1 = 0x600000u;

constexpr uint32_t dram_bank_bytes(Kind kind) {
    return kind == Kind::Dram1Mb ? 512u * 1024u : kind == Kind::Dram4Mb ? 2u * 1024u * 1024u : 0u;
}

/* Writes reach only the DRAM banks of a DRAM expansion, one bank at a time.
 * Backup Memory cartridges are written through the BIOS (sat_save_*), and
 * writing anywhere else on the A-Bus could hit cartridge registers. */
constexpr bool write_ok(Kind kind, uint32_t area, uint32_t offset, uint32_t bytes) {
    const uint32_t bank = dram_bank_bytes(kind);
    if (bank == 0u || area != 0u || !range_ok(area, offset, bytes) || bytes == 0u) return false;
    return (offset >= kDramBank0 && offset - kDramBank0 <= bank && bytes <= bank - (offset - kDramBank0)) ||
           (offset >= kDramBank1 && offset - kDramBank1 <= bank && bytes <= bank - (offset - kDramBank1));
}

/* Reads are refused on an empty slot and on Backup Memory cartridges (their
 * contents belong to the BIOS backup library). */
constexpr bool read_ok(Kind kind) {
    return kind == Kind::Dram1Mb || kind == Kind::Dram4Mb || kind == Kind::Unknown;
}

}  // namespace saturn::core::abus_logic

#endif  // SATURN_CORE_ABUS_LOGIC_HPP
