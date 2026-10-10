#include <cassert>
#include <cstdint>
#include <cstdio>

#include "saturn/sampler.h"
#include "src/audio/playback/claim.hpp"
#include "src/audio/playback/state.hpp"
#include "src/audio/sampler/logic.hpp"

using namespace saturn::core::audio::playback;
namespace sampler = saturn::core::audio::sampler;

saturn::core::AudioStreamRegistry saturn::core::g_audio_streams = {};

// A model of the slots: what the stubbed HAL was told, and what sounds after each KYONEX.
static uint16_t g_word[32][12];
static uint8_t g_kyonb[32];
static uint8_t g_sounding[32];
static uint32_t g_kyonex = 0u, g_pitch_writes = 0u, g_level_writes = 0u, g_waits = 0u;

namespace saturn::hal::scsp {
void mute_slot(uint8_t slot) { g_word[slot][0x0C / 2] = 0x00FFu; }
void key_off(uint8_t slot) {
    g_kyonb[slot] = 0u;
    g_sounding[slot] = 0u;
}
bool stage_slot(uint8_t slot, const SlotConfig& config, uint16_t pitch) {
    assert(slot < 32u);
    assert(g_sounding[slot] == 0u); /* never reprogram a slot that is heard */
    g_kyonb[slot] = 0u;
    g_word[slot][0x02 / 2] = static_cast<uint16_t>(config.start_address);
    g_word[slot][0x04 / 2] = config.loop_start;
    g_word[slot][0x06 / 2] = config.loop_end;
    g_word[slot][0x0C / 2] = config.total_level;
    g_word[slot][0x10 / 2] = pitch;
    g_word[slot][0x00 / 2] = static_cast<uint16_t>((config.pcm8 ? 0x10u : 0u) | (config.loop ? 0x20u : 0u) |
                                                   ((config.start_address >> 16) & 0xFu));
    return true;
}
void stage_key(uint8_t slot, bool on) { g_kyonb[slot] = on ? 1u : 0u; }
void execute_key_transitions() {
    ++g_kyonex;
    for (int i = 0; i < 32; ++i) g_sounding[i] = g_kyonb[i];
}
void wait_samples(uint32_t count) { g_waits += count; }
void set_slot_pitch(uint8_t slot, uint16_t pitch) {
    g_word[slot][0x10 / 2] = pitch;
    ++g_pitch_writes;
}
void set_slot_level_pan(uint8_t slot, uint8_t total_level, uint8_t direct_level, uint8_t pan) {
    g_word[slot][0x0C / 2] = total_level;
    g_word[slot][0x16 / 2] = static_cast<uint16_t>(direct_level << 13 | (pan & 0x1Fu) << 8);
    ++g_level_writes;
}
}  // namespace saturn::hal::scsp

static uint32_t sounding_count() {
    uint32_t n = 0u;
    for (int i = 0; i < 32; ++i) n += g_sounding[i];
    return n;
}

static void conversions() {
    using sampler::level_word;
    using sampler::pitch_word;
    assert(pitch_word(44100u) == 0x0000u);
    assert(pitch_word(88200u) == 0x0800u);         /* OCT 1 */
    assert(pitch_word(22050u) == 0x7800u);         /* OCT -1 */
    assert(pitch_word(66150u) == 0x0200u);         /* FNS 512: x1.5 */
    assert(pitch_word(0u) == 0x4000u);             /* slowest */
    assert(pitch_word(0xFFFFFFFFu) == 0x3BFFu);    /* OCT 7, FNS clamped */
    assert(sat_sampler_pitch_word(13379u) == pitch_word(13379u));

    assert(sat_sampler_total_level(255u) == 0u && sat_sampler_total_level(0u) == 255u);
    assert(sat_sampler_total_level(128u) == 16u);  /* -6 dB */
    for (int a = 1; a < 256; ++a) assert(sampler::kTotalLevel[a] <= sampler::kTotalLevel[a - 1]);

    assert(level_word(255u, 255u, 0u) == 0x0000u); /* centre, 0 dB */
    assert(level_word(0u, 0u, 0u) == 0x00FFu);     /* silent */
    assert(level_word(255u, 0u, 0u) == 0x1F00u);   /* left only: right -45 dB */
    assert(level_word(0u, 255u, 0u) == 0x0F00u);   /* right only */
    assert(level_word(255u, 128u, 0u) == 0x1200u); /* right 6 dB down: 2 DIPAN steps */
    assert(level_word(128u, 255u, 32u) == 0x0220u);/* attenuation adds to TL */
    assert(level_word(1u, 1u, 200u) == 0x00FFu);   /* TL clamps */
}

