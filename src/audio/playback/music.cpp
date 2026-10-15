#include "saturn/audio.h"

#include <stddef.h>

#include "saturn/asset.h"
#include "saturn/cdfs.h"
#include "saturn/disc_stream.h"
#include "src/audio/playback/ima_adpcm.hpp"
#include "src/audio/streaming/runtime.hpp"

namespace {

constexpr uint32_t kMusicFeedFrames = 2048u;
// A renderer may submit fewer than eleven frames per second while the SCSP
// still consumes 22050 samples each second. Refill several chunks per update
// so a slow display frame cannot permanently starve the software ring.
constexpr uint32_t kMusicUpdateFeedLimit = 4u;
constexpr uint32_t kMusicFeedBytes = kMusicFeedFrames * 2u * 2u;
using MusicCopyHalf = uint16_t __attribute__((may_alias));

// A disc segment that was started and whose frames are not all in the rings yet.
struct PendingSegment {
    uint32_t skip;   // bytes still to drop at its head (after a seek)
    uint32_t bound;  // frames it can still add to the rings (a room reservation)
};

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
    uint32_t position_frame;
    uint32_t decoded_block;
    // Disc-scheduled feed (sat_music_use_disc_stream): the track is read in
    // segments by the scheduler and delivered here without waiting.
    const sat_cdfs_file_t* file;  // non-null: this track streams through g_disc
    uint32_t src_bytes;           // bytes of the file the track occupies (PCM payload, or whole ADPCM blocks)
    uint32_t file_pos;            // sector-aligned file offset of the next segment to start
    uint32_t skip_bytes;          // leading bytes of that segment before the track position (after a seek)
    uint32_t carry_have;          // ADPCM: bytes of a block split across segments, held in `encoded`
    PendingSegment pend[2];       // started, not yet delivered, oldest first
    uint8_t pend_count;
    uint8_t channels;
    uint8_t format;
    uint8_t playing;
    uint8_t starting;             // play was called, the rings are filling, the streams are still paused
    uint8_t used;
    uint16_t generation;
    uint8_t buffer[SAT_MUSIC_BUFFER_FRAMES * 2u];
    uint8_t buffer_r[SAT_MUSIC_BUFFER_FRAMES * 2u];
    uint8_t staging[kMusicFeedBytes];
    // Deinterleaved L/R channel frames for stereo feeds.
    uint8_t staging_split[kMusicFeedBytes];
    uint8_t encoded[saturn::core::audio::ima_adpcm::kMaxBlockBytes];
};

/* ~288 KB of ring and staging buffers. Charging that to Work RAM High
 * would leave a program barely 300 KB for its own code and data, so the
 * slots live in Work RAM Low, which crt0 zeroes exactly like .bss. Low
 * RAM is the slower of the two, but a refill only runs once the SCSP has
 * moved to the other 4096-sample half, so there is most of that half's
 * time to copy -- orders of magnitude more than the copy costs. */
MusicSlot g_music[SAT_MUSIC_CAPACITY] __attribute__((section(".wram_l"))) = {};

/* The disc scheduler tracks stream through, and the one slot that is attached to it (the drive
 * serves one music feed). */
sat_disc_stream_t* g_disc = nullptr;
MusicSlot* g_disc_slot = nullptr;

constexpr uint32_t kSectorBytes = 2048u;

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

