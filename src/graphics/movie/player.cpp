#include "saturn/movie.h"

#include "saturn/color.h"
#include "saturn/dma.h"
#include "saturn/input.h"
#include "saturn/saturn.h"
#include "saturn/time.h"
#include "saturn/vdp2.h"
#include "saturn/vdp2_color_offset.h"
#include "saturn/vdp2_layers.h"
#include "saturn/video.h"
#include "src/core/runtime/state.hpp"

/* sat_movie_play: the movie file layout is in tools/satmovie.py. */

namespace {

constexpr uint32_t kHeaderBytes = 2048u;
constexpr uint32_t kSector = 2048u;
constexpr uint32_t kPrerollSectors = 24u;
constexpr uint32_t kBankBytes = 0x40000u;      /* VRAM-A and VRAM-B each hold one 512x256 15-bit bitmap */
constexpr uintptr_t kVramBase = 0x25E00000u;
constexpr uint32_t kDefaultLatencyFields = 8u;
constexpr uint32_t kRingBytes = SAT_MOVIE_RING_SECTORS * kSector;
constexpr uint32_t kBlockBytes = SAT_AUDIO_IMA_BLOCK_BYTES(2);
constexpr uint32_t kBlockFrames = SAT_AUDIO_IMA_BLOCK_FRAMES;

uint32_t be32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | p[3];
}
uint32_t be16(const uint8_t* p) { return (static_cast<uint32_t>(p[0]) << 8) | p[1]; }

struct Header {
    uint32_t width, height, frames, frame_us, audio_bytes_rate, packet_bytes;
    bool has_sound;
};

bool parse_header(const uint8_t* p, Header* h) {
    static const uint8_t magic[8] = {'S', 'A', 'T', 'M', 'O', 'V', '0', '1'};
    for (unsigned i = 0u; i < 8u; ++i) {
        if (p[i] != magic[i]) return false;
    }
    h->width = be16(p + 8);
    h->height = be16(p + 10);
    h->frames = be32(p + 12);
    h->frame_us = be32(p + 16);
    h->has_sound = be32(p + 20) == 22050u && p[24] == 2u && p[25] == 1u && be16(p + 26) == kBlockFrames;
    h->packet_bytes = be32(p + 32);
    return h->frame_us != 0u && h->frames != 0u;
}

void poll_ring(sat_movie_player_t* m) {
    const uint32_t held = m->sectors_taken - m->used / kSector;
    if (m->remaining != 0u && held < m->stats.ring_low_sectors) m->stats.ring_low_sectors = held;
    if (m->remaining == 0u || held >= SAT_MOVIE_RING_SECTORS) return;
    uint32_t left = 0u;
    if (sat_cd_stream_poll(m->block, SAT_MOVIE_RING_SECTORS - held, &left) != SAT_OK) return;   /* seen as starvation */
    m->sectors_taken += m->remaining - left;
    m->remaining = left;
}

uint32_t available(const sat_movie_player_t* m) { return m->sectors_taken * kSector - m->used; }

/* `bytes` from the ring at stream offset `from`, wrapping once at most. Word copies: offsets and sizes are
 * multiples of 4. */
void ring_copy(const sat_movie_player_t* m, uint8_t* to, uint32_t from, uint32_t bytes) {
    uint32_t at = from % kRingBytes;
    while (bytes != 0u) {
        uint32_t n = kRingBytes - at;
        if (n > bytes) n = bytes;
        const uint32_t* in = reinterpret_cast<const uint32_t*>(m->ring + at);
        uint32_t* out = reinterpret_cast<uint32_t*>(to);
        for (uint32_t i = 0u; i < (n + 3u) / 4u; ++i) out[i] = in[i];
        to += n;
        bytes -= n;
        at = 0u;
    }
}

/* Decodes the next packet: its sound into the SCSP streams, its frame into the picture. false when the packet
 * has not landed or the sound has no room yet (nothing is used then). */
