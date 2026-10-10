#include "saturn/sampler.h"

#include "src/audio/playback/claim.hpp"
#include "src/audio/playback/state.hpp"
#include "src/audio/sampler/logic.hpp"
#include "src/audio/streaming/runtime.hpp"
#include "src/hal/scsp/scsp.hpp"

namespace saturn::core::audio::sampler {

static_assert(SAT_SAMPLER_RAM_BASE == saturn::hal::scsp::kSystemReservedBytes, "Sound RAM above the driver's");

struct State {
    Voice voices[SAT_SAMPLER_MAX_VOICES];
    sat_sampler_stats_t stats;
    uint32_t ram_begin;
    uint32_t ram_end;
    uint16_t ram_allocation; /* playback g_ram slot holding the range */
    uint8_t first_slot;
    uint8_t voice_count;
    uint8_t direct_level;
    uint8_t pending;         /* a key change is staged */
    uint8_t initialized;
};

State g_sampler = {};

namespace {

void reset() {
    g_sampler = {};
    claim::g_claim = {};
}

/* sat_audio_shutdown clears the claim (reset_runtime_state): the sampler ended with it. */
bool live() {
    if (g_sampler.initialized != 0u && claim::g_claim.slots == 0u) g_sampler = {};
    return g_sampler.initialized != 0u;
}

constexpr uint32_t kSoundRamCpu = 0x25A00000u;

using namespace saturn::core::audio::playback;

Voice* voice_at(uint8_t index) {
    if (!live() || index >= g_sampler.voice_count) return nullptr;
    return &g_sampler.voices[index];
}

uint8_t base_slot(uint8_t index) {
    return static_cast<uint8_t>(g_sampler.first_slot + index * 2u);
}

void stage_off(Voice& v) {
    if (v.keyed == 0u) return;
    v.keyed = 0u;
    v.fresh = 0u;
    saturn::hal::scsp::stage_key(v.slot, false);
    g_sampler.pending = 1u;
    ++g_sampler.stats.key_offs;
}

bool claim_conflicts(uint32_t slots, uint32_t ram_begin, uint32_t ram_end) {
    for (uint16_t i = 0u; i < kVoiceCapacity; ++i) {
        if (((slots >> i) & 1u) != 0u && g_voice_registry.entries[i].active != 0u) return true;
    }
    for (uint16_t i = 0u; i < kAudioStreamCapacity; ++i) {
        const AudioStreamSlot& stream = g_audio_streams.slots[i];
        if (stream.used == 0u) continue;
        const uint32_t begin = kAudioStreamRamBase + static_cast<uint32_t>(i) * 2u * kAudioStreamChunkBytes;
        if (((slots >> (kAudioStreamScspSlotBase + i)) & 1u) != 0u ||
            (ram_begin < ram_end && begin < ram_end && begin + 2u * kAudioStreamChunkBytes > ram_begin))
            return true;
    }
    return false;
}

}  // namespace
}  // namespace saturn::core::audio::sampler

using namespace saturn::core::audio::sampler;
namespace claim = saturn::core::audio::claim;
namespace playback = saturn::core::audio::playback;

