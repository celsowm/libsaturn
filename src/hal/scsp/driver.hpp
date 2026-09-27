#ifndef SATURN_HAL_SCSP_DRIVER_HPP
#define SATURN_HAL_SCSP_DRIVER_HPP

#include <stdint.h>

#include "src/hal/scsp/driver_logic.hpp"

namespace saturn::hal::scsp::driver {

struct Info {
    bool running;
    uint16_t version;
    uint16_t heartbeat;
    uint32_t tick;
    uint16_t queued;
    uint16_t executed;
    uint16_t max_lateness;
    uint16_t last_lateness;
    uint16_t score_event_count;
    uint16_t score_index;
    uint16_t score_loops;
    uint16_t score_executed;
    bool score_running;
};

struct ScoreEvent {
    uint32_t tick;
    uint16_t register_offset;
    uint16_t value;
};

/* Puts the driver image into Sound RAM, clears its mailbox, releases the 68000
 * and waits for its first ticks. Needs scsp::init(). False if the sound CPU
 * does not come up. */
bool start();

/* Restores the idle stub. */
void stop();

bool running();
bool read_info(Info* out);
uint32_t tick();

/* Queues one SCSP register write (or, with the marker bit, a bare timestamp)
 * for `due_tick`. False when the ring is full or the register is not writable. */
bool push_write(uint32_t due_tick, uint32_t register_offset, uint16_t value);
bool push_marker(uint32_t due_tick);

/* The last 64 executed events: the tick low word it ran at and how late (in
 * ticks) it was. */
bool read_log(uint32_t index, uint16_t* out_tick, uint16_t* out_lateness);

/* Tells the driver to clear its executed/lateness counters. */
void clear_counters();

bool score_load(const ScoreEvent* events, uint16_t count, uint32_t loop_ticks);
void score_stop();

}  // namespace saturn::hal::scsp::driver

#endif