sat_result_t feed(MusicSlot& slot) {
    if (slot.playing == 0u) return SAT_OK;
    if (slot.format == SAT_AUDIO_IMA_ADPCM) return feed_adpcm(slot);
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

/* ---- disc-scheduled feed ----------------------------------------------------------------------
 * The scheduler (src/storage/cd/disc_stream.cpp) owns the drive and asks `next` whenever it is
 * free. A segment is read from a sector boundary; `deliver` turns the landed bytes into frames and
 * writes them into the rings, a slice per call so one frame never pays for a whole segment.
 *
 * Two segments can be started and not yet delivered, so `next` names a read only when the rings
 * have room for every frame the segment can produce on top of what the earlier ones can still add
 * (the `bound` of each pending segment, which shrinks as its frames are written). A write never
 * fails and nothing waits. */

constexpr uint32_t kAsyncSliceBytes = 4096u;      // payload moved by one PCM `deliver` call
constexpr uint32_t kAdpcmSegmentBlocks = 8u;      // ADPCM reads stay near 8k frames: two must fit a ring

/* Bytes of the next segment: a whole one, or what is left of the file. ADPCM expands 4:1, so a
 * 16 KiB read would be most of a ring; its segments are cut to whole sectors near 8 blocks. */
uint32_t async_segment_bytes(const MusicSlot& slot) {
    uint32_t cap = SAT_DISC_STREAM_SEGMENT_BYTES;
    if (slot.format == SAT_AUDIO_IMA_ADPCM) {
        namespace codec = saturn::core::audio::ima_adpcm;
        uint32_t sectors = kAdpcmSegmentBlocks * codec::block_bytes(slot.channels) / kSectorBytes;
        if (sectors == 0u) sectors = 1u;
        if (sectors * kSectorBytes < cap) cap = sectors * kSectorBytes;
    }
    const uint32_t left = slot.src_bytes - slot.file_pos;
    return left < cap ? left : cap;
}

/* Frames the next segment can add to the rings at most. */
uint32_t async_segment_bound(const MusicSlot& slot, uint32_t segment) {
    const uint32_t payload = segment > slot.skip_bytes ? segment - slot.skip_bytes : 0u;
    if (slot.format == SAT_AUDIO_IMA_ADPCM) {
        namespace codec = saturn::core::audio::ima_adpcm;
        // A block may be completed by this segment's first bytes: whole blocks in (carry + payload).
        const uint32_t block_bytes = codec::block_bytes(slot.channels);
        return (payload + block_bytes - 1u) / block_bytes * codec::kBlockFrames;
    }
    const uint32_t frame_bytes = bytes_per_sample(slot.format) * slot.channels;
    return frame_bytes == 0u ? 0u : payload / frame_bytes;
}

uint8_t async_next(void* user, const sat_cdfs_file_t** file, uint32_t* offset, uint32_t* bytes) {
    const MusicSlot& slot = *static_cast<const MusicSlot*>(user);
    if (slot.playing == 0u || slot.pend_count >= 2u) return 0u;
    const uint32_t segment = async_segment_bytes(slot);
    uint32_t reserved = 0u;
    for (uint8_t i = 0u; i < slot.pend_count; ++i) reserved += slot.pend[i].bound;
    const uint32_t room = writable_frames(slot);
    if (room < reserved || async_segment_bound(slot, segment) > room - reserved) return 0u;
    *file = slot.file;
    *offset = slot.file_pos;
    *bytes = segment;
    return 1u;
}

/* The read `async_next` named is in flight: reserve its room and move the read cursor on. */
void async_started(void* user) {
    MusicSlot& slot = *static_cast<MusicSlot*>(user);
    const uint32_t segment = async_segment_bytes(slot);
    PendingSegment& pending = slot.pend[slot.pend_count++];
    pending.skip = slot.skip_bytes;
    pending.bound = async_segment_bound(slot, segment);
    slot.skip_bytes = 0u;
    // The read covers whole sectors; the next one starts after them, and the track loops when
    // the file's payload ends (a segment never straddles it).
    slot.file_pos += (segment + kSectorBytes - 1u) / kSectorBytes * kSectorBytes;
    if (slot.file_pos >= slot.src_bytes) slot.file_pos = 0u;
}

void async_wrote(MusicSlot& slot, uint32_t frames) {
    PendingSegment& pending = slot.pend[0];
    pending.bound = frames < pending.bound ? pending.bound - frames : 0u;
}

sat_result_t async_deliver_pcm(MusicSlot& slot, const uint8_t* data, uint32_t bytes, uint32_t* consumed) {
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
sat_result_t async_deliver_adpcm(MusicSlot& slot, const uint8_t* data, uint32_t bytes, uint32_t* consumed) {
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

sat_result_t async_deliver(void* user, const uint8_t* data, uint32_t bytes, uint32_t* consumed) {
    MusicSlot& slot = *static_cast<MusicSlot*>(user);
    if (slot.pend_count == 0u) {      // not a segment this feed started (aborted): drop it
        *consumed = bytes;
        return SAT_OK;
    }
    sat_result_t status = SAT_OK;
    uint32_t used = 0u;
    if (slot.pend[0].skip != 0u) {
        // The head of the segment lies before the track position: drop it without a slice.
        used = slot.pend[0].skip < bytes ? slot.pend[0].skip : bytes;
        slot.pend[0].skip -= used;
    } else {
        status = slot.format == SAT_AUDIO_IMA_ADPCM
            ? async_deliver_adpcm(slot, data, bytes, &used)
            : async_deliver_pcm(slot, data, bytes, &used);
    }
    if (status != SAT_OK || used >= bytes) {
        // Segment finished (or dropped): its reservation goes.
        used = bytes;
        slot.pend[0] = slot.pend[1];
        slot.pend_count--;
    }
    *consumed = used;
    return status;
}

uint32_t async_buffered(void* user) {
    const MusicSlot& slot = *static_cast<const MusicSlot*>(user);
    // A track that is not wanted (stopped, paused) asks for nothing, so it must not hold data jobs
    // back until its rings reach the reserve.
    if (slot.playing == 0u) return SAT_DISC_MUSIC_RING_FRAMES;
    uint32_t frames = sat_audio_stream_buffered(slot.stream);
    if (slot.channels == 2u) {
        const uint32_t right = sat_audio_stream_buffered(slot.stream_r);
        if (right < frames) frames = right;
    }
    return frames;
}

/* The rings are full: nothing is on its way and the next segment would not fit. */
bool async_primed(const MusicSlot& slot) {
    if (slot.pend_count != 0u) return false;
    return writable_frames(slot) < async_segment_bound(slot, async_segment_bytes(slot));
}

/* Both channel streams start together, once the rings hold what they will keep topped up. */
void start_streams(MusicSlot& slot) {
    slot.starting = 0u;
    if (slot.file != nullptr) sat_disc_stream_music_set_starting(g_disc, 0u);
    (void)sat_audio_stream_resume(slot.stream);
    if (slot.channels == 2u) (void)sat_audio_stream_resume(slot.stream_r);
}

void async_tick(void* user) {
    MusicSlot& slot = *static_cast<MusicSlot*>(user);
    if (slot.starting != 0u && async_primed(slot)) start_streams(slot);
}

const sat_disc_music_feed_ops_t kAsyncFeed = {async_next, async_started, async_deliver, async_buffered, 1u, async_tick};

/* Where frame `frame` lies in the file: the sector the read starts at and the bytes to drop.
 * The scheduler has dropped the segments read for the old position (sat_disc_stream_music_abort). */
void async_locate(MusicSlot& slot, uint32_t frame) {
    uint32_t offset;
    if (slot.format == SAT_AUDIO_IMA_ADPCM) {
        namespace codec = saturn::core::audio::ima_adpcm;
        offset = frame / codec::kBlockFrames * codec::block_bytes(slot.channels);
        slot.position_frame = frame;
    } else {
        offset = frame * bytes_per_sample(slot.format) * slot.channels;
    }
    slot.file_pos = offset / kSectorBytes * kSectorBytes;
    slot.skip_bytes = offset - slot.file_pos;
    slot.carry_have = 0u;
    slot.pend_count = 0u;
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
    const uint8_t compressed = info.format == SAT_AUDIO_IMA_ADPCM ? 1u : 0u;
    const uint32_t sample_bytes = compressed != 0u ? 2u : bytes_per_sample(info.format);
    const uint32_t frame_bytes = sample_bytes * info.channels;
    if (sample_bytes == 0u || info.size == 0u) return SAT_ERR_INVALID_ARG;
    uint32_t sample_count;
    if (compressed != 0u) {
        namespace codec = saturn::core::audio::ima_adpcm;
        sample_count = info.sample_count;
        const uint64_t blocks =
            (static_cast<uint64_t>(sample_count) + codec::kBlockFrames - 1u) /
            codec::kBlockFrames;
        if (sample_count == 0u ||
            blocks * codec::block_bytes(info.channels) != info.size) {
            return SAT_ERR_INVALID_ARG;
        }
    } else {
        if (info.size % frame_bytes != 0u) return SAT_ERR_INVALID_ARG;
        sample_count = info.sample_count == 0u
            ? info.size / frame_bytes : info.sample_count;
        if (sample_count == 0u || sample_count > info.size / frame_bytes) {
            return SAT_ERR_INVALID_ARG;
        }
    }

    // With a disc scheduler bound, a track backed by a disc file streams
    // through it; any other source (RAM, a blob, a non-disc reader) is read as before.
    namespace adpcm = saturn::core::audio::ima_adpcm;
    const sat_cdfs_file_t* disc_file = nullptr;
    uint32_t src_bytes = 0u;
    if (g_disc != nullptr && info.data == nullptr && info.source_path != nullptr &&
        sat_cdfs_source_file(info.source_path, &disc_file) == SAT_OK) {
        if (g_disc_slot != nullptr) return SAT_ERR_BUSY;
        src_bytes = compressed != 0u
            ? static_cast<uint32_t>((static_cast<uint64_t>(sample_count) + adpcm::kBlockFrames - 1u) /
                                    adpcm::kBlockFrames) * adpcm::block_bytes(info.channels)
            : sample_count * frame_bytes;
        if (disc_file->size < src_bytes) return SAT_ERR_INVALID_ARG;
    } else {
        disc_file = nullptr;
    }

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
            info.sample_rate, SAT_MUSIC_BUFFER_FRAMES, 1u,
            compressed != 0u
                ? static_cast<uint8_t>(SAT_AUDIO_PCM_S16)
                : info.format,
            0u};
        const sat_result_t st = sat_audio_stream_open(
            &slot.stream, &spec, slot.buffer, sizeof(slot.buffer));
        if (st != SAT_OK) return st;
        // CD reads pump sat_audio_update(). Keep an unopened music stream
        // silent until both channel rings have been primed by music_play().
        (void)sat_audio_stream_pause(slot.stream);
        const uint8_t stereo = info.channels == 2u ? 1u : 0u;
        if (stereo != 0u) {
            const sat_result_t st_r = sat_audio_stream_open(
                &slot.stream_r, &spec, slot.buffer_r, sizeof(slot.buffer_r));
            if (st_r != SAT_OK) {
                (void)sat_audio_stream_close(slot.stream);
                return st_r;
            }
            (void)sat_audio_stream_pause(slot.stream_r);
            // A stereo track is two mono SCSP slots panned hard L/R.
            (void)sat_audio_stream_set_pan(slot.stream, SAT_AUDIO_PAN_LEFT);
            (void)sat_audio_stream_set_pan(slot.stream_r, SAT_AUDIO_PAN_RIGHT);
        }
        slot.file = nullptr;
        slot.src_bytes = 0u;
        slot.file_pos = 0u;
        slot.skip_bytes = 0u;
        slot.carry_have = 0u;
        slot.pend_count = 0u;
        slot.asset = asset;
        slot.data = static_cast<const uint8_t*>(info.data);
        for (uint16_t j = 0u; j <= path_length; ++j) slot.logical_path[j] = logical_path[j];
        slot.payload_bytes = sample_count * frame_bytes;
        slot.sample_count = sample_count;
        slot.sample_rate = info.sample_rate;
        slot.position_bytes = 0u;
        slot.position_frame = 0u;
        slot.decoded_block = 0xFFFFFFFFu;
        slot.channels = info.channels;
        slot.format = info.format;
        slot.playing = 0u;
        slot.starting = 0u;
        slot.used = 1u;
        if (disc_file != nullptr) {
            const sat_result_t attached = sat_disc_stream_music_attach(g_disc, &kAsyncFeed, &slot);
            if (attached != SAT_OK) {
                slot.used = 0u;
                (void)sat_audio_stream_close(slot.stream);
                if (stereo != 0u) (void)sat_audio_stream_close(slot.stream_r);
                return attached;
            }
            slot.file = disc_file;
            slot.src_bytes = src_bytes;
            g_disc_slot = &slot;
        }
        out_music->slot = i;
        out_music->generation = slot.generation;
        return SAT_OK;
    }
    return SAT_ERR_CAPACITY;
}

extern "C" sat_result_t sat_music_play(sat_music_t music) {
    MusicSlot* slot = resolve(music);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    if (slot->playing != 0u) return SAT_OK;
    // Nothing is read here. Both streams stay paused while sat_music_update fills the rings (the
    // scheduler's service for a disc track), and start together once they are full.
    slot->playing = 1u;
    slot->starting = 1u;
    if (slot->file != nullptr) sat_disc_stream_music_set_starting(g_disc, 1u);
    return SAT_OK;
}

extern "C" sat_result_t sat_music_pause(sat_music_t music) {
    MusicSlot* slot = resolve(music);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    SAT_TRY(sat_audio_stream_pause(slot->stream));
    if (slot->channels == 2u) SAT_TRY(sat_audio_stream_pause(slot->stream_r));
    slot->playing = 0u;
    slot->starting = 0u;
    if (slot->file != nullptr) sat_disc_stream_music_set_starting(g_disc, 0u);
    return SAT_OK;
}

extern "C" sat_result_t sat_music_resume(sat_music_t music) {
    return sat_music_play(music);
}

namespace {

/* Keys off both channels, drops everything buffered and moves the source
 * cursor, leaving the track stopped. */
sat_result_t rewind_to(MusicSlot& slot, uint32_t frame) {
    // A segment read for the old position must not land in the flushed rings.
    if (slot.file != nullptr) sat_disc_stream_music_abort(g_disc);
    SAT_TRY(sat_audio_stream_pause(slot.stream));
    SAT_TRY(sat_audio_stream_flush(slot.stream));
    if (slot.channels == 2u) {
        SAT_TRY(sat_audio_stream_pause(slot.stream_r));
        SAT_TRY(sat_audio_stream_flush(slot.stream_r));
    }
    if (slot.file != nullptr) {
        async_locate(slot, frame);
    } else if (slot.format == SAT_AUDIO_IMA_ADPCM) {
        slot.position_frame = frame;
        slot.decoded_block = 0xFFFFFFFFu;
    } else {
        slot.position_bytes = frame * bytes_per_sample(slot.format) * slot.channels;
    }
    slot.playing = 0u;
    slot.starting = 0u;
    if (slot.file != nullptr) sat_disc_stream_music_set_starting(g_disc, 0u);
    return SAT_OK;
}

}  // namespace

extern "C" sat_result_t sat_music_stop(sat_music_t music) {
    MusicSlot* slot = resolve(music);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    return rewind_to(*slot, 0u);
}

extern "C" sat_result_t sat_music_seek(sat_music_t music, uint32_t frame) {
    MusicSlot* slot = resolve(music);
    if (slot == nullptr || frame >= slot->sample_count) return SAT_ERR_INVALID_ARG;
    const uint8_t was_playing = slot->playing;
    SAT_TRY(rewind_to(*slot, frame));
    // Restarting refills both channels from the new position and starts them in lockstep.
    return was_playing != 0u ? sat_music_play(music) : SAT_OK;
}

extern "C" sat_result_t sat_music_update(sat_music_t music) {
    MusicSlot* slot = resolve(music);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    // A disc-scheduled track: land the segment that has arrived and start the
    // next one. Never waits for the drive; also moves the scheduler's data jobs.
    // The scheduler's tick starts the streams once the rings are full.
    if (slot->file != nullptr) return sat_disc_stream_service(g_disc);
    for (uint32_t i = 0u; i < kMusicUpdateFeedLimit; ++i) {
        if (slot->playing == 0u || writable_frames(*slot) == 0u) break;
        SAT_TRY(feed(*slot));
    }
    if (slot->starting != 0u && writable_frames(*slot) == 0u) start_streams(*slot);
    return SAT_OK;
}

extern "C" sat_result_t sat_music_use_disc_stream(sat_disc_stream_t* stream) {
    if (g_disc_slot != nullptr) return SAT_ERR_BUSY;
    g_disc = stream;
    return SAT_OK;
}

extern "C" sat_result_t sat_music_close(sat_music_t music) {
    MusicSlot* slot = resolve(music);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    if (slot->file != nullptr) {
        sat_disc_stream_music_detach(g_disc);
        g_disc_slot = nullptr;
        slot->file = nullptr;
    }
    SAT_TRY(sat_audio_stream_close(slot->stream));
    if (slot->channels == 2u) SAT_TRY(sat_audio_stream_close(slot->stream_r));
    slot->used = 0u;
    slot->generation = next_generation(slot->generation);
    slot->data = nullptr;
    slot->asset = {};
    slot->logical_path[0] = '\0';
    slot->payload_bytes = 0u;
    slot->playing = 0u;
    slot->starting = 0u;
    slot->position_frame = 0u;
    slot->decoded_block = 0xFFFFFFFFu;
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
    out_info->disc_streamed = slot->file != nullptr ? 1u : 0u;
    out_info->starting = slot->starting;
    out_info->reserved[0] = 0u;
    out_info->reserved[1] = 0u;
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
    if (g_disc_slot != nullptr) sat_disc_stream_music_detach(g_disc);
    g_disc_slot = nullptr;
    for (uint16_t i = 0u; i < SAT_MUSIC_CAPACITY; ++i) {
        MusicSlot& slot = g_music[i];
        slot.used = 0u;
        slot.generation = next_generation(slot.generation);
        slot.data = nullptr;
        slot.file = nullptr;
        slot.asset = {};
        slot.logical_path[0] = '\0';
        slot.payload_bytes = 0u;
        slot.playing = 0u;
        slot.starting = 0u;
        slot.position_bytes = 0u;
        slot.position_frame = 0u;
        slot.decoded_block = 0xFFFFFFFFu;
    }
}

}  // namespace saturn::core
