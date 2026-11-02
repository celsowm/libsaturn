#ifndef SATURN_SOUND_DRIVER_H
#define SATURN_SOUND_DRIVER_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The resident sound driver: a small program for the SCSP 68000 that runs
 * timestamped SCSP register writes at the moment they come due, so a sound
 * starts on its tick whatever the SH-2 is doing (a long frame, a CD read).
 *
 * The SH-2 fills a 63-slot ring in Sound RAM; the 68000 wakes on SCSP timer A,
 * one tick every 256 samples (44100 / 256 = 172.27 Hz, 5.805 ms), and runs
 * every event whose tick has come. Nothing else changes: the ordinary sat_sound
 * and sat_music calls keep working, and sat_sound_play with
 * SAT_SOUND_PLAY_AT_TICK hands its key-on to the driver. A voice scheduled for
 * a later tick and stopped before then would still be keyed on when the tick
 * arrives: schedule only what will play. */

#define SAT_SOUND_DRIVER_TICK_HZ_X100 17227u
#define SAT_SOUND_DRIVER_RING_EVENTS 63u
#define SAT_SOUND_DRIVER_LOG_EVENTS 64u

typedef struct sat_sound_driver_info {
    uint8_t running;
    uint8_t reserved0;
    uint16_t version;
    uint16_t heartbeat;         /* the driver main loop woke this many times */
    uint16_t queued;            /* events waiting in the ring */
    uint16_t executed;          /* events run since start or clear */
    uint16_t max_lateness_ticks;   /* worst tick - due seen */
    uint16_t last_lateness_ticks;
    uint16_t reserved1;
    uint32_t tick;              /* ticks since the driver started */
    uint16_t score_event_count;
    uint16_t score_index;
    uint16_t score_loops;
    uint16_t score_executed;
    uint8_t score_running;
    uint8_t reserved2[3];
} sat_sound_driver_info_t;

/* Sorted register writes for an autonomous looping score. Times are relative
 * to the beginning of each pass, in 68000 timer ticks. Wavetable samples can
 * be loaded once into Sound RAM with sat_sound_create; no CD read or SH-2
 * update is needed while the 68000 plays the score. */
typedef struct sat_sound_score_event {
    uint32_t tick;
    uint16_t register_offset;
    uint16_t value;
} sat_sound_score_event_t;

#define SAT_SOUND_SCORE_MAX_EVENTS 1024u

/* Stops the 68000, loads the driver into Sound RAM, restarts it and waits for
 * its first ticks. The audio system must be initialised (sat_audio_init).
 * SAT_ERR_TIMEOUT when the sound CPU does not come up (the idle stub is put
 * back). */
sat_result_t sat_sound_driver_start(void);

/* Puts the idle stub back. Scheduled events not yet run are lost. */
/* Keeps the SCSP slots in `slot_mask` (bit n: slot n) for a score the 68000 plays: resident voices
 * (sat_sound_play) never take them, so effects and a long score share the chip. Released by
 * sat_sound_driver_stop. SAT_ERR_BUSY when a slot is already claimed (the sampler's, or an earlier
 * call). Claim before the score starts; a resident voice already sounding on a slot keeps it until it ends. */
sat_result_t sat_sound_driver_claim_slots(uint32_t slot_mask);
sat_result_t sat_sound_driver_stop(void);

uint8_t sat_sound_driver_running(void);
sat_result_t sat_sound_driver_info(sat_sound_driver_info_t* out_info);

/* Ticks since the driver started (0 when it is not running). */
uint32_t sat_sound_driver_tick(void);

uint32_t sat_sound_driver_ticks_from_ms(uint32_t milliseconds);
uint32_t sat_sound_driver_ms_from_ticks(uint32_t ticks);

/* Queues a write of `value` to the SCSP register at byte `register_offset`
 * (even, below 0x1000) for `due_tick`. SAT_ERR_CAPACITY when the ring is full,
 * SAT_ERR_NOT_INITIALIZED when the driver is not running. */
