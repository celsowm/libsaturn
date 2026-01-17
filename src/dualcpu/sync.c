#include "saturn/dualcpu.h"
#include "saturn/hardware.h"

extern void _slave_entry(void);

static volatile u32* slave_start = (volatile u32*)0x06000000;

void dualcpu_init(void) {
}

void dualcpu_start_slave(void) {
    *slave_start = (u32)_slave_entry;
}

void dualcpu_stop_slave(void) {
    *slave_start = 0;
}

void dualcpu_signal_master(void) {
}

void dualcpu_signal_slave(void) {
}

void dualcpu_wait_for_slave(void) {
}

void dualcpu_wait_for_master(void) {
}

void dualcpu_purge_cache(CpuId cpu) {
    (void)cpu;
}

void dualcpu_flush_cache(CpuId cpu) {
    (void)cpu;
}
