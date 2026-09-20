#include "saturn/audio.h"

#include <stddef.h>

#include "saturn/asset.h"
#include "src/core/audio_stream_runtime.hpp"

namespace {

constexpr uint32_t kMusicFeedFrames = 2048u;
constexpr uint32_t kMusicFeedBytes = kMusicFeedFrames * 2u * 2u;

struct MusicSlot {
    sat_audio_stream_t stream;
    sat_audio_stream_t stream_r;  // right channel; valid only when stereo
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
    uint8_t buffer_r[SAT_MUSIC_BUFFER_FRAMES * 2u];
    uint8_t staging[kMusicFeedBytes];
    // Deinterleaved L/R channel frames for stereo feeds.
    uint8_t staging_split[kMusicFeedBytes];
};

/* ~288 KB of ring and staging buffers. Charging that to Work RAM High
 * would leave a program barely 300 KB for its own code and data, so the
 * slots live in Work RAM Low, which crt0 zeroes exactly like .bss. Low
 * RAM is the slower of the two, but a refill only runs once the SCSP has
 * moved to the other 4096-sample half, so there is most of that half's
 * time to copy -- orders of magnitude more than the copy costs. */
MusicSlot g_music[SAT_MUSIC_CAPACITY] __attribute__((section(".wram_l"))) = {};

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

sat_result_t write_channel(MusicSlot& slot, uint32_t stream_index,
                           const uint8_t* frames, uint32_t frame_count) {
    const sat_audio_stream_t target = stream_index == 0u ? slot.stream : slot.stream_r;
    return sat_audio_stream_write(target, frames, frame_count);
}

// Split `frame_count` interleaved stereo frames into the two halves of
// staging_split: left first, right second. `sample_bytes` is 1 (S8) or 2.
void deinterleave_stereo(const uint8_t* interleaved, uint8_t* split,
                         uint32_t frame_count, uint32_t sample_bytes) {
    uint8_t* left = split;
    uint8_t* right = split + frame_count * sample_bytes;
    for (uint32_t i = 0u; i < frame_count; ++i) {
        for (uint32_t b = 0u; b < sample_bytes; ++b) {
            left[i * sample_bytes + b] = interleaved[(i * 2u) * sample_bytes + b];
            right[i * sample_bytes + b] = interleaved[(i * 2u + 1u) * sample_bytes + b];
        }
    }
}

sat_result_t feed(MusicSlot& slot) {
    if (slot.playing == 0u) return SAT_OK;
    const uint8_t stereo = slot.channels == 2u ? 1u : 0u;
    uint32_t frames = sat_audio_stream_available(slot.stream);
    if (stereo != 0u) {
        const uint32_t available_r = sat_audio_stream_available(slot.stream_r);
        if (available_r < frames) frames = available_r;
    }
    if (frames > kMusicFeedFrames) frames = kMusicFeedFrames;
    const uint32_t sample_bytes = bytes_per_sample(slot.format);
    const uint32_t frame_bytes = sample_bytes * slot.channels;
    while (frames != 0u) {
        uint32_t contiguous = (slot.payload_bytes - slot.position_bytes) / frame_bytes;
        if (contiguous > frames) contiguous = frames;
        if (contiguous == 0u) {
            slot.position_bytes = 0u;
            continue;
        }
        const uint32_t byte_count = contiguous * frame_bytes;
        const uint8_t* left = nullptr;
        const uint8_t* right = nullptr;
        if (slot.data != nullptr) {
            if (stereo == 0u) {
                left = slot.data + slot.position_bytes;
            } else {
                deinterleave_stereo(slot.data + slot.position_bytes,
                                    slot.staging_split, contiguous, sample_bytes);
                left = slot.staging_split;
                right = slot.staging_split + contiguous * sample_bytes;
            }
        } else {
            uint32_t read = 0u;
            const sat_result_t read_status = sat_asset_read_at(
                slot.logical_path, slot.position_bytes, slot.staging, byte_count, &read);
            if (read_status != SAT_OK || read != byte_count) {
                return read_status == SAT_OK ? SAT_ERR_IO : read_status;
            }
            if (stereo == 0u) {
                left = slot.staging;
            } else {
                deinterleave_stereo(slot.staging, slot.staging_split,
                                    contiguous, sample_bytes);
                left = slot.staging_split;
                right = slot.staging_split + contiguous * sample_bytes;
            }
        }
        SAT_TRY(write_channel(slot, 0u, left, contiguous));
        if (stereo != 0u) SAT_TRY(write_channel(slot, 1u, right, contiguous));
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
        (info.channels != 1u && info.channels != 2u)) return SAT_ERR_UNSUPPORTED;
    const uint32_t sample_bytes = bytes_per_sample(info.format);
    const uint32_t frame_bytes = sample_bytes * info.channels;
    if (sample_bytes == 0u || info.size == 0u || info.size % frame_bytes != 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    const uint32_t sample_count = info.sample_count == 0u
        ? info.size / frame_bytes : info.sample_count;
    if (sample_count == 0u || sample_count > info.size / frame_bytes) return SAT_ERR_INVALID_ARG;

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
            info.sample_rate, SAT_MUSIC_BUFFER_FRAMES, 1u, info.format, 0u};
        const sat_result_t st = sat_audio_stream_open(
            &slot.stream, &spec, slot.buffer, sizeof(slot.buffer));
        if (st != SAT_OK) return st;
        const uint8_t stereo = info.channels == 2u ? 1u : 0u;
        if (stereo != 0u) {
            const sat_result_t st_r = sat_audio_stream_open(
                &slot.stream_r, &spec, slot.buffer_r, sizeof(slot.buffer_r));
            if (st_r != SAT_OK) {
                (void)sat_audio_stream_close(slot.stream);
                return st_r;
            }
            // A stereo track is two mono SCSP slots panned hard L/R.
            (void)sat_audio_stream_set_pan(slot.stream, SAT_AUDIO_PAN_LEFT);
            (void)sat_audio_stream_set_pan(slot.stream_r, SAT_AUDIO_PAN_RIGHT);
        }
        slot.asset = asset;
        slot.data = static_cast<const uint8_t*>(info.data);
        for (uint16_t j = 0u; j <= path_length; ++j) slot.logical_path[j] = logical_path[j];
        slot.payload_bytes = sample_count * frame_bytes;
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
    if (slot->channels == 2u) SAT_TRY(sat_audio_stream_resume(slot->stream_r));
    slot->playing = 1u;
    // Prime the entire software ring before key-on. CD-backed music otherwise
    // starts with only the two SCSP halves and immediately exhausts them on
    // the first seek/cache fill.
    while (sat_audio_stream_available(slot->stream) != 0u) {
        SAT_TRY(feed(*slot));
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_music_pause(sat_music_t music) {
    MusicSlot* slot = resolve(music);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    SAT_TRY(sat_audio_stream_pause(slot->stream));
    if (slot->channels == 2u) SAT_TRY(sat_audio_stream_pause(slot->stream_r));
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
    if (slot->channels == 2u) {
        SAT_TRY(sat_audio_stream_pause(slot->stream_r));
        SAT_TRY(sat_audio_stream_flush(slot->stream_r));
    }
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
    if (slot->channels == 2u) SAT_TRY(sat_audio_stream_close(slot->stream_r));
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
    SAT_TRY(sat_audio_stream_stats(slot->stream, out_stats));
    if (slot->channels == 2u) {
        sat_audio_stream_stats_t right{};
        SAT_TRY(sat_audio_stream_stats(slot->stream_r, &right));
        // Keep the two channel streams honest: report the limiting fill,
        // combined progress, and summed anomaly counters.
        if (right.buffered_frames < out_stats->buffered_frames) {
            out_stats->buffered_frames = right.buffered_frames;
        }
        if (right.consumed_frames < out_stats->consumed_frames) {
            out_stats->consumed_frames = right.consumed_frames;
        }
        if (right.minimum_fill < out_stats->minimum_fill) {
            out_stats->minimum_fill = right.minimum_fill;
        }
        if (right.maximum_fill > out_stats->maximum_fill) {
            out_stats->maximum_fill = right.maximum_fill;
        }
        out_stats->underrun_count += right.underrun_count;
        out_stats->rejected_write_count += right.rejected_write_count;
        out_stats->refill_count += right.refill_count;
        out_stats->playing = (out_stats->playing != 0u && right.playing != 0u) ? 1u : 0u;
    }
    return SAT_OK;
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
