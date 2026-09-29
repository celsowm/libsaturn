#ifndef IKEMEN_AUDIO_H
#define IKEMEN_AUDIO_H

#include <stdint.h>

#include "saturn/audio.h"
#include "saturn/core.h"

#include "ikemen_fight.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ik_audio {
    sat_sound_t punch_whiff;
    sat_sound_t kick_whiff;
    sat_sound_t strong_punch_whiff;
    sat_sound_t punch_hit;
    sat_sound_t kick_hit;
    sat_sound_t strong_hit;
} ik_audio_t;

sat_result_t ik_audio_init(ik_audio_t* audio);
void ik_audio_process_fight(ik_audio_t* audio, const ik_fight_t* fight);
sat_result_t ik_audio_update(void);

#ifdef __cplusplus
}
#endif

#endif