sat_result_t sat_sound_driver_schedule_write(uint32_t due_tick, uint16_t register_offset, uint16_t value);

/* Queues a key-on or key-off of `slot` (0-31) for `due_tick`, using the slot
 * control the SH-2 side last programmed. */
sat_result_t sat_sound_driver_schedule_key(uint32_t due_tick, uint8_t slot, uint8_t on);

/* Queues an event that only records when it ran (for timing checks). */
sat_result_t sat_sound_driver_schedule_marker(uint32_t due_tick);

/* The last 64 events the driver ran, oldest overwritten first: the tick (low
 * 16 bits) it ran at and how many ticks late it was. */
sat_result_t sat_sound_driver_read_log(uint32_t index, uint16_t* out_tick, uint16_t* out_lateness);

sat_result_t sat_sound_driver_clear_counters(void);

/* Replaces the current score. Events must be sorted, each tick must be less
 * than loop_ticks, and register offsets must be even and below 0x1000.
 * Playback begins two driver ticks after the score is copied. */
sat_result_t sat_sound_driver_score_load(
    const sat_sound_score_event_t* events, uint16_t count, uint32_t loop_ticks);
sat_result_t sat_sound_driver_score_stop(void);

/* Paged score (experimental, driver version 3). The score area (8 KiB, the same one a 1024-event score uses)
 * holds two pages of SAT_SOUND_DRIVER_PAGE_EVENTS events. The SH-2 publishes a page when one is free; the
 * 68000 alone consumes them, in number order, and goes on to the next page within the same tick, so a long
 * score plays without the SH-2 touching any single note. Event ticks are absolute stream time: the time of an
 * event is the stream base + the page's time_base + event.tick, so nothing accumulates. A page is published by
 * one word write after its data, count and times; a published page is never rewritten until the 68000 frees it.
 * The mode and the 1024-event score share the score area: starting one stops the other.
 *
 * tick_samples is 256 (5.805 ms, the default) or 128 (2.902 ms: timers A and B, 128 samples apart, both
 * free-running, so the grid cannot drift). It applies to every tick based call while the paged score runs (and
 * until sat_sound_driver_pages_stop): with 128 the driver tick counts twice as fast. */
#define SAT_SOUND_DRIVER_PAGE_EVENTS 512u

typedef struct sat_sound_pages_info {
    uint8_t on;                 /* the 68000 is playing pages */
    uint8_t done;               /* the last page has been played */
    uint8_t underrun_seen;
    uint8_t reserved;
    uint16_t tick_samples;
    uint16_t pages_played;
    uint16_t pages_published;
    uint16_t pages_free;        /* 0..2: pages the SH-2 may publish into right now */
    uint16_t underrun_ticks;    /* ticks the next page was missing after the previous one's end time */
    uint16_t late_events;       /* events run after their tick */
    uint16_t max_lateness_ticks;
    uint16_t bad_sequence;      /* pages seen out of number order (a broken protocol) */
    uint16_t max_burst;         /* most events run in a single tick */
    uint16_t refused;           /* publishes refused because the page was still in use */
    uint32_t executed;
    uint32_t base_tick;         /* driver tick of stream time 0 */
} sat_sound_pages_info_t;

/* Stream time 0 is the driver tick `now + lead_ticks`. SAT_ERR_INVALID_ARG for another tick_samples. */
sat_result_t sat_sound_driver_pages_start(uint16_t tick_samples, uint32_t lead_ticks);
/* Events sorted by tick (relative to time_base), count 1..512, registers even and below 0x1000. end_time is
 * the stream time (relative to time_base) up to which no later page holds an event: the first event of the
 * next page, or the end of the song. last ends the stream. SAT_ERR_BUSY while the page is still in use. */
sat_result_t sat_sound_driver_page_publish(
    const sat_sound_score_event_t* events, uint16_t count, uint32_t time_base, uint32_t end_time, uint8_t last);
sat_result_t sat_sound_driver_pages_info(sat_sound_pages_info_t* out_info);
sat_result_t sat_sound_driver_pages_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_SOUND_DRIVER_H */
