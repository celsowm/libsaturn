#ifndef SATURN_MUSIC_H
#define SATURN_MUSIC_H
#include "saturn/audio.h"
#include "saturn/disc_stream.h"
#ifdef __cplusplus
extern "C" {
#endif
/* One music engine for PCM8, PCM16BE, IMA ADPCM and CRI ADX.
 * The registered SAT_ASSET_STREAM specifies the format.
 * Source and workspace are caller-owned; no hidden giant static buffers.
 * SH-2 decodes compressed formats to PCM for SCSP playback.
 */
#define SAT_MUSIC_CAPACITY 2u
#define SAT_MUSIC_MIN_RING_FRAMES 8192u
#define SAT_MUSIC_MAX_RING_FRAMES 32768u
#define SAT_MUSIC_DEFAULT_RING_FRAMES 16384u
#define SAT_MUSIC_SCRATCH_BYTES (2u * 8192u + 1032u)
#define SAT_MUSIC_WORKSPACE_BYTES(frames,channels) \
    (2u * (frames) * (channels) + SAT_MUSIC_SCRATCH_BYTES)
typedef struct sat_music { uint16_t slot; uint16_t generation; } sat_music_t;
typedef struct sat_music_desc {
    const char* asset;
    sat_disc_stream_t* disc; /* required for a track on the disc; NULL for RAM and other readers */
    void* workspace;        /* 4-byte-aligned, unique per open track */
    uint32_t workspace_bytes;
    uint32_t ring_frames;   /* 8192..32768, multiple of 4096 */
} sat_music_desc_t;
typedef struct sat_music_info {
    uint32_t sample_rate;
    uint32_t sample_count;
    uint8_t channels;
    uint8_t format;
    uint8_t looping;
    uint8_t playing;
    uint8_t disc_streamed;
    uint8_t starting;
    uint8_t standby;
    uint8_t prefetched;
} sat_music_info_t;
sat_result_t sat_music_open(sat_music_t* out, const sat_music_desc_t* desc);
sat_result_t sat_music_play(sat_music_t music);
sat_result_t sat_music_prefetch(sat_music_t music);
void sat_music_set_prefetch_while_playing(uint8_t enable);
sat_result_t sat_music_pause(sat_music_t music);
sat_result_t sat_music_resume(sat_music_t music);
sat_result_t sat_music_stop(sat_music_t music);
sat_result_t sat_music_seek(sat_music_t music, uint32_t frame);
sat_result_t sat_music_update(sat_music_t music);
sat_result_t sat_music_close(sat_music_t music);
sat_result_t sat_music_info(sat_music_t music, sat_music_info_t* out);
sat_result_t sat_music_stats(sat_music_t music, sat_audio_stream_stats_t* out);
uint8_t sat_music_is_playing(sat_music_t music);
uint16_t sat_music_capacity(void);
#ifdef __cplusplus
}
#endif
#endif
