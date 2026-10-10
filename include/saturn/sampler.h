#ifndef SATURN_SAMPLER_H
#define SATURN_SAMPLER_H

#include <stdint.h>

#include "saturn/audio.h"
#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Sampler: SCSP voices driven by the caller every frame, for a sequencer that runs its own
 * envelopes, pitch bends and pans (a tracker, a port of another console's sound engine).
 *
 * The sampler claims a range of slots and a range of Sound RAM from the audio runtime. The
 * caller fills the RAM with samples (sat_sampler_ram gives the CPU address, e.g. as a
 * sat_load_files destination) and plays them on voices:
 * - Each voice owns two slots and plays a new note on the one not sounding, so no slot is
 *   reprogrammed while it is still heard.
 * - Key-ons and key-offs are staged and applied together by sat_sampler_commit (one KYONEX),
 *   once a frame: notes started in the same frame start on the same sample.
 * - The SCSP envelope is held at full level (attack 31, release 31); the level and the pitch
 *   are what the caller sets each frame with sat_sampler_voice_set (only changes are written).
 *
 * While the sampler runs, resident voices (sat_sound_play) never take its slots, resident
 * sounds and effect rings are not allocated in its RAM, and a stream whose slot or buffer is
 * claimed does not open (SAT_ERR_BUSY). sat_audio_shutdown ends the sampler. */

#define SAT_SAMPLER_MAX_VOICES 16u
#define SAT_SAMPLER_RAM_BASE 0x3000u /* below: the sound CPU's program, mailbox, stack and score */
#define SAT_SAMPLER_RAM_END 0x80000u

typedef struct sat_sampler_config {
    uint8_t first_slot;   /* the voices take slots first_slot .. first_slot + 2 * voice_count - 1 */
    uint8_t voice_count;  /* 1..SAT_SAMPLER_MAX_VOICES */
    uint8_t direct_level; /* DISDL 0..7 of every voice (7: 0 dB, each step -6 dB) */
    uint8_t reserved;
    uint32_t ram_offset;  /* Sound RAM byte offset of the claimed range (even, >= SAT_SAMPLER_RAM_BASE) */
    uint32_t ram_bytes;   /* 0: no RAM (the samples live in RAM the caller claims otherwise) */
} sat_sampler_config_t;

typedef struct sat_sampler_note {
    uint32_t offset;     /* Sound RAM byte offset of the first sample, inside the claimed range */
    uint16_t loop_start; /* LSA, samples from `offset` */
    uint16_t loop_end;   /* LEA: a one-shot note stops after this sample; a loop plays
                          * loop_start..loop_end - 1 and reads loop_end only to interpolate the
                          * wrap, so store a copy of the loop's first sample there */
    uint8_t format;      /* SAT_AUDIO_PCM_S8 or SAT_AUDIO_PCM_S16 */
    uint8_t loop;        /* 1: forward loop */
} sat_sampler_note_t;

typedef struct sat_sampler_stats {
    uint32_t key_ons;     /* notes started */
    uint32_t key_offs;    /* notes stopped (by the caller or a muted range) */
    uint32_t commits;     /* KYONEX writes */
    uint32_t range_mutes; /* notes stopped by sat_sampler_mute_range */
    uint32_t active;      /* voices keyed on after the last commit */
} sat_sampler_stats_t;

/* Claims the slots and the RAM. SAT_ERR_BUSY: a resident voice or a stream is using a claimed
 * slot, or a resident sound, an effect ring or a stream buffer lies in the RAM. */
sat_result_t sat_sampler_init(const sat_sampler_config_t* config);
/* Stops every voice and gives the slots and the RAM back. */
sat_result_t sat_sampler_shutdown(void);
uint8_t sat_sampler_is_initialized(void);
/* CPU address of Sound RAM byte `offset` of the claimed range; NULL outside it. */
void* sat_sampler_ram(uint32_t offset);

/* Stages a note on the voice's other slot (keyed on at the next commit) and stages the key-off
 * of the note it was playing. The note is silent until sat_sampler_voice_set gives it a level. */
sat_result_t sat_sampler_voice_start(uint8_t voice, const sat_sampler_note_t* note);
/* Stages the key-off of the voice's note; nothing if it plays none. */
sat_result_t sat_sampler_voice_stop(uint8_t voice);
/* Playback rate in Hz (44100: the sample's own rate at a 44.1 kHz sample) and linear levels
 * 0..255 for the left and right outputs (255: 0 dB), turned into TL and DIPAN; `attenuation`
 * adds TL steps (0.375 dB each) on both sides. */
sat_result_t sat_sampler_voice_set(uint8_t voice, uint32_t rate_hz, uint8_t left, uint8_t right,
                                   uint8_t attenuation);
/* 1 while a note is keyed on (or staged to be) on the voice. */
uint8_t sat_sampler_voice_active(uint8_t voice);
/* Applies the staged key changes with one KYONEX; nothing if there are none. */
sat_result_t sat_sampler_commit(void);
/* Before Sound RAM [offset, offset + bytes) is overwritten: silences and keys off every slot
 * reading it (current notes and released ones) and commits. Returns the notes it stopped. */
uint32_t sat_sampler_mute_range(uint32_t offset, uint32_t bytes);
sat_result_t sat_sampler_get_stats(sat_sampler_stats_t* out_stats);

/* The conversions the voices use, for callers that want them elsewhere. */
uint16_t sat_sampler_pitch_word(uint32_t rate_hz);     /* SCSP OCT/FNS, word 0x10 */
uint8_t sat_sampler_total_level(uint8_t amplitude);    /* TL of a linear amplitude 0..255 */

#ifdef __cplusplus
}
#endif

#endif
