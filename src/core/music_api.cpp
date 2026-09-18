#include "saturn/audio.h"

#include <stddef.h>

#include "saturn/asset.h"
#include "src/core/audio_stream_runtime.hpp"

namespace {

struct MusicSlot {
    sat_audio_stream_t stream;
    sat_asset_t asset;
    const uint8_t* data;
    char logical_path[SAT_FILE_PATH_MAX];
    uint32_t payload_bytes;
    uint32_t sample_count;
    uint32_t sample_rate;
    uint32_t position_bytes;
    uint8_t channels;
    uint8_t format;
    uint8_t playing;
    uint8_t used;
    uint16_t generation;
    uint8_t buffer[SAT_MUSIC_BUFFER_FRAMES * 2u];
    uint8_t staging[SAT_MUSIC_BUFFER_FRAMES * 2u];
};

MusicSlot g_music[SAT_MUSIC_CAPACITY] = {};

uint32_t bytes_per_sample(uint8_t format) {
    if (format == SAT_AUDIO_PCM_S8) return 1u;
    if (format == SAT_AUDIO_PCM_S16) return 2u;
    return 0u;
}

uint16_t next_generation(uint16_t generation) {
    ++generation;
    return generation == 0u ? 1u : generation;
}

MusicSlot* resolve(sat_music_t music) {
    if (music.slot >= SAT_MUSIC_CAPACITY || music.generation == 0u) return nullptr;
    MusicSlot& slot = g_music[music.slot];
    if (slot.used == 0u || slot.generation != music.generation) return nullptr;
    return &slot;
}

sat_result_t feed(MusicSlot& slot) {
    if (slot.playing == 0u) return SAT_OK;
    uint32_t frames = sat_audio_stream_available(slot.stream);
    if (frames > 2048u) frames = 2048u;
    const uint32_t sample_bytes = bytes_per_sample(slot.format);
    while (frames != 0u) {
        uint32_t contiguous = (slot.payload_bytes - slot.position_bytes) / sample_bytes;
        if (contiguous > frames) contiguous = frames;
        if (contiguous == 0u) {
            slot.position_bytes = 0u;
            continue;
        }
        const uint32_t byte_count = contiguous * sample_bytes;
        const uint8_t* source = nullptr;
        if (slot.data != nullptr) {
            source = slot.data + slot.position_bytes;
        } else {
            uint32_t read = 0u;
            const sat_result_t read_status = sat_asset_read_at(
                slot.logical_path, slot.position_bytes, slot.staging, byte_count, &read);
            if (read_status != SAT_OK || read != byte_count) {
                return read_status == SAT_OK ? SAT_ERR_IO : read_status;
            }
            source = slot.staging;
        }
        const sat_result_t st = sat_audio_stream_write(
            slot.stream, source, contiguous);
        if (st != SAT_OK) return st;
        slot.position_bytes += byte_count;
        if (slot.position_bytes >= slot.payload_bytes) slot.position_bytes = 0u;
        frames -= contiguous;
    }
    return SAT_OK;
}

}  // namespace