bool decode_next(sat_movie_player_t* m, bool has_sound) {
    uint8_t head[8] __attribute__((aligned(4)));
    if (available(m) < 8u) return false;
    ring_copy(m, head, m->used, 8u);
    const uint32_t video_bytes = be32(head), audio_bytes = be32(head + 4);
    const uint32_t packet = 8u + audio_bytes + video_bytes + ((4u - (video_bytes & 3u)) & 3u);
    if (packet > SAT_MOVIE_MAX_PACKET_BYTES || (audio_bytes % kBlockBytes) != 0u) return false;
    const uint32_t blocks = audio_bytes / kBlockBytes;
    if (available(m) < packet) return false;
    if (has_sound && sat_audio_stream_available(m->voice[0]) < blocks * kBlockFrames) return false;
    ring_copy(m, m->packet, m->used, packet);
    for (uint32_t b = 0u; has_sound && b < blocks; ++b) {
        if (sat_audio_ima_decode_block(m->packet + 8u + b * kBlockBytes, 2u, m->pcm) != SAT_OK) return false;
        for (unsigned c = 0u; c < 2u; ++c) {
            (void)sat_audio_stream_write(m->voice[c], m->pcm + c * 2u * kBlockFrames, kBlockFrames);
        }
    }
    if (sat_movie_decode_frame(&m->decoder, m->packet + 8u + audio_bytes, video_bytes, m->origin,
                               SAT_MOVIE_PICTURE_STRIDE) != SAT_OK) {
        return false;
    }
    m->used += packet;
    return true;
}

void fade(int level) {
    const sat_vdp2_color_offset_t offset = {static_cast<int16_t>(level), static_cast<int16_t>(level),
                                            static_cast<int16_t>(level)};
    (void)sat_vdp2_color_offset_set(SAT_VDP2_COLOR_OFFSET_A, &offset);
}

sat_result_t clear_both_bitmaps(sat_movie_player_t* m) {
    for (uint32_t i = 0u; i < sizeof m->picture / 4u; ++i) reinterpret_cast<uint32_t*>(m->picture)[i] = 0u;
    SAT_TRY(sat_dma_copy(reinterpret_cast<void*>(kVramBase), m->picture, sizeof m->picture));
    return sat_dma_copy(reinterpret_cast<void*>(kVramBase + kBankBytes), m->picture, sizeof m->picture);
}

