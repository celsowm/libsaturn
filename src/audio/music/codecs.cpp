#include "src/audio/music/internal.hpp"
#include "saturn/music_codecs.h"

namespace saturn::core::music {

/* ---- the formats and sources a program can leave out (saturn/music_codecs.h) ---------------------
 * Everything here is reached only through `sat_music_codecs`: PCM and IMA ADPCM delivered by the disc scheduler,
 * and the feeds for tracks read from RAM or an asset reader (every format). ADX from the disc scheduler
 * (async_deliver_adx) stays in disc_feed.cpp and is always present. */

namespace {

constexpr uint32_t kAsyncSliceBytes = 4096u;      // payload moved by one PCM `deliver` call

using MusicCopyHalf = uint16_t __attribute__((may_alias));

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

sat_result_t feed_pcm(MusicSlot& slot) {
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

sat_result_t deliver_pcm(MusicSlot& slot, const uint8_t* data, uint32_t bytes, uint32_t* consumed) {
    const uint32_t sample_bytes = bytes_per_sample(slot.format);
    const uint32_t frame_bytes = sample_bytes * slot.channels;
    if (frame_bytes == 0u) return SAT_ERR_UNSUPPORTED;     // open() only admits S8/S16: keeps the divide defined
    uint32_t frames = (bytes < kAsyncSliceBytes ? bytes : kAsyncSliceBytes) / frame_bytes;
    if (frames == 0u) frames = 1u;
    if (slot.channels == 1u) {
        SAT_TRY(write_channel(slot, 0u, data, frames));
    } else {
        deinterleave_stereo(data, slot.staging_split, frames, sample_bytes);
        SAT_TRY(write_channel(slot, 0u, slot.staging_split, frames));
        SAT_TRY(write_channel(slot, 1u, slot.staging_split + frames * sample_bytes, frames));
    }
    async_wrote(slot, frames);
    *consumed = frames * frame_bytes;
    return SAT_OK;
}

/* One block per call. A block can straddle two segments: its head waits in `encoded`
 * (carry_have bytes) until the rest arrives. */
sat_result_t deliver_adpcm(MusicSlot& slot, const uint8_t* data, uint32_t bytes, uint32_t* consumed) {
    namespace codec = saturn::core::audio::ima_adpcm;
    const uint32_t block_bytes = codec::block_bytes(slot.channels);
    const uint8_t* source;
    uint32_t used;
    if (slot.carry_have != 0u) {
        used = block_bytes - slot.carry_have;
        if (used > bytes) used = bytes;
        for (uint32_t i = 0u; i < used; ++i) slot.encoded[slot.carry_have + i] = data[i];
        slot.carry_have += used;
        *consumed = used;
        if (slot.carry_have < block_bytes) return SAT_OK;
        slot.carry_have = 0u;
        source = slot.encoded;
    } else if (bytes >= block_bytes) {
        used = block_bytes;
        source = data;
    } else {
        slot.carry_have = bytes;
        for (uint32_t i = 0u; i < bytes; ++i) slot.encoded[i] = data[i];
        *consumed = bytes;
        return SAT_OK;
    }
    *consumed = used;
    if (!codec::decode_block(source, slot.channels, slot.staging_split)) return SAT_ERR_IO;
    const uint32_t in_block = slot.position_frame % codec::kBlockFrames;
    uint32_t take = codec::kBlockFrames - in_block;
    if (take > slot.sample_count - slot.position_frame) take = slot.sample_count - slot.position_frame;
    SAT_TRY(write_channel(slot, 0u, slot.staging_split + in_block * 2u, take));
    if (slot.channels == 2u) {
        SAT_TRY(write_channel(slot, 1u, slot.staging_split + codec::kChannelBytes + in_block * 2u, take));
    }
    slot.position_frame += take;
    if (slot.position_frame == slot.sample_count) slot.position_frame = 0u;
    async_wrote(slot, take);
    return SAT_OK;
}

const DiscDeliver kPcmDisc = {deliver_pcm};
const DiscDeliver kImaDisc = {deliver_adpcm};
const ResidentFeed kResident = {feed_pcm, feed_adpcm, feed_adx};

} // namespace
} // namespace saturn::core::music

extern "C" const sat_music_codecs_t sat_music_codecs = {
    &saturn::core::music::kPcmDisc, &saturn::core::music::kImaDisc, &saturn::core::music::kResident};
