#include "saturn/mpeg.h"

#include "src/hal/cd/mpeg_logic.hpp"

namespace {

namespace logic = saturn::hal::cd::mpeg_logic;

constexpr uint32_t kAuthenticationPolls = 64u;

sat_result_t authentication_status(sat_cd_block_t* block, uint8_t* out_status) {
    const uint16_t command[4] = {logic::kIsDeviceAuthenticated, logic::kAuthTypeMpeg, 0u, 0u};
    uint16_t response[4] = {};
    SAT_TRY(sat_cd_block_command(block, command, response));
    *out_status = static_cast<uint8_t>(response[1]);
    return SAT_OK;
}

}  // namespace

extern "C" sat_result_t sat_mpeg_probe(sat_cd_block_t* block, sat_mpeg_info_t* out_info) {
    if (block == nullptr || out_info == nullptr) return SAT_ERR_INVALID_ARG;
    *out_info = {};
    const uint16_t command[4] = {logic::kGetHardwareInfo, 0u, 0u, 0u};
    uint16_t response[4] = {};
    SAT_TRY(sat_cd_block_command(block, command, response));
    const logic::HardwareInfo info = logic::decode_hardware_info(response);
    uint8_t status = 0u;
    SAT_TRY(authentication_status(block, &status));
    out_info->present = logic::card_present(info) ? 1u : 0u;
    out_info->mpeg_version = info.mpeg_version;
    out_info->hardware_flags = info.hardware_flags;
    out_info->hardware_version = info.hardware_version;
    out_info->drive_version = info.drive_version;
    out_info->drive_revision = info.drive_revision;
    out_info->authentication_status = status;
    out_info->authenticated = status == logic::kAuthenticated ? 1u : 0u;
    return SAT_OK;
}

extern "C" sat_result_t sat_mpeg_start(sat_cd_block_t* block) {
    sat_mpeg_info_t info = {};
    SAT_TRY(sat_mpeg_probe(block, &info));
    if (info.present == 0u) return SAT_ERR_NOT_CONNECTED;

    if (info.authenticated == 0u) {
        const uint16_t authenticate[4] = {logic::kAuthenticateDevice, logic::kAuthTypeMpeg, 0u, 0u};
        uint16_t response[4] = {};
        SAT_TRY(sat_cd_block_command(block, authenticate, response));
        uint8_t status = 0u;
        for (uint32_t poll = 0u; poll < kAuthenticationPolls && status != logic::kAuthenticated; ++poll) {
            SAT_TRY(authentication_status(block, &status));
        }
        if (status != logic::kAuthenticated) return SAT_ERR_TIMEOUT;
    }

    const uint16_t init[4] = {logic::kMpegInit, 0u, 0u, 0u};
    uint16_t response[4] = {};
    SAT_TRY(sat_cd_block_command(block, init, response));
    return logic::status_byte(response) == logic::kStatusUnauthenticated ? SAT_ERR_VERIFY_FAILED : SAT_OK;
}
