#ifndef SATURN_ADX_H
#define SATURN_ADX_H

#include <stdint.h>
#include "saturn/audio.h"
#include "saturn/disc_stream.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Compact CRI ADX encoding-3 disc player. This is a different API from
 * sat_music: linking only sat_adx_player_* does not link its ~288 KiB
 * static Work RAM Low rings. The caller owns the two 32 KiB PCM16 rings
 * (or just left for mono), and keeps the player/rings alive until close.
 *
 * Input is the LibSaturn ADX payload produced by convert_audio.py --format
 * adx: a four-byte LPC header followed by native 18-byte ADX frames.
 * Pass the generated coefficient macros (not raw full .adx containers).
 * SH-2 decodes the frames; SCSP plays PCM16, while the CD scheduler
 * streams encoded data. This is NOT Motorola 68000 decoding.
 *
 * The disc scheduler's primary music feed must be free. Do not mix this
 * player and sat_music on the same scheduler. No heap allocations.
 */
#define SAT_ADX_PCM_RING_FRAMES 16384u
#define SAT_ADX_PCM_RING_BYTES (SAT_ADX_PCM_RING_FRAMES * 2u)
#define SAT_ADX_STAGING_FRAMES 2048u
#define SAT_ADX_STAGING_BYTES (SAT_ADX_STAGING_FRAMES * 4u)

typedef struct sat_adx_pcm_rings {
    uint8_t left[SAT_ADX_PCM_RING_BYTES] __attribute__((aligned(4)));
    uint8_t right[SAT_ADX_PCM_RING_BYTES] __attribute__((aligned(4)));
} sat_adx_pcm_rings_t;

typedef struct sat_adx_config {
    uint32_t sample_rate;
    uint32_t sample_count;  /* original decoded frames, excludes ADX padding */
    int16_t coefficient_1;  /* Q12, from generated asset header */
    int16_t coefficient_2;  /* Q12, from generated asset header */
    uint8_t channels;      /* 1 or 2 */
    uint8_t reserved[3];
} sat_adx_config_t;

typedef struct sat_adx_pending {
    uint32_t bytes_left;
    uint32_t reserved_frames;
    uint32_t skip_bytes;
} sat_adx_pending_t;

/* Public only to allow static allocation. The fields are implementation
 * details; initialize the entire object to zero before its first open. */
typedef struct sat_adx_player {
    sat_disc_stream_t* disc;
    const sat_cdfs_file_t* file;
    sat_audio_stream_t left;
    sat_audio_stream_t right;
    uint32_t sample_rate;
    uint32_t sample_count;
    uint32_t payload_bytes;
    uint32_t read_offset;
    uint32_t read_skip;
    uint32_t position_frame;
    uint32_t decoded_frames;
    uint32_t delivery_calls;
    uint32_t max_decode_frames;
    uint32_t failed_deliveries;
    sat_adx_pending_t pending[2];
    int16_t coefficient[2];
    int16_t previous[2][2];
    uint8_t channels;
    uint8_t opened;
    uint8_t wanted;
    uint8_t starting;
    uint8_t pending_count;
    uint8_t carry_bytes;
    uint8_t failed;
    uint8_t reserved[2];
    uint8_t carry[36];
    uint8_t staging[SAT_ADX_STAGING_BYTES] __attribute__((aligned(4)));
} sat_adx_player_t;

typedef struct sat_adx_player_info {
    uint32_t buffered_frames;   /* minimum of both channels */
    uint32_t decoded_frames;    /* cumulative PCM output since open */
    uint32_t delivery_calls;
    uint32_t max_decode_frames; /* maximum decoded during one delivery */
    uint32_t underrun_count;
    uint32_t failed_deliveries;
    uint8_t playing;            /* wanted and both channels primed */
    uint8_t starting;
    uint8_t channels;
    uint8_t reserved;
} sat_adx_player_info_t;

/* Audio and CD scheduler must be initialized first. The caller has already
 * resolved the disc file (no synchronous file lookups during streaming).
 * Only one compact ADX player may own a scheduler's primary music feed.
 */
sat_result_t sat_adx_player_open(sat_adx_player_t* player,
                                sat_disc_stream_t* disc,
                                const sat_cdfs_file_t* file,
                                const sat_adx_config_t* config,
                                sat_adx_pcm_rings_t* rings);
sat_result_t sat_adx_player_play(sat_adx_player_t* player);
sat_result_t sat_adx_player_pause(sat_adx_player_t* player);
sat_result_t sat_adx_player_stop(sat_adx_player_t* player);
/* Call once per display frame, before sat_audio_update. This pumps the
 * shared CD scheduler (including its queued data jobs). */
sat_result_t sat_adx_player_update(sat_adx_player_t* player);
sat_result_t sat_adx_player_info(const sat_adx_player_t* player,
                                sat_adx_player_info_t* out_info);
sat_result_t sat_adx_player_close(sat_adx_player_t* player);

#ifdef __cplusplus
}
#endif
#endif