extern "C" sat_result_t sat_music_open(sat_music_t* out_music, const char* logical_path) {
    if (out_music == nullptr || logical_path == nullptr) return SAT_ERR_INVALID_ARG;
    if (sat_audio_is_initialized() == 0u) return SAT_ERR_NOT_INITIALIZED;

    sat_asset_t asset{};
    SAT_TRY(sat_asset_open(logical_path, &asset));
    sat_asset_info_t info{};
    SAT_TRY(sat_asset_info(asset, &info));
    if (info.kind != SAT_ASSET_STREAM ||
        (info.data == nullptr && info.source_path == nullptr) || info.sample_rate == 0u ||
        info.channels != 1u) return SAT_ERR_UNSUPPORTED;
    const uint32_t sample_bytes = bytes_per_sample(info.format);
    if (sample_bytes == 0u || info.size == 0u || info.size % sample_bytes != 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    const uint32_t sample_count = info.sample_count == 0u
        ? info.size / sample_bytes : info.sample_count;
    if (sample_count == 0u || sample_count > info.size / sample_bytes) return SAT_ERR_INVALID_ARG;

    for (uint16_t i = 0u; i < SAT_MUSIC_CAPACITY; ++i) {
        MusicSlot& slot = g_music[i];
        if (slot.used != 0u) continue;
        if (slot.generation == 0u) slot.generation = 1u;
        uint16_t path_length = 0u;
        while (logical_path[path_length] != '\0') {
            if (path_length + 1u >= SAT_FILE_PATH_MAX) return SAT_ERR_CAPACITY;
            ++path_length;
        }
        sat_audio_spec_t spec = {
            info.sample_rate, SAT_MUSIC_BUFFER_FRAMES, info.channels, info.format, 0u};
        const sat_result_t st = sat_audio_stream_open(
            &slot.stream, &spec, slot.buffer, sizeof(slot.buffer));
        if (st != SAT_OK) return st;
        slot.asset = asset;
        slot.data = static_cast<const uint8_t*>(info.data);
        for (uint16_t j = 0u; j <= path_length; ++j) slot.logical_path[j] = logical_path[j];
        slot.payload_bytes = sample_count * sample_bytes;
        slot.sample_count = sample_count;
        slot.sample_rate = info.sample_rate;
        slot.position_bytes = 0u;
        slot.channels = info.channels;
        slot.format = info.format;
        slot.playing = 0u;
        slot.used = 1u;
        out_music->slot = i;
        out_music->generation = slot.generation;
        return SAT_OK;
    }
    return SAT_ERR_CAPACITY;
}

extern "C" sat_result_t sat_music_play(sat_music_t music) {
    MusicSlot* slot = resolve(music);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    SAT_TRY(sat_audio_stream_resume(slot->stream));
    slot->playing = 1u;
    return feed(*slot);
}

extern "C" sat_result_t sat_music_pause(sat_music_t music) {
    MusicSlot* slot = resolve(music);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    SAT_TRY(sat_audio_stream_pause(slot->stream));
    slot->playing = 0u;
    return SAT_OK;
}

extern "C" sat_result_t sat_music_resume(sat_music_t music) {
    return sat_music_play(music);
}

extern "C" sat_result_t sat_music_stop(sat_music_t music) {
    MusicSlot* slot = resolve(music);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    SAT_TRY(sat_audio_stream_pause(slot->stream));
    SAT_TRY(sat_audio_stream_flush(slot->stream));
    slot->position_bytes = 0u;
    slot->playing = 0u;
    return SAT_OK;
}

extern "C" sat_result_t sat_music_update(sat_music_t music) {
    MusicSlot* slot = resolve(music);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    return feed(*slot);
}

extern "C" sat_result_t sat_music_close(sat_music_t music) {
    MusicSlot* slot = resolve(music);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    SAT_TRY(sat_audio_stream_close(slot->stream));
    slot->used = 0u;
    slot->generation = next_generation(slot->generation);
    slot->data = nullptr;
    slot->asset = {};
    slot->logical_path[0] = '\0';
    slot->payload_bytes = 0u;
    slot->playing = 0u;
    return SAT_OK;
}

extern "C" sat_result_t sat_music_info(sat_music_t music, sat_music_info_t* out_info) {
    if (out_info == nullptr) return SAT_ERR_INVALID_ARG;
    const MusicSlot* slot = resolve(music);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    out_info->sample_rate = slot->sample_rate;
    out_info->sample_count = slot->sample_count;
    out_info->channels = slot->channels;
    out_info->format = slot->format;
    out_info->looping = 1u;
    out_info->playing = slot->playing;
    return SAT_OK;
}

extern "C" sat_result_t sat_music_stats(
    sat_music_t music, sat_audio_stream_stats_t* out_stats) {
    const MusicSlot* slot = resolve(music);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    return sat_audio_stream_stats(slot->stream, out_stats);
}

extern "C" uint8_t sat_music_is_playing(sat_music_t music) {
    const MusicSlot* slot = resolve(music);
    return slot == nullptr ? 0u : slot->playing;
}

extern "C" uint16_t sat_music_capacity(void) {
    return SAT_MUSIC_CAPACITY;
}

namespace saturn::core {

void music_runtime_reset() {
    for (uint16_t i = 0u; i < SAT_MUSIC_CAPACITY; ++i) {
        MusicSlot& slot = g_music[i];
        slot.used = 0u;
        slot.generation = next_generation(slot.generation);
        slot.data = nullptr;
        slot.asset = {};
        slot.logical_path[0] = '\0';
        slot.payload_bytes = 0u;
        slot.playing = 0u;
        slot.position_bytes = 0u;
    }
}

}  // namespace saturn::core
