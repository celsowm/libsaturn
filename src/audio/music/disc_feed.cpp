#include "src/audio/music/internal.hpp"

namespace saturn::core::music {

constexpr uint32_t kSectorBytes = 2048u;

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

/* The most one read can be. IMA segments are cut to whole sectors near 8 blocks, since ADPCM expands 4:1 and a
 * 16 KiB read would be most of a ring. ADX segments keep the full 16 KiB: they wait compressed in their buffer
 * until the ring has room for their blocks, so the ring never has to hold them decoded. */
uint32_t async_segment_cap(uint8_t format, uint8_t channels) {
    uint32_t cap = SAT_DISC_STREAM_SEGMENT_BYTES;
    if (format == SAT_AUDIO_IMA_ADPCM) {
        namespace codec = saturn::core::audio::ima_adpcm;
        uint32_t sectors = kAdpcmSegmentBlocks * codec::block_bytes(channels) / kSectorBytes;
        if (sectors == 0u) sectors = 1u;
        if (sectors * kSectorBytes < cap) cap = sectors * kSectorBytes;
    }
    return cap;
}

/* Bytes of the next segment: a whole one, or what is left of the file. */
uint32_t async_segment_bytes(const MusicSlot& slot) {
    const uint32_t cap = async_segment_cap(slot.format, slot.channels);
    const uint32_t left = slot.src_bytes - slot.file_pos;
    return left < cap ? left : cap;
}

/* Frames a segment of `payload` bytes can add to the rings at most. */
uint32_t async_payload_frames(uint8_t format, uint8_t channels, uint32_t payload) {
    if (format == SAT_AUDIO_ADX_ADPCM) {
        namespace codec = saturn::core::audio::adx_adpcm;
        const uint32_t n = codec::block_bytes(channels);
        return (payload + n - 1u) / n * codec::kBlockFrames;
    }
    if (format == SAT_AUDIO_IMA_ADPCM) {
        namespace codec = saturn::core::audio::ima_adpcm;
        // A block may be completed by this segment's first bytes: whole blocks in (carry + payload).
        const uint32_t block_bytes = codec::block_bytes(channels);
        return (payload + block_bytes - 1u) / block_bytes * codec::kBlockFrames;
    }
    const uint32_t frame_bytes = bytes_per_sample(format) * channels;
    return frame_bytes == 0u ? 0u : payload / frame_bytes;
}

/* Frames the next segment can add to the rings at most. */
uint32_t async_segment_bound(const MusicSlot& slot, uint32_t segment) {
    // ADX reserves nothing: its segment waits in the drive buffer and decodes into the ring as room appears.
    if (slot.format == SAT_AUDIO_ADX_ADPCM) return 0u;
    const uint32_t payload = segment > slot.skip_bytes ? segment - slot.skip_bytes : 0u;
    return async_payload_frames(slot.format, slot.channels, payload);
}

/* The smallest ring a disc track can start in. The SCSP stream starts when both its Sound RAM halves are written
 * (2 chunks of frames), and the feed starts it when no further whole segment fits the ring: a ring under
 * 2 chunks + one segment never reaches both, and the track stays silent. */
uint32_t async_min_ring_frames(uint8_t format, uint8_t channels) {
    if (format == SAT_AUDIO_ADX_ADPCM) {
        // A compressed segment decodes one block at a time: the ring needs room for one block, not the segment.
        namespace codec = saturn::core::audio::adx_adpcm;
        return 2u * saturn::core::kAudioStreamChunkFrames + codec::kBlockFrames;
    }
    return 2u * saturn::core::kAudioStreamChunkFrames +
           async_payload_frames(format, channels, async_segment_cap(format, channels));
}

uint8_t async_next(void* user, const sat_cdfs_file_t** file, uint32_t* offset, uint32_t* bytes) {
    const MusicSlot& slot = *static_cast<const MusicSlot*>(user);
    if ((slot.playing == 0u && slot.prefetch == 0u) || slot.pend_count >= 2u) return 0u;
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
    if (slot.file_pos >= slot.src_bytes) {
        slot.file_pos = 0u;
        if (slot.format == SAT_AUDIO_ADX_ADPCM) slot.skip_bytes = 4u;
    }
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


/* The disc scheduler delivers only one slice per service visit. A single
 * ADX group contains just 32 frames: delivering one group per display frame
 * would cap playback at ~1920 frames/s. Decode up to 2048 frames in a visit,
 * then issue a single write per channel, preserving both ring lockstep and
 * predictor history. The 128-byte block buffer lives on the SH-2 stack. */
sat_result_t async_deliver_adx(MusicSlot& slot, const uint8_t* data,
                               uint32_t bytes, uint32_t* consumed) {
    namespace codec = saturn::core::audio::adx_adpcm;
    const uint32_t block_bytes = codec::block_bytes(slot.channels);
    uint32_t room = writable_frames(slot);
    if (room > kMusicFeedFrames) room = kMusicFeedFrames;
    uint32_t frames_out = 0u;
    uint32_t used = 0u;
    uint8_t pcm_block[2u * codec::kChannelBytes] = {};
    const uint32_t right_offset = kMusicFeedFrames * 2u;

    while (used < bytes && frames_out + codec::kBlockFrames <= room) {
        const uint8_t* source = nullptr;
        if (slot.carry_have != 0u) {
            uint32_t copy = block_bytes - slot.carry_have;
            if (copy > bytes - used) copy = bytes - used;
            for (uint32_t i = 0u; i < copy; ++i)
                slot.encoded[slot.carry_have + i] = data[used + i];
            slot.carry_have += copy;
            used += copy;
            if (slot.carry_have < block_bytes) break;
            slot.carry_have = 0u;
            source = slot.encoded;
        } else if (bytes - used >= block_bytes) {
            source = data + used;
            used += block_bytes;
        } else {
            slot.carry_have = bytes - used;
            for (uint32_t i = 0u; i < slot.carry_have; ++i)
                slot.encoded[i] = data[used + i];
            used = bytes;
            break;
        }
        if (!codec::decode_block(source, slot.channels, slot.adx_state,
                                 pcm_block)) return SAT_ERR_IO;
        uint32_t take = codec::kBlockFrames;
        if (take > slot.sample_count - slot.position_frame)
            take = slot.sample_count - slot.position_frame;
        for (uint32_t i = 0u; i < take * 2u; ++i)
            slot.staging_split[frames_out * 2u + i] = pcm_block[i];
        if (slot.channels == 2u)
            for (uint32_t i = 0u; i < take * 2u; ++i)
                slot.staging_split[right_offset + frames_out * 2u + i] =
                    pcm_block[codec::kChannelBytes + i];
        slot.position_frame += take;
        frames_out += take;
        if (slot.position_frame == slot.sample_count) {
            slot.position_frame = 0u;
            codec::reset_history(slot.adx_state);
        }
    }
    *consumed = used;
    if (frames_out == 0u) return SAT_OK;
    SAT_TRY(write_channel(slot, 0u, slot.staging_split, frames_out));
    if (slot.channels == 2u)
        SAT_TRY(write_channel(slot, 1u, slot.staging_split + right_offset, frames_out));
    async_wrote(slot, frames_out);
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
            : slot.format == SAT_AUDIO_ADX_ADPCM
              ? async_deliver_adx(slot, data, bytes, &used)
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
    if (slot.format == SAT_AUDIO_ADX_ADPCM) {
        namespace codec = saturn::core::audio::adx_adpcm;
        return writable_frames(slot) < codec::kBlockFrames;     // the ring cannot take another block
    }
    if (slot.pend_count != 0u) return false;
    return writable_frames(slot) < async_segment_bound(slot, async_segment_bytes(slot));
}

/* A prefetched disc track counts as ready (and starts at once on play) with this many frames in its
 * rings: enough to ride out a seek and the reserve, and the rest fills at normal priority. A drive
 * with little spare throughput would otherwise take minutes to fill the whole ring. */
constexpr uint32_t kPrefetchReadyFrames = 16384u;

uint32_t ring_buffered(const MusicSlot& slot) {
    uint32_t frames = sat_audio_stream_buffered(slot.stream);
    if (slot.channels == 2u) {
        const uint32_t right = sat_audio_stream_buffered(slot.stream_r);
        if (right < frames) frames = right;
    }
    return frames;
}

/* The rings of a track that sat_music_prefetch was called on hold what play needs to start at once. */
bool prefetch_ready(const MusicSlot& slot) {
    if (slot.prefetch == 0u || slot.playing != 0u) return false;
    return slot.file != nullptr
        ? ring_buffered(slot) >= (slot.ring_frames < kPrefetchReadyFrames
            ? slot.ring_frames : kPrefetchReadyFrames)
        : writable_frames(slot) == 0u;
}

/* Both channel streams start together, once the rings hold what they will keep topped up. */
void start_streams(MusicSlot& slot) {
    slot.starting = 0u;
    if (slot.file != nullptr && g_disc_primary == &slot) sat_disc_stream_music_set_starting(g_disc, 0u);
    (void)sat_audio_stream_resume(slot.stream);
    if (slot.channels == 2u) (void)sat_audio_stream_resume(slot.stream_r);
}

void async_tick(void* user) {
    MusicSlot& slot = *static_cast<MusicSlot*>(user);
    if (slot.starting != 0u && async_primed(slot)) start_streams(slot);
}

const sat_disc_music_feed_ops_t kAsyncFeed = {async_next, async_started, async_deliver, async_buffered, 1u, async_tick, 1u};

/* Where frame `frame` lies in the file: the sector the read starts at and the bytes to drop.
 * The scheduler has dropped the segments read for the old position (sat_disc_stream_music_abort). */
void async_locate(MusicSlot& slot, uint32_t frame) {
    uint32_t offset;
    if (slot.format == SAT_AUDIO_ADX_ADPCM) {
        namespace codec = saturn::core::audio::adx_adpcm;
        offset = codec::kCoefficientBytes + frame / codec::kBlockFrames *
                 codec::block_bytes(slot.channels);
        slot.position_frame = frame;
        codec::reset_history(slot.adx_state);
    } else if (slot.format == SAT_AUDIO_IMA_ADPCM) {
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

} // namespace saturn::core::music
