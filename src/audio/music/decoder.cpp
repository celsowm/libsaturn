#include "src/audio/music/internal.hpp"

namespace saturn::core::music {

using MusicCopyHalf = uint16_t __attribute__((may_alias));

uint32_t bytes_per_sample(uint8_t format) {
    if (format == SAT_AUDIO_PCM_S8) return 1u;
    if (format == SAT_AUDIO_PCM_S16) return 2u;
    return 0u;
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
    if (sample_bytes == 2u &&
        ((reinterpret_cast<uintptr_t>(interleaved) |
          reinterpret_cast<uintptr_t>(split)) & 1u) == 0u) {
        const MusicCopyHalf* source =
            reinterpret_cast<const MusicCopyHalf*>(interleaved);
        MusicCopyHalf* out_left = reinterpret_cast<MusicCopyHalf*>(left);
        MusicCopyHalf* out_right = reinterpret_cast<MusicCopyHalf*>(right);
        for (uint32_t i = 0u; i < frame_count; ++i) {
            out_left[i] = source[i * 2u];
            out_right[i] = source[i * 2u + 1u];
        }
        return;
    }
    for (uint32_t i = 0u; i < frame_count; ++i) {
        for (uint32_t b = 0u; b < sample_bytes; ++b) {
            left[i * sample_bytes + b] = interleaved[(i * 2u) * sample_bytes + b];
            right[i * sample_bytes + b] = interleaved[(i * 2u + 1u) * sample_bytes + b];
        }
    }
}

/* Frames every channel can take: stereo writes both rings in lockstep. */
uint32_t writable_frames(const MusicSlot& slot) {
    uint32_t frames = sat_audio_stream_available(slot.stream);
    if (slot.channels == 2u) {
        const uint32_t available_r = sat_audio_stream_available(slot.stream_r);
        if (available_r < frames) frames = available_r;
    }
    return frames;
}

sat_result_t feed_adpcm(MusicSlot& slot) {
    namespace codec = saturn::core::audio::ima_adpcm;
    uint32_t frames = writable_frames(slot);
    if (frames > kMusicFeedFrames) frames = kMusicFeedFrames;
    const uint32_t encoded_bytes = codec::block_bytes(slot.channels);
    while (frames != 0u) {
        const uint32_t block = slot.position_frame / codec::kBlockFrames;
        const uint32_t in_block = slot.position_frame % codec::kBlockFrames;
        if (slot.decoded_block != block) {
            const uint8_t* source = nullptr;
            if (slot.data != nullptr) {
                source = slot.data + block * encoded_bytes;
            } else {
                uint32_t read = 0u;
                const sat_result_t st = sat_asset_read_at(
                    slot.logical_path, block * encoded_bytes,
                    slot.encoded, encoded_bytes, &read);
                if (st != SAT_OK || read != encoded_bytes) {
                    return st == SAT_OK ? SAT_ERR_IO : st;
                }
                source = slot.encoded;
            }
            if (!codec::decode_block(source, slot.channels, slot.staging_split)) {
                return SAT_ERR_IO;
            }
            slot.decoded_block = block;
        }
        uint32_t take = codec::kBlockFrames - in_block;
        if (take > slot.sample_count - slot.position_frame) {
            take = slot.sample_count - slot.position_frame;
        }
        if (take > frames) take = frames;
        SAT_TRY(write_channel(slot, 0u,
            slot.staging_split + in_block * 2u, take));
        if (slot.channels == 2u) {
            SAT_TRY(write_channel(slot, 1u,
                slot.staging_split + codec::kChannelBytes + in_block * 2u, take));
        }
        slot.position_frame += take;
        if (slot.position_frame == slot.sample_count) slot.position_frame = 0u;
        frames -= take;
    }
    return SAT_OK;
}


/* ADX 18-byte frames encode 32 samples per channel with continuous predictor
 * history. Decode just one frame per channel and feed the native S16 streams. */
sat_result_t feed_adx(MusicSlot& slot) {
    namespace codec = saturn::core::audio::adx_adpcm;
    uint32_t frames = writable_frames(slot);
    if (frames > kMusicFeedFrames) frames = kMusicFeedFrames;
    const uint32_t encoded_bytes = codec::block_bytes(slot.channels);
    while (frames != 0u) {
        const uint32_t block = slot.position_frame / codec::kBlockFrames;
        const uint32_t in_block = slot.position_frame % codec::kBlockFrames;
        if (slot.decoded_block != block) {
            const uint8_t* source = nullptr;
            if (slot.data != nullptr) {
                source = slot.data + codec::kCoefficientBytes + block * encoded_bytes;
            } else {
                uint32_t read = 0u;
                const sat_result_t st = sat_asset_read_at(
                    slot.logical_path,
                    codec::kCoefficientBytes + block * encoded_bytes,
                    slot.encoded, encoded_bytes, &read);
                if (st != SAT_OK || read != encoded_bytes)
                    return st == SAT_OK ? SAT_ERR_IO : st;
                source = slot.encoded;
            }
            if (!codec::decode_block(source, slot.channels, slot.adx_state,
                                     slot.staging_split)) return SAT_ERR_IO;
            slot.decoded_block = block;
        }
        uint32_t take = codec::kBlockFrames - in_block;
        if (take > slot.sample_count - slot.position_frame)
            take = slot.sample_count - slot.position_frame;
        if (take > frames) take = frames;
        SAT_TRY(write_channel(slot, 0u, slot.staging_split + in_block * 2u, take));
        if (slot.channels == 2u)
            SAT_TRY(write_channel(slot, 1u,
                slot.staging_split + codec::kChannelBytes + in_block * 2u, take));
        slot.position_frame += take;
        if (slot.position_frame == slot.sample_count) {
            slot.position_frame = 0u;
            slot.decoded_block = 0xFFFFFFFFu;
            codec::reset_history(slot.adx_state);
        }
        frames -= take;
    }
    return SAT_OK;
}

sat_result_t feed(MusicSlot& slot) {
    if (slot.playing == 0u && slot.prefetch == 0u) return SAT_OK;
    if (slot.format == SAT_AUDIO_IMA_ADPCM) return feed_adpcm(slot);
    if (slot.format == SAT_AUDIO_ADX_ADPCM) return feed_adx(slot);
    const uint8_t stereo = slot.channels == 2u ? 1u : 0u;
    uint32_t frames = writable_frames(slot);
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

} // namespace saturn::core::music
