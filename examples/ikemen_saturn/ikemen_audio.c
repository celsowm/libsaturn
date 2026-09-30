#include "ikemen_audio.h"

#include "ikemen_saturn/kfm_sounds.h"

static sat_result_t create_pcm_sound(
    sat_sound_t* out_sound,
    const ik_snd_pcm_t* asset
) {
    if (out_sound == 0 || asset == 0 || asset->samples == 0 ||
        asset->sample_count == 0u || asset->sample_rate == 0u) {
        return SAT_ERR_INVALID_ARG;
    }
    sat_sound_desc_t desc = {0};
    desc.samples = asset->samples;
    desc.sample_count = asset->sample_count;
    desc.sample_rate = asset->sample_rate;
    desc.format = SAT_AUDIO_PCM_S8;
    return sat_sound_create(out_sound, &desc);
}

static void play_attack_sound(const ik_audio_t* audio, int16_t state, int hit) {
    if (audio == 0) return;
    sat_sound_t sound = {0u, 0u};

    if (hit) {
        switch (state) {
            case IK_STATE_PUNCH:
            case IK_STATE_CROUCH_PUNCH:
            case IK_STATE_JUMP_PUNCH:
            case IK_STATE_JUMP_KICK:
                sound = audio->punch_hit;
                break;
            case IK_STATE_KICK:
            case IK_STATE_CROUCH_KICK:
                sound = audio->kick_hit;
                break;
            case IK_STATE_STRONG_PUNCH:
            case IK_STATE_STRONG_KICK:
            case IK_STATE_CROUCH_STRONG_PUNCH:
            case IK_STATE_CROUCH_STRONG_KICK:
                sound = audio->strong_hit;
                break;
            case IK_STATE_JUMP_STRONG_PUNCH:
            case IK_STATE_JUMP_STRONG_KICK:
                sound = audio->air_strong_hit;
                break;
            default:
                return;
        }
    } else {
        switch (state) {
            case IK_STATE_PUNCH:
            case IK_STATE_CROUCH_PUNCH:
            case IK_STATE_CROUCH_STRONG_PUNCH:
            case IK_STATE_CROUCH_KICK:
            case IK_STATE_JUMP_PUNCH:
            case IK_STATE_JUMP_KICK:
                sound = audio->punch_whiff;
                break;
            case IK_STATE_KICK:
            case IK_STATE_STRONG_KICK:
            case IK_STATE_JUMP_STRONG_PUNCH:
            case IK_STATE_JUMP_STRONG_KICK:
                sound = audio->kick_whiff;
                break;
            case IK_STATE_STRONG_PUNCH:
                sound = audio->strong_punch_whiff;
                break;
            case IK_STATE_CROUCH_STRONG_KICK:
                sound = audio->sweep_whiff;
                break;
            default:
                return;
        }
    }

    (void)sat_sound_play(sound, 0, 0);
}

sat_result_t ik_audio_init(ik_audio_t* audio) {
    if (audio == 0) return SAT_ERR_INVALID_ARG;
    *audio = (ik_audio_t){0};

    sat_result_t st = sat_audio_init();
    if (st != SAT_OK) return st;

    st = create_pcm_sound(&audio->punch_whiff, &kfm_sounds_punch_whiff);
    if (st != SAT_OK) return st;
    st = create_pcm_sound(&audio->kick_whiff, &kfm_sounds_kick_whiff);
    if (st != SAT_OK) return st;
    st = create_pcm_sound(
        &audio->strong_punch_whiff, &kfm_sounds_strong_punch_whiff);
    if (st != SAT_OK) return st;
    st = create_pcm_sound(&audio->sweep_whiff, &kfm_sounds_sweep_whiff);
    if (st != SAT_OK) return st;
    st = create_pcm_sound(&audio->special_whiff, &kfm_sounds_special_whiff);
    if (st != SAT_OK) return st;
    st = create_pcm_sound(&audio->throw_voice, &kfm_sounds_throw_voice);
    if (st != SAT_OK) return st;
    st = create_pcm_sound(&audio->throw_slam, &kfm_sounds_throw_slam);
    if (st != SAT_OK) return st;
    st = create_pcm_sound(&audio->landing, &kfm_sounds_landing);
    if (st != SAT_OK) return st;
    st = create_pcm_sound(&audio->knee_voice, &kfm_sounds_knee_voice);
    if (st != SAT_OK) return st;
    st = create_pcm_sound(&audio->super_pause, &kfm_sounds_super_pause);
    if (st != SAT_OK) return st;
    st = create_pcm_sound(&audio->punch_hit, &kfm_sounds_punch_hit);
    if (st != SAT_OK) return st;
    st = create_pcm_sound(&audio->kick_hit, &kfm_sounds_kick_hit);
    if (st != SAT_OK) return st;
    st = create_pcm_sound(&audio->strong_hit, &kfm_sounds_strong_hit);
    if (st != SAT_OK) return st;
    st = create_pcm_sound(
        &audio->air_strong_hit, &kfm_sounds_air_strong_hit);
    if (st != SAT_OK) return st;
    st = create_pcm_sound(&audio->heavy_hit, &kfm_sounds_heavy_hit);
    if (st != SAT_OK) return st;
    return create_pcm_sound(
        &audio->reversal_hit, &kfm_sounds_reversal_hit);
}

