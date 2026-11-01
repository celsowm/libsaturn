#ifndef SATURN_MOVIE_H
#define SATURN_MOVIE_H

#include <stdint.h>

#include "saturn/audio.h"
#include "saturn/cd_block.h"
#include "saturn/cdfs.h"
#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Software video: a Cinepak picture decoded by the SH-2, no MPEG card.
 *
 * The frame layout is Cinepak's (the 10-byte frame header, strips, chunk ids 0x20-0x23 for codebooks and
 * 0x30-0x32 for vectors in the high byte of a 16-bit id). ffmpeg's `cinepak` encoder makes it, and ffmpeg
 * reads it too, so a movie can be checked against another decoder. A codebook entry (four luma samples and
 * a shared u and v, six bytes) is converted when it is loaded into a 2x2 patch of four VDP2 15-bit dots
 * (0x8000 | B << 10 | G << 5 | R, ffmpeg's colour rule), so the vectors that follow are copies. A V1 vector
 * doubles its patch to 4x4, a V4 vector is four patches.
 *
 * Limits: one strip per frame (encode with `-max_strips 1`), width and height multiples of 4, colour
 * entries only (no grey chunks). A frame that breaks them is SAT_ERR_UNSUPPORTED. A frame is an update of
 * the last one: blocks an inter frame skips keep their pixels, so the destination must be the same picture
 * from frame to frame. */

#define SAT_MOVIE_PATCH_BYTES 8u
#define SAT_MOVIE_CODEBOOK_ENTRIES 256u

typedef struct sat_movie_decoder {
    uint16_t width;
    uint16_t height;
    /* 2x2 patches: [entry][row0 pixel0, row0 pixel1, row1 pixel0, row1 pixel1]. */
    uint16_t v1[SAT_MOVIE_CODEBOOK_ENTRIES][4] __attribute__((aligned(4)));
    uint16_t v4[SAT_MOVIE_CODEBOOK_ENTRIES][4] __attribute__((aligned(4)));
    uint8_t clip5[1024];   /* private: a sum of -384 .. 639 as a 5-bit colour */
} sat_movie_decoder_t;

/* SAT_ERR_INVALID_ARG unless width and height are non-zero multiples of 4. Clears the codebooks. */
sat_result_t sat_movie_decoder_init(sat_movie_decoder_t* decoder, uint16_t width, uint16_t height);

/* Decodes one frame (`bytes` long) into `destination`, the picture's top-left dot, rows
 * `stride_pixels` apart (even, at least the width; `destination` is 4-byte aligned).
 * SAT_ERR_INVALID_ARG for a size that does not match the decoder, SAT_ERR_UNSUPPORTED for more than
 * one strip or a grey chunk, SAT_ERR_IO for a stream that runs past its end or names a block outside
 * the picture. A failed frame leaves the destination and the codebooks partly updated. */
sat_result_t sat_movie_decode_frame(sat_movie_decoder_t* decoder, const void* frame, uint32_t bytes,
                                    uint16_t* destination, uint32_t stride_pixels);


/* ------------------------------------------------------------------ */
/* Playing a movie file                                                */
/* ------------------------------------------------------------------ */

/* A movie file (tools/satmovie.py makes one from any video; its layout is documented there) holds Cinepak
 * pictures and IMA ADPCM sound, a packet per frame. sat_movie_play reads it from the disc once, start to end,
 * through one CD Block play into a ring, decodes each frame into a picture in Work RAM, copies the picture's
 * rows to the hidden one of two 15-bit VDP2 bitmaps (VRAM-A and VRAM-B) with the SCU, and flips NBG0 between
 * them in VBlank. The sound goes to two mono SCSP streams. A picture smaller than the screen is centred and the
 * rest stays black.
 *
 * The frame shown is the one the field count says is due, so a frame that comes late is skipped, never waited
 * for. The clock starts a few fields after the sound does (the SCSP's start-up delay, measured on Ymir). */

#define SAT_MOVIE_RING_SECTORS 64u
#define SAT_MOVIE_MAX_PACKET_BYTES (24u * 1024u)
#define SAT_MOVIE_AUDIO_RING_FRAMES 16384u
#define SAT_MOVIE_PICTURE_STRIDE 512u
#define SAT_MOVIE_PICTURE_ROWS 256u

/* What a play counted, for tuning a movie. */
typedef struct sat_movie_stats {
    uint32_t frames_shown;      /* frames decoded, including the ones skipped */
    uint32_t frames_skipped;    /* decoded but never shown: the next was due already */
    uint32_t starved_fields;    /* fields in which a frame was due and its data had not arrived */
    uint32_t ring_low_sectors;  /* fewest sectors the ring held before a poll while the disc still had data */
    uint32_t work_max_us;       /* longest decode, sound and upload in one field */
} sat_movie_stats_t;

typedef struct sat_movie_play_options {
    uint16_t skip_buttons;        /* SAT_PAD_* bits on pad 1 or 2 that end the movie at once; 0: it cannot be skipped */
    uint8_t fade_in_fields;       /* 0: the picture starts at once */
    uint8_t fade_out_fields;      /* the picture and the sound fade out at the end; 0: they stop at once */
    uint8_t audio_latency_fields; /* the clock starts this many fields after the sound; 0: 8 */
    uint8_t reserved[3];
} sat_movie_play_options_t;

/* All the memory a play needs, about 490 KiB. Static, and in Work RAM High: the SCU reads the picture from it.
 * Its contents between calls are private. */
typedef struct sat_movie_player {
    sat_movie_decoder_t decoder;
    uint16_t picture[SAT_MOVIE_PICTURE_STRIDE * SAT_MOVIE_PICTURE_ROWS] __attribute__((aligned(16)));
    uint8_t ring[SAT_MOVIE_RING_SECTORS * 2048u] __attribute__((aligned(4)));
    uint8_t packet[SAT_MOVIE_MAX_PACKET_BYTES] __attribute__((aligned(4)));
    uint8_t audio_ring[2][SAT_MOVIE_AUDIO_RING_FRAMES * 2u] __attribute__((aligned(4)));
    uint8_t pcm[2u * 2u * SAT_AUDIO_IMA_BLOCK_FRAMES] __attribute__((aligned(4)));
    sat_audio_stream_t voice[2];
    sat_cd_block_t* block;
    uint16_t* origin;
    uint32_t sectors_taken;
    uint32_t remaining;
    uint32_t used;
    sat_movie_stats_t stats;
} sat_movie_player_t;

/* Plays the movie at `path` on the volume `volume` reads from (`block` is the CD Block it is mounted on) and
 * returns when it ends or a skip button is pressed. `*out_skipped` (may be NULL) tells which; `*out_stats`
 * (may be NULL) counts what happened. `options` may be NULL.
 *
 * The caller has run sat_init at 320x224 or less (not a hi-res mode: SAT_ERR_UNSUPPORTED), and nothing else may
 * use the disc or the VDP2 NBG0 layer meanwhile. The VDP1 sprite layer is cleared and left empty. The audio
 * driver is started when it is not. On return the NBG0 layer is released and the screen is faded to black
 * through colour offset A (all layers): the caller calls sat_vdp2_color_offset_disable to show anything again,
 * or goes on to another movie or sat_exec. SAT_ERR_VERSION for a file that is not a movie, SAT_ERR_NOT_FOUND
 * for a missing one, SAT_ERR_CAPACITY for a picture or a packet that does not fit. */
sat_result_t sat_movie_play(sat_movie_player_t* player, sat_cdfs_volume_t* volume, sat_cd_block_t* block,
                            const char* path, const sat_movie_play_options_t* options,
                            uint8_t* out_skipped, sat_movie_stats_t* out_stats);

#ifdef __cplusplus
}
#endif

#endif
