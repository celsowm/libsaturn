#include "saturn/adx.h"
#include "src/audio/playback/adx_adpcm.hpp"

namespace {
namespace codec = saturn::core::audio::adx_adpcm;

constexpr uint32_t kSectorBytes = 2048u;
constexpr uint32_t kSegmentBytes = 4096u;  // two sectors, ~3640 stereo frames
constexpr uint32_t kPrimeFrames = 12288u; // 0.56 s at 22.05 kHz
constexpr uint32_t kRightStageOffset = SAT_ADX_STAGING_FRAMES * 2u;

uint32_t writable(const sat_adx_player_t& p) {
    uint32_t room = sat_audio_stream_available(p.left);
    if (p.channels == 2u) {
        const uint32_t right = sat_audio_stream_available(p.right);
        if (right < room) room = right;
    }
    return room;
}
uint32_t buffered(const sat_adx_player_t& p) {
    uint32_t frames = sat_audio_stream_buffered(p.left);
    if (p.channels == 2u) {
        const uint32_t right = sat_audio_stream_buffered(p.right);
        if (right < frames) frames = right;
    }
    return frames;
}
uint32_t segment_bytes(const sat_adx_player_t& p) {
    if (p.read_offset >= p.payload_bytes) return 0u;
    const uint32_t remaining = p.payload_bytes - p.read_offset;
    return remaining < kSegmentBytes ? remaining : kSegmentBytes;
}
uint32_t segment_bound(const sat_adx_player_t& p, uint32_t bytes) {
    const uint32_t payload = bytes > p.read_skip ? bytes - p.read_skip : 0u;
    const uint32_t frame_bytes = codec::block_bytes(p.channels);
    return ((payload + frame_bytes - 1u) / frame_bytes) * codec::kBlockFrames;
}
void reset_predictor(sat_adx_player_t& p) {
    for (uint32_t c = 0u; c < 2u; ++c) {
        p.previous[c][0] = 0;
        p.previous[c][1] = 0;
    }
}

/* Every next segment reserves its entire worst-case decoded output, including
 * the in-flight and delivery-queued segment. No decode callback can overfill
 * the two PCM rings even if the drive races ahead. */
uint8_t next(void* user, const sat_cdfs_file_t** file,
             uint32_t* offset, uint32_t* bytes) {
    auto& p = *static_cast<sat_adx_player_t*>(user);
    if (!p.wanted || p.failed || p.pending_count >= 2u) return 0u;
    const uint32_t segment = segment_bytes(p);
    if (segment == 0u) return 0u;
    uint32_t reserved = 0u;
    for (uint32_t i = 0u; i < p.pending_count; ++i)
        reserved += p.pending[i].reserved_frames;
    const uint32_t room = writable(p);
    if (room < reserved || segment_bound(p, segment) > room - reserved)
        return 0u;
    *file = p.file;
    *offset = p.read_offset;
    *bytes = segment;
    return 1u;
}

void started(void* user) {
    auto& p = *static_cast<sat_adx_player_t*>(user);
    const uint32_t bytes = segment_bytes(p);
    sat_adx_pending_t& pending = p.pending[p.pending_count++];
    pending.bytes_left = bytes;
    pending.reserved_frames = segment_bound(p, bytes);
    pending.skip_bytes = p.read_skip;
    p.read_skip = 0u;
    p.read_offset += ((bytes + kSectorBytes - 1u) / kSectorBytes) * kSectorBytes;
    if (p.read_offset >= p.payload_bytes) {
        p.read_offset = 0u;
        p.read_skip = codec::kCoefficientBytes;
    }
}

/* Decode one CD delivery slice into two contiguous planar staging regions.
 * A partial 18/36-byte ADX block is held in carry across deliveries.
 * Each CD scheduler visit decodes at most 2048 PCM frames, preventing the
 * SH-2 from paying for an entire segment in one display frame. */
sat_result_t decode_slice(sat_adx_player_t& p, const uint8_t* source,
                          uint32_t bytes, uint32_t* used_out,
                          uint32_t* frames_out) {
    const uint32_t group_bytes = codec::block_bytes(p.channels);
    uint32_t room = writable(p);
    if (room > SAT_ADX_STAGING_FRAMES) room = SAT_ADX_STAGING_FRAMES;
    uint32_t used = 0u;
    uint32_t produced = 0u;
    uint8_t planar[2u * codec::kChannelBytes] = {};

    codec::State decoder{};
    decoder.coefficient[0] = p.coefficient[0];
    decoder.coefficient[1] = p.coefficient[1];
    for (uint32_t c = 0u; c < 2u; ++c)
        for (uint32_t i = 0u; i < 2u; ++i)
            decoder.previous[c][i] = p.previous[c][i];

    while (used < bytes && produced + codec::kBlockFrames <= room) {
        const uint8_t* compressed;
        if (p.carry_bytes != 0u) {
            uint32_t copy = group_bytes - p.carry_bytes;
            if (copy > bytes - used) copy = bytes - used;
            for (uint32_t i = 0u; i < copy; ++i)
                p.carry[p.carry_bytes + i] = source[used + i];
            p.carry_bytes = static_cast<uint8_t>(p.carry_bytes + copy);
            used += copy;
            if (p.carry_bytes != group_bytes) break;
            p.carry_bytes = 0u;
            compressed = p.carry;
        } else if (bytes - used >= group_bytes) {
            compressed = source + used;
            used += group_bytes;
        } else {
            p.carry_bytes = static_cast<uint8_t>(bytes - used);
            for (uint32_t i = 0u; i < p.carry_bytes; ++i)
                p.carry[i] = source[used + i];
            used = bytes;
            break;
        }
        if (!codec::decode_block(compressed, p.channels, decoder, planar))
            return SAT_ERR_IO;
        uint32_t take = codec::kBlockFrames;
        if (take > p.sample_count - p.position_frame)
            take = p.sample_count - p.position_frame;
        for (uint32_t i = 0u; i < take * 2u; ++i)
            p.staging[produced * 2u + i] = planar[i];
        if (p.channels == 2u)
            for (uint32_t i = 0u; i < take * 2u; ++i)
                p.staging[kRightStageOffset + produced * 2u + i] =
                    planar[codec::kChannelBytes + i];
        produced += take;
        p.position_frame += take;
        if (p.position_frame == p.sample_count) {
            p.position_frame = 0u;
            codec::reset_history(decoder);
        }
    }

    for (uint32_t c = 0u; c < 2u; ++c)
        for (uint32_t i = 0u; i < 2u; ++i)
            p.previous[c][i] = decoder.previous[c][i];
    if (produced != 0u) {
        SAT_TRY(sat_audio_stream_write(p.left, p.staging, produced));
        if (p.channels == 2u)
            SAT_TRY(sat_audio_stream_write(
                p.right, p.staging + kRightStageOffset, produced));
        p.decoded_frames += produced;
        if (produced > p.max_decode_frames) p.max_decode_frames = produced;
    }
    *used_out = used;
    *frames_out = produced;
    return SAT_OK;
}

/* A pending record persists until ALL bytes of its segment were handed to
 * us. Consuming a full slice is NOT equivalent to completing a segment. */
sat_result_t deliver(void* user, const uint8_t* data, uint32_t bytes,
                     uint32_t* consumed) {
    auto& p = *static_cast<sat_adx_player_t*>(user);
    if (consumed == nullptr || data == nullptr) return SAT_ERR_INVALID_ARG;
    if (!p.wanted || p.pending_count == 0u || p.failed) {
        *consumed = bytes;
        return SAT_OK;
    }
    sat_adx_pending_t& pending = p.pending[0];
    uint32_t used = 0u, produced = 0u;
    sat_result_t result = SAT_OK;
    if (pending.skip_bytes != 0u) {
        used = pending.skip_bytes < bytes ? pending.skip_bytes : bytes;
        pending.skip_bytes -= used;
    } else {
        result = decode_slice(p, data, bytes, &used, &produced);
    }
    if (result != SAT_OK || (used == 0u && bytes != 0u)) {
        ++p.failed_deliveries;
        p.failed = 1u;
        p.wanted = 0u;
        *consumed = bytes;
        return result == SAT_OK ? SAT_ERR_CAPACITY : result;
    }
    ++p.delivery_calls;
    pending.bytes_left = used < pending.bytes_left ? pending.bytes_left - used : 0u;
    if (pending.reserved_frames >= produced)
        pending.reserved_frames -= produced;
    else
        pending.reserved_frames = 0u;
    if (pending.bytes_left == 0u) {
        p.pending[0] = p.pending[1];
        --p.pending_count;
    }
    *consumed = used;
    return SAT_OK;
}
uint32_t available_buffered(void* user) {
    const auto& p = *static_cast<const sat_adx_player_t*>(user);
    return !p.wanted ? SAT_DISC_MUSIC_RING_FRAMES : buffered(p);
}
void tick(void*) {}

const sat_disc_music_feed_ops_t kFeed = {
    next, started, deliver, available_buffered, 1u, tick
};

sat_result_t start_streams(sat_adx_player_t& p) {
    SAT_TRY(sat_audio_stream_resume(p.left));
    if (p.channels == 2u) SAT_TRY(sat_audio_stream_resume(p.right));
    p.starting = 0u;
    sat_disc_stream_music_set_starting(p.disc, 0u);
    return SAT_OK;
}

} // namespace

