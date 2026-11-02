#ifndef SATURN_AUDIO_MUSIC_INTERNAL_HPP
#define SATURN_AUDIO_MUSIC_INTERNAL_HPP
#include <stdint.h>
#include "saturn/music.h"
#include "saturn/asset.h"
#include "saturn/cdfs.h"
#include "saturn/disc_stream.h"
#include "src/audio/music/codecs/ima.hpp"
#include "src/audio/music/codecs/adx.hpp"

namespace saturn::core::music {

constexpr uint32_t kMusicFeedFrames = 2048u;
constexpr uint32_t kMusicUpdateFeedLimit = 4u;
constexpr uint32_t kMusicFeedBytes = kMusicFeedFrames * 2u * 2u;

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
    saturn::core::audio::adx_adpcm::State adx_state;
    // Disc-scheduled feed (bound when the track opens): the track is read in
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
    uint8_t prefetch;             // sat_music_prefetch: fill the rings, silent, before play
    uint8_t used;
    uint16_t generation;
    uint8_t* buffer;
    uint8_t* buffer_r;
    uint8_t* staging;
    uint8_t* staging_split;
    uint8_t* encoded;
    uint32_t ring_frames;
    void* workspace;
    uint32_t workspace_bytes;
};

extern MusicSlot g_music[SAT_MUSIC_CAPACITY];
extern sat_disc_stream_t* g_disc;
extern MusicSlot* g_disc_primary;
extern MusicSlot* g_disc_standby;
extern uint8_t g_prefetch_while_playing;

uint32_t bytes_per_sample(uint8_t format);
sat_result_t write_channel(MusicSlot& slot, uint32_t channel,
                           const uint8_t* frames, uint32_t count);
void deinterleave_stereo(const uint8_t* source, uint8_t* planar,
                         uint32_t frames, uint32_t sample_bytes);
uint32_t writable_frames(const MusicSlot& slot);
sat_result_t feed(MusicSlot& slot);

extern const sat_disc_music_feed_ops_t kAsyncFeed;
bool async_primed(const MusicSlot& slot);
uint32_t ring_buffered(const MusicSlot& slot);
bool prefetch_ready(const MusicSlot& slot);
void start_streams(MusicSlot& slot);
void async_locate(MusicSlot& slot, uint32_t frame);

} // namespace saturn::core::music
#endif