void ik_audio_process_fight(ik_audio_t* audio, const ik_fight_t* fight) {
    if (audio == 0 || fight == 0) return;

    for (uint8_t i = 0u; i < fight->sound_count; ++i) {
        const ik_sound_event_t* event = &fight->sound_events[i];
        sat_sound_t sound = {0u, 0u};
        if (event->group == 0 && event->item == 0) {
            sound = audio->punch_whiff;
        } else if (event->group == 0 && event->item == 1) {
            sound = audio->kick_whiff;
        } else if (event->group == 0 && event->item == 2) {
            sound = audio->sweep_whiff;
        } else if (event->group == 0 && event->item == 3) {
            sound = audio->special_whiff;
        } else if (event->group == 0 && event->item == 4) {
            sound = audio->strong_punch_whiff;
        } else if (event->group == 1 && event->item == 1) {
            sound = audio->throw_voice;
        } else if (event->group == 800 && event->item == 0) {
            sound = audio->throw_slam;
        } else if (event->group == 40 && event->item == 0) {
            sound = audio->landing;
        } else if (event->group == 100 && event->item == 0) {
            sound = audio->knee_voice;
        } else if (event->group == 20 && event->item == 0) {
            sound = audio->super_pause;
        } else if (event->group == 5 && event->item == 0) {
            sound = audio->punch_hit;
        } else if (event->group == 5 && event->item == 1) {
            sound = audio->kick_hit;
        } else if (event->group == 5 && event->item == 2) {
            sound = audio->strong_hit;
        } else if (event->group == 5 && event->item == 3) {
            sound = audio->air_strong_hit;
        } else if (event->group == 5 && event->item == 4) {
            sound = audio->heavy_hit;
        } else if (event->group == 6 && event->item == 0) {
            sound = audio->reversal_hit;
        }
        if (sound.id != 0u || sound.generation != 0u) {
            (void)sat_sound_play(sound, 0, 0);
        }
    }

    for (int i = 0; i < 2; ++i) {
        const ik_fighter_t* fighter = &fight->fighters[i];
        const int16_t state = fighter->state;
        const uint16_t time = fighter->state_time;

        if ((state == IK_STATE_PUNCH ||
             state == IK_STATE_CROUCH_PUNCH ||
             state == IK_STATE_CROUCH_STRONG_PUNCH ||
             state == IK_STATE_CROUCH_KICK ||
             state == IK_STATE_JUMP_PUNCH ||
             state == IK_STATE_JUMP_KICK) && time == 1u) {
            play_attack_sound(audio, state, 0);
        } else if ((state == IK_STATE_STRONG_PUNCH ||
                    state == IK_STATE_KICK ||
                    state == IK_STATE_STRONG_KICK ||
                    state == IK_STATE_CROUCH_STRONG_KICK ||
                    state == IK_STATE_JUMP_STRONG_PUNCH ||
                    state == IK_STATE_JUMP_STRONG_KICK) && time == 2u) {
            play_attack_sound(audio, state, 0);
        }
    }

    if ((fight->events & IK_EVENT_HIT) != 0u) {
        for (int i = 0; i < 2; ++i) {
            const int16_t state = fight->fighters[i].state;
            if (state == IK_STATE_PUNCH ||
                state == IK_STATE_STRONG_PUNCH ||
                state == IK_STATE_KICK ||
                state == IK_STATE_STRONG_KICK ||
                state == IK_STATE_CROUCH_PUNCH ||
                state == IK_STATE_CROUCH_STRONG_PUNCH ||
                state == IK_STATE_CROUCH_KICK ||
                state == IK_STATE_CROUCH_STRONG_KICK ||
                state == IK_STATE_JUMP_PUNCH ||
                state == IK_STATE_JUMP_STRONG_PUNCH ||
                state == IK_STATE_JUMP_KICK ||
                state == IK_STATE_JUMP_STRONG_KICK) {
                play_attack_sound(audio, state, 1);
                break;
            }
        }
    }
}

sat_result_t ik_audio_update(void) {
    return sat_audio_update();
}