extern "C" sat_result_t sat_sampler_init(const sat_sampler_config_t* config) {
    if (config == nullptr || config->voice_count == 0u || config->voice_count > SAT_SAMPLER_MAX_VOICES ||
        config->first_slot + 2u * config->voice_count > saturn::hal::scsp::kSlotCount ||
        config->direct_level > 7u || (config->ram_offset & 1u) != 0u ||
        (config->ram_bytes != 0u &&
         (config->ram_offset < SAT_SAMPLER_RAM_BASE || config->ram_offset > SAT_SAMPLER_RAM_END ||
          config->ram_bytes > SAT_SAMPLER_RAM_END - config->ram_offset)))
        return SAT_ERR_INVALID_ARG;
    if (playback::g_initialized == 0u) return SAT_ERR_NOT_INITIALIZED;
    if (live()) return SAT_ERR_BUSY;

    const uint32_t slots = ((config->voice_count == 16u ? 0xFFFFFFFFu : (1u << (2u * config->voice_count)) - 1u))
                           << config->first_slot;
    const uint32_t ram_begin = config->ram_offset;
    const uint32_t ram_end = config->ram_offset + config->ram_bytes;
    if (claim_conflicts(slots, ram_begin, ram_end)) return SAT_ERR_BUSY;
    uint16_t allocation = 0xFFFFu;
    if (config->ram_bytes != 0u) {
        /* An exact-range allocation: it fits only if no sound or effect ring is there. */
        uint32_t offset = 0u;
        if (!playback::g_ram.allocate(ram_begin, ram_end, config->ram_bytes, 2u, &allocation, &offset)) {
            return SAT_ERR_BUSY;
        }
    }

    g_sampler = {};
    g_sampler.ram_begin = ram_begin;
    g_sampler.ram_end = ram_end;
    g_sampler.ram_allocation = allocation;
    g_sampler.first_slot = config->first_slot;
    g_sampler.voice_count = config->voice_count;
    g_sampler.direct_level = config->direct_level;
    for (uint8_t i = 0u; i < config->voice_count; ++i) {
        Voice& v = g_sampler.voices[i];
        v.slot = base_slot(i);
        v.pitch = kNoWord;
        v.level = kNoWord;
        v.at[0] = v.at[1] = 0xFFFFFFFFu;
    }
    claim::g_claim.slots = slots;
    claim::g_claim.ram_begin = ram_begin;
    claim::g_claim.ram_end = ram_end;
    g_sampler.initialized = 1u;
    return SAT_OK;
}

extern "C" sat_result_t sat_sampler_shutdown(void) {
    if (!live()) return SAT_OK;
    for (uint8_t i = 0u; i < g_sampler.voice_count; ++i) {
        const uint8_t base = base_slot(i);
        saturn::hal::scsp::mute_slot(base);
        saturn::hal::scsp::mute_slot(static_cast<uint8_t>(base + 1u));
        saturn::hal::scsp::stage_key(base, false);
        saturn::hal::scsp::stage_key(static_cast<uint8_t>(base + 1u), false);
    }
    saturn::hal::scsp::execute_key_transitions();
    if (g_sampler.ram_allocation != 0xFFFFu) (void)playback::g_ram.release(g_sampler.ram_allocation);
    reset();
    return SAT_OK;
}

extern "C" uint8_t sat_sampler_is_initialized(void) {
    return live() ? 1u : 0u;
}

extern "C" void* sat_sampler_ram(uint32_t offset) {
    if (!live() || offset < g_sampler.ram_begin || offset >= g_sampler.ram_end) return nullptr;
    return reinterpret_cast<void*>(static_cast<uintptr_t>(kSoundRamCpu + offset));
}

extern "C" sat_result_t sat_sampler_voice_start(uint8_t voice, const sat_sampler_note_t* note) {
    Voice* const v = voice_at(voice);
    if (v == nullptr) return g_sampler.initialized == 0u ? SAT_ERR_NOT_INITIALIZED : SAT_ERR_INVALID_ARG;
    const uint32_t bytes_per_sample = note != nullptr && note->format == SAT_AUDIO_PCM_S16 ? 2u : 1u;
    if (note == nullptr || (note->format != SAT_AUDIO_PCM_S8 && note->format != SAT_AUDIO_PCM_S16) ||
        note->offset < g_sampler.ram_begin || note->offset >= g_sampler.ram_end ||
        (bytes_per_sample == 2u && (note->offset & 1u) != 0u) ||
        (static_cast<uint32_t>(note->loop_end) + 1u) * bytes_per_sample > g_sampler.ram_end - note->offset ||
        (note->loop != 0u && note->loop_start >= note->loop_end))
        return SAT_ERR_INVALID_ARG;

    const uint8_t base = base_slot(voice);
    const uint8_t slot = next_slot(*v, base);
    if (slot != v->slot) stage_off(*v);
    saturn::hal::scsp::SlotConfig config = {};
    config.start_address = note->offset;
    config.loop_start = note->loop_start;
    config.loop_end = note->loop_end;
    config.pcm8 = note->format == SAT_AUDIO_PCM_S8 ? 1u : 0u;
    config.loop = note->loop != 0u ? 1u : 0u;
    config.total_level = 0xFFu; /* silent until voice_set */
    config.direct_level = g_sampler.direct_level;
    config.pan = 0u;
    if (!saturn::hal::scsp::stage_slot(slot, config, kLowestPitch)) return SAT_ERR_NOT_INITIALIZED;
    v->slot = slot;
    v->at[slot - base] = note->offset;
    v->pitch = kLowestPitch;
    v->level = 0x00FFu;
    v->keyed = 1u;
    v->fresh = 1u;
    saturn::hal::scsp::stage_key(slot, true);
    g_sampler.pending = 1u;
    ++g_sampler.stats.key_ons;
    return SAT_OK;
}

