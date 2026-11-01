#ifndef SATURN_STORAGE_CD_CDDA_LOGIC_HPP
#define SATURN_STORAGE_CD_CDDA_LOGIC_HPP

#include <stdint.h>

#include "saturn/cdda.h"

/* Pure CD audio rules: no register access, so host tests cover them. */
namespace saturn::storage::cdda::logic {

constexpr uint32_t kFadMask = 0x00FFFFFFu;
constexpr uint8_t kControlAudio = 0x01u;
/* A TOC is 102 big-endian longwords: 99 tracks, first, last, lead-out. */
constexpr uint32_t kTocLongwords = 102u;
constexpr uint32_t kTocWords = kTocLongwords * 2u;
constexpr uint32_t kFirstEntry = SAT_CD_TOC_TRACKS;
constexpr uint32_t kLastEntry = SAT_CD_TOC_TRACKS + 1u;
constexpr uint32_t kLeadOutEntry = SAT_CD_TOC_TRACKS + 2u;

inline void toc_store(sat_cd_toc_t& toc, uint32_t index, uint32_t entry) {
    if (index < SAT_CD_TOC_TRACKS) {
        toc.tracks[index] = entry;
    } else if (index == kFirstEntry) {
        toc.first_track = static_cast<uint8_t>(entry >> 16u);
    } else if (index == kLastEntry) {
        toc.last_track = static_cast<uint8_t>(entry >> 16u);
    } else if (index == kLeadOutEntry) {
        toc.lead_out_fad = entry & kFadMask;
    }
}

inline sat_result_t audio_track(const sat_cd_toc_t& toc, uint8_t track,
                                uint32_t& start, uint32_t& length) {
    if (track < toc.first_track || track > toc.last_track ||
        track == 0u || track > SAT_CD_TOC_TRACKS) {
        return SAT_ERR_INVALID_ARG;
    }
    const uint32_t entry = toc.tracks[track - 1u];
    if ((entry >> 24u) != kControlAudio) return SAT_ERR_INVALID_ARG;
    const uint32_t from = entry & kFadMask;
    const uint32_t end = track == toc.last_track
        ? toc.lead_out_fad : toc.tracks[track] & kFadMask;
    if (end <= from) return SAT_ERR_INVALID_ARG;
    start = from;
    length = end - from;
    return SAT_OK;
}

/* Play Disc by FAD, repeating without end. An explicit length also defines the
 * final track's boundary without a track/index report at the lead-out. */
inline bool compose_play(uint32_t start, uint32_t length, uint16_t command[4]) {
    if (length == 0u || start > kFadMask || length > kFadMask) return false;
    command[0] = static_cast<uint16_t>(0x1080u | (start >> 16u));
    command[1] = static_cast<uint16_t>(start);
    command[2] = static_cast<uint16_t>(0x0F80u | (length >> 16u));
    command[3] = static_cast<uint16_t>(length);
    return true;
}

inline sat_cd_status_t decode_status(const uint16_t response[4]) {
    sat_cd_status_t status;
    status.state = static_cast<uint8_t>((response[0] >> 8u) & 0x0Fu);
    status.control = static_cast<uint8_t>(response[1] >> 8u);
    status.track = static_cast<uint8_t>(response[1]);
    status.index = static_cast<uint8_t>(response[2] >> 8u);
    status.fad = (static_cast<uint32_t>(response[2] & 0xFFu) << 16u) | response[3];
    return status;
}

/* The first response byte: 0xFF is a refused command, and the low nibble's
 * states 6, 7, 9 and 10 are no disc, retry, error and fatal. */
inline bool status_is_fault(uint8_t status) {
    const uint8_t state = status & 0x0Fu;
    return status == 0xFFu || state == 6u || state == 7u || state == 9u || state == 10u;
}

/* SCSP manual 4.2.7: EXTS0/1 return through slot 16/17's low byte, EFSDL in the
 * top three bits and EFPAN below: hard left and hard right. The high byte is
 * the slot's own direct-send control, which stays. */
inline uint16_t exts_word(uint16_t current, uint8_t level, bool right) {
    const uint8_t clamped = level > SAT_CDDA_LEVEL_MAX ? uint8_t{SAT_CDDA_LEVEL_MAX} : level;
    const uint8_t pan = right ? 0x0Fu : 0x1Fu;
    return static_cast<uint16_t>((current & 0xFF00u) | (clamped << 5u) | pan);
}

}  // namespace saturn::storage::cdda::logic

#endif
