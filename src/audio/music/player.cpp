#include "src/audio/music/internal.hpp"
#include <stddef.h>

namespace saturn::core::music {

/* Metadata only: large buffers are allocated by the application. */
MusicSlot g_music[SAT_MUSIC_CAPACITY] = {};

/* The disc scheduler tracks stream through, and the slots attached to it: the one the drive plays
 * (primary) and one that fills its rings behind it (standby, sat_music_prefetch). A play on the
 * standby swaps the two. */
sat_disc_stream_t* g_disc = nullptr;
MusicSlot* g_disc_primary = nullptr;
MusicSlot* g_disc_standby = nullptr;
/* sat_music_set_prefetch_while_playing: off, so the standby is only read while the playing track is
 * paused or stopped (nothing audible then waits for a seek). */
uint8_t g_prefetch_while_playing = 0u;

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

} // namespace saturn::core::music

using namespace saturn::core::music;

namespace {

sat_result_t rewind_to(MusicSlot& slot, uint32_t frame);

/* The standby track is about to play: the track the drive was playing stops and goes back to the
 * start (it becomes the standby), and the scheduler serves the new one. */
sat_result_t hand_off(MusicSlot& incoming) {
    MusicSlot* outgoing = g_disc_primary;
    // The swap first: it lets the outgoing track's read finish on its own instead of waiting for it.
    SAT_TRY(sat_disc_stream_music_swap(g_disc));
    g_disc_primary = &incoming;
    g_disc_standby = outgoing;
    return outgoing != nullptr ? rewind_to(*outgoing, 0u) : SAT_OK;
}

}  // namespace

extern "C" sat_result_t sat_music_play(sat_music_t music) {
    MusicSlot* slot = resolve(music);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    if (slot->playing != 0u) return SAT_OK;
    const bool ready = prefetch_ready(*slot);
    if (slot->file != nullptr && g_disc_standby == slot) SAT_TRY(hand_off(*slot));
    // Nothing is read here. Both streams stay paused while sat_music_update fills the rings (the
    // scheduler's service for a disc track), and start together once they are full: at once for a
    // track whose rings are already full (sat_music_prefetch, or a pause).
    slot->prefetch = 0u;
    slot->playing = 1u;
    slot->starting = 1u;
    if (slot->file != nullptr) {
        sat_disc_stream_music_set_starting(g_disc, 1u);
        if (ready || async_primed(*slot)) start_streams(*slot);
    } else if (ready || writable_frames(*slot) == 0u) {
        start_streams(*slot);
    }
    return SAT_OK;
}

extern "C" sat_result_t sat_music_prefetch(sat_music_t music) {
    MusicSlot* slot = resolve(music);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    if (slot->playing == 0u) slot->prefetch = 1u;
    return SAT_OK;
}

extern "C" sat_result_t sat_music_pause(sat_music_t music) {
    MusicSlot* slot = resolve(music);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    SAT_TRY(sat_audio_stream_pause(slot->stream));
    if (slot->channels == 2u) SAT_TRY(sat_audio_stream_pause(slot->stream_r));
    slot->playing = 0u;
    slot->starting = 0u;
    if (slot->file != nullptr && g_disc_primary == slot) sat_disc_stream_music_set_starting(g_disc, 0u);
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
    if (slot.file != nullptr) {
        if (g_disc_primary == &slot) sat_disc_stream_music_abort(g_disc);
        else sat_disc_stream_music_abort_standby(g_disc);
    }
    SAT_TRY(sat_audio_stream_pause(slot.stream));
    SAT_TRY(sat_audio_stream_flush(slot.stream));
    if (slot.channels == 2u) {
        SAT_TRY(sat_audio_stream_pause(slot.stream_r));
        SAT_TRY(sat_audio_stream_flush(slot.stream_r));
    }
    if (slot.file != nullptr) {
        async_locate(slot, frame);
    } else if (slot.format == SAT_AUDIO_IMA_ADPCM ||
               slot.format == SAT_AUDIO_ADX_ADPCM) {
        slot.position_frame = frame;
        slot.decoded_block = 0xFFFFFFFFu;
        if (slot.format == SAT_AUDIO_ADX_ADPCM)
            saturn::core::audio::adx_adpcm::reset_history(slot.adx_state);
    } else {
        slot.position_bytes = frame * bytes_per_sample(slot.format) * slot.channels;
    }
    slot.playing = 0u;
    slot.starting = 0u;
    slot.prefetch = 0u;
    if (slot.file != nullptr && g_disc_primary == &slot) sat_disc_stream_music_set_starting(g_disc, 0u);
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
    // ADX prediction carries across blocks. Nonzero random access needs a
    // decoded history checkpoint; do not silently play wrong samples.
    if (slot->format == SAT_AUDIO_ADX_ADPCM && frame != 0u)
        return SAT_ERR_UNSUPPORTED;
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
    if (slot->file != nullptr) {
        sat_disc_stream_music_standby_gate(
            g_disc, g_prefetch_while_playing != 0u || g_disc_primary == nullptr || g_disc_primary->playing == 0u ? 1u : 0u);
        return sat_disc_stream_service(g_disc);
    }
    for (uint32_t i = 0u; i < kMusicUpdateFeedLimit; ++i) {
        if ((slot->playing == 0u && slot->prefetch == 0u) || writable_frames(*slot) == 0u) break;
        SAT_TRY(feed(*slot));
    }
    if (slot->starting != 0u && writable_frames(*slot) == 0u) start_streams(*slot);
    return SAT_OK;
}

