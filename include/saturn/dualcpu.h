#ifndef SATURN_DUALCPU_H
#define SATURN_DUALCPU_H

#include "saturn/types.h"

typedef enum {
    CPU_MASTER = 0,
    CPU_SLAVE
} CpuId;

void dualcpu_init(void);
void dualcpu_start_slave(void);
void dualcpu_stop_slave(void);

void dualcpu_signal_master(void);
void dualcpu_signal_slave(void);
void dualcpu_wait_for_slave(void);
void dualcpu_wait_for_master(void);

void dualcpu_purge_cache(CpuId cpu);
void dualcpu_flush_cache(CpuId cpu);

#endif
