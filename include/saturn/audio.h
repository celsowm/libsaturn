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

typedef enum sat_audio_format {
    SAT_AUDIO_PCM_S8 = 0,
    SAT_AUDIO_PCM_S16 = 1,
    /* Blocked 4-bit IMA ADPCM source for sat_music; decoded to S16 streams. */
    SAT_AUDIO_IMA_ADPCM = 2,
    /* CRI ADX encoding 3 payload: LPC header + native 18-byte frames,
     * decoded by sat_music into S16 streams on the SH-2. */
    SAT_AUDIO_ADX_ADPCM = 3
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

/* One block of blocked IMA ADPCM, the format sat_music reads from SAT_AUDIO_IMA_ADPCM assets and a movie
 * carries its sound in: SAT_AUDIO_IMA_BLOCK_FRAMES frames per block; per channel a 4-byte header (the first
 * sample as S16BE, the step index, a pad byte), then the other 1023 frames as 4-bit codes, high nibble first,
 * the channels alternating within a byte (mono: two frames to a byte). A block is
 * SAT_AUDIO_IMA_BLOCK_BYTES(channels) long (1032 for stereo), padded to 4 bytes. */
#define SAT_AUDIO_IMA_BLOCK_FRAMES 1024u
#define SAT_AUDIO_IMA_BLOCK_BYTES(channels) \
    ((4u * (channels) + ((SAT_AUDIO_IMA_BLOCK_FRAMES - 1u) * (channels) + 1u) / 2u + 3u) & ~3u)
/* Decodes one block to channel-planar S16BE, SAT_AUDIO_IMA_BLOCK_FRAMES * 2 bytes per channel, the left
 * channel first: ready for sat_audio_stream_write on one mono stream per channel. `channels` is 1 or 2.
 * SAT_ERR_INVALID_ARG for a null pointer, another channel count or a step index above 88. */
sat_result_t sat_audio_ima_decode_block(const void* block, uint8_t channels, void* out_planar_s16be);


/* LibSaturn ADX payload format for SAT_AUDIO_ADX_ADPCM assets:
 *   int16_be LPC coefficient[2] (4 bytes), followed by
 *   ceil(sample_count / 32) groups of 18 * channels bytes.
 * Each group holds one standard CRI ADX encoding-3 frame per channel;
 * the CRI file header and end marker are stripped by convert_audio.py.
 * Prediction carries across groups and is reset when the music loops.
 * sat_music_seek currently supports frame 0 only for ADX.
 * Decoder runs on the SH-2; this is not an SCSP 68000 offload.
 */
#define SAT_AUDIO_ADX_BLOCK_FRAMES 32u
#define SAT_AUDIO_ADX_BLOCK_BYTES(channels) (18u * (channels))
#define SAT_AUDIO_ADX_COEFFICIENT_BYTES 4u

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
/* Writes `bytes` (even) at the even Sound RAM `offset`, for a program that places samples itself (a bank swapped
 * in section by section while a 68000 score plays). The range must lie in the resident sample area, above the
 * driver's 0x3000 and below the streaming rings; the caller owns it, normally by having reserved it with
 * sat_sound_create. Nothing checks that no voice is still reading the range. SAT_ERR_INVALID_ARG outside it. */
sat_result_t sat_sound_ram_write(uint32_t offset, const void* data, uint32_t bytes);
/* Reserves `bytes` (even) of the resident sample area for a caller that places samples itself, without uploading or
 * creating a sound: the range counts in sound_ram_used and the high water mark, and no later sat_sound_create gets
 * it. Up to four reservations; SAT_ERR_CAPACITY when the pool cannot hold it. Released by its offset. */
sat_result_t sat_sound_ram_reserve(uint32_t bytes, uint32_t* out_offset);
sat_result_t sat_sound_ram_release(uint32_t offset);
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