extern "C" sat_result_t sat_sampler_voice_stop(uint8_t voice) {
    Voice* const v = voice_at(voice);
    if (v == nullptr) return g_sampler.initialized == 0u ? SAT_ERR_NOT_INITIALIZED : SAT_ERR_INVALID_ARG;
    stage_off(*v);
    return SAT_OK;
}

extern "C" sat_result_t sat_sampler_voice_set(uint8_t voice, uint32_t rate_hz, uint8_t left, uint8_t right,
                                              uint8_t attenuation) {
    Voice* const v = voice_at(voice);
    if (v == nullptr) return g_sampler.initialized == 0u ? SAT_ERR_NOT_INITIALIZED : SAT_ERR_INVALID_ARG;
    if (v->keyed == 0u) return SAT_OK;
    const uint16_t level = level_word(left, right, attenuation);
    if (level != v->level) {
        v->level = level;
        saturn::hal::scsp::set_slot_level_pan(v->slot, static_cast<uint8_t>(level), g_sampler.direct_level,
                                              static_cast<uint8_t>(level >> 8));
    }
    const uint16_t pitch = pitch_word(rate_hz);
    if (pitch != v->pitch) {
        v->pitch = pitch;
        saturn::hal::scsp::set_slot_pitch(v->slot, pitch);
    }
    return SAT_OK;
}

extern "C" uint8_t sat_sampler_voice_active(uint8_t voice) {
    const Voice* const v = voice_at(voice);
    return v != nullptr ? v->keyed : 0u;
}

extern "C" sat_result_t sat_sampler_commit(void) {
    if (!live()) return SAT_ERR_NOT_INITIALIZED;
    uint32_t active = 0u;
    for (uint8_t i = 0u; i < g_sampler.voice_count; ++i) {
        g_sampler.voices[i].fresh = 0u;
        active += g_sampler.voices[i].keyed;
    }
    g_sampler.stats.active = active;
    if (g_sampler.pending != 0u) {
        g_sampler.pending = 0u;
        saturn::hal::scsp::execute_key_transitions();
        /* KYONEX takes effect at the next sample: a KYONB staged before then would be applied by
         * this one (the KYONEX latch race), so let it pass. */
        saturn::hal::scsp::wait_samples(1u);
        ++g_sampler.stats.commits;
    }
    return SAT_OK;
}

extern "C" uint32_t sat_sampler_mute_range(uint32_t offset, uint32_t bytes) {
    if (!live() || bytes == 0u) return 0u;
    const uint32_t end = offset + bytes < offset ? 0xFFFFFFFFu : offset + bytes;
    uint32_t stopped = 0u;
    for (uint8_t i = 0u; i < g_sampler.voice_count; ++i) {
        Voice& v = g_sampler.voices[i];
        const uint8_t base = base_slot(i);
        for (uint8_t k = 0u; k < 2u; ++k) {
            const uint8_t slot = static_cast<uint8_t>(base + k);
            if (!reads_range(v.at[k], offset, end)) continue;
            saturn::hal::scsp::mute_slot(slot);
            if (slot == v.slot) {
                v.level = kNoWord;
                if (v.keyed != 0u) {
                    stage_off(v);
                    ++stopped;
                }
            } else {
                saturn::hal::scsp::stage_key(slot, false);
                g_sampler.pending = 1u;
            }
            v.at[k] = 0xFFFFFFFFu;
        }
    }
    g_sampler.stats.range_mutes += stopped;
    (void)sat_sampler_commit();
    return stopped;
}

extern "C" sat_result_t sat_sampler_get_stats(sat_sampler_stats_t* out_stats) {
    if (out_stats == nullptr) return SAT_ERR_INVALID_ARG;
    *out_stats = g_sampler.stats;
    return SAT_OK;
}

extern "C" uint16_t sat_sampler_pitch_word(uint32_t rate_hz) {
    return pitch_word(rate_hz);
}

extern "C" uint8_t sat_sampler_total_level(uint8_t amplitude) {
    return kTotalLevel[amplitude];
}
