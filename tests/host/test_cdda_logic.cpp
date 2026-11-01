#include <cassert>
#include <cstdint>
#include <cstdio>

#include "src/storage/cd/cdda_logic.hpp"

using namespace saturn::storage::cdda::logic;

/* Data track at FAD 150, audio track 2 at 0x1000, lead-out at 0x5000. */
static sat_cd_toc_t two_track_toc() {
    sat_cd_toc_t toc = {};
    toc_store(toc, 0u, 0x41000096u);
    toc_store(toc, 1u, 0x01001000u);
    toc_store(toc, kFirstEntry, 0x41010000u);
    toc_store(toc, kLastEntry, 0x01020000u);
    toc_store(toc, kLeadOutEntry, 0x41005000u);
    return toc;
}

static void toc_entries_land_in_their_fields() {
    const sat_cd_toc_t toc = two_track_toc();
    assert(toc.tracks[1] == 0x01001000u);
    assert(toc.first_track == 1u && toc.last_track == 2u);
    assert(toc.lead_out_fad == 0x5000u);
}

static void an_audio_track_gives_its_range() {
    const sat_cd_toc_t toc = two_track_toc();
    uint32_t start = 0u, length = 0u;
    assert(audio_track(toc, 2u, start, length) == SAT_OK);
    assert(start == 0x1000u && length == 0x4000u);
}

static void a_following_track_ends_the_range() {
    sat_cd_toc_t toc = two_track_toc();
    toc_store(toc, 2u, 0x01002000u);
    toc_store(toc, kLastEntry, 0x01030000u);
    uint32_t start = 0u, length = 0u;
    assert(audio_track(toc, 2u, start, length) == SAT_OK);
    assert(start == 0x1000u && length == 0x1000u);
}

static void a_data_or_absent_track_is_refused() {
    const sat_cd_toc_t toc = two_track_toc();
    uint32_t start = 0u, length = 0u;
    assert(audio_track(toc, 1u, start, length) == SAT_ERR_INVALID_ARG);
    assert(audio_track(toc, 0u, start, length) == SAT_ERR_INVALID_ARG);
    assert(audio_track(toc, 3u, start, length) == SAT_ERR_INVALID_ARG);
}

static void a_lead_out_before_the_track_is_refused() {
    sat_cd_toc_t toc = two_track_toc();
    toc_store(toc, kLeadOutEntry, 0x41000800u);
    uint32_t start = 0u, length = 0u;
    assert(audio_track(toc, 2u, start, length) == SAT_ERR_INVALID_ARG);
}

static void play_splits_both_fads() {
    uint16_t command[4];
    assert(compose_play(0x012345u, 0x067890u, command));
    assert(command[0] == 0x1081u && command[1] == 0x2345u);
    assert(command[2] == 0x0F86u && command[3] == 0x7890u);
    assert(!compose_play(0x1000u, 0u, command));
    assert(!compose_play(0x1000000u, 1u, command));
    assert(!compose_play(1u, 0x1000000u, command));
}

static void status_splits_its_fields() {
    const uint16_t response[4] = {0x0320u, 0x0102u, 0x0101u, 0x2345u};
    const sat_cd_status_t status = decode_status(response);
    assert(status.state == SAT_CD_STATE_PLAY);
    assert(status.control == 1u && status.track == 2u && status.index == 1u);
    assert(status.fad == 0x012345u);
}

static void faults_are_the_documented_states() {
    assert(status_is_fault(0xFFu));
    assert(status_is_fault(0x06u) && status_is_fault(0x07u));
    assert(status_is_fault(0x09u) && status_is_fault(0x0Au));
    assert(!status_is_fault(SAT_CD_STATE_PLAY));
    assert(!status_is_fault(SAT_CD_STATE_PAUSE));
    assert(!status_is_fault(0x20u));
}

/* Tekken 3's level 5 and the dino demo's level 7 were written as raw words. */
static void level_keeps_the_slot_byte_and_pans_hard() {
    assert(exts_word(0x1234u, 5u, false) == 0x12BFu);
    assert(exts_word(0x1234u, 5u, true) == 0x12AFu);
    assert(exts_word(0u, 7u, false) == 0x00FFu);
    assert(exts_word(0u, 7u, true) == 0x00EFu);
    assert(exts_word(0u, 9u, false) == 0x00FFu);
    assert(exts_word(0xFF00u, 0u, true) == 0xFF0Fu);
}

int main() {
    toc_entries_land_in_their_fields();
    an_audio_track_gives_its_range();
    a_following_track_ends_the_range();
    a_data_or_absent_track_is_refused();
    a_lead_out_before_the_track_is_refused();
    play_splits_both_fads();
    status_splits_its_fields();
    faults_are_the_documented_states();
    level_keeps_the_slot_byte_and_pans_hard();
    std::puts("test_cdda_logic: OK");
    return 0;
}
