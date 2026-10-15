#ifndef SATURN_AUDIO_H
#define SATURN_AUDIO_H

#include <stdint.h>
#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SAT_AUDIO_VOLUME_MAX 255u
#define SAT_AUDIO_PAN_LEFT   (-15)
#define SAT_AUDIO_PAN_CENTER 0
#define SAT_AUDIO_PAN_RIGHT  15
#define SAT_MUSIC_CAPACITY 2u
#define SAT_MUSIC_BUFFER_FRAMES 32768u

typedef enum sat_audio_format {
    SAT_AUDIO_PCM_S8 = 0,
    SAT_AUDIO_PCM_S16 = 1,
    /* Blocked 4-bit IMA ADPCM source for sat_music; decoded to S16 streams. */
    SAT_AUDIO_IMA_ADPCM = 2
} sat_audio_format_t;

typedef struct sat_sound {
    uint16_t slot;
    uint16_t generation;
} sat_sound_t;

typedef struct sat_voice {
    uint16_t slot;
    uint16_t generation;
} sat_voice_t;

typedef struct sat_audio_spec {
    uint32_t sample_rate;
    uint32_t buffer_frames;
    uint8_t channels;
    uint8_t format;
    uint16_t reserved;
} sat_audio_spec_t;

typedef struct sat_audio_stream {
    uint16_t slot;
    uint16_t generation;
} sat_audio_stream_t;

typedef struct sat_music {
    uint16_t slot;
    uint16_t generation;
} sat_music_t;

typedef struct sat_music_info {
    uint32_t sample_rate;
    uint32_t sample_count;
    uint8_t channels;
    uint8_t format;
    uint8_t looping;
    /* 1 from sat_music_play until sat_music_pause / stop / seek: the track is wanted. It is audible
     * once `starting` is 0. */
    uint8_t playing;
    /* 1: the track streams through the disc scheduler (sat_music_use_disc_stream), so
     * sat_music_update never waits for the drive. */
    uint8_t disc_streamed;
    /* 1: sat_music_play was called and the rings are still filling; the streams start, together,
     * in the sat_music_update (or sat_disc_stream_service) that finds them full. */
    uint8_t starting;
    /* 1: a disc track waiting behind the one the drive plays (the second one opened). It fills its
     * rings only after sat_music_prefetch; sat_music_play on it swaps the two. */
    uint8_t standby;
    /* 1: sat_music_prefetch was called, the track is not playing and its rings hold enough (16384
     * frames for a disc track, full for any other), so sat_music_play starts it at once. */
    uint8_t prefetched;
} sat_music_info_t;

typedef struct sat_audio_stream_stats {
    uint32_t buffered_frames;
    uint32_t capacity_frames;
    uint32_t underrun_count;
    uint32_t rejected_write_count;
    uint32_t minimum_fill;
    uint32_t maximum_fill;
    uint32_t consumed_frames;
    uint32_t refill_count;
    uint8_t paused;
    uint8_t playing;
    uint8_t reserved0;
    uint16_t reserved1;
} sat_audio_stream_stats_t;

typedef struct sat_sound_desc {
    const void* samples;
    uint32_t sample_count;
    uint32_t sample_rate;
    uint16_t loop_start;
    uint16_t loop_end;
    uint8_t format;
    uint8_t loop;
    uint8_t reserved0;
    uint8_t reserved1;
} sat_sound_desc_t;

typedef struct sat_sound_play_params {
    uint16_t volume;
    int16_t pan;
    uint16_t priority;
    uint16_t flags;
    sat_fx16_t pitch;
    /* With SAT_SOUND_PLAY_AT_TICK: the sound driver tick (saturn/sound_driver.h)
     * the voice is keyed on at. The voice is configured now and started by the
     * 68000 when the tick comes. If the driver is not running the sound starts
     * at once. */
    uint32_t start_tick;
} sat_sound_play_params_t;

#define SAT_SOUND_PLAY_AT_TICK ((uint16_t)0x0001u)

typedef struct sat_audio_stats {
    uint32_t sound_ram_used;
    uint32_t sound_ram_capacity;
    uint32_t sound_ram_high_water;
    uint32_t voice_steals;
    uint32_t failed_play_requests;
    uint16_t resident_sounds;
    uint16_t resident_sound_capacity;
    uint16_t active_voices;
    uint16_t voice_capacity;
} sat_audio_stats_t;

