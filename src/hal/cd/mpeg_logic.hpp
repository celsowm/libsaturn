#ifndef SATURN_HAL_CD_MPEG_LOGIC_HPP
#define SATURN_HAL_CD_MPEG_LOGIC_HPP

#include <stdint.h>

/* The MPEG card is reached through CD Block commands; there is no register of
 * its own. Command numbers and layouts follow the CD Block command list as the
 * Ymir core implements it (cdblock.cpp: Get Hardware Info, Authenticate Device,
 * Is Device Authenticated, MPEG Init). None of it has been run against a real
 * MPEG card: neither Ymir nor Mednafen emulates one. */
namespace saturn::hal::cd::mpeg_logic {

constexpr uint16_t kGetHardwareInfo = 0x0100u;     /* 0x01 in the high byte of CR1 */
constexpr uint16_t kAuthenticateDevice = 0xE000u;
constexpr uint16_t kIsDeviceAuthenticated = 0xE100u;
constexpr uint16_t kMpegInit = 0x9300u;

constexpr uint16_t kAuthTypeMpeg = 0x0001u;        /* CR2 of the two authentication commands */
constexpr uint8_t kAuthenticated = 2u;             /* Is Device Authenticated, MPEG */
constexpr uint8_t kStatusUnauthenticated = 0xFFu;  /* status byte of MPEG Init without authentication */

struct HardwareInfo {
    uint8_t hardware_flags;
    uint8_t hardware_version;
    uint8_t mpeg_version;     /* 0: no MPEG card, or one that is not authenticated */
    uint8_t drive_version;
    uint8_t drive_revision;
};

/* Get Hardware Info answer: CR2 = flags:version, CR3 low byte = MPEG version,
 * CR4 = drive version:revision. */
constexpr HardwareInfo decode_hardware_info(const uint16_t response[4]) {
    return {static_cast<uint8_t>(response[1] >> 8u), static_cast<uint8_t>(response[1]),
            static_cast<uint8_t>(response[2]), static_cast<uint8_t>(response[3] >> 8u),
            static_cast<uint8_t>(response[3])};
}

constexpr bool card_present(const HardwareInfo& info) {
    return info.mpeg_version != 0u;
}

constexpr uint8_t status_byte(const uint16_t response[4]) {
    return static_cast<uint8_t>(response[0] >> 8u);
}

}  // namespace saturn::hal::cd::mpeg_logic

#endif  // SATURN_HAL_CD_MPEG_LOGIC_HPP
