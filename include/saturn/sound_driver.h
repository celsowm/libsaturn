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
} sat_sound_driver_info_t;

/* Stops the 68000, loads the driver into Sound RAM, restarts it and waits for
 * its first ticks. The audio system must be initialised (sat_audio_init).
 * SAT_ERR_TIMEOUT when the sound CPU does not come up (the idle stub is put
 * back). */
sat_result_t sat_sound_driver_start(void);

/* Puts the idle stub back. Scheduled events not yet run are lost. */
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

#ifdef __cplusplus
}
#endif

#endif /* SATURN_SOUND_DRIVER_H */