/* The play itself. `opened` tells the caller's cleanup what to undo. */
sat_result_t run(sat_movie_player_t* m, sat_cdfs_volume_t* volume, const char* path,
                 const sat_movie_play_options_t& opt, bool* started_stream, bool* opened_voices, bool* skipped) {
    sat_cdfs_file_t file;
    SAT_TRY(sat_cdfs_lookup(volume, path, &file));
    uint32_t read = 0u;
    SAT_TRY(sat_cdfs_read_at(volume, &file, 0u, m->packet, kHeaderBytes, &read));
    Header h;
    if (read < 48u || !parse_header(m->packet, &h)) return SAT_ERR_VERSION;
    if (h.width > sat_video_width() || h.height > sat_video_height() || h.width > 320u || h.height > 224u) {
        return SAT_ERR_CAPACITY;
    }
    SAT_TRY(sat_movie_decoder_init(&m->decoder, static_cast<uint16_t>(h.width), static_cast<uint16_t>(h.height)));
    const uint32_t x0 = (sat_video_width() - h.width) / 2u, y0 = (sat_video_height() - h.height) / 2u;
    m->origin = m->picture + y0 * SAT_MOVIE_PICTURE_STRIDE + x0;
    m->sectors_taken = m->used = 0u;
    m->stats = {};
    m->stats.ring_low_sectors = SAT_MOVIE_RING_SECTORS;

    if (h.has_sound) {
        SAT_TRY(sat_audio_is_initialized() != 0u ? SAT_OK : sat_audio_init());
        const sat_audio_spec_t spec = {22050u, SAT_MOVIE_AUDIO_RING_FRAMES, 1u, SAT_AUDIO_PCM_S16, 0u};
        for (unsigned c = 0u; c < 2u; ++c) {
            SAT_TRY(sat_audio_stream_open(&m->voice[c], &spec, m->audio_ring[c], sizeof m->audio_ring[c]));
            *opened_voices = true;
            SAT_TRY(sat_audio_stream_pause(m->voice[c]));
            SAT_TRY(sat_audio_stream_set_pan(m->voice[c], c != 0u ? SAT_AUDIO_PAN_RIGHT : SAT_AUDIO_PAN_LEFT));
        }
    }

    uint32_t first_lba = 0u, sectors = 0u;
    SAT_TRY(sat_cdfs_file_sectors(&file, kHeaderBytes, 0u, &first_lba, &sectors));
    m->remaining = (h.packet_bytes + kSector - 1u) / kSector;
    if (m->remaining > sectors) return SAT_ERR_IO;
    SAT_TRY(clear_both_bitmaps(m));
    sat_vdp2_layer_config_t layer;
    sat_vdp2_layer_config_default(SAT_VDP2_NBG0, &layer);
    layer.bitmap = 1u;
    layer.bitmap_size = SAT_VDP2_BITMAP_512X256;
    layer.color_mode = SAT_VDP2_COLOR_MODE_32768;
    layer.priority = 7u;
    layer.transparent = 0u;
    for (unsigned i = 0u; i < 4u; ++i) layer.plane_address[i] = 0u;
    SAT_TRY(sat_vdp2_layer_configure(&layer));
    SAT_TRY(sat_vdp2_back_color_set(SAT_COLOR_BLACK));
    SAT_TRY(sat_vdp2_layers_commit());
    SAT_TRY(sat_cd_stream_begin_ring(m->block, first_lba, m->remaining, m->ring, SAT_MOVIE_RING_SECTORS));
    *started_stream = true;

    /* Let the ring fill before the clock starts, then take the first packet: it carries the sound that primes
     * the SCSP rings, and with it in the streams can start. */
    while (m->sectors_taken < kPrerollSectors && m->remaining != 0u) {
        SAT_TRY(sat_wait_vblank());
        poll_ring(m);
    }
    while (!decode_next(m, h.has_sound)) {
        SAT_TRY(sat_wait_vblank());
        poll_ring(m);
        if (m->remaining == 0u && available(m) < 8u) return SAT_ERR_IO;
    }
    SAT_TRY(sat_dma_copy(reinterpret_cast<void*>(kVramBase + y0 * SAT_MOVIE_PICTURE_STRIDE * 2u),
                         m->picture + y0 * SAT_MOVIE_PICTURE_STRIDE, h.height * SAT_MOVIE_PICTURE_STRIDE * 2u));
    for (unsigned c = 0u; h.has_sound && c < 2u; ++c) SAT_TRY(sat_audio_stream_resume(m->voice[c]));

    const uint32_t hz = sat_video_refresh_hz();
    const uint32_t latency = opt.audio_latency_fields != 0u ? opt.audio_latency_fields : kDefaultLatencyFields;
    const uint32_t t0 = sat_field_count() + latency;
    uint32_t shown_base = 0u, next = 1u, faded_in = 0u;
    m->stats.frames_shown = 1u;
    for (;;) {
        SAT_TRY(sat_wait_vblank());
        (void)sat_vdp2_layers_commit();                                    /* applies the flip decided last time */
        if (h.has_sound) (void)sat_audio_update();
        if (opt.skip_buttons != 0u) {
            SAT_TRY(sat_input_poll());
            if (((sat_pad_held_port(0) | sat_pad_held_port(1)) & opt.skip_buttons) != 0u) {
                *skipped = true;
                break;
            }
        }
        poll_ring(m);
        if (faded_in <= opt.fade_in_fields) {
            fade(opt.fade_in_fields == 0u ? 0 : -static_cast<int>(255u - faded_in * 255u / opt.fade_in_fields));
            ++faded_in;
        }

        const int32_t elapsed = static_cast<int32_t>(sat_field_count() - t0);
        const uint32_t due = elapsed <= 0 ? 0u
            : static_cast<uint32_t>(static_cast<uint64_t>(elapsed) * 1000000u / hz / h.frame_us);
        if (due >= h.frames) break;
        const uint64_t work_start = sat_time_ticks();
        uint32_t decoded = 0u;
        while (next <= due && decode_next(m, h.has_sound)) {
            ++next;
            ++decoded;
        }
        if (next <= due) ++m->stats.starved_fields;                       /* a frame was due and its data was not here */
        if (decoded == 0u) continue;
        m->stats.frames_shown += decoded;
        m->stats.frames_skipped += decoded - 1u;
        const uint32_t hidden = shown_base != 0u ? 0u : kBankBytes;
        SAT_TRY(sat_dma_copy(reinterpret_cast<void*>(kVramBase + hidden + y0 * SAT_MOVIE_PICTURE_STRIDE * 2u),
                             m->picture + y0 * SAT_MOVIE_PICTURE_STRIDE, h.height * SAT_MOVIE_PICTURE_STRIDE * 2u));
        SAT_TRY(sat_vdp2_layer_set_pattern_name_base(SAT_VDP2_NBG0, hidden));
        shown_base = hidden;
        const uint64_t hz_ticks = sat_time_frequency_hz();
        const uint32_t us = hz_ticks != 0u
            ? static_cast<uint32_t>((sat_time_ticks() - work_start) * 1000000u / hz_ticks) : 0u;
        if (us > m->stats.work_max_us) m->stats.work_max_us = us;
    }

    const uint32_t out = opt.fade_out_fields;
    for (uint32_t f = 0u; f <= out; ++f) {
        fade(out == 0u ? -255 : -static_cast<int>(f * 255u / out));
        (void)sat_audio_set_master_volume(
            static_cast<uint16_t>(out == 0u ? 0u : SAT_AUDIO_VOLUME_MAX - f * SAT_AUDIO_VOLUME_MAX / out));
        SAT_TRY(sat_wait_vblank());
        (void)sat_vdp2_layers_commit();
        if (h.has_sound) (void)sat_audio_update();
    }
    return SAT_OK;
}

}  // namespace