extern "C" void sat_music_set_prefetch_while_playing(uint8_t enable) {
    g_prefetch_while_playing = enable != 0u ? 1u : 0u;
}



extern "C" sat_result_t sat_music_close(sat_music_t music) {
    MusicSlot* slot = resolve(music);
    if (slot == nullptr) return SAT_ERR_INVALID_ARG;
    if (slot->file != nullptr) {
        if (g_disc_primary == slot) {
            sat_disc_stream_music_detach(g_disc);
            g_disc_primary = nullptr;
        } else {
            sat_disc_stream_music_detach_standby(g_disc);
            g_disc_standby = nullptr;
        }
        slot->file = nullptr;
    }
    SAT_TRY(sat_audio_stream_close(slot->stream));
    if (slot->channels == 2u) SAT_TRY(sat_audio_stream_close(slot->stream_r));
    slot->used = 0u;
    slot->generation = next_generation(slot->generation);
    slot->data = nullptr;
    slot->workspace = nullptr;
    slot->workspace_bytes = 0u;
    slot->asset = {};
    slot->logical_path[0] = '\0';
    slot->payload_bytes = 0u;
    slot->playing = 0u;
    slot->starting = 0u;
    slot->prefetch = 0u;
    slot->position_frame = 0u;
    slot->decoded_block = 0xFFFFFFFFu;
    if (g_disc_primary == nullptr && g_disc_standby == nullptr) g_disc = nullptr;
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
    out_info->standby = slot->file != nullptr && g_disc_standby == slot ? 1u : 0u;
    // The rings of a track that was prefetched (not yet playing) hold all they will take.
    out_info->prefetched = prefetch_ready(*slot) ? 1u : 0u;
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
    g_prefetch_while_playing = 0u;
    if (g_disc_primary != nullptr) sat_disc_stream_music_detach(g_disc);
    if (g_disc_standby != nullptr) sat_disc_stream_music_detach_standby(g_disc);
    g_disc_primary = nullptr;
    g_disc_standby = nullptr;
    g_disc = nullptr;
    for (uint16_t i = 0u; i < SAT_MUSIC_CAPACITY; ++i) {
        MusicSlot& slot = g_music[i];
        slot.used = 0u;
        slot.generation = next_generation(slot.generation);
        slot.data = nullptr;
        slot.workspace = nullptr;
        slot.workspace_bytes = 0u;
        slot.file = nullptr;
        slot.asset = {};
        slot.logical_path[0] = '\0';
        slot.payload_bytes = 0u;
        slot.playing = 0u;
        slot.starting = 0u;
        slot.prefetch = 0u;
        slot.position_bytes = 0u;
        slot.position_frame = 0u;
        slot.decoded_block = 0xFFFFFFFFu;
    }
}

}  // namespace saturn::core
