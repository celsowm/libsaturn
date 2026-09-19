#include "saturn/audio.h"

#include "saturn/video.h"
#include "src/core/audio_stream_runtime.hpp"
#include "src/core/runtime_state.hpp"
#include "src/hal/scsp.hpp"
#include "src/hal/vdp2.hpp"

namespace saturn::core {
void music_runtime_reset();
}

namespace {

constexpr uint16_t kSoundCapacity = 24u;
constexpr uint16_t kVoiceCapacity = 32u;
constexpr uint16_t kResidentVoiceCapacity = kVoiceCapacity - saturn::core::kAudioStreamCapacity;
constexpr uint16_t kAllocationCapacity = 40u;
constexpr uint32_t kSoundRamBase = saturn::hal::scsp::kSystemReservedBytes;
constexpr uint32_t kResidentSoundRamEnd = saturn::core::kAudioStreamRamBase;

struct AllocationEntry {
    uint32_t offset;
    uint32_t size;
    uint8_t used;
};

struct SoundEntry {
    uint32_t ram_offset;
    uint32_t byte_count;
    uint32_t sample_count;
    uint32_t sample_rate;
    uint16_t loop_start;
    uint16_t loop_end;
    uint16_t generation;
    uint16_t allocation_slot;
    uint8_t format;
    uint8_t loop;
    uint8_t used;
    uint8_t reserved;
};

struct VoiceEntry {
    uint32_t end_frame;
    uint32_t start_serial;
    uint16_t generation;
    uint16_t sound_slot;
    uint16_t sound_generation;
    uint16_t priority;
    uint16_t volume;
    int16_t pan;
    uint8_t active;
    uint8_t looping;
};

AllocationEntry g_allocations[kAllocationCapacity] = {};
SoundEntry g_sounds[kSoundCapacity] = {};
VoiceEntry g_voices[kVoiceCapacity] = {};
uint32_t g_sound_ram_used = 0u;
uint32_t g_sound_ram_high_water = 0u;
uint32_t g_voice_steals = 0u;
uint32_t g_failed_play_requests = 0u;
uint32_t g_start_serial = 0u;
uint8_t g_initialized = 0u;
uint32_t g_audio_service_frame = 0u;
uint32_t g_audio_last_app_frame = 0u;
uint8_t g_audio_last_vblank = 0u;
uint8_t g_audio_clock_valid = 0u;

uint32_t align_up(uint32_t value, uint32_t alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

bool ranges_overlap(uint32_t a0, uint32_t a1, uint32_t b0, uint32_t b1) {
    return a0 < b1 && b0 < a1;
}

bool ram_allocate(uint32_t size, uint32_t alignment, uint16_t* out_slot, uint32_t* out_offset) {
    if (out_slot == nullptr || out_offset == nullptr || size == 0u) return false;

    uint16_t metadata_slot = kAllocationCapacity;
    for (uint16_t i = 0; i < kAllocationCapacity; ++i) {
        if (g_allocations[i].used == 0u) {
            metadata_slot = i;
            break;
        }
    }
    if (metadata_slot == kAllocationCapacity) return false;

    uint32_t candidate = align_up(kSoundRamBase, alignment);
    while (candidate <= kResidentSoundRamEnd && size <= (kResidentSoundRamEnd - candidate)) {
        bool collision = false;
        uint32_t bump_to = candidate;
        for (uint16_t i = 0; i < kAllocationCapacity; ++i) {
            if (g_allocations[i].used == 0u) continue;
            const uint32_t begin = g_allocations[i].offset;
            const uint32_t end = begin + g_allocations[i].size;
            if (ranges_overlap(candidate, candidate + size, begin, end)) {
                const uint32_t bumped = align_up(end, alignment);
                if (bumped > bump_to) bump_to = bumped;
                collision = true;
            }
        }
        if (!collision) {
            AllocationEntry& entry = g_allocations[metadata_slot];
            entry.offset = candidate;
            entry.size = size;
            entry.used = 1u;
            g_sound_ram_used += size;
            if (g_sound_ram_used > g_sound_ram_high_water) g_sound_ram_high_water = g_sound_ram_used;
            *out_slot = metadata_slot;
            *out_offset = candidate;
            return true;
        }
        if (bump_to <= candidate) return false;
        candidate = bump_to;
    }
    return false;
}

void ram_free(uint16_t slot) {
    if (slot >= kAllocationCapacity || g_allocations[slot].used == 0u) return;
    if (g_sound_ram_used >= g_allocations[slot].size) g_sound_ram_used -= g_allocations[slot].size;
    else g_sound_ram_used = 0u;
    g_allocations[slot] = {};
}

SoundEntry* resolve_sound(sat_sound_t sound) {
    if (sound.slot >= kSoundCapacity) return nullptr;
    SoundEntry& entry = g_sounds[sound.slot];
    if (entry.used == 0u || entry.generation != sound.generation) return nullptr;
    return &entry;
}

VoiceEntry* resolve_voice(sat_voice_t voice) {
    if (voice.slot >= kVoiceCapacity) return nullptr;
    VoiceEntry& entry = g_voices[voice.slot];
    if (entry.active == 0u || entry.generation != voice.generation) return nullptr;
    return &entry;
}

uint8_t volume_to_tl(uint16_t volume) {
    if (volume > SAT_AUDIO_VOLUME_MAX) volume = SAT_AUDIO_VOLUME_MAX;
    if (volume == 0u) return 0xFFu;
    return static_cast<uint8_t>(((SAT_AUDIO_VOLUME_MAX - volume) * 64u + 127u) / 255u);
}

uint32_t duration_frames(uint32_t sample_count, uint32_t sample_rate, uint32_t pitch_q16) {
    if (sample_rate == 0u || pitch_q16 == 0u) return 1u;
    const uint64_t numerator = static_cast<uint64_t>(sample_count) * 60u * 65536u;
    const uint64_t denominator = static_cast<uint64_t>(sample_rate) * pitch_q16;
    uint64_t frames = (numerator + denominator - 1u) / denominator;
    if (frames == 0u) frames = 1u;
    if (frames > 0x7FFFFFFFu) frames = 0x7FFFFFFFu;
    return static_cast<uint32_t>(frames);
}

void release_voice(uint16_t slot) {
    if (slot >= kVoiceCapacity || g_voices[slot].active == 0u) return;
    saturn::hal::scsp::key_off(static_cast<uint8_t>(slot));
    g_voices[slot].active = 0u;
}

int32_t choose_voice(uint16_t priority) {
    for (uint16_t i = 0; i < kResidentVoiceCapacity; ++i) {
        if (g_voices[i].active == 0u) return static_cast<int32_t>(i);
    }

    int32_t best = -1;
    for (uint16_t i = 0; i < kResidentVoiceCapacity; ++i) {
        const VoiceEntry& v = g_voices[i];
        if (v.looping != 0u || v.priority > priority) continue;
        if (best < 0 || v.priority < g_voices[best].priority ||
            (v.priority == g_voices[best].priority && v.start_serial < g_voices[best].start_serial)) {
            best = static_cast<int32_t>(i);
        }
    }
    if (best >= 0) ++g_voice_steals;
    return best;
}

void reset_runtime_state() {
    for (uint16_t i = 0; i < kAllocationCapacity; ++i) g_allocations[i] = {};
    for (uint16_t i = 0; i < kSoundCapacity; ++i) {
        uint16_t generation = static_cast<uint16_t>(g_sounds[i].generation + 1u);
        if (generation == 0u) generation = 1u;
        g_sounds[i] = {};
        g_sounds[i].generation = generation;
    }
    for (uint16_t i = 0; i < kVoiceCapacity; ++i) {
        uint16_t generation = static_cast<uint16_t>(g_voices[i].generation + 1u);
        if (generation == 0u) generation = 1u;
        g_voices[i] = {};
        g_voices[i].generation = generation;
    }
    g_sound_ram_used = 0u;
    g_sound_ram_high_water = 0u;
    g_voice_steals = 0u;
    g_failed_play_requests = 0u;
    g_start_serial = 0u;
    g_audio_service_frame = 0u;
    g_audio_last_app_frame = 0u;
    g_audio_last_vblank = 0u;
    g_audio_clock_valid = 0u;
}

}  // namespace

extern "C" sat_result_t sat_audio_init(void) {
    if (g_initialized != 0u) return SAT_OK;
    saturn::core::music_runtime_reset();
    saturn::core::audio_stream_registry_reset(saturn::core::g_audio_streams);
    reset_runtime_state();
    if (!saturn::hal::scsp::init()) return SAT_ERR_NOT_INITIALIZED;
    g_initialized = 1u;
    return SAT_OK;
}

extern "C" sat_result_t sat_audio_shutdown(void) {
    if (g_initialized == 0u) return SAT_OK;
    saturn::hal::scsp::shutdown();
    saturn::core::audio_stream_registry_reset(saturn::core::g_audio_streams);
    saturn::core::music_runtime_reset();
    reset_runtime_state();
    g_initialized = 0u;
    return SAT_OK;
}

extern "C" uint8_t sat_audio_is_initialized(void) {
    return g_initialized;
}

extern "C" sat_result_t sat_audio_update(void) {
    if (g_initialized == 0u) return SAT_ERR_NOT_INITIALIZED;
    const uint32_t now = sat_frame_count();
    const uint8_t vblank =
        (saturn::hal::vdp2::read_tvstat() & 0x0008u) != 0u ? 1u : 0u;
    if (g_audio_clock_valid == 0u) {
        g_audio_service_frame = now;
        g_audio_last_app_frame = now;
        g_audio_clock_valid = 1u;
    } else if (now != g_audio_last_app_frame) {
        // sat_frame_count() catches up missed display frames when the app
        // returns to its normal VBlank path. Those same frames may already
        // have advanced g_audio_service_frame through TVSTAT while a
        // synchronous CD read was pumping audio. Reconcile the two clocks
        // instead of adding the elapsed interval a second time: double
        // counting can make the stream scheduler overwrite an SCSP buffer
        // half that is still being played.
        if (static_cast<int32_t>(now - g_audio_service_frame) > 0) {
            g_audio_service_frame = now;
        }
        g_audio_last_app_frame = now;
    } else if (vblank != 0u && g_audio_last_vblank == 0u) {
        // CD Block waits pump this function while the application frame is
        // stalled. Count real VBlank edges so streaming time still advances.
        ++g_audio_service_frame;
    }
    g_audio_last_vblank = vblank;
    const uint32_t display_rate = saturn::core::g_state.config.ntsc != 0u ? 60u : 50u;
    saturn::core::audio_stream_service(
        saturn::core::g_audio_streams, g_audio_service_frame, display_rate);
    for (uint16_t i = 0; i < kResidentVoiceCapacity; ++i) {
        VoiceEntry& voice = g_voices[i];
        if (voice.active == 0u || voice.looping != 0u) continue;
        if (static_cast<int32_t>(now - voice.end_frame) >= 0) release_voice(i);
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_audio_set_master_volume(uint16_t volume) {
    if (g_initialized == 0u) return SAT_ERR_NOT_INITIALIZED;
    if (volume > SAT_AUDIO_VOLUME_MAX) volume = SAT_AUDIO_VOLUME_MAX;
    const uint8_t level = static_cast<uint8_t>((volume * 15u + 127u) / 255u);
    saturn::hal::scsp::set_master_volume(level);
    return SAT_OK;
}

extern "C" sat_result_t sat_audio_get_stats(sat_audio_stats_t* out_stats) {
    if (out_stats == nullptr) return SAT_ERR_INVALID_ARG;
    if (g_initialized == 0u) return SAT_ERR_NOT_INITIALIZED;

    uint16_t sounds = 0u;
    uint16_t voices = 0u;
    for (uint16_t i = 0; i < kSoundCapacity; ++i) sounds += g_sounds[i].used != 0u ? 1u : 0u;
    for (uint16_t i = 0; i < kResidentVoiceCapacity; ++i) voices += g_voices[i].active != 0u ? 1u : 0u;

    out_stats->sound_ram_used = g_sound_ram_used;
    out_stats->sound_ram_capacity = kResidentSoundRamEnd - kSoundRamBase;
    out_stats->sound_ram_high_water = g_sound_ram_high_water;
    out_stats->voice_steals = g_voice_steals;
    out_stats->failed_play_requests = g_failed_play_requests;
    out_stats->resident_sounds = sounds;
    out_stats->resident_sound_capacity = kSoundCapacity;
    out_stats->active_voices = voices;
    out_stats->voice_capacity = kResidentVoiceCapacity;
    return SAT_OK;
}

extern "C" sat_result_t sat_sound_create(sat_sound_t* out_sound, const sat_sound_desc_t* desc) {
    if (out_sound == nullptr || desc == nullptr || desc->samples == nullptr) return SAT_ERR_INVALID_ARG;
    if (g_initialized == 0u) return SAT_ERR_NOT_INITIALIZED;
    if (desc->sample_count == 0u || desc->sample_count > 65535u || desc->sample_rate == 0u) return SAT_ERR_INVALID_ARG;
    if (desc->format != SAT_AUDIO_PCM_S8 && desc->format != SAT_AUDIO_PCM_S16) return SAT_ERR_UNSUPPORTED;

    uint16_t sound_slot = kSoundCapacity;
    for (uint16_t i = 0; i < kSoundCapacity; ++i) {
        if (g_sounds[i].used == 0u) {
            sound_slot = i;
            break;
        }
    }
    if (sound_slot == kSoundCapacity) return SAT_ERR_CAPACITY;

    const uint32_t bytes_per_sample = desc->format == SAT_AUDIO_PCM_S16 ? 2u : 1u;
    const uint32_t byte_count = desc->sample_count * bytes_per_sample;
    uint16_t allocation_slot = 0u;
    uint32_t offset = 0u;
    if (!ram_allocate(byte_count, 2u, &allocation_slot, &offset)) return SAT_ERR_CAPACITY;

    if (!saturn::hal::scsp::upload(offset, desc->samples, byte_count)) {
        ram_free(allocation_slot);
        return SAT_ERR_INVALID_ARG;
    }

    SoundEntry& entry = g_sounds[sound_slot];
    uint16_t generation = static_cast<uint16_t>(entry.generation + 1u);
    if (generation == 0u) generation = 1u;
    entry = {};
    entry.used = 1u;
    entry.generation = generation;
    entry.ram_offset = offset;
    entry.byte_count = byte_count;
    entry.sample_count = desc->sample_count;
    entry.sample_rate = desc->sample_rate;
    entry.format = desc->format;
    entry.loop = desc->loop != 0u ? 1u : 0u;
    entry.loop_start = desc->loop_start;
    entry.loop_end = desc->loop_end;
    entry.allocation_slot = allocation_slot;

    if (entry.loop != 0u) {
        if (entry.loop_end == 0u || entry.loop_end >= entry.sample_count) {
            entry.loop_end = static_cast<uint16_t>(entry.sample_count - 1u);
        }
        if (entry.loop_start >= entry.loop_end) entry.loop_start = 0u;
    } else {
        entry.loop_start = 0u;
        entry.loop_end = static_cast<uint16_t>(entry.sample_count - 1u);
    }

    out_sound->slot = sound_slot;
    out_sound->generation = generation;
    return SAT_OK;
}

extern "C" sat_result_t sat_sound_unload(sat_sound_t sound) {
    if (g_initialized == 0u) return SAT_ERR_NOT_INITIALIZED;
    SoundEntry* entry = resolve_sound(sound);
    if (entry == nullptr) return SAT_ERR_INVALID_ARG;

    sat_sound_stop_all_instances(sound);
    ram_free(entry->allocation_slot);
    uint16_t generation = static_cast<uint16_t>(entry->generation + 1u);
    if (generation == 0u) generation = 1u;
    *entry = {};
    entry->generation = generation;
    return SAT_OK;
}

extern "C" sat_result_t sat_sound_play(sat_sound_t sound, const sat_sound_play_params_t* params, sat_voice_t* out_voice) {
    if (g_initialized == 0u) return SAT_ERR_NOT_INITIALIZED;
    SoundEntry* entry = resolve_sound(sound);
    if (entry == nullptr) return SAT_ERR_INVALID_ARG;

    uint16_t volume = SAT_AUDIO_VOLUME_MAX;
    int16_t pan = SAT_AUDIO_PAN_CENTER;
    uint16_t priority = 0u;
    uint32_t pitch_q16 = SAT_FX16_ONE;
    if (params != nullptr) {
        volume = params->volume;
        pan = params->pan;
        priority = params->priority;
        if (params->pitch > 0) pitch_q16 = static_cast<uint32_t>(params->pitch);
    }
    if (volume > SAT_AUDIO_VOLUME_MAX) volume = SAT_AUDIO_VOLUME_MAX;
    if (pan < SAT_AUDIO_PAN_LEFT) pan = SAT_AUDIO_PAN_LEFT;
    if (pan > SAT_AUDIO_PAN_RIGHT) pan = SAT_AUDIO_PAN_RIGHT;

    const int32_t selected = choose_voice(priority);
    if (selected < 0) {
        ++g_failed_play_requests;
        return SAT_ERR_CAPACITY;
    }
    const uint16_t voice_slot = static_cast<uint16_t>(selected);
    if (g_voices[voice_slot].active != 0u) release_voice(voice_slot);

    saturn::hal::scsp::SlotConfig config = {};
    config.start_address = entry->ram_offset;
    config.sample_rate = entry->sample_rate;
    config.sample_count = static_cast<uint16_t>(entry->sample_count);
    config.loop_start = entry->loop_start;
    config.loop_end = entry->loop_end;
    config.pitch_scale_q16 = pitch_q16;
    config.pcm8 = entry->format == SAT_AUDIO_PCM_S8 ? 1u : 0u;
    config.loop = entry->loop;
    config.total_level = volume_to_tl(volume);
    config.direct_level = 7u;
    config.pan = saturn::hal::scsp::encode_pan(pan);
    if (!saturn::hal::scsp::configure_slot(static_cast<uint8_t>(voice_slot), config)) {
        ++g_failed_play_requests;
        return SAT_ERR_UNSUPPORTED;
    }

    VoiceEntry& voice = g_voices[voice_slot];
    uint16_t generation = static_cast<uint16_t>(voice.generation + 1u);
    if (generation == 0u) generation = 1u;
    voice = {};
    voice.generation = generation;
    voice.active = 1u;
    voice.looping = entry->loop;
    voice.sound_slot = sound.slot;
    voice.sound_generation = sound.generation;
    voice.priority = priority;
    voice.volume = volume;
    voice.pan = pan;
    voice.start_serial = ++g_start_serial;
    voice.end_frame = sat_frame_count() + duration_frames(entry->sample_count, entry->sample_rate, pitch_q16);

    saturn::hal::scsp::key_on(static_cast<uint8_t>(voice_slot));

    if (out_voice != nullptr) {
        out_voice->slot = voice_slot;
        out_voice->generation = generation;
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_sound_stop_all_instances(sat_sound_t sound) {
    if (g_initialized == 0u) return SAT_ERR_NOT_INITIALIZED;
    if (resolve_sound(sound) == nullptr) return SAT_ERR_INVALID_ARG;
    for (uint16_t i = 0; i < kVoiceCapacity; ++i) {
        VoiceEntry& voice = g_voices[i];
        if (voice.active != 0u && voice.sound_slot == sound.slot && voice.sound_generation == sound.generation) {
            release_voice(i);
        }
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_voice_stop(sat_voice_t voice) {
    if (g_initialized == 0u) return SAT_ERR_NOT_INITIALIZED;
    VoiceEntry* entry = resolve_voice(voice);
    if (entry == nullptr) return SAT_ERR_INVALID_ARG;
    release_voice(voice.slot);
    return SAT_OK;
}

extern "C" sat_result_t sat_voice_set_volume(sat_voice_t voice, uint16_t volume) {
    if (g_initialized == 0u) return SAT_ERR_NOT_INITIALIZED;
    VoiceEntry* entry = resolve_voice(voice);
    if (entry == nullptr) return SAT_ERR_INVALID_ARG;
    if (volume > SAT_AUDIO_VOLUME_MAX) volume = SAT_AUDIO_VOLUME_MAX;
    entry->volume = volume;
    saturn::hal::scsp::set_slot_level_pan(
        static_cast<uint8_t>(voice.slot), volume_to_tl(volume), 7u,
        saturn::hal::scsp::encode_pan(entry->pan)
    );
    return SAT_OK;
}

extern "C" sat_result_t sat_voice_set_pan(sat_voice_t voice, int16_t pan) {
    if (g_initialized == 0u) return SAT_ERR_NOT_INITIALIZED;
    VoiceEntry* entry = resolve_voice(voice);
    if (entry == nullptr) return SAT_ERR_INVALID_ARG;
    if (pan < SAT_AUDIO_PAN_LEFT) pan = SAT_AUDIO_PAN_LEFT;
    if (pan > SAT_AUDIO_PAN_RIGHT) pan = SAT_AUDIO_PAN_RIGHT;
    entry->pan = pan;
    saturn::hal::scsp::set_slot_level_pan(
        static_cast<uint8_t>(voice.slot), volume_to_tl(entry->volume), 7u,
        saturn::hal::scsp::encode_pan(pan)
    );
    return SAT_OK;
}

extern "C" uint8_t sat_voice_is_playing(sat_voice_t voice) {
    if (g_initialized == 0u) return 0u;
    return resolve_voice(voice) != nullptr ? 1u : 0u;
}