extern "C" sat_result_t sat_adx_player_open(sat_adx_player_t* player,
        sat_disc_stream_t* disc, const sat_cdfs_file_t* file,
        const sat_adx_config_t* config, sat_adx_pcm_rings_t* rings) {
    if (!player || !disc || !file || !config || !rings ||
        player->opened || !config->sample_rate || !config->sample_count ||
        (config->channels != 1u && config->channels != 2u))
        return SAT_ERR_INVALID_ARG;
    if (sat_audio_is_initialized() == 0u) return SAT_ERR_NOT_INITIALIZED;
    const uint64_t bytes = codec::kCoefficientBytes +
        ((static_cast<uint64_t>(config->sample_count) + 31u) / 32u) *
        codec::block_bytes(config->channels);
    if (bytes > UINT32_MAX || file->size < bytes) return SAT_ERR_INVALID_ARG;
    sat_audio_spec_t spec = {config->sample_rate,
        SAT_ADX_PCM_RING_FRAMES, 1u, SAT_AUDIO_PCM_S16, 0u};
    SAT_TRY(sat_audio_stream_open(
        &player->left, &spec, rings->left, sizeof(rings->left)));
    SAT_TRY(sat_audio_stream_pause(player->left));
    if (config->channels == 2u) {
        const sat_result_t right = sat_audio_stream_open(
            &player->right, &spec, rings->right, sizeof(rings->right));
        if (right != SAT_OK) {
            (void)sat_audio_stream_close(player->left);
            return right;
        }
        SAT_TRY(sat_audio_stream_pause(player->right));
        (void)sat_audio_stream_set_pan(player->left, SAT_AUDIO_PAN_LEFT);
        (void)sat_audio_stream_set_pan(player->right, SAT_AUDIO_PAN_RIGHT);
    }

    player->disc = disc;
    player->file = file;
    player->sample_rate = config->sample_rate;
    player->sample_count = config->sample_count;
    player->payload_bytes = static_cast<uint32_t>(bytes);
    player->read_offset = 0u;
    player->read_skip = codec::kCoefficientBytes;
    player->position_frame = 0u;
    player->decoded_frames = 0u;
    player->delivery_calls = 0u;
    player->max_decode_frames = 0u;
    player->failed_deliveries = 0u;
    player->channels = config->channels;
    player->pending_count = 0u;
    player->carry_bytes = 0u;
    player->failed = 0u;
    player->wanted = 0u;
    player->starting = 0u;
    player->coefficient[0] = config->coefficient_1;
    player->coefficient[1] = config->coefficient_2;
    reset_predictor(*player);

    const sat_result_t attached = sat_disc_stream_music_attach(disc, &kFeed, player);
    if (attached != SAT_OK) {
        (void)sat_audio_stream_close(player->left);
        if (config->channels == 2u)
            (void)sat_audio_stream_close(player->right);
        player->disc = nullptr;
        return attached;
    }
    player->opened = 1u;
    return SAT_OK;
}