sat_result_t sat_audio_init(void);
sat_result_t sat_audio_shutdown(void);
sat_result_t sat_audio_update(void);
uint8_t sat_audio_is_initialized(void);

/* A music stream plays from a two-half Sound RAM loop (4096 samples per channel per half) that
 * sat_audio_update keeps filled. Writing a half in one call would cost about 12 ms for a stereo pair
 * on Mednafen (SH-2 word/longword writes to Sound RAM), so the driver never does: when the SCSP moves
 * into one half, the other is rewritten a slice per sat_audio_update over the display frames the half
 * lasts (186 ms at 22.05 kHz; three frames are kept spare, and a half shorter than four frames is
 * written in one call), and the two opening halves are written the same way, 1024 frames per call,
 * before the voice is keyed on (about eight updates). A ring is consumed only when its half is whole,
 * so buffered/available frames lag by up to one half meanwhile. A stream therefore starts a few
 * display frames after sat_audio_stream_resume, not inside the call that follows it. */

sat_result_t sat_audio_set_master_volume(uint16_t volume);
sat_result_t sat_audio_get_stats(sat_audio_stats_t* out_stats);

sat_result_t sat_sound_create(sat_sound_t* out_sound, const sat_sound_desc_t* desc);
sat_result_t sat_sound_unload(sat_sound_t sound);
sat_result_t sat_sound_play(sat_sound_t sound, const sat_sound_play_params_t* params, sat_voice_t* out_voice);
sat_result_t sat_sound_stop_all_instances(sat_sound_t sound);

sat_result_t sat_voice_stop(sat_voice_t voice);
sat_result_t sat_voice_set_volume(sat_voice_t voice, uint16_t volume);
sat_result_t sat_voice_set_pan(sat_voice_t voice, int16_t pan);
uint8_t sat_voice_is_playing(sat_voice_t voice);

/* Generic caller-backed PCM producer/consumer stream. The first runtime
 * implementation accepts mono PCM_S8/PCM_S16. `buffer` remains owned by the
 * caller and must stay valid until close. No audio API allocates a fallback
 * buffer. A write is all-or-nothing when the ring lacks free frames. */
sat_result_t sat_audio_stream_open(
    sat_audio_stream_t* out_stream,
    const sat_audio_spec_t* spec,
    void* buffer,
    uint32_t buffer_bytes
);
sat_result_t sat_audio_stream_write(
    sat_audio_stream_t stream,
    const void* frames,
    uint32_t frame_count
);
uint32_t sat_audio_stream_available(sat_audio_stream_t stream);
uint32_t sat_audio_stream_buffered(sat_audio_stream_t stream);
sat_result_t sat_audio_stream_pause(sat_audio_stream_t stream);
sat_result_t sat_audio_stream_resume(sat_audio_stream_t stream);
sat_result_t sat_audio_stream_flush(sat_audio_stream_t stream);
/* Stereo placement of a mono stream. `pan` spans SAT_AUDIO_PAN_LEFT (-15)
 * through SAT_AUDIO_PAN_RIGHT (15); streams open centered. Used by the stereo
 * music runtime to place its left/right channel streams. */
sat_result_t sat_audio_stream_set_pan(sat_audio_stream_t stream, int16_t pan);
sat_result_t sat_audio_stream_close(sat_audio_stream_t stream);
sat_result_t sat_audio_stream_stats(
    sat_audio_stream_t stream,
    sat_audio_stream_stats_t* out_stats
);

/* Preconverted PCM or blocked IMA ADPCM from the logical asset registry.
 * ADPCM blocks contain 1024 frames; the runtime expands them to S16BE before
 * writing the SCSP stream rings. OGG/MP3 decoding remains a build step.
 * Music loops the complete asset and uses a fixed internal pool of two
 * streams; use sat_music_capacity() to size content budgets. */
sat_result_t sat_music_open(sat_music_t* out_music, const char* logical_path);
/* Never waits: it marks the track wanted and returns. The rings fill over the following
 * sat_music_update calls (a RAM track in a few, a disc track at the drive's pace through the
 * scheduler: 87 display frames for the jukebox's 1.5 s of 22.05 kHz stereo S16 on Mednafen) and the two channel
 * streams start together once they are full; until then sat_music_info reports `starting`.
 * A program that calls play and never updates hears nothing. Also resumes a paused track. */