extern "C" sat_result_t sat_movie_play(sat_movie_player_t* player, sat_cdfs_volume_t* volume, sat_cd_block_t* block,
                                       const char* path, const sat_movie_play_options_t* options,
                                       uint8_t* out_skipped, sat_movie_stats_t* out_stats) {
    if (player == nullptr || volume == nullptr || block == nullptr || path == nullptr) return SAT_ERR_INVALID_ARG;
    SAT_TRY(saturn::core::require_initialized());
    if (sat_video_is_hires() != 0u) return SAT_ERR_UNSUPPORTED;
    const sat_movie_play_options_t defaults = {};
    const sat_movie_play_options_t& opt = options != nullptr ? *options : defaults;
    player->block = block;

    /* The sprite layer holds whatever the last program left in the VDP1 buffers: send one empty frame, and keep
     * the picture black until the first frame is in. */
    SAT_TRY(sat_vdp2_color_offset_enable(SAT_VDP2_LAYER_ALL, SAT_VDP2_COLOR_OFFSET_A));
    fade(-255);
    (void)sat_vdp2_layers_commit();
    if (sat_begin_frame() == SAT_OK) (void)sat_end_frame();

    bool stream = false, voices = false, skipped = false;
    const sat_result_t result = run(player, volume, path, opt, &stream, &voices, &skipped);

    if (stream) (void)sat_cd_stream_abort(block);
    for (unsigned c = 0u; voices && c < 2u; ++c) (void)sat_audio_stream_close(player->voice[c]);
    (void)sat_audio_set_master_volume(SAT_AUDIO_VOLUME_MAX);
    (void)sat_vdp2_layer_release(SAT_VDP2_NBG0);
    (void)sat_vdp2_layers_commit();
    fade(-255);
    if (out_skipped != nullptr) *out_skipped = skipped ? 1u : 0u;
    if (out_stats != nullptr) *out_stats = player->stats;
    return result;
}
