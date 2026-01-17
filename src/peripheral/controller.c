#include "saturn/types.h"
#include "saturn/peripheral.h"
#include "saturn/hardware.h"

#define SMPC_INPUT_DATA  0x20100061

void peripheral_init(void) {
}

void peripheral_read(PadState* state) {
    SMPC_COMREG = 0x01;
    
    while (!(SMPC_SF & 0x80));
    
    u8 data[4];
    for (int i = 0; i < 4; i++) {
        data[i] = *(volatile u8*)(SMPC_INPUT_DATA + i);
    }
    
    state->type = PERIPH_TYPE_DIGITAL;
    state->buttons = 0;
    
    if (!(data[1] & 0x01)) state->buttons |= BTN_UP;
    if (!(data[1] & 0x02)) state->buttons |= BTN_DOWN;
    if (!(data[1] & 0x04)) state->buttons |= BTN_LEFT;
    if (!(data[1] & 0x08)) state->buttons |= BTN_RIGHT;
    if (!(data[1] & 0x10)) state->buttons |= BTN_A;
    if (!(data[1] & 0x20)) state->buttons |= BTN_B;
    if (!(data[1] & 0x40)) state->buttons |= BTN_C;
    if (!(data[2] & 0x01)) state->buttons |= BTN_X;
    if (!(data[2] & 0x02)) state->buttons |= BTN_Y;
    if (!(data[2] & 0x04)) state->buttons |= BTN_Z;
    if (!(data[2] & 0x10)) state->buttons |= BTN_L;
    if (!(data[2] & 0x20)) state->buttons |= BTN_R;
    if (!(data[2] & 0x80)) state->buttons |= BTN_START;
    
    state->axis_x = 0;
    state->axis_y = 0;
    state->axis_l = 0;
    state->axis_r = 0;
}
