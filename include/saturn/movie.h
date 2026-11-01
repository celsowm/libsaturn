#ifndef SATURN_MOVIE_H
#define SATURN_MOVIE_H

#include <stdint.h>

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

#ifdef __cplusplus
}
#endif

#endif