static void claims() {
    sat_sampler_config_t config = {0u, 16u, 4u, 0u, SAT_SAMPLER_RAM_BASE, SAT_SAMPLER_RAM_END - SAT_SAMPLER_RAM_BASE};
    g_initialized = 0u;
    assert(sat_sampler_init(&config) == SAT_ERR_NOT_INITIALIZED);
    g_initialized = 1u;

    sat_sampler_config_t bad = config;
    bad.voice_count = 17u;
    assert(sat_sampler_init(&bad) == SAT_ERR_INVALID_ARG);
    bad = config;
    bad.first_slot = 2u; /* 16 voices from slot 2 run past slot 31 */
    assert(sat_sampler_init(&bad) == SAT_ERR_INVALID_ARG);
    bad = config;
    bad.ram_offset = 0x1000u; /* the sound CPU's */
    assert(sat_sampler_init(&bad) == SAT_ERR_INVALID_ARG);
    assert(sat_sampler_init(nullptr) == SAT_ERR_INVALID_ARG);

    // a resident voice on a slot it wants, a sound in its RAM, an open stream: busy
    g_voice_registry.entries[5].active = 1u;
    assert(sat_sampler_init(&config) == SAT_ERR_BUSY);
    g_voice_registry.entries[5].active = 0u;
    uint16_t sound = 0u;
    uint32_t at = 0u;
    assert(g_ram.allocate(kSoundRamBase, kResidentSoundRamEnd, 0x1000u, 2u, &sound, &at));
    assert(sat_sampler_init(&config) == SAT_ERR_BUSY);
    assert(g_ram.release(sound));
    saturn::core::g_audio_streams.slots[3].used = 1u; /* slot 31, RAM 0x7C000 */
    assert(sat_sampler_init(&config) == SAT_ERR_BUSY);
    sat_sampler_config_t low = {0u, 8u, 4u, 0u, SAT_SAMPLER_RAM_BASE, 0x10000u}; /* slots 0-15: no conflict */
    assert(sat_sampler_init(&low) == SAT_OK);
    assert(sat_sampler_shutdown() == SAT_OK && g_ram.live_count == 0u);
    saturn::core::g_audio_streams.slots[3].used = 0u;

    assert(sat_sampler_init(&config) == SAT_OK);
    assert(sat_sampler_is_initialized() == 1u);
    assert(sat_sampler_init(&config) == SAT_ERR_BUSY);
    namespace claim = saturn::core::audio::claim;
    assert(claim::g_claim.slots == 0xFFFFFFFFu && claim::slot_claimed(28u));
    assert(claim::ram_claimed(0x70000u, 0x74000u) && !claim::ram_claimed(0x0u, SAT_SAMPLER_RAM_BASE));
    // resident sounds cannot land in its RAM, resident voices cannot take its slots
    assert(!g_ram.allocate(kSoundRamBase, kResidentSoundRamEnd, 0x100u, 2u, &sound, &at));
    assert(choose_voice(0u) == -1);
    assert(sat_sampler_ram(SAT_SAMPLER_RAM_BASE) == reinterpret_cast<void*>(0x25A00000u + SAT_SAMPLER_RAM_BASE));
    assert(sat_sampler_ram(SAT_SAMPLER_RAM_BASE - 0x1000u) == nullptr);

    // sat_audio_shutdown's reset ends it
    reset_runtime_state();
    assert(sat_sampler_is_initialized() == 0u && claim::g_claim.slots == 0u);
    assert(sat_sampler_commit() == SAT_ERR_NOT_INITIALIZED);
    assert(choose_voice(0u) == 0);

    // a partial claim leaves the other slots to resident voices
    sat_sampler_config_t mid = {4u, 2u, 4u, 0u, 0x20000u, 0x1000u};
    assert(sat_sampler_init(&mid) == SAT_OK);
    for (uint16_t i = 0u; i < 4u; ++i) g_voice_registry.entries[i].active = 1u;
    assert(choose_voice(0u) == 8); /* 4-7 are the sampler's */
    for (uint16_t i = 0u; i < 4u; ++i) g_voice_registry.entries[i].active = 0u;
    assert(g_ram.allocate(kSoundRamBase, kResidentSoundRamEnd, 0x100u, 2u, &sound, &at) && at == kSoundRamBase);
    assert(sat_sampler_shutdown() == SAT_OK);
    assert(g_ram.release(sound) && g_ram.live_count == 0u);
}

