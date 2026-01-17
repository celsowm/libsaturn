#ifndef SATURN_PERIPHERAL_H
#define SATURN_PERIPHERAL_H

#include "saturn/types.h"

typedef enum {
    PERIPH_TYPE_NONE = 0,
    PERIPH_TYPE_DIGITAL,
    PERIPH_TYPE_ANALOG,
    PERIPH_TYPE_KEYBOARD,
    PERIPH_TYPE_MOUSE
} PeripheralType;

typedef enum {
    BTN_UP = 0x0001,
    BTN_DOWN = 0x0002,
    BTN_LEFT = 0x0004,
    BTN_RIGHT = 0x0008,
    BTN_A = 0x0010,
    BTN_B = 0x0020,
    BTN_C = 0x0040,
    BTN_X = 0x0100,
    BTN_Y = 0x0200,
    BTN_Z = 0x0400,
    BTN_L = 0x1000,
    BTN_R = 0x2000,
    BTN_START = 0x8000
} PadButton;

typedef struct {
    PeripheralType type;
    u16 buttons;
    s16 axis_x;
    s16 axis_y;
    s16 axis_l;
    s16 axis_r;
} PadState;

void peripheral_init(void);
void peripheral_read(PadState* state);

#endif
