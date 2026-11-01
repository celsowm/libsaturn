#ifndef SATURN_CDDA_H
#define SATURN_CDDA_H

#include <stdint.h>

#include "saturn/cd_block.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CD audio: the disc's table of contents, playback by FAD and the SCSP return
 * of the CD's stereo signal. All data reads must finish before playback starts:
 * a read moves the drive's head away from the music. */

enum { SAT_CD_TOC_TRACKS = 99 };

typedef struct {
    uint32_t tracks[SAT_CD_TOC_TRACKS]; /* control/ADR << 24 | start FAD */
    uint8_t first_track;
    uint8_t last_track;
    uint32_t lead_out_fad;
} sat_cd_toc_t;

/* What the CD Block's Get Status reports. */
typedef struct {
    uint8_t state;   /* SAT_CD_STATE_* */
    uint8_t control; /* control/ADR of the current track */
    uint8_t track;
    uint8_t index;
    uint32_t fad;
} sat_cd_status_t;

enum {
    SAT_CD_STATE_BUSY = 0,
    SAT_CD_STATE_PAUSE = 1,
    SAT_CD_STATE_STANDBY = 2,
    SAT_CD_STATE_PLAY = 3,
    SAT_CD_STATE_SEEK = 4
};

/* Maximum level for sat_cdda_set_level: 7 is 0 dB, each step down is -6 dB. */
enum { SAT_CDDA_LEVEL_MAX = 7 };

/* Reads the table of contents. Waits for at most 120 fields with
 * sat_wait_vblank, so it needs sat_init. SAT_ERR_IO when the CD Block refuses,
 * SAT_ERR_TIMEOUT when the data never arrives. */
sat_result_t sat_cd_read_toc(sat_cd_block_t* block, sat_cd_toc_t* out_toc);

/* One audio track's first FAD and length in sectors. It ends where the next
 * track starts, or at the lead-out for the last. SAT_ERR_INVALID_ARG when the
 * track is outside the table, holds data, or is empty. */
sat_result_t sat_cd_toc_audio_track(const sat_cd_toc_t* toc, uint8_t track,
                                    uint32_t* out_start_fad, uint32_t* out_length);

/* Plays [start_fad, start_fad + length) on repeat, then sets nothing else: call
 * sat_cdda_set_level for the sound to be heard. SAT_ERR_INVALID_ARG for a
 * position that does not fit 24 bits or an empty range. */
sat_result_t sat_cdda_play(sat_cd_block_t* block, uint32_t start_fad, uint32_t length);

/* Sends Get Status, a real command: poll it rarely while music plays. */
sat_result_t sat_cd_get_status(sat_cd_block_t* block, sat_cd_status_t* out_status);

/* Reads the CD Block's periodic response without sending a command, so it may
 * run every field while music plays. SAT_ERR_IO when it reports a fault. */
sat_result_t sat_cd_check_periodic(void);

/* Sets the level of the CD's left and right signal in the SCSP output, keeping
 * the slots' direct-send bytes. A sound voice can overwrite these slots, so call
 * it again after sound updates that touch slots 16 and 17. */
void sat_cdda_set_level(uint8_t level);

#ifdef __cplusplus
}
#endif

#endif
