#ifndef DINO_DEMO_MUSIC_68K_H
#define DINO_DEMO_MUSIC_68K_H

#include "saturn/cdfs.h"
#include "saturn/core.h"

/* Loads a small resident sample bank, then starts an autonomous 68000 score. */
sat_result_t dino_music_68k_start(sat_cdfs_volume_t* volume);

#endif
