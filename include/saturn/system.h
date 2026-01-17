#ifndef SATURN_SYSTEM_H
#define SATURN_SYSTEM_H

#include "saturn/types.h"

void system_init(void);
void system_halt(void);

void interrupt_enable_vblank(void);
void interrupt_disable_vblank(void);
void interrupt_wait_vblank(void);

typedef void (*InterruptHandler)(void);
void interrupt_set_vblank_handler(InterruptHandler handler);

#endif