sat_result_t sat_music_play(sat_music_t music);
/* Fills the track's rings now, silent, so a later sat_music_play starts it at once instead of after
 * the refill. A disc track is read by the scheduler over the following sat_music_update calls, in
 * bursts while the playing track's rings are nearly full (saturn/disc_stream.h, standby_high /
 * standby_floor): sat_music_info reports `prefetched` once the rings hold 16384 frames, and the rest
 * fills at normal priority. A drive with little spare throughput needs time for that: prefetch as
 * early as the game knows what comes next (on Mednafen, which leaves a few percent beyond a 22.05
 * kHz stereo S16 track, tens of seconds). Cancelled by stop / seek (call it again). Returns at once
 * and reads nothing.
 *
 * Two disc tracks can be open (sat_music_use_disc_stream): the drive plays the first opened, the
 * second waits behind it as the standby. sat_music_play on the standby is a gapless handoff when it
 * was prefetched: the playing track stops and goes back to its start (it is then the standby), and
 * the new one is audible in that call; the old track's read in flight is not waited for. Without a
 * prefetch (or while it is still short of 16384 frames) it is the same switch with the refill
 * before the new track is heard. The two share the library's two music slots.
 *
 * Real-drive caution: a prefetch moves the laser to the standby's file and back, and costs the
 * playing track up to about (standby_high - standby_floor) frames plus two segments of margin. The
 * manual's average seek is 500 ms at 2x, close to what the playing track's ring holds, so prefetch
 * while the playing track is paused or fading, or at a moment where a glitch is acceptable. The
 * emulators treat seeks as free, so they cannot show this. */
sat_result_t sat_music_prefetch(sat_music_t music);
sat_result_t sat_music_pause(sat_music_t music);
sat_result_t sat_music_resume(sat_music_t music);
sat_result_t sat_music_stop(sat_music_t music);
/* Moves playback to `frame` (0..sample_count-1). A playing track restarts
 * there after re-priming both channels; a paused or stopped one starts there
 * on the next sat_music_play. The buffered audio is dropped either way. */
sat_result_t sat_music_seek(sat_music_t music, uint32_t frame);
/* Moves the audio forward. A RAM-resident track copies into the rings; a track read from the
 * disc through the legacy path waits for the drive when it needs more. A disc-scheduled track
 * (sat_music_use_disc_stream) lands the segment that has arrived and starts the next, never
 * waiting; it also advances the scheduler's data jobs. */
sat_result_t sat_music_update(sat_music_t music);
sat_result_t sat_music_close(sat_music_t music);
sat_result_t sat_music_info(sat_music_t music, sat_music_info_t* out_info);
sat_result_t sat_music_stats(sat_music_t music, sat_audio_stream_stats_t* out_stats);
uint8_t sat_music_is_playing(sat_music_t music);
uint16_t sat_music_capacity(void);

/* ---- Procedural placeholder audio ----------------------------------------
 * Real games ship recorded samples. Examples, prototypes and tests need
 * something audible without an asset pipeline, and every one of them ends up
 * hand-rolling the same decaying blip -- which is where the off-by-one
 * envelope and clipping bugs live. All of these write signed 8-bit PCM into
 * caller-owned storage and allocate nothing. Output is fully deterministic,
 * so emulator captures stay reproducible. */

/* Descending-pitch sawtooth blip that decays to silence: jumps, pickups, UI
 * clicks. `step` sets the pitch; larger is higher. */
sat_result_t sat_audio_synth_blip(int8_t* out, uint32_t count, uint8_t step);

/* Same envelope over a deterministic hash instead of a tone: impacts, falls. */
sat_result_t sat_audio_synth_noise(int8_t* out, uint32_t count);

/* One note of a triangle-wave melody over a steady bass, with a short attack
 * and release so consecutive notes do not click. `period` is the wavelength
 * in samples (smaller is higher). `bass_phase` is the sample index this note
 * starts at, so the bass line continues across notes; writing a melody is a
 * loop over periods, which also lets a caller show progress between notes.
 * Requires count >= 322 (two 160-sample edges). */
sat_result_t sat_audio_synth_arpeggio_note(
    int8_t* out, uint32_t count, uint8_t period, uint32_t bass_phase);

#ifdef __cplusplus
}
#endif

#endif
