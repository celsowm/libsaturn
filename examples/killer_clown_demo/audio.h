#pragma once
#include "saturn/core.h"
enum { CLOWN_SOUND_PUNCH, CLOWN_SOUND_KICK, CLOWN_SOUND_DAMAGE, CLOWN_SOUND_BELL, CLOWN_SOUND_COUNT };
void clown_audio_init(void);
void clown_audio_play(unsigned effect,sat_fx16_t x);
void clown_audio_update(void);
typedef struct {
    uint32_t effects[CLOWN_SOUND_COUNT], failures, running, executed, queued;
    uint32_t max_lateness, tick, active_voices, peak_voices;
} clown_audio_metrics_t;
extern volatile clown_audio_metrics_t g_clown_audio;
