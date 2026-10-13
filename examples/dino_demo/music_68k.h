#ifndef DINO_DEMO_MUSIC_68K_H
#define DINO_DEMO_MUSIC_68K_H

#include "saturn/cdfs.h"
#include "saturn/core.h"
#include "saturn/loader.h"

/* Loads a small resident sample bank, then starts an autonomous 68000 score.
 * `progress` (may be NULL) is called after each sample file with the number
 * of files read and the total. */
sat_result_t dino_music_68k_start(sat_cdfs_volume_t* volume,
                                  sat_load_progress_fn progress, void* context);

#endif