static void voices() {
    sat_sampler_config_t config = {0u, 4u, 4u, 0u, SAT_SAMPLER_RAM_BASE, 0x10000u};
    assert(sat_sampler_init(&config) == SAT_OK);
    const sat_sampler_note_t a = {SAT_SAMPLER_RAM_BASE, 0u, 99u, SAT_AUDIO_PCM_S8, 0u};
    sat_sampler_note_t loop = {0xC000u, 10u, 200u, SAT_AUDIO_PCM_S8, 1u};

    sat_sampler_note_t bad = a;
    bad.offset = SAT_SAMPLER_RAM_BASE - 0x1000u;
    assert(sat_sampler_voice_start(0u, &bad) == SAT_ERR_INVALID_ARG);
    bad = a;
    bad.offset = SAT_SAMPLER_RAM_BASE + 0x10000u - 50u; /* LEA 99 reads past the claimed RAM */
    assert(sat_sampler_voice_start(0u, &bad) == SAT_ERR_INVALID_ARG);
    bad = loop;
    bad.loop_start = 200u;
    assert(sat_sampler_voice_start(0u, &bad) == SAT_ERR_INVALID_ARG);
    assert(sat_sampler_voice_start(4u, &a) == SAT_ERR_INVALID_ARG);

    // staged: nothing sounds before the commit; then the voice's second slot does
    assert(sat_sampler_voice_start(0u, &a) == SAT_OK);
    assert(sat_sampler_voice_start(1u, &loop) == SAT_OK);
    assert(sounding_count() == 0u && sat_sampler_voice_active(0u) == 1u);
    assert(g_word[1][0x0C / 2] == 0xFFu); /* silent until set */
    const uint32_t kyonex = g_kyonex;
    assert(sat_sampler_commit() == SAT_OK);
    assert(g_kyonex == kyonex + 1u && g_waits >= 1u);
    assert(g_sounding[1] && g_sounding[3] && sounding_count() == 2u);
    assert(g_word[3][0x00 / 2] == 0x30u && g_word[3][0x04 / 2] == 10u && g_word[3][0x06 / 2] == 200u);
    assert(sat_sampler_commit() == SAT_OK && g_kyonex == kyonex + 1u); /* nothing staged: no KYONEX */

    // level and pitch: written on change only
    const uint32_t levels = g_level_writes, pitches = g_pitch_writes;
    assert(sat_sampler_voice_set(0u, 88200u, 255u, 0u, 0u) == SAT_OK);
    assert(g_word[1][0x0C / 2] == 0u && g_word[1][0x16 / 2] == (4u << 13 | 0x1Fu << 8) && g_word[1][0x10 / 2] == 0x0800u);
    assert(sat_sampler_voice_set(0u, 88200u, 255u, 0u, 0u) == SAT_OK);
    assert(g_level_writes == levels + 1u && g_pitch_writes == pitches + 1u);

    // a new note on voice 0 goes to slot 0; slot 1 is keyed off in the same KYONEX
    assert(sat_sampler_voice_start(0u, &loop) == SAT_OK);
    assert(g_sounding[1] && !g_sounding[0]);
    assert(sat_sampler_voice_set(0u, 44100u, 200u, 200u, 0u) == SAT_OK);
    assert(g_word[0][0x10 / 2] == 0u); /* the new slot got the pitch, not the old one */
    assert(sat_sampler_commit() == SAT_OK);
    assert(g_sounding[0] && !g_sounding[1]);

    // started twice in one frame: the second note reuses the slot that has not sounded
    assert(sat_sampler_voice_start(2u, &a) == SAT_OK);
    assert(sat_sampler_voice_start(2u, &loop) == SAT_OK);
    assert(sat_sampler_commit() == SAT_OK);
    assert(g_sounding[5] && !g_sounding[4] && g_word[5][0x02 / 2] == 0xC000u);

    // stop
    assert(sat_sampler_voice_stop(2u) == SAT_OK && sat_sampler_voice_active(2u) == 0u);
    assert(sat_sampler_voice_stop(2u) == SAT_OK);
    assert(sat_sampler_commit() == SAT_OK && !g_sounding[5]);
    assert(sat_sampler_voice_set(2u, 44100u, 255u, 255u, 0u) == SAT_OK); /* no note: ignored */

    // mute_range: voices 0 and 1 play from 0xC000; voice 3 from SAT_SAMPLER_RAM_BASE stays
    assert(sat_sampler_voice_start(3u, &a) == SAT_OK);
    assert(sat_sampler_commit() == SAT_OK);
    const uint32_t muted_kyonex = g_kyonex;
    assert(sat_sampler_mute_range(0xC000u, 0x1000u) == 2u);
    assert(g_kyonex == muted_kyonex + 1u);
    assert(!g_sounding[0] && !g_sounding[3] && g_sounding[7]);
    assert(g_word[0][0x0C / 2] == 0xFFu && g_word[3][0x0C / 2] == 0xFFu);
    assert(sat_sampler_voice_active(0u) == 0u && sat_sampler_voice_active(3u) == 1u);
    assert(sat_sampler_mute_range(0xC000u, 0x1000u) == 0u);

    sat_sampler_stats_t stats = {};
    assert(sat_sampler_get_stats(&stats) == SAT_OK);
    assert(stats.key_ons == 6u && stats.range_mutes == 2u && stats.active == 1u);

    assert(sat_sampler_shutdown() == SAT_OK);
    assert(sounding_count() == 0u && sat_sampler_voice_start(0u, &a) == SAT_ERR_NOT_INITIALIZED);
}

int main() {
    g_ram.reset();
    g_voice_registry.reset();
    conversions();
    claims();
    voices();
    std::printf("test_sampler_api: OK\n");
    return 0;
}
