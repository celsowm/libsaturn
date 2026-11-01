#include "saturn/cdda.h"

#include "saturn/video.h"
#include "src/storage/cd/cdda_logic.hpp"

namespace {

namespace logic = saturn::storage::cdda::logic;

constexpr uint32_t kDataReadyFields = 120u;

volatile uint16_t& reg16(uint32_t address) {
    return *reinterpret_cast<volatile uint16_t*>(static_cast<uintptr_t>(address));
}

constexpr uint32_t kExtsLeft = 0x25B00216u;
constexpr uint32_t kExtsRight = 0x25B00236u;

sat_result_t wait_data_ready() {
    for (uint32_t field = 0u; field < kDataReadyFields; ++field) {
        if ((reg16(SAT_CD_BLOCK_HIRQ) & SAT_CD_BLOCK_HIRQ_DRDY) != 0u) return SAT_OK;
        SAT_TRY(sat_wait_vblank());
    }
    return SAT_ERR_TIMEOUT;
}

}  // namespace

extern "C" sat_result_t sat_cd_read_toc(sat_cd_block_t* block, sat_cd_toc_t* out_toc) {
    if (block == nullptr || out_toc == nullptr) return SAT_ERR_INVALID_ARG;
    const uint16_t get_toc[4] = {0x0200u, 0u, 0u, 0u};
    const uint16_t end_transfer[4] = {0x0600u, 0u, 0u, 0u};
    uint16_t response[4];
    reg16(SAT_CD_BLOCK_HIRQ) = static_cast<uint16_t>(~SAT_CD_BLOCK_HIRQ_DRDY);
    SAT_TRY(sat_cd_block_command(block, get_toc, response));
    if (response[1] != logic::kTocWords) {
        (void)sat_cd_block_command(block, end_transfer, response);
        return SAT_ERR_IO;
    }
    SAT_TRY(wait_data_ready());
    for (uint32_t index = 0u; index < logic::kTocLongwords; ++index) {
        const uint32_t high = reg16(SAT_CD_BLOCK_DTR);
        const uint32_t low = reg16(SAT_CD_BLOCK_DTR);
        logic::toc_store(*out_toc, index, (high << 16u) | low);
    }
    SAT_TRY(sat_cd_block_command(block, end_transfer, response));
    return response[1] == logic::kTocWords ? SAT_OK : SAT_ERR_IO;
}

extern "C" sat_result_t sat_cd_toc_audio_track(const sat_cd_toc_t* toc, uint8_t track,
                                               uint32_t* out_start_fad, uint32_t* out_length) {
    if (toc == nullptr || out_start_fad == nullptr || out_length == nullptr) {
        return SAT_ERR_INVALID_ARG;
    }
    return logic::audio_track(*toc, track, *out_start_fad, *out_length);
}

extern "C" sat_result_t sat_cdda_play(sat_cd_block_t* block, uint32_t start_fad, uint32_t length) {
    uint16_t command[4];
    if (block == nullptr || !logic::compose_play(start_fad, length, command)) {
        return SAT_ERR_INVALID_ARG;
    }
    uint16_t response[4];
    SAT_TRY(sat_cd_block_command(block, command, response));
    return (response[0] >> 8u) == 0xFFu ? SAT_ERR_IO : SAT_OK;
}

extern "C" sat_result_t sat_cd_get_status(sat_cd_block_t* block, sat_cd_status_t* out_status) {
    if (block == nullptr || out_status == nullptr) return SAT_ERR_INVALID_ARG;
    const uint16_t get_status[4] = {0u, 0u, 0u, 0u};
    uint16_t response[4];
    SAT_TRY(sat_cd_block_command(block, get_status, response));
    *out_status = logic::decode_status(response);
    return SAT_OK;
}

extern "C" sat_result_t sat_cd_check_periodic(void) {
    return logic::status_is_fault(static_cast<uint8_t>(reg16(SAT_CD_BLOCK_CR1) >> 8u))
        ? SAT_ERR_IO : SAT_OK;
}

extern "C" void sat_cdda_set_level(uint8_t level) {
    reg16(kExtsLeft) = logic::exts_word(reg16(kExtsLeft), level, false);
    reg16(kExtsRight) = logic::exts_word(reg16(kExtsRight), level, true);
}
