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
#define SAT_MUSIC_BUFFER_FRAMES 8192u

typedef enum sat_audio_format {
    SAT_AUDIO_PCM_S8 = 0,
    SAT_AUDIO_PCM_S16 = 1
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
    uint8_t playing;
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
} sat_sound_play_params_t;

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

/* Preconverted streamable PCM from the logical asset registry. Runtime
 * decoding of OGG/MP3 is intentionally out of scope. The initial music
 * runtime loops the complete asset and uses a fixed internal pool of two
 * streams; use sat_music_capacity() to size content budgets. */
sat_result_t sat_music_open(sat_music_t* out_music, const char* logical_path);
sat_result_t sat_music_play(sat_music_t music);
sat_result_t sat_music_pause(sat_music_t music);
sat_result_t sat_music_resume(sat_music_t music);
sat_result_t sat_music_stop(sat_music_t music);
sat_result_t sat_music_update(sat_music_t music);
sat_result_t sat_music_close(sat_music_t music);
sat_result_t sat_music_info(sat_music_t music, sat_music_info_t* out_info);
sat_result_t sat_music_stats(sat_music_t music, sat_audio_stream_stats_t* out_stats);
uint8_t sat_music_is_playing(sat_music_t music);
uint16_t sat_music_capacity(void);

#ifdef __cplusplus
}
#endif

#endif