extern "C" sat_result_t sat_adx_player_play(sat_adx_player_t* p) {
    if (!p || !p->opened) return SAT_ERR_INVALID_ARG;
    if (p->failed) return SAT_ERR_IO;
    if (p->wanted) return SAT_OK;
    p->wanted = 1u;
    p->starting = 1u;
    sat_disc_stream_music_set_starting(p->disc, 1u);
    if (buffered(*p) >= kPrimeFrames) return start_streams(*p);
    return SAT_OK;
}
extern "C" sat_result_t sat_adx_player_pause(sat_adx_player_t* p) {
    if (!p || !p->opened) return SAT_ERR_INVALID_ARG;
    SAT_TRY(sat_audio_stream_pause(p->left));
    if (p->channels == 2u) SAT_TRY(sat_audio_stream_pause(p->right));
    p->wanted = 0u;
    p->starting = 0u;
    sat_disc_stream_music_set_starting(p->disc, 0u);
    return SAT_OK;
}
extern "C" sat_result_t sat_adx_player_stop(sat_adx_player_t* p) {
    if (!p || !p->opened) return SAT_ERR_INVALID_ARG;
    SAT_TRY(sat_adx_player_pause(p));
    sat_disc_stream_music_abort(p->disc);
    SAT_TRY(sat_audio_stream_flush(p->left));
    if (p->channels == 2u) SAT_TRY(sat_audio_stream_flush(p->right));
    p->read_offset = 0u;
    p->read_skip = codec::kCoefficientBytes;
    p->position_frame = 0u;
    p->pending_count = 0u;
    p->carry_bytes = 0u;
    p->failed = 0u;
    reset_predictor(*p);
    return SAT_OK;
}
extern "C" sat_result_t sat_adx_player_update(sat_adx_player_t* p) {
    if (!p || !p->opened) return SAT_ERR_INVALID_ARG;
    SAT_TRY(sat_disc_stream_service(p->disc));
    if (p->failed) return SAT_ERR_IO;
    if (p->wanted && p->starting && buffered(*p) >= kPrimeFrames)
        return start_streams(*p);
    return SAT_OK;
}
extern "C" sat_result_t sat_adx_player_info(const sat_adx_player_t* p,
        sat_adx_player_info_t* out_info) {
    if (!p || !p->opened || !out_info) return SAT_ERR_INVALID_ARG;
    sat_audio_stream_stats_t left{};
    SAT_TRY(sat_audio_stream_stats(p->left, &left));
    uint32_t underruns = left.underrun_count;
    if (p->channels == 2u) {
        sat_audio_stream_stats_t right{};
        SAT_TRY(sat_audio_stream_stats(p->right, &right));
        underruns += right.underrun_count;
    }
    *out_info = {};
    out_info->buffered_frames = buffered(*p);
    out_info->decoded_frames = p->decoded_frames;
    out_info->delivery_calls = p->delivery_calls;
    out_info->max_decode_frames = p->max_decode_frames;
    out_info->underrun_count = underruns;
    out_info->failed_deliveries = p->failed_deliveries;
    out_info->playing = p->wanted && !p->starting && !p->failed ? 1u : 0u;
    out_info->starting = p->starting;
    out_info->channels = p->channels;
    return SAT_OK;
}
extern "C" sat_result_t sat_adx_player_close(sat_adx_player_t* p) {
    if (!p || !p->opened) return SAT_ERR_INVALID_ARG;
    (void)sat_adx_player_pause(p);
    sat_disc_stream_music_detach(p->disc);
    SAT_TRY(sat_audio_stream_close(p->left));
    if (p->channels == 2u) SAT_TRY(sat_audio_stream_close(p->right));
    p->opened = 0u;
    p->disc = nullptr;
    p->file = nullptr;
    return SAT_OK;
}
