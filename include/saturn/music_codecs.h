#ifndef SATURN_MUSIC_CODECS_H
#define SATURN_MUSIC_CODECS_H
/* Which music formats and sources a program carries (optional; most programs never include this).
 *
 * sat_music plays PCM, IMA ADPCM and ADX from the disc scheduler, and every format from RAM or an asset reader.
 * The library supplies the table that names that code (src/audio/music/codecs.cpp), so a program that links
 * sat_music gets all of it. A program that plays one thing, and is short of Work RAM, can define the table
 * itself in one source file. The linker then finds `sat_music_codecs` already defined and leaves the library's
 * table, and the code only it refers to, out of the program (the IMA tables alone are 2 KB):
 *
 *     #include "saturn/music_codecs.h"
 *     SAT_MUSIC_CODECS_ADX_FROM_DISC_ONLY;
 *
 * The program then plays ADX tracks that sat_music_open binds to the disc scheduler (desc->disc); any other
 * track (another format, or ADX from RAM or an asset reader) is SAT_ERR_UNSUPPORTED. The public API is the same. */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sat_music_codecs {
    const void* pcm_disc;   /* S8 / S16 tracks the disc scheduler delivers */
    const void* ima_disc;   /* IMA ADPCM tracks the disc scheduler delivers */
    const void* resident;   /* tracks read from RAM or an asset reader, every format */
} sat_music_codecs_t;

extern const sat_music_codecs_t sat_music_codecs;

#ifdef __cplusplus
}
#endif

#define SAT_MUSIC_CODECS_ADX_FROM_DISC_ONLY \
    const sat_music_codecs_t sat_music_codecs = {0, 0, 0}

#endif
