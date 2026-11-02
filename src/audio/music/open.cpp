#include "src/audio/music/internal.hpp"
#include "saturn/music_codecs.h"

using namespace saturn::core::music;

extern "C" sat_result_t sat_music_open(sat_music_t* out_music, const sat_music_desc_t* desc) {
    if (out_music == nullptr || desc == nullptr || desc->asset == nullptr ||
        desc->workspace == nullptr) return SAT_ERR_INVALID_ARG;
    const char* logical_path = desc->asset;
    if (sat_audio_is_initialized() == 0u) return SAT_ERR_NOT_INITIALIZED;
    if (desc->ring_frames < SAT_MUSIC_MIN_RING_FRAMES ||
        desc->ring_frames > SAT_MUSIC_MAX_RING_FRAMES ||
        (desc->ring_frames % 4096u) != 0u ||
        (reinterpret_cast<uintptr_t>(desc->workspace) & 3u) != 0u)
        return SAT_ERR_INVALID_ARG;
    if (desc->disc != nullptr && g_disc != desc->disc &&
        (g_disc_primary != nullptr || g_disc_standby != nullptr)) return SAT_ERR_BUSY;
    const uintptr_t begin = reinterpret_cast<uintptr_t>(desc->workspace);
    const uintptr_t end = begin + desc->workspace_bytes;
    if (end < begin) return SAT_ERR_INVALID_ARG;
    for (const MusicSlot& active : g_music) {
        if (active.used == 0u) continue;
        const uintptr_t other = reinterpret_cast<uintptr_t>(active.workspace);
        if (begin < other + active.workspace_bytes && other < end)
            return SAT_ERR_BUSY;
    }

    sat_asset_t asset{};
    SAT_TRY(sat_asset_open(logical_path, &asset));
    sat_asset_info_t info{};
    SAT_TRY(sat_asset_info(asset, &info));
    if (info.kind != SAT_ASSET_STREAM ||
        (info.data == nullptr && info.source_path == nullptr) || info.sample_rate == 0u ||
        (info.channels != 1u && info.channels != 2u)) return SAT_ERR_UNSUPPORTED;
    const bool is_adx = info.format == SAT_AUDIO_ADX_ADPCM;
    const uint8_t compressed = (info.format == SAT_AUDIO_IMA_ADPCM || is_adx) ? 1u : 0u;
    const uint32_t sample_bytes = compressed != 0u ? 2u : bytes_per_sample(info.format);
    const uint32_t frame_bytes = sample_bytes * info.channels;
    if (sample_bytes == 0u || info.size == 0u) return SAT_ERR_INVALID_ARG;
    uint32_t sample_count;
    if (compressed != 0u) {
        sample_count = info.sample_count;
        if (sample_count == 0u) return SAT_ERR_INVALID_ARG;
        if (is_adx) {
            namespace codec = saturn::core::audio::adx_adpcm;
            const uint64_t blocks =
                (static_cast<uint64_t>(sample_count) + codec::kBlockFrames - 1u) /
                codec::kBlockFrames;
            if (codec::kCoefficientBytes +
                blocks * codec::block_bytes(info.channels) != info.size)
                return SAT_ERR_INVALID_ARG;
        } else {
            namespace codec = saturn::core::audio::ima_adpcm;
            const uint64_t blocks =
                (static_cast<uint64_t>(sample_count) + codec::kBlockFrames - 1u) /
                codec::kBlockFrames;
            if (blocks * codec::block_bytes(info.channels) != info.size)
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
    const sat_cdfs_file_t* disc_file = nullptr;
    uint32_t src_bytes = 0u;
    const bool on_disc = info.data == nullptr && info.source_path != nullptr &&
        sat_cdfs_source_file(info.source_path, &disc_file) == SAT_OK;
    // A track on the disc is read by the scheduler. The synchronous reader asks the drive for one block at a time
    // (32 frames of ADX, 1024 of IMA): hundreds of blocking CD reads a second. RAM tracks and non-disc readers
    // keep it.
    if (on_disc && desc->disc == nullptr) return SAT_ERR_UNSUPPORTED;
    // The program's codec table (saturn/music_codecs.h) names the formats and sources it carries.
    if (on_disc) {
        if (info.format == SAT_AUDIO_IMA_ADPCM ? sat_music_codecs.ima_disc == nullptr
            : !is_adx && sat_music_codecs.pcm_disc == nullptr) return SAT_ERR_UNSUPPORTED;
    } else if (sat_music_codecs.resident == nullptr) {
        return SAT_ERR_UNSUPPORTED;
    }
    // The scheduler delivers the bytes: a disc track needs no read buffer of its own.
    const uint32_t needed = on_disc ? SAT_MUSIC_DISC_WORKSPACE_BYTES(desc->ring_frames, info.channels)
                                    : SAT_MUSIC_WORKSPACE_BYTES(desc->ring_frames, info.channels);
    if (desc->workspace_bytes < needed) return SAT_ERR_CAPACITY;
    if (on_disc) {
        if (g_disc_primary != nullptr && g_disc_standby != nullptr) return SAT_ERR_BUSY;
        if (desc->ring_frames < async_min_ring_frames(info.format, static_cast<uint8_t>(info.channels)))
            return SAT_ERR_INVALID_ARG;   // would never fill both SCSP halves: silent
        src_bytes = compressed != 0u ? info.size : sample_count * frame_bytes;
        if (disc_file->size < src_bytes) return SAT_ERR_INVALID_ARG;
    } else {
        disc_file = nullptr;
    }

    for (uint16_t i = 0u; i < SAT_MUSIC_CAPACITY; ++i) {
        MusicSlot& slot = g_music[i];
        if (slot.used != 0u) continue;
        if (desc->disc != nullptr) g_disc = desc->disc;
        const uint32_t ring_bytes = desc->ring_frames * 2u;
        uint8_t* const memory = static_cast<uint8_t*>(desc->workspace);
        slot.buffer = memory;
        slot.buffer_r = memory + ring_bytes;
        slot.staging = on_disc ? nullptr : memory + ring_bytes * info.channels;
        slot.staging_split = memory + ring_bytes * info.channels + (on_disc ? 0u : kMusicFeedBytes);
        slot.encoded = slot.staging_split + kMusicFeedBytes;
        slot.workspace = desc->workspace;
        slot.workspace_bytes = desc->workspace_bytes;
        slot.ring_frames = desc->ring_frames;
        if (slot.generation == 0u) slot.generation = 1u;
        uint16_t path_length = 0u;
        while (logical_path[path_length] != '\0') {
            if (path_length + 1u >= SAT_FILE_PATH_MAX) return SAT_ERR_CAPACITY;
            ++path_length;
        }
        sat_audio_spec_t spec = {
            info.sample_rate, slot.ring_frames, 1u,
            compressed != 0u
                ? static_cast<uint8_t>(SAT_AUDIO_PCM_S16)
                : info.format,
            0u};
        const sat_result_t st = sat_audio_stream_open(
            &slot.stream, &spec, slot.buffer, ring_bytes);
        if (st != SAT_OK) return st;
        // CD reads pump sat_audio_update(). Keep an unopened music stream
        // silent until both channel rings have been primed by music_play().
        (void)sat_audio_stream_pause(slot.stream);
        const uint8_t stereo = info.channels == 2u ? 1u : 0u;
        if (stereo != 0u) {
            const sat_result_t st_r = sat_audio_stream_open(
                &slot.stream_r, &spec, slot.buffer_r, ring_bytes);
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
        // The 4-byte LPC header belongs to the container, not the first
        // ADX audio frame. The sector reader starts at offset zero.
        slot.skip_bytes = is_adx ? 4u : 0u;
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
        if (is_adx) {
            uint8_t coeff[4];
            if (info.data != nullptr) {
                const uint8_t* source = static_cast<const uint8_t*>(info.data);
                for (uint32_t k = 0u; k < 4u; ++k) coeff[k] = source[k];
            } else {
                // The file's own reader: the asset cache and the path lookup are not needed for 4 bytes.
                sat_file_read_at_fn read_at = nullptr;
                void* context = nullptr;
                uint32_t count = 0u;
                sat_result_t read_result = sat_file_backend(info.source_path, &read_at, &context);
                if (read_result == SAT_OK) read_result = read_at(context, 0u, coeff, sizeof(coeff), &count);
                if (read_result != SAT_OK || count != sizeof(coeff)) {
                    (void)sat_audio_stream_close(slot.stream);
                    if (stereo != 0u) (void)sat_audio_stream_close(slot.stream_r);
                    return read_result == SAT_OK ? SAT_ERR_IO : read_result;
                }
            }
            saturn::core::audio::adx_adpcm::init(slot.adx_state, coeff);
        }
        slot.channels = info.channels;
        slot.format = info.format;
        slot.playing = 0u;
        slot.starting = 0u;
        slot.prefetch = 0u;
        slot.used = 1u;
        if (disc_file != nullptr) {
            // The first disc track is the one the drive plays; a second waits behind it as the standby.
            const bool as_primary = g_disc_primary == nullptr;
            const sat_result_t attached = as_primary
                ? sat_disc_stream_music_attach(g_disc, &kAsyncFeed, &slot)
                : sat_disc_stream_music_attach_standby(g_disc, &kAsyncFeed, &slot);
            if (attached != SAT_OK) {
                slot.used = 0u;
                (void)sat_audio_stream_close(slot.stream);
                if (stereo != 0u) (void)sat_audio_stream_close(slot.stream_r);
                return attached;
            }
            slot.file = disc_file;
            slot.src_bytes = src_bytes;
            if (as_primary) g_disc_primary = &slot;
            else g_disc_standby = &slot;
        }
        out_music->slot = i;
        out_music->generation = slot.generation;
        return SAT_OK;
    }
    return SAT_ERR_CAPACITY;
}
