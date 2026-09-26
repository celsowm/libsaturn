#ifndef SATURN_INPUT_H
#define SATURN_INPUT_H

#include <stdint.h>

#include "saturn/core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SAT_PAD_PORT_COUNT 2u
/* Devices per port: 1 when plugged in directly, up to 6 behind a multitap. */
#define SAT_PAD_TAP_MAX 6u

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
    /* An analog axis moved: `control` is a SAT_AXIS_* index, `value` the new
     * 0..255 reading (mouse axes: the counts moved since the last event). */
    SAT_EVENT_AXIS = 5,
    /* Keyboard: `control` is the key number, `value` 1 on make, 0 on break. */
    SAT_EVENT_KEY_DOWN = 6,
    SAT_EVENT_KEY_UP = 7
} sat_event_type_t;

/* `control` of SAT_EVENT_AXIS. */
#define SAT_AXIS_X 0u
#define SAT_AXIS_Y 1u
#define SAT_AXIS_Z 2u        /* third axis of a three-axis stick (throttle) */
#define SAT_AXIS_L 3u        /* left analog trigger */
#define SAT_AXIS_R 4u        /* right analog trigger */
#define SAT_AXIS_MOUSE_X 5u
#define SAT_AXIS_MOUSE_Y 6u

typedef struct sat_event {
    uint16_t type;
    uint8_t port;
    uint8_t tap;         /* device index behind a multitap, else 0 */
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

/* What is plugged in. Kinds follow the SMPC peripheral formats: a digital
 * pad and a 3D Control Pad in digital mode are both SAT_DEVICE_PAD; the 3D
 * pad in analog mode (and other analog sticks) is SAT_DEVICE_ANALOG_PAD, with
 * the same buttons as a pad plus axes. */
typedef enum sat_device_kind {
    SAT_DEVICE_NONE = 0,
    SAT_DEVICE_PAD = 1,
    SAT_DEVICE_ANALOG_PAD = 2,
    SAT_DEVICE_MOUSE = 3,
    SAT_DEVICE_KEYBOARD = 4,
    SAT_DEVICE_UNKNOWN = 5      /* connected, but SMPC gave no data for it */
} sat_device_kind_t;

typedef struct sat_device_info {
    uint8_t kind;            /* sat_device_kind_t */
    uint8_t peripheral_id;   /* SMPC ID byte: type << 4 | data size; 0xFF when empty */
    uint8_t data_size;       /* data bytes the device sent (before any truncation) */
    uint8_t axis_count;      /* analog axes after the two button bytes */
    uint8_t multitap_id;     /* 0xF: none; 0: Sega Tap; 1: 6-player adapter */
    uint8_t tap_count;       /* connectors on this port: 0 empty, 1 direct, 2..6 taps */
    uint8_t reserved0;
    uint8_t reserved1;
} sat_device_info_t;

/* Analog readings: X and Y run 0 (left/up) to 255 (right/down), about 128 at
 * rest. A 3D Control Pad also reports its triggers (0 released, 255 pressed);
 * a three-axis stick has a throttle in z. */
typedef struct sat_analog_state {
    uint8_t axis_count;
    uint8_t x;
    uint8_t y;
    uint8_t z;
    uint8_t l;
    uint8_t r;
    uint8_t has_triggers;
    uint8_t reserved;
} sat_analog_state_t;

#define SAT_MOUSE_LEFT   ((uint8_t)(1u << 0))
#define SAT_MOUSE_RIGHT  ((uint8_t)(1u << 1))
#define SAT_MOUSE_MIDDLE ((uint8_t)(1u << 2))
#define SAT_MOUSE_START  ((uint8_t)(1u << 3))

typedef struct sat_mouse_state {
    uint8_t buttons;         /* SAT_MOUSE_* */
    uint8_t overflow;        /* 1 when a poll saw motion past 255 counts */
    int16_t reserved;
    int32_t dx;              /* counts since the last sat_mouse_read; right is positive */
    int32_t dy;              /* down the screen is positive */
} sat_mouse_state_t;

#define SAT_KEYBOARD_CAPS   ((uint8_t)(1u << 0))
#define SAT_KEYBOARD_NUM    ((uint8_t)(1u << 1))
#define SAT_KEYBOARD_SCROLL ((uint8_t)(1u << 2))

typedef struct sat_keyboard_state {
    uint16_t held;           /* the pad-equivalent keys (arrows, Z X C ...) */
    uint8_t locks;           /* SAT_KEYBOARD_* lit LEDs */
    uint8_t last_key;        /* key number of the latest make or break, else 0 */
    uint8_t make;            /* 1 when last_key was pressed on the latest poll */
    uint8_t brk;             /* 1 when it was released */
    uint16_t reserved;
} sat_keyboard_state_t;

/* Device queries. They describe the latest sat_input_poll /
 * sat_pad_poll_port result and touch no hardware. */
sat_result_t sat_input_device_info(uint8_t port, uint8_t tap, sat_device_info_t* out_info);
sat_result_t sat_pad_tap_state(uint8_t port, uint8_t tap, sat_pad_state_t* out_state);
/* SAT_ERR_UNSUPPORTED unless the device reports analog axes. */
sat_result_t sat_pad_analog(uint8_t port, uint8_t tap, sat_analog_state_t* out_analog);
/* Motion adds up across polls until read, then starts again from zero.
 * SAT_ERR_UNSUPPORTED unless the device is a mouse. Mouse buttons also
 * appear in the pad state: left = A, right = C, middle = B, start = START. */
sat_result_t sat_mouse_read(uint8_t port, uint8_t tap, sat_mouse_state_t* out_mouse);
/* SAT_ERR_UNSUPPORTED unless the device is a keyboard. */
sat_result_t sat_keyboard_read(uint8_t port, uint8_t tap, sat_keyboard_state_t* out_keyboard);

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
