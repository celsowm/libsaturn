#ifndef DINO_DEMO_MUSIC_CDDA_H
#define DINO_DEMO_MUSIC_CDDA_H

#include "saturn/cd_block.h"

/* The mixed-mode disc holds the complete reference soundtrack on track 2. */
sat_result_t dino_music_cdda_start(sat_cd_block_t* block);
sat_result_t dino_music_cdda_status(void);

#endif
