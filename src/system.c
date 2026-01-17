#include "saturn/types.h"
#include "saturn/hardware.h"

static InterruptHandler vblank_handler = 0;

void system_init(void) {
}

void system_halt(void) {
    while (1);
}

void interrupt_enable_vblank(void) {
}

void interrupt_disable_vblank(void) {
}

void interrupt_wait_vblank(void) {
    while (!(VDP2_TVSTAT & 0x0008));
}

void interrupt_set_vblank_handler(InterruptHandler handler) {
    vblank_handler = handler;
}
