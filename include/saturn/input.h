#ifndef SATURN_INPUT_H
#define SATURN_INPUT_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SAT_PAD_PORT_COUNT 2u

#define SAT_PAD_UP    ((uint16_t)(1u << 12))
#define SAT_PAD_DOWN  ((uint16_t)(1u << 13))
#define SAT_PAD_LEFT  ((uint16_t)(1u << 14))
#define SAT_PAD_RIGHT ((uint16_t)(1u << 15))
#define SAT_PAD_START ((uint16_t)(1u << 3))
#define SAT_PAD_A     ((uint16_t)(1u << 2))
#define SAT_PAD_B     ((uint16_t)(1u << 4))
#define SAT_PAD_C     ((uint16_t)(1u << 5))
#define SAT_PAD_X     ((uint16_t)(1u << 6))
#define SAT_PAD_Y     ((uint16_t)(1u << 7))
#define SAT_PAD_Z     ((uint16_t)(1u << 8))
#define SAT_PAD_L     ((uint16_t)(1u << 9))
#define SAT_PAD_R     ((uint16_t)(1u << 10))

typedef struct sat_pad_state {
    uint16_t held;
    uint16_t pressed;
    uint16_t released;
    uint8_t connected;
    uint8_t reserved;
} sat_pad_state_t;

typedef enum sat_event_type {
    SAT_EVENT_PAD_CONNECTED = 1,
    SAT_EVENT_PAD_DISCONNECTED = 2,
    SAT_EVENT_BUTTON_DOWN = 3,
    SAT_EVENT_BUTTON_UP = 4,
    SAT_EVENT_AXIS = 5
} sat_event_type_t;

typedef struct sat_event {
    uint16_t type;
    uint8_t port;
    uint8_t reserved;
    uint16_t control;
    int16_t value;
} sat_event_t;

/* Polls both physical controller ports and updates the shared polling/event
 * snapshot. Events are generated from the same samples returned by
 * sat_pad_poll_port(), so polling and event views cannot diverge. */
sat_result_t sat_input_poll(void);
sat_result_t sat_pad_poll_port(uint8_t port, sat_pad_state_t* out_state);
sat_result_t sat_pad_poll(sat_pad_state_t* out_state);
uint16_t sat_pad_held_port(uint8_t port);
uint16_t sat_pad_held(void);

/* Returns 1 when an event was written, 0 when the queue is empty, or a
 * negative sat_result_t value for invalid/not-initialized calls. */
int sat_event_poll(sat_event_t* out_event);
uint16_t sat_event_capacity(void);
uint16_t sat_event_count(void);
uint32_t sat_event_overflow_count(void);

sat_result_t sat_pad_format_held(uint16_t held, char* out, uint16_t out_size);
sat_result_t sat_pad_format_frame(uint32_t frame_count, char* out, uint16_t out_size);

#ifdef __cplusplus
}
#endif

#endif /* SATURN_INPUT_H */
